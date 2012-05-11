/*****************************************************************************
 * caopengl_vout.m: MacOS X Core Animation OpenGL vout plugin (used by NPAPI Webplugin)
 *****************************************************************************
 * Copyright (C) 2001-2012 the VideoLAN team
 * $Id: e374dfbc9c00f7da68cc7795c8850fcbb063df6f $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>
#import <dlfcn.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>
#include <vlc_dialog.h>
#include <vlc_threads.h>

#include "caopengl.h"
#include "../vlcplugin_layer.h"

/**
 * Forward declarations
 */
static int Open (vlc_object_t *);
static void OpenCallback(void *args);
static void Close (vlc_object_t *);

static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count);
static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
static void PictureRenderCallback (void *args);
static void PictureDisplay (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture);
static void PictureDisplayCallback (void *args);
static int Control (vout_display_t *vd, int query, va_list ap);

static void *OurGetProcAddress(vlc_gl_t *, const char *);

static int OpenglLock (vlc_gl_t *gl);
static void OpenglUnlock (vlc_gl_t *gl);
static void OpenglSwap (vlc_gl_t *gl);

static void EndCallback(void *args);

static void *sync_call_thread_Run(void *thread_run_args);


/**
 * Module declaration
 */
vlc_module_begin ()
    /* Will be loaded even without interface module. see voutgl.m */
    set_shortname ("CA OpenGL Mac OS X")
    set_description ("Mac OS X Core Animation OpenGL video output (for webplugin)")
    set_category (CAT_VIDEO)
    set_subcategory (SUBCAT_VIDEO_VOUT )
    set_capability ("vout display", 300)
    set_callbacks (Open, Close)

    add_shortcut ("caopengl_vout")
vlc_module_end ()

struct vout_display_sys_t
{
    VlcPluginLayer *caLayer;

    vlc_gl_t gl;
    vout_display_caopengl_t *vgl;

    picture_pool_t *pool;
    picture_t *current;
    bool has_first_frame;

    vout_display_place_t place;
    
    vlc_mutex_t *mutex_sync_ca_callback;
    vlc_cond_t *cond_sync_ca_callback;
    
    bool sync_ca_callback_complete;
};

typedef void (*ca_callback_t)(void *);

typedef struct ca_callback_args_t
{
    vout_display_t *vd;
    picture_t *pic;
    subpicture_t *subpicture;
} ca_callback_args_t;

typedef struct thread_run_args_t {

    ca_callback_args_t *callback_args;
    ca_callback_t callback;
} thread_run_args_t;


int sync_call_caLayer(ca_callback_t callback, ca_callback_args_t *args) {

    vout_display_t *vd = args->vd;

	vlc_mutex_lock(vd->sys->mutex_sync_ca_callback);

    if (!vd->sys->sync_ca_callback_complete) {
    
        vlc_mutex_unlock(vd->sys->mutex_sync_ca_callback);
        msg_Dbg(vd, "A callback is already in progress; aborting this one.\n");
        return -1;
    }

	vd->sys->sync_ca_callback_complete = false;
    
    thread_run_args_t *thread_run_args = calloc(1,sizeof(thread_run_args_t));
    thread_run_args->callback_args = args;
    thread_run_args->callback = callback;
    vlc_thread_t thr_launcher;
    
    if (vlc_clone(&thr_launcher, sync_call_thread_Run, thread_run_args, VLC_THREAD_PRIORITY_VIDEO)) {
    	
    	vlc_mutex_unlock(vd->sys->mutex_sync_ca_callback);
    	free(thread_run_args);
    	return -1;
    }
    
    do {
        vlc_cond_wait(vd->sys->cond_sync_ca_callback, vd->sys->mutex_sync_ca_callback);
    } while (!vd->sys->sync_ca_callback_complete);
        
    vlc_mutex_unlock(vd->sys->mutex_sync_ca_callback);
    
	return 0;
}

static void *sync_call_thread_Run(void *thread_run_args) {

	ca_callback_args_t *args = ((thread_run_args_t *) thread_run_args)->callback_args;
	ca_callback_t callback = ((thread_run_args_t *) thread_run_args)->callback;
	free(thread_run_args);

	vlc_mutex_lock(args->vd->sys->mutex_sync_ca_callback);	

    [args->vd->sys->caLayer setupCallbackWithFunc:callback args:args exitFunc:EndCallback];
    [args->vd->sys->caLayer setNeedsDisplay];

	vlc_mutex_unlock(args->vd->sys->mutex_sync_ca_callback);
	
	return NULL;
}

static void StartCallback(void *callback_args) {

    ca_callback_args_t *args = (ca_callback_args_t *) callback_args;

    vlc_mutex_lock(args->vd->sys->mutex_sync_ca_callback);
}

