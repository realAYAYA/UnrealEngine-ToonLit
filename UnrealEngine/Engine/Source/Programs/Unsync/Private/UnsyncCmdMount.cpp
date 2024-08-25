// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdMount.h"
#include "UnsyncMount.h"

namespace unsync {

int32
CmdMount(const FCmdMountOptions& Options)
{
	FMountedDirectory MountedDir;

	bool bOk = MountedDir.Mount(Options.Path);

	return bOk ? 0 : 1;
}

}  // namespace unsync
