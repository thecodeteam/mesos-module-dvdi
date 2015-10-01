
/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// TODO: process() should be used instead of system(), but desire for
// synchronous dvdcli mount/unmount could make this complex.

#include <fstream>
#include <list>
#include <array>
#include <iostream>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>
#include <mesos/module/isolator.hpp>
#include <mesos/slave/isolator.hpp>
#include "docker_volume_driver_isolator.hpp"

#include <glog/logging.h>
#include <mesos/type_utils.hpp>

#include <process/process.hpp>
#include <process/subprocess.hpp>

#include "linux/fs.hpp"
using namespace mesos::internal;
#include <stout/foreach.hpp>
#include <stout/os/ls.hpp>
#include <stout/format.hpp>
#include <stout/strings.hpp>

#include <sstream>

using namespace process;

using std::list;
using std::set;
using std::string;
using std::array;

using namespace mesos;
using namespace mesos::slave;

using mesos::slave::ExecutorRunState;
using mesos::slave::Isolator;
using mesos::slave::IsolatorProcess;
using mesos::slave::Limitation;

DockerVolumeDriverIsolatorProcess::DockerVolumeDriverIsolatorProcess(
	const Parameters& _parameters)
  : parameters(_parameters) {}

Try<Isolator*> DockerVolumeDriverIsolatorProcess::create(const Parameters& parameters)
{
  Result<string> user = os::user();
  if (!user.isSome()) {
    return Error("Failed to determine user: " +
                 (user.isError() ? user.error() : "username not found"));
  }

  if (user.get() != "root") {
    return Error("DockerVolumeDriverIsolator requires root privileges");
  }

  process::Owned<IsolatorProcess> process(
      new DockerVolumeDriverIsolatorProcess(parameters));

  return new Isolator(process);
}

DockerVolumeDriverIsolatorProcess::~DockerVolumeDriverIsolatorProcess() {}

