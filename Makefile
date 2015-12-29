# MESOS_VERSIONS is a list of space, separated versions of mesos that
# will be built.
MESOS_VERSIONS := 0.23.1 0.24.1 0.25.0

# ISO_VERSIONS is either equal to or a subset of the MESOS_VERSIONS
# list. The versions in this list are the versions of Mesos against
# which to build the isolator module.
ISO_VERSIONS := 0.23.1 0.24.1 0.25.0

########################################################################
##                             MAKEFLAGS                              ##
########################################################################
ifeq "$(origin MAKEFLAGS)" "undefined"
MAKEFLAGS := -j2
endif
ifeq ($(strip $(MAKEFLAGS)),)
MAKEFLAGS := -j2
endif

########################################################################
##                               OS/Arch                              ##
########################################################################
OS := $(shell uname -s)
ARCH := x86_64
UNAME := $(shell uname -a)
ifneq ($(origin TRAVIS_BRANCH), undefined)
U1204 := ubuntu
else
U1204 := $(findstring ubuntu,$(UNAME))
endif

########################################################################
##                               Version                              ##
########################################################################
# parse a semver
SEMVER_PATT := ^[^\d]*(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z].+?))?(?:-(\d+)-g(.+?)(?:-(dirty))?)?$$
PARSE_SEMVER = $(shell echo $(1) | perl -pe 's/$(SEMVER_PATT)/$(2)/gim')

# describe the git information and create a parsing function for it
GIT_DESCRIBE := $(shell git describe --tags --long --dirty)
PARSE_GIT_DESCRIBE = $(call PARSE_SEMVER,$(GIT_DESCRIBE),$(1))

# parse the version components from the git information
V_MAJOR := $(call PARSE_GIT_DESCRIBE,$$1)
V_MINOR := $(call PARSE_GIT_DESCRIBE,$$2)
V_PATCH := $(call PARSE_GIT_DESCRIBE,$$3)
V_NOTES := $(call PARSE_GIT_DESCRIBE,$$4)
V_BUILD := $(call PARSE_GIT_DESCRIBE,$$5)
V_SHA_SHORT := $(call PARSE_GIT_DESCRIBE,$$6)
V_DIRTY := $(call PARSE_GIT_DESCRIBE,$$7)

V_OS_ARCH := $(OS)-$(ARCH)

# the long commit hash
V_SHA_LONG := $(shell git show HEAD -s --format=%H)

# the branch name, possibly from travis-ci
ifeq ($(origin TRAVIS_BRANCH), undefined)
	TRAVIS_BRANCH := $(shell git branch | grep '*' | awk '{print $$2}')
else
	ifeq ($(strip $(TRAVIS_BRANCH)),)
		TRAVIS_BRANCH := $(shell git branch | grep '*' | awk '{print $$2}')
	endif
endif
ifeq ($(origin TRAVIS_TAG), undefined)
	TRAVIS_TAG := $(TRAVIS_BRANCH)
else
	ifeq ($(strip $(TRAVIS_TAG)),)
		TRAVIS_TAG := $(TRAVIS_BRANCH)
	endif
endif
V_BRANCH := $(TRAVIS_TAG)

# the build date as an epoch
V_EPOCH := $(shell date +%s)

# the build date
V_BUILD_DATE := $(shell perl -e 'use POSIX strftime; print strftime("%a, %d %b %Y %H:%M:%S %Z", localtime($(V_EPOCH)))')

# the release date as required by bintray
V_RELEASE_DATE := $(shell perl -e 'use POSIX strftime; print strftime("%Y-%m-%d", localtime($(V_EPOCH)))')

# init the semver
V_SEMVER := $(V_MAJOR).$(V_MINOR).$(V_PATCH)
ifneq ($(V_NOTES),)
	V_SEMVER := $(V_SEMVER)-$(V_NOTES)
endif

# get the version file's version
V_FILE := $(strip $(shell cat VERSION 2> /dev/null))

# append the build number and dirty values to the semver if appropriate
ifneq ($(V_BUILD),)
	ifneq ($(V_BUILD),0)
		# if the version file's version is different than the version parsed from the
		# git describe information then use the version file's version
		ifneq ($(V_SEMVER),$(V_FILE))
			V_MAJOR := $(call PARSE_SEMVER,$(V_FILE),$$1)
			V_MINOR := $(call PARSE_SEMVER,$(V_FILE),$$2)
			V_PATCH := $(call PARSE_SEMVER,$(V_FILE),$$3)
			V_NOTES := $(call PARSE_SEMVER,$(V_FILE),$$4)
			V_SEMVER := $(V_MAJOR).$(V_MINOR).$(V_PATCH)
			ifneq ($(V_NOTES),)
				V_SEMVER := $(V_SEMVER)-$(V_NOTES)
			endif
		endif
		V_SEMVER := $(V_SEMVER)+$(V_BUILD)
	endif
