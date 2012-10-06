/*****************************************************************************
 * vlcplugin_base.h: a VLC plugin for Mozilla
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Damien Fouilleul <damienf@videolan.org>
 *          Jean-Paul Saman <jpsaman@videolan.org>
 *          Cheng Sun <chengsun9@gmail.com>
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

/*******************************************************************************
 * Instance state information about the plugin.
 ******************************************************************************/
#ifndef __VLCPLUGIN_BASE_H__
#define __VLCPLUGIN_BASE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc/vlc.h>

// Setup XP_MACOSX, XP_UNIX, XP_WIN
#if defined(_WIN32)
#define XP_WIN 1
#elif defined(__APPLE__)
#define XP_MACOSX 1
#else
#define XP_UNIX 1
#define MOZ_X11 1
#endif

#if !defined(XP_MACOSX) && !defined(XP_UNIX) && !defined(XP_WIN)
#define XP_UNIX 1
#elif defined(XP_MACOSX)
#undef XP_UNIX
#endif

#ifdef XP_WIN
    /* Windows stuff */
#   include <windows.h>
#   include <winbase.h>
#endif

#ifdef XP_UNIX
#   include <pthread.h>
#endif

#ifndef __MAX
#   define __MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef __MIN
#   define __MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

//on windows, to avoid including <npapi.h> 
//from Microsoft SDK (rather then from Mozilla SDK),
//#include it indirectly via <npfunctions.h>
#include <npfunctions.h>

#include <vector>
#include <set>
#include <assert.h>

#include "control/nporuntime.h"
#include "../common/vlc_player_options.h"
#include "../common/vlc_player.h"

#if (((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR) < 20)
    typedef uint16 NPuint16_t;
    typedef int16 NPint16_t;
    typedef int32 NPint32_t;
#else
    typedef uint16_t NPuint16_t;
    typedef int16_t NPint16_t;
    typedef int32_t NPint32_t;
#endif

typedef struct {
#if defined(XP_UNIX)
    pthread_mutex_t mutex;
#elif defined(XP_WIN)
    CRITICAL_SECTION cs;
#else
#warning "locking not implemented in this platform"
#endif
} plugin_lock_t;

typedef struct {
    const char *name;                      /* event name */
    const libvlc_event_type_t libvlc_type; /* libvlc event type */
    libvlc_callback_t libvlc_callback;     /* libvlc callback function */
} vlcplugin_event_t;

class EventObj
{
private:

    class Listener
    {
    public:
        Listener(vlcplugin_event_t *event, NPObject *p_object, bool b_bubble):
            _event(event), _listener(p_object), _bubble(b_bubble)
            {
                assert(event);
                assert(p_object);
            }
        Listener(): _event(NULL), _listener(NULL), _bubble(false) { }
        ~Listener()
            {
            }
        libvlc_event_type_t event_type() const { return _event->libvlc_type; }
        NPObject *listener() const { return _listener; }
        bool bubble() const { return _bubble; }
    private:
        vlcplugin_event_t *_event;
        NPObject *_listener;
        bool _bubble;
    };

    class VLCEvent
    {
    public:
        VLCEvent(libvlc_event_type_t libvlc_event_type, NPVariant *npparams, uint32_t npcount):
            _libvlc_event_type(libvlc_event_type), _npparams(npparams), _npcount(npcount)
            {
            }
        VLCEvent(): _libvlc_event_type(0), _npparams(NULL), _npcount(0) { }
        ~VLCEvent()
            {
            }
        libvlc_event_type_t event_type() const { return _libvlc_event_type; }
        NPVariant *params() const { return _npparams; }
        uint32_t count() const { return _npcount; }
    private:
        libvlc_event_type_t _libvlc_event_type;
        NPVariant *_npparams;
        uint32_t _npcount;
    };
    libvlc_event_manager_t *_em; /* libvlc media_player event manager */
public:
    EventObj(): _em(NULL), _already_in_deliver(false) { /* deferred to init() */ }
    bool init();
    ~EventObj();

