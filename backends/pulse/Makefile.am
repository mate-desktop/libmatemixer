backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-pulse.la

AM_CPPFLAGS =                                                   \
	-Wno-unknown-pragmas                                    \
	-I$(top_srcdir)                                         \
	-DG_LOG_DOMAIN=\"libmatemixer-pulse\"

libmatemixer_pulse_la_CFLAGS =                                  \
	$(GLIB_CFLAGS)                                          \
	$(PULSEAUDIO_CFLAGS)

libmatemixer_pulse_la_SOURCES =                                 \
	pulse-backend.c                                         \
	pulse-backend.h                                         \
	pulse-connection.c                                      \
	pulse-connection.h                                      \
	pulse-device.c                                          \
	pulse-device.h                                          \
	pulse-device-profile.c                                  \
	pulse-device-profile.h                                  \
	pulse-device-switch.c                                   \
	pulse-device-switch.h                                   \
	pulse-enums.h                                           \
	pulse-enum-types.c                                      \
	pulse-enum-types.h                                      \
	pulse-ext-stream.c                                      \
	pulse-ext-stream.h                                      \
	pulse-helpers.c                                         \
	pulse-helpers.h                                         \
	pulse-monitor.c                                         \
	pulse-monitor.h                                         \
	pulse-port.c                                            \
	pulse-port.h                                            \
	pulse-port-switch.c                                     \
	pulse-port-switch.h                                     \
	pulse-stream.c                                          \
	pulse-stream.h                                          \
	pulse-stream-control.c                                  \
	pulse-stream-control.h                                  \
	pulse-sink.c                                            \
	pulse-sink.h                                            \
	pulse-sink-control.c                                    \
	pulse-sink-control.h                                    \
	pulse-sink-input.c                                      \
	pulse-sink-input.h                                      \
	pulse-sink-switch.c                                     \
	pulse-sink-switch.h                                     \
	pulse-source.c                                          \
	pulse-source.h                                          \
	pulse-source-control.c                                  \
	pulse-source-control.h                                  \
	pulse-source-output.c                                   \
	pulse-source-output.h                                   \
	pulse-source-switch.c                                   \
	pulse-source-switch.h                                   \
	pulse-types.h

libmatemixer_pulse_la_LIBADD =                                  \
	$(GLIB_LIBS)                                            \
	$(PULSEAUDIO_LIBS)

libmatemixer_pulse_la_LDFLAGS =                                 \
	-avoid-version                                          \
	-no-undefined                                           \
	-export-dynamic                                         \
	-module

-include $(top_srcdir)/git.mk
