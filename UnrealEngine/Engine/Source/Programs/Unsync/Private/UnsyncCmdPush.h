// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncCore.h"
#include "UnsyncRemote.h"

namespace unsync {

struct FCmdPushOptions
{
	FPath		Input;
	FRemoteDesc Remote;
};

int32 CmdPush(const FCmdPushOptions& Options);

}  // namespace unsync
