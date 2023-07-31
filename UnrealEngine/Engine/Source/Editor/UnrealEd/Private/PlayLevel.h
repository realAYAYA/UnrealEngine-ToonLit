// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Containers/Array.h"

class UBlueprint;
class ULevelEditorPlaySettings;

struct FInternalPlayLevelUtils
{
	static int32 ResolveDirtyBlueprints(const bool bPromptForCompile, TArray<UBlueprint*>& ErroredBlueprints, const bool bForceLevelScriptRecompile = true);
};

DECLARE_LOG_CATEGORY_EXTERN(LogPlayLevel, Log, All);