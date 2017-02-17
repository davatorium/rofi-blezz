ACLOCAL_AMFLAGS=-I m4
plugindir=${libdir}/rofi/

plugin_LTLIBRARIES = blezz.la

blezz_la_SOURCES=\
		 src/blezz.c

blezz_la_CFLAGS= @glib_CFLAGS@ @rofi_CFLAGS@
blezz_la_LIBADD= @glib_LIBS@ @rofi_LIBS@
blezz_la_LDFLAGS= -module -avoid-version
