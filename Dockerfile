FROM mesosphere/mesos-modules-dev-phusion
MAINTAINER Steve Wong <steven.wong@emc.com>

####################
# Mesos-DNS
####################
RUN wget https://dl.dropboxusercontent.com/u/4550074/mesos/mesos-dns+50fc45a9 -O /usr/bin/mesos-dns && \
    chmod +x /usr/bin/mesos-dns

###################
# Docker
###################
RUN apt-get update -qq && apt-get install -qqy \
    apt-transport-https \
    ca-certificates \
    curl \
    lxc \
    iptables

# Install Docker from Docker Inc. repositories.
RUN curl -sSL https://get.docker.com/ubuntu/ | sh

# Define additional metadata for our image.
VOLUME /var/lib/docker

###################
# RexRay DVD
###################
# TBD Install rexray DVD
# has legacy version for now
RUN curl -sSL https://dl.bintray.com/emccode/rexray/install | sh -

#######################
# Star (test workload)
#######################
WORKDIR /star

ADD http://downloads.mesosphere.io/demo/star/v0.5.0/star-collect-v0.5.0-linux-x86_64 /star/
RUN chmod +x star-collect-v0.5.0-linux-x86_64
ADD http://downloads.mesosphere.io/demo/star/v0.5.0/star-probe-v0.5.0-linux-x86_64 /star/
RUN chmod +x star-probe-v0.5.0-linux-x86_64

COPY ./demo/marathon/star-resources-before.json /star/star-resources-before.json
COPY ./demo/marathon/star-resources.json /star/star-resources.json
COPY ./demo/marathon/star-iso-resources.json /star/star-iso-resources.json

##################
# Sample Flask App
#################
RUN apt-get install -y python-pip libffi-dev
RUN pip install flask
COPY ./test/sampleflaskapp.tgz /mesos/sampleflaskapp.tgz

#################
# Init scripts
#################
ADD ./init_scripts/etc/service/mesos_slave/run /etc/service/mesos_slave/run
ADD ./init_scripts/etc/service/docker/run /etc/service/docker/run
ADD ./init_scripts/etc/service/mesos-dns/run /etc/service/mesos-dns/run
ADD ./init_scripts/etc/config/mesos-dns.json /etc/config/mesos-dns.json


####################
# Isolator
####################
WORKDIR /isolator
ADD ./isolator/ /isolator/

# Build the isolator.
# We need libmesos which is located in /usr/local/lib.
RUN ./bootstrap && \
    rm -rf build && \
    mkdir build && \
    cd build && \
    export LD_LIBRARY_PATH=LD_LIBRARY_PATH:/usr/local/lib && \
    ../configure --with-mesos=/usr/local --with-protobuf=/usr && \
    make all

  #####################
  # Docker Volume Drive Isolator
  ######################
  COPY ./dvdi/ /dvdi/
