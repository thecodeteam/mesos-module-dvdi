
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

#include <fstream>
#include <list>
#include <array>
#include <iostream>
#include <sstream>

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
#include <stout/error.hpp>
#include <stout/nothing.hpp>
#include <stout/os.hpp>
#include <stout/format.hpp>
#include <stout/strings.hpp>

using namespace process;

using std::list;
using std::set;
using std::string;
using std::array;

using namespace mesos;
using namespace mesos::slave;

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
using mesos::slave::ExecutorRunState;
using mesos::slave::IsolatorProcess;
using mesos::slave::Limitation;
#endif
using mesos::slave::Isolator;

//TODO temporary code until checkpoints are public by mesosphere dev
#include <stout/path.hpp>
#include <slave/paths.hpp>
#include <slave/state.hpp>
using namespace mesos::internal::slave::paths;
using namespace mesos::internal::slave::state;
//TODO temporary code until checkpoints are public by mesosphere dev


const char DockerVolumeDriverIsolator::prohibitedchars[NUM_PROHIBITED]  =
{
  '%', '/', ':', ';', '\0',
  '<', '>', '|', '`', '$', '\'',
  '?', '^', '&', ' ', '{', '\"',
  '}', '[', ']', '\n', '\t', '\v', '\b', '\r', '\\'
};

std::string DockerVolumeDriverIsolator::mountPbFilename;
std::string DockerVolumeDriverIsolator::mesosWorkingDir;


DockerVolumeDriverIsolator::DockerVolumeDriverIsolator(
  const Parameters& _parameters)
  : parameters(_parameters)
  {
    // Verify that the version of the library that we linked against is
    // compatible with the version of the headers we compiled against.
    GOOGLE_PROTOBUF_VERIFY_VERSION;
  }

Try<Isolator*> DockerVolumeDriverIsolator::create(
    const Parameters& parameters)
{
  Result<string> user = os::user();
  if (!user.isSome()) {
    return Error("Failed to determine user: " +
                 (user.isError() ? user.error() : "username not found"));
  }

  if (user.get() != "root") {
    return Error("DockerVolumeDriverIsolator requires root privileges");
  }

  LOG(INFO) << "DockerVolumeDriverIsolator::create() called";
  mountPbFilename = DVDI_MOUNTLIST_DEFAULT_DIR;
  //TODO: we dont have the flags.work_dir yet. Hardcoded for /tmp/mesos
  //this will be overwritten with environment parameters below
  mesosWorkingDir = DEFAULT_WORKING_DIR;

  foreach (const Parameter& parameter, parameters.parameter()) {
    if (parameter.key() == DVDI_WORKDIR_PARAM_NAME) {
      LOG(INFO) << "parameter " << parameter.key() << ":" << parameter.value();

      if (parameter.value().length() > 2 &&
          strings::startsWith(parameter.value(), "/") &&
          strings::endsWith(parameter.value(), "/")) {
        mountPbFilename = parameter.value();
        mesosWorkingDir = parameter.value();
      } else {
        std::stringstream ss;
        ss << "DockerVolumeDriverIsolator " << DVDI_WORKDIR_PARAM_NAME
           << " parameter is invalid, must start and end with /";
        return Error(ss.str());
      }
    }
  }

  mountPbFilename = path::join(getMetaRootDir(mesosWorkingDir),
                                 DVDI_MOUNTLIST_FILENAME);
  LOG(INFO) << "using " << mountPbFilename;

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
  process::Owned<IsolatorProcess> process(
      new DockerVolumeDriverIsolator(parameters));

  return new Isolator(process);
#else
  return new DockerVolumeDriverIsolator(parameters);
#endif
}

DockerVolumeDriverIsolator::~DockerVolumeDriverIsolator()
{
  // Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();
}

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
Future<Nothing> DockerVolumeDriverIsolator::recover(
    const list<ExecutorRunState>& states,
    const hashset<ContainerID>& orphans)
#else
Future<Nothing> DockerVolumeDriverIsolator::recover(
    const list<ContainerState>& states,
    const hashset<ContainerID>& orphans)
