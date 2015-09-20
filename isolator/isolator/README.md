# docker Volume Driver Isolation Module

This module handles mounting and unmounting external storage volumes
before and after launching tasks/executors.

##Build
See [Building the Modules](https://github.com/mesosphere/metaswitch-modules).

##Setup

Create a JSON file for specifying the modules for the slave as per the
[template](modules.json.in).


This module accepts ? parameters:


###Example JSON file:
```
{
  "libraries": [
    {
      "file": "/path/to/libmesos_dvdi_isolator.so
      "modules": [
        {
          "name": "com_emc_mesos_DockerVolumeDriverIsolator",
          "parameters": [
            {
              "key": "initialization_command",
              "value": "python /path/to/initialization_script.py arg1 arg2"
            },
            {
              "key": "cleanup_command",
              "value": "python /path/to/cleanup_script.py arg1 arg2"
            }
          ]
        }
      ]
    }
  ]
}
```

### TODO: Optional arguments

If needed, we can add two more parameters -- `initialization_args` and
`cleanup_args` to explicitly supply arguments to the corresponding commands.


##Use

The module is only useful for the slave. In order to use this module, one needs
to specify two flags -- `modules` and `isolation`. Here is an example:


```
./bin/mesos-slave.sh --master=master_ip:port --namespaces='network' \
    --modules=file://path/to/slave_gssapi.json \
    --isolation="com_emc_mesos_DockerVolumeDriverIsolator"
```
The RexRay DVDCLI must also be installed on the slave
