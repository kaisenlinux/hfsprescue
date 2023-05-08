#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(hfsprescue, [3.6 07/04/2023], [Elmar Hanlhofer https://www.plop.at/en/hfsprescue.html])
AM_INIT_AUTOMAKE([no-define])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_RANLIB


# Checks for libraries.

dnl 
dnl Configure libz.
dnl 
have_libz="1"
AC_CHECK_HEADERS([zlib.h], , [have_libz="0"])
AC_CHECK_LIB([z], [deflate], , [have_libz="0"])
if test "x${have_libz}" = "x0" ; then
    AC_MSG_ERROR([Cannot build without libz])
fi





# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h unistd.h utime.h zlib.h inttypes.h])

AC_DEFINE([__STDC_FORMAT_MACROS],[],[Force definition of format macros for C++])



# Checks iconv
#AC_CHECK_HEADER([iconv.h], ,[AC_MSG_ERROR([can not find iconv.h])]) 
#AC_CHECK_LIB([iconv], [iconv_open], ,[AC_MSG_ERROR([can not find iconv_open])])



# https://stackoverflow.com/questions/38898591/how-to-check-the-os-with-automake
# AC_CANONICAL_HOST is needed to access the 'host_os' variable    
AC_CANONICAL_HOST

build_linux=no
build_freebsd=no
build_openbsd=no
build_windows=no
build_mac=no

# Detect the target system
case "${host_os}" in
    linux*)
        build_linux=yes
        ;;
    freebsd*)
        build_freebsd=yes
        ;;
    openbsd*)
        build_openbsd=yes
        ;;
#    cygwin*|mingw*)
#        build_windows=yes
#        ;;
    darwin*)
        build_mac=yes
        ;;
    *)
#        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac










# Checks for libraries.

if test "$build_mac" = no; then
#### Find OpenSSL  (for SHA)
AC_CHECK_LIB([crypto],[CRYPTO_new_ex_data], [], [AC_MSG_ERROR([OpenSSL libraries required])])
AC_CHECK_HEADERS([openssl/sha.h],[],[AC_MSG_ERROR([OpenSSL headers required])])
fi





# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T



# Add an option to produce statically linked binaries.
AC_ARG_ENABLE([static],
               AC_HELP_STRING([--enable-static],
                              [build static library @<:@default=no@:>@]),
               [static=$enableval],
               [static=no])

# Check whether user supplied the option to statically link binaries.
if test "$static" = yes; then
        # if we're using gcc, add `-static' to LDFLAGS
        if test -n "$GCC" || test "$ac_cv_prog_gcc" = "yes"; then
                STATIC_LD="-static"
                LDFLAGS="$LDFLAGS -static"
        fi
fi



# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero mkdir rmdir strtoull utime])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 man/Makefile
                 doc/Makefile
                 doc/en/Makefile
                 doc/de/Makefile                 
                 ])
AC_OUTPUT


