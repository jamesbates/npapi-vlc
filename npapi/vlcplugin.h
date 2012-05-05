#ifndef __VLCPLUGIN_H__
#define __VLCPLUGIN_H__

#include "config.h"
#include "vlcplugin_base.h"

#if defined(XP_UNIX)
#   if defined(USE_GTK)
#       include "vlcplugin_gtk.h"
        typedef VlcPluginGtk VlcPlugin;
#   else
#       include "vlcplugin_xcb.h"
        typedef VlcPluginXcb VlcPlugin;
#   endif
#elif defined(XP_WIN)
#   include "vlcplugin_win.h"
    typedef VlcPluginWin VlcPlugin;
#elif defined(XP_MACOSX)
#   include "vlcplugin_mac.h"
    typedef VlcPluginMac VlcPlugin;
#endif

#endif /* __VLCPLUGIN_H__ */
