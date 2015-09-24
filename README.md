# docker-volume-driver-isolator

Project Summary
-------------------
Mesos is a cluster manager that schedules workloads in a way that optimizes efficient use of available cluster resources.  

The pre-0.23.0 Mesos architecture is based on having cluster node agents (aka slaves) determine and report their available resources, This works adequately when workloads consume storage exclusively from direct attached storage on the cluster nodes, but externally host shared storage volumes have been outside the scope of Mesos management.

This is an ongoing project to provide a Mesos plugin module that allows Mesos to extend the scope of management to include network attached storage.

The current release of this project supports volume mount/dismounts from external storage.  

Mesos to management of storage mounts allows flexible deployemnt of applications across a cluster. For example, an application configured in Marathon, can declare external storage needs, and Mesos will manage mounts without tying the application to a single specific cluster node. This feature is also useful for applications that require persistent storage.

It is expected that over time, this module will be extended to offer more storage related features.

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

Copy/Update libmesos_dvdi_isolator-<version>.so on slave(s), typically in /usr/lib/


# Mesos Agent Configuration

The .so file is an implementation of a Mesos module. In particular, it reports itself as a Mesos Isolator module for installation on Mesos Agent nodes.

### Installation Steps

1. Copy the .so file to the filesystem.
2. Compose and install a small .json configuration file
3. Create or edit the agent (mesos-slave) startup configuration flag to allow the agent to find and utilize the .json file


### Pre-requisite:

1. Install the REX-Ray Docker Volume driver API implementation on agent.
2. Install the DVD CLI interface on agent.


Detailed instructions:   

Copy/Update libmesos_dvdi_isolator-\<version>.so  to /usr/lib/ on each Mesos Agent node that will offer external storage volumes.

Compose or copy a json configuration file tells the agent to load the module and enable the isolator.

Mesos agent option flags may be specified in several ways, but one way is to create a text file in /etc/mesos-slave/modules


    - As an alternative, the mesos-slave command line flag "--modules=" can directly specify the location of the json file.

Example of content in text file /etc/mesos-slave/modules:
```
 /usr/lib/dvdi-mod.json
 ```

 Alternative: Run Slave, with explicit --modules flag
 ---------
 ```
 nohup  /usr/sbin/mesos-slave \ --isolation="com_emc_mesos_DockerVolumeDriverIsolator" \
 --master=zk://172.31.0.11:2181/mesos \
 --log_dir=/var/log/mesos \
 --containerizers=docker,mesos \
 --executor_registration_timeout=5mins \
 --ip=172.31.2.11 --work_dir=/tmp/mesos \
 --modules=file:///usr/lib/dvdi-mod.json &
 ```

---------

Example JSON file to configure the docker volume driver isolater module on a Mesos slave:  

    Load a library libmesos_dvdi_isolator-0.23.0.so with module com_emccode_mesos_DockerVolumeDriverIsolator. Suggested location is /usr/lib/dvdi-mod.json
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


## Slave Pre-requisite

### DVD CLI Available At
---
[dvdcli](https://github.com/clintonskitson/dvdcli)

The isolator utilizes an abstraction driver based on the Docker Volume Driver API, called dvdcli.

One-liner dvdcli install:

```
coming soon: get release from github
```


### REX-Ray Available At
---
[REX-Ray](https://github.com/emccode/rexray)

REX-Ray is a volume driver endpoint used by dvdcli

REX-Ray provides visibility and management of external/underlying storage via guest storage introspection.

One-liner REX-Ray install:

```
curl -sSL https://dl.bintray.com/emccode/rexray/install | sh -
```




DVD CLI and REX-Ray must be installed on each Mesos agent node that uses the docker volume isolator. The installation can (and should) be tested at a command line.

Example DVD CLI command line invocation to test installation of REX-Ray and DVD CLI:
---

```
/go/src/github.com/clintonskitson/dvdcli/dvdcli mount --volumedriver=rexray --volumename=test123456789  --volumeopts=size=5 --volumeopts=iops=150 --volumeopts=volumetype=io1 --volumeopts=newFsType=ext4 --volumeopts=overwritefs=true
```


# How to use Marathon to submit a job, mounting an external storage volume


### Example Marathon Call - using configuration in test.json file
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


# How to build this project using a pre-composed Docker image


To simplify the process of assembling and configuring a build environment for the docker volume driver isolator, a Docker image is offered.


Build Docker Images
-------------------
`docker build -t name/mesos-build-module-dev:0.23.0 - < Dockerfile-mesos-build-module-dev`

`docker build -t name/mesos-build-module-dvdi:0.23.0 - < Dockerfile-mesos-build-module-dvdi`


Build DVDI Module
-----------------
`git clone https://github.com/cantbewong/docker-volume-driver-isolator && cd docker-volume-driver-isolator`  
`docker run -ti -v path-to-docker-volume-driver-isolator:/isolator clintonskitson/dvdi-modules:0.23.0`
