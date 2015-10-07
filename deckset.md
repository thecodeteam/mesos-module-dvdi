# Docker Volume Driver Isolator Module for Mesos 0.23.0
[github.com/emccode/mesos-module-dvdi](https://github.com/emccode/mesos-module-dvdi)

- Steve Wong (@cantbewong)

---
# Goals
- Be a lasting component to larger *Mesos External Storage* project
- Enable external storage to be orchestrated with tasks agnostic of framework
- Extend Docker eco-system of Volume Drivers to Mesos containerizer agnostic of storage platforms

---
# Architecture 0.23.0
![inline fill 55%](/Users/clintonkitson/Projects/150924/diagram1.png)

---
# Installing
1. Install and Test **rexray** (or others)
2. Install and Test **dvdcli**
3. Install **Mesos dvdi module**
4. Send Marathon Job

---

## Invoking with Jobs

```
"env": {
  "DVDI_VOLUME_NAME": "testing",
  "DVDI_VOLUME_DRIVER": "platform1",
  "DVDI_VOLUME_OPTS": "size=5,iops=150,volumetype=io1,newfstype=xfs,overwritefs=true"
}
```
 - name of the volume as represented by the storage platform
 - name of the storage platform
 - options for volume (VD dependency)
 - options for fs (VD dependency)

---

# DEMO

---

# Next Steps for MesosCon EU Demo

---
# Critical Path
- Mesos containerizer for isolation of FS
 - Priority of isolators
 - Subclass of mesos containerizer
- Multiple volumes per task
- Slave recovery

---

# Caveats
- Slave recovery process
- 0.23.0
- Use Volume Driver exclusively between containerizers due to unmounts

---

# Feedback
- Mountpoint coming back into task
- Credential management
