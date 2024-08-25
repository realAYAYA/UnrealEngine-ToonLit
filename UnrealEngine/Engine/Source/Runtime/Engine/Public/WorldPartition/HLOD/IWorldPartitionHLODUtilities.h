// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionStreamingGenerationContext.h"
#include "WorldPartition/DataLayer/DataLayersID.h"

#if WITH_EDITOR

#include "WorldPartition/DataLayer/ExternalDataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerInstance.h"

class AActor;
class UWorldPartition;
class UHLODLayer;
class UHLODBuilder;
class UHLODBuilderSettings;
class AWorldPartitionHLOD;

struct FHLODCreationContext
{
	TMap<FName, FWorldPartitionHandle> HLODActorDescs;
	TArray<FWorldPartitionReference> ActorReferences;
};

struct FHLODCreationParams
{
	UWorldPartition* WorldPartition;

	FGuid CellGuid;
	FString CellName;
	FBox CellBounds;
	TUniqueFunction<FName(const UHLODLayer*)> GetRuntimeGrid;
	uint32 HLODLevel;
	FGuid ContentBundleGuid;
	TArray<const UDataLayerInstance*> DataLayerInstances;

	const UExternalDataLayerAsset* GetExternalDataLayerAsset() const
	{
		auto IsAnExternalDataLayerPred = [](const UDataLayerInstance* DataLayerInstance) { return DataLayerInstance->IsA<UExternalDataLayerInstance>(); };
		if (const UDataLayerInstance* const* ExternalDataLayerInstance = DataLayerInstances.FindByPredicate(IsAnExternalDataLayerPred))
		{
			return CastChecked<UExternalDataLayerInstance>(*ExternalDataLayerInstance)->GetExternalDataLayerAsset();
		}
		return nullptr;
	}

	double MinVisibleDistance;
};

/**
 * Tools for building HLODs in WorldPartition
 */
class IWorldPartitionHLODUtilities
{
public:
	virtual ~IWorldPartitionHLODUtilities() {}

	/**
	 * Create HLOD actors for a given cell
	 *
	 * @param	InCreationContext	HLOD creation context object
	 * @param	InCreationParams	HLOD creation parameters object
	 * @param	InActors			The actors for which we'll build an HLOD representation
	 * @param	InDataLayers		The data layers to assign to the newly created HLOD actors
	 */
	virtual TArray<AWorldPartitionHLOD*> CreateHLODActors(FHLODCreationContext& InCreationContext, const FHLODCreationParams& InCreationParams, const TArray<IStreamingGenerationContext::FActorInstance>& InActors) = 0;

	/**
	 * Build HLOD for the specified AWorldPartitionHLOD actor.
	 *
	 * @param 	InHLODActor		The HLOD actor for which we'll build the HLOD
	 * @return An hash that represent the content used to build this HLOD.
	 */
	virtual uint32 BuildHLOD(AWorldPartitionHLOD* InHLODActor) = 0;

	/**
	 * Retrieve the HLOD Builder class to use for the given HLODLayer.
	 * 
	 * @param	InHLODLayer		HLODLayer
	 * @return The HLOD builder subclass to use for building HLODs for the provided HLOD layer.
	 */
	virtual TSubclassOf<UHLODBuilder> GetHLODBuilderClass(const UHLODLayer* InHLODLayer) = 0;

	/**
	 * Create the HLOD builder settings for the provided HLOD layer object. The type of settings created will depend on the HLOD layer type.
	 *
	 * @param 	InHLODLayer		The HLOD layer for which we'll create a setting object
	 * @return A newly created UHLODBuilderSettings object, outered to the provided HLOD layer.
	 */
	virtual UHLODBuilderSettings* CreateHLODBuilderSettings(UHLODLayer* InHLODLayer) = 0;


	UE_DEPRECATED(5.2, "Use the overload that passes the DataLayersInstances via InCreationParams")
	virtual TArray<AWorldPartitionHLOD*> CreateHLODActors(FHLODCreationContext& InCreationContext, const FHLODCreationParams& InCreationParams, const TArray<IStreamingGenerationContext::FActorInstance>& InActors, const TArray<const UDataLayerInstance*>& InDataLayerInstances)
	{
		const_cast<FHLODCreationParams&>(InCreationParams).DataLayerInstances = InDataLayerInstances;
		return CreateHLODActors(InCreationContext, InCreationParams, InActors);
	}
};
#endif
