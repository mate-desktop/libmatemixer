backenddir = $(libdir)/libmatemixer

backend_LTLIBRARIES = libmatemixer-pulse.la

AM_CPPFLAGS =                                                   \
        -I$(top_srcdir)                                         \
        -DG_LOG_DOMAIN=\"libmatemixer-pulse\"

libmatemixer_pulse_la_CFLAGS =                                  \
        $(GLIB_CFLAGS)                                          \
        $(PULSEAUDIO_CFLAGS)

libmatemixer_pulse_la_SOURCES =                                 \
        pulse.c                                                 \
        pulse.h                                                 \
        pulse-connection.c                                      \
        pulse-connection.h                                      \
        pulse-device.c                                          \
        pulse-device.h                                          \
        pulse-stream.c                                          \
        pulse-stream.h                                          \
        pulse-sink.c                                            \
        pulse-sink.h                                            \
        pulse-sink-input.c                                      \
        pulse-sink-input.h                                      \
        pulse-source.c                                          \
        pulse-source.h                                          \
        pulse-source-output.c                                   \
        pulse-source-output.h

libmatemixer_pulse_la_LIBADD =                                  \
        $(GLIB_LIBS)                                            \
        $(PULSEAUDIO_LIBS)

libmatemixer_pulse_la_LDFLAGS =                                 \
        -avoid-version                                          \
        -export-dynamic                                         \
        -module

-include $(top_srcdir)/git.mk
