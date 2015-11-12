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

Name:           dvdi-modules
Version:        0.25.0
Release:        0%{?dist}
License:        Apache License, Version 2.0
Summary:        docker volume driver isolator module
Url:            https://github.com/cantbewong/docker-volume-driver-isolator
Source0:        dvdi-modules-0.25.0tar.gz

# This package is functional only on i386 and x86_64 architectures.
ExclusiveArch:	%ix86 x86_64

%description
docker volume driver isolator module.

%prep
%setup -q

%build
# Replace $HOME/usr with your Mesos install location (/usr/local).
%configure --with-mesos=$HOME/usr
make %{?_smp_mflags}

%install
make install DESTDIR=%{buildroot} %{?_smp_mflags}

%post

%postun

%files
%defattr(-,root,root)
%{_libdir}/mesos

%changelog
* Wed Nov 11 2015 Steve Wong <steven.wong@emc.com> - 0.2.0
* Sat Sep 19 2015 Steve Wong <steven.wong@emc.com> - 0.1
- Preparing for initial packaging.