#endif
{
  LOG(INFO) << "DockerVolumeDriverIsolator recover() was called";

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

  // originalContainerMounts is a multihashmap is similar to the infos
  // multihashmap but note that the key is an std::string instead of a
  // ContainerID. This is because some of the ContainerIDs present when
  // it was recorded may now be gone. The key is a string rendering of the
  // ContainerID but not a ContainerID.
  multihashmap<std::string, process::Owned<ExternalMount>>
      originalContainerMounts;

  // Recover the state.
  //TODO: need public version of recover in checkpointing
  LOG(INFO) << "dvdicheckpoint::recover() called";
  Result<State> resultState =
    mesos::internal::slave::state::recover(mesosWorkingDir, true);

  State state = resultState.get();
  LOG(INFO) << "dvdicheckpoint::recover() returned: " << state.errors;

  if (state.errors != 0) {
    LOG(INFO) << "recover state error:" << state.errors;
    return Nothing();
  }

  // read container mounts from filesystem
  std::ifstream ifs(mountPbFilename);

  if (!os::exists(mountPbFilename)) {
    LOG(INFO) << "No mount protobuf file exists at " << mountPbFilename
              << " so there are no mounts to recover";
    return Nothing();
  }

  LOG(INFO) << "Parsing mount protobuf file(" << mountPbFilename
            << ") in recover()";

  ExternalMountList mountlist;
  if( !mountlist.ParseFromIstream(&ifs) )
  {
    LOG(INFO) << "Invalid protobuf data contained within " << mountPbFilename;
    return Nothing();
  }

  for (int i = 0; i < mountlist.mount_size(); i++)
  {
    ExternalMount mount = mountlist.mount(i);

    std::string data;
    bool bSerialize = mount.SerializeToString(&data);
    LOG(INFO) << "External Mount: ";
    LOG(INFO) << data;

    if (bSerialize) {
      if (containsProhibitedChars(mount.volumedriver())) {
        LOG(ERROR) << "Volumedriver element in protobuf contains an illegal "
                   << "character, mount will be ignored";
        mount.set_volumedriver(std::string(""));
      }

      if (containsProhibitedChars(mount.volumename())) {
        LOG(ERROR) << "Volumename element in protobuf contains an illegal "
                   << "character, mount will be ignored";
        mount.set_volumename(std::string(""));
      }

      if (!mount.containerid().empty() && !mount.volumename().empty()) {
        LOG(INFO) << "Adding to legacyMounts: ";
        LOG(INFO) << mount.SerializeAsString();

        originalContainerMounts.put(mount.containerid(),
          process::Owned<ExternalMount>(&mount));
      }
    }
  }

  LOG(INFO) << "Parsed " << mountPbFilename
            << " and found evidence of " << originalContainerMounts.size()
            << " previous active external mounts in recover()";

  // Both maps start empty, we will iterate to populate.
  using externalmountmap =
    hashmap<ExternalMountID, process::Owned<ExternalMount>>;
  // legacyMounts is a list of all mounts in use according to mount list file.
  externalmountmap legacyMounts;
  // inUseMounts is a list of all mounts deduced to be still in use now.
  externalmountmap inUseMounts;

  // Populate legacyMounts with all mounts at time file was written.
  // Note: some of the tasks using these may be gone now.
  for (const auto &elem : originalContainerMounts) {
    legacyMounts.put(getExternalMountId(*(elem.second.get())), elem.second);
  }

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
  foreach (const ExecutorRunState& state, states) {

    if (originalContainerMounts.contains(state.id.value())) {

      // We found a task that is still running and has mounts.
      LOG(INFO) << "Running container(" << state.id
                << ") re-identified on recover()";
      LOG(INFO) << "State.directory is (" << state.directory << ")";
      std::list<process::Owned<ExternalMount>> mountsForContainer =
          originalContainerMounts.get(state.id.value());

      for (const auto &iter : mountsForContainer) {

        // Copy task element to rebuild infos.
        infos.put(state.id, iter);
        ExternalMountID id = getExternalMountId(*iter);
        LOG(INFO) << "Re-identified a preserved mount, id is " << id;
        inUseMounts.put(id, iter);
      }
    }
  }
#else
  foreach (const ContainerState& state, states)
  {
    if (originalContainerMounts.contains(state.container_id().value())) {

      // We found a task that is still running and has mounts.
      LOG(INFO) << "Running container(" << state.container_id().value()
                << ") re-identified on recover()";
      LOG(INFO) << "State.directory is (" << state.directory() << ")";

      std::list<process::Owned<ExternalMount>> mountsForContainer =
          originalContainerMounts.get(state.container_id().value());

      for (const auto &iter : mountsForContainer) {
        // Copy task element to rebuild infos.
        infos.put(state.container_id(), iter);
        ExternalMountID id = getExternalMountId(*iter);
        LOG(INFO) << "Re-identified a preserved mount, id is " << id;
        inUseMounts.put(id, iter);
      }
    }
  }
#endif

  // Create ExternalMountList protobuf message to checkpoint
  ExternalMountList inUseMountsProtobuf;
  for( const auto &iter : inUseMounts) {
    ExternalMount* mount = inUseMountsProtobuf.add_mount();
    mount->CopyFrom(*(iter.second.get()));
  }

  //checkpoint the dvdi mounts for persistence
  mesos::internal::slave::state::checkpoint(mountPbFilename,
      inUseMountsProtobuf);

  // We will now reduce legacyMounts to only the mounts that should be removed.
  // We will do this by deleting the mounts still in use.
  for( const auto &iter : inUseMounts) {
    legacyMounts.erase(iter.first);
  }

  // legacyMounts now contains only "orphan" mounts whose task is gone.
  // We will attempt to unmount these.
  for (const auto &iter : legacyMounts) {
    if (!unmount(*(iter.second.get()), "recover()")) {
      return Failure("recover() failed during unmount attempt");
    }
  }

  return Nothing();
}

