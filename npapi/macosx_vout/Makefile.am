# 
# Copied mostly verbatim from main-vlc-src/modules/common.am
#

# Automake forgets to add a proper tag to libtool with Objective-C files.
# Moreocer Libtool should default tag to CC when none is specified but
# obviously does not. Here is a fix for that.
LIBTOOL=@LIBTOOL@ --tag=CC

MODULE_NAME = `p="$@"; p="$${p\#\#*/}"; p="$${p\#lib}"; echo "$${p%_plugin*}"`

AM_CPPFLAGS = \
	-DMODULE_NAME=$(MODULE_NAME) \
	-DMODULE_NAME_IS_$(MODULE_NAME) \
	-DMODULE_STRING=\"$(MODULE_NAME)\" \
    $(VLC_PLUGIN_CFLAGS)

AM_CFLAGS =
AM_CXXFLAGS =
AM_OBJCFLAGS =
AM_LDFLAGS = -avoid-version -module \
	-export-symbols-regex ^vlc_entry \
	-shrext .dylib \
	-no-undefined \
    $(VLC_PLUGIN_LIBS) -lobjc



lib_LTLIBRARIES=libcaopengl_vout_plugin.la

libcaopengl_vout_plugin_la_SOURCES=caopengl_vout.m
