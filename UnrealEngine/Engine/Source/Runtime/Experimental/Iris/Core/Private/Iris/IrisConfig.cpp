// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/IrisConfig.h"
#include "HAL/IConsoleManager.h"

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

}
