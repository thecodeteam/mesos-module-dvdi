
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

#include "docker_volume_driver_isolator.hpp"

#include <list>
#include <glog/logging.h>
#include <mesos/type_utils.hpp>

#include <process/process.hpp>
#include <process/subprocess.hpp>

#include "common/protobuf_utils.hpp"


#include "linux/fs.hpp"
#include <boost/foreach.hpp>

using namespace process;

using std::list;
using std::set;
using std::string;

namespace mesos {
namespace internal {
namespace slave {

using mesos::slave::ExecutorRunState;
using mesos::slave::Isolator;
using mesos::slave::IsolatorProcess;
using mesos::slave::Limitation;

DockerVolumeDriverIsolatorProcess::DockerVolumeDriverIsolatorProcess(
    const Flags& _flags)
  : flags(_flags) {}

DockerVolumeDriverIsolatorProcess::~DockerVolumeDriverIsolatorProcess() {}

Try<Isolator*> DockerVolumeDriverIsolatorProcess::create(const Flags& flags)
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
      new DockerVolumeDriverIsolatorProcess(flags));

  return new Isolator(process);
}

process::Future<Option<int>> DockerVolumeDriverIsolatorProcess::namespaces()
{
  return CLONE_NEWNS;
}


Future<Nothing> SharedFilesystemIsolatorProcess::recover(
    const list<ExecutorRunState>& states,
    const hashset<ContainerID>& orphans)
{
  // There is nothing to recover because we do not keep any state and
  // do not monitor filesystem usage or perform any action on cleanup.
  return Nothing();
}

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


  // Read the mount table in the host mount namespace to recover paths
  // to device mountpoints.
  // Method 'cleanup()' relies on this information to clean
  // up mounts in the host mounts for each container.
  Try<fs::MountInfoTable> table = fs::MountInfoTable::read();
  if (table.isError()) {
    return Failure("Failed to get mount table: " + table.error());
  }

  hashset<std::string> inUseDirs;

  foreach (const ExecutorRunState& state, states) {
    Owned<Info> info(new Info(state.directory()));
    inUseDirs.insert(state.directory) ;
    infos.put(state.id, info);
  }
  // infos now has a root mount directory for every task now running

  //  Mounts from unknown orphans will be cleaned up now.
  // Mounts from known orphans will be cleaned up when
  // those known orphan containers are being destroyed by the slave.
  set<std::string> unknownOrphans;

  foreach (const fs::MountInfoTable::Entry& entry, table.get().entries) {
    if (!strings::startsWith(entry.root, REXRAY_MOUNT_PREFIX)) {
      continue; // not a mount created by this isolator
    }
    bool dirInUse = false;

    foreach (const Info& info, infos) {
	  if (entry.root == info.mountrootpath) {
	    dirInUse = true;
        break; // a known container is associated with this mount,
      }
    }
    if (dirInUse) {
    	continue;
    }
    unknownOrphans.insert(entry.root);
  }

  foreach (const std::string orphan, unknownOrphans) {

	  if (orphan.length() <= REXRAY_MOUNT_PREFIX.length()) {
		  // too small to include a valid rexray mount
		  continue;
	  }
	  const std::string volumeName = orphan.substr(REXRAY_MOUNT_PREFIX.length());

	  if (system(NULL)) { // Is a command processor available?
		std::string cmd = REXRAY_DVDCLI_UNMOUNT_CMD + volumeName;
	    int i = system(cmd.c_str());
	    if( 0 != i ) {
	        return Failure("recover() failed to execute unmount command " + cmd );
	    }
	  }

  }
  return Nothing();
}

// Prepare runs BEFORE a task is started
// will check if the volume is already mounted and if not,
// will mount the volume
//
// 1. verify rexray is installed
//    make rexray get-instance call to do this
//      will get slave's instance id (MYINST)
//      will get provider id (PROV)
//     TBD: multiple providers = multiple Rexray's, how will this work?
// 2. get desired volume drive (volumedriver=) from ENVIRONMENT from task in ExecutorInfo


// 3. get volume identifier (from ENVIRONMENT from task in ExecutorInfo VOLID
//      TBD: Is this volume id or name? answer name

// 4. make dvdcli mount call <volumename>
//    mount location is fixed, based on volume name (/var/lib/rexray/volumes/
//    this call is synchrounous, and return 0 if success
// 5. bump ref count for volumename


