# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)			\
		   -I$(top_srcdir)/include	\
		   -I$(srcdir)

if DEBUG
DBGFLAGS = -O0 -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLNX_LIBS_COMMON) $(DBGFLAGS)

lib_LTLIBRARIES = libeventdisp.la

libeventdisp_la_SOURCES =  \
                     lib_event_disp.c \
                     lib_event_disp.h

libeventdisp_apiincludedir = $(includedir)/mlnx_lib
libeventdisp_apiinclude_HEADERS = \
                    lib_event_disp.h
