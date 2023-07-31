// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "WorldPartition/ErrorHandling/WorldPartitionStreamingGenerationLogErrorHandler.h"
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "GameFramework/Actor.h"

void FStreamingGenerationLogErrorHandler::OnInvalidReference(const FWorldPartitionActorDescView& ActorDescView, const FGuid& ReferenceGuid)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Actor %s have missing reference to %s"), *GetFullActorName(ActorDescView), *ReferenceGuid.ToString());
}

void FStreamingGenerationLogErrorHandler::OnInvalidReferenceGridPlacement(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	const FString SpatiallyLoadedActor(TEXT("Spatially loaded actor"));
	const FString NonSpatiallyLoadedActor(TEXT("Non-spatially loaded loaded actor"));

	UE_LOG(LogWorldPartition, Log, TEXT("%s %s reference %s %s"), ActorDescView.GetIsSpatiallyLoaded() ? *SpatiallyLoadedActor : *NonSpatiallyLoadedActor, *GetFullActorName(ActorDescView), ReferenceActorDescView.GetIsSpatiallyLoaded() ? *SpatiallyLoadedActor : *NonSpatiallyLoadedActor, *GetFullActorName(ReferenceActorDescView));
}

void FStreamingGenerationLogErrorHandler::OnInvalidReferenceDataLayers(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Actor %s references an actor in a different set of runtime data layers %s"), *GetFullActorName(ActorDescView), *GetFullActorName(ReferenceActorDescView));
}

void FStreamingGenerationLogErrorHandler::OnInvalidReferenceRuntimeGrid(const FWorldPartitionActorDescView& ActorDescView, const FWorldPartitionActorDescView& ReferenceActorDescView)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Actor %s references an actor in a different runtime grid %s"), *GetFullActorName(ActorDescView), *GetFullActorName(ReferenceActorDescView));
}

void FStreamingGenerationLogErrorHandler::OnInvalidReferenceLevelScriptStreamed(const FWorldPartitionActorDescView& ActorDescView)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Level Script Blueprint references streamed actor %s"), *GetFullActorName(ActorDescView));
}

void FStreamingGenerationLogErrorHandler::OnInvalidReferenceLevelScriptDataLayers(const FWorldPartitionActorDescView& ActorDescView)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Level Script Blueprint references streamed actor %s with a non empty set of data layers"), *GetFullActorName(ActorDescView));
}

void FStreamingGenerationLogErrorHandler::OnInvalidReferenceDataLayerAsset(const UDataLayerInstanceWithAsset* DataLayerInstance)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Data Layer %s does not have a Data Layer asset"), *DataLayerInstance->GetDataLayerFName().ToString());
}

void FStreamingGenerationLogErrorHandler::OnDataLayerHierarchyTypeMismatch(const UDataLayerInstance* DataLayerInstance, const UDataLayerInstance* Parent)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Data Layer %s is of Type %s and its parent %s is of type %s"), *DataLayerInstance->GetDataLayerFullName(), *UEnum::GetValueAsString(DataLayerInstance->GetType()), *Parent->GetDataLayerFullName(), *UEnum::GetValueAsString(Parent->GetType()));
}

void FStreamingGenerationLogErrorHandler::OnDataLayerAssetConflict(const UDataLayerInstanceWithAsset* DataLayerInstance, const UDataLayerInstanceWithAsset* ConflictingDataLayerInstance)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Data Layer Instance %s and Data Layer Instance %s are both referencing Data Layer Asset %s"), *DataLayerInstance->GetDataLayerFName().ToString(), *ConflictingDataLayerInstance->GetDataLayerFName().ToString(), *DataLayerInstance->GetAsset()->GetFullName());
}

void FStreamingGenerationLogErrorHandler::OnActorNeedsResave(const FWorldPartitionActorDescView& ActorDescView)
{
	UE_LOG(LogWorldPartition, Log, TEXT("Actor %s needs to be resaved"), *GetFullActorName(ActorDescView));
}

void FStreamingGenerationLogErrorHandler::OnLevelInstanceInvalidWorldAsset(const FWorldPartitionActorDescView& ActorDescView, FName WorldAsset, ELevelInstanceInvalidReason Reason)
{
	const FString ActorName = GetFullActorName(ActorDescView);

	switch (Reason)
	{
	case ELevelInstanceInvalidReason::WorldAssetNotFound:
		UE_LOG(LogWorldPartition, Log, TEXT("Actor %s has an invalid world asset '%s'"), *ActorName, *WorldAsset.ToString());
		break;
	case ELevelInstanceInvalidReason::WorldAssetNotUsingExternalActors:
		UE_LOG(LogWorldPartition, Log, TEXT("Actor %s world asset '%s' is not using external actors"), *ActorName, *WorldAsset.ToString());
		break;
	case ELevelInstanceInvalidReason::WorldAssetImcompatiblePartitioned:
		UE_LOG(LogWorldPartition, Log, TEXT("Actor %s world asset '%s' is partitioned but not marked as compatible"), *ActorName, *WorldAsset.ToString());
		break;
	};
}
#endif
