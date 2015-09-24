# docker-volume-driver-isolator
This project is an Isolator module for Mesos. This isolator manages volumes mounted from external storage.  

This allows Mesos to manage external storage mounts for applications.


Build Requirements
-------------------
Build tool and library requirements are similar to Mesos. (See below for a description of a Docker container image that pre-configures and automates many of these requirements):

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

To invoke an automake build:

cd docker-volume-driver-isolator/isolator  
./bootstrap  
rm -rf build  
mkdir build  
cd build  
export LD_LIBRARY_PATH=LD_LIBRARY_PATH:/usr/local/lib  
../configure --with-mesos-root=/mesos --with-mesos-build-dir=/mesos  
make


A libmesos_dvdi_isolator-<version>.so will be generated
in isolator/build/.libs

Copy/Update libmesos_dvdi_isolator-<version>.so on slave(s) in /usr/lib/


# Mesos Agent Configuration

The .so file is an implementation of a Mesos module. In particular, it is a Mesos Isolator module for installation on Mesos Agent nodes.

Copy/Update libmesos_dvdi_isolator-<version>.so  /usr/lib/ to on each Mesos Agent node that will offer mounted storage volumes.

The json configuration file tells the agent to load the module and enable the isolator. The command line flag "--modules=" specifies the location of the json file. Mesos agent option flags may be specified in several ways, but one way is to create a text file in /etc/mesos-slave/modules

Example of content in text file /etc/mesos-slave/modules:
```
 /usr/lib/dvdi-mod.json
 ```


---------

Example JSON file to configure a Mesos Isolator module on a Mesos slave:  

    Load a library libmesos_dvdi_isolator-0.23.0.so with two modules org_apache_mesos_bar and org_apache_mesos_baz.
```
     {
       "libraries": [
         {
           "file": "/usr/lib/libmesos_dvdi_isolator-0.23.0.so",
           "modules": [
             {
               "name": "com_emccode_mesos_DockerVolumeDriverIsolator"
             }
           ]
         }
       ]
     }
```

The isolator utilizes an abstraction driver based on the Docker Volume Driver API, called dvdcli.

DVD CLI Available At
---
[dvdcli](https://github.com/clintonskitson/dvdcli)

REX-Ray is a volume driver endpoint used by dvdcli

REX-Ray provides visibility and management of external/underlying storage via guest storage introspection.
REX-Ray Available At
---
[REX-Ray](https://github.com/emccode/rexray)


DVD CLI and REX-Ray must be installed on each Mesos agent node using the isolator. The installation can (and should) be tested at a command line.

Example DVD CLI Call
---

```
/go/src/github.com/clintonskitson/dvdcli/dvdcli mount --volumedriver=rexray --volumename=test123456789  --volumeopts=size=5 --volumeopts=iops=150 --volumeopts=volumetype=io1 --volumeopts=newFsType=ext4 --volumeopts=overwritefs=true
```


# Using the isolator

Run Slave
---------
```
nohup  /usr/sbin/mesos-slave \ --isolation="com_emc_mesos_DockerVolumeDriverIsolator" \
--master=zk://172.31.0.11:2181/mesos \
--log_dir=/var/log/mesos \
--containerizers=docker,mesos \
--executor_registration_timeout=5mins \
--ip=172.31.2.11 --work_dir=/tmp/mesos \
--modules=file:///home/ubuntu/docker-volume-driver-isolator/isolator/modules.json &
```


Example Marathon Call - test.json
---

`curl -i -H 'Content-Type: application/json' -d @test.json localhost:8080/v2/apps`

```
{
  "id": "hello-play",
  "cmd": "while [ true ] ; do touch /var/lib/rexray/volumes/test12345/hello ; sleep 5 ; done",
  "mem": 32,
  "cpus": 0.1,
  "instances": 1,
  "env": {
    "DVDI_VOLUME_NAME": "test1234567890",
    "DVDI_VOLUME_OPTS": "size=5,iops=150,volumetype=io1,newfstype=xfs,overwritefs=true"
  },
  "volumes": {
    "/var/lib/rexray/volumes/test12345":"/test12345"
  }
}
```


# Docker image for Build and development


To simplify the process of assembling and configuring a build environment for the docker volume driver isolaor, a Docker image is offered.


Build Docker Images
-------------------
`docker build -t name/mesos-build-module-dev:0.23.0 - < Dockerfile-mesos-build-module-dev`

`docker build -t name/mesos-build-module-dvdi:0.23.0 - < Dockerfile-mesos-build-module-dvdi`


Build DVDI Module
-----------------
`git clone https://github.com/cantbewong/docker-volume-driver-isolator && cd docker-volume-driver-isolator`  
`docker run -ti -v path-to-docker-volume-driver-isolator:/isolator clintonskitson/dvdi-modules:0.23.0`