// Attempts to unmount specified external mount, returns true on success.
// Also returns true so long as DVDCLI is successfully invoked,
// even if a non-zero return code occurs.
bool DockerVolumeDriverIsolator::unmount(
    const ExternalMount& em,
    const std::string&   callerLabelForLogging ) const
{
  LOG(INFO) << em.volumedriver() << "/" << em.volumename()
            << " is being unmounted on "
            << callerLabelForLogging;

  if (system(NULL)) { // Is a command processor available?

    LOG(INFO) << "Invoking " << DVDCLI_UNMOUNT_CMD << " "
              << VOL_DRIVER_CMD_OPTION << em.volumedriver() << " "
              << VOL_NAME_CMD_OPTION << em.volumename();

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
    std::ostringstream cmdOut;
    Try<int> retcode = os::shell(&cmdOut, "%s %s%s %s%s",
      DVDCLI_UNMOUNT_CMD,
      VOL_DRIVER_CMD_OPTION,
      em.volumedriver().c_str(),
      VOL_NAME_CMD_OPTION,
      em.volumename().c_str());
#else
    Try<string> retcode = os::shell("%s %s%s %s%s ",
      DVDCLI_UNMOUNT_CMD,
      VOL_DRIVER_CMD_OPTION,
      em.volumedriver().c_str(),
      VOL_NAME_CMD_OPTION,
      em.volumename().c_str());
#endif

    if (retcode.isError()) {
      LOG(WARNING) << DVDCLI_UNMOUNT_CMD << " failed to execute on "
                   << callerLabelForLogging
                   << ", continuing on the assumption this volume was "
                   << "manually unmounted previously "
                   << retcode.error();
    } else {
#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
      LOG(INFO) << DVDCLI_UNMOUNT_CMD << " returned " << retcode.get() << ", "
                << cmdOut.str();
#else
      LOG(INFO) << DVDCLI_UNMOUNT_CMD << " returned " << retcode.get();
#endif
    }
  } else {
    LOG(ERROR) << "failed to acquire a command processor for unmount on "
               << callerLabelForLogging;
    return false;
  }

  return true;
}

