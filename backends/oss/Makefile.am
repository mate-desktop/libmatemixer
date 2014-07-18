backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-oss.la

AM_CPPFLAGS =                                                   \
        -I$(top_srcdir)                                         \
        -DG_LOG_DOMAIN=\"libmatemixer-oss\"

libmatemixer_oss_la_CFLAGS =                                    \
        $(GLIB_CFLAGS)                                          \
        $(OSS_CFLAGS)

libmatemixer_oss_la_SOURCES =                                   \
        oss-backend.c                                           \
        oss-backend.h

libmatemixer_oss_la_LIBADD =                                    \
        $(GLIB_LIBS)

libmatemixer_oss_la_LDFLAGS =                                   \
        -avoid-version                                          \
        -no-undefined                                           \
        -export-dynamic                                         \
        -module

-include $(top_srcdir)/git.mk
