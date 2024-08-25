// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/IrisConfig.h"
#include "Iris/Core/IrisCsv.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace UE::Net
{

static int32 CVarUseIrisReplication = 0;
static FAutoConsoleVariableRef CVarUseIrisReplicationRef(TEXT("net.Iris.UseIrisReplication"), CVarUseIrisReplication, TEXT("Enables Iris replication system. 0 will fallback to legacy replicationsystem."), ECVF_Default );

bool ShouldUseIrisReplication()
{
	return CVarUseIrisReplication > 0;
}

void SetUseIrisReplication(bool EnableIrisReplication)
{
	CVarUseIrisReplication = EnableIrisReplication ? 1 : 0;
}

EReplicationSystem GetUseIrisReplicationCmdlineValue()
{
	int32 UseIrisReplication=0;
	if (FParse::Value(FCommandLine::Get(), TEXT("UseIrisReplication="), UseIrisReplication))
	{
		// Try to force the requested system if the cmdline is present
		return UseIrisReplication > 0 ? EReplicationSystem::Iris : EReplicationSystem::Generic;
	}

	// Use the default engine value
	return EReplicationSystem::Default;
}

}


// Enable Iris category by default on servers
CSV_DEFINE_CATEGORY(Iris, WITH_SERVER_CODE);