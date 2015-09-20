#!/bin/bash -e

set -o errexit -o nounset -o pipefail

DEMO_DIR=`dirname $0`
PROJECT_DIR=$DEMO_DIR/..

echo "Launching cluster with dvdi isolation modules enabled..."

pushd $PROJECT_DIR
docker-compose up -d
docker-compose scale slave=2
popd

echo ""
echo "Done."
