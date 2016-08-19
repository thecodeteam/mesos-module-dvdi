#!/bin/bash
set -e

# $VER and $TRAVIS_BRANCH are assumed to be set (by travis) when this script is called
# this is expected to be invoked like this: eval $(./set-travis-env.sh)

GIT_DESC=$(git describe --tags)

REL_MAJOR=$( echo "$GIT_DESC" | sed -rn 's/[^[:digit:]]*([0-9]*).*$/\1/p' )
REL_MINOR=$( echo "$GIT_DESC" | sed -rn 's/[^[:digit:]]*[0-9]*.([0-9]*).*$/\1/p' )
REL_PATCH=$( echo "$GIT_DESC" | sed -rn 's/[^[:digit:]]*[0-9]*.[0-9]*.([0-9]*).*$/\1/p' )
REL_BUILD=$( echo "$GIT_DESC" | sed -rn 's/[^[:digit:]]*[0-9]*.[0-9].[0-9]*-([0-9]*).*$/\1/p' )

# XTEMP=$(git branch | grep '*')
# export V_BRANCH=$( echo "$XTEMP" | sed -rn 's/\*\s(.+)$/\1/p' )
V_SHA_LONG=$(git show HEAD -s --format=%H)
V_EPOCH=$(date +%s)
V_RELEASE_DATE=$(date +"%Y-%m-%d")

V_SEMVER=${REL_MAJOR}.${REL_MINOR}.${REL_PATCH}
SEMVER=${V_SEMVER}+${REL_BUILD}
SO_FILENAME="libmesos_dvdi_isolator-${SEMVER}+mesos-${VER}.so"

sed -e 's/${SEMVER}/'"$V_SEMVER/g" \
    -e 's|${DSCRIP}|'"$V_SEMVER.Branch.$TRAVIS_BRANCH.Sha.$V_SHA_LONG|g" \
    -e 's/${RELDTE}/'"$V_RELEASE_DATE/g" \
    bintray-unstable.json > bintray-unstable-filtered.json

echo "export REL_MAJOR=$REL_MAJOR"
echo "export REL_MINOR=$REL_MINOR"
echo "export REL_PATCH=$REL_PATCH"
echo "export REL_BUILD=$REL_BUILD"
echo "export V_BRANCH=$V_BRANCH"
echo "export V_SHA_LONG=$V_SHA_LONG"
echo "export V_EPOCH=$V_EPOCH"
echo "export V_RELEASE_DATE=$V_RELEASE_DATE"
echo "export V_SEMVER=$V_SEMVER"
echo "export SEMVER=$SEMVER"
echo "export SO_FILENAME=$SO_FILENAME"