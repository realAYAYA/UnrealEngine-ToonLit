// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationProxyErrorHandler.h"

void FStreamingGenerationProxyErrorHandler::OnInvalidRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, FName GridName)
{
	InnerErrorHandler->OnInvalidRuntimeGrid(ActorDescView, GridName);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid, FWorldPartitionActorDescView* ReferenceActorDescView)
{
	InnerErrorHandler->OnInvalidReference(ActorDescView, ReferenceGuid, ReferenceActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	InnerErrorHandler->OnInvalidReferenceGridPlacement(ActorDescView, ReferenceActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	InnerErrorHandler->OnInvalidReferenceDataLayers(ActorDescView, ReferenceActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	InnerErrorHandler->OnInvalidReferenceRuntimeGrid(ActorDescView, ReferenceActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView)
{
	InnerErrorHandler->OnInvalidReferenceLevelScriptStreamed(ActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView)
{
	InnerErrorHandler->OnInvalidReferenceLevelScriptDataLayers(ActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidReferenceDataLayerAsset(const UDataLayerInstanceWithAsset* DataLayerInstance)
{
	InnerErrorHandler->OnInvalidReferenceDataLayerAsset(DataLayerInstance);
}

void FStreamingGenerationProxyErrorHandler::OnDataLayerHierarchyTypeMismatch(const UDataLayerInstance* DataLayerInstance, const UDataLayerInstance* Parent)
{
	InnerErrorHandler->OnDataLayerHierarchyTypeMismatch(DataLayerInstance, Parent);
}

void FStreamingGenerationProxyErrorHandler::OnDataLayerAssetConflict(const UDataLayerInstanceWithAsset* DataLayerInstance, const UDataLayerInstanceWithAsset* ConflictingDataLayerInstance)
{
	InnerErrorHandler->OnDataLayerAssetConflict(DataLayerInstance, ConflictingDataLayerInstance);
}

void FStreamingGenerationProxyErrorHandler::OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView)
{
	InnerErrorHandler->OnActorNeedsResave(ActorDescView);
}

void FStreamingGenerationProxyErrorHandler::OnLevelInstanceInvalidWorldAsset(const FWorldPartitionActorDescView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason)
{
	InnerErrorHandler->OnLevelInstanceInvalidWorldAsset(ActorDescView, WorldAsset, Reason);
}

void FStreamingGenerationProxyErrorHandler::OnInvalidActorFilterReference(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	InnerErrorHandler->OnInvalidActorFilterReference(ActorDescView, ReferenceActorDescView);
}
#endif
