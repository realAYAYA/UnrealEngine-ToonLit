#!/usr/bin/env bash


# Generates the Dockerfile for the specified container image
function generateDockerfile {
	
	# Create a temporary directory to hold our generated files before we move them to the actual output directory
	# (Note that this is necessary to ensure ue4-docker doesn't clobber our other Dockerfiles, since it truncates its target directory)
	tempDir=$(mktemp -d -t dockerfile-XXXXXXXXXX)
	
	# Assemble our arguments to pass to the `ue4-docker build` command
	args=(
		
		# Note that the release number here is actually irrelevant to the generated files, since the repository and branch used to clone the
		# Engine source code are defined by Docker build arguments that the user sets when building images with the generated Dockerfiles.
		# The need to specify a release number is purely an artifact of how the `ue4-docker build` command was originally designed prior to
		# the addition of functionality to generate Dockerfiles rather than always building container images.
		'5.0.0'
		
		# Ensure that we generate Dockerfiles for Linux container images even if this script is run on a Windows host system (e.g. in git bash)
		# (Note that although generating the Dockerfiles under Windows is fine, it is NOT recommended that the images actually be built under Windows)
		--linux
		
		# This will ensure our target directory exists and is empty, and will then copy the generated Dockerfiles to it
		-layout "$tempDir"
		
		# This will combine the generated Dockerfiles into a single Dockerfile that uses a multi-stage build
		--combine
		
		# This specifies the individual images whose Dockerfiles will be combined
		--target=build-prerequisites,source,minimal
		
		# This disables building the Engine for AArch64 when creating an Installed Build, and enables support for client and dedicated server targets
		--opt buildgraph-args='-set:WithLinuxArm64=false -set:WithClient=true -set:WithServer=true'
		
		# This enables the use of BuildKit build secrets, which is necessary in order to build images independently of ue4-docker itself
		--opt credential-mode=secrets
		
		# This strips out the Dockerfile code that ue4-docker would ordinarily generate that is uneccesary for Unreal Engine 4.26+
		--opt disable-all-patches
		
		# This strips out the image labels that are ordinarily used to facilitate cleanup and inspection of images built by ue4-docker
		--opt disable-labels
		
		# This enables the glibc DSO sorting patch, which significantly improves Editor startup times
		--opt enable-dso-patch
	)
	
	# Invoke ue4-docker to generate the Dockerfile
	ue4-docker build "${args[@]}" $2
	
	# Move the Dockerfile to the actual output directory
	outputDir="./$1"
	test -d "$outputDir" && rm -rf "$outputDir"
	mv "$tempDir/combined" "$outputDir"
	
	# Clean up the temporary directory
	rm -rf "$tempDir"
}


# Generate the `dev` and `dev-slim` Dockerfiles
generateDockerfile 'dev'
generateDockerfile 'dev-slim' '--exclude debug --exclude templates'