Future<Nothing> DockerVolumeDriverIsolatorProcess::recover(
    const list<ExecutorRunState>& states,
    const hashset<ContainerID>& orphans)
{
  LOG(INFO) << "DockerVolumeDriverIsolatorProcess recover() was called";

  //temporary disable recover because it is broken, to enable test of other new code
	return Nothing();
  // temporary mount json test TODO remove later
/*
expected input
{"mounts": [
{
"containerid": "dgsja",
"volumedriver": "rexray",
"volumename:": "abc123456789",
"mountoptions": "size=5,iops=150,volumetype=io1,newfstype=xfs,overwritefs=true"
},
{
"containerid": "dgsja",
"volumedriver": "rexray",
"volumename:": "test1234567890",
"mountoptions": ""
}
]}
 */
  {
const std::string MOUNT_JSON_TEST =
"{\"mounts\": [\n{\n\"containerid\": \"dgsja\",\n\"volumedriver\": \"rexray\",\n\"volumename:\": \"abc123456789\",\n\"mountoptions\": \"size=5,iops=150,volumetype=io1,newfstype=xfs,overwritefs=true\"\n},\n{\n\"containerid\": \"dgsja\",\n\"volumedriver\": \"rexray\",\n\"volumename:\": \"test1234567890\",\n\"mountoptions\": \"\"\n}\n]}";

picojson::value v;
const char *s = MOUNT_JSON_TEST.c_str();
string err = picojson::parse(v, s, s +strlen(s) );
if (! err.empty()) {
	LOG(INFO) << "picojson parse error:" << err;
}
// check if the type of the value is "object"
if (! v.is<picojson::object>()) {
	LOG(INFO) << "JSON is not an object" << std::endl;
	return Nothing();
}

// obtain a const reference to the map, and print the contents
const picojson::value::object& obj = v.get<picojson::object>();
for (picojson::value::object::const_iterator i = obj.begin();
     i != obj.end();
     ++i) {
	LOG(INFO) << i->first << ": " << i->second.to_str() ;
}
picojson::array list = v.get("mounts").get<picojson::array>();

  for (picojson::array::iterator iter = list.begin(); iter != list.end(); ++iter) {
    std::string containerid = (*iter).get("containerid").get<string>();
    std::string deviceDriverName = (*iter).get("volumedriver").get<string>();
    std::string volumeName = (*iter).get("volumename").get<string>();
    std::string mountOptions = (*iter).get("mountoptions").get<string>();
    LOG(INFO) << "containerid:" << containerid;
    LOG(INFO) << "deviceDriverName:" << deviceDriverName;
    LOG(INFO) << "volumeName:" << volumeName;
    LOG(INFO) << "mountOptions:" << mountOptions;
  }

infos.clear();
// copy task element to rebuild infos
mesos::ContainerID* cid = new ContainerID();
process::Owned<ExternalMount> m(
    new ExternalMount("abc123", "rexray", ""));

infos.put(*cid, m);
std::ofstream infosout1("/tmp/json-out-test.json");
dumpInfos(infosout1);
infosout1.flush();
infosout1.close();
infos.clear();
delete cid;

} // end of test code to be removed

  // Slave recovery is a feature of Mesos that allows task/executors
  // to keep running if a slave process goes down, AND
  // allows the slave process to reconnect with already running
  // slaves when it restarts.
  // The orphans parameter is list of tasks (ContainerID) still running now.
  // The states parameter is a list of structures containing a tuples
  // of (ContainerID, pid, directory) where directory is the slave directory
  // specified at task launch.
  // We need to rebuild mount ref counts using these.
  // However there is also a possibility that a task
  // terminated while we were gone, leaving a "orphanned" mount.
  // If any of these exist, they should be unmounted.
  // Sometime after the 0.23.0 release a ContainerState will be provided
  // instead of the current ExecutorRunState.

  // note: this multihashmap is similar to the infos multihashmap but note that the
  // key is an std::string instead of a ContainerID.
  // This is because some of the ContainerIDs present when it was recorded may now be gone
  // The key is a string rendering of the ContainerID but not a ContainerID
  multihashmap<std::string, process::Owned<ExternalMount>> originalContainerMounts;

  // read container mounts from filesystem
  std::ifstream ifs(DVDI_MOUNTLIST_FILENAME);
  picojson::value v;
  ifs >> v;
  picojson::array list = v.get("mounts").get<picojson::array>();

  for (picojson::array::iterator iter = list.begin(); iter != list.end(); ++iter) {
    std::string containerid = (*iter).get("containerid").get<string>();
    std::string deviceDriverName = (*iter).get("volumedriver").get<string>();
    std::string volumeName = (*iter).get("volumename").get<string>();
    std::string mountOptions = (*iter).get("mountoptions").get<string>();
    process::Owned<ExternalMount> mount(
        new ExternalMount(volumeName, deviceDriverName, mountOptions));
    originalContainerMounts.put(containerid, mount);
  }


  // allMounts is list of all mounts in use at according to recovered file
  // inUseMounts is list of all mounts deduced to be still in use now
  // both maps starts empty, we will iterate to populate
  typedef hashmap<
    ExternalMountID, process::Owned<ExternalMount>> externalmountmap;
  externalmountmap legacyMounts;
  externalmountmap inUseMounts;

  // populate legacyMounts with all mounts at time file was written
  // note: some of the tasks using these may be gone now
  for (auto& elem : originalContainerMounts) {
    // elem->second is ExternalMount,
    legacyMounts.put(elem.second.get()->getExternalMountId(), elem.second);
  }

  foreach (const ExecutorRunState& state, states) {
    if (originalContainerMounts.contains(state.id.value())) {
      // we found a task that is still running and has mounts
      LOG(INFO) << "running container(" << state.id << ") found on recover()";
      LOG(INFO) << "state.directory is (" << state.directory << ")";
      std::list<process::Owned<ExternalMount>> mountsForContainer =
          originalContainerMounts.get(state.id.value());
      for (auto iter : mountsForContainer) {
        // copy task element to rebuild infos
        infos.put(state.id, iter);
        ExternalMountID id = iter->getExternalMountId();
        LOG(INFO) << "mount id is " << id;
        inUseMounts.put(iter->getExternalMountId(), iter);
      }
	}
  }

  // infos has now been rebuilt for every task now running
  // we will now reduce legacyMounts to only the mounts that should be removed
  // we will do this by deleting the mounts still in use
  for( auto iter : inUseMounts) {
    legacyMounts.erase(iter.first);
  }

  // legacyMounts now contains only "orphan" mounts whose task is gone
  for (auto iter : legacyMounts) {
    if (!unmount(*(iter.second), "recover()")) {
      return Failure("recover() failed during unmount attempt");
    }
  }

  // flush the infos structure to disk
  std::ofstream infosout(DVDI_MOUNTLIST_FILENAME);
  dumpInfos(infosout);
  infosout.flush();
  infosout.close();

  return Nothing();
}

