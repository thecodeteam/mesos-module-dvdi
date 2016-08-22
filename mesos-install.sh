#!/bin/bash   

#install mesos
# This script is an installation aid for installing Mesos on a pre-existing AWS Ubuntu instance.
# A t2.small is the recommended minimum instance type (2 cpu+2GB memory)
# This is a aid for developers using an AWS environment for testing.
# This script is not used during a build, or in production deployments.

# Make sure only root can run our script
if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 1>&2
   exit 1
fi

# Validate parameters
if [ $# -ne 2 ]; then
    echo $0: usage: mesos-install version hostname
    echo example: meoss-install 1.0.0 ec2-42-40-251-160.us-west-2.compute.amazonaws.com 
    exit 1
fi

version=$1
myhostname=$2
myip=$(hostname -i)

dnslookup=$(dig +short $myhostname)
if [ "$dnslookup" != "$myip" ]; then
    echo hostname $2 is invalid - it does not resolve to the ip of this host
    exit 1
fi

hostnamectl set-hostname $myhostname

# Add Mesosphere repositories
apt-key adv --keyserver keyserver.ubuntu.com --recv E56151BF
DISTRO=$(lsb_release -is | tr '[:upper:]' '[:lower:]')
CODENAME=$(lsb_release -cs)
echo "deb http://repos.mesosphere.io/${DISTRO} ${CODENAME} main" | sudo tee /etc/apt/sources.list.d/mesosphere.list

# map version to specific package in Mesosphere repository
# add additional lines when new releases of Mesos occur
case "$version" in
1.0.0)  mesosver=1.0.0-2.0.89.ubuntu1404 ;;
0.28.2) mesosver=0.28.2-2.0.27.ubuntu1404 ;;
0.28.1) mesosver=0.28.1-2.0.20.ubuntu1404 ;;
0.28.0) mesosver=0.28.0-2.0.16.ubuntu1404 ;;
0.27.2) mesosver=0.27.2-2.0.15.ubuntu1404 ;;
0.27.1) mesosver=0.27.1-2.0.226.ubuntu1404 ;;
0.27.0) mesosver=0.27.0-0.2.190.ubuntu1404 ;;
0.26.2) mesosver=0.26.2-2.0.60.ubuntu1404 ;;
0.26.1) mesosver=0.26.1-2.0.19.ubuntu1404 ;;
0.26.0) mesosver=0.26.0-0.2.145.ubuntu1404 ;;
0.25.1) mesosver=0.25.1-2.0.21.ubuntu1404 ;;
0.25.0) mesosver=0.25.0-0.2.70.ubuntu1404 ;;
0.24.2) mesosver=0.24.2-2.0.17.ubuntu1404 ;;
0.24.1) mesosver=0.24.1-0.2.35.ubuntu1404 ;;
0.24.0) mesosver=0.24.0-1.0.27.ubuntu1404 ;;
0.23.1) mesosver=0.23.1-0.2.61.ubuntu1404 ;;
0.23.0) mesosver=0.23.0-1.0.ubuntu1404 ;;
*) echo "bad version argument $1"
   exit 1
esac

marathonver=1.1.2-1.0.482.ubuntu1404

add-apt-repository -y ppa:webupd8team/java

apt-get -y update

# Install Oracle Java - note this will trigger a license "popup"
apt-get -y install oracle-java8-installer
apt-get install oracle-java8-set-default

apt-get install mesos=$mesosver
apt-mark hold mesos
apt-get -y install marathon=$marathonver

# Write zookeeper configuration
stop zookeeper
echo "1" > /etc/zookeeper/conf/myid
echo "server.1=${myip}:2888:3888" >> /etc/zookeeper/conf/zoo.cfg
start zookeeper

# Write Mesos master configuration
echo "zk://${myip}:2181/mesos" > /etc/mesos/zk

echo "1" > /etc/mesos-master/quorum
echo "$myhostname" > /etc/mesos-master/hostname
echo "$myip" > /etc/mesos-master/ip

# Write Mesos agent configuration
echo "$myhostname" > /etc/mesos-slave/hostname
echo "$myip" > /etc/mesos-slave/ip
echo "mesos" > /etc/mesos-slave/containerizers
echo "5mins" > /etc/mesos-slave/executor_registration_timeout

# Write marathon configuration
mkdir -p /etc/marathon/conf
echo "$myhostname" > /etc/marathon/conf/hostname
echo "zk://${myip}:2181/marathon" > /etc/marathon/conf/zk
echo "zk://${myip}:2181/mesos" > /etc/marathon/conf/master

