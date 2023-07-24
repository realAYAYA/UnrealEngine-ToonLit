// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MemoryUsageQueriesConfig.generated.h"

// “MemQuery Savings” command is supposed to list potential memory savings for assets based on predefined presets. 
// You can define them in DefaultMemoryUsageQueries.ini in your project – you have to provide preset name and UClass.
// "MemQuery Savings" command should list all derived classes with potential savings for the specified preset.
// 
// e.g.
// [/Script/MemoryUsageQueries.MemoryUsageQueriesConfig]
// +SavingsPresets = (("Pawns", "/Script/OurProject.OurCharacter"))

UCLASS(config = MemoryUsageQueries)
class UMemoryUsageQueriesConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(config)
	TMap<FString, FString> SavingsPresets;
};