// Attempts to mount specified external mount,
// returns non-empty string (mountpoint) on success.
std::string DockerVolumeDriverIsolator::mount(
    const ExternalMount& em,
    const std::string&   callerLabelForLogging) const
{
  LOG(INFO) << em.volumedriver() << "/" << em.volumename()
            << " is being mounted on "
            << callerLabelForLogging;

  const std::string volumeDriver = em.volumedriver();
  const std::string volumeName = em.volumename();
  std::string mountpoint; // Return value init'd to empty.

  if (system(NULL)) { // Is a command processor available?
    LOG(INFO) << "Invoking " << DVDCLI_MOUNT_CMD << " "
              << VOL_DRIVER_CMD_OPTION << em.volumedriver() << " "
              << VOL_NAME_CMD_OPTION << em.volumename() << " "
              << em.options();

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
    std::ostringstream cmdOut;
    Try<int> retcode = os::shell(&cmdOut, "%s %s%s %s%s %s",
      DVDCLI_MOUNT_CMD,
      VOL_DRIVER_CMD_OPTION,
      em.volumedriver().c_str(),
      VOL_NAME_CMD_OPTION,
      em.volumename().c_str(),
      em.options().c_str());
#else
    Try<string> retcode = os::shell("%s %s%s %s%s %s",
      DVDCLI_MOUNT_CMD,
      VOL_DRIVER_CMD_OPTION,
      em.volumedriver().c_str(),
      VOL_NAME_CMD_OPTION,
      em.volumename().c_str(),
      em.options().c_str());
#endif

    if (retcode.isError()) {
      LOG(ERROR) << DVDCLI_MOUNT_CMD << " failed to execute on "
                 << callerLabelForLogging
                 << retcode.error();
    } else {
#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
      if (retcode.get() != 0) {
        LOG(ERROR) << DVDCLI_MOUNT_CMD << " returned errorcode "
                   << retcode.get();
      } else if (strings::trim(cmdOut.str()).empty()) {
        LOG(ERROR) << DVDCLI_MOUNT_CMD
                   << " returned an empty mountpoint name";
      } else {
        mountpoint = strings::trim(cmdOut.str());
        LOG(INFO) << DVDCLI_MOUNT_CMD << " returned mountpoint:"
                  << mountpoint;
      }
#else
      if (strings::trim(retcode.get()).empty()) {
        LOG(ERROR) << DVDCLI_MOUNT_CMD
                   << " returned an empty mountpoint name";
      } else {
        mountpoint = strings::trim(retcode.get());
        LOG(INFO) << DVDCLI_MOUNT_CMD << " returned mountpoint:"
                  << mountpoint;
      }
#endif
    }
  } else {
    LOG(ERROR) << "Failed to acquire a command processor for unmount on "
               << callerLabelForLogging;
  }
  return mountpoint;
}

bool DockerVolumeDriverIsolator::containsProhibitedChars(
    const std::string& s) const
{
  return (string::npos != s.find_first_of(prohibitedchars, 0, NUM_PROHIBITED));
}

bool DockerVolumeDriverIsolator::parseEnvVar(
  const Environment_Variable&  envvar,
  const char*                  expectedName,
  envvararray                  (&insertTarget),
  bool                         limitCharset) const
{
  const size_t prefixLength = strlen(expectedName);
  if (!strings::startsWith(envvar.name(), expectedName) ||
      envvar.name().length() > (prefixLength+1) ) {
    LOG(ERROR) << "Environment variable " << envvar.name()
               << " rejected because it's name is invalid.";
    return false;
  }
  if (limitCharset && containsProhibitedChars(envvar.value())) {
    LOG(ERROR) << "Environment variable " << envvar.name()
               << " rejected because it's value contains "
               << "prohibited characters";
    return false;
  }

  size_t index = 0;
  if (envvar.name().length() == (prefixLength+1)) {
    char digit = envvar.name().data()[prefixLength];

    if (!isdigit(digit)) {
      LOG(ERROR) << "Environment variable " << envvar.name()
                 << " rejected because it doesn't end with a digit.";
      return false;
    }
    index = std::atoi(envvar.name().substr(prefixLength).c_str());
  }

  insertTarget[index] = envvar.value();
  LOG(INFO) << envvar.name()  << "("
            << envvar.value() << ") parsed from environment";
  return true;
}

// Prepare runs BEFORE a task is started
// will check if the volume is already mounted and if not,
// will mount the volume.
// A container can ask for multiple mounts, but if
// there are any problems parsing or mounting even one
// mount, we want to exit with an error and no new
// mounted volumes. Goal: make all mounts or none.
#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
Future<Option<CommandInfo>> DockerVolumeDriverIsolator::prepare(
  const ContainerID& containerId,
  const ExecutorInfo& executorInfo,
  const string& directory,
  const Option<string>& rootfs,
  const Option<string>& user)
