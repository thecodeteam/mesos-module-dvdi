# docker-volume-driver-isolator
This is an Isolator for Mesos that provides external storage management for Mesos containers

for all builds (docker host should have these already):

- install bash-completion maven autoconf automake libtool subversion boost(libboost1.54-dev)
- also install gtest, glog (libgoogle-glog-dev), and picojson
    - picojson.h must be copied to /usr/local/include
- install protobuf-2.5.0
    - Mesos has this in tar.gz form but it must be uncompressed and installed in the mesos/3rdparty/libprocess/3rdparty/protobuf-2.5.0 directory
        - tar -xzf protobuf-2.5.0.tar.gz
        - cd protobuf-2.5.0
        - sudo ./configure
        - sudo make
        - sudo make check
        - sudo make install
        - protoc --version

export MESOS_HOME=/path/to/mesos
export GTEST_HOME=/path/to/gtest-1.7.0  
export MAVEN_HOME = /usr/share/maven



To build with vagrant docker image:

cd docker-volume-driver-isolator/isolator
./bootstrap
rm -rf build
mkdir build
cd build
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:/usr/local/lib
../configure --with-mesos-root=/mesos --with-mesos-build-dir=/mesos
make

Example JSON strings:

    Load a library libfoo.so with two modules org_apache_mesos_bar and org_apache_mesos_baz.

     {
       "libraries": [
         {
           "file": "/path/to/libfoo.so",
           "modules": [
             {
               "name": "org_apache_mesos_bar"
             },
             {
               "name": "org_apache_mesos_baz"
             }
           ]
         }
       ]
     }


A libmesos_dvdi_isolator.so and libmesos_dvdi_isolator-0.1.so will be generated
in isolator/build/.libs

Place e .so on slave(s) in te /usr/lib/
