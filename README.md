# docker-volume-driver-isolator
This is an Isolator for Mesos that provides external storage management for Mesos containers

for all builds  (docker host should have these already)


install bash-completion maven autoconf automake libtool subversion

install protobug-2.5.0
Mesos has this in tar.gz form but it must be uncompressed and installed
in the mesos/3rdparty/libprocess/3rdparty/protobuf-2.5.0 directory
also install gtest, glog, and picojson

export MESOS_HOME=/path/to/mesos
export GTEST_HOME=/path/to/gtest-1.7.0  
MAVEN_HOME = /usr/local/Cellar/maven/3.3.1



To build with vagrant docker image:

cd isolator
./bootstrap
rm -rf build
mkdir build
cd build
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:/usr/local/lib
../configure --with-mesos-root=/mesos --with-mesos-build-dir=/mesos

A libmesos_dvdi_isolator.so and libmesos_dvdi_isolator-0.1.so will be generated
in isolator/build/.libs
