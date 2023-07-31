// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/Actor.h"

#include "LevelVariantSetsActor.generated.h"

class UBlueprintGeneratedClass;
class ULevelVariantSets;
class ULevelVariantSetsFunctionDirector;
class UVariantSet;

UCLASS(hideCategories=(Rendering, Physics, HLOD, Activation, Input, Actor, Cooking))
class VARIANTMANAGERCONTENT_API ALevelVariantSetsActor : public AActor
{
public:

	GENERATED_BODY()

	ALevelVariantSetsActor(const FObjectInitializer& Init);

	// Non-const so that it doesn't show as pure in blueprints,
	// since it might trigger a load
	UFUNCTION(BlueprintCallable, Category="LevelVariantSets", meta=(ToolTip="Returns the LevelVariantSets asset, optionally loading it if necessary"))
	ULevelVariantSets* GetLevelVariantSets(bool bLoad = false);

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	void SetLevelVariantSets(ULevelVariantSets* InVariantSets);

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	bool SwitchOnVariantByName(FString VariantSetName, FString VariantName);

	UFUNCTION(BlueprintCallable, Category="LevelVariantSets")
	bool SwitchOnVariantByIndex(int32 VariantSetIndex, int32 VariantIndex);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="LevelVariantSets", meta=(AllowedClasses="/Script/VariantManagerContent.LevelVariantSets"))
	FSoftObjectPath LevelVariantSets;

private:

	friend class ULevelVariantSets;

	UPROPERTY(Transient)
	TMap<TObjectPtr<UBlueprintGeneratedClass>, TObjectPtr<ULevelVariantSetsFunctionDirector>> DirectorInstances;
};
