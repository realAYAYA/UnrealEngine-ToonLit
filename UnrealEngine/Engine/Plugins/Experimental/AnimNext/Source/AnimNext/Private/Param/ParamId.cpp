// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/ParamId.h"
#include "Misc/ScopeRWLock.h"
#include "HAL/ThreadSingleton.h"

namespace UE::AnimNext
{

static FRWLock GParamIdLock;
static TArray<FName> GParamIdToName;
static TMap<FName, uint32> GNameToParamId;
#if WITH_DEV_AUTOMATION_TESTS
static bool bGTestSandbox = false;
struct FSandboxThreadData : TThreadSingleton<FSandboxThreadData>
{
	TArray<FName> ParamIdToName;
	TMap<FName, uint32> NameToParamId;
};
#endif

FParamId::FParamId(FName InName)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bGTestSandbox)
	{
		if (const uint32* FoundIndex = FSandboxThreadData::Get().NameToParamId.Find(InName))
		{
			ParameterIndex = *FoundIndex;
		}
		else
		{
			ParameterIndex = FSandboxThreadData::Get().ParamIdToName.Num();
			FSandboxThreadData::Get().NameToParamId.Add(InName, ParameterIndex);
			FSandboxThreadData::Get().ParamIdToName.Add(InName);
		}
	}
	else
#endif
	{
		FRWScopeLock Lock(GParamIdLock, SLT_Write);
		if (const uint32* FoundIndex = GNameToParamId.Find(InName))
		{
			ParameterIndex = *FoundIndex;
		}
		else
		{
			ParameterIndex = GParamIdToName.Num();
			GNameToParamId.Add(InName, ParameterIndex);
			GParamIdToName.Add(InName);
		}
	}
}

FName FParamId::ToName() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bGTestSandbox)
	{
		return FSandboxThreadData::Get().ParamIdToName[ParameterIndex];
	}
	else
#endif
	{
		FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
		return GParamIdToName[ParameterIndex];
	}
}

FParamId FParamId::GetMaxParamId()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bGTestSandbox)
	{
		return FParamId((uint32)FSandboxThreadData::Get().ParamIdToName.Num());
	}
	else
#endif
	{
		FRWScopeLock Lock(GParamIdLock, SLT_ReadOnly);
		return FParamId((uint32)GParamIdToName.Num());
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void FParamId::BeginTestSandbox()
{
	bGTestSandbox = true;
	FSandboxThreadData::Get().NameToParamId.Empty();
	FSandboxThreadData::Get().ParamIdToName.Empty();
}

void FParamId::EndTestSandbox()
{
	FSandboxThreadData::Get().NameToParamId.Empty();
	FSandboxThreadData::Get().ParamIdToName.Empty();
	bGTestSandbox = false;
}
#endif

}