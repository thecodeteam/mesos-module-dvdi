
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

// TODO Code Complete, but untested
// process() should be used instead of system(), but desire for synchronous dvdcli mount/unmount
// could make this complex.
// Currently only handles one mount per container, infos structure and logic will need enhancement
// to handle (n) mounts per containerized task.
// The recover() function only provides ContainerId  and not the original environment which contains
// the mounted volume name. This is problematic. Currently assumes that mount path has been placed in
// the executor work directory, which I am speculating is confurable as a work-around.
// If the executor work directory is not set to the dvdcli mount path, then recover won't clean up
// orphaned mounts, but it should do no harm.infos
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

 // TODO (when multi driver, multiple volume support implemented)
 //    1. Deduce a list of all mounts from all volume drivers
 //    2. Use this to embellish cleanup of orphanned mounts

  hashset<std::string> inUseDirs;

  foreach (const ExecutorRunState& state, states) {

    infos.put(state.id, Owned<Info>(new Info(state.directory)));
    inUseDirs.insert(state.directory) ;
    LOG(INFO) << "running container found on recover()";
    LOG(INFO) << "directory is (" << state.directory << ")";
  }
  // infos now has a root mount directory for every task now running

  //disable unmount in recover() until some issues are resolved
/*
  // Mounts from unknown orphans will be cleaned up now.
  // Mounts from known orphans will be re-inserted into the infos hashmap
  set<std::string> unknownOrphans;

  // Read currently mounted file systems from /proc/mounts.
  Try<fs::MountTable> table = fs::MountTable::read("/proc/mounts");
  if (table.isError()) {
	  return Failure("Failed to read mount table: " + table.error());
  }

  std::list<std::string> mountlist;
  foreach (const fs::MountTable::Entry& entry, table.get().entries) {
      LOG(INFO) << "proc/mounts device is " << entry.fsname;
      LOG(INFO) << "proc/mounts fsname is " << entry.dir;
      if (entry.dir.length() <= REXRAY_MOUNT_PREFIX.length()) {
    	continue; // too small to be a rexray mount
      }
      if (!strings::startsWith(entry.dir, REXRAY_MOUNT_PREFIX)) {
    	  continue; // not a rexray mount
      }
      // we found a rexray mount
      mountlist.push_back(entry.dir);
  }

// get a list of entries under the rexray mount point
//  Try<std::list<std::string> > entries = os::ls(REXRAY_MOUNT_PREFIX);
  if (!mountlist.empty()) {
    LOG(INFO) << "rexray mounts found on recover()";

    foreach (const std::string& entry, mountlist) {
      bool dirInUse = false;

      for(auto const ent : infos) {
        if (entry == ent.second->mountrootpath.substr(REXRAY_MOUNT_PREFIX.length())) {
          dirInUse = true;
          break; // a known container is associated with this mount,
        }
      }
      if (dirInUse) {
      	continue;
      }
      unknownOrphans.insert(entry);
      LOG(INFO) << entry << "is an orphan rexray mount found on recover(), it will be unmounted";
    }
  }

  foreach (const std::string orphan, unknownOrphans) {
     if (system(NULL)) { // Is a command processor available?
	   std::string cmd = DVDCLI_UNMOUNT_CMD + orphan;
       int i = system(cmd.c_str());
       if( 0 != i ) {
         return Failure("recover() failed to execute unmount command " + cmd );
       }
     }
  }
  */
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

  // We may want to use the directory to indicate volumeName even though
  // initial plan was environment variable. For now code will use environment

  // get things we need from task's environment in ExecutoInfo
  if (!executorInfo.command().has_environment()) {
    // No environment means no external volume specification
    // not an error, just nothing to do so return None.
    LOG(INFO) << "No environment specified for container ";
    return None();
  }

  // iterate through the environment variables,
  // looking for the ones we need
  foreach (const mesos:: Environment_Variable& variable,
           executorInfo.command().environment().variables()) {
    JSON::Object variableObject;
    variableObject.values["name"] = variable.name();
    variableObject.values["value"] = variable.value();
    jsonVariables.values.push_back(variableObject);

    if (variable.name() == VOL_NAME_ENV_VAR_NAME) {
   	  volumeName = variable.value();
    }

    if (variable.name() == VOL_DRIVER_ENV_VAR_NAME) {
   	  volumeDriver = variable.value();
    }

    if (variable.name() == VOL_OPTS_ENV_VAR_NAME) {
      volumeOpts = variable.value();
    }
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

  // parse and format volume options
  std::stringstream ss(volumeOpts);
  std::string opts;

  while( ss.good() )
  {
    string substr;
    getline( ss, substr, ',' );
    opts = opts + VOL_OPTS_CMD_OPTION + substr;
  }

  // we have a volume name, now check if we are the first task to request a mount
  const std::string fullpath = REXRAY_MOUNT_PREFIX + volumeName;
  bool mountInUse = false;

  for(auto const ent : infos) {
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

  string mountToRelease(infos[containerId]->mountrootpath);

  if (mountToRelease.length() <= REXRAY_MOUNT_PREFIX.length()) {
    // too small to include a valid rexray mount
    return Nothing();
  }

  if (!strings::startsWith(mountToRelease, REXRAY_MOUNT_PREFIX)) {
    // too small to include a valid rexray mount
    return Nothing();
  }

  // we have now confirmed that this is a docker volume driver mount
  // next check if we are the last and only task using this mount
  // note: we should always find our own mount as we iterate
  int mountcount = 0;
  for(auto const ent : infos) {
    if (ent.second->mountrootpath == mountToRelease) {
      if( ++mountcount > 1) {
     	  break; // as soon as we find two users we can quit
      }
    }
  }

  if (mountcount == 1) {
    // we were the only or last user of the mount
    const std::string volumeName = mountToRelease.substr(REXRAY_MOUNT_PREFIX.length());

    if (system(NULL)) { // Is a command processor available?
	  std::string cmd = DVDCLI_UNMOUNT_CMD + volumeName;
      int i = system(cmd.c_str());
      if( 0 != i ) {
        return Failure("Failed to execute unmount command " + cmd );
      }
    }
  }

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
