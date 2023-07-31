#!/bin/bash
# Copyright Epic Games, Inc. All Rights Reserved.

BASH_LOCATION=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

pushd "${BASH_LOCATION}" > /dev/null

print_help() {
 echo "
 Tool for fetching PixelStreaming Infrastructure. If no flags are set specifying a version to fetch, 
 the recommended version will be chosen as a default.

 Usage:
   ${0} [-h] [-v <UE version>] [-b <branch>] [-t <tag>]
 Where:
   -v      Specify a version of Unreal Engine to download the recommended 
           release for
   -b      Specify a specific branch for the tool to download from repo
   -t      Specify a specific tag for the tool to download from repo
   -h      Display this help message
"
 exit 1
}

while(($#)) ; do
  case "$1" in
   -h ) print_help;;
   -v ) UEVersion="$2"; shift 2;;
   -b ) PSInfraTagOrBranch="$2"; IsTag=0; shift 2;;
   -t ) PSInfraTagOrBranch="$2"; IsTag=1; shift 2;;
   * ) echo "Unknown command: $1"; shift;;
  esac
 done

# Name and version of ps-infra that we are downloading
PSInfraOrg=EpicGames
PSInfraRepo=PixelStreamingInfrastructure

# If a UE version is supplied set the right branch or tag to fetch for that version of UE
if [ ! -z "$UEVersion" ]
then
  if [ "$UEVersion" = "4.26" ]
  then
    PSInfraTagOrBranch=UE4.26
    IsTag=0
  fi
  if [ "$UEVersion" = "4.27" ]
  then
    PSInfraTagOrBranch=UE4.27
    IsTag=0
  fi
  if [ "$UEVersion" = "5.0" ]
  then
    PSInfraTagOrBranch=UE5.0
    IsTag=0
  fi
fi

# If no arguments select a specific version, fetch the appropriate default
if [ -z "$PSInfraTagOrBranch" ]
then
  PSInfraTagOrBranch=UE5.1
  IsTag=0
fi

# Whether the named reference is a tag or a branch affects the URL we fetch it on
if [ "$IsTag" -eq 1 ]
then
  RefType=tags
else
  RefType=heads
fi

# Look for a SignallingWebServer directory next to this script
if [ -d SignallingWebServer ]
then
  echo "SignallingWebServer directory found...skipping install."
else
  echo "SignallingWebServer directory not found...beginning ps-infra download."

  # Download ps-infra and follow redirects.
  curl -L https://github.com/$PSInfraOrg/$PSInfraRepo/archive/refs/$RefType/$PSInfraTagOrBranch.tar.gz > ps-infra.tar.gz

  # Unarchive the .tar
  tar -xmf ps-infra.tar.gz || $(echo "bad archive, contents:" && head --lines=20 ps-infra.tar.gz && exit 0)

  # Move the server folders into the current directory (WebServers) and delete the original directory
  mv PixelStreamingInfrastructure-*/* .
  # Copy any files and folders beginning with dot (ignored by * glob) and discard errors regarding to not being able to move "." and ".."
  mv PixelStreamingInfrastructure-*/.* . 2>/dev/null || :
  rm -rf PixelStreamingInfrastructure-*

  # Delete the downloaded tar
  rm ps-infra.tar.gz
fi