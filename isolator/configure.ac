AC_INIT([dvdi-modules],[0.1])
AC_PREREQ([2.69])
AC_CONFIG_FILES([Makefile])
AC_LANG([C++])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_CC_C_O
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign])

AX_PYTHON_DEVEL([])

LT_PREREQ([2.2])
LT_INIT
LT_LANG([C++])
LT_OUTPUT

AC_ARG_WITH([mesos],
            [AS_HELP_STRING([--with-mesos=dir],
                            [provide root dir for Mesos installation])],
            [mesos=$withval],
            [mesos=no])

AC_ARG_WITH([mesos_root],
            [AS_HELP_STRING([--with-mesos-root=dir],
                            [provide root dir for Mesos sources])],
            [mesos_root=$withval],
            [mesos_root=no])

AC_ARG_WITH([mesos_build_dir],
            [AS_HELP_STRING([--with-mesos-build-dir=dir],
                            [provide Mesos build dir])],
            [mesos_build_dir=$withval],
            [mesos_build_dir=no])

AC_ARG_WITH([protobuf],
            [AS_HELP_STRING([--with-protobuf=dir],
                            [provide install location of protobuf])],
            [protobuf=$withval],
            [protobuf=/usr])

if test -d "$mesos"; then
  MESOS_INCLUDE_PATH="-I${mesos}/include"
  MESOS_LDFLAGS="-L${mesos}/lib64 -lmesos -lglog -lprotobuf"
  # Use the system default prefix if not specified.
  if test -n "`echo $with_protobuf`"; then
    PROTOC_PATH="${with_protobuf}/bin"
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${with_protobuf}/include"
  else
    PROTOC_PATH="/usr/bin"
  fi

else

  if test -d "$mesos_root"; then
    MESOS_INCLUDE_PATH="-I${mesos_root}/include"

    # Add libprocess to include path.
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${mesos_root}/3rdparty/libprocess/include"

    # Add stout to include path.
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${mesos_root}/3rdparty/libprocess/3rdparty/stout/include"

    # Add src to include path
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${mesos_root}/src"

    # Add build/src to include path
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${mesos_root}/build/src"
  else
    AC_MSG_ERROR([Invalid mesos path; use --with-mesos-root=<DIR>
                  <DIR> is the top-level directory of the Mesos distribution.])
  fi

  if test -d "$mesos_build_dir"; then
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${mesos_build_dir}/include"
    PROTOBUF="${mesos_build_dir}/3rdparty/libprocess/3rdparty/protobuf-2.5.0"
    PROTOC_PATH="${PROTOBUF}/src"
    MESOS_INCLUDE_PATH="${MESOS_INCLUDE_PATH} -I${PROTOBUF}/src"
  else
    AC_MSG_ERROR([Invalid mesos build path; use --with-mesos-build-dir=<DIR>
                  <DIR> is the Mesos build directory.])
  fi
fi

MESOS_CPPFLAGS="$MESOS_INCLUDE_PATH"

AC_SUBST(MESOS_CPPFLAGS)
AC_SUBST(MESOS_INCLUDE_PATH)
AC_SUBST(MESOS_LDFLAGS)

# Check if we are on OSX or Linux.
case "$host_os" in
  linux*)
    LIB_EXT="so"
    ;;
  darwin*)
    LIB_EXT="dylib"
    ;;
  *)
  AC_MSG_ERROR("Mesosphere's Modules are currently unsupported on your platform.")
esac
AC_SUBST(LIB_EXT)

# Check if we're using clang.
AC_MSG_CHECKING([if compiling with clang])

AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([], [[
#ifndef __clang__
       not clang
#endif
]])],
[CLANG=yes], [CLANG=no])
AC_LANG_POP([C++])

AC_MSG_RESULT([$CLANG])
AC_SUBST([CLANG])

