// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionActorDescView.h"

class UDataLayerInstance;
class UDataLayerInstanceWithAsset;

class ENGINE_API IStreamingGenerationErrorHandler
{
public:
	virtual ~IStreamingGenerationErrorHandler() {}

	/** 
	 * Called when an actor references an invalid actor.
	 */
	virtual void OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid) = 0;

	/** 
	 * Called when an actor references an actor using a different grid placement.
	 */
	virtual void OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) = 0;

	/** 
	 * Called when an actor references an actor using a different set of data layers.
	 */
	virtual void OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) = 0;

	/**
	 * Called when an actor references an actor using a different RuntimeGrid.
	 */
	virtual void OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) = 0;

	/** 
	 * Called when the level script references a streamed actor.
	 */
	virtual void OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView) = 0;

	/** 
	 * Called when an actor descriptor references an actor using data layers.
	 */
	virtual void OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView) = 0;

	/**
	 * Called when a data layer instance does not have a data layer asset
	 */
	virtual void OnInvalidReferenceDataLayerAsset(const UDataLayerInstanceWithAsset* DataLayerInstance) = 0;

	/**
	 * Called when a data layer is not of the same type as its parent
	 */
	virtual void OnDataLayerHierarchyTypeMismatch(const UDataLayerInstance* DataLayerInstance, const UDataLayerInstance* Parent) = 0;

	/**
	 * Called when two data layer instances share the same asset
	 */
	virtual void OnDataLayerAssetConflict(const UDataLayerInstanceWithAsset* DataLayerInstance, const UDataLayerInstanceWithAsset* ConflictingDataLayerInstance) = 0;

	/**
	 * Called when an actor needs to be resaved.
	 */
	virtual void OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView) = 0;

	/**
	 * Called when a level instance actor has errrors
	 */
	enum class ELevelInstanceInvalidReason
	{
		WorldAssetNotFound,
		WorldAssetNotUsingExternalActors,
		WorldAssetImcompatiblePartitioned
	};

	virtual void OnLevelInstanceInvalidWorldAsset(const FWorldPartitionActorDescView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason) = 0;

	// Helpers
	static FString GetFullActorName(const FWorldPartitionActorDescView& ActorDescView);
};
#endif
