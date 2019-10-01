NULL =

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	$(GLIB_CFLAGS)						\
	$(NULL)

noinst_PROGRAMS = matemixer-monitor

matemixer_monitor_SOURCES = monitor.c

matemixer_monitor_CFLAGS =					\
	$(WARN_CFLAGS)						\
	$(NULL)

matemixer_monitor_LDADD =                                       \
	$(GLIB_LIBS)                                            \
	$(top_builddir)/libmatemixer/libmatemixer.la

EXTRA_DIST = monitor.c

-include $(top_srcdir)/git.mk