static void EndCallback(void *callback_args) {
    
    ca_callback_args_t *args = (ca_callback_args_t *) callback_args;

	args->vd->sys->sync_ca_callback_complete = true;

	vlc_cond_signal(args->vd->sys->cond_sync_ca_callback);
	vlc_mutex_unlock(args->vd->sys->mutex_sync_ca_callback);
	
    free(args);
}

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);

    return dlsym(RTLD_DEFAULT, name);
}

static int Open (vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = calloc (1, sizeof(*sys));

    if (!sys)
        return VLC_ENOMEM;

    if (!CGDisplayUsesOpenGLAcceleration (kCGDirectMainDisplay))
    {
        msg_Err (this, "no OpenGL hardware acceleration found. this can lead to slow output and unexpected results");
//        dialog_Fatal (this, _("OpenGL acceleration is not supported on your Mac"), _("Your Mac lacks Quartz Extreme acceleration, which is required for video output. It will still work, but much slower and with possibly unexpected results."));
    }
    else
        msg_Dbg (this, "Quartz Extreme acceleration is active");

    vd->sys = sys;
    sys->gl.sys = NULL;

    /* Get the drawable object */
    VlcPluginLayer *layer = (VlcPluginLayer *) var_CreateGetAddress (vd, "drawable-nsobject");
    if (!layer)
    {
        msg_Err(vd, "No drawable-nsobject nor vout_window_t found, passing over.");
        goto error;
    }

	msg_Dbg(vd, "Got the Core Animation layer through drawable-nsobject");
	sys->caLayer = layer;

    /* Set-up wait/notify machinery for synchronous callbacks to caLayer */
    sys->mutex_sync_ca_callback = calloc(1, sizeof(vlc_mutex_t));
    vlc_mutex_init(sys->mutex_sync_ca_callback);
    sys->cond_sync_ca_callback =  calloc(1, sizeof(vlc_cond_t));
    vlc_cond_init(sys->cond_sync_ca_callback);
	sys->sync_ca_callback_complete = true;

    /* Initialize common OpenGL video display */
    sys->gl.lock = OpenglLock;
    sys->gl.unlock = OpenglUnlock;
    sys->gl.swap = OpenglSwap;
    sys->gl.getProcAddress = OurGetProcAddress;
    sys->gl.sys = sys;
    
    const vlc_fourcc_t *subpicture_chromas;

    video_format_t fmt = vd->fmt;
    
    ca_callback_args_t *args = calloc(1, sizeof(ca_callback_args_t));
    args->vd = vd;
    args->pic = (picture_t *) &subpicture_chromas;
    args->subpicture = NULL;
        
	if (sync_call_caLayer(OpenCallback, args)) {
	
	    msg_Err(vd, "Open: sync callback FAILED!\n");
	    goto error;
	}
	    
	//printf("Open: sync callback completed.\n");    
    if (!sys->vgl)
    {
        sys->gl.sys = NULL;
        goto error;
    }
    
    vout_display_info_t info = args->vd->info;
    info.has_pictures_invalid = false;
    info.has_event_thread = true;
    info.subpicture_chromas = subpicture_chromas;

    /* Setup vout_display_t once everything is fine */
    args->vd->info = info;

    args->vd->pool = Pool;
    args->vd->prepare = PictureRender;
    args->vd->display = PictureDisplay;
    args->vd->control = Control;
	
    /* */
    vout_display_SendEventDisplaySize (args->vd, args->vd->source.i_visible_width, args->vd->source.i_visible_height, false);
    
	return VLC_SUCCESS;

error:
    Close(this);
    return VLC_EGENERIC;
}

static void OpenCallback(void *callback_args) {

	//printf("In OpenCallback.\n");
	StartCallback(callback_args);

    ca_callback_args_t *args = (ca_callback_args_t *) callback_args; 

	vout_display_sys_t *sys = args->vd->sys;
	const vlc_fourcc_t **subpicture_chromas_addr = (const vlc_fourcc_t **) args->pic;
	
    sys->vgl = vout_display_caopengl_New (&args->vd->fmt, subpicture_chromas_addr, &sys->gl);
}

