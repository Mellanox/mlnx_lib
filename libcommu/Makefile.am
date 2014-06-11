# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir)			\
		   -I$(top_srcdir)/include	\
		   -I$(SX_COMPLIB_PATH)/include			\
		   -I$(srcdir)


if DEBUG
DBGFLAGS = -O0 -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLNX_LIBS_COMMON) $(DBGFLAGS)

lib_LTLIBRARIES = libcommu.la

libcommu_la_SOURCES =  \
                     lib_commu_db.c \
                     lib_commu.c
                     

libcommu_apiincludedir = $(includedir)/mlnx_lib
libcommu_apiinclude_HEADERS = \
                    lib_commu_db.h \
                    lib_commu.h \
                    lib_commu_bail.h \
                    lib_commu_log.h

libcommu_la_LIBADD=  -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog
