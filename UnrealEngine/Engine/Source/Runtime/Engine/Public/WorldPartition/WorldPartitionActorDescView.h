// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/Optional.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

#if WITH_EDITOR
class AActor;
class IStreamingGenerationErrorHandler;
class UActorDescContainer;
class UWorldPartition;
struct FWorldPartitionActorFilter;
enum class EWorldPartitionActorFilterType : uint8;

/**
 * A view on top of an actor desc, used to cache information that can be (potentially) different than the actor desc
 * itself due to streaming generation logic, etc.
 */
class UE_DEPRECATED(5.4, "Class is deprecated in favor of IWorldPartitionActorDescInstanceView interface") FWorldPartitionActorDescView
{
	friend class UWorldPartitionRuntimeHash;

public:
	ENGINE_API FWorldPartitionActorDescView();
	ENGINE_API FWorldPartitionActorDescView(const FWorldPartitionActorDesc * InActorDesc);

	ENGINE_API const FGuid& GetGuid() const;
	ENGINE_API FTopLevelAssetPath GetBaseClass() const;
	ENGINE_API FTopLevelAssetPath GetNativeClass() const;
	ENGINE_API UClass* GetActorNativeClass() const;
	UE_DEPRECATED(5.2, "GetOrigin is deprecated.")
		ENGINE_API FVector GetOrigin() const;
	ENGINE_API FName GetRuntimeGrid() const;
	ENGINE_API bool GetIsSpatiallyLoaded() const;
	ENGINE_API bool GetActorIsEditorOnly() const;
	ENGINE_API bool GetActorIsRuntimeOnly() const;
	ENGINE_API bool GetActorIsHLODRelevant() const;
	ENGINE_API FSoftObjectPath GetHLODLayer() const;
	ENGINE_API const TArray<FName>& GetDataLayerInstanceNames() const;
	ENGINE_API const TArray<FName>& GetRuntimeDataLayerInstanceNames() const;
	ENGINE_API const TArray<FName>& GetTags() const;
	ENGINE_API FName GetActorPackage() const;

	ENGINE_API FSoftObjectPath GetActorSoftPath() const;
	ENGINE_API FName GetActorLabel() const;

	UE_DEPRECATED(5.2, "GetBounds is deprecated, GetEditorBounds or GetRuntimeBounds should be used instead.")
		ENGINE_API FBox GetBounds() const;

	ENGINE_API FBox GetEditorBounds() const;
	ENGINE_API FBox GetRuntimeBounds() const;

	ENGINE_API const TArray<FGuid>& GetReferences() const;
	ENGINE_API const TArray<FGuid>& GetEditorReferences() const;
	ENGINE_API FString ToString() const;
	ENGINE_API const FGuid& GetParentActor() const;
	ENGINE_API FName GetActorName() const;
	ENGINE_API const FGuid& GetFolderGuid() const;

	ENGINE_API FGuid GetContentBundleGuid() const;
	ENGINE_API FName GetContainerPackage() const;
	ENGINE_API bool IsContainerInstance() const;
	ENGINE_API bool GetContainerInstance(FWorldPartitionActorDesc::FContainerInstance & OutContainerInstance) const;
	ENGINE_API EWorldPartitionActorFilterType GetContainerFilterType() const;
	ENGINE_API const FWorldPartitionActorFilter* GetContainerFilter() const;

	UE_DEPRECATED(5.3, "GetLevelPackage is deprecated use GetContainerPackage instead.")
		FName GetLevelPackage() const { return GetContainerPackage(); }

	ENGINE_API void CheckForErrors(IStreamingGenerationErrorHandler * ErrorHandler) const;

	ENGINE_API FName GetActorLabelOrName() const;

	ENGINE_API void SetForcedNonSpatiallyLoaded();
	ENGINE_API void SetForcedNoRuntimeGrid();
	ENGINE_API void SetForcedNoDataLayers();
	ENGINE_API void SetRuntimeDataLayerInstanceNames(const TArray<FName>&InRuntimeDataLayerInstanceNames);
	ENGINE_API void SetRuntimeReferences(const TArray<FGuid>&InRuntimeReferences);
	ENGINE_API void SetEditorReferences(const TArray<FGuid>&InEditorReferences);
	ENGINE_API void SetDataLayerInstanceNames(const TArray<FName>&InDataLayerInstanceNames);

	ENGINE_API void SetForcedNoHLODLayer();
	ENGINE_API void SetRuntimeHLODLayer(const FSoftObjectPath & InHLODLayer);

	ENGINE_API AActor* GetActor() const;

	bool operator==(const FWorldPartitionActorDescView & Other) const
	{
		return GetGuid() == Other.GetGuid();
	}

	friend uint32 GetTypeHash(const FWorldPartitionActorDescView & Key)
	{
		return GetTypeHash(Key.GetGuid());
	}

	const FWorldPartitionActorDesc* GetActorDesc() const { return ActorDesc; }

	ENGINE_API bool IsEditorOnlyReference(const FGuid & ReferenceGuid) const;

	ENGINE_API bool GetProperty(FName PropertyName, FName * PropertyValue) const;
	ENGINE_API bool HasProperty(FName PropertyName) const;

	ENGINE_API void SetParentView(const FWorldPartitionActorDescView * InParentView);

protected:
	const FWorldPartitionActorDesc* ActorDesc;
	const FWorldPartitionActorDescView* ParentView;
	bool bIsForcedNonSpatiallyLoaded;
	bool bIsForcedNoRuntimeGrid;
	bool bIsForcedNoDataLayers;
	bool bIsForceNoHLODLayer;
	TOptional<TArray<FName>> ResolvedDataLayerInstanceNames;
	TOptional<TArray<FName>> RuntimeDataLayerInstanceNames;
	TOptional<TArray<FGuid>> RuntimeReferences;
	TOptional<FSoftObjectPath> RuntimedHLODLayer;
	TArray<FGuid> EditorReferences;
};
#endif