void Close (vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = vd->sys;

    var_Destroy (vd, "drawable-nsobject");

    if (sys->gl.sys != NULL)
        vout_display_caopengl_Delete (sys->vgl);

    /* Destroy wait/notify machinery for synchronous callbacks to caLayer */
    if (sys->cond_sync_ca_callback) {
    
        vlc_cond_destroy(sys->cond_sync_ca_callback);
        free(sys->cond_sync_ca_callback);
    }

    if (sys->mutex_sync_ca_callback) {
    
         vlc_mutex_destroy(sys->mutex_sync_ca_callback);
         free(sys->mutex_sync_ca_callback);
    }

    free (sys);
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static picture_pool_t *Pool (vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;

    if (!sys->pool)
        sys->pool = vout_display_caopengl_GetPool (sys->vgl, requested_count);
    assert(sys->pool);
    return sys->pool;
}

static void PictureRender (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
	ca_callback_args_t *callback_args = calloc(1, sizeof(ca_callback_args_t));
	callback_args->vd = vd;
	callback_args->pic = pic;
	callback_args->subpicture = subpicture;
	
	if (sync_call_caLayer(PictureRenderCallback, callback_args)) {
	
		msg_Err(vd, "PictureRender callback FAILED!\n");
	}
}

static void PictureRenderCallback (void *callback_args)
{
	StartCallback(callback_args);

	ca_callback_args_t *args = (ca_callback_args_t *) callback_args;

    vout_display_sys_t *sys = args->vd->sys;
    vout_display_caopengl_Prepare (sys->vgl, args->pic, args->subpicture);
}

static void PictureDisplay (vout_display_t *vd, picture_t *pic, subpicture_t *subpicture)
{
	ca_callback_args_t *callback_args = calloc(1, sizeof(ca_callback_args_t));
	callback_args->vd = vd;
	callback_args->pic = pic;
	callback_args->subpicture = subpicture;
	
	if (sync_call_caLayer(PictureDisplayCallback, callback_args)) {
	
		msg_Err(vd, "PictureDisplayr callback FAILED!\n");
	}
}

static void PictureDisplayCallback (void *callback_args)
{
	StartCallback(callback_args);

	ca_callback_args_t *args = (ca_callback_args_t *) callback_args;

    vout_display_sys_t *sys = args->vd->sys;
    vout_display_caopengl_Display (sys->vgl, &args->vd->source);
    picture_Release (args->pic);
    sys->has_first_frame = true;

    if (args->subpicture)
        subpicture_Delete(args->subpicture);
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        {
            msg_Dbg(vd, "Full-screen requested");
            /* TODO: figure out what to do here */
             
            /*const vout_display_cfg_t *cfg = va_arg (ap, const vout_display_cfg_t *);
            if (vout_window_SetFullScreen (sys->embed, cfg->is_fullscreen))
                return VLC_EGENERIC;
			*/
            return VLC_SUCCESS;
        }
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        {
            msg_Dbg(vd, "Change window state requested");
            /* TODO: figure out what to do here */
            
            /*
            NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
            unsigned state = va_arg (ap, unsigned);
            [sys->glView performSelectorOnMainThread:@selector(setWindowLevel:) withObject:[NSNumber numberWithUnsignedInt:state] waitUntilDone:NO];
            [o_pool release];*/
            return VLC_SUCCESS;
        }
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        {
            /* Plugins' size cannot be altered */
        	return VLC_EGENERIC;
        	
 /*
            if (!config_GetInt(vd, "macosx-video-autoresize"))
            {
                NSRect bounds = [sys->glView bounds];
                cfg_tmp.display.width = bounds.size.width;
                cfg_tmp.display.height = bounds.size.height;
            }

            vout_display_PlacePicture (&sys->place, source, &cfg_tmp, false);
*/
            /* For resize, we call glViewport in reshape and not here.
               This has the positive side effect that we avoid erratic sizing as we animate every resize. */
/*            if (query != VOUT_DISPLAY_CHANGE_DISPLAY_SIZE)
            {
                // x / y are top left corner, but we need the lower left one
                glViewport (sys->place.x, cfg_tmp.display.height - (sys->place.y + sys->place.height), sys->place.width, sys->place.height);
            }

            [o_pool release];
            return VLC_SUCCESS;*/
        }

        case VOUT_DISPLAY_HIDE_MOUSE:
        {
            /* Can't do this in a plugin */
            return VLC_EGENERIC;
        }

        case VOUT_DISPLAY_GET_OPENGL:
        {
            vlc_gl_t **gl = va_arg (ap, vlc_gl_t **);
            *gl = &sys->gl;
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            assert (0);
        default:
            msg_Err (vd, "Unknown request in CA OpenGL Mac OS X vout display");
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int OpenglLock (vlc_gl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    CGLContextObj context = [sys->caLayer lastKnownGLContext];
    if (context) {
        CGLError err = CGLLockContext (context);
        if (kCGLNoError == err)
        {
            CGLSetCurrentContext(context);
    
            return 0;
        }
    }
    printf("Couldn't lock OpenGL context\n");
    return 0;
}

static void OpenglUnlock (vlc_gl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    CGLContextObj context = [sys->caLayer lastKnownGLContext];
    if (context) {
        CGLUnlockContext (context);
    }
}

static void OpenglSwap (vlc_gl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    CGLContextObj context = [sys->caLayer lastKnownGLContext];

    CGLFlushDrawable(context);
}

