MAKEFLAGS := -j2

OS := $(shell uname -s)
ARCH := x86_64
UNAME := $(shell uname -a)
TARGZ := tar --strip=1 -xzf
TARJZ := tar --strip=1 -xjf

export CXX=g++-4.8
export CC=gcc-4.8

ifeq "$(origin USE_DOCKER)" "undefined"
	ifeq "$(origin DEPS_JOB_ID)" "undefined"
		ifneq (,$(findstring ubuntu,$(UNAME)))
			USE_DOCKER := false
		else
			USE_DOCKER := true
		endif
	else
		USE_DOCKER := true
	endif
endif

all: install

install: isolator

########################################################################
##                           Dependencies                             ##
########################################################################
DEPS_DIR := $(PWD)/.deps

########################################################################
##                            Subversion                              ##
########################################################################
SVN_VER := 1.9.2
SVN_TAR := subversion-$(SVN_VER).tar.bz2
SVN_URL := http://apache.mirrors.tds.net/subversion
SVN_SRC_DIR := $(DEPS_DIR)/subversion-$(SVN_VER)/src
SVN_OPT_DIR := $(DEPS_DIR)/subversion-$(SVN_VER)/opt
SVN_CONFIGURE := $(SVN_SRC_DIR)/configure
SVN_MAKEFILE := $(SVN_SRC_DIR)/Makefile
SVN_SRC_BIN := $(SVN_SRC_DIR)/subversion/svn/svn
SVN := $(SVN_OPT_DIR)/bin/svn

SQLITE_VER := 3071501
SQLITE_ZIP := sqlite-amalgamation-$(SQLITE_VER).zip
SQLITE_URL := http://www.sqlite.org
SQLITE_ZIP_DIR := $(SVN_SRC_DIR)/sqlite-amalgamation-$(SQLITE_VER)
SQLITE_C := $(SVN_SRC_DIR)/sqlite-amalgamation/sqlite3.c

