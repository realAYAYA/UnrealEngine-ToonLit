// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescView.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

FWorldPartitionActorDescView::FWorldPartitionActorDescView()
	: FWorldPartitionActorDescView(nullptr)
{}

FWorldPartitionActorDescView::FWorldPartitionActorDescView(const FWorldPartitionActorDesc* InActorDesc)
	: ActorDesc(InActorDesc)
	, bIsForcedNonSpatiallyLoaded(false)
	, bIsForcedNoRuntimeGrid(false)
	, bInvalidDataLayers(false)	
{}

const FGuid& FWorldPartitionActorDescView::GetGuid() const
{
	return ActorDesc->GetGuid();
}

FTopLevelAssetPath FWorldPartitionActorDescView::GetBaseClass() const
{
	return ActorDesc->GetBaseClass();
}

FTopLevelAssetPath FWorldPartitionActorDescView::GetNativeClass() const
{
	return ActorDesc->GetNativeClass();
}

UClass* FWorldPartitionActorDescView::GetActorNativeClass() const
{
	return ActorDesc->GetActorNativeClass();
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FVector FWorldPartitionActorDescView::GetOrigin() const
{
	return ActorDesc->GetOrigin();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FName FWorldPartitionActorDescView::GetRuntimeGrid() const
{
	return bIsForcedNoRuntimeGrid ? NAME_None : ActorDesc->GetRuntimeGrid();
}

bool FWorldPartitionActorDescView::GetActorIsEditorOnly() const
{
	return ActorDesc->GetActorIsEditorOnly();
}

bool FWorldPartitionActorDescView::GetActorIsRuntimeOnly() const
{
	return ActorDesc->GetActorIsRuntimeOnly();
}

bool FWorldPartitionActorDescView::GetIsSpatiallyLoaded() const
{
	return bIsForcedNonSpatiallyLoaded ? false : ActorDesc->GetIsSpatiallyLoaded();
}

bool FWorldPartitionActorDescView::GetActorIsHLODRelevant() const
{
	return ActorDesc->GetActorIsHLODRelevant();
}

FName FWorldPartitionActorDescView::GetHLODLayer() const
{
	return ActorDesc->GetHLODLayer();
}

const TArray<FName>& FWorldPartitionActorDescView::GetDataLayers() const
{
	static TArray<FName> EmptyDataLayers;
	return bInvalidDataLayers ? EmptyDataLayers : ActorDesc->GetDataLayerInstanceNames();
}

const TArray<FName>& FWorldPartitionActorDescView::GetDataLayerInstanceNames() const
{
	return ActorDesc->GetDataLayerInstanceNames();
}
const TArray<FName>& FWorldPartitionActorDescView::GetRuntimeDataLayers() const
{
	static TArray<FName> EmptyDataLayers;
	return (bInvalidDataLayers || !RuntimeDataLayers.IsSet()) ? EmptyDataLayers : RuntimeDataLayers.GetValue();
}

const TArray<FName>& FWorldPartitionActorDescView::GetTags() const
{
	return ActorDesc->GetTags();
}

FName FWorldPartitionActorDescView::GetActorPackage() const
{
	return ActorDesc->GetActorPackage();
}

FSoftObjectPath FWorldPartitionActorDescView::GetActorSoftPath() const
{
	return ActorDesc->GetActorSoftPath();
}

FName FWorldPartitionActorDescView::GetActorLabel() const
{
	return ActorDesc->GetActorLabel();
}

FName FWorldPartitionActorDescView::GetActorName() const
{
	return ActorDesc->GetActorName();
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FBox FWorldPartitionActorDescView::GetBounds() const
{
	return ActorDesc->GetBounds();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FBox FWorldPartitionActorDescView::GetEditorBounds() const
{
	return ActorDesc->GetEditorBounds();
}

FBox FWorldPartitionActorDescView::GetRuntimeBounds() const
{
	return ActorDesc->GetRuntimeBounds();
}

const TArray<FGuid>& FWorldPartitionActorDescView::GetReferences() const
{
	return RuntimeReferences.IsSet() ? RuntimeReferences.GetValue() : ActorDesc->GetReferences();
}

FString FWorldPartitionActorDescView::ToString() const
{
	return ActorDesc->ToString();
}

const FGuid& FWorldPartitionActorDescView::GetParentActor() const
{
	return ActorDesc->GetParentActor();
}

const FGuid& FWorldPartitionActorDescView::GetFolderGuid() const
{
	return ActorDesc->GetFolderGuid();
}

FGuid FWorldPartitionActorDescView::GetContentBundleGuid() const
{
	return ActorDesc->GetContentBundleGuid();
}

bool FWorldPartitionActorDescView::IsContainerInstance() const
{
	return ActorDesc->IsContainerInstance();
}

FName FWorldPartitionActorDescView::GetLevelPackage() const
{
	return ActorDesc->GetLevelPackage();
}

bool FWorldPartitionActorDescView::GetContainerInstance(const UActorDescContainer*& OutLevelContainer, FTransform& OutLevelTransform, EContainerClusterMode& OutClusterMode) const
{
	return ActorDesc->GetContainerInstance(OutLevelContainer, OutLevelTransform, OutClusterMode);
}

void FWorldPartitionActorDescView::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	ActorDesc->CheckForErrors(ErrorHandler);
}

FName FWorldPartitionActorDescView::GetActorLabelOrName() const
{
	return ActorDesc->GetActorLabelOrName();
}

bool FWorldPartitionActorDescView::ShouldValidateRuntimeGrid() const
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return ActorDesc->ShouldValidateRuntimeGrid();
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FWorldPartitionActorDescView::SetForcedNonSpatiallyLoaded()
{
	if (!bIsForcedNonSpatiallyLoaded)
	{
		bIsForcedNonSpatiallyLoaded = true;
		UE_LOG(LogWorldPartition, Verbose, TEXT("Actor '%s' forced to be non-spatially loaded"), *GetActorLabelOrName().ToString());
	}
}

void FWorldPartitionActorDescView::SetForcedNoRuntimeGrid()
{
	bIsForcedNoRuntimeGrid = true;	
}

void FWorldPartitionActorDescView::SetInvalidDataLayers()
{
	if (!bInvalidDataLayers)
	{
		bInvalidDataLayers = true;
		UE_LOG(LogWorldPartition, Verbose, TEXT("Actor '%s' data layers invalidated"), *GetActorLabelOrName().ToString());
	}
}

void FWorldPartitionActorDescView::SetRuntimeDataLayers(TArray<FName>& InRuntimeDataLayers)
{
	RuntimeDataLayers = InRuntimeDataLayers;
}

void FWorldPartitionActorDescView::SetRuntimeReferences(TArray<FGuid>& InRuntimeReferences)
{
	RuntimeReferences = InRuntimeReferences;
}

AActor* FWorldPartitionActorDescView::GetActor() const
{
	return ActorDesc->GetActor();
}
#endif
