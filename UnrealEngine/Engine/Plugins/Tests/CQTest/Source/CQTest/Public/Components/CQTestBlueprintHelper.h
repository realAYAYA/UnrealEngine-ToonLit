// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/Object.h"
#include "UObject/Class.h"

/// Utilities for working with Blueprint in C++ Tests
struct CQTEST_API FCQTestBlueprintHelper
{
	UObject* FindDataBlueprint(const FString& Directory, const FString& Name);
	UClass* GetBlueprintClass(const FString& Directory, const FString& Name);
};