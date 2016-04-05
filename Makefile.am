ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS =                               \
	libmatemixer                    \
	backends                        \
	data                            \
	docs                            \
	examples                        \
	po

EXTRA_DIST = autogen.sh

MAINTAINERCLEANFILES =                      \
	$(srcdir)/aclocal.m4                \
	$(srcdir)/config.h.in               \
	$(srcdir)/build-aux/compile         \
	$(srcdir)/build-aux/config.guess    \
	$(srcdir)/build-aux/config.sub      \
	$(srcdir)/build-aux/depcomp         \
	$(srcdir)/build-aux/install-sh      \
	$(srcdir)/build-aux/ltmain.sh       \
	$(srcdir)/build-aux/missing         \
	$(srcdir)/build-aux/mkinstalldirs   \
	$(srcdir)/m4/libtool.m4             \
	$(srcdir)/m4/lt~obsolete.m4         \
	$(srcdir)/m4/ltoptions.m4           \
	$(srcdir)/m4/ltsugar.m4             \
	$(srcdir)/m4/ltversion.m4           \
	$(srcdir)/m4/gtk-doc.m4

# Build ChangeLog from GIT  history
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog

-include $(top_srcdir)/git.mk
