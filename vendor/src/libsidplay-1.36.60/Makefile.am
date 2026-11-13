## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src

EXTRA_DIST = scripts DEVELOPER sidplay.xpm

dist-hook:
		rm -rf $(distdir)/src/compconf.h $(distdir)/src/libcfg.h

