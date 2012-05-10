# Automake forgets to add a proper tag to libtool with Objective-C files.
# Moreocer Libtool should default tag to CC when none is specified but
# obviously does not. Here is a fix for that.
LIBTOOL=@LIBTOOL@ --tag=CC

SUBDIRS = plugins

AM_CPPFLAGS = $(LIBVLC_CFLAGS) -I$(srcdir)/.. -I$(srcdir) -I$(top_srcddir) -c \
	-F/System/Library/Frameworks/CoreFoundation.framework \
	-I/Developer/Headers/FlatCarbon -fno-common -fpascal-strings \
	-Wmost -Wno-four-char-constants -Wno-unknown-pragmas \
	-DXP_UNIX -DXP_MACOSX=1 \
	-DNO_X11=1 -DUSE_SYSTEM_CONSOLE=1 -pipe -fmessage-length=0

noinst_LTLIBRARIES = libmacosx.la
libmacosx_la_SOURCES = misc.m misc.h vlcplugin_layer.m vlcplugin_layer.h
