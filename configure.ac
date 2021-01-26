dnl Process this file with autoconf to produce a configure script.

AC_PREREQ([2.60])

# =======================================================================
# Versioning
# =======================================================================

m4_define(libmatemixer_major_version, 1)
m4_define(libmatemixer_minor_version, 25)
m4_define(libmatemixer_micro_version, 0)
m4_define(libmatemixer_version,
         [libmatemixer_major_version.libmatemixer_minor_version.libmatemixer_micro_version])

AC_INIT([libmatemixer],
        [libmatemixer_version],
        [https://mate-desktop.org])

# Before making a release, the LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A

LT_VERSION=0:0:0

AC_DEFINE(LIBMATEMIXER_MAJOR_VERSION, libmatemixer_major_version, [Libmatemixer major version])
AC_DEFINE(LIBMATEMIXER_MINOR_VERSION, libmatemixer_minor_version, [Libmatemixer minor version])
AC_DEFINE(LIBMATEMIXER_MICRO_VERSION, libmatemixer_micro_version, [Libmatemixer micro version])

AC_SUBST(LIBMATEMIXER_MAJOR_VERSION, libmatemixer_major_version)
AC_SUBST(LIBMATEMIXER_MINOR_VERSION, libmatemixer_minor_version)
AC_SUBST(LIBMATEMIXER_MICRO_VERSION, libmatemixer_micro_version)
AC_SUBST(LIBMATEMIXER_VERSION,       libmatemixer_version)

AC_SUBST(LT_VERSION)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR([libmatemixer/matemixer.c])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz foreign tar-ustar check-news])

# Use pretty build output with automake >= 1.11
# Disable by either passing --disable-silent-rules to configure or passing
# V=1 to make
m4_ifdef([AM_SILENT_RULES],
         [AM_SILENT_RULES([yes])],
         [AM_DEFAULT_VERBOSITY=1 AC_SUBST(AM_DEFAULT_VERBOSITY)])

# =======================================================================
# Compiler warnings
# =======================================================================
MATE_COMPILE_WARNINGS

# Checks for required programs.
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL

# =======================================================================
# Libtool
# =======================================================================

LT_PREREQ([2.2])
LT_INIT

# =======================================================================
# Gettext
# =======================================================================

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext Package])

AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

# =======================================================================
# Check for required packages
# =======================================================================
PKG_PROG_PKG_CONFIG

GLIB_REQUIRED_VERSION=2.50.0

PKG_CHECK_MODULES(GLIB, [
        glib-2.0 >= $GLIB_REQUIRED_VERSION
        gobject-2.0 >= $GLIB_REQUIRED_VERSION
        gmodule-2.0 >= $GLIB_REQUIRED_VERSION
])

GTK_DOC_CHECK([1.10], [--flavour no-tmpl])

# =======================================================================
# Check for backend module support
# =======================================================================
# -----------------------------------------------------------------------
# Null
# -----------------------------------------------------------------------
AC_ARG_ENABLE([null],
              AS_HELP_STRING([--enable-null],
                             [Enable Null backend module @<:@default=yes@:>@]),
              enable_null=$enableval,
              enable_null=yes)

have_null=no
if test "x$enable_null" != "xno" ; then
  AC_DEFINE(HAVE_NULL, [], [Define if we have Null module])
  have_null=yes
fi

AM_CONDITIONAL(HAVE_NULL, test "x$have_null" = "xyes")
AC_SUBST(HAVE_NULL)

# -----------------------------------------------------------------------
# PulseAudio
# -----------------------------------------------------------------------
PA_REQUIRED_VERSION=5.0.0

AC_ARG_ENABLE([pulseaudio],
              AS_HELP_STRING([--enable-pulseaudio],
                             [Enable PulseAudio backend module @<:@default=auto@:>@]),
              enable_pulseaudio=$enableval,
              enable_pulseaudio=auto)

have_pulseaudio=no
if test "x$enable_pulseaudio" != "xno"; then
  PKG_CHECK_MODULES(PULSEAUDIO, [
          libpulse >= $PA_REQUIRED_VERSION
          libpulse-mainloop-glib >= $PA_REQUIRED_VERSION
          ],
          have_pulseaudio=yes,
          have_pulseaudio=no)

  if test "x$have_pulseaudio" = "xyes"; then
    AC_DEFINE(HAVE_PULSEAUDIO, [], [Define if we have PulseAudio support])
  else
    if test "x$enable_pulseaudio" = "xyes"; then
      AC_MSG_ERROR([PulseAudio support explicitly requested but dependencies not found])
    else
      AC_MSG_NOTICE([PulseAudio dependencies not found, the module will not be built])
    fi
  fi
fi

AM_CONDITIONAL(HAVE_PULSEAUDIO, test "x$have_pulseaudio" = "xyes")

AC_SUBST(HAVE_PULSEAUDIO)
AC_SUBST(PULSEAUDIO_CFLAGS)
AC_SUBST(PULSEAUDIO_LIBS)