curl -sSL https://dl.bintray.com/emccode/rexray/install | sh -s -- stable 0.3.3
curl -sSL https://dl.bintray.com/emccode/dvdcli/install | sh -s stable

# Write Rex-Ray configuration for an AWS hosted Mesos agent
# Do not keep your keys in this shell script.
# Run this script run and use an editor after to modify the keys
cat > /etc/rexray/config.yml << EOF
rexray:
  storageDrivers:
  - ec2
aws:
    accessKey: REPLACE_WITH_YOUR_ACCESS_KEY_HERE
    secretKey: REPLACE_WITH_YOUR_SECRET_KEY_HERE
EOF

# Write configuration for dvdi isolator module
echo "com_emccode_mesos_DockerVolumeDriverIsolator" > /etc/mesos-slave/isolation
echo "file:///usr/lib/dvdi-mod.json" > /etc/mesos-slave/modules
cat > /usr/lib/dvdi-mod.json << EOF
 {
   "libraries": [
     {
       "file": "/usr/lib/libmesos_dvdi_isolator.so",
       "modules": [
         {
           "name": "com_emccode_mesos_DockerVolumeDriverIsolator"
         }
       ]
     }
   ]
 }
EOF

start mesos-master
start marathon

echo The remainder of this script contains example commands for installing a specific version of the dvdi isolation
echo This needs to be customized for your own requirements. The steps are here for documentation.
echo It is expected that these steps would be performed manually. 
echo Please examine the lines in the script after the exit 0 and adapt as needed

# Prepare a directory for use by container mounts
mkdir -p /var/lib/mesos/containermounts/webserv

# write a json file for testing deployment of a task using an external volume mount deployed by Marathon
# this application mounts a volume and appends a new Chuck Norris joke each time the application starts
# The state s persisted after the application terminates, so the jokes will build up
# The application hosts a web server that can be used to see the joke inventory, a file per joke
cat > /home/ubuntu/marathontest.json << EOF
{
  "id": "simpwebserv",
  "cmd": "cd /var/lib/mesos/containermounts/webserv && now=$(date +%m_%d_%Y_%T_%Z) && curl -o run_$now.json http://api.icndb.com/jokes/random && python -m SimpleHTTPServer $PORT",
  "mem": 32,
  "cpus": 0.1,
  "instances": 1,
  "env": {
    "DVDI_VOLUME_NAME": "mesoscon",
    "DVDI_VOLUME_OPTS": "size=5,iops=150,volumetype=io1,newfstype=xfs,overwritefs=false",
    "DVDI_VOLUME_DRIVER": "rexray",
    "DVDI_VOLUME_CONTAINERPATH": "/var/lib/mesos/containermounts/webserv"
  }
}
EOF
chown ubuntu:ubuntu /home/ubuntu/marathontest.json

exit 0

# Download a specific version of the dvdi isolator .so and install it
wget "-O /tmp/libmesos_dvdi_isolator-$version.so https://bintray.com/emccode/mesos-module-dvdi/download_file?file_path=unstable%2F0.4.4%2Flibmesos_dvdi_isolator-0.4.4%2B11%2Bmesos-0.28.2.so%2F%242"
chmod +x /tmp/libmesos_dvdi_isolator-$version.so
cp /tmp/libmesos_dvdi_isolator-$version.so /usr/lib/
ln -s /usr/lib/libmesos_dvdi_isolator-$version.so /usr/lib/libmesos_dvdi_isolator.so

# Start the instance of the Mesos agent for this single node test cluster
# Isolator must be installed and configured before starting the agent 
start mesos-slave

# trigger Marathon deployment of the test application
curl -i -H 'Content-Type: application/json' -d @marathontest.json localhost:8080/v2/apps

# Next steps for verifying operational status:
# Verify test application deployment status using Marathon UI (or Marathon REST API)
# Verify volume attachment using rexray volume list or Amazon UI
# Use Marathon to destroy the application
# verify volume unmount has occurred using rexray volume list or Amazon UI

# Troubleshooting
# use 'dvdcli mount --volumedriver=rexray --volumename-mesoscon' to verify the rexray and dvdcli configuration
# examine Mesos logs
# cat /var/log/syslog | grep marathon
# cat cat /var/log/mesos/mesos-slave.ERROR
# cat /var/log/mesos/mesos-slave.WARNING
# cat /var/log/mesos/mesos-slave.INFO
