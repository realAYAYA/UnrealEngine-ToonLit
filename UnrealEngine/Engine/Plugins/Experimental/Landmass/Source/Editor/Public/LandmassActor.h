// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/GCObject.h"
#include "Tickable.h"
#include "GameFramework/Actor.h"
#include "LandmassActor.generated.h"


UCLASS(Blueprintable, hidecategories = (Replication, Input, LOD, Actor, Cooking, Rendering))
class ALandmassActor : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "Tick")
	void CustomTick(float DeltaSeconds);

	virtual bool IsEditorOnly() const override { return true; }

	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, category = "Default")
	void SetEditorTickEnabled(bool bEnabled) { EditorTickIsEnabled = bEnabled; }

	UPROPERTY()
	bool EditorTickIsEnabled = false;

	UFUNCTION(BlueprintNativeEvent, CallInEditor, BlueprintCallable, Category = "Selection")
	void ActorSelectionChanged(bool bSelected);

private:
	bool bWasSelected = false;

	FDelegateHandle OnActorSelectionChangedHandle;

	/** Called when the editor selection has changed. */
	void HandleActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);

};