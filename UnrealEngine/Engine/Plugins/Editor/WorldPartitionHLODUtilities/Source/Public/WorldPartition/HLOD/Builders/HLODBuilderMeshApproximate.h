// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/HLODBuilder.h"
#include "Engine/MeshMerging.h"

#include "HLODBuilderMeshApproximate.generated.h"

class UMaterialInterface;

class UMaterial;


UCLASS(Blueprintable, Config = Engine, PerObjectConfig)
class WORLDPARTITIONHLODUTILITIES_API UHLODBuilderMeshApproximateSettings : public UHLODBuilderSettings
{
	GENERATED_UCLASS_BODY()

	virtual uint32 GetCRC() const override;

	/** Mesh approximation settings */
	UPROPERTY(EditAnywhere, Config, Category = HLOD, meta=(EditInline))
	FMeshApproximationSettings MeshApproximationSettings;

	/** Material that will be used by the generated HLOD static mesh */
	UPROPERTY(EditAnywhere, Config, AdvancedDisplay, Category = HLOD, meta = (DisplayName = "HLOD Material"))
	TSoftObjectPtr<UMaterialInterface> HLODMaterial;
};


/**
 * Build an approximated mesh using geometry from the provided actors
 */
UCLASS(HideDropdown)
class WORLDPARTITIONHLODUTILITIES_API UHLODBuilderMeshApproximate : public UHLODBuilder
{
	GENERATED_UCLASS_BODY()

public:
	virtual TSubclassOf<UHLODBuilderSettings> GetSettingsClass() const override;
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const override;
};