    void deliver(NPP browser);
    void callback(const libvlc_event_t *event, NPVariant *npparams, uint32_t count);
    bool insert(const NPString &name, NPObject *listener, bool bubble);
    bool remove(const NPString &name, NPObject *listener, bool bubble);
    void unhook_manager(void *);
    void hook_manager(libvlc_event_manager_t *, void *);
private:
    vlcplugin_event_t *find_event(const char *s) const;
    const char *find_name(const libvlc_event_t *event);
    typedef std::vector<Listener> lr_l;
    typedef std::vector<VLCEvent> ev_l;
    lr_l _llist; /* list of registered listeners with 'addEventListener' method */
    ev_l _elist; /* scheduled events list for delivery to browser */

    plugin_lock_t lock;
    bool _already_in_deliver;
};

typedef enum vlc_toolbar_clicked_e {
    clicked_Unknown = 0,
    clicked_Play,
    clicked_Pause,
    clicked_Stop,
    clicked_timeline,
    clicked_Time,
    clicked_Fullscreen,
    clicked_Mute,
    clicked_Unmute
} vlc_toolbar_clicked_t;

class VlcPluginBase: private vlc_player_options, private vlc_player
{
protected:

public:
    VlcPluginBase( NPP, NPuint16_t );
    virtual ~VlcPluginBase();

    vlc_player& get_player()
    {
        return *static_cast<vlc_player*>(this);
    }

    vlc_player_options& get_options()
        { return *static_cast<vlc_player_options*>(this); }
    const vlc_player_options& get_options() const
        { return *static_cast<const vlc_player_options*>(this); }

    NPError             init(int argc, char* const argn[], char* const argv[]);
    libvlc_instance_t*  getVLC()
                            { return libvlc_instance; };
    libvlc_media_player_t* getMD()
    {
        if( !get_player().is_open() )
        {
             libvlc_printerr("no mediaplayer");
        }
        return get_player().get_mp();
    }
    NPP                 getBrowser()
                            { return p_browser; };
    char*               getAbsoluteURL(const char *url);
    NPWindow&           getWindow()
                            { return npwindow; };
    virtual void        setWindow(const NPWindow &window);

    NPClass*            getScriptClass()
                            { return p_scriptClass; };

    NPuint16_t  i_npmode; /* either NP_EMBED or NP_FULL */

    /* plugin properties */
    int      b_stream;
    char *   psz_target;

    void playlist_play()
    {
        get_player().play();
    }
    void playlist_play_item(int idx)
    {
        get_player().play(idx);
    }
    void playlist_stop()
    {
        get_player().stop();
    }
    void playlist_next()
    {
        get_player().next();
    }
    void playlist_prev()
    {
        get_player().prev();
    }
    void playlist_pause()
    {
        get_player().pause();
    }
    int playlist_isplaying()
    {
        return get_player().is_playing();
    }
    int playlist_add( const char * mrl)
    {
        return get_player().add_item(mrl);
    }
    int playlist_add_extended_untrusted( const char *mrl, const char *,
                    int optc, const char **optv )
    {
        return get_player().add_item(mrl, optc, optv);
    }
    int playlist_delete_item( int idx)
    {
        return get_player().delete_item(idx);
    }
    void playlist_clear()
    {
        get_player().clear_items() ;
    }
    int  playlist_count()
    {
        return get_player().items_count();
    }

    void control_handler(vlc_toolbar_clicked_t);

    bool  player_has_vout();

    virtual bool create_windows() = 0;
    virtual bool resize_windows() = 0;
    virtual bool destroy_windows() = 0;

    virtual void toggle_fullscreen() = 0;
    virtual void set_fullscreen(int) = 0;
    virtual int get_fullscreen() = 0;

    virtual void set_toolbar_visible(bool) = 0;
    virtual bool get_toolbar_visible() = 0;

    virtual void update_controls() = 0;
    virtual void popup_menu() = 0;

    virtual void set_player_window() = 0;

    static bool canUseEventListener();

    EventObj events;
    void event_callback(const libvlc_event_t *, NPVariant *, uint32_t);

protected:
    // called after libvlc_media_player_new_from_media
    virtual void on_media_player_new()     {};
    // called before libvlc_media_player_release
    virtual void on_media_player_release() {};

    bool playlist_select(int);

    /* VLC reference */
    libvlc_instance_t   *libvlc_instance;
    NPClass             *p_scriptClass;

    /* browser reference */
    NPP     p_browser;
    char    *psz_baseURL;

    /* display settings */
    NPWindow  npwindow;

    static void eventAsync(void *);

private:
    static std::set<VlcPluginBase*> _instances;
};

#endif