if test "x$CLANG" = "xno"; then
  # Check the version of gcc and add any flags as appropriate. Note
  # that '-dumpversion' works for clang as well but as of clang 3.3 it
  # reports version 4.2.1 (for gcc backwards compatibility).
  GCC_VERSION="`${CC} -dumpversion`"
  AC_MSG_NOTICE([GCC version: $GCC_VERSION])
  test $? = 0 || AC_MSG_ERROR([failed to determine version of gcc])

  # Check for GCC version 4.4.
  AX_COMPARE_VERSION([$GCC_VERSION], [eq2], [4.4],
                     [is_gxx44=yes], [is_gxx44=no])
  if test "x$is_gxx44" = "xyes"; then
    AC_MSG_NOTICE([Setting up CXXFLAGS for g++-4.4])
    # We fail to build some protobuf generated code with gcc 4.4
    # without setting -fno-strict-aliasing.
    CFLAGS="$CFLAGS -fno-strict-aliasing"
    CXXFLAGS="$CXXFLAGS -fno-strict-aliasing"
  fi

  # Check for GCC version >= 4.8.
  AX_COMPARE_VERSION([$GCC_VERSION], [ge], [4.8],
                     [is_ge_gxx48=yes], [is_ge_gxx48=no])
  if test "x$is_ge_gxx48" = "xyes"; then
    AC_MSG_NOTICE([Setting up CXXFLAGS for g++ version >= 4.8])
    # Boost 1.53.0 fails to compile with GCC 4.8 without
    # -Wno-unused-local-typedefs, and automake does not recognize the
    # flag.
    # TODO(brenden): Remove this when Boost has a resolution.
    CFLAGS="${CFLAGS} -Wno-unused-local-typedefs"
    CXXFLAGS="${CXXFLAGS} -Wno-unused-local-typedefs"
  fi

  # Check for GCC version == 4.7 and fail. Starting with 4.7 the '#if
  # __cplusplus >= 201103L' will evaluate to true which means the
  # C++11 code paths will be compiled but certain C++11 features that
  # we use are not supported by 4.7. Since we're requiring C++11 going
  # forward we can't support this compiler.
  AX_COMPARE_VERSION([$GCC_VERSION], [eq2], [4.7],
                     [is_gxx47=yes], [is_gxx47=no])
  if test "x$is_gxx47" = "xyes"; then
    AC_MSG_ERROR([Unable to build with g++-4.7 due to missing C++11 features])
  fi
fi

# Ensure we can build the C++11 features we expect, and set the std
# CXXFLAG as appropriate.
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])

# Check if headers and library were located.
AC_CHECK_HEADER([glog/logging.h],
                [AC_LINK_IFELSE(
                    [AC_LANG_PROGRAM([#include <glog/logging.h>], [])],
                    [],
                    [AC_MSG_ERROR([glog is not installed.])])],
                [AC_MSG_ERROR([cannot find glog header.])])

AC_CHECK_HEADER([picojson.h],
                [],
                [AC_MSG_ERROR([picojson is not installed.])])

AC_CHECK_HEADERS([boost/lexical_cast.hpp boost/functional/hash.hpp],
                [],
                [AC_MSG_ERROR([boost is not installed.])])

AC_CHECK_HEADER([google/protobuf/message.h],
                [AC_LINK_IFELSE(
                    [AC_LANG_PROGRAM([#include <google/protobuf/message.h>], [])],
                    [],
                    [AC_MSG_ERROR([google protobuf is not installed.])])],
                [AC_MSG_ERROR([google protobuf is not installed.])])

# Check for protobuf.
AC_CHECK_TOOL([PROTOCOMPILER_TEST], [protoc], [], [$PROTOC_PATH])

if test -z "`echo $PROTOCOMPILER_TEST`"; then
  AC_MSG_ERROR([protoc not found in $PROTOC_PATH])
fi

PROTOCOMPILER="$PROTOC_PATH/protoc"

AC_SUBST(PROTOCOMPILER)

AC_OUTPUT
