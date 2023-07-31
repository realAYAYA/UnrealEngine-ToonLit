// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "VCamModifier.h"

#include "VCamTypes.generated.h"

class UVCamModifier;

PRAGMA_DISABLE_OPTIMIZATION

// Links a Modifier with a Name for use in a Modifier Stack
USTRUCT()
struct FModifierStackEntry
{
	GENERATED_BODY()

	// Identifier for this modifier in the stack
    UPROPERTY(EditAnywhere, Category="Modifier")
    FName Name;

	// Controls whether the modifier actually gets applied
	UPROPERTY(EditAnywhere, Category="Modifier")
	bool bEnabled = true;

	// The current generated modifier instance
	UPROPERTY(EditAnywhere, Instanced, Category="Modifier")
    TObjectPtr<UVCamModifier> GeneratedModifier = nullptr;

#if WITH_EDITOR
	// GUID used in the editor to identify specific stack entries during editor operations
	FGuid EditorGuid;
#endif
	
	FModifierStackEntry()
	{
#if WITH_EDITOR
		EditorGuid = FGuid::NewGuid();
#endif
	}

	// If ModifierClass is provided then you must also supply a valid outer for the generated modifier
	FModifierStackEntry(const FName& InName, const TSubclassOf<UVCamModifier> InModifierClass, UObject* InOuter)
		: Name(InName)
	{
		if (InModifierClass)
		{
			GeneratedModifier = NewObject<UVCamModifier>(InOuter, InModifierClass.Get());
		}

#if WITH_EDITOR
		EditorGuid = FGuid::NewGuid();
#endif
	};

	~FModifierStackEntry()
	{
		// It's possible the Generated Modifier may pass IsValid while having already been destroyed so ensure we explicitly check for this case
		if (IsValid(GeneratedModifier) && !GeneratedModifier->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
		{
			GeneratedModifier->Deinitialize();
		}
	}

	bool operator==(const FModifierStackEntry& Other) const
	{
		bool bArePropertiesEqual = this->Name.IsEqual(Other.Name) && this->bEnabled == Other.bEnabled && this->GeneratedModifier == Other.GeneratedModifier;

#if WITH_EDITOR
		bArePropertiesEqual = bArePropertiesEqual && this->EditorGuid == Other.EditorGuid;
#endif

		return bArePropertiesEqual;
	};

	bool operator!=(const FModifierStackEntry& Other) const
	{
		return !(*this == Other);
	}
};

PRAGMA_ENABLE_OPTIMIZATION