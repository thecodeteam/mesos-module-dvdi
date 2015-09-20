# -*- mode: ruby -*-
# vi: set ft=ruby :

VAGRANTFILE_API_VERSION = "2"

Vagrant.configure(VAGRANTFILE_API_VERSION) do |config|

  # Every Vagrant virtual environment requires a box to build off of.
  config.vm.box     = "ubuntu/trusty64"

  # Mesos and Marathon UI ports.
  config.vm.network "forwarded_port", guest: 5050, host: 5050
  config.vm.network "forwarded_port", guest: 8080, host: 8080

  # Visualizer tasks use these ports.
  config.vm.network "forwarded_port", guest: 9001, host: 9001
  config.vm.network "forwarded_port", guest: 9002, host: 9002

  # This port is for the "before" part of the demo.  Docker will map it to
  # slave1 port 9001, where the collector starts in the "before" demo.
  config.vm.network "forwarded_port", guest: 9003, host: 9003

  # Create a private network, which allows host-only access to the machine
  # using a specific IP.
  config.vm.network "private_network", ip: "10.141.141.10"


  # If true, then any SSH connections made will enable agent forwarding.
  # Default value: false
  config.ssh.forward_agent = true

  # Share an additional folder to the guest VM. The first argument is
  # the path on the host to the actual folder. The second argument is
  # the path on the guest to mount the folder. And the optional third
  # argument is a set of non-required options.
  config.vm.synced_folder ".", "/home/vagrant/docker-volume-driver-isolator"

  # Provider-specific configuration so you can fine-tune various
  # backing providers for Vagrant. These expose provider-specific options.
  config.vm.provider "virtualbox" do |vb|
    vb.customize ["modifyvm", :id, "--cpus", "4"]
    vb.customize ["modifyvm", :id, "--memory", "4096"]
    vb.customize ["modifyvm", :id, "--natnet1", "10.1.1.0/24"]
  end

  # Post-create configuration follows.

  $install_docker = <<SCRIPT
    echo Installing docker gpg key...
    wget -qO- https://get.docker.com/gpg | sudo apt-key add -

    echo Installing docker...
    wget -qO- https://get.docker.com/ | sh

    echo Checking whether 'docker ps' works...
    docker ps

    echo Finished installing docker.
SCRIPT

  $install_docker_compose = <<SCRIPT
    echo Installing docker-compose...
    wget -qO- https://github.com/docker/compose/releases/download/1.3.3/docker-compose-`uname -s`-`uname -m` > /usr/local/bin/docker-compose

    echo Making docker-compose executable...
    chmod +x /usr/local/bin/docker-compose

    echo Checking whether 'docker-compose ps' works...
    docker-compose ps

    echo Finished installing docker-compose.
SCRIPT

  $install_demo_viz_forwarding = <<SCRIPT
    echo Setting up forwarding to demo viz
    iptables -A PREROUTING -t nat -i eth0 -p tcp --dport 9001 -j DNAT --to 192.168.255.253:9001
    iptables -A FORWARD -p tcp -d 192.168.255.253 --dport 9001 -j ACCEPT

    iptables -A PREROUTING -t nat -i eth0 -p tcp --dport 9002 -j DNAT --to 192.168.255.252:9001
    iptables -A FORWARD -p tcp -d 192.168.255.252 --dport 9001 -j ACCEPT

    echo Finished setting up port forwarding to demo viz
SCRIPT

  $install_kernel_modules = <<SCRIPT
    echo Setting up kernel modules
    modprobe ip6_tables
    echo "ip6_tables" >> /etc/modules
    modprobe xt_set
    echo "xt_set" >> /etc/modules

    echo Finished setting up kernel modules
SCRIPT

  config.vm.provision "shell", inline: $install_docker

  config.vm.provision "shell", inline: $install_docker_compose

  config.vm.provision "shell", inline: $install_demo_viz_forwarding

  config.vm.provision "shell", inline: $install_kernel_modules

  config.vm.provision "docker" do |d|
    d.pull_images "mesosphere/mesos-modules-dev:latest"
    d.pull_images "mesosphere/mesos-modules-dev-phusion:latest"
    d.pull_images "mesosphere/marathon:v0.9.1"
    d.pull_images "jplock/zookeeper:3.4.5"
    d.pull_images "spikecurtis/single-etcd"
  end

end
