// Copyright Epic Games, Inc. All Rights Reserved.

#include "GarbageCollectionTesting.h"
#include "UObject/GarbageCollectionSchema.h"
#include "UObject/UnrealType.h"

/*
	TODO additional features for a future changeset:
	- gc.GenerateReachabilityStressData should have extra arg
	  to specify # of levels in UObject tree
	- gc.GenerateReachabilityStressData should have extra arg
	  to specify whether or not the tree is gc-rooted (to see
		how long it takes to collect)
	- UObjectReachabilityStressData can expose more members to
	  GC to make the token stream bigger
 */

static void GenerateReachabilityStressData(int Levels, UObjectReachabilityStressData* Data);

UObjectReachabilityStressData* GenerateReachabilityStressData()
{
	UObjectReachabilityStressData* Data = NewObject<UObjectReachabilityStressData>();
	GenerateReachabilityStressData(20, Data);
	return Data;
}

static void GenerateReachabilityStressData(int Levels, UObjectReachabilityStressData* Data)
{
	Data->AddToRoot();

	if (Levels == 0)
	{
		return;
	}

	const int N = 2;
	for (int I = 0; I < N; ++I)
	{
		UObjectReachabilityStressData* Child = NewObject<UObjectReachabilityStressData>();
		GenerateReachabilityStressData(Levels - 1, Child);
		Data->Children.Add(Child);
	}
}

void UnlinkReachabilityStressData(UObjectReachabilityStressData* Data)
{
	Data->RemoveFromRoot();
	for (const auto Child : Data->Children)
	{
		UnlinkReachabilityStressData(Child);
	}
}

IMPLEMENT_CORE_INTRINSIC_CLASS(UObjectReachabilityStressData, UObject,
	{
		UE::GC::DeclareIntrinsicMembers(Class, { UE_GC_MEMBER(UObjectReachabilityStressData, Children) });
	});
