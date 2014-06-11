# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(srcdir)/include 

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_SX_COMPLIB_COMMON) $(DBGFLAGS) -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1

lib_LTLIBRARIES = libsxlog.la

libsxlog_la_SOURCES = sx_log.c 

libsxlog_api_version=$(shell grep LIBVERSION= $(top_srcdir)/sxcomp.ver | sed 's/LIBVERSION=//')
libsxlog_la_LDFLAGS = -version-info $(libsxlog_api_version)

libsxlog_apiincludedir = $(includedir)/complib
libsxlog_apiinclude_HEADERS = \
	$(top_srcdir)/include/complib/sx_log.h
