// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Blueprint for editor utilities
 */

#pragma once

#include "Engine/Blueprint.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "EditorUtilityBlueprint.generated.h"

class UObject;

UCLASS()
class BLUTILITY_API UEditorUtilityBlueprint : public UBlueprint
{
	GENERATED_UCLASS_BODY()

	// UBlueprint interface
	virtual bool SupportedByDefaultBlueprintFactory() const override;
	virtual bool AlwaysCompileOnLoad() const override;
	// End of UBlueprint interface
};