svn-src: $(SVN_CONFIGURE)
$(SVN_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(SVN_URL)/$(SVN_TAR) && \
		$(TARJZ) $(SVN_TAR) && \
		rm -f $(SVN_TAR)

svn-sqlite: $(SQLITE_C)
$(SQLITE_C): $(SVN_CONFIGURE)
	cd $(<D) && \
		curl -SLO $(SQLITE_URL)/$(SQLITE_ZIP) && \
		unzip $(SQLITE_ZIP) && \
		rm -fr $(SQLITE_ZIP) && \
		mv $(SQLITE_ZIP_DIR) $(@D) && \
		touch $(@D) && touch $@

svn-configure: $(SVN_MAKEFILE)
$(SVN_MAKEFILE): $(SVN_CONFIGURE) $(SQLITE_C)
	cd $(@D) && $(@D)/configure --prefix=$(SVN_OPT_DIR)

svn-make: $(SVN_SRC_BIN)
$(SVN_SRC_BIN): $(SVN_MAKEFILE)
	cd $(<D) && $(MAKE)

svn: $(SVN)
$(SVN): $(SVN_MAKEFILE) $(SVN_SRC_BIN)
	cd $(<D) && $(MAKE) install

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
CMAKE_TAR := cmake-$(CMAKE_VER).tar.gz
CMAKE_URL := https://cmake.org/files/v3.3
CMAKE_SRC_DIR := $(DEPS_DIR)/cmake-$(CMAKE_VER)/src
CMAKE_OPT_DIR := $(DEPS_DIR)/cmake-$(CMAKE_VER)/opt
CMAKE_CONFIGURE := $(CMAKE_SRC_DIR)/configure
CMAKE_MAKEFILE := $(CMAKE_SRC_DIR)/Makefile
CMAKE_SRC_BIN := $(CMAKE_SRC_DIR)/bin/cmake
CMAKE := $(CMAKE_OPT_DIR)/bin/cmake

cmake-src: $(CMAKE_CONFIGURE)
$(CMAKE_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(CMAKE_URL)/$(CMAKE_TAR) && \
		$(TARGZ) $(CMAKE_TAR) && \
		rm -f $(CMAKE_TAR)

cmake-configure: $(CMAKE_MAKEFILE)
$(CMAKE_MAKEFILE): MAKEFLAGS=-j1
$(CMAKE_MAKEFILE): $(CMAKE_CONFIGURE)
	cd $(<D) && $< --prefix=$(CMAKE_OPT_DIR)

cmake-make: $(CMAKE_SRC_BIN)
$(CMAKE_SRC_BIN): $(CMAKE_MAKEFILE)
	cd $(<D) && $(MAKE)

cmake: $(CMAKE)
$(CMAKE): $(CMAKE_MAKEFILE) $(CMAKE_SRC_BIN)
	cd $(<D) && $(MAKE) install && touch $@

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
GLOG_TAR := v$(GLOG_VER).tar.gz
GLOG_URL := https://github.com/google/glog/archive
GLOG_SRC_DIR := $(DEPS_DIR)/glog-$(GLOG_VER)/src
GLOG_OPT_DIR := $(DEPS_DIR)/glog-$(GLOG_VER)/opt
GLOG_CONFIGURE := $(GLOG_SRC_DIR)/configure
GLOG_MAKEFILE := $(GLOG_SRC_DIR)/Makefile
GLOG_SRC_LIB := $(GLOG_SRC_DIR)/.libs/libglog.a
GLOG := $(GLOG_OPT_DIR)/lib/libglog.a

glog-src: $(GLOG_CONFIGURE)
$(GLOG_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(GLOG_URL)/$(GLOG_TAR) && \
		$(TARGZ) $(GLOG_TAR) && \
		rm -f $(GLOG_TAR)

glog-configure: $(GLOG_MAKEFILE)
$(GLOG_MAKEFILE): $(GLOG_CONFIGURE)
	cd $(<D) && $< --prefix=$(GLOG_OPT_DIR)

glog-make: $(GLOG_SRC_LIB)
$(GLOG_SRC_LIB): $(GLOG_MAKEFILE)
	cd $(<D) && $(MAKE)

glog: $(GLOG)
$(GLOG): $(GLOG_MAKEFILE) $(GLOG_SRC_BIN)
	cd $(<D) && $(MAKE) install

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
PICOJSON_URL := https://raw.githubusercontent.com/kazuho/picojson
PICOJSON_OPT_DIR := $(DEPS_DIR)/picojson-$(PICOJSON_VER)/opt
PICOJSON := $(PICOJSON_OPT_DIR)/include/picojson.h

picojson: $(PICOJSON)
$(PICOJSON):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(PICOJSON_URL)/v$(PICOJSON_VER)/picojson.h

picojson-clean:
	rm -fr $(PICOJSON_OPT_DIR)

########################################################################
##                                Boost                               ##
########################################################################
BOOST_VER := 1.59.0
BOOST_VER_ := $(subst .,_,$(BOOST_VER))
BOOST_TGZ := boost_$(BOOST_VER_).tar.gz
BOOST_URL := http://downloads.sourceforge.net/project/boost/boost
BOOST_OPT_DIR := $(DEPS_DIR)/boost-$(BOOST_VER)/opt
BOOST := $(BOOST_OPT_DIR)/INSTALL

boost: $(BOOST)
$(BOOST):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(BOOST_URL)/$(BOOST_VER)/$(BOOST_TGZ) && \
		$(TARGZ) $(BOOST_TGZ) && \
		rm -f $(BOOST_TGZ)

boost-clean:
	rm -fr $(BOOST_OPT_DIR)

########################################################################
##                                Boto                                ##
########################################################################
BOTO_VER := 2.2.2
BOTO_TGZ := python-boto_$(BOTO_VER).orig.tar.gz
BOTO_URL := https://launchpad.net/ubuntu/+archive/primary/+files/
BOTO_OPT_DIR := $(DEPS_DIR)/boto-$(BOTO_VER)/opt
BOTO := $(BOTO_OPT_DIR)/PKG-INFO

boto: $(BOTO)
$(BOTO):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(BOTO_URL)/$(BOTO_TGZ) && \
		$(TARGZ) $(BOTO_TGZ) && \
		rm -f $(BOTO_TGZ)

boto-clean:
	rm -fr $(BOTO_OPT_DIR)

########################################################################
##                              Protobuf                              ##
########################################################################
PBUF_VER := 2.5.0
PBUF_TAR := protobuf-$(PBUF_VER).tar.gz
PBUF_URL := https://github.com/google/protobuf/releases/download
PBUF_JAR_URL := http://search.maven.org/remotecontent?filepath=com/google/protobuf/protobuf-java/$(PBUF_VER)/protobuf-java-$(PBUF_VER).jar
PBUF_SRC_DIR := $(DEPS_DIR)/protobuf-$(PBUF_VER)/src
PBUF_OPT_DIR := $(DEPS_DIR)/protobuf-$(PBUF_VER)/opt
PBUF_CONFIGURE := $(PBUF_SRC_DIR)/configure
PBUF_MAKEFILE := $(PBUF_SRC_DIR)/Makefile
PBUF_SRC_BIN := $(PBUF_SRC_DIR)/src/protoc
PBUF_OPT_BIN := $(PBUF_OPT_DIR)/bin/protoc
PROTOBUF_JAR := $(PBUF_OPT_DIR)/share/java/protobuf.jar
PROTOBUF := $(PBUF_OPT_BIN) $(PROTOBUF_JAR)

protobuf-src: $(PBUF_CONFIGURE)
$(PBUF_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(PBUF_URL)/v$(PBUF_VER)/$(PBUF_TAR) && \
		$(TARGZ) $(PBUF_TAR) && \
		rm -f $(PBUF_TAR)

protobuf-configure: $(PBUF_MAKEFILE)
$(PBUF_MAKEFILE): $(PBUF_CONFIGURE)
	cd $(<D) && $< --prefix=$(PBUF_OPT_DIR)

protobuf-make: $(PBUF_SRC_BIN)
$(PBUF_SRC_BIN): $(PBUF_MAKEFILE)
	cd $(<D) && $(MAKE)

protobuf-bin: $(PBUF_OPT_BIN)
$(PBUF_OPT_BIN): $(PBUF_MAKEFILE) $(PBUF_SRC_BIN)
	cd $(<D) && $(MAKE) install

protobuf-jar: $(PROTOBUF_JAR)
$(PROTOBUF_JAR): $(PBUF_OPT_BIN)
	mkdir -p $(@D) && \
		curl -SL -o protobuf.jar $(PBUF_JAR_URL) && \
		touch $@

protobuf: $(PROTOBUF)

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
MESOS_VER := 0.23.0
MESOS_TAR := mesos-$(MESOS_VER).tar.gz
MESOS_URL := http://archive.apache.org/dist/mesos
MESOS_SRC_DIR := $(DEPS_DIR)/mesos-$(MESOS_VER)/src
MESOS_OPT_DIR := $(DEPS_DIR)/mesos-$(MESOS_VER)/opt
MESOS_3RD_PTY := $(MESOS_SRC_DIR)/build/3rdparty/libprocess/3rdparty
MESOS_CONFIGURE := $(MESOS_SRC_DIR)/configure
MESOS_MAKEFILE := $(MESOS_SRC_DIR)/build/Makefile
MESOS_SRC_BIN := $(MESOS_SRC_DIR)/build/src/mesos
MESOS := $(MESOS_OPT_DIR)/bin/mesos

mesos-src: $(MESOS_CONFIGURE)
$(MESOS_CONFIGURE): $(SVN) $(BOOST) $(GLOG) $(PROTOBUF) $(PICOJSON)
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(MESOS_URL)/$(MESOS_VER)/$(MESOS_TAR) && \
		$(TARGZ) $(MESOS_TAR) && \
		rm -f $(MESOS_TAR) && \
		touch $@

mesos-configure: $(MESOS_MAKEFILE)
$(MESOS_MAKEFILE): CXXFLAGS=-I$(GLOG_OPT_DIR) \
														-I$(PICOJSON_OPT_DIR)/include \
														-I$(BOOST_OPT_DIR) \
														-I$(PBUF_OPT_DIR)/include
$(MESOS_MAKEFILE): PYTHONPATH=$(BOTO_OPT_DIR)
$(MESOS_MAKEFILE): $(MESOS_CONFIGURE)
	mkdir -p $(@D) && \
		cd $(@D) && \
		env CXXFLAGS="$(CXXFLAGS)" \
				CPPFLAGS="$(CXXFLAGS)" \
				PYTHONPATH="$(PYTHONPATH):$$PYTHONPATH" \
			$< \
			--prefix=$(MESOS_OPT_DIR) \
	 		--disable-java \
			--disable-optimize \
			--with-svn=$(SVN_OPT_DIR) \
			--with-boost=$(BOOST_OPT_DIR) \
			--with-protobuf=$(PBUF_OPT_DIR) \
			--with-picojson=$(PICOJSON_OPT_DIR) \
			--with-glog=$(GLOG_OPT_DIR)

mesos-make: $(MESOS_SRC_BIN)
$(MESOS_SRC_BIN): MAKEFLAGS=-j1
$(MESOS_SRC_BIN): $(MESOS_MAKEFILE)
	cd $(<D) && $(MAKE)

mesos: $(MESOS)
$(MESOS): $(MESOS_SRC_BIN)
	cd $(<D) && $(MAKE) install

mesos-clean-src:
	rm -fr $(MESOS_SRC_DIR)

mesos-clean-build:
	rm -fr $(MESOS_SRC_DIR)/build

mesos-clean-opt:
	rm -fr $(MESOS_OPT_DIR)

mesos-clean: mesos-clean-src mesos-clean-opt

########################################################################
##                              Autoconf                              ##
########################################################################
AUTOCONF_VER := 2.69
AUTOCONF_TAR := autoconf-$(AUTOCONF_VER).tar.gz
AUTOCONF_URL := http://ftp.gnu.org/gnu/autoconf
AUTOCONF_SRC_DIR := $(DEPS_DIR)/autoconf-$(AUTOCONF_VER)/src
AUTOCONF_OPT_DIR := $(DEPS_DIR)/autoconf-$(AUTOCONF_VER)/opt
AUTOCONF_CONFIGURE := $(AUTOCONF_SRC_DIR)/configure
AUTOCONF_MAKEFILE := $(AUTOCONF_SRC_DIR)/Makefile
AUTOCONF_SRC_BIN := $(AUTOCONF_SRC_DIR)/bin/autoconf
AUTOCONF := $(AUTOCONF_OPT_DIR)/bin/autoconf

autoconf-src: $(AUTOCONF_CONFIGURE)
$(AUTOCONF_CONFIGURE):
	mkdir -p $(@D) && cd $(@D) && \
		curl -SLO $(AUTOCONF_URL)/$(AUTOCONF_TAR) && \
		$(TARGZ) $(AUTOCONF_TAR) && \
		rm -f $(AUTOCONF_TAR)

autoconf-configure: $(AUTOCONF_MAKEFILE)
$(AUTOCONF_MAKEFILE): $(AUTOCONF_CONFIGURE)
	cd $(<D) && $< --prefix=$(AUTOCONF_OPT_DIR)

autoconf-make: $(AUTOCONF_SRC_LIB)
$(AUTOCONF_SRC_BIN): $(AUTOCONF_MAKEFILE)
	cd $(<D) && $(MAKE)

autoconf: $(AUTOCONF)
$(AUTOCONF): $(AUTOCONF_MAKEFILE) $(AUTOCONF_SRC_BIN)
	cd $(<D) && $(MAKE) install

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
ISO_VER := $(MESOS_VER)
ISO_SRC_DIR := $(PWD)/isolator
ISO_CONFIGURE := $(ISO_SRC_DIR)/configure
ISO_MAKEFILE := $(ISO_SRC_DIR)/build/Makefile
ISO_SRC_LIB := $(ISO_SRC_DIR)/build/.libs/libmesos_dvdi_isolator-$(MESOS_VER).so

iso-bootstrap: $(ISO_CONFIGURE)
$(ISO_CONFIGURE): $(BOOST) $(GLOG) $(PROTOBUF) $(PICOJSON) $(MESOS) $(AUTOCONF) 
	cd $(@D) && env PATH=$(dir $(AUTOCONF)):$$PATH ./bootstrap

iso-configure: $(ISO_MAKEFILE)
$(ISO_MAKEFILE): CXXFLAGS=-I$(GLOG_OPT_DIR)/include \
													-I$(PICOJSON_OPT_DIR)/include \
													-I$(BOOST_OPT_DIR) \
													-I$(PBUF_OPT_DIR)/include
$(ISO_MAKEFILE): $(ISO_CONFIGURE)
	mkdir -p $(@D) && cd $(@D) && \
		env CXXFLAGS="$(CXXFLAGS)" \
				CPPFLAGS="$(CXXFLAGS)" \
			$< \
			--with-mesos-root=$(MESOS_SRC_DIR) \
			--with-mesos-build-dir=$(MESOS_SRC_DIR)/build \
			--with-protobuf=$(PBUF_OPT_DIR)

iso-make: $(ISO_SRC_LIB)
isolator: $(ISO_SRC_LIB)
ifeq (true,$(USE_DOCKER))
$(ISO_SRC_LIB):
	docker run -ti -v $(PWD):/isolator emccode/mesos-module-dvdi-dev:$(MESOS_VER)
else
$(ISO_SRC_LIB): MAKEFLAGS=-j1
$(ISO_SRC_LIB): $(ISO_MAKEFILE)
	cd $(<D) && $(MAKE)
endif

iso-clean:
	rm -fr $(ISO_SRC_DIR)/build

clean: iso-clean

.phony: clean