// Attempts to unmount specified external mount, returns true on success
bool DockerVolumeDriverIsolatorProcess::unmount(
    const ExternalMount& em,
    const std::string&   callerLabelForLogging )
{
    LOG(INFO) << em << " is being unmounted on " << callerLabelForLogging;
    if (system(NULL)) { // Is a command processor available?
      const Try<std::string>& cmd = strings::format("%s %s%s %s%s",
              DVDCLI_UNMOUNT_CMD,
              VOL_DRIVER_CMD_OPTION, em.deviceDriverName,
              VOL_NAME_CMD_OPTION, em.volumeName);
      if (cmd.isError()) {
        LOG(ERROR) << "failed to format an unmount command on " << callerLabelForLogging;
        return false;
      }
      int i = system(cmd.get().c_str());
      if( 0 != i ) {
        LOG(WARNING) << cmd.get() << " failed to execute on " << callerLabelForLogging
   	                 << ", continuing on the assumption this volume was manually unmounted previously";
      }
    } else {
      LOG(ERROR) << "failed to acquire a command processor for unmount on " << callerLabelForLogging;
      return false;
    }
    return true;
}

// Attempts to mount specified external mount, returns true on success
bool DockerVolumeDriverIsolatorProcess::mount(
    const ExternalMount& em,
    const std::string&   callerLabelForLogging)
{
    LOG(INFO) << em << " is being mounted on  " << callerLabelForLogging;
    const std::string volumeDriver = em.deviceDriverName;
    const std::string volumeName = em.volumeName;

    // parse and format volume options
    std::stringstream ss(em.mountOptions);
    std::string opts;

    while( ss.good() )
    {
      string substr;
      getline( ss, substr, ',' );
      opts = opts + " " + VOL_OPTS_CMD_OPTION + substr;
    }

    if (system(NULL)) { // Is a command processor available?
      const Try<std::string>& cmd = strings::format("%s %s%s %s%s %s",
            DVDCLI_MOUNT_CMD,
            VOL_DRIVER_CMD_OPTION, volumeDriver,
  	        VOL_NAME_CMD_OPTION, volumeName,
  	        opts);
      if (cmd.isError()) {
        LOG(ERROR) << "failed to format an mount command on "
                   << callerLabelForLogging;
        return false;
      }
      int i = system(cmd.get().c_str());
      if( 0 != i ) {
        LOG(ERROR) << cmd.get() << " failed to execute on "
                   << callerLabelForLogging
                   << ", continuing on the assumption this volume was manually unmounted previously";
        return false;
      }
    } else {
      LOG(ERROR) << "failed to acquire a command processor for unmount on "
                 << callerLabelForLogging;
      return false;
    }
    return true;
}

std::ostream& DockerVolumeDriverIsolatorProcess::dumpInfos(std::ostream& out)
{
  out << "{\"mounts\": [\n";
  std::string delimiter = "";
  for (auto const ent : infos) {
    out << delimiter << "{\n";
    //out << "\"containerid\": \""  << ent.first.SerializeAsString() << "\"\n";
    out << "\"volumedriver\": \"" << ent.second.get()->deviceDriverName << "\"\n";
    out << "\"volumename\": \""   << ent.second.get()->volumeName << "\"\n";
    out << "\"mountoptions\": \"" << ent.second.get()->mountOptions << "\"\n";
    out << "}";
    delimiter = ",\n";
  }
  out << "\n]}";
  return out;
}

