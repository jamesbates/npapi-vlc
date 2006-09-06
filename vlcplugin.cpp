/*****************************************************************************
 * vlcplugin.cpp: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "config.h"

#ifdef HAVE_MOZILLA_CONFIG_H
#   include <mozilla-config.h>
#endif

#include "vlcplugin.h"
#include "control/npovlc.h"
#include "control/npolibvlc.h"

/*****************************************************************************
 * VlcPlugin constructor and destructor
 *****************************************************************************/
VlcPlugin::VlcPlugin( NPP instance, uint16 mode ) :
    i_npmode(mode),
    b_stream(0),
    b_autoplay(0),
    psz_target(NULL),
    libvlc_instance(NULL),
    scriptClass(NULL),
    p_browser(instance),
    psz_baseURL(NULL)
#if XP_WIN
    ,pf_wndproc(NULL)
#endif
#if XP_UNIX
    ,i_width((unsigned)-1)
    ,i_height((unsigned)-1)
#endif
{
    memset(&npwindow, 0, sizeof(NPWindow));
}

static int boolValue(const char *value) {
    return ( !strcmp(value, "1") || 
             !strcasecmp(value, "true") ||
             !strcasecmp(value, "yes") );
}

NPError VlcPlugin::init(int argc, char* const argn[], char* const argv[])
{
    /* prepare VLC command line */
    char *ppsz_argv[32] =
    {
        "vlc",
        "-vv",
        "--no-stats",
        "--no-media-library",
        "--intf", "dummy",
    };
    int ppsz_argc = 5;

    /* locate VLC module path */
#ifdef XP_MACOSX
    ppsz_argv[ppsz_argc++] = "--plugin-path";
    ppsz_argv[ppsz_argc++] = "/Library/Internet Plug-Ins/VLC Plugin.plugin/"
                             "Contents/MacOS/modules";
#elif defined(XP_WIN)
    HKEY h_key;
    DWORD i_type, i_data = MAX_PATH + 1;
    char p_data[MAX_PATH + 1];
    if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "Software\\VideoLAN\\VLC",
                      0, KEY_READ, &h_key ) == ERROR_SUCCESS )
    {
         if( RegQueryValueEx( h_key, "InstallDir", 0, &i_type,
                              (LPBYTE)p_data, &i_data ) == ERROR_SUCCESS )
         {
             if( i_type == REG_SZ )
             {
                 strcat( p_data, "\\vlc" );
                 ppsz_argv[0] = p_data;
             }
         }
         RegCloseKey( h_key );
    }
    ppsz_argv[ppsz_argc++] = "--no-one-instance";
#if 0
    ppsz_argv[ppsz_argc++] = "--fast-mutex";
    ppsz_argv[ppsz_argc++] = "--win9x-cv-method=1";
#endif

#endif /* XP_MACOSX */

    const char *version = NULL;

    /* parse plugin arguments */
    for( int i = 0; i < argc ; i++ )
    {
        fprintf(stderr, "argn=%s, argv=%s\n", argn[i], argv[i]);

        if( !strcmp( argn[i], "target" )
         || !strcmp( argn[i], "mrl")
         || !strcmp( argn[i], "filename")
         || !strcmp( argn[i], "src") )
        {
            psz_target = argv[i];
        }
        else if( !strcmp( argn[i], "autoplay")
              || !strcmp( argn[i], "autostart") )
        {
            b_autoplay = boolValue(argv[i]);
        }
        else if( !strcmp( argn[i], "fullscreen" ) )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--fullscreen";
            }
            else
            {
                ppsz_argv[ppsz_argc++] = "--no-fullscreen";
            }
        }
        else if( !strcmp( argn[i], "mute" ) )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--volume";
                ppsz_argv[ppsz_argc++] = "0";
            }
        }
        else if( !strcmp( argn[i], "loop")
              || !strcmp( argn[i], "autoloop") )
        {
            if( boolValue(argv[i]) )
            {
                ppsz_argv[ppsz_argc++] = "--loop";
            }
            else {
                ppsz_argv[ppsz_argc++] = "--no-loop";
            }
        }
        else if( !strcmp( argn[i], "version") )
        {
            version = argv[i];
        }
    }

    libvlc_instance = libvlc_new(ppsz_argc, ppsz_argv, NULL);
    if( ! libvlc_instance )
    {
        return NPERR_GENERIC_ERROR;
    }

    /*
    ** fetch plugin base URL, which is the URL of the page containing the plugin
    ** this URL is used for making absolute URL from relative URL that may be
    ** passed as an MRL argument
    */
    NPObject *plugin;

    if( NPERR_NO_ERROR == NPN_GetValue(p_browser, NPNVWindowNPObject, &plugin) )
    {
        /*
        ** is there a better way to get that info ?
        */
        static const char docLocHref[] = "document.location.href";
        NPString script;
        NPVariant result;

        script.utf8characters = docLocHref;
        script.utf8length = sizeof(docLocHref)-1;

        if( NPN_Evaluate(p_browser, plugin, &script, &result) )
        {
            if( NPVARIANT_IS_STRING(result) )
            {
                NPString &location = NPVARIANT_TO_STRING(result);

                psz_baseURL = new char[location.utf8length+1];
                if( psz_baseURL )
                {
                    strncpy(psz_baseURL, location.utf8characters, location.utf8length);
                    psz_baseURL[location.utf8length] = '\0';
                }
            }
            NPN_ReleaseVariantValue(&result);
        }
        NPN_ReleaseObject(plugin);
    }

    if( psz_target )
    {
        // get absolute URL from src
        psz_target = getAbsoluteURL(psz_target);
    }

    /* assign plugin script root class */
    if( (NULL != version) && (!strcmp(version, "VideoLAN.VLCPlugin.2")) )
    {
        /* new APIs */
        scriptClass = RuntimeNPClass<LibvlcRootNPObject>::getClass();
    }
    else
    {
        /* legacy APIs */
        scriptClass = RuntimeNPClass<VlcNPObject>::getClass();
    }

    return NPERR_NO_ERROR;
}

