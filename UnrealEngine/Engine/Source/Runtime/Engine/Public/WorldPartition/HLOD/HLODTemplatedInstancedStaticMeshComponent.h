// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Templates/SubclassOf.h"
#include "HLODTemplatedInstancedStaticMeshComponent.generated.h"

class AActor;

UCLASS(Hidden, NotPlaceable, MinimalAPI)
class UHLODTemplatedInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_UCLASS_BODY()

public: 
	//~ Begin UObject Interface.
	ENGINE_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	ENGINE_API virtual void PostLoad() override;
	//~ End UObject Interface.
	
	ENGINE_API void SetTemplateActorClass(const TSubclassOf<AActor>& InTemplateActorClass);
	ENGINE_API void SetTemplateComponentName(const FName& InTemplateComponentName);
	
private:
	void RestoreAssetsFromActorTemplate();

private:
	UPROPERTY()
	TSubclassOf<AActor> TemplateActorClass;

	UPROPERTY()
	FName TemplateComponentName;
};
