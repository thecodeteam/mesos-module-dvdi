# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
libmesos_dvdi_isolator_la_SOURCES = isolator/dvdi.cpp ${CXX_PROTOS}
libmesos_dvdi_isolator_la_LDFLAGS = -release $(PACKAGE_VERSION) -shared $(MESOS_LDFLAGS)
