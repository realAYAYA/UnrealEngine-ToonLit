#!/usr/bin/env bash


# Runs tests for the specified container image
function runTests {
	
	# Spin up a container using the specified container image
	docker run --name 'unrealtest' --rm -d "$1" bash -c 'sleep infinity'
	
	# Checkout the source code of a demo Unreal project in the container
	docker exec 'unrealtest' git clone --progress --depth=1 'https://gitlab.com/ue4-test-projects/4.26/firstpersoncxx.git' /home/ue4/project
	
	# Enable the WebBrowserWidget plugin, to ensure we test the ability to package projects that use CEF
	docker exec 'unrealtest' sed -i 's|"Modules"|"Plugins": [{"Name": "WebBrowserWidget", "Enabled": true}],\n\t"Modules"|' /home/ue4/project/FirstPersonCxx.uproject
	
	# Test that we can build the demo Unreal project in the container
	docker exec 'unrealtest' /home/ue4/UnrealEngine/Engine/Build/BatchFiles/Linux/Build.sh \
		FirstPersonCxxEditor Linux Development \
		-project=/home/ue4/project/FirstPersonCxx.uproject
	
	# Test that we can package the demo Unreal project in the container
	docker exec 'unrealtest' /home/ue4/UnrealEngine/Engine/Build/BatchFiles/RunUAT.sh \
		BuildCookRun \
		-clientconfig=Shipping -serverconfig=Shipping \
		-project=/home/ue4/project/FirstPersonCxx.uproject \
		-utf8output -nodebuginfo -allmaps -noP4 -cook -build -stage -prereqs -pak -archive \
		-archivedirectory=/home/ue4/project/dist \
		-platform=Linux
	
	# Copy the packaged binaries to the host system so the user can run the project if desired
	docker cp 'unrealtest:/home/ue4/project/dist' "`pwd`/$2"
	
	# Spin down the container and wait for it to be removed
	docker kill 'unrealtest'
	sleep 10
}


# Determine which release of the Unreal Engine we will be testing the built container images for
UNREAL_ENGINE_RELEASE="4.27"
if [[ ! -z "$1" ]]; then
	UNREAL_ENGINE_RELEASE="$1"
fi

# Print commands as they are executed and halt immediately if any command fails
set -ex

# Run tests using both the regular and slim variants of the development container image
runTests "ghcr.io/epicgames/unreal-engine:dev-${UNREAL_ENGINE_RELEASE}" 'dist-dev'
runTests "ghcr.io/epicgames/unreal-engine:dev-slim-${UNREAL_ENGINE_RELEASE}" 'dist-slim'
