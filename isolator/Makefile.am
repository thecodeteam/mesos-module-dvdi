# This file is © 2014 Mesosphere, Inc. (“Mesosphere”). Mesosphere
# licenses this file to you solely pursuant to the following terms
# (and you may not use this file except in compliance with such
# terms):
#
# 1) Subject to your compliance with the following terms, Mesosphere
# hereby grants you a nonexclusive, limited, personal,
# non-sublicensable, non-transferable, royalty-free license to use
# this file solely for your internal business purposes.
#
# 2) You may not (and agree not to, and not to authorize or enable
# others to), directly or indirectly:
#   (a) copy, distribute, rent, lease, timeshare, operate a service
#   bureau, or otherwise use for the benefit of a third party, this
#   file; or
#
#   (b) remove any proprietary notices from this file.  Except as
#   expressly set forth herein, as between you and Mesosphere,
#   Mesosphere retains all right, title and interest in and to this
#   file.
#
# 3) Unless required by applicable law or otherwise agreed to in
# writing, Mesosphere provides this file on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied,
# including, without limitation, any warranties or conditions of
# TITLE, NON-INFRINGEMENT, MERCHANTABILITY, or FITNESS FOR A
# PARTICULAR PURPOSE.
#
# 4) In no event and under no legal theory, whether in tort (including
# negligence), contract, or otherwise, unless required by applicable
# law (such as deliberate and grossly negligent acts) or agreed to in
# writing, shall Mesosphere be liable to you for damages, including
# any direct, indirect, special, incidental, or consequential damages
# of any character arising as a result of these terms or out of the
# use or inability to use this file (including but not limited to
# damages for loss of goodwill, work stoppage, computer failure or
# malfunction, or any and all other commercial damages or losses),
# even if Mesosphere has been advised of the possibility of such
# damages.

AUTOMAKE_OPTIONS = subdir-objects
ACLOCAL_AMFLAGS = -I m4

PROTOC = @PROTOCOMPILER@
PROTOCFLAGS=-I$(top_srcdir)/isolator ${MESOS_INCLUDE_PATH}

# We want to install modules in mesos directory.
pkglibdir = $(libdir)/mesos

# Initialize variables here so we can use += operator everywhere else.
pkglib_LTLIBRARIES =
bin_PROGRAMS =
BUILT_SOURCES =
CLEANFILES =

# Add compiler and linker flags for pthreads.
AM_CXXFLAGS = $(PTHREAD_CFLAGS)
AM_LIBS = $(PTHREAD_LIBS)

# Add top-level include dir to our include path.
MESOS_CPPFLAGS += -I$(top_srcdir)/include

# Setup CPPFLAGS that are used for most source files.
AM_CPPFLAGS = $(MESOS_CPPFLAGS) -Wall -Werror

CXX_PROTOS = isolator/interface.pb.cc isolator/interface.pb.h

BUILT_SOURCES += $(CXX_PROTOS)
CLEANFILES += $(CXX_PROTOS)

%.pb.cc %.pb.h: %.proto
	$(PROTOC) $(PROTOCFLAGS) --cpp_out=isolator/. $^

# Library containing kerberos ticket forwarding module.
pkglib_LTLIBRARIES += libmesos_dvdi_isolator.la
libmesos_dvdi_isolator_la_SOURCES = isolator/network_isolator.cpp ${CXX_PROTOS}
libmesos_dvdi_isolator_la_LDFLAGS = -release $(PACKAGE_VERSION) -shared $(MESOS_LDFLAGS)
