// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "AnimationSequenceBrowserMenuContexts.generated.h"

class SAnimationSequenceBrowser;

UCLASS(BlueprintType)
class PERSONA_API UAnimationSequenceBrowserContextMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SAnimationSequenceBrowser> OwningAnimSequenceBrowser;

	UPROPERTY()
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;

	UFUNCTION(BlueprintCallable, Category = AnimationEditorExtensions)
	TArray<UObject*> GetSelectedObjects() const
	{
		TArray<UObject*> Result;
		Result.Reserve(SelectedObjects.Num());
		for (const TWeakObjectPtr<UObject>& Object : SelectedObjects)
		{
			Result.Add(Object.Get());
		}
		return Result;
	}
};
