/*****************************************************************************
 * vlcplugin_mac.cpp: a VLC plugin for Mozilla (Mac interface)
 *****************************************************************************
 * Copyright (C) 2011 the VideoLAN team
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

#include "vlcplugin_mac.h"
#include "macosx/vlcplugin_layer.h"
#include <npapi.h>

VlcPluginMac::VlcPluginMac(NPP instance, NPuint16_t mode) :
    VlcPluginBase(instance, mode),
    vlcplugin_layer(NULL)
{
}

NPError VlcPluginMac::init(int argc, char* const argn[], char* const argv[]) {

	NPError result = VlcPluginBase::init(argc, argn, argv);

	if (!result) {
		vlcplugin_layer = createVlcPluginLayer();

		result = (vlcplugin_layer ? NPERR_NO_ERROR : NPERR_GENERIC_ERROR);
	}
	return result;
}

VlcPluginMac::~VlcPluginMac()
{
    if (vlcplugin_layer) {

    	destroyVlcPluginLayer(vlcplugin_layer);
    }
}

void VlcPluginMac::set_player_window()
{
	libvlc_media_player_set_nsobject(libvlc_media_player, vlcplugin_layer);
}

void VlcPluginMac::toggle_fullscreen()
{
    if (!get_enable_fs()) return;
    if (playlist_isplaying())
        libvlc_toggle_fullscreen(libvlc_media_player);
}

void VlcPluginMac::set_fullscreen(int yes)
{
    if (!get_enable_fs()) return;
    if (playlist_isplaying())
        libvlc_set_fullscreen(libvlc_media_player, yes);
}

int  VlcPluginMac::get_fullscreen()
{
    int r = 0;
    if (playlist_isplaying())
        r = libvlc_get_fullscreen(libvlc_media_player);
    return r;
}

bool VlcPluginMac::create_windows()
{
    return true;
}

bool VlcPluginMac::resize_windows()
{
	setVlcPluginLayerBounds(vlcplugin_layer, CGRectMake(0.0, 0.0, npwindow.width, npwindow.height));



    /* as MacOS X video output is windowless, set viewport */
    libvlc_rectangle_t view, clip;

    /* browser sets port origin to top-left location of plugin
     * relative to GrafPort window origin is set relative to document,
     * which of little use for drawing
     */
    view.top	= 0; // ((NP_Port*) (npwindow.window))->porty;
    view.left	= 0; // ((NP_Port*) (npwindow.window))->portx;
    view.bottom  = npwindow.height+view.top;
    view.right   = npwindow.width+view.left;

    /* clipRect coordinates are also relative to GrafPort */
    clip.top     = npwindow.clipRect.top;
    clip.left    = npwindow.clipRect.left;
    clip.bottom  = npwindow.clipRect.bottom;
    clip.right   = npwindow.clipRect.right;

#ifdef NOT_WORKING
    libvlc_video_set_viewport(p_vlc, p_plugin->getMD(), &view, &clip);
#else
#warning disabled code
#endif

    return true;
}

bool VlcPluginMac::destroy_windows()
{
	return true;
}
