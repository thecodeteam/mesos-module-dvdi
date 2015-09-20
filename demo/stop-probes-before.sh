#!/bin/bash -e

set -o errexit -o nounset -o pipefail

echo "Destroying group 'star-before'"

curl -X DELETE http://localhost:8080/v2/groups/star-before?force=true

echo ""
echo "Done."
