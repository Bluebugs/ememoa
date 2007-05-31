# -*- Makefile -*-

lib_LTLIBRARIES = libememoa.la

libememoa_la_SOURCES 	=			\
	ememoa_mempool_error.c			\
	ememoa_mempool_fixed.c			\
	ememoa_mempool_unknown_size.c		\
	ememoa_memory_base.c			\
	mempool_struct.h
libememoa_la_CFLAGS	= $(PTHREAD_CFLAGS)
libememoa_la_LDFLAGS	= $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) -version-info 0:16:0
CC			= $(PTHREAD_CC)
INCLUDES		= -I$(top_srcdir)/include

MAINTAINERCLEANFILES	= Makefile.in
