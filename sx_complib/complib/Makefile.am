# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)/include

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_SX_COMPLIB_COMMON) $(DBGFLAGS) -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1

lib_LTLIBRARIES = libsxcomp.la

libsxcomp_la_SOURCES = cl_commchnl.c \
		       cl_complib.c \
		       cl_event.c \
		       cl_list.c \
		       cl_map.c \
		       cl_pool.c \
		       cl_spinlock.c \
		       cl_statustext.c \
		       cl_thread.c \
		       cl_mem.c \
		       cl_timer.c

libsxcomp_api_version=$(shell grep LIBVERSION= $(top_srcdir)/sxcomp.ver | sed 's/LIBVERSION=//')
libsxcomp_la_LDFLAGS = -version-info $(libsxcomp_api_version)

libsxcomp_la_LIBADD = -lpthread

libsxcomp_apiincludedir = $(includedir)/complib
libsxcomp_apiinclude_HEADERS = \
	$(top_srcdir)/include/complib/cl_atomic.h \
	$(top_srcdir)/include/complib/cl_atomic_osd.h \
	$(top_srcdir)/include/complib/cl_byteswap.h \
	$(top_srcdir)/include/complib/cl_byteswap_osd.h \
	$(top_srcdir)/include/complib/cl_commchnl.h \
	$(top_srcdir)/include/complib/cl_commchnl_osd.h \
	$(top_srcdir)/include/complib/cl_comppool.h \
	$(top_srcdir)/include/complib/cl_event.h \
	$(top_srcdir)/include/complib/cl_event_osd.h \
	$(top_srcdir)/include/complib/cl_fleximap.h \
	$(top_srcdir)/include/complib/cl_init.h \
	$(top_srcdir)/include/complib/cl_list.h \
	$(top_srcdir)/include/complib/cl_map.h \
	$(top_srcdir)/include/complib/cl_math.h \
	$(top_srcdir)/include/complib/cl_packoff.h \
	$(top_srcdir)/include/complib/cl_packon.h \
	$(top_srcdir)/include/complib/cl_pool.h \
	$(top_srcdir)/include/complib/cl_qcomppool.h \
	$(top_srcdir)/include/complib/cl_qlist.h \
	$(top_srcdir)/include/complib/cl_qmap.h \
	$(top_srcdir)/include/complib/cl_qpool.h \
	$(top_srcdir)/include/complib/cl_spinlock.h \
	$(top_srcdir)/include/complib/cl_spinlock_osd.h \
	$(top_srcdir)/include/complib/cl_thread.h \
	$(top_srcdir)/include/complib/cl_thread_osd.h \
	$(top_srcdir)/include/complib/cl_timer.h \
	$(top_srcdir)/include/complib/cl_timer_osd.h \
	$(top_srcdir)/include/complib/cl_types.h \
	$(top_srcdir)/include/complib/cl_passivelock.h \
	$(top_srcdir)/include/complib/cl_shared_memory.h \
	$(top_srcdir)/include/complib/cl_process_id.h \
	$(top_srcdir)/include/complib/cl_mem.h \
	$(top_srcdir)/include/complib/cl_types_osd.h

