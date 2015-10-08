OS := $(shell uname -s)
ARCH := x86_64
UNAME := $(shell uname -a)

PWD := $(shell pwd)

MESOS_DIR := $(PWD)/mesos
MESOS_BUILD_DIR := $(MESOS_DIR)/build/$(OS)-$(ARCH)
MESOS_VERSION := 0.23.0

ISOLATOR_DIR := $(PWD)/isolator
INC_DIR := $(ISOLATOR_DIR)/include
BUILD_DIR := $(ISOLATOR_DIR)/build
LIBMESOS_DVDI_ISOLATOR := $(BUILD_DIR)/.libs/libmesos_dvdi_isolator-$(MESOS_VERSION).so
LIBS := $(LIBMESOS_DVDI_ISOLATOR)
SRCS := $(wildcard $(ISOLATOR_DIR)/isolator/*.cpp) \
        $(wildcard $(ISOLATOR_DIR)/isolator/*.hpp) \
        $(wildcard $(ISOLATOR_DIR)/isolator/*.proto)

APT_REPO_CMAKE := /etc/apt/sources.list.d/george-edison55-cmake-3_x-trusty.list
PICOJSON_H := $(INC_DIR)/picojson.h

ifeq "$(origin USE_DOCKER)" "undefined"
	ifeq "$(origin TRAVIS_JOB_ID)" "undefined"
		ifneq (,$(findstring ubuntu,$(UNAME)))
			USE_DOCKER := false
		else
			USE_DOCKER := true
		endif
	else
		USE_DOCKER := true
	endif
endif
USE_DOCKER ?:= $(TRAVIS_JOB_ID)

ifeq (true,$(USE_DOCKER))
$(LIBS): $(SRCS)
	docker run -ti -v $(PWD):/isolator emccode/mesos-module-dvdi-dev:$(MESOS_VERSION)
else
$(LIBS): $(MESOS_BUILD_DIR) $(SRCS)
	cd $(ISOLATOR_DIR); \
		./bootstrap; \
		rm -fr $(BUILD_DIR); \
		mkdir -p $(BUILD_DIR); \
		cd $(BUILD_DIR); \
		../configure \
			CXXFLAGS="-I$(INC_DIR)" \
			--with-mesos-root=$(MESOS_DIR) \
			--with-mesos-build-dir=$(MESOS_BUILD_DIR); \
		make
endif

all: install

install: $(LIBS)

$(MESOS_DIR):
	git clone git://git.apache.org/mesos.git $(MESOS_DIR); \
		cd $(MESOS_DIR); \
		git checkout 4ce5475346a0abb7ef4b7ffc9836c5836d7c7a66; \
		git log -n 1

$(MESOS_BUILD_DIR): $(MESOS_DIR)
	cd $(MESOS_DIR) && ./bootstrap

	mkdir -p $(MESOS_BUILD_DIR); \
		cd $(MESOS_BUILD_DIR); \
		$(MESOS_DIR)/configure \
			--disable-java \
			--disable-optimize \
			--without-included-zookeeper
		
	cd $(MESOS_BUILD_DIR) && make

	cd $(MESOS_DIR) && easy_install $(MESOS_BUILD_DIR)/src/python/dist/mesos.interface-*.egg
	cd $(MESOS_DIR) && easy_install $(MESOS_BUILD_DIR)/src/python/dist/mesos.native-*.egg

clean:
	rm -fr isolator/build

clean-mesos: 
	rm -fr $(MESOS_BUILD_DIR)

$(APT_REPO_CMAKE):
	add-apt-repository -y ppa:george-edison55/cmake-3.x 

apt-packages: $(APT_REPO_CMAKE)
		apt-get update && \
			apt-get install -y \
				build-essential                         \
				autoconf                                \
				automake                                \
				cmake=3.2.2-2ubuntu2~ubuntu14.04.1~ppa1 \
				ca-certificates                         \
				gdb                                     \
				wget                                    \
				git-core                                \
				libcurl4-nss-dev                        \
				libsasl2-dev                            \
				libtool                                 \
				libsvn-dev                              \
				libapr1-dev                             \
				libgoogle-glog-dev                      \
				libboost-dev                            \
				protobuf-compiler                       \
				libprotobuf-dev                         \
				make                                    \
				python                                  \
				python2.7                               \
				libpython-dev                           \
				python-dev                              \
				python-protobuf                         \
				python-setuptools                       \
				heimdal-clients                         \
				libsasl2-modules-gssapi-heimdal         \
				unzip                                   \
				--no-install-recommends

$(INC_DIR):
	mkdir -p $(INC_DIR)

$(PICOJSON_H): $(INC_DIR)
	wget -O $(PICOJSON_H) \
			https://raw.githubusercontent.com/kazuho/picojson/v1.3.0/picojson.h 

install-deps: apt-packages $(PICOJSON_H)

.phony: clean clean-mesos install-deps