DVDI_NODE_VERSION=v0.6.0
DOCKER_COMPOSE_URL=https://github.com/docker/compose/releases/download/1.4.0/docker-compose-`uname -s`-`uname -m`

default: images

docker-compose:
	  curl -L ${DOCKER_COMPOSE_URL} > docker-compose
	  chmod +x ./docker-compose

images: dvdi-node docker-compose
	  ./docker-compose build;

dvdi-node: dvdi/dvdi-node-$(dvdi_NODE_VERSION).tar

dvdi/dvdi-node-$(dvdi_NODE_VERSION).tar:
	docker pull dvdi/node:$(dvdi_NODE_VERSION)
	docker save -o dvdi/dvdi-node-$(dvdi_NODE_VERSION).tar dvdi/node:$(dvdi_NODE_VERSION)

st: images
	docker-compose kill
	docker-compose rm --force
	docker-compose pull
	test/run_compose_st.sh