// 6. make rexray call to verify that (rexray get-volume --volumename=VOLID?)
//      - volume exists?
//      - whether device(volume) is already attached somewhere
//         TBD: if mounted to another host, should this cause an error?
//      - whether device is already attached on this slave host
// 7. IF volume not mounted on this host(slave?
//      a. make rexray call to attach device
//          rexray attach-volume --instanceid=<> --volumeid=<>
//          call should be synchronous and check for success
//      b. make rexray call to mount volume
//          rexray mount-volume
//          bump use_count for device /
//            TBD, would a more complex structure listing actual task ids be better?
//          call should be synchronous and verify success
//    ELSE (volume was already mounted)
//      a. bump use count for device
//
Future<Option<CommandInfo>> DockerVolumeDriverIsolatorProcess::prepare(
    const ContainerID& containerId,
    const ExecutorInfo& executorInfo,
    const string& directory,
	const Option<string>& rootfs,
    const Option<string>& user)
{
// TODO remove this temporary code to show we invoked isolator prepare ny touching a file in /tmp
  if (system(NULL)) { // Is a command processor available?
    int i = system("touch /tmp/DockerVolumeDriverIsolatorProcess-prepare-called.txt");
    if( 0 != i ) {
      LOG(WARNING) << "touch command failed in DockerVolumeDriverIsolatorProcesss";
    }
  }

  if (executorInfo.has_container() &&
      executorInfo.container().type() != ContainerInfo::MESOS) {
    return Failure("Can only prepare external storage for a MESOS container");
  }

  LOG(INFO) << "Preparing external storage for container: "
            << stringify(containerId);

  if (!executorInfo.has_container()) {
    // We don't consider this an error, there's just nothing to do so
    // we return None.

    return None();
  }

  std::string volumeName;
  JSON::Object environment;
  JSON::Array jsonVariables;

  // We may want to use the directory to indicate volumeName even though
  // initial plan was environment variable. For now code will take either

  // get things we need from task's environment in ExecutoInfo
  if (!executorInfo.command().has_environment()) {
	   // No environment means no external volume specification
	  // not an error, just nothing to do so return None.
	  // return None();
  }

  // iterate through the environment variables,
  // looking for the ones we need
  foreach (const mesos:: Environment_Variable& variable,
           executorInfo.command().environment().variables()) {
       JSON::Object variableObject;
      variableObject.values["name"] = variable.name();
      variableObject.values["value"] = variable.value();
      jsonVariables.values.push_back(variableObject);

      if (variable.name() == REXRAY_MOUNT_VOL_ENVIRONMENT_VAR_NAME) {
    	  volumeName = variable.value();
      }
  }
  environment.values["variables"] = jsonVariables;

  if (volumeName.empty()) {
      LOG(WARNING) << "No " << REXRAY_MOUNT_VOL_ENVIRONMENT_VAR_NAME << " environment variable specified for container ";
  }

  if (system(NULL)) { // Is a command processor available?
	std::string cmd = REXRAY_DVDCLI_MOUNT_CMD + volumeName;
    int i = system(cmd.c_str());
    if( 0 != i ) {
        return Failure("Failed to execute mount command " + cmd );
    }
  }

  infos.insert(containerId,  )

  // We don't support mounting to a container path which is a parent
  // to another container path as this can mask entries. We'll keep
  // track of all container paths so we can check this.
  set<string> containerPaths;
  containerPaths.insert(directory);

  list<string> commands;

  foreach (const Volume& volume, executorInfo.container().volumes()) {
    // Because the filesystem is shared we require the container path
    // already exist, otherwise containers can create arbitrary paths
    // outside their sandbox.
    if (!os::exists(volume.container_path())) {
      return Failure("Volume with container path '" +
                     volume.container_path() +
                     "' must exist on host for shared filesystem isolator");
    }

    // Host path must be provided.
    if (!volume.has_host_path()) {
      return Failure("Volume with container path '" +
                     volume.container_path() +
                     "' must specify host path for shared filesystem isolator");
    }

    // Check we won't mask another volume.
    // NOTE: Assuming here that the container path is absolute, see
    // Volume protobuf.
    foreach (const string& containerPath, containerPaths) {
      if (strings::startsWith(volume.container_path(), containerPath)) {
        return Failure("Cannot mount volume to '" +
                        volume.container_path() +
                        "' because it is under volume '" +
                        containerPath +
                        "'");
      }

      if (strings::startsWith(containerPath, volume.container_path())) {
        return Failure("Cannot mount volume to '" +
                        containerPath +
                        "' because it is under volume '" +
                        volume.container_path() +
                        "'");
      }
    }
    containerPaths.insert(volume.container_path());

    // A relative host path will be created in the container's work
    // directory, otherwise check it already exists.
    string hostPath;
    if (!strings::startsWith(volume.host_path(), "/")) {
      hostPath = path::join(directory, volume.host_path());

      // Do not support any relative components in the resulting path.
      // There should not be any links in the work directory to
      // resolve.
      if (strings::contains(hostPath, "/./") ||
          strings::contains(hostPath, "/../")) {
        return Failure("Relative host path '" +
                       hostPath +
                       "' cannot contain relative components");
      }

      if (system(NULL)) { // Is a command processor available?
    	std::string = REXRAY_DVDCLI_MOUNT_CMD +
        int i = system("/go/src/github.com/clintonskitson/dvdcli/dvdcli path --volumedriver=rexray --volumename=test");
        if( 0 != i ) {
          LOG(WARNING) << "touch command failed";
        }
      }

      Try<Nothing> mkdir = os::mkdir(hostPath, true);
      if (mkdir.isError()) {
        return Failure("Failed to create host_path '" +
                        hostPath +
                        "' for mount to '" +
                        volume.container_path() +
                        "': " +
                        mkdir.error());
      }

      // Set the ownership and permissions to match the container path
      // as these are inherited from host path on bind mount.
      struct stat stat;
      if (::stat(volume.container_path().c_str(), &stat) < 0) {
        return Failure("Failed to get permissions on '" +
                        volume.container_path() + "'" +
                        ": " + strerror(errno));
      }

      Try<Nothing> chmod = os::chmod(hostPath, stat.st_mode);
      if (chmod.isError()) {
        return Failure("Failed to chmod hostPath '" +
                       hostPath +
                       "': " +
                       chmod.error());
      }

      Try<Nothing> chown = os::chown(stat.st_uid, stat.st_gid, hostPath, false);
      if (chown.isError()) {
        return Failure("Failed to chown hostPath '" +
                       hostPath +
                       "': " +
                       chown.error());
      }
    } else {
      hostPath = volume.host_path();

      if (!os::exists(hostPath)) {
        return Failure("Volume with container path '" +
                      volume.container_path() +
                      "' must have host path '" +
                      hostPath +
                      "' present on host for shared filesystem isolator");
      }
    }

    commands.push_back("mount -n --bind " +
                       hostPath +
                       " " +
                       volume.container_path());
  }

  CommandInfo command;
  command.set_value(strings::join(" && ", commands));

  return command;
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


Future<Nothing> DockerVolumeDriverIsolatorProcess::cleanup(
    const ContainerID& containerId)
{
  if (!infos.contains(containerId)) {
	  return Nothing();
  }
  const Owned<Info>& info = infos[containerId];

  if (info->mountrootpath.length() <= REXRAY_MOUNT_PREFIX.length()) {
	  // too small to include a valid rexray mount
	  return Nothing();
  }

  if (!strings::startsWith(info->mountrootpath, REXRAY_MOUNT_PREFIX)) {
       continue; // not a mount created by this isolator
  }

  const std::string mountToRelease = info->mountrootpath;
  // we have now confirmed that this is a docker volume driver mount
  // next check if we are the last and only task using this mount
  for (std::map<ContainerID, process::Owned<Info>>::iterator ii=infos.begin(); ii!=infos.end(); ++ii) {
	  if ((*ii).first == containerId) {
		  // this is the entry for the task beiong cleaned up
		  continue;
	  }
	  if (mountToRelease == (*ii).second->mountrootpath) {
		  // another task is using this mount
		  // remove our use indication, but leave mount in place
		  infos.erase(containerId);
		  return Nothing();
	  }
  }

  const std::string volumeName = info->mountrootpath.substr(REXRAY_MOUNT_PREFIX.length());

  if (system(NULL)) { // Is a command processor available?
	std::string cmd = REXRAY_DVDCLI_UNMOUNT_CMD + volumeName;
    int i = system(cmd.c_str());
    if( 0 != i ) {
        return Failure("Failed to execute unmount command " + cmd );
    }
  }

  infos.erase(containerId);

  return Nothing();
}


} /* namespace slave */
} /* namespace internal */
} /* namespace mesos */
