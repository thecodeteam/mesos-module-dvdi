
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
#include <iostream>
#include <boost/functional/hash.hpp>
#include <boost/algorithm/string.hpp>
#include <mesos/mesos.hpp>
#include <mesos/slave/isolator.hpp>
#include <slave/flags.hpp>
#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>

#include <stout/multihashmap.hpp>
#include <stout/protobuf.hpp>
#include <stout/try.hpp>

namespace mesos {
namespace slave {

constexpr char    VOL_NAME_ENV_VAR_NAME[]   = "DVDI_VOLUME_NAME";
constexpr char    VOL_DRIVER_ENV_VAR_NAME[] = "DVDI_VOLUME_DRIVER";
constexpr char    VOL_OPTS_ENV_VAR_NAME[]   = "DVDI_VOLUME_OPTS";
// TODO: Remove these constants when we implement json array parsing from environment
constexpr char    VOL_NAME_ENV_VAR_NAME2[]  = "DVDI_VOLUME_NAME2";
constexpr char    VOL_DRIVER_ENV_VAR_NAME2[]= "DVDI_VOLUME_DRIVER2";
constexpr char    VOL_OPTS_ENV_VAR_NAME2[]  = "DVDI_VOLUME_OPTS2";
constexpr char    JSON_VOLS_ENV_VAR_NAME[]  = "DVDI_VOLS_JSON_ARRAY";

class DockerVolumeDriverIsolatorProcess: public mesos::slave::IsolatorProcess {
public:
  static Try<mesos::slave::Isolator*> create(const Parameters& parameters);

  virtual ~DockerVolumeDriverIsolatorProcess();

  // Slave recovery is a feature of Mesos that allows task/executors
  // to keep running if a slave process goes down, AND
  // allows the slave process to reconnect with already running
  // slaves when it restarts
  // TODO This interface will change post 0.23.0 to pass a list of
  // of container states which will assist in recovery,
  // when this is available, code should use it.
  // TBD how determine volume mounts and device mounts
  //   rexray can report volume and device mount but
  //   unclear whether volume or device mount is related to mesos or not.
  virtual process::Future<Nothing> recover(
    const std::list<mesos::slave::ExecutorRunState>& states,
    const hashset<ContainerID>& orphans);

  // Prepare runs BEFORE a task is started
  // will check if the volume is already mounted and if not,
  // will mount the volume
  //
  // 1. get volume identifier (from ENVIRONMENT from task in ExecutorInfo
  //     VOL_NAME_ENV_VAR_NAME is defined below
  //     This is volume name, not ID.
  //     Warning, name collisions on name can be trecherous.
  //     For now a simple string value is presumed, will need to enhance to
  //     support a JSON array to allow multiple volume mounts per task.
  // 2. get desired volume driver (volumedriver=) from ENVIRONMENT from task in ExecutorInfo
  //     VOL_DRIVER_ENV_VAR_NAME is defined below
  // 3. Check for other pre-existing users of the mount.
  // 4. Only if we are first user, make dvdcli mount call <volumename>
  //    Mount location is fixed, based on volume name (/var/lib/rexray/volumes/
  //    this call is synchrounous, and returns 0 if success
  //    actual call is defined below in DVDCLI_MOUNT_CMD
  // 5. Add entry to hashmap that contains root mountpath indexed by ContainerId
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

  // no-op, mount occurs at prepare
  virtual process::Future<mesos::slave::Limitation> watch(
     const ContainerID& containerId);

  // no-op, nothing enforced
  virtual process::Future<Nothing> update(
      const ContainerID& containerId,
      const Resources& resources);

  // no-op, no usage stats gathered
  virtual process::Future<ResourceStatistics> usage(
      const ContainerID& containerId);

  // will (possibly) unmount here
  // 1. Get mount root path by looking up based on ContainerId
  // 2. Verify that it is a rexray mount.
  // 3. Start counting tasks using this same mount. Quit counted after count == 2
  // 4. If count was exactly 1, Unmount the volume
  //     dvdcli unmount defined in DVDCLI_UNMOUNT_CMD below
  // 5. Remove the listing for this task's mount from hashmap
  virtual process::Future<Nothing> cleanup(
      const ContainerID& containerId);

  //friend std::ostream& operator<<(std::ostream& os, const ExternalMount& em);

private:
  DockerVolumeDriverIsolatorProcess(const Parameters& parameters);

  const Parameters parameters;

  typedef size_t ExternalMountID;

  struct ExternalMount
  {
    explicit ExternalMount(
        const std::string& _deviceDriverName,
        const std::string& _volumeName,
		const std::string& _mountOptions)
      : deviceDriverName(_deviceDriverName), volumeName(_volumeName), mountOptions(_mountOptions) {}

    bool operator ==(const ExternalMount& other) {
    	return getExternalMountId() == other.getExternalMountId();
    }

    ExternalMountID getExternalMountId(void) const {
        size_t seed = 0;
        std::string s1(boost::to_lower_copy(deviceDriverName));
        std::string s2(boost::to_lower_copy(volumeName));
        boost::hash_combine(seed, s1);
        boost::hash_combine(seed, s2);
        return seed;
    }

    // We save the full root path of any mounted device here.
    // note device driver name and volume name are not case sensitive,
    // but are stored as submitted in constructor
    const std::string deviceDriverName;
	const std::string volumeName;
    const std::string mountOptions;
  };

  // overload '<<' operator to allow rendering of ExternalMount
  friend inline std::ostream& operator<<(std::ostream& os, const ExternalMount& em)
  {
    os << em.deviceDriverName << '/' << em.volumeName
    << '(' << em.mountOptions << ")";
    return os;
  }

  typedef multihashmap<
    ContainerID, process::Owned<ExternalMount>> containermountmap;
  containermountmap infos;

  const std::string REXRAY_MOUNT_PREFIX       = "/var/lib/rexray/volumes/";
  const std::string DVDCLI_MOUNT_CMD          = "/usr/bin/dvdcli mount";
  const std::string DVDCLI_UNMOUNT_CMD        = "/usr/bin/dvdcli unmount";

  const std::string VOL_NAME_CMD_OPTION       = "--volumename=";
  const std::string VOL_DRIVER_CMD_OPTION     = "--volumedriver=";
  const std::string VOL_OPTS_CMD_OPTION       = "--volumeopts=";
  const std::string VOL_DRIVER_DEFAULT        = "rexray";

  const std::string VOL_NAME_ENV_VAR_NAME     = "DVDI_VOLUME_NAME";
  const std::string VOL_DRIVER_ENV_VAR_NAME   = "DVDI_VOLUME_DRIVER";
  const std::string VOL_OPTS_ENV_VAR_NAME     = "DVDI_VOLUME_OPTS";
  const std::string JSON_VOLS_ENV_VAR_NAME    = "DVDI_VOLS_JSON_ARRAY";
};

} /* namespace slave */
} /* namespace mesos */

#endif /* SRC_DOCKER_VOLUME_DRIVER_ISOLATOR_HPP_ */
