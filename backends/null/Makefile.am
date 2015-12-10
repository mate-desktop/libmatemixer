backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-null.la

AM_CPPFLAGS =                                                   \
	-Wno-unknown-pragmas                                    \
	-I$(top_srcdir)                                         \
	-DG_LOG_DOMAIN=\"libmatemixer-null\"

libmatemixer_null_la_CFLAGS = $(GLIB_CFLAGS)

libmatemixer_null_la_SOURCES =                                  \
	null-backend.c                                          \
	null-backend.h

libmatemixer_null_la_LIBADD = $(GLIB_LIBS)

libmatemixer_null_la_LDFLAGS =                                  \
	-avoid-version                                          \
	-no-undefined                                           \
	-export-dynamic                                         \
	-module

-include $(top_srcdir)/git.mk