endif
ifeq ($(V_DIRTY),dirty)
	V_SEMVER := $(V_SEMVER)+$(V_DIRTY)
endif

########################################################################
##                                 Tar                                ##
########################################################################
TARGZ := tar --strip=1 -xzf
TARJZ := tar --strip=1 -xjf

########################################################################
##                                GCC/G++                             ##
########################################################################
export CXX=g++-4.8
export CC=gcc-4.8

########################################################################
##                                  Main                              ##
########################################################################
MESOS := $(addprefix mesos-,$(MESOS_VERSIONS))
ISOLATOR := $(addprefix isolator-,$(ISO_VERSIONS))

all: install
install: $(SVN) $(BOOST) $(BOTO) $(GLOG) $(PROTOBUF) $(PICOJSON)
install: $(MESOS)
install: $(AUTOCONF)
install: $(ISOLATOR)
install: bintray

########################################################################
##                           Dependencies                             ##
########################################################################
DEPS_DIR := /tmp/mesos-ubuntu-12.04-deps
DEPS_7ZS_DIR := $(DEPS_DIR)/.7zs
DEPS_URL := https://dl.bintray.com/emccode/mesos-module-dvdi/ubuntu-12.04-build-deps
USE_U1204_DEP = $(shell if [ "$(U1204)" != "" -a "$$(perl -e "\$$s=\`curl $(DEPS_URL)/$(1) --head --silent\`; \$$s=~m/(\\d{3})/;print \$$1")" -eq "200" ]; then printf "true"; else printf "false"; fi)
USE_U1204_CACHED_DEP = $(shell if [ "$(U1204)" != "" -a -e "$(DEPS_7ZS_DIR)/$(1)" ]; then printf "true"; else printf "false"; fi)

########################################################################
##                            Subversion                              ##
########################################################################
SVN_VER := 1.9.2
SVN_SRC_TAR := subversion-$(SVN_VER).tar.bz2
SVN_SRC_URL := http://apache.mirrors.tds.net/subversion
SVN_OPT_7Z := subversion-$(SVN_VER).7z
SVN_SRC_DIR := $(DEPS_DIR)/subversion-$(SVN_VER)/src
SVN_OPT_DIR := $(DEPS_DIR)/subversion-$(SVN_VER)/opt
SVN_CONFIGURE := $(SVN_SRC_DIR)/configure
SVN_MAKEFILE := $(SVN_SRC_DIR)/Makefile
SVN_SRC_BIN := $(SVN_SRC_DIR)/subversion/svn/svn
SVN := $(SVN_OPT_DIR)/bin/svn

SQLITE_VER := 3071501
SQLITE_SRC_ZIP := sqlite-amalgamation-$(SQLITE_VER).zip
SQLITE_SRC_URL := http://www.sqlite.org
SQLITE_SRC_ZIP_DIR := $(SVN_SRC_DIR)/sqlite-amalgamation-$(SQLITE_VER)
SQLITE_C := $(SVN_SRC_DIR)/sqlite-amalgamation/sqlite3.c

svn-src: $(SVN_CONFIGURE)
$(SVN_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(SVN_SRC_URL)/$(SVN_SRC_TAR) && \
		$(TARJZ) $(SVN_SRC_TAR) && \
		rm -f $(SVN_SRC_TAR)

svn-sqlite: $(SQLITE_C)
$(SQLITE_C): $(SVN_CONFIGURE)
	cd $(<D) && \
		curl -SLO $(SQLITE_SRC_URL)/$(SQLITE_SRC_ZIP) && \
		unzip $(SQLITE_SRC_ZIP) && \
		rm -fr $(SQLITE_SRC_ZIP) && \
		mv $(SQLITE_SRC_ZIP_DIR) $(@D) && \
		touch $(@D) && touch $@

svn-configure: $(SVN_MAKEFILE)
$(SVN_MAKEFILE): $(SVN_CONFIGURE) $(SQLITE_C)
	cd $(@D) && $(@D)/configure --prefix=$(SVN_OPT_DIR)

ifeq "$(origin SVN_MAKEFLAGS)" "undefined"
SVN_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(SVN_MAKEFLAGS)),)
SVN_MAKEFLAGS := $(MAKEFLAGS)
endif

svn-make: $(SVN_SRC_BIN)
$(SVN_SRC_BIN): MAKEFLAGS=$(SVN_MAKEFLAGS)
$(SVN_SRC_BIN): $(SVN_MAKEFILE)
	cd $(<D) && $(MAKE)

svn: $(SVN)
ifeq ($(call USE_U1204_CACHED_DEP,$(SVN_OPT_7Z)),true)
$(SVN):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(SVN_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(SVN_OPT_7Z)),true)
$(SVN):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(SVN_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(SVN_OPT_7Z) > /dev/null
else
$(SVN): MAKEFLAGS=$(SVN_MAKEFLAGS)
$(SVN): $(SVN_MAKEFILE) $(SVN_SRC_BIN)
	cd $(<D) && $(MAKE) install
endif
endif

svn-clean-src:
	rm -fr $(SVN_SRC_DIR)

svn-clean-opt:
	rm -fr $(SVN_OPT_DIR)

svn-clean: svn-clean-src svn-clean-opt

svn-touch:
	touch $(SVN_CONFIGURE) \
				$(SQLITE_C) \
				$(SVN_MAKEFILE) \
				$(SVN_SRC_BIN) \
				$(SVN)

########################################################################
##                               CMake                                ##
########################################################################
CMAKE_VER := 3.3.2
CMAKE_SRC_TAR := cmake-$(CMAKE_VER).tar.gz
CMAKE_SRC_URL := https://cmake.org/files/v3.3
CMAKE_OPT_7Z := cmake-$(CMAKE_VER).7z
CMAKE_SRC_DIR := $(DEPS_DIR)/cmake-$(CMAKE_VER)/src
CMAKE_OPT_DIR := $(DEPS_DIR)/cmake-$(CMAKE_VER)/opt
CMAKE_CONFIGURE := $(CMAKE_SRC_DIR)/configure
CMAKE_MAKEFILE := $(CMAKE_SRC_DIR)/Makefile
CMAKE_SRC_BIN := $(CMAKE_SRC_DIR)/bin/cmake
CMAKE := $(CMAKE_OPT_DIR)/bin/cmake

cmake-src: $(CMAKE_CONFIGURE)
$(CMAKE_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(CMAKE_SRC_URL)/$(CMAKE_SRC_TAR) && \
		$(TARGZ) $(CMAKE_SRC_TAR) && \
		rm -f $(CMAKE_SRC_TAR)

ifeq "$(origin CMAKE_MAKEFLAGS)" "undefined"
CMAKE_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(CMAKE_MAKEFLAGS)),)
CMAKE_MAKEFLAGS := $(MAKEFLAGS)
endif

cmake-configure: $(CMAKE_MAKEFILE)
$(CMAKE_MAKEFILE): MAKEFLAGS=$(CMAKE_MAKEFLAGS)
$(CMAKE_MAKEFILE): $(CMAKE_CONFIGURE)
	cd $(<D) && $< --prefix=$(CMAKE_OPT_DIR)

cmake-make: $(CMAKE_SRC_BIN)
$(CMAKE_SRC_BIN): MAKEFLAGS=$(CMAKE_MAKEFLAGS)
$(CMAKE_SRC_BIN): $(CMAKE_MAKEFILE)
	cd $(<D) && $(MAKE)

cmake: $(CMAKE)
ifeq ($(call USE_U1204_CACHED_DEP,$(CMAKE_OPT_7Z)),true)
$(CMAKE):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(CMAKE_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(CMAKE_OPT_7Z)),true)
$(CMAKE):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(CMAKE_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(CMAKE_OPT_7Z) > /dev/null
else
$(CMAKE): MAKEFLAGS=$(CMAKE_MAKEFLAGS)
$(CMAKE): $(CMAKE_MAKEFILE) $(CMAKE_SRC_BIN)
	cd $(<D) && $(MAKE) install && touch $@
endif
endif

cmake-clean-src:
	rm -fr $(CMAKE_SRC_DIR)

cmake-clean-opt:
	rm -fr $(CMAKE_OPT_DIR)

cmake-clean: cmake-clean-src cmake-clean-opt

cmake-touch:
	touch $(CMAKE_CONFIGURE) \
				$(CMAKE_MAKEFILE) \
				$(CMAKE_SRC_BIN) \
				$(CMAKE)

########################################################################
##                               GLog                                 ##
########################################################################
GLOG_VER := 0.3.4
GLOG_SRC_TAR := v$(GLOG_VER).tar.gz
GLOG_SRC_URL := https://github.com/google/glog/archive
GLOG_OPT_7Z := glog-$(GLOG_VER).7z
GLOG_SRC_DIR := $(DEPS_DIR)/glog-$(GLOG_VER)/src
GLOG_OPT_DIR := $(DEPS_DIR)/glog-$(GLOG_VER)/opt
GLOG_CONFIGURE := $(GLOG_SRC_DIR)/configure
GLOG_MAKEFILE := $(GLOG_SRC_DIR)/Makefile
GLOG_SRC_LIB := $(GLOG_SRC_DIR)/.libs/libglog.a
GLOG := $(GLOG_OPT_DIR)/lib/libglog.a

glog-src: $(GLOG_CONFIGURE)
$(GLOG_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(GLOG_SRC_URL)/$(GLOG_SRC_TAR) && \
		$(TARGZ) $(GLOG_SRC_TAR) && \
		rm -f $(GLOG_SRC_TAR)

glog-configure: $(GLOG_MAKEFILE)
$(GLOG_MAKEFILE): $(GLOG_CONFIGURE)
	cd $(<D) && $< --prefix=$(GLOG_OPT_DIR)

ifeq "$(origin GLOG_MAKEFLAGS)" "undefined"
GLOG_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(GLOG_MAKEFLAGS)),)
GLOG_MAKEFLAGS := $(MAKEFLAGS)
endif

glog-make: $(GLOG_SRC_LIB)
$(GLOG_SRC_LIB): MAKEFLAGS=$(GLOG_MAKEFLAGS)
$(GLOG_SRC_LIB): $(GLOG_MAKEFILE)
	cd $(<D) && $(MAKE)

glog: $(GLOG)
ifeq ($(call USE_U1204_CACHED_DEP,$(GLOG_OPT_7Z)),true)
$(GLOG):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(GLOG_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(GLOG_OPT_7Z)),true)
$(GLOG):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(GLOG_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(GLOG_OPT_7Z) > /dev/null
else
$(GLOG): MAKEFLAGS=$(CMAKE_MAKEFLAGS)
$(GLOG): $(GLOG_MAKEFILE) $(GLOG_SRC_BIN)
	cd $(<D) && $(MAKE) install
endif
endif

glog-clean-src:
	rm -fr $(GLOG_SRC_DIR)

glog-clean-opt:
	rm -fr $(GLOG_OPT_DIR)

glog-clean: glog-clean-src glog-clean-opt

glog-touch:
	touch $(GLOG_CONFIGURE) \
				$(GLOG_MAKEFILE) \
				$(GLOG_SRC_LIB) \
				$(GLOG)

########################################################################
##                             PicoJSON                               ##
########################################################################
PICOJSON_VER := 1.3.0
PICOJSON_SRC_URL := https://raw.githubusercontent.com/kazuho/picojson
PICOJSON_OPT_7Z := picojson-$(PICOJSON_VER).7z
PICOJSON_OPT_URL := $(DEPS_URL)/$(PICOJSON_OPT_7Z)
PICOJSON_OPT_DIR := $(DEPS_DIR)/picojson-$(PICOJSON_VER)/opt
PICOJSON := $(PICOJSON_OPT_DIR)/include/picojson.h

picojson: $(PICOJSON)
ifeq ($(call USE_U1204_CACHED_DEP,$(PICOJSON_OPT_7Z)),true)
$(PICOJSON):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(PICOJSON_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(PICOJSON_OPT_7Z)),true)
$(PICOJSON):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(PICOJSON_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(PICOJSON_OPT_7Z) > /dev/null
else
$(PICOJSON):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(PICOJSON_SRC_URL)/v$(PICOJSON_VER)/picojson.h
endif
endif

picojson-clean:
	rm -fr $(PICOJSON_OPT_DIR)

########################################################################
##                                Boost                               ##
########################################################################
BOOST_VER := 1.59.0
BOOST_VER_ := $(subst .,_,$(BOOST_VER))
BOOST_TGZ := boost_$(BOOST_VER_).tar.gz
BOOST_SRC_URL := http://downloads.sourceforge.net/project/boost/boost
BOOST_OPT_7Z := boost-$(BOOST_VER).7z
BOOST_OPT_DIR := $(DEPS_DIR)/boost-$(BOOST_VER)/opt
BOOST := $(BOOST_OPT_DIR)/INSTALL

boost: $(BOOST)
ifeq ($(call USE_U1204_CACHED_DEP,$(BOOST_OPT_7Z)),true)
$(BOOST):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(BOOST_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(BOOST_OPT_7Z)),true)
$(BOOST):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(BOOST_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(BOOST_OPT_7Z) > /dev/null
else
$(BOOST):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(BOOST_SRC_URL)/$(BOOST_VER)/$(BOOST_TGZ) && \
		$(TARGZ) $(BOOST_TGZ) && \
		rm -f $(BOOST_TGZ)
endif
endif

boost-clean:
	rm -fr $(BOOST_OPT_DIR)

########################################################################
##                                Boto                                ##
########################################################################
BOTO_VER := 2.2.2
BOTO_TGZ := python-boto_$(BOTO_VER).orig.tar.gz
BOTO_SRC_URL := https://launchpad.net/ubuntu/+archive/primary/+files/
BOTO_OPT_7Z := boto-$(BOTO_VER).7z
BOTO_OPT_DIR := $(DEPS_DIR)/boto-$(BOTO_VER)/opt
BOTO := $(BOTO_OPT_DIR)/PKG-INFO

boto: $(BOTO)
ifeq ($(call USE_U1204_CACHED_DEP,$(BOTO_OPT_7Z)),true)
$(BOTO):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(BOTO_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(BOTO_OPT_7Z)),true)
$(BOTO):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(BOTO_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(BOTO_OPT_7Z) > /dev/null
else
$(BOTO):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(BOTO_SRC_URL)/$(BOTO_TGZ) && \
		$(TARGZ) $(BOTO_TGZ) && \
		rm -f $(BOTO_TGZ)
endif
endif

boto-clean:
	rm -fr $(BOTO_OPT_DIR)

########################################################################
##                              Protobuf                              ##
########################################################################
PBUF_VER := 2.5.0
PBUF_SRC_TAR := protobuf-$(PBUF_VER).tar.gz
PBUF_SRC_URL := https://github.com/google/protobuf/releases/download
PBUF_OPT_7Z := protobuf-$(PBUF_VER).7z
PBUF_JAR_SRC_URL := http://search.maven.org/remotecontent?filepath=com/google/protobuf/protobuf-java/$(PBUF_VER)/protobuf-java-$(PBUF_VER).jar
PBUF_SRC_DIR := $(DEPS_DIR)/protobuf-$(PBUF_VER)/src
PBUF_OPT_DIR := $(DEPS_DIR)/protobuf-$(PBUF_VER)/opt
PBUF_CONFIGURE := $(PBUF_SRC_DIR)/configure
PBUF_MAKEFILE := $(PBUF_SRC_DIR)/Makefile
PBUF_SRC_BIN := $(PBUF_SRC_DIR)/src/protoc
PBUF_OPT_BIN := $(PBUF_OPT_DIR)/bin/protoc
PROTOBUF := $(PBUF_OPT_DIR)/share/java/protobuf.jar

protobuf-src: $(PBUF_CONFIGURE)
$(PBUF_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(PBUF_SRC_URL)/v$(PBUF_VER)/$(PBUF_SRC_TAR) && \
		$(TARGZ) $(PBUF_SRC_TAR) && \
		rm -f $(PBUF_SRC_TAR)

protobuf-configure: $(PBUF_MAKEFILE)
$(PBUF_MAKEFILE): $(PBUF_CONFIGURE)
	cd $(<D) && $< --prefix=$(PBUF_OPT_DIR)

ifeq "$(origin PROTOBUF_MAKEFLAGS)" "undefined"
PROTOBUF_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(PROTOBUF_MAKEFLAGS)),)
PROTOBUF_MAKEFLAGS := $(MAKEFLAGS)
endif

protobuf-make: $(PBUF_SRC_BIN)
$(PBUF_SRC_BIN): MAKEFLAGS=$(PROTOBUF_MAKEFLAGS)
$(PBUF_SRC_BIN): $(PBUF_MAKEFILE)
	cd $(<D) && $(MAKE)

protobuf-bin: $(PBUF_OPT_BIN)
$(PBUF_OPT_BIN): MAKEFLAGS=$(PROTOBUF_MAKEFLAGS)
$(PBUF_OPT_BIN): $(PBUF_MAKEFILE) $(PBUF_SRC_BIN)
	cd $(<D) && $(MAKE) install

protobuf: $(PROTOBUF)
ifeq ($(call USE_U1204_CACHED_DEP,$(PBUF_OPT_7Z)),true)
$(PROTOBUF):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(PBUF_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(PBUF_OPT_7Z)),true)
$(PROTOBUF):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(PBUF_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(PBUF_OPT_7Z) > /dev/null
else
$(PROTOBUF): $(PBUF_OPT_BIN)
	mkdir -p $(@D) && \
		curl -SL -o protobuf.jar $(PBUF_JAR_SRC_URL) && \
		touch $@
endif
endif

protobuf-clean-src:
	rm -fr $(PBUF_SRC_DIR)

protobuf-clean-opt:
	rm -fr $(PBUF_OPT_DIR)

protobuf-clean: protobuf-clean-src protobuf-clean-opt

protobuf-touch:
	touch $(PBUF_CONFIGURE) \
				$(PBUF_MAKEFILE) \
				$(PBUF_SRC_BIN) \
				$(PBUF_OPT_BIN) \
				$(PROTOBUF_JAR)

########################################################################
##                                 Mesos                              ##
########################################################################
MESOS_DEPS := $(SVN) $(BOOST) $(BOTO) $(GLOG) $(PROTOBUF) $(PICOJSON)

ifeq "$(origin MESOS_MAKEFLAGS)" "undefined"
MESOS_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(MESOS_MAKEFLAGS)),)
MESOS_MAKEFLAGS := $(MAKEFLAGS)
endif

define MESOS_BUILD_RULES
MESOS_VER_$1 := $1
MESOS_SRC_TAR_$1 := mesos-$$(MESOS_VER_$1).tar.gz
MESOS_SRC_URL_$1 := http://archive.apache.org/dist/mesos
MESOS_OPT_7Z_$1 := mesos-$$(MESOS_VER_$1).7z
MESOS_SRC_DIR_$1 := $$(DEPS_DIR)/mesos-$$(MESOS_VER_$1)/src
MESOS_OPT_DIR_$1 := $$(DEPS_DIR)/mesos-$$(MESOS_VER_$1)/opt
MESOS_BUILD_DIR_$1 := $$(MESOS_SRC_DIR_$1)/build
MESOS_CONFIGURE_$1 := $$(MESOS_SRC_DIR_$1)/configure
MESOS_MAKEFILE_$1 := $$(MESOS_BUILD_DIR_$1)/Makefile
MESOS_SRC_BIN_$1 := $$(MESOS_BUILD_DIR_$1)/src/mesos
MESOS_$1 := $$(MESOS_OPT_DIR_$1)/bin/mesos

mesos-$1-src: $$(MESOS_CONFIGURE_$1)
$$(MESOS_CONFIGURE_$1):
	mkdir -p $$(@D) && cd $$(@D) && \
		curl -SLO $$(MESOS_SRC_URL_$1)/$$(MESOS_VER_$1)/$$(MESOS_SRC_TAR_$1) && \
		$$(TARGZ) $$(MESOS_SRC_TAR_$1) && \
		rm -f $$(MESOS_SRC_TAR_$1) && \
		touch $$@

mesos-$1-configure: $$(MESOS_MAKEFILE_$1)
$$(MESOS_MAKEFILE_$1): CXXFLAGS=-I$$(GLOG_OPT_DIR) \
														-I$$(PICOJSON_OPT_DIR)/include \
														-I$$(BOOST_OPT_DIR) \
														-I$$(PBUF_OPT_DIR)/include
$$(MESOS_MAKEFILE_$1): PYTHONPATH=$$(BOTO_OPT_DIR)
$$(MESOS_MAKEFILE_$1): $$(MESOS_CONFIGURE_$1) $$(MESOS_DEPS)
	mkdir -p $$(@D) && \
		cd $$(@D) && \
		env CXXFLAGS="$$(CXXFLAGS)" \
				CPPFLAGS="$$(CXXFLAGS)" \
				PYTHONPATH="$$(PYTHONPATH):$$$$PYTHONPATH" \
			$$< \
			--prefix=$$(MESOS_OPT_DIR_$1) \
	 		--disable-java \
			--disable-optimize \
			--with-svn=$$(SVN_OPT_DIR) \
			--with-boost=$$(BOOST_OPT_DIR) \
			--with-protobuf=$$(PBUF_OPT_DIR) \
			--with-picojson=$$(PICOJSON_OPT_DIR) \
			--with-glog=$$(GLOG_OPT_DIR)

mesos-$1-make: $$(MESOS_SRC_BIN_$1)
$$(MESOS_SRC_BIN_$1): MAKEFLAGS=$$(MESOS_MAKEFLAGS)
$$(MESOS_SRC_BIN_$1): $$(MESOS_MAKEFILE_$1)
	cd $$(<D) && $$(MAKE)

mesos-$1: $$(MESOS_$1)
ifeq ($$(call USE_U1204_CACHED_DEP,$$(MESOS_OPT_7Z_$1)),true)
$$(MESOS_$1): $$(MESOS_DEPS)
	cd $$(DEPS_DIR) && \
		7z x $$(DEPS_7ZS_DIR)/$$(MESOS_OPT_7Z_$1) > /dev/null
else
ifeq ($$(call USE_U1204_DEP,$$(MESOS_OPT_7Z_$1)),true)
$$(MESOS_$1): $$(MESOS_DEPS)
	mkdir -p $$(DEPS_7ZS_DIR) && \
		cd $$(DEPS_7ZS_DIR) && \
		curl -SLO $$(DEPS_URL)/$$(MESOS_OPT_7Z_$1) && \
		cd $$(DEPS_DIR) && \
		7z x $$(DEPS_7ZS_DIR)/$$(MESOS_OPT_7Z_$1) > /dev/null
else
$$(MESOS_$1): MAKEFLAGS=$$(MESOS_MAKEFLAGS)
$$(MESOS_$1): $$(MESOS_SRC_BIN_$1)
	cd $$(<D) && $$(MAKE) install
endif
endif

mesos-$1-clean-src:
	rm -fr $$(MESOS_SRC_DIR_$1)

mesos-$1-clean-build:
	rm -fr $$(MESOS_BUILD_DIR_$1)

mesos-$1-clean-opt:
	rm -fr $$(MESOS_OPT_DIR_$1)

mesos-$1-clean: mesos-$1-clean-src mesos-$1-clean-opt
endef

$(foreach V,$(MESOS_VERSIONS),$(eval $(call MESOS_BUILD_RULES,$(V))))

mesos-src: $(addsuffix -src,$(addprefix mesos-,$(MESOS_VERSIONS)))
mesos-configure: $(addsuffix -configure,$(addprefix mesos-,$(MESOS_VERSIONS)))
mesos-make: $(addsuffix -make,$(addprefix mesos-,$(MESOS_VERSIONS)))
mesos-clean: $(addsuffix -clean,$(addprefix mesos-,$(MESOS_VERSIONS)))
mesos: $(MESOS)

########################################################################
##                              Autoconf                              ##
########################################################################
AUTOCONF_VER := 2.69
AUTOCONF_SRC_TAR := autoconf-$(AUTOCONF_VER).tar.gz
AUTOCONF_SRC_URL := http://ftp.gnu.org/gnu/autoconf
AUTOCONF_OPT_7Z := autoconf-$(AUTOCONF_VER).7z
AUTOCONF_SRC_DIR := $(DEPS_DIR)/autoconf-$(AUTOCONF_VER)/src
AUTOCONF_OPT_DIR := $(DEPS_DIR)/autoconf-$(AUTOCONF_VER)/opt
AUTOCONF_CONFIGURE := $(AUTOCONF_SRC_DIR)/configure
AUTOCONF_MAKEFILE := $(AUTOCONF_SRC_DIR)/Makefile
AUTOCONF_SRC_BIN := $(AUTOCONF_SRC_DIR)/bin/autoconf
AUTOCONF := $(AUTOCONF_OPT_DIR)/bin/autoconf

autoconf-src: $(AUTOCONF_CONFIGURE)
$(AUTOCONF_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(AUTOCONF_SRC_URL)/$(AUTOCONF_SRC_TAR) && \
		$(TARGZ) $(AUTOCONF_SRC_TAR) && \
		rm -f $(AUTOCONF_SRC_TAR)

autoconf-configure: $(AUTOCONF_MAKEFILE)
$(AUTOCONF_MAKEFILE): $(AUTOCONF_CONFIGURE)
	cd $(<D) && $< --prefix=$(AUTOCONF_OPT_DIR)

ifeq "$(origin AUTOCONF_MAKEFLAGS)" "undefined"
AUTOCONF_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(AUTOCONF_MAKEFLAGS)),)
AUTOCONF_MAKEFLAGS := $(MAKEFLAGS)
endif

autoconf-make: $(AUTOCONF_SRC_LIB)
$(AUTOCONF_SRC_BIN): MAKEFLAGS=$(AUTOCONF_MAKEFLAGS)
$(AUTOCONF_SRC_BIN): $(AUTOCONF_MAKEFILE)
	cd $(<D) && $(MAKE)

autoconf: $(AUTOCONF)
ifeq ($(call USE_U1204_CACHED_DEP,$(AUTOCONF_OPT_7Z)),true)
$(AUTOCONF):
	cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(AUTOCONF_OPT_7Z) > /dev/null
else
ifeq ($(call USE_U1204_DEP,$(AUTOCONF_OPT_7Z)),true)
$(AUTOCONF):
	mkdir -p $(DEPS_7ZS_DIR) && \
		cd $(DEPS_7ZS_DIR) && \
		curl -SLO $(DEPS_URL)/$(AUTOCONF_OPT_7Z) && \
		cd $(DEPS_DIR) && \
		7z x $(DEPS_7ZS_DIR)/$(AUTOCONF_OPT_7Z) > /dev/null
else
$(AUTOCONF): MAKEFLAGS=$(AUTOCONF_MAKEFLAGS)
$(AUTOCONF): $(AUTOCONF_MAKEFILE) $(AUTOCONF_SRC_BIN)
	cd $(<D) && $(MAKE) install
endif
endif

autoconf-clean-src:
	rm -fr $(AUTOCONF_SRC_DIR)

autoconf-clean-opt:
	rm -fr $(AUTOCONF_OPT_DIR)

autoconf-clean: autoconf-clean-src autoconf-clean-opt

autoconf-touch:
	touch $(AUTOCONF_CONFIGURE) \
				$(AUTOCONF_MAKEFILE) \
				$(AUTOCONF_SRC_LIB) \
				$(AUTOCONF)

########################################################################
##                             Isolator                               ##
########################################################################

# if USE_DOCKER is undefined and this build is not on travis or on an
# ubuntu system then use docker for the build
ifeq "$(origin USE_DOCKER)" "undefined"
	ifeq "$(origin DEPS_JOB_ID)" "undefined"
		ifneq (,$(U1204))
			USE_DOCKER := false
		else
			USE_DOCKER := true
		endif
	else
		USE_DOCKER := true
	endif
endif

ifeq "$(origin ISOLATOR_MAKEFLAGS)" "undefined"
ISOLATOR_MAKEFLAGS := $(MAKEFLAGS)
endif
ifeq ($(strip $(ISOLATOR_MAKEFLAGS)),)
ISOLATOR_MAKEFLAGS := $(MAKEFLAGS)
endif

ISO_SRC_DIR := $(PWD)/isolator
ISO_DEPS := $(BOOST) $(GLOG) $(PROTOBUF) $(PICOJSON)
ISO_BOOTSTRAP := $(ISO_SRC_DIR)/bootstrap
ISO_SRCS := $(wildcard $(ISO_SRC_DIR)/isolator/*)

define ISOLATOR_BUILD_RULES
ISO_VER_$1 := $$(V_SEMVER)+mesos-$1
ISO_DEPS_$1 := $$(ISO_DEPS) $$(MESOS_$1)
ISO_WORK_DIR_$1 := $$(ISO_SRC_DIR)/build/$1
ISO_BOOTSTRAP_$1 := $$(ISO_WORK_DIR_$1)/bootstrap
ISO_CONFIGURE_$1 := $$(ISO_WORK_DIR_$1)/configure
ISO_BUILD_DIR_$1 := $$(ISO_WORK_DIR_$1)/build
ISO_MAKEFILE_$1 := $$(ISO_BUILD_DIR_$1)/Makefile
ISO_LIBDIR_$1 := $$(ISO_BUILD_DIR_$1)/.libs
ISO_SRC_LIB_$1 := $$(ISO_LIBDIR_$1)/libmesos_dvdi_isolator-$$(ISO_VER_$1).so

isolator-$1-src: $$(ISO_BOOTSTRAP_$1)
$$(ISO_BOOTSTRAP_$1): $$(ISO_BOOTSTRAP) $$(ISO_SRCS)
	mkdir -p $$(@D) && \
	for F in "$$$$(git ls-tree --name-only HEAD isolator/)"; do \
		cp -fr $$$$F $$(@D); \
	done

isolator-$1-bootstrap: $$(ISO_CONFIGURE_$1)
$$(ISO_CONFIGURE_$1): $$(ISO_BOOTSTRAP_$1) $(AUTOCONF)
	cd $$(<D) && \
		env \
			PATH="$$(dir $$(AUTOCONF)):$$$$PATH" \
			ISOLATOR_VERSION=$$(ISO_VER_$1) \
			$$<

isolator-$1-configure: $$(ISO_MAKEFILE_$1)
$$(ISO_MAKEFILE_$1): CXXFLAGS=-I$$(GLOG_OPT_DIR)/include \
													-I$$(PICOJSON_OPT_DIR)/include \
													-I$$(BOOST_OPT_DIR) \
													-I$$(PBUF_OPT_DIR)/include \
													-DMESOS_VERSION_INT=$$(subst .,,$1)
$$(ISO_MAKEFILE_$1): $$(ISO_CONFIGURE_$1) $$(ISO_DEPS_$1)
	mkdir -p $$(@D) && cd $$(@D) && \
		env CXXFLAGS="$$(CXXFLAGS)" \
				CPPFLAGS="$$(CXXFLAGS)" \
			$$< \
			--with-mesos-root=$$(MESOS_SRC_DIR_$1) \
			--with-mesos-build-dir=$$(MESOS_BUILD_DIR_$1) \
			--with-protobuf=$$(PBUF_OPT_DIR)

isolator-$1: $$(ISO_SRC_LIB_$1)
ifeq (true,$$(USE_DOCKER))
$$(ISO_SRC_LIB_$1):
	docker run -ti -v $$(PWD):/isolator emccode/mesos-module-dvdi-dev:$1
else
$$(ISO_SRC_LIB_$1): MAKEFLAGS=$$(ISOLATOR_MAKEFLAGS)
$$(ISO_SRC_LIB_$1): $$(ISO_MAKEFILE_$1)
	cd $$(<D) && $$(MAKE)
endif

isolator-$1-clean:
	rm -fr $$(ISO_WORK_DIR_$1)
endef

$(foreach V,$(ISO_VERSIONS),$(eval $(call ISOLATOR_BUILD_RULES,$(V))))

isolator-src: $(addsuffix -src,$(addprefix isolator-,$(ISO_VERSIONS)))
isolator-bootstrap: $(addsuffix -bootstrap,$(addprefix isolator-,$(ISO_VERSIONS)))
isolator-configure: $(addsuffix -configure,$(addprefix isolator-,$(ISO_VERSIONS)))
isolator-clean: $(addsuffix -clean,$(addprefix isolator-,$(ISO_VERSIONS)))
isolator: $(ISOLATOR)

bintray: bintray-unstable-filtered.json
bintray-unstable-filtered.json: bintray-unstable.json
	sed -e 's/$${SEMVER}/$(V_SEMVER)/g' \
		-e 's|$${DSCRIP}|$(V_SEMVER).Branch.$(V_BRANCH).Sha.$(V_SHA_LONG)|g' \
		-e 's/$${RELDTE}/$(V_RELEASE_DATE)/g' \
		bintray-unstable.json > bintray-unstable-filtered.json

print-version:
	@echo SemVer: $(V_SEMVER)
	@echo Branch: $(V_BRANCH)
	@echo Commit: $(V_SHA_LONG)
	@echo Formed: $(V_BUILD_DATE)

.phony: print-version isolator-clean mesos-clean
