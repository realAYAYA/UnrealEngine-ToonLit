// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorUtilityObject.h"
#include "UObject/ScriptMacros.h"
#include "BlutilityMenuExtensions.h"
#include "AssetActionUtility.generated.h"

/** 
 * Base class for all asset action-related utilities
 * Any functions/events that are exposed on derived classes that have the correct signature will be
 * included as menu options when right-clicking on a group of assets in the content browser.
 */
UCLASS(Abstract, hideCategories=(Object), Blueprintable)
class BLUTILITY_API UAssetActionUtility : public UEditorUtilityObject, public IEditorUtilityExtension
{
	GENERATED_BODY()

public:
	/** Return the class that this asset action supports (if not implemented, it will show up for all asset types) */
	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, Category="Assets")
	UClass* GetSupportedClass() const;

	/**
	 * Returns whether or not this action is designed to work specifically on Blueprints (true) or on all assets (false).
	 * If true, GetSupportedClass() is treated as a filter against the Parent Class of selected Blueprint assets
	 */
	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, Category = "Assets")
	bool IsActionForBlueprints() const;
};