// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/StatsMisc.h"

struct CONTROLRIG_API FControlRigStats
{
	bool Enabled;
	TArray<FName> Stack;
	TMap<FName, double> Counters;

	FControlRigStats()
	: Enabled(false)
	{

	}

	static FControlRigStats& Get();
	void Clear();
	double& RetainCounter(const TCHAR* Key);
	double& RetainCounter(const FName& Key);
	void ReleaseCounter();
	void Dump();
};

struct CONTROLRIG_API FControlRigSimpleScopeSecondsCounter : public FSimpleScopeSecondsCounter
{
public:
	FControlRigSimpleScopeSecondsCounter(const TCHAR* InName);
	~FControlRigSimpleScopeSecondsCounter();
};

#if STATS
#if WITH_EDITOR
#define CONTROLRIG_SCOPE_SECONDS_COUNTER(Name) \
	FControlRigSimpleScopeSecondsCounter ANONYMOUS_VARIABLE(ControlRigScopeSecondsCounter)(TEXT(#Name))
#else
#define CONTROLRIG_SCOPE_SECONDS_COUNTER(Name)
#endif
#endif
