// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionActorDescView.h"

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionLog.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

FWorldPartitionActorDescView::FWorldPartitionActorDescView()
	: FWorldPartitionActorDescView(nullptr)
{}

FWorldPartitionActorDescView::FWorldPartitionActorDescView(const FWorldPartitionActorDesc* InActorDesc)
	: ActorDesc(InActorDesc)
	, ParentView(nullptr)
	, bIsForcedNonSpatiallyLoaded(false)
	, bIsForcedNoRuntimeGrid(false)
	, bIsForcedNoDataLayers(false)
	, bIsForceNoHLODLayer(false)
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

FVector FWorldPartitionActorDescView::GetOrigin() const
{
	return ActorDesc->GetOrigin();
}

FName FWorldPartitionActorDescView::GetRuntimeGrid() const
{
	if (bIsForcedNoRuntimeGrid)
	{
		return NAME_None;
	}

	if (ParentView)
	{
		return ParentView->GetRuntimeGrid();
	}

	return ActorDesc->GetRuntimeGrid();
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
	if (bIsForcedNonSpatiallyLoaded)
	{
		return false;
	}

	bool bIsSpatiallyLoaded = ActorDesc->GetIsSpatiallyLoaded();
	if (bIsSpatiallyLoaded && ParentView)
	{
		bIsSpatiallyLoaded = ParentView->GetIsSpatiallyLoaded();
	}

	return bIsSpatiallyLoaded;
}

bool FWorldPartitionActorDescView::GetActorIsHLODRelevant() const
{
	return ActorDesc->GetActorIsHLODRelevant();
}

FSoftObjectPath FWorldPartitionActorDescView::GetHLODLayer() const
{
	if (bIsForceNoHLODLayer)
	{
		return FSoftObjectPath();
	}

	if (RuntimedHLODLayer.IsSet())
	{
		return RuntimedHLODLayer.GetValue();
	}

	return ActorDesc->GetHLODLayer();
}

void FWorldPartitionActorDescView::SetDataLayerInstanceNames(const TArray<FName>& InDataLayerInstanceNames)
{
	check(!ActorDesc->HasResolvedDataLayerInstanceNames());
	ResolvedDataLayerInstanceNames = InDataLayerInstanceNames;
}

const TArray<FName>& FWorldPartitionActorDescView::GetDataLayerInstanceNames() const
{
	static TArray<FName> EmptyDataLayers;
	if (bIsForcedNoDataLayers)
	{
		return EmptyDataLayers;
	}

	if (ParentView)
	{
		return ParentView->GetDataLayerInstanceNames();
	}

	if (ResolvedDataLayerInstanceNames.IsSet())
	{
		return ResolvedDataLayerInstanceNames.GetValue();
	}

	return EmptyDataLayers;
}

const TArray<FName>& FWorldPartitionActorDescView::GetRuntimeDataLayerInstanceNames() const
{
	if (bIsForcedNoDataLayers || !ensure(RuntimeDataLayerInstanceNames.IsSet()))
	{
		static TArray<FName> EmptyDataLayers;
		return EmptyDataLayers;
	}

	if (ParentView)
	{
		return ParentView->GetRuntimeDataLayerInstanceNames();
	}

	return RuntimeDataLayerInstanceNames.GetValue();
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

FBox FWorldPartitionActorDescView::GetBounds() const
{
	return ActorDesc->GetBounds();
}

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

const TArray<FGuid>& FWorldPartitionActorDescView::GetEditorReferences() const
{
	return EditorReferences;
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
	return ActorDesc->IsChildContainerInstance();
}

EWorldPartitionActorFilterType FWorldPartitionActorDescView::GetContainerFilterType() const
{
	return ActorDesc->GetChildContainerFilterType();
}

FName FWorldPartitionActorDescView::GetContainerPackage() const
{
	return ActorDesc->GetChildContainerPackage();
}

bool FWorldPartitionActorDescView::GetContainerInstance(FWorldPartitionActorDesc::FContainerInstance& OutContainerInstance) const
{
	return ActorDesc->GetContainerInstance(OutContainerInstance);
}

const FWorldPartitionActorFilter* FWorldPartitionActorDescView::GetContainerFilter() const
{
	check(GetContainerFilterType() != EWorldPartitionActorFilterType::None);
	return ActorDesc->GetContainerFilter();
}

void FWorldPartitionActorDescView::CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const
{
	ActorDesc->CheckForErrors(ErrorHandler);
}

FName FWorldPartitionActorDescView::GetActorLabelOrName() const
{
	return ActorDesc->GetActorLabelOrName();
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
	if (!bIsForcedNoRuntimeGrid)
	{
		bIsForcedNoRuntimeGrid = true;
		UE_LOG(LogWorldPartition, Verbose, TEXT("Actor '%s' runtime grid invalidated"), *GetActorLabelOrName().ToString());
	}
}

void FWorldPartitionActorDescView::SetForcedNoDataLayers()
{
	if (!bIsForcedNoDataLayers)
	{
		bIsForcedNoDataLayers = true;
		UE_LOG(LogWorldPartition, Verbose, TEXT("Actor '%s' data layers invalidated"), *GetActorLabelOrName().ToString());
	}
}

void FWorldPartitionActorDescView::SetRuntimeDataLayerInstanceNames(const TArray<FName>& InRuntimeDataLayerInstanceNames)
{
	RuntimeDataLayerInstanceNames = InRuntimeDataLayerInstanceNames;
}

void FWorldPartitionActorDescView::SetRuntimeReferences(const TArray<FGuid>& InRuntimeReferences)
{
	RuntimeReferences = InRuntimeReferences;
}

void FWorldPartitionActorDescView::SetEditorReferences(const TArray<FGuid>& InEditorReferences)
{
	EditorReferences = InEditorReferences;
}

void FWorldPartitionActorDescView::SetForcedNoHLODLayer()
{
	if (!bIsForceNoHLODLayer)
	{
		bIsForceNoHLODLayer = true;
		UE_LOG(LogWorldPartition, Verbose, TEXT("Actor '%s' HLOD layer invalidated"), *GetActorLabelOrName().ToString());
	}
}

void FWorldPartitionActorDescView::SetRuntimeHLODLayer(const FSoftObjectPath& InHLODLayer)
{
	RuntimedHLODLayer = InHLODLayer;
}

AActor* FWorldPartitionActorDescView::GetActor() const
{
	return ActorDesc->GetActor();
}

bool FWorldPartitionActorDescView::IsEditorOnlyReference(const FGuid& ReferenceGuid) const
{
	return ActorDesc->IsEditorOnlyReference(ReferenceGuid);
}

bool FWorldPartitionActorDescView::GetProperty(FName PropertyName, FName* PropertyValue) const
{
	return ActorDesc->GetProperty(PropertyName, PropertyValue);
}

bool FWorldPartitionActorDescView::HasProperty(FName PropertyName) const
{
	return ActorDesc->HasProperty(PropertyName);
}

void FWorldPartitionActorDescView::SetParentView(const FWorldPartitionActorDescView* InParentView)
{
	check(!ParentView);
	check(GetParentActor().IsValid());
	ParentView = InParentView;
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif