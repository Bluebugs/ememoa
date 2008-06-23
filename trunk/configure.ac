#  -*-Autconf-*-
#
# Process this file with autoconf to produce a configure script. 
#

# Autconf init
AC_PREREQ(2.59)
AC_INIT([ememoa], [0.0.22], [cedric.bail@free.fr])

# Auxiliary files
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_FILES([
  Makefile
  build-aux/Makefile
])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

# Automake init
AM_INIT_AUTOMAKE([1.9.2 foreign dist-bzip2])
AM_CONFIG_HEADER([config.h])

# Make sure $prefix is defined.
test "x$prefix" = xNONE && prefix=$ac_default_prefix
test "x$exec_prefix" = xNONE && exec_prefix=$prefix

AC_ARG_ENABLE([debug],
        [AS_HELP_STRING([--enable-debug], [for debugging with gdb])],
        [echo "compiling in debug mode... -DDEBUG -ggdb3 -W -Wall";
	 CFLAGS="-DDEBUG -ggdb3 -W -Wall"
        ])

dnl Coverage
AC_ARG_ENABLE(coverage,
  [AC_HELP_STRING([--enable-coverage],
    [compile with coverage profiling instrumentation @<:@default=no@:>@])],
  [
   if test "x${enableval}" = "xyes" ; then
     enable_coverage="yes"
   else
     enable_coverage="no"
   fi],
  [enable_coverage="no"]
)
AC_MSG_CHECKING([whether to use profiling instrumentation])
AC_MSG_RESULT($enable_coverage)

if test "x$enable_coverage" = "xyes"; then
  AC_CHECK_PROG(have_lcov,
    [lcov],
    [yes],
    [no]
  )
  if test "x$have_lcov" = "xyes" ; then
    COVERAGE_CFLAGS="-fprofile-arcs -ftest-coverage"
    COVERAGE_LIBS="-lgcov"
dnl remove any optimisation flag and force debug symbols
    CFLAGS="-g -O0"
  else
    AC_MSG_WARN([lcov is not found, disable profiling instrumentation])
    enable_coverage="no"
  fi
fi
AC_SUBST(COVERAGE_CFLAGS)
AC_SUBST(COVERAGE_LIBS)

AM_CONDITIONAL(ENABLE_COVERAGE, test "x$enable_coverage" = "xyes")

# Check for standard programs.
AC_PROG_CC

AC_TYPE_SIZE_T
# AC_COMPILE_CHECK_SIZEOF(void *)

# Doxygen
AC_CHECK_PROGS([DOXYGEN], [doxygen], [$am_aux_dir/missing doxygen])
AC_ARG_VAR([DOXYGEN], [A C++ documentation system program])

# Require C99 style.
#CFLAGS="$CFLAGS -std=c99"

# Libtool
AC_DISABLE_STATIC
AM_PROG_LIBTOOL

# Check for pthread.
AC_ARG_ENABLE([pthread],
        [AS_HELP_STRING([--disable-pthread], [to disable pthread])],
        [use_pthread=no],
	[use_pthread=yes])
if test x$use_pthread = xyes; then
   ACX_PTHREAD
fi

# More thing in one register :-)
want_use64="no"
AC_MSG_CHECKING([whether to use 64bits often])
AC_ARG_ENABLE([use64],
	AS_HELP_STRING([--enable-use64], [use 64Bits as often as possible]),
	[want_use64=$enableval]
)
AC_MSG_RESULT($want_use64)

AM_CONDITIONAL(USE64, test "x$want_use64" = "xyes")

if test "x$want_use64" = "xyes"; then
   AC_DEFINE(USE64, 1, [Use 64 bits support as often as possible])
fi

# Display basic alloc request.
want_basic_alloc_report="no"
AC_MSG_CHECKING([whether to display all call to basic 64m alloc])
AC_ARG_ENABLE([alloc-report],
	AS_HELP_STRING([--enable-alloc-report], [Report all call to malloc/free/realloc 64m version]),
	[want_basic_alloc_report=$enableval]
)
AC_MSG_RESULT($want_basic_alloc_report)

AM_CONDITIONAL(ALLOC_REPORT, test "x$want_basic_alloc_report" = "x$yes")

if test "x$want_basic_alloc_report" = "xyes"; then
   AC_DEFINE(ALLOC_REPORT, 1, [Report all call to malloc/free/realloc 64m version])
fi

# Configure files.
AC_CONFIG_FILES([
  include/Makefile
  src/lib/ememoa/Makefile
  doc/Makefile
  doc/doc.doxy
  test/Makefile
])
AC_OUTPUT

