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