#else
Future<Option<ContainerPrepareInfo>> DockerVolumeDriverIsolator::prepare(
  const ContainerID& containerId,
  const ExecutorInfo& executorInfo,
  const string& directory,
  const Option<string>& user)
#endif
{
  LOG(INFO) << "Preparing external storage for container: "
            << stringify(containerId);

  if (infos.contains(containerId)) {
    return Failure("Container has already been prepared");
  }

  // Get things we need from task's environment in ExecutoInfo.
  if (!executorInfo.command().has_environment()) {
    // No environment means no external volume specification.
    // Not an error, just nothing to do, so return None.
    LOG(INFO) << "No environment specified for container ";
    return None();
  }

  ContainerPrepareInfo prepareInfo;
  prepareInfo.set_namespaces(CLONE_NEWNS); // new filesystem namespace

  // We accept <environment-var-name>#, where # can be 1-9, saved in array[#].
  // We also accept <environment-var-name>, saved in array[0].
  // parsing is "messy" because we don't insist that environment
  // variable are in any particular order, or grouped by volume #
  envvararray deviceDriverNames;
  envvararray volumeNames;
  envvararray mountOptions;
  envvararray containerPaths;

  // Iterate through the environment variables,
  // looking for the ones we need.
  foreach (const auto &variable,
           executorInfo.command().environment().variables()) {

    if (strings::startsWith(variable.name(), VOL_NAME_ENV_VAR_NAME)) {
      if (!parseEnvVar(variable, VOL_NAME_ENV_VAR_NAME, volumeNames, true)) {
        return Failure("prepare() failed due to illegal environment variable");
      }
    } else if (strings::startsWith(variable.name(), VOL_DRIVER_ENV_VAR_NAME)) {
      if (!parseEnvVar(
          variable,
          VOL_DRIVER_ENV_VAR_NAME,
          deviceDriverNames,
          true)) {
        return Failure("prepare() failed due to illegal environment variable");
      }
    } else if (strings::startsWith(variable.name(), VOL_OPTS_ENV_VAR_NAME)) {
      if (!parseEnvVar(variable, VOL_OPTS_ENV_VAR_NAME, mountOptions, true)) {
        return Failure("prepare() failed due to illegal environment variable");
      }
    } else if (strings::startsWith(variable.name(), VOL_CPATH_ENV_VAR_NAME)) {
      if (!parseEnvVar(
          variable,
          VOL_CPATH_ENV_VAR_NAME,
          containerPaths,
          false)) {
        return Failure("prepare() failed due to illegal environment variable");
      }
    }
  }

  // requestedExternalMounts is all mounts requested by container.
  std::vector<process::Owned<ExternalMount>> requestedExternalMounts;

  // unconnectedExternalMounts is the subset of those not already
  // in use by another container.
  std::vector<process::Owned<ExternalMount>> unconnectedExternalMounts;

  // prevConnectedExternalMounts is the subset of those that are
  // in use by another container.
  std::vector<process::Owned<ExternalMount>> prevConnectedExternalMounts;

  // Not using iterator because we access all 4 arrays using common index.
  for (size_t i = 0; i < volumeNames.size(); i++) {

    if (volumeNames[i].empty()) {
      continue;
    }

    LOG(INFO) << "Validating mount name " << volumeNames[i];

    if (deviceDriverNames[i].empty()) {
      deviceDriverNames[i] = VOL_DRIVER_DEFAULT;
    }

    if (containerPaths[i].empty()) {
      containerPaths[i] = mesosWorkingDir + '/' +
          deviceDriverNames[i] +
          "/volumes/" +
          volumeNames[i];
    }

    // note: mountpoint is not set yet, because we haven't mounted yet
    process::Owned<ExternalMount> mount(
      Builder().setContainerId(stringify(containerId))
               .setVolumeDriver(deviceDriverNames[i])
               .setVolumeName(volumeNames[i])
               .setOptions(mountOptions[i])
               .setContainerPath(containerPaths[i])
               .build()
      );

    // Check for duplicates in environment.
    bool duplicateInEnv = false;
    for (const auto &ent : requestedExternalMounts) {

      if (getExternalMountId(*(ent.get())) ==
             getExternalMountId(*(mount.get())) ) {
        duplicateInEnv = true;
        break;
      }
    }

    if (duplicateInEnv) {
      LOG(INFO) << "Duplicate mount request("
                << (*mount).SerializeAsString()
                << ") in environment will be ignored";
      continue;
    }

    requestedExternalMounts.push_back(mount);

    // Now check if another container is already using this same mount.
    bool mountInUse = false;
    for (const auto &ent : infos) {

      if (getExternalMountId(*(ent.second.get())) ==
            getExternalMountId(*(mount.get())) ) {
        mountInUse = true;
        // TODO must make a new External Mount here because
        // container path can be different, can't
        // copy ent.second
        mount->set_mountpoint(ent.second->mountpoint());
        prevConnectedExternalMounts.push_back(mount);
        LOG(INFO) << "Requested mount(" << mount->volumedriver() << "/"
                  << mount->volumename()
                  << ") is already mounted by another container";
        break;
      }
    }

    if (!mountInUse) {
      unconnectedExternalMounts.push_back(mount);
    }
  }

  // As we connect mounts we will build a list of successful mounts.
  // We need this because, if there is a failure, we need to unmount these.
  // The goal is we mount either ALL or NONE.
  std::vector<process::Owned<ExternalMount>> successfulExternalMounts;
  for (const auto &iter : unconnectedExternalMounts) {
    std::string mountpoint = mount(*iter, "prepare()");

    if (!mountpoint.empty()) {

      // Need to construct a newExternalMount because we just
      // learned the mountpoint.
      process::Owned<ExternalMount> newmount(
        Builder().setContainerId(stringify(containerId))
                 .setVolumeDriver(iter->volumedriver())
                 .setVolumeName(iter->volumename())
                 .setOptions(iter->options())
                 .setContainerPath(iter->container_path())
                 .setMountPoint(mountpoint)
                 .build()
        );

      successfulExternalMounts.push_back(newmount);
    } else {
      // Once any mount attempt fails, give up on whole list
      // and attempt to undo the mounts we already made.
      LOG(ERROR) << "Mount failed during prepare()";

      for (const auto &unmountme : successfulExternalMounts) {

        if (unmount(*unmountme, "prepare()-reverting mounts after failure")) {
          LOG(ERROR) << "During prepare() of a container requesting multiple "
                     << "mounts, a mount failure occurred after making "
                     << "at least one mount and a second failure occurred "
                     << "while attempting to remove the earlier mount(s)";
          break;
        }
      }
      return Failure("prepare() failed during mount attempt");
    }
  }

  // we need to attach previous and newly created mounts to
  // container path now


  // Note: infos has a record for each mount associated with this container
  // even if the mount is also used by another container.
  for (const auto &iter : prevConnectedExternalMounts) {

    LOG(INFO) << "mount " << iter->mountpoint() << " was previously connected";
    LOG(INFO) << "queueing mount -n --rbind " << iter->mountpoint()
              << " " << iter->container_path();
    prepareInfo.add_commands()->set_value(
        "mount -n --rbind " +
        iter->mountpoint() + " " +
        iter->container_path());
    infos.put(containerId, iter);
  }

  for (const auto &iter : successfulExternalMounts) {
    std::string containerPath = iter->container_path();
    std::string mountPoint = iter->mountpoint();

    // Set the ownership and permissions to match the container path
    // as these are inherited from host path on bind mount.
    struct stat stat;
    if (::stat(containerPath.c_str(), &stat) < 0) {
      return Failure("Failed to get permissions on '" +
                      containerPath + "'" +
                      ": " + strerror(errno));
    }

    Try<Nothing> chmod = os::chmod(mountPoint, stat.st_mode);
    if (chmod.isError()) {
      return Failure("Failed to chmod hostPath '" +
                     mountPoint +
                     "': " +
                     chmod.error());
    }

    Try<Nothing> chown = os::chown(stat.st_uid, stat.st_gid, mountPoint, false);
    if (chown.isError()) {
      return Failure("Failed to chown hostPath '" +
                     mountPoint +
                     "': " +
                     chown.error());
    }

    LOG(INFO) << "queueing mount -n --rbind " << mountPoint
    << " " << containerPath;
    // -n means don't write to /etc/mtab
    prepareInfo.add_commands()->set_value(
            "mount -n --rbind " + mountPoint + " " + containerPath);

    infos.put(containerId, iter);
  }

  // Create ExternalMountList protobuf message to checkpoint
  ExternalMountList inUseMountsProtobuf;
  for( const auto &iter : infos) {
    ExternalMount* mount = inUseMountsProtobuf.add_mount();
    mount->CopyFrom(*(iter.second.get()));
  }
  mesos::internal::slave::state::checkpoint(mountPbFilename,
    inUseMountsProtobuf);

  return prepareInfo;
}

