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
class AWorldPartitionHLOD;
class UHLODBuilder;
class UHLODBuilderSettings;
class UMaterial;
class UWorldPartition;
class UWorldPartitionHLODModifier;
class FWorldPartitionActorDesc;

UENUM()
enum class EHLODLayerType : uint8
{
	Instancing				UMETA(DisplayName = "Instancing"),
	MeshMerge				UMETA(DisplayName = "Merged Mesh"),
	MeshSimplify			UMETA(DisplayName = "Simplified Mesh"),
	MeshApproximate			UMETA(DisplayName = "Approximated Mesh"),
	Custom					UMETA(DisplayName = "Custom"),
};

UCLASS(Blueprintable, MinimalAPI)
class UHLODLayer : public UObject
{
	GENERATED_UCLASS_BODY()
	
#if WITH_EDITOR
public:
	/** Get the default engine HLOD layers setup */
	static ENGINE_API UHLODLayer* GetEngineDefaultHLODLayersSetup();

	/** Duplicate the provided HLOD layers setup */
	static ENGINE_API UHLODLayer* DuplicateHLODLayersSetup(UHLODLayer* HLODLayer, const FString& DestinationPath, const FString& Prefix);

	EHLODLayerType GetLayerType() const { return LayerType; }
	void SetLayerType(EHLODLayerType InLayerType) { LayerType = InLayerType; }
	const TSubclassOf<UHLODBuilder> GetHLODBuilderClass() const { return HLODBuilderClass; }
	const UHLODBuilderSettings* GetHLODBuilderSettings() const { return HLODBuilderSettings; }
	const TSubclassOf<AWorldPartitionHLOD> GetHLODActorClass() const { return HLODActorClass; }
	const TSubclassOf<UWorldPartitionHLODModifier> GetHLODModifierClass() const { return HLODModifierClass; }
	ENGINE_API FName GetRuntimeGrid(uint32 InHLODLevel) const;
	int32 GetCellSize() const { return !bIsSpatiallyLoaded ? 0 : CellSize; }
	double GetLoadingRange() const { return !bIsSpatiallyLoaded ? WORLD_MAX : LoadingRange; }
	UHLODLayer* GetParentLayer() const { return !bIsSpatiallyLoaded ? nullptr : ParentLayer; }
	void SetParentLayer(UHLODLayer* InParentLayer) { ParentLayer = InParentLayer; }
	bool IsSpatiallyLoaded() const { return bIsSpatiallyLoaded; }
	void SetIsSpatiallyLoaded(bool bInIsSpatiallyLoaded) { bIsSpatiallyLoaded = bInIsSpatiallyLoaded; }

	ENGINE_API bool DoesRequireWarmup() const;

	static ENGINE_API FName GetRuntimeGridName(uint32 InLODLevel, int32 InCellSize, double InLoadingRange);

private:
	//~ Begin UObject Interface.
	ENGINE_API virtual void PostLoad() override;
#if WITH_EDITORONLY_DATA
	static ENGINE_API void DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass);
#endif
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.
#endif

private:
	/** Type of HLOD generation to use */
	UPROPERTY(EditAnywhere, Category=HLOD)
	EHLODLayerType LayerType;

	/** HLOD Builder class */
	UPROPERTY(EditAnywhere, Category=HLOD, meta = (DisplayName = "HLOD Builder Class", EditConditionHides, EditCondition = "LayerType == EHLODLayerType::Custom"))
	TSubclassOf<UHLODBuilder> HLODBuilderClass;

	UPROPERTY(VisibleAnywhere, Export, NoClear, Category=HLOD, meta = (EditInline, NoResetToDefault))
	TObjectPtr<UHLODBuilderSettings> HLODBuilderSettings;

	/** Whether HLOD actors generated for this layer will be spatially loaded */
	UPROPERTY(EditAnywhere, Category=HLOD)
	uint32 bIsSpatiallyLoaded : 1;

	/** Cell size of the runtime grid created to encompass HLOD actors generated for this HLOD Layer */
	UPROPERTY(EditAnywhere, Category=HLOD, meta = (EditConditionHides, EditCondition = "bIsSpatiallyLoaded"))
	int32 CellSize;

	/** Loading range of the runtime grid created to encompass HLOD actors generated for this HLOD Layer */
	UPROPERTY(EditAnywhere, Category=HLOD, meta = (EditConditionHides, EditCondition = "bIsSpatiallyLoaded"))
	double LoadingRange;

	/** HLOD Layer to assign to the generated HLOD actors */
	UPROPERTY(EditAnywhere, Category=HLOD, meta = (EditConditionHides, EditCondition = "bIsSpatiallyLoaded"))
	TObjectPtr<UHLODLayer> ParentLayer;

	/** Specify a custom HLOD Actor class, the default is AWorldPartitionHLOD */
	UPROPERTY(EditAnywhere, Category = HLOD, AdvancedDisplay, meta = (DisplayName = "HLOD Actor Class"))
	TSubclassOf<AWorldPartitionHLOD> HLODActorClass;

	/** HLOD Modifier class, to allow changes to the HLOD at runtime */
	UPROPERTY(EditAnywhere, Category = HLOD, AdvancedDisplay, meta = (DisplayName = "HLOD Modifier Class"))
	TSubclassOf<UWorldPartitionHLODModifier> HLODModifierClass;

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
