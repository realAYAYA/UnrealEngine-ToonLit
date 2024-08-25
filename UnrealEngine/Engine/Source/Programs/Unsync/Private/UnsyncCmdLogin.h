// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCommon.h"
#include "UnsyncRemote.h"

namespace unsync {

struct FCmdLoginOptions
{
	FRemoteDesc Remote;
	bool		bInteractive	 = false;  // TODO
	bool		bPrint			 = false;
	bool		bPrintHttpHeader = false;
	bool		bDecode			 = false;
	bool		bForceRefresh	 = false;
	bool		bQuick			 = false;
};

int32 CmdLogin(const FCmdLoginOptions& Options);

}  // namespace unsync
