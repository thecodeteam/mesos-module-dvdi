# dvdi-modules

This repo contains the `Docker Volume Driver Isolator Module` for Mesos.  The purpose is to create a module that lives on the Mesos Slaves (agents) that enables external storage to be created/mounted/unmounted with each task that is assigned to a slave.

The module leverages [dvdcli](https://github.com/clintonskitson/dvdcli) to enable any existing `Docker Volume Drivers` to be used **without** the Docker containerizer.  All Volume Drivers that work with `Docker`, **will also** work with `dvdcli` and thus this Isolator Module.

Currently it targets Mesos 0.23.0.

Project Summary
-------------------
This repo is part of a larger project to deliver external storage and introduce cluster wide resources capabilities to the Mesos platform.  

The existing Mesos architecture (0.23.0) is based on having cluster node agents (aka slaves) determine and report their available resources. This works adequately when workloads consume storage exclusively from direct attached storage on the cluster nodes, but no so well for external storage volumes possibly shared among Mesos slaves.

See the notes, project, and planning information [here](https://github.com/cantbewong/mesos-proposal-externalstorage).

Functionality
-------------
With this module running, the frameworks are now able to leverage the environment variables parameters to determine which `Volume`, from which `Storage Platform` to make available on the `Mesos Slaves`.  This is without a resource advertisement.  Below is an example of `Marathon` specifying an environment variable for `Volume Management`.  

In addition, notice how the `VOLUME_OPTS` parameter allows for specifying extra functionality.  The `size`, `iops`, and `volumetype` can be requested from the `Storage Platform` if the `Volume` does not exist yet.  In addition, when the `Volume` is then created, a filesystem (EXT4/XFS) can be specified to be used on the `Volume`.

There is one additional option, `overwritefs` which can be used to determine whether to overwrite the filesystem or not.  When the `overwritefs` flag is set and the `Volume` is detected to contain a EXT4/XFS filesystem, it wil be replaced.  Otherwise a filesystem will **always** be created if EXT4/XFS is not found.

Options like these are only available if your `Volume Driver` exposes them.  For `REX-Ray`, these are supported but depend on the `Storage Driver`.  See the [dvdcli](https://github.com/emccode/dvdcli) for a full list of options.

```
"env": {
  "DVDI_VOLUME_NAME": "testing",
  "DVDI_VOLUME_DRIVER": "platform1",
  "DVDI_VOLUME_OPTS": "size=5,iops=150,volumetype=io1,newfstype=ext4,overwritefs=false",
  "DVDI_VOLUME_NAME1": "testing2",
  "DVDI_VOLUME_DRIVER1": "platform2",
  "DVDI_VOLUME_OPTS1": "size=6,volumetype=gp2,newfstype=xfs,overwritefs=true"
}
```

# Mesos Agent Configuration

### Volume Driver Endpoint
---
[REX-Ray](https://github.com/emccode/rexray) is a `Docker Volume Driver` endpoint that runs as a service that can be then consumed by `dvdcli`.  Any volume driver can be used in it's place.  See the [Docker Plugin List](https://github.com/docker/docker/blob/master/docs/extend/plugins.md).

`REX-Ray` provides visibility and management of external/underlying storage via guest storage introspection.

Below is a one-liner `REX-Ray` install.  Further than this, a configuration file or environment variables must be specified followed by `rexray start` to begin servicing requests.  See the project page for more details.

```
curl -sSL https://dl.bintray.com/emccode/rexray/install | sh -
```

Issuing a `rexray volume` should return you a list of volumes if the configuration is correct for your storage platform.

### Docker Volume Driver CLI
---

The isolator utilizes a CLI implementation of the `Docker Volume Driver`, called [dvdcli](https://github.com/emccode/dvdcli).  Below is a one-liner install for `dvdcli`.

```
curl -sSL https://dl.bintray.com/emccode/dvdcli/install | sh -
```


The `dvdcli` functions exactly as the `Docker` daemon would by looking up spec files from `/etc/docker` or socket files from `/run/docker/plugins` based on the `Volume Driver` name.  To make `dvdcli` work, a `Volume Driver` service must be actively running.

The combination of the `mesos-module-dvdi` isolator, `dvdcli`, and the `Docker Volume Driver` must be functioning on each Mesos agent to enable external volumes.  The `Docker` daemon is not required.

The following commands should work which means the isolator should function as expected.  You should be returned a path to a mounted volume.  Following this, perform a `unmount`.

```
dvdcli mount --volumedriver=rexray --volumename=test1
```

### Docker Volume Driver Isolator

The installation of the isolator is simple.  It is a matter of placing the `.so` file, creating a json file, and updating the startup parameters.

1. Copy/Update the `libmesos_dvdi_isolator-<version>.so`  to /usr/lib/ on each Mesos Agent node that will offer external storage volumes.

2. Compose or copy a json configuration file tells the agent to load the module and enable the isolator.

3. Create a text file similar to  `/usr/lib/dvdi-mod.json` with respective paths set.
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

4. (optional) Mesos slave/agent option flags may be specified in several ways.  One common way is to create a text file in `/etc/mesos-slave/modules` and additionally `/etc/mesos-slave/isolation` matching the flags in step 4.

5. (optional) Run slave/agent with explicit `--modules` flag and `--isolation` flags.

    ```
    nohup  /usr/sbin/mesos-slave \    
    --master=zk://172.31.0.11:2181/mesos \
    --log_dir=/var/log/mesos \
    --containerizers=docker,mesos \
    --executor_registration_timeout=5mins \
    --ip=172.31.2.11 --work_dir=/tmp/mesos \
    --modules=file:///usr/lib/dvdi-mod.json \
    --isolation="com_emc_mesos_DockerVolumeDriverIsolator" &
    ```


### Example Marathon Call
The following will submit a job, which mounts a volume from an external storage platform.

Up to nine additional volumes may be mounted by appending a digit (1-9)
to the environment variable name. (e.g DVDI_VOLUME_NAME1=).

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
    "DVDI_VOLUME_NAME": "test12345",
    "DVDI_VOLUME_DRIVER": "rexray",
    "DVDI_VOLUME_OPTS": "size=5,iops=150,volumetype=io1,newfstype=xfs,overwritefs=true"
  }
}
```

# Troubleshooting
See the `/var/log/mesos/mesos-slave.INFO` log for details.  Troubleshooting via `dvdcli` is always a great first step.


# Building Module with Docker Images
To simplify the process of assembling and configuring a build environment for the docker volume driver isolator, a Docker image is offered.

### Build using our Docker Module Build Image
These steps can be used to compile your own Isolat Module from a specific commit.
`git clone https://github.com/emccode/mesos-module-dvdi`
`cd mesos-module-dvdi`
`docker run -ti -v $(pwd):/isolator emccode/mesos-build-dvdi-modules:0.23.0`

Following this locate the `libmesos_dvdi_isolator-<version>.so` file under `isolator/build/.libs` and copy it to the `/usr/lib` directory.

### (optional) Build a custom Mesos Build Image
These steps can be used to compile a custom Mesos build image allowing you to change whether 0.23.0 is used or not.
`docker build -t name/mesos-build-module-dev:0.23.0 - < Dockerfile-mesos-build-module-dev`
`docker build -t name/mesos-build-module-dvdi:0.23.0 - < Dockerfile-mesos-build-module-dvdi`

# Release information
---------
Please refer to the [wiki](https://github.com/emccode/mesos-module-dvdi/wiki) for more information relating to the project.

# Licensing
---------
Licensed under the Apache License, Version 2.0 (the “License”); you may not use this file except in compliance with the License. You may obtain a copy of the License at <http://www.apache.org/licenses/LICENSE-2.0>

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an “AS IS” BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.

# Support
-------
If you have questions relating to the project, please either post [Github Issues](https://github.com/emccode/mesos-module-dvdi/issues), join our Slack channel available by signup through [community.emc.com](https://community.emccode.com) and post questions into `#projects`, or reach out to the maintainers directly.  The code and documentation are released with no warranties or SLAs and are intended to be supported through a community driven process.
