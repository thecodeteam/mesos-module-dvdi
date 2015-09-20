
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

#ifndef SRC_DOCKER_VOLUME_DRIVER_ISOLATOR_HPP_
#define SRC_DOCKER_VOLUME_DRIVER_ISOLATOR_HPP_

#include <mesos/slave/isolator.hpp>

#include "slave/flags.hpp"

#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>

#include <stout/try.hpp>
#include <stout/option.hpp>


namespace mesos {
namespace internal {
namespace slave {

class DockerVolumeDriverIsolatorProcess: public mesos::slave::IsolatorProcess {
public:
	static Try<mesos::slave::Isolator*> create(const mesos::internal::slave::Flags& flags);

	virtual ~DockerVolumeDriverIsolatorProcess() {}

    virtual process::Future<Option<int>> namespaces();

	// Slave recovery is a feature of Mesos that allows task/executors
	// to keep running if a slave process goes down, AND
	// allows the slave process to reconnect with already running
	// slaves when it restarts
	// TODO This interface will change post 0.23.0 to pass a list of
	// of container states which will assist in recovery,
	// when this is available, code should use it.
    // TBD how determine volume mounts and device mounts
    //   rexray can report volume and device mount but
    //   unclear whether volume or device mount is related to mesos or not
    //   persistent store of volume mounts by task id may be needed
	virtual process::Future<Nothing> recover(
	      const std::list<mesos::slave::ExecutorRunState>& states,
	      const hashset<ContainerID>& orphans);

	// CALL THIS DOCKER VOLUME DRIVER ISOLATOR
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
	//    mount location is fixed, based on volume name (/ver/lib/rexray/volumes/
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
	virtual process::Future<Option<CommandInfo>> prepare(
	      const ContainerID& containerId,
	      const ExecutorInfo& executorInfo,
	      const std::string& directory,
		  const Option<std::string>& rootfs,
	      const Option<std::string>& user);

	// Nothing will be done at task start
	virtual process::Future<Nothing> isolate(
	      const ContainerID& containerId,
	      pid_t pid);

	// no-op
	virtual process::Future<mesos::slave::Limitation> watch(
	     const ContainerID& containerId);

	// no-op, nothing enforced
	virtual process::Future<Nothing> update(
	      const ContainerID& containerId,
	      const Resources& resources);

	// no-op, no usage stats gathered
	virtual process::Future<ResourceStatistics> usage(
	      const ContainerID& containerId);

	// will call unmount here
	// 1. Decrement use count
	//    TBD, may be more complex list of use by Task ID
	// 2. Unmount the volume
	//     dvdcli unmount
	//       should be synchronous call and check for success
	virtual process::Future<Nothing> cleanup(
	      const ContainerID& containerId);

private:
  DockerVolumeDriverIsolatorProcess(const Flags& flags);

  const Flags flags;

  struct Info
  {
	// We save the full root path of any mounted device here.
    const std::string mountrootpath;
  };

  hashmap<ContainerID, process::Owned<Info>> infos;

  const std::string REXRAY_MOUNT_PREFIX = "/var/lib/rexray/volumes/";
  const std::string REXRAY_DVDCLI_MOUNT_CMD = "go/src/github.com/clintonskitson/dvdcli/dvdcli mount --volumedriver=rexray --volumename=";
  const std::string REXRAY_DVDCLI_UNMOUNT_CMD = "go/src/github.com/clintonskitson/dvdcli/dvdcli unmount --volumedriver=rexray --volumename=";
  const std::string REXRAY_MOUNT_VOL_ENVIRONMENT_VAR_NAME = "/var/lib/rexray/volumes/";
};

} /* namespace slave */
} /* namespace internal */
} /* namespace mesos */

#endif /* SRC_DOCKER_VOLUME_DRIVER_ISOLATOR_HPP_ */
