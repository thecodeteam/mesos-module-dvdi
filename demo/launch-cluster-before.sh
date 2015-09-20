#!/bin/bash -e

set -o errexit -o nounset -o pipefail

DEMO_DIR=`dirname $0`
PROJECT_DIR=$DEMO_DIR/..

echo "Launching cluster with dvdi isolation modules disabled..."

pushd $DEMO_DIR/before
docker-compose -p dvdimodules up -d
docker-compose scale slave=2
popd

$DEMO_DIR/add-container-route.sh

echo ""
echo "Done."
