// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"

namespace unsync {

struct FCmdMountOptions
{
	FPath Path;
};

int32 CmdMount(const FCmdMountOptions& Options);

}
