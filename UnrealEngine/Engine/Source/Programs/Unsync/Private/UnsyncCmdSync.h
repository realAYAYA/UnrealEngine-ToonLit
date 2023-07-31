// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncCore.h"
#include "UnsyncRemote.h"

namespace unsync {

struct FCmdSyncOptions
{
	FAlgorithmOptions Algorithm;

	FPath Source;
	FPath Target;
	FPath SourceManifestOverride;

	std::vector<FPath> Overlays;

	FRemoteDesc Remote;

	bool bFullDifference = false;
	bool bFullSourceScan = false;
	bool bCleanup		 = false;

	bool bValidateTargetFiles = true;  // WARNING: turning this off is intended only for testing/profiling

	uint32 BlockSize = uint32(64_KB);

	FSyncFilter* Filter = nullptr;
};

int32 CmdSync(const FCmdSyncOptions& Options);

}  // namespace unsync
