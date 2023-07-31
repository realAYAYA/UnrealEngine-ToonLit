// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/HLOD/HLODBuilder.h"
#include "Engine/MeshMerging.h"

#include "HLODBuilderMeshSimplify.generated.h"

class UMaterial;


UCLASS(Blueprintable, Config = Engine, PerObjectConfig)
class WORLDPARTITIONHLODUTILITIES_API UHLODBuilderMeshSimplifySettings : public UHLODBuilderSettings
{
	GENERATED_UCLASS_BODY()

	virtual uint32 GetCRC() const override;

	/** Simplified mesh generation settings */
	UPROPERTY(EditAnywhere, Config, Category = HLOD)
	FMeshProxySettings MeshSimplifySettings;

	/** Material that will be used by the generated HLOD static mesh */
	UPROPERTY(EditAnywhere, Config, AdvancedDisplay, Category = HLOD, meta = (DisplayName = "HLOD Material"))
	TSoftObjectPtr<UMaterialInterface> HLODMaterial;
};


/**
 * Build a simplified mesh using geometry from the provided actors
 */
UCLASS(HideDropdown)
class WORLDPARTITIONHLODUTILITIES_API UHLODBuilderMeshSimplify : public UHLODBuilder
{
	GENERATED_UCLASS_BODY()

public:
	virtual TSubclassOf<UHLODBuilderSettings> GetSettingsClass() const override;
	virtual TArray<UActorComponent*> Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const override;
};