#if MESOS_VERSION_INT != 0 && MESOS_VERSION_INT < 0240
Future<Limitation> DockerVolumeDriverIsolator::watch(
    const ContainerID& containerId)
{
  // No-op, for now.

  return Future<Limitation>();
}
#else
Future<ContainerLimitation> DockerVolumeDriverIsolator::watch(
    const ContainerID& containerId)
{
  // No-op, for now.

  return Future<ContainerLimitation>();
}
#endif

Future<Nothing> DockerVolumeDriverIsolator::update(
    const ContainerID& containerId,
    const Resources& resources)
{
  // No-op, nothing enforced.

  return Nothing();
}


Future<ResourceStatistics> DockerVolumeDriverIsolator::usage(
    const ContainerID& containerId)
{
  // No-op, no usage gathered.

  return ResourceStatistics();
}

process::Future<Nothing> DockerVolumeDriverIsolator::isolate(
    const ContainerID& containerId,
    pid_t pid)
{
  // No-op, isolation happens when mounting/unmounting in prepare/cleanup
  return Nothing();
}

Future<Nothing> DockerVolumeDriverIsolator::cleanup(
    const ContainerID& containerId)
{
  //    1. Get driver name and volume list from infos.
  //    2. Iterate list and perform unmounts.

  if (!infos.contains(containerId)) {
    return Nothing();
  }

  std::list<process::Owned<ExternalMount>> mountsList =
      infos.get(containerId);
  // mountList now contains all the mounts used by this container.

  // Note: it is possible that some of these mounts are
  // also used by other tasks.
  for( const auto &iter : mountsList) {
    size_t mountCount = 0;

    for (const auto &elem : infos) {
      // elem.second is ExternalMount.

      if (getExternalMountId(*iter) ==
        getExternalMountId(*(elem.second.get()))) {
        if( ++mountCount > 1) {
           break; // As soon as we find two users we can quit.
        }
      }
    }

    if (1 == mountCount) {
      // This container was the only, or last, user of this mount.

      if (!unmount(*iter, "cleanup()")) {
        return Failure("cleanup() failed during unmount attempt");
      }
    }
  }

  // Remove all this container's mounts from infos.
  infos.remove(containerId);

  // Create ExternalMountList protobuf message to checkpoint
  ExternalMountList inUseMountsProtobuf;
  for( const auto &iter : infos) {
    ExternalMount* mount = inUseMountsProtobuf.add_mount();
    mount->CopyFrom(*(iter.second.get()));
  }
  mesos::internal::slave::state::checkpoint(mountPbFilename,
    inUseMountsProtobuf);

  return Nothing();
}

static Isolator* createDockerVolumeDriverIsolator(const Parameters& parameters)
{
  LOG(INFO) << "Loading Docker Volume Driver Isolator module";

  Try<Isolator*> result = DockerVolumeDriverIsolator::create(parameters);

  if (result.isError()) {
    return NULL;
  }

  return result.get();
}

// Declares the isolator named com_emccode_mesos_DockerVolumeDriverIsolator.
mesos::modules::Module<Isolator> com_emccode_mesos_DockerVolumeDriverIsolator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "emc{code}",
    "emccode@emc.com",
    "Docker Volume Driver Isolator module.",
    NULL,
    createDockerVolumeDriverIsolator);
