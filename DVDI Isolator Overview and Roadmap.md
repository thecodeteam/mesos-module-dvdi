### DVDI Isolator Overview and Roadmap

---

## Persistent Storage for Mesos

Certain classes of applications, such as databases, require long term access to persistent storage. 

---

## Issue and Workarounds

Techniques are available to run these types of workloads on Mesos, but sometimes the methods involved are essentially work-arounds that amount to using storage *not managed* by Mesos

---

### Workaround: Use direct attached storage

- Locks your workload to a single cluster node after first run, defeating an essential benefit of a cluster scheduler - the dynamic assignment of workloads to any, or the most appropriate, cluster node

---

### Workaround: Use external storage such as NFS from within the application

- Mesos is oblivious to this use of storage and cannot deliver in its role as a data center OS, offering centralized management and audit
    - This violates a key best practice premise of 12 factor apps by requiring maintenance of run time platform configuration inside the app itself

---

### Better solution

- Use the new DVDI Isolator

---

## What is the DVDI Isolator Module

Mesos has a rich facility to support plugins that add features.

The DVDI Isolator is a *module*, which is a binary plugin for Mesos.

In particular, it is an *isolator* module, which is a module that runs on agent cluster nodes and can interact with resource usage

---

## Features of the Mesos DVDI Isolator

- Manages external storage - storage that is network attachable to multiple cluster nodes,  
- Allows an application workload to be configured with a declarative statement describing its storage needs
    - This declaration is managed by Mesos. For example it can be done using the Marathon API.

---

##  Manages full volume mount lifecycle

- Allows a volume to be composed or allocated  and formatted from a pool upon first task run.
- Allows subsequent task runs to re-attach to the volume, even if the task runs on a different cluster node each time

---

## Supports Many storage types

- AWS EC2 EBS
- EMC ScaleIO
- EMC XtremIO
- many others too including NFS

Some of these storage types allow declarative specification of a rich set of storage attributes such as IOPs 

---

## The DVDI Storage Interface

DVDI stand for Docker Volume Driver Interface.

It leverages the internal storage volume interface of Docker.

-  The rapid investment growth in Docker related infrastructure is resulting in widespread implementation of the Docker interface by all popular forms of storage

--- 

## Why wasn't Docker sufficient by itself

- The DVDI Isolator allow a Mesos workload to take advantage of any storage with a Docker interface *but does not require that application to run in a Docker container*
- Many Mesos workloads do not run in Docker containers

---

## DVDI Isolator Architecture

![](https://emccode.files.wordpress.com/2015/10/screen-shot-2015-10-06-at-7-03-01-pm.png?w=521&h=379)

---

## DVDI Futures

---

### Short term roadmap: Improvements to integration

- use "first class" workload attributes to specify storage characteristics, instead of environment variables
- Allow concurrent use with Mesos containerizer isolator


---

### Longer term roadmap

- Enable Mesos management of tiered pools of storage
- Enable implementation of storage providers as a Mesos Framework, reporting storage as a global or cluster wide resource, with advertised attributes to guide workload placement