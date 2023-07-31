// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"

#include "EngineDefines.h"
#include "Engine/MeshMerging.h"

#include "WorldPartition/HLOD/HLODBuilder.h"

#include "HLODLayer.generated.h"

class AActor;
class UMaterial;
class UWorldPartition;
class FWorldPartitionActorDesc;
class FWorldPartitionActorDescView;

UENUM()
enum class EHLODLayerType : uint8
{
	Instancing				UMETA(DisplayName = "Instancing"),
	MeshMerge				UMETA(DisplayName = "Merged Mesh"),
	MeshSimplify			UMETA(DisplayName = "Simplified Mesh"),
	MeshApproximate			UMETA(DisplayName = "Approximated Mesh"),
	Custom					UMETA(DisplayName = "Custom"),
};

UCLASS(Blueprintable, Config=Engine, PerObjectConfig)
class ENGINE_API UHLODLayer : public UObject
{
	GENERATED_UCLASS_BODY()
	
#if WITH_EDITOR
public:
	static UHLODLayer* GetHLODLayer(const AActor* InActor);
	static UHLODLayer* GetHLODLayer(const FWorldPartitionActorDesc& InActorDesc, const UWorldPartition* InWorldPartition);
	static UHLODLayer* GetHLODLayer(const FWorldPartitionActorDescView& InActorDescView, const UWorldPartition* InWorldPartition);

	/** Get the default engine HLOD layers setup */
	static UHLODLayer* GetEngineDefaultHLODLayersSetup();

	/** Duplicate the provided HLOD layers setup */
	static UHLODLayer* DuplicateHLODLayersSetup(UHLODLayer* HLODLayer, const FString& DestinationPath, const FString& Prefix);

	EHLODLayerType GetLayerType() const { return LayerType; }
	void SetLayerType(EHLODLayerType InLayerType) { LayerType = InLayerType; }
	const TSubclassOf<UHLODBuilder> GetHLODBuilderClass() const { return HLODBuilderClass; }
	const UHLODBuilderSettings* GetHLODBuilderSettings() const { return HLODBuilderSettings; }
	FName GetRuntimeGrid(uint32 InHLODLevel) const;
	int32 GetCellSize() const { return !bIsSpatiallyLoaded ? 0 : CellSize; }
	double GetLoadingRange() const { return !bIsSpatiallyLoaded ? WORLD_MAX : LoadingRange; }
	const TSoftObjectPtr<UHLODLayer>& GetParentLayer() const;
	const void SetParentLayer(const TSoftObjectPtr<UHLODLayer>& InParentLayer);
	bool IsSpatiallyLoaded() const { return bIsSpatiallyLoaded; }
	void SetIsSpatiallyLoaded(bool bInIsSpatiallyLoaded) { bIsSpatiallyLoaded = bInIsSpatiallyLoaded; }

	bool DoesRequireWarmup() const;

	static FName GetRuntimeGridName(uint32 InLODLevel, int32 InCellSize, double InLoadingRange);

private:
	//~ Begin UObject Interface.
	virtual void PostLoad() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.
#endif

private:
	/** Type of HLOD generation to use */
	UPROPERTY(EditAnywhere, Config, Category=HLOD)
	EHLODLayerType LayerType;

	/** HLODBuilder class */
	UPROPERTY(EditAnywhere, Config, Category=HLOD, meta = (DisplayName = "HLOD Builder Class", EditConditionHides, EditCondition = "LayerType == EHLODLayerType::Custom"))
	TSubclassOf<UHLODBuilder> HLODBuilderClass;

	UPROPERTY(VisibleAnywhere, Export, NoClear, Category=HLOD, meta = (EditInline, NoResetToDefault))
	TObjectPtr<UHLODBuilderSettings> HLODBuilderSettings;

	/** Whether HLOD actors generated for this layer will be spatially loaded */
	UPROPERTY(EditAnywhere, Config, Category=HLOD)
	uint32 bIsSpatiallyLoaded : 1;

	/** Cell size of the runtime grid created to encompass HLOD actors generated for this HLOD Layer */
	UPROPERTY(EditAnywhere, Config, Category=HLOD, meta = (EditConditionHides, EditCondition = "bIsSpatiallyLoaded"))
	int32 CellSize;

	/** Loading range of the runtime grid created to encompass HLOD actors generated for this HLOD Layer */
	UPROPERTY(EditAnywhere, Config, Category=HLOD, meta = (EditConditionHides, EditCondition = "bIsSpatiallyLoaded"))
	double LoadingRange;

	/** HLOD Layer to assign to the generated HLOD actors */
	UPROPERTY(EditAnywhere, Config, Category=HLOD, meta = (EditConditionHides, EditCondition = "bIsSpatiallyLoaded"))
	TSoftObjectPtr<UHLODLayer> ParentLayer;

private:
	friend class FWorldPartitionHLODUtilities;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FMeshMergingSettings MeshMergeSettings_DEPRECATED;
	UPROPERTY()
	FMeshProxySettings MeshSimplifySettings_DEPRECATED;
	UPROPERTY()
	FMeshApproximationSettings MeshApproximationSettings_DEPRECATED;
	UPROPERTY()
	TSoftObjectPtr<UMaterialInterface> HLODMaterial_DEPRECATED;
	UPROPERTY()
	uint32 bAlwaysLoaded_DEPRECATED : 1;
#endif
};
