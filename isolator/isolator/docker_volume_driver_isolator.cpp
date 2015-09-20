
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

#include <process/process.hpp>
#include <process/subprocess.hpp>

#include <list>
#include <glog/logging.h>
#include "linux/fs.hpp"

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
  // specified at task launch
  // We need to rebuild mount ref counts using these.
  // However there is also a possibility that a task
  // terminated while we were gone, leaving a "orphanned" mount.
  // If any of these exist, they should be unmounted.

  // Read the mount table in the host mount namespace to recover paths
  // to device mountpoints.
  // Method 'cleanup()' relies on this information to clean
  // up mounts in the host mounts for each container.
  Try<fs::MountInfoTable> table = fs::MountInfoTable::read();
  if (table.isError()) {
    return Failure("Failed to get mount table: " + table.error());
  }

  foreach (const ContainerState& state, states) {
    Owned<Info> info(new Info(state.directory()));

	    foreach (const fs::MountInfoTable::Entry& entry, table.get().entries) {
	      if (entry.root == info->directory) {
	        info->sandbox = entry.target;
	        break;
	      }
	    }

	    infos.put(state.container_id(), info);
	  }

	  // Recover both known and unknown orphans by scanning the mount
	  // table and finding those mounts whose roots are under slave's
	  // sandbox root directory. Those mounts are container's work
	  // directory mounts. Mounts from unknown orphans will be cleaned up
	  // immediately. Mounts from known orphans will be cleaned up when
	  // those known orphan containers are being destroyed by the slave.
	  hashset<ContainerID> unknownOrphans;

	  string sandboxRootDir = paths::getSandboxRootDir(flags.work_dir);

	  foreach (const fs::MountInfoTable::Entry& entry, table.get().entries) {
	    if (!strings::startsWith(entry.root, sandboxRootDir)) {
	      continue;
	    }

	    // TODO(jieyu): Here, we retrieve the container ID by taking the
	    // basename of 'entry.root'. This assumes that the slave's sandbox
	    // root directory are organized according to the comments in the
	    // beginning of slave/paths.hpp.
	    ContainerID containerId;
	    containerId.set_value(Path(entry.root).basename());

	    if (infos.contains(containerId)) {
	      continue;
	    }

	    Owned<Info> info(new Info(entry.root));

	    if (entry.root != entry.target) {
	      info->sandbox = entry.target;
	    }

	    infos.put(containerId, info);

	    // Remember all the unknown orphan containers.
	    if (!orphans.contains(containerId)) {
	      unknownOrphans.insert(containerId);
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
// TODO use subprocess
// use <process::Subprocess> child = process::subprocess
  if (system(NULL)) { // Is a command processor available?
    int i = system("touch /tmp/SharedFilesystemIsolatorProcess-prepare-called.txt");
    if( 0 != i ) {
      LOG(WARNING) << "touch command failed in SharedFilesystemIsolatorProcess";
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

  // get things we need from task's environment in ExecutoInfo
  if (!executorInfo.command().has_environment()) {
	   // No environment means no external volume specification
	  // not an error, just nothing to do so return None.
	  return None();
  }
  executorInfo.command().environment().Environment().
  // iterate through the environment variables,
  // looking for the ones we need
  Environment::
  foreach (const Environment::Variable& variable,
           executorInfo.command().environment().variables()) {
    if (variable. )
  }

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
    // TODO(idownes): This test is unnecessarily strict and could be
    // relaxed if mounts could be re-ordered.
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
  // Cleanup of mounts is done automatically done by the kernel when
  // the mount namespace is destroyed after the last process
  // terminates.

  return Nothing();
}


} /* namespace slave */
} /* namespace internal */
} /* namespace mesos */
