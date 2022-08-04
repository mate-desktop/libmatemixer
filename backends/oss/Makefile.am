NULL =

backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-oss.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	-DG_LOG_DOMAIN=\"libmatemixer-oss\"			\
	$(GLIB_CFLAGS)						\
	$(OSS_CFLAGS)						\
	$(NULL)

libmatemixer_oss_la_CFLAGS =					\
	$(WARN_CFLAGS)						\
	$(NULL)

libmatemixer_oss_la_SOURCES =                                   \
	oss-common.h                                            \
	oss-backend.c                                           \
	oss-backend.h                                           \
	oss-device.c                                            \
	oss-device.h                                            \
	oss-stream.c                                            \
	oss-stream.h                                            \
	oss-stream-control.c                                    \
	oss-stream-control.h                                    \
	oss-switch.c                                            \
	oss-switch.h                                            \
	oss-switch-option.c                                     \
	oss-switch-option.h                                     \
	oss-types.h

libmatemixer_oss_la_LIBADD =                                    \
	$(top_builddir)/libmatemixer/libmatemixer.la            \
	$(GLIB_LIBS)                                            \
	$(OSS_LIBS)

libmatemixer_oss_la_LDFLAGS =                                   \
	-avoid-version                                          \
	-Wl,--no-undefined                                      \
	-export-dynamic                                         \
	-module

-include $(top_srcdir)/git.mk
