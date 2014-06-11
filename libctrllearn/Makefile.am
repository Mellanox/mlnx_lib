# Makefile.am -- Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/include \
           -I$(top_srcdir)/src/ -I$(srcdir) -I$(SX_COMPLIB_PATH)/include -I$(OES_PATH)/OES 
		   

if DEBUG
DBGFLAGS = -O0 -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

CFLAGS = @CFLAGS@ $(CFLAGS_MLNX_LIBS_COMMON) $(DBGFLAGS)

lib_LTLIBRARIES = libctrllearn.la libctrllearnhalstub.la

libctrllearn_la_SOURCES =  \
                     lib_ctrl_learn.c \
                     lib_ctrl_learn.h  \
                     lib_ctrl_learn_uc_db.c \
                     lib_ctrl_learn_filters.c

libctrllearn_apiincludedir = $(includedir)/mlnx_lib
libctrllearn_apiinclude_HEADERS = \
                    lib_ctrl_learn.h \
                    lib_ctrl_learn_uc_db.h \
                    lib_ctrl_learn_defs.h \
                    lib_ctrl_learn_filters.h
                                      
libctrllearn_la_LIBADD=  -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog 

libctrllearnhalstub_la_SOURCES =  \
                     hal_stub/lib_ctrl_learn_hal.c 

libctrllearnhalstub_apiincludedir = $(includedir)/mlnx_lib

libctrllearnhalstub_la_LIBADD=  -L$(SX_COMPLIB_PATH)/lib/ -lsxcomp -lsxlog
