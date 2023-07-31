// Copyright Epic Games, Inc. All Rights Reserved.

#include "PartyModule.h"

IMPLEMENT_MODULE(FPartyModule, Party);

DEFINE_LOG_CATEGORY(LogParty);

#if STATS
DEFINE_STAT(STAT_PartyStat1);
#endif

void FPartyModule::StartupModule()
{	
}

void FPartyModule::ShutdownModule()
{
}

bool FPartyModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with Party
	if (FParse::Command(&Cmd, TEXT("Party")))
	{
		// this is where we can implement "Party XYZABC" commands
		return false;
	}

	return false;
}


