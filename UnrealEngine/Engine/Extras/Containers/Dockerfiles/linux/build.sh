#!/usr/bin/env bash

# Determine which release of the Unreal Engine we will be building container images for
UNREAL_ENGINE_RELEASE="5.0"
if [[ ! -z "$1" ]]; then
	UNREAL_ENGINE_RELEASE="$1"
fi

# Determine which Git branch/tag we will be pulling the Unreal Engine source code from
# (If the release value contains a dash or does not contain two dots then we treat it as a branch/tag name)
GIT_BRANCH="${UNREAL_ENGINE_RELEASE}-release"
dots=$(echo "${UNREAL_ENGINE_RELEASE}" | grep -o '[.]' | wc -l)
if [[ "${UNREAL_ENGINE_RELEASE}" == *"-"* ]] || [[ "${dots}" != "2" ]]; then
	GIT_BRANCH="${UNREAL_ENGINE_RELEASE}"
fi

# If the user specified a custom Git repository to clone from then use that
GIT_REPO=https://github.com/EpicGames/UnrealEngine.git
if [[ ! -z "$2" ]]; then
	GIT_REPO="$2"
fi

# Determine whether the user specified a changelist number to set in Build.version
CHANGELIST_OVERRIDE=""
if [[ ! -z "$3" ]]; then
	CHANGELIST_OVERRIDE="$3"
fi

# If the user specified a git commit hash instead of a release, branch or tag then automatically determine the changelist
# number from the commit message, and tag the built container images with an abbreviated version of the commit hash
COMMIT_HASH=$(echo "${UNREAL_ENGINE_RELEASE}" | grep -E -o '^[0-9a-f]{40}$')
COMMIT_SHORT=$(echo "${COMMIT_HASH}" | grep -E -o '^[0-9a-f]{8}')
if [[ ! -z "$COMMIT_HASH" ]]; then
	UNREAL_ENGINE_RELEASE="$COMMIT_SHORT"
	CHANGELIST_OVERRIDE="auto"
fi

# Verify that the user has placed their GitHub username in the file `username.txt`
if [ ! -f ./username.txt ]; then
	echo 'Please place your GitHub username in a text file called `username.txt` in the current directory.'
	exit 1
fi

# Verify that the user has placed their GitHub personal access token in the file `password.txt`
if [ ! -f ./password.txt ]; then
	echo 'Please place your GitHub personal access token in a text file called `password.txt` in the current directory.'
	echo 'Note that you should NOT use your GitHub password. See the following page for details on why a token is needed:'
	echo 'https://github.blog/2020-12-15-token-authentication-requirements-for-git-operations/'
	exit 1
fi

# Assemble the common arguments to pass to the `docker buildx build` command
args=(
	--build-arg 'BASEIMAGE=nvidia/opengl:1.0-glvnd-devel-ubuntu18.04'
	--build-arg "GIT_REPO=${GIT_REPO}"
	--build-arg "GIT_BRANCH=${GIT_BRANCH}"
	--build-arg "CHANGELIST=${CHANGELIST_OVERRIDE}"
	--secret id=username,src=username.txt
	--secret id=password,src=password.txt
	--platform linux/amd64
	--progress=plain
)

# Print our build configuration values
echo 'Build configuration'
echo '-------------------'
echo
echo "Unreal Engine Release: ${UNREAL_ENGINE_RELEASE}"
echo "Git Repository:        ${GIT_REPO}"
echo "Git Branch/Tag/Commit: ${GIT_BRANCH}"
echo "Changelist Override:   ${CHANGELIST_OVERRIDE}"
echo

# Print commands as they are executed and halt immediately if any command fails
set -ex

# Build our runtime container images
docker build -t 'ghcr.io/epicgames/unreal-engine:runtime' ./runtime
docker build -t 'ghcr.io/epicgames/unreal-engine:runtime-pixel-streaming' ./runtime-pixel-streaming

# Build a container image that encapsulates a full Installed Build of the Unreal Engine
echo '[build.sh] Building the `unreal-engine` image for Unreal Engine release' "${UNREAL_ENGINE_RELEASE}..."
docker buildx build \
	-t "ghcr.io/epicgames/unreal-engine:dev-${UNREAL_ENGINE_RELEASE}" \
	"${args[@]}" \
	./dev

# Build a "slim" version of the container image that excludes debug symbols and templates
echo '[build.sh] Building the `unreal-engine` image for Unreal Engine release' "${UNREAL_ENGINE_RELEASE}..."
docker buildx build \
	-t "ghcr.io/epicgames/unreal-engine:dev-slim-${UNREAL_ENGINE_RELEASE}" \
	"${args[@]}" \
	./dev-slim

# Build the container image for the Multi-User Editing server, which pulls files from our other images
docker build \
	-t "ghcr.io/epicgames/multi-user-server:${UNREAL_ENGINE_RELEASE}" \
	--build-arg "UNREAL_ENGINE_RELEASE=${UNREAL_ENGINE_RELEASE}" \
	./multi-user-server
