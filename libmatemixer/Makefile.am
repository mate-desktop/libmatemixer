lib_LTLIBRARIES = libmatemixer.la

AM_CPPFLAGS =                                                   \
        $(GLIB_CFLAGS)                                          \
        -I$(top_srcdir)                                         \
        -I$(top_srcdir)/libmatemixer                            \
        -DG_LOG_DOMAIN=\"libmatemixer\"                         \
        -DLIBMATEMIXER_BACKEND_DIR=\"$(libdir)/libmatemixer\"

libmatemixer_includedir = $(includedir)/libmatemixer

libmatemixer_include_HEADERS =                                  \
        matemixer.h                                             \
        matemixer-control.h                                     \
        matemixer-device.h                                      \
        matemixer-device-port.h                                 \
        matemixer-device-profile.h                              \
        matemixer-enums.h                                       \
        matemixer-track.h

libmatemixer_la_SOURCES =                                       \
        matemixer.c                                             \
        matemixer-private.h                                     \
        matemixer-backend.c                                     \
        matemixer-backend.h                                     \
        matemixer-backend-module.c                              \
        matemixer-control.c                                     \
        matemixer-device.c                                      \
        matemixer-device-port.c                                 \
        matemixer-device-profile.c                              \
        matemixer-enum-types.c                                  \
        matemixer-enum-types.h                                  \
        matemixer-track.c

libmatemixer_la_LIBADD = $(GLIB_LIBS)

libmatemixer_la_LDFLAGS =                                       \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
        -export-dynamic

-include $(top_srcdir)/git.mk
