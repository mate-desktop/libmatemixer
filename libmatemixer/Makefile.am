lib_LTLIBRARIES = libmatemixer.la

AM_CPPFLAGS =                                                   \
        -I$(top_srcdir)                                         \
        -I$(top_srcdir)/libmatemixer                            \
        -DG_LOG_DOMAIN=\"libmatemixer\"                         \
        -DLIBMATEMIXER_BACKEND_DIR=\"$(libdir)/libmatemixer\"

libmatemixer_includedir = $(includedir)/libmatemixer

libmatemixer_include_HEADERS =                                  \
        matemixer.h                                             \
        matemixer-client-stream.h                               \
        matemixer-control.h                                     \
        matemixer-device.h                                      \
        matemixer-enums.h                                       \
        matemixer-port.h                                        \
        matemixer-profile.h                                     \
        matemixer-stream.h                                      \
        matemixer-version.h

libmatemixer_la_CFLAGS = $(GLIB_CFLAGS)

libmatemixer_la_SOURCES =                                       \
        matemixer.c                                             \
        matemixer-private.h                                     \
        matemixer-backend.c                                     \
        matemixer-backend.h                                     \
        matemixer-backend-module.c                              \
        matemixer-backend-module.h                              \
        matemixer-client-stream.c                               \
        matemixer-control.c                                     \
        matemixer-device.c                                      \
        matemixer-enum-types.c                                  \
        matemixer-enum-types.h                                  \
        matemixer-port.c                                        \
        matemixer-profile.c                                     \
        matemixer-stream.c

libmatemixer_la_LIBADD = $(GLIB_LIBS)

libmatemixer_la_LDFLAGS =                                       \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
        -no-undefined                                           \
        -export-dynamic

-include $(top_srcdir)/git.mk
