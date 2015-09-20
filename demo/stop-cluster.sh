#!/bin/bash -e

set -o errexit -o nounset -o pipefail

DEMO_DIR=`dirname $0`
PROJECT_DIR=$DEMO_DIR/..

echo "Shutting down cluster..."

cd $PROJECT_DIR
docker-compose kill
docker-compose rm --force

echo ""
echo "Done."
