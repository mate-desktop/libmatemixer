lib_LTLIBRARIES = libmatemixer.la

AM_CPPFLAGS =                                                   \
	-I$(top_srcdir)                                         \
	-I$(top_srcdir)/libmatemixer                            \
	-DG_LOG_DOMAIN=\"libmatemixer\"                         \
	-DLIBMATEMIXER_BACKEND_DIR=\"$(libdir)/libmatemixer\"

libmatemixer_includedir = $(includedir)/mate-mixer/libmatemixer

libmatemixer_include_HEADERS =                                  \
	matemixer.h                                             \
	matemixer-app-info.h                                    \
	matemixer-context.h                                     \
	matemixer-device.h                                      \
	matemixer-device-switch.h                               \
	matemixer-enums.h                                       \
	matemixer-enum-types.h                                  \
	matemixer-stored-control.h                              \
	matemixer-stream.h                                      \
	matemixer-stream-control.h                              \
	matemixer-stream-switch.h                               \
	matemixer-stream-toggle.h                               \
	matemixer-switch.h                                      \
	matemixer-switch-option.h                               \
	matemixer-types.h                                       \
	matemixer-version.h

libmatemixer_la_CFLAGS = $(GLIB_CFLAGS)

libmatemixer_la_SOURCES =                                       \
	matemixer.c                                             \
	matemixer-private.h                                     \
	matemixer-app-info.c                                    \
	matemixer-app-info-private.h                            \
	matemixer-backend.c                                     \
	matemixer-backend.h                                     \
	matemixer-backend-module.c                              \
	matemixer-backend-module.h                              \
	matemixer-context.c                                     \
	matemixer-device.c                                      \
	matemixer-device-switch.c                               \
	matemixer-enum-types.c                                  \
	matemixer-stored-control.c                              \
	matemixer-stream.c                                      \
	matemixer-stream-private.h                              \
	matemixer-stream-control.c                              \
	matemixer-stream-control-private.h                      \
	matemixer-stream-switch.c                               \
	matemixer-stream-toggle.c                               \
	matemixer-switch.c                                      \
	matemixer-switch-private.h                              \
	matemixer-switch-option.c                               \
	matemixer-switch-option-private.h

libmatemixer_la_LIBADD = $(GLIB_LIBS)

libmatemixer_la_LDFLAGS =                                       \
	-version-info $(LT_VERSION)                             \
	-no-undefined                                           \
	-export-dynamic

-include $(top_srcdir)/git.mk
