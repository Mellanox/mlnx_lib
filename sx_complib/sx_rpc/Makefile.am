# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include -I$(srcdir)/include 

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_SX_COMPLIB_COMMON) $(DBGFLAGS) -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1

lib_LTLIBRARIES = libsxrpc.la

libsxrpc_la_SOURCES = sx_rpc_common.c

libsxrpc_api_version=$(shell grep LIBVERSION= $(top_srcdir)/sxcomp.ver | sed 's/LIBVERSION=//')
libsxrpc_la_LDFLAGS = -version-info $(libsxrpc_api_version)

libsxrpc_la_LIBADD = -L../complib/.libs -lsxcomp \
		     -lpthread

libsxrpc_apiincludedir = $(includedir)/complib
libsxrpc_apiinclude_HEADERS = \
	$(top_srcdir)/include/complib/sx_rpc.h
