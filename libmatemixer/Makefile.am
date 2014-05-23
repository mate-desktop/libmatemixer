lib_LTLIBRARIES = libmatemixer.la

AM_CPPFLAGS =                                                   \
        $(GLIB_CFLAGS)                                          \
        -I$(top_srcdir)                                         \
        -I$(top_srcdir)/libmatemixer                            \
        -DG_LOG_DOMAIN=\"libmatemixer\"                         \
        -DLIBMATEMIXER_BACKEND_DIR=\"$(libdir)/libmatemixer\"

BUILT_SOURCES =                                                 \
        matemixer-enum-types.c                                  \
        matemixer-enum-types.h

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
        matemixer-track.c                                       \
        $(BUILD_SOURCES)

libmatemixer_la_LIBADD = $(GLIB_LIBS)

libmatemixer_la_LDFLAGS =                                       \
        -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
        -export-dynamic

libmatemixer_ENUM_TYPES = matemixer-enums.h

matemixer-enum-types.h: $(libmatemixer_ENUM_TYPES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template matemixer-enum-types.h.template \
		$(libmatemixer_ENUM_TYPES) ) > matemixer-enum-types.h.tmp \
	&& mv matemixer-enum-types.h.tmp matemixer-enum-types.h \
	|| rm -f matemixer-enum-type.h.tmp

matemixer-enum-types.c: $(libmatemixer_ENUM_TYPES)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template matemixer-enum-types.c.template \
		$(libmatemixer_ENUM_TYPES) ) > matemixer-enum-types.c.tmp \
	&& mv matemixer-enum-types.c.tmp matemixer-enum-types.c \
	|| rm -f matemixer-enum-type.c.tmp

CLEANFILES = $(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