#if 0
#ifdef XP_WIN
/* This is really ugly but there is a deadlock when stopping a stream
 * (in VLC_CleanUp()) because the video output is a child of the drawable but
 * is in a different thread. */
static void HackStopVout( VlcPlugin* p_plugin )
{
    MSG msg;
    HWND hwnd;
    vlc_value_t value;

    int i_vlc = libvlc_get_vlc_id(p_plugin->libvlc_instance);
    VLC_VariableGet( i_vlc, "drawable", &value );

    hwnd = FindWindowEx( (HWND)value.i_int, 0, 0, 0 );
    if( !hwnd ) return;

    PostMessage( hwnd, WM_CLOSE, 0, 0 );

    do
    {
        while( PeekMessage( &msg, (HWND)value.i_int, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if( FindWindowEx( (HWND)value.i_int, 0, 0, 0 ) ) Sleep( 10 );
    }
    while( (hwnd = FindWindowEx( (HWND)value.i_int, 0, 0, 0 )) );
}
#endif /* XP_WIN */
#endif

VlcPlugin::~VlcPlugin()
{
    delete psz_baseURL;
    delete psz_target;
    if( libvlc_instance )
        libvlc_destroy(libvlc_instance);
}

/*****************************************************************************
 * VlcPlugin methods
 *****************************************************************************/

char *VlcPlugin::getAbsoluteURL(const char *url)
{
    if( NULL != url )
    {
        // check whether URL is already absolute
        const char *end=strchr(url, ':');
        if( (NULL != end) && (end != url) )
        {
            // validate protocol header
            const char *start = url;
            while( start != end ) {
                char c = *start | 0x20;
                if( (c < 'a') || (c > 'z') )
                    // not valid protocol header, assume relative URL
                    break;
                ++start;
            }
            /* we have a protocol header, therefore URL is absolute */
            return strdup(url);
        }

        if( psz_baseURL )
        {
            size_t baseLen = strlen(psz_baseURL);
            char *href = new char[baseLen+strlen(url)];
            if( href )
            {
                /* prepend base URL */
                strcpy(href, psz_baseURL);

                /*
                ** relative url could be empty,
                ** in which case return base URL
                */
                if( '\0' == *url )
                    return href;

                /*
                ** locate pathname part of base URL
                */

                /* skip over protocol part  */
                char *pathstart = strchr(href, ':');
                char *pathend;
                if( '/' == *(++pathstart) )
                {
                    if( '/' == *(++pathstart) )
                    {
                        ++pathstart;
                    }
                }
                /* skip over host part */
                pathstart = strchr(pathstart, '/');
                pathend = href+baseLen;
                if( ! pathstart )
                {
                    // no path, add a / past end of url (over '\0')
                    pathstart = pathend;
                    *pathstart = '/';
                }

                /* relative URL made of an absolute path ? */
                if( '/' == *url )
                {
                    /* replace path completely */
                    strcpy(pathstart, url);
                    return href;
                }

                /* find last path component and replace it */ 
                while( '/' != *pathend) --pathend;

                /*
                ** if relative url path starts with one or more '../',
                ** factor them out of href so that we return a
                ** normalized URL
                */
                while( pathend != pathstart )
                {
                    const char *p = url;
                    if( '.' != *p )
                        break;
                    ++p;
                    if( '.' != *p ) 
                        break;
                    ++p;
                    if( '/' != *p ) 
                        break;
                    ++p;
                    url = p;
                    while( '/' != *pathend ) --pathend;
                }
                /* concatenate remaining base URL and relative URL */
                strcpy(pathend+1, url);
            }
            return href;
        }
    }
    return NULL;
}

#if XP_UNIX
int  VlcPlugin::setSize(unsigned width, unsigned height)
{
    int diff = (width != i_width) || (height != i_height);

    i_width = width;
    i_height = height;

    /* return size */
    return diff;
}
#endif

