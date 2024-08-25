// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Base class of any editor-only objects
 */

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "IAssetRegistryTagProviderInterface.h"

#include "EditorUtilityObject.generated.h"


UCLASS(Abstract, Blueprintable, meta = (ShowWorldContextPin))
class BLUTILITY_API UEditorUtilityObject : public UObject, public IAssetRegistryTagProviderInterface
{
	GENERATED_UCLASS_BODY()

	// Standard function to execute
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = "Editor")
	void Run();

	//~ Begin IAssetRegistryTagProviderInterface interface
	virtual bool ShouldAddCDOTagsToBlueprintClass() const override 
	{ 
		return true; 
	}
	//~ End IAssetRegistryTagProviderInterface interface
};
