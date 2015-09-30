
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

// process() should be used instead of system(), but desire for synchronous dvdcli mount/unmount
// could make this complex.

#include <fstream>
#include <list>

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
  // Recovery will only be viable if the directory is aligned to
  // the slave root mount point. Otherwise we have no means to
  // determine which mount is associated with which task, because the
  // environment for the task is not available to us.
  // Sometime after the 0.23.0 release a ContainerState will be provided
  // instead of the current ExecutorRunState.

  // TODO read container mounts from filesystem
  containermountmap originalContainerMounts;

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
	if (originalContainerMounts.contains(state.id)) {
      // we found a task that is still running and has mounts
      LOG(INFO) << "running container(" << state.id << ") found on recover()";
      LOG(INFO) << "state.directory is (" << state.directory << ")";
      std::list<process::Owned<ExternalMount>> mountsForContainer =
          originalContainerMounts.get(state.id);
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
  for( auto iter : legacyMounts) {
    const std::string volumeDriver = iter.second->deviceDriverName;
    const std::string volumeName = iter.second->volumeName;
    LOG(INFO) << volumeDriver << "/" << volumeName
              << " is an orphan mount found on recover(), it will be unmounted";
    if (system(NULL)) { // Is a command processor available?
      const Try<std::string>& cmd = strings::format("%s %s%s %s%s",
              DVDCLI_UNMOUNT_CMD,
              VOL_DRIVER_CMD_OPTION, volumeDriver,
              VOL_NAME_CMD_OPTION, volumeName);
      if (cmd.isError()) {
        return Failure("recover() failed to format an unmount command" + cmd.error() );
      }
      int i = system(cmd.get().c_str());
      if( 0 != i ) {
        LOG(WARNING) << cmd.get() << " failed to execute during recover(), "
   	                 << " continuing on the assumption this volume was manually unmounted previously";
      }
    } else {
      return Failure("recover() failed to acquire a command processor for unmount" );
    }
  }

  // TODO flush the infos structure to disk
  return Nothing();
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

  std::string volumeName;
  std::string volumeDriver;
  std::string volumeOpts;
  JSON::Object environment;
  JSON::Array jsonVariables;
  // TODO remove these temporary Volume 2 variables when we implement json array
  // parsing, using protocol buffer, instead of brute force second env variable
  std::string volumeName2;
  std::string volumeDriver2;
  std::string volumeOpts2;

  std::string deviceDriverNames[10];
  std::string volumeNames[10];
  std::string mountOptions[10];

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
    }


//      case str2int(JSON_VOLS_ENV_VAR_NAME):
//		JSON::Value jsonVolArray = JSON::parse(variable.value());
//        break;
//    }

    /*
    if (variable.name() == VOL_NAME_ENV_VAR_NAME) {
   	  volumeName = variable.value();
    }

    if (variable.name() == VOL_DRIVER_ENV_VAR_NAME) {
   	  volumeDriver = variable.value();
    }

    if (variable.name() == VOL_OPTS_ENV_VAR_NAME) {
      volumeOpts = variable.value();
    }

    if (variable.name() == JSON_VOLS_ENV_VAR_NAME) {
      JSON::Value jsonVolArray = JSON::parse(variable.value());
    }
    */
  }
  // TODO: json environment is not used yet, but will be when multi mount support is completed
  environment.values["variables"] = jsonVariables;

  if (volumeName.empty()) {
    LOG(WARNING) << "No " << VOL_NAME_ENV_VAR_NAME << " environment variable specified for container ";
    return None();
  }

  if (volumeDriver.empty()) {
    volumeDriver = VOL_DRIVER_DEFAULT;
  }

  process::Owned<ExternalMount> mount1(new ExternalMount(volumeDriver, volumeName, volumeOpts));
  // We save a list of mounts here
  std::vector<process::Owned<ExternalMount>> requestedExternalMounts;
  requestedExternalMounts.push_back(mount1);

  // check for optional second volume
  if (!volumeName2.empty()) {
    if (volumeDriver2.empty()) {
      volumeDriver2 = VOL_DRIVER_DEFAULT;
    }
    process::Owned<ExternalMount> mount2(new ExternalMount(volumeDriver2, volumeName2, volumeOpts2));
    requestedExternalMounts.push_back(mount1);
  }




  // parse and format volume options
  std::stringstream ss(volumeOpts);
  std::string opts;

  while( ss.good() )
  {
    string substr;
    getline( ss, substr, ',' );
    opts = opts + " " + VOL_OPTS_CMD_OPTION + substr;
  }

  // we have a volume name, now check if we are the first task to request a mount
  const std::string fullpath = REXRAY_MOUNT_PREFIX + volumeName;
  bool mountInUse = false;

  for(auto const ent : infos) {
	for (auto &ExternalMount : ent.second.get()->externalMounts) {

	}
    if (ent.second->mountrootpath == fullpath) {
      mountInUse = true;
      break; // another container is already associated with this mount,
    }
  }

  if (!mountInUse) {
    if (system(NULL)) { // Is a command processor available?
      const Try<std::string>& cmd = strings::format("%s %s%s %s%s %s",
          DVDCLI_MOUNT_CMD,
		  VOL_DRIVER_CMD_OPTION, volumeDriver,
		  VOL_NAME_CMD_OPTION, volumeName,
		  opts);
      if (cmd.isError()) {
            return Failure("prepare() failed to format a mount command from env vars" + cmd.error() );
      }
      int i = system(cmd.get().c_str());
      if( 0 != i ) {
        return Failure("prepare() failed to execute mount command " + cmd.get() );
      }
    }
  }

  infos.put(containerId, Owned<Info>(new Info(fullpath)));
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
      // unmount
      const std::string volumeDriver = iter.get()->deviceDriverName;
      const std::string volumeName = iter.get()->volumeName;
      LOG(INFO) << volumeDriver << "/"
                << volumeName
                << " is being unmounted on cleanup()";
   	  if (system(NULL)) { // Is a command processor available?
        const Try<std::string>& cmd = strings::format("%s %s%s %s%s",
                 DVDCLI_UNMOUNT_CMD,
                 VOL_DRIVER_CMD_OPTION, volumeDriver,
                 VOL_NAME_CMD_OPTION, volumeName);
   	    if (cmd.isError()) {
   	      return Failure("cleanup() failed to format an unmount command" + cmd.error() );
        }
        int i = system(cmd.get().c_str());
        if( 0 != i ) {
          LOG(WARNING) << cmd.get() << " failed to execute during cleanup(), "
       	               << " continuing on the assumption this volume was manually unmounted previously";
   	    }
      } else {
   	    return Failure("cleanup() failed to acquire a command processor for unmount" );
      }
    }
  }

  // remove this records from infos
  infos.remove(containerId);

  // TODO flush infos to disk

  // For a normally exited container, we take its info pointer off the
  // hashmap infos before using the helper function to clean it up.
  infos.erase(containerId);

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
