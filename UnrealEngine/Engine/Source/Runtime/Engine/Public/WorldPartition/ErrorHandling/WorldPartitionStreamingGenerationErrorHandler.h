// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "WorldPartition/WorldPartitionActorDescInstanceViewInterface.h"

class UDataLayerAsset;
class UDataLayerInstance;
class UDataLayerInstanceWithAsset;

// deprecated
class FWorldPartitionActorDescView;

class IStreamingGenerationErrorHandler
{
public:
	virtual ~IStreamingGenerationErrorHandler() {}

	/**
	 * Called when an actor has an invalid runtime grid.
	 */
	virtual void OnInvalidRuntimeGrid(const IWorldPartitionActorDescInstanceView& ActorDescView, FName GridName) = 0;

	/**
	 * Called when an actor references an invalid actor.
	 */
	virtual void OnInvalidReference(const IWorldPartitionActorDescInstanceView& ActorDescView, const FGuid& ReferenceGuid, IWorldPartitionActorDescInstanceView* ReferenceActorDescView) = 0;

	/**
	 * Called when an actor references an actor using a different grid placement.
	 */
	virtual void OnInvalidReferenceGridPlacement(const IWorldPartitionActorDescInstanceView& ActorDescView, const IWorldPartitionActorDescInstanceView& ReferenceActorDescView) = 0;

	/**
	 * Called when there's an error with the data layers used by an actor and one or more referenced actor.
	 */
	enum class EDataLayerInvalidReason
	{
		ReferencedActorDifferentRuntimeDataLayers,
		ReferencedActorDifferentExternalDataLayer
	};

	virtual void OnInvalidReferenceDataLayers(const IWorldPartitionActorDescInstanceView& ActorDescView, const IWorldPartitionActorDescInstanceView& ReferenceActorDescView, EDataLayerInvalidReason Reason) = 0;

	/**
	 * Called when an actor references an actor using a different RuntimeGrid.
	 */
	virtual void OnInvalidReferenceRuntimeGrid(const IWorldPartitionActorDescInstanceView& ActorDescView, const IWorldPartitionActorDescInstanceView& ReferenceActorDescView) = 0;

	/**
	 * Called when the world references a streamed actor.
	 */
	enum class EWorldReferenceInvalidReason
	{
		ReferencedActorIsSpatiallyLoaded,
		ReferencedActorHasDataLayers
	};

	virtual void OnInvalidWorldReference(const IWorldPartitionActorDescInstanceView& ActorDescView, EWorldReferenceInvalidReason Reason) = 0;

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
	 * Called when the data layer asset is not compatible with its data layer asset.
	 */
	virtual void OnInvalidDataLayerAssetType(const UDataLayerInstanceWithAsset* DataLayerInstance, const UDataLayerAsset* DataLayerAsset) = 0;

	/**
	 * Called when an actor needs to be resaved.
	 */
	virtual void OnActorNeedsResave(const IWorldPartitionActorDescInstanceView& ActorDescView) = 0;

	/**
	 * Used to identify a level instance actor error
	 */
	enum class ELevelInstanceInvalidReason
	{
		WorldAssetNotFound,
		WorldAssetNotUsingExternalActors,
		WorldAssetImcompatiblePartitioned,
		WorldAssetHasInvalidContainer,
		CirculalReference
	};

	virtual void OnLevelInstanceInvalidWorldAsset(const IWorldPartitionActorDescInstanceView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason) = 0;

	/**
	 * Called when an actor references another actor with a different set of actor filter.
	 */
	virtual void OnInvalidActorFilterReference(const IWorldPartitionActorDescInstanceView& ActorDescView, const IWorldPartitionActorDescInstanceView& ReferenceActorDescView) = 0;

	/**
	 * Called when an actor has an invalid HLOD layer.
	 */
	virtual void OnInvalidHLODLayer(const IWorldPartitionActorDescInstanceView& ActorDescView) = 0;

	// Helpers
	static ENGINE_API FString GetActorName(const IWorldPartitionActorDescInstanceView& ActorDescView);
	static ENGINE_API FString GetFullActorName(const IWorldPartitionActorDescInstanceView& ActorDescView);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	UE_DEPRECATED(5.4, "Use OnInvalidReferenceDataLayers with EDataLayerInvalidReason instead.")
	virtual void OnInvalidReferenceDataLayers(const IWorldPartitionActorDescInstanceView& ActorDescView, const IWorldPartitionActorDescInstanceView& ReferenceActorDescView) final {}

	UE_DEPRECATED(5.2, "OnInvalidReference is deprecated, use the version which takes an optional actor descriptor view.")
	virtual void OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, FName GridName) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid, FWorldPartitionActorDescView* ReferenceActorDescView) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView) {}
	
	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView) {}
	
	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnLevelInstanceInvalidWorldAsset(const FWorldPartitionActorDescView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason) {}

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidActorFilterReference(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView) {}
	
	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	virtual void OnInvalidHLODLayer(const FWorldPartitionActorDescView& ActorDescView) {}

	UE_DEPRECATED(5.4, "Use OnInvalidWorldReference instead")
	void OnInvalidReferenceLevelScriptStreamed(const IWorldPartitionActorDescInstanceView& ActorDescView) { OnInvalidWorldReference(ActorDescView, EWorldReferenceInvalidReason::ReferencedActorIsSpatiallyLoaded); }

	UE_DEPRECATED(5.4, "Use OnInvalidWorldReference instead")
	void OnInvalidReferenceLevelScriptDataLayers(const IWorldPartitionActorDescInstanceView& ActorDescView) { OnInvalidWorldReference(ActorDescView, EWorldReferenceInvalidReason::ReferencedActorHasDataLayers); }

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	static ENGINE_API FString GetActorName(const FWorldPartitionActorDescView& ActorDescView) { return FString(); }

	UE_DEPRECATED(5.4, "Use IWorldPartitionActorDescInstanceView version instead")
	static ENGINE_API FString GetFullActorName(const FWorldPartitionActorDescView& ActorDescView) { return FString(); }

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};
#endif
