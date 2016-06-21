AC_PREREQ([2.60])

AC_INIT(
    [globus_dsi_rest],
    [0.5],
    [https://github.com/globus/globus-dsi-rest/issues])

AC_CONFIG_MACRO_DIR([m4])
AC_SUBST([MAJOR_VERSION], [${PACKAGE_VERSION%%.*}])
AC_SUBST([MINOR_VERSION], [${PACKAGE_VERSION##*.}])
AC_SUBST([AGE_VERSION], [0])

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11 foreign parallel-tests])

AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS

LT_INIT([dlopen win32-dll])

PKG_CHECK_MODULES([GLOBUS_GRIDFTP_SERVER], [globus-gridftp-server])
PKG_CHECK_MODULES([GLOBUS_COMMON], [globus-common])
PKG_CHECK_MODULES([GLOBUS_XIO], [globus-xio])
PKG_CHECK_MODULES([OPENSSL], [openssl])
PKG_CHECK_MODULES([JANSSON], [jansson])

AC_PATH_PROG([DOXYGEN], doxygen)
AM_CONDITIONAL([ENABLE_DOXYGEN], [test "$DOXYGEN" != ""])

LIBCURL_CHECK_CONFIG

AC_CONFIG_FILES([Makefile test/Makefile version.h Doxyfile])
AC_CONFIG_FILES([globus-dsi-rest.pc])
AC_OUTPUT