# -----------------------------------------------------------------------
# ALSA
# -----------------------------------------------------------------------
# This is set accordingly to the version reported by liboss4-salsa
ALSA_REQUIRED_VERSION=1.0.5

AC_ARG_ENABLE([alsa],
              AS_HELP_STRING([--enable-alsa],
                             [Enable ALSA backend module @<:@default=auto@:>@]),
              enable_alsa=$enableval,
              enable_alsa=auto)

have_alsa=no
if test "x$enable_alsa" != "xno"; then
  PKG_CHECK_MODULES(ALSA, [
          alsa >= $ALSA_REQUIRED_VERSION
          gthread-2.0 >= $GLIB_REQUIRED_VERSION
          ],
          have_alsa=yes,
          have_alsa=no)

  if test "x$have_alsa" = "xyes"; then
    AC_DEFINE(HAVE_ALSA, [], [Define if we have ALSA support])
  else
    if test "x$enable_alsa" = "xyes"; then
      AC_MSG_ERROR([ALSA support explicitly requested but dependencies not found])
    else
      AC_MSG_NOTICE([ALSA dependencies not found, the module will not be built])
    fi
  fi
fi

AC_ARG_ENABLE([udev],
              AS_HELP_STRING([--enable-udev],
                             [Enable UDEV support @<:@default=auto@:>@]),
              enable_udev=$enableval,
              enable_udev=auto)

have_udev=no
if test "x$enable_udev" != "xno" -a \
        "x$have_alsa" = "xyes"; then
        PKG_CHECK_MODULES(UDEV, [libudev],
                          have_udev=yes,
                          have_udev=no)
  if test "x$have_udev" = "xyes"; then
    AC_DEFINE(HAVE_UDEV, [], [Define if we have UDEV support])
  else
    if test "x$enable_udev" = "xyes"; then
      AC_MSG_ERROR([UDEV support explicitly requested but dependencies not found])
    else
      AC_MSG_NOTICE([UDEV dependencies not found, the module will not be built])
    fi
  fi
fi

AM_CONDITIONAL(HAVE_ALSA, test "x$have_alsa" = "xyes")
AM_CONDITIONAL(HAVE_UDEV, test "x$have_udev" = "xyes")

AC_SUBST(HAVE_ALSA)
AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
AC_SUBST(HAVE_UDEV)
AC_SUBST(UDEV_CFLAGS)
AC_SUBST(UDEV_LIBS)

# -----------------------------------------------------------------------
# OSS
# -----------------------------------------------------------------------
AC_ARG_ENABLE([oss],
              AS_HELP_STRING([--enable-oss],
                             [Enable OSS backend module @<:@default=no@:>@]),
              enable_oss=$enableval,
              enable_oss=no)

have_oss=no
if test "x$enable_oss" != "xno"; then
  AC_CHECK_HEADERS([soundcard.h sys/soundcard.h machine/soundcard.h])
  if test "x$ac_cv_header_soundcard_h" = "xyes" -o \
          "x$ac_cv_header_sys_soundcard_h" = "xyes" -o \
          "x$ac_cv_header_machine_soundcard_h" = "xyes"; then
    have_oss=yes
    AC_CHECK_HEADERS([sys/ioctl.h sys/stat.h sys/types.h errno.h fcntl.h],, [
      have_oss=no
      break
    ])
  fi

  if test "x$have_oss" = "xyes"; then
    # NetBSD/OpenBSD require -lossaudio, other OSS implementations do not have
    # this library
    AC_CHECK_LIB([ossaudio], [_oss_ioctl], [OSS_LIBS="-lossaudio"])

    AC_DEFINE(HAVE_OSS, [], [Define if we have OSS support])
  else
    if test "x$enable_oss" = "xyes"; then
      AC_MSG_ERROR([OSS support explicitly requested but dependencies not found])
    else
      AC_MSG_NOTICE([OSS dependencies not found, the module will not be built])
    fi
  fi
fi

AM_CONDITIONAL(HAVE_OSS, test "x$have_oss" = "xyes")

AC_SUBST(HAVE_OSS)
AC_SUBST(OSS_CFLAGS)
AC_SUBST(OSS_LIBS)

# =======================================================================
# Finish
# =======================================================================
AC_CONFIG_FILES([
Makefile
libmatemixer/Makefile
backends/Makefile
backends/null/Makefile
backends/pulse/Makefile
backends/alsa/Makefile
backends/oss/Makefile
data/Makefile
data/libmatemixer.pc
docs/Makefile
docs/reference/Makefile
docs/reference/version.xml
examples/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "

                    ${PACKAGE} ${VERSION}
                    ============


        Prefix:                      ${prefix}
        Source code location:        ${srcdir}
        Compiler:                    ${CC}
        Compiler flags:              ${CFLAGS}
        Warning flags:               ${WARN_CFLAGS}

        Build Null module:           $have_null
        Build PulseAudio module:     $have_pulseaudio
        Build ALSA module:           $have_alsa (udev: $have_udev)
        Build OSS module:            $have_oss
"