// Prepare runs BEFORE a task is started
// will check if the volume is already mounted and if not,
// will mount the volume
Future<Option<CommandInfo>> DockerVolumeDriverIsolatorProcess::prepare(
    const ContainerID& containerId,
    const ExecutorInfo& executorInfo,
    const string& directory,
	const Option<string>& rootfs,
    const Option<string>& user)
{
  LOG(INFO) << "Preparing external storage for container: "
            << stringify(containerId);

  JSON::Object environment;
  JSON::Array jsonVariables;

  static const size_t ARRAY_SIZE = 10;
  std::array<std::string, ARRAY_SIZE> deviceDriverNames;
  std::array<std::string, ARRAY_SIZE> volumeNames;
  std::array<std::string, ARRAY_SIZE> mountOptions;

  // get things we need from task's environment in ExecutoInfo
  if (!executorInfo.command().has_environment()) {
    // No environment means no external volume specification
    // not an error, just nothing to do so return None.
    LOG(INFO) << "No environment specified for container ";
    return None();
  }

  // iterate through the environment variables,
  // looking for the ones we need
  foreach (const mesos::Environment_Variable& variable,
           executorInfo.command().environment().variables()) {
    JSON::Object variableObject;
    variableObject.values["name"] = variable.name();
    variableObject.values["value"] = variable.value();
    jsonVariables.values.push_back(variableObject);

    if (strings::startsWith(variable.name(), VOL_NAME_ENV_VAR_NAME)) {
      const size_t prefixLength = VOL_NAME_ENV_VAR_NAME.length();
      if (variable.name().length() == prefixLength) {
        volumeNames[0] = variable.value();
      } else if (variable.name().length() == (prefixLength+1)) {
        char digit = variable.name().data()[prefixLength];
        if (isdigit(digit)) {
          size_t index = std::atoi(variable.name().substr(prefixLength).c_str());
          if (index !=0) {
            volumeNames[index] = variable.value();
          }
        }
      }
      LOG(INFO) << "external volume name (" << variable.value() << "parsed from environment";
    } else if (strings::startsWith(variable.name(), VOL_DRIVER_ENV_VAR_NAME)) {
      const size_t prefixLength = VOL_DRIVER_ENV_VAR_NAME.length();
      if (variable.name().length() == prefixLength) {
    	deviceDriverNames[0] = variable.value();
      } else if (variable.name().length() == (prefixLength+1)) {
        char digit = variable.name().data()[prefixLength];
        if (isdigit(digit)) {
          size_t index = std::atoi(variable.name().substr(prefixLength).c_str());
          if (index !=0) {
            deviceDriverNames[index] = variable.value();
          }
        }
      }
    } else if (strings::startsWith(variable.name(), VOL_OPTS_ENV_VAR_NAME)) {
      const size_t prefixLength = VOL_OPTS_ENV_VAR_NAME.length();
      if (variable.name().length() == prefixLength) {
        mountOptions[0] = variable.value();
      } else if (variable.name().length() == (prefixLength+1)) {
        char digit = variable.name().data()[prefixLength];
        if (isdigit(digit)) {
          size_t index = std::atoi(variable.name().substr(prefixLength).c_str());
          if (index !=0) {
            mountOptions[index] = variable.value();
          }
        }
      }
    } else if (variable.name() == JSON_VOLS_ENV_VAR_NAME) {
      //JSON::Value jsonVolArray = JSON::parse(variable.value());
    }
  }
  // TODO: json environment is not used yet
  environment.values["variables"] = jsonVariables;

  // requestedExternalMounts is all mounts requested by container
  std::vector<process::Owned<ExternalMount>> requestedExternalMounts;
  // unconnectedExternalMounts is the subset of those not already
  // in use by another container
  std::vector<process::Owned<ExternalMount>> unconnectedExternalMounts;

  for (size_t i = 0; i < volumeNames.size(); i++) {
    if (volumeNames[i].empty()) {
      continue;
    }
    LOG(INFO) << "validating mount" << volumeNames[i];
    if (deviceDriverNames[i].empty()) {
      deviceDriverNames[i] = VOL_DRIVER_DEFAULT;
    }
    process::Owned<ExternalMount> mount(
        new ExternalMount(volumeNames[i], deviceDriverNames[i], mountOptions[i]));
    // check for duplicates in environment
    bool duplicateInEnv = false;
    for (auto ent : requestedExternalMounts) {
      if (ent.get()->getExternalMountId() ==
             mount.get()->getExternalMountId()) {
        duplicateInEnv = true;
        break;
      }
    }
    if (duplicateInEnv) {
      LOG(INFO) << "duplicate mount request(" << *mount
                << ") in environment will be ignored";
      continue;
    }
    requestedExternalMounts.push_back(mount);

    // now check if another container is already using this same mount
    bool mountInUse = false;
    for (auto const ent : infos) {
      if (ent.second.get()->getExternalMountId() ==
             mount.get()->getExternalMountId()) {
        mountInUse = true;
        LOG(INFO) << "requested mount(" << *mount
                  << ") is already mounted by another container";
        break;
      }
  	}
    if (!mountInUse) {
      unconnectedExternalMounts.push_back(mount);
    }
  }

  // As we connect mounts we will build a list of successful mounts
  // We need this because, if there is a failure, we need to unmount these.
  // The goal is we mount either ALL or NONE.
  std::vector<process::Owned<ExternalMount>> successfulExternalMounts;
  for (auto const iter : unconnectedExternalMounts) {
    if (mount(*iter, "prepare()")) {
      successfulExternalMounts.push_back(iter);
    } else {
      // once any mount attempt fails, give up on whole list
      // and attempt to undo the mounts we already made
      for (auto const unmountme : successfulExternalMounts) {
        if (unmount(*unmountme, "prepare()-recovery after failed mount")) {
        	break;
        }
      }
      return Failure("prepare() failed during mount attempt");
    }
  }

  // note: infos has a record for each mount asscoiated with this container
  // even if the mount is also used by another container
  for (auto const iter : requestedExternalMounts) {
    infos.put(containerId, iter);
  }
  // flush infos to disk
  std::ofstream infosout(DVDI_MOUNTLIST_FILENAME);
  dumpInfos(infosout);
  infosout.flush();
  infosout.close();

  return None();
}

