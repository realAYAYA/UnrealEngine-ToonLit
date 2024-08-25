// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

class UObjectReachabilityStressData : public UObject
{
	DECLARE_CLASS_INTRINSIC(UObjectReachabilityStressData,
		UObject,
		CLASS_Transient,
		TEXT("/Script/CoreUObject"));

public:
	TArray<UObjectReachabilityStressData*> Children;
};

void GenerateReachabilityStressData(TArray<UObjectReachabilityStressData*>& Data);
void UnlinkReachabilityStressData(TArray<UObjectReachabilityStressData*>& Data);
