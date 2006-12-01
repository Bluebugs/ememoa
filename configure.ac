#  -*-Autconf-*-
#
# Process this file with autoconf to produce a configure script. 
#

# Autconf init
AC_PREREQ(2.59)
AC_INIT([ememoa], [0.0.12], [cedric.bail@free.fr])

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

# Configure files.
AC_CONFIG_FILES([
  include/Makefile
  src/lib/ememoa/Makefile
  doc/Makefile
  doc/doc.doxy
  test/Makefile
])
AC_OUTPUT

