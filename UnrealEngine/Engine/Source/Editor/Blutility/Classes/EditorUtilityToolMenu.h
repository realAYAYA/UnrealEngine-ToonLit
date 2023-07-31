// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ScriptMacros.h"
#include "ToolMenuEntryScript.h"
#include "ToolMenuSection.h"
#include "EditorUtilityToolMenu.generated.h"


UCLASS(Blueprintable, abstract)
class BLUTILITY_API UEditorUtilityToolMenuEntry : public UToolMenuEntryScript
{
	GENERATED_BODY()
};

UCLASS(Blueprintable, abstract)
class BLUTILITY_API UEditorUtilityToolMenuSection : public UToolMenuSectionDynamic
{
	GENERATED_BODY()
};

