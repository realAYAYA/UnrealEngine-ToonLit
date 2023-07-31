// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCommon.h"
#include "UnsyncRemote.h"

#include <string>

namespace unsync {

struct FCmdQueryOptions
{
	std::string Query;
	FRemoteDesc Remote;
};

int32 CmdQuery(const FCmdQueryOptions& Options);

} // namespace unsync