Future<Limitation> DockerVolumeDriverIsolatorProcess::watch(
    const ContainerID& containerId)
{
  // No-op, for now.

  return Future<Limitation>();
}

Future<Nothing> DockerVolumeDriverIsolatorProcess::update(
    const ContainerID& containerId,
    const Resources& resources)
{
  // No-op, nothing enforced.

  return Nothing();
}


Future<ResourceStatistics> DockerVolumeDriverIsolatorProcess::usage(
    const ContainerID& containerId)
{
  // No-op, no usage gathered.

  return ResourceStatistics();
}

process::Future<Nothing> DockerVolumeDriverIsolatorProcess::isolate(
    const ContainerID& containerId,
    pid_t pid)
{
  // No-op, isolation happens when mounting/unmounting in prepare/cleanup
  return Nothing();
}

Future<Nothing> DockerVolumeDriverIsolatorProcess::cleanup(
    const ContainerID& containerId)
{
  // TODO (when multi driver, multiple volume support implemented)
  //    1. Get driver name and volume list from enhance infos
  //    2. Iterate list and perform unmounts

  if (!infos.contains(containerId)) {
    return Nothing();
  }
  std::list<process::Owned<ExternalMount>> mountsList =
      infos.get(containerId);
  // mountList now contains all the mounts used by this container

  // note: it is possible that some of these mounts are also used by other tasks
  for( auto iter : mountsList) {
    size_t mountCount = 0;
    for (auto& elem : infos) {
      // elem.second is ExternalMount,
      if (iter->getExternalMountId() == elem.second.get()->getExternalMountId()) {
          if( ++mountCount > 1) {
         	  break; // as soon as we find two users we can quit
          }
      }
    }
    if (1 == mountCount) {
      // this container has the was the only, or last, user of this mount
      if (!unmount(*iter, "cleanup()")) {
        return Failure("cleanup() failed during unmount attempt");
      }
    }
  }

  // remove all this container's mounts from infos
  infos.remove(containerId);

  // flush infos to disk
  std::ofstream infosout(DVDI_MOUNTLIST_FILENAME);
  dumpInfos(infosout);
  infosout.flush();
  infosout.close();

  return Nothing();

}

static Isolator* createDockerVolumeDriverIsolator(const Parameters& parameters)
{
  LOG(INFO) << "Loading Docker Volume Driver Isolator module";

  Try<Isolator*> result = DockerVolumeDriverIsolatorProcess::create(parameters);

  if (result.isError()) {
    return NULL;
  }

  return result.get();
}

// Declares the isolator named com_emccode_mesos_DockerVolumeDriverIsolator
mesos::modules::Module<Isolator> com_emccode_mesos_DockerVolumeDriverIsolator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "emc{code}",
    "emccode@emc.com",
    "Docker Volume Driver Isolator module.",
    NULL,
	createDockerVolumeDriverIsolator);
