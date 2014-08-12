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
        matemixer-context.h                                     \
        matemixer-device.h                                      \
        matemixer-device-profile.h                              \
        matemixer-enums.h                                       \
        matemixer-stream.h                                      \
        matemixer-stream-control.h                              \
        matemixer-switch.h                                      \
        matemixer-switch-option.h                               \
        matemixer-toggle.h                                      \
        matemixer-types.h                                       \
        matemixer-version.h

libmatemixer_la_CFLAGS = $(GLIB_CFLAGS)

libmatemixer_la_SOURCES =                                       \
        matemixer.c                                             \
        matemixer-private.h                                     \
        matemixer-backend.c                                     \
        matemixer-backend.h                                     \
        matemixer-backend-private.h                             \
        matemixer-backend-module.c                              \
        matemixer-backend-module.h                              \
        matemixer-client-stream.c                               \
        matemixer-context.c                                     \
        matemixer-device.c                                      \
        matemixer-device-profile.c                              \
        matemixer-device-profile-private.h                      \
        matemixer-enum-types.c                                  \
        matemixer-enum-types.h                                  \
        matemixer-stream.c                                      \
        matemixer-stream-control.c                              \
        matemixer-stream-control-private.h                      \
        matemixer-switch.c                                      \
        matemixer-switch-private.h                              \
        matemixer-switch-option.c                               \
        matemixer-switch-option-private.h                       \
        matemixer-toggle.c

libmatemixer_la_LIBADD = $(GLIB_LIBS)

libmatemixer_la_LDFLAGS =                                       \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
        -no-undefined                                           \
        -export-dynamic

-include $(top_srcdir)/git.mk
