backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-oss4.la

AM_CPPFLAGS =                                                   \
        -I$(top_srcdir)                                         \
        -DG_LOG_DOMAIN=\"libmatemixer-oss4\"

libmatemixer_oss4_la_CFLAGS =                                   \
        $(GLIB_CFLAGS)                                          \
        $(OSS4_CFLAGS)

libmatemixer_oss4_la_SOURCES =                                  \
        oss4-common.h                                           \
        oss4-backend.c                                          \
        oss4-backend.h                                          \
        oss4-device.c                                           \
        oss4-device.h

libmatemixer_oss4_la_LIBADD =                                   \
        $(GLIB_LIBS)

libmatemixer_oss4_la_LDFLAGS =                                  \
        -avoid-version                                          \
        -no-undefined                                           \
        -export-dynamic                                         \
        -module

-include $(top_srcdir)/git.mk
