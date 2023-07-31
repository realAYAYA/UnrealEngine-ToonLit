// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorUtilityObject.h"
#include "UObject/ScriptMacros.h"
#include "BlutilityMenuExtensions.h"
#include "ActorActionUtility.generated.h"

/** 
 * Base class for all actor action-related utilities
 * Any functions/events that are exposed on derived classes that have the correct signature will be
 * included as menu options when right-clicking on a group of actors in the level editor.
 */
UCLASS(Abstract, hideCategories=(Object), Blueprintable)
class BLUTILITY_API UActorActionUtility : public UEditorUtilityObject, public IEditorUtilityExtension
{
	GENERATED_BODY()

public:
	/** Return the class that this actor action supports. Leave this blank to support all actor classes. */
	UFUNCTION(BlueprintPure, BlueprintImplementableEvent, Category="Assets")
	UClass* GetSupportedClass() const;
};