// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncCore.h"

namespace unsync {

struct FCmdPatchOptions
{
	FPath Base;
	FPath Patch;
	FPath Output;
};

int32 CmdPatch(const FCmdPatchOptions& Options);

}  // namespace unsync
