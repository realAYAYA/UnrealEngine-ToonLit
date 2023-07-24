// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "Misc/Optional.h"

#if WITH_EDITOR
class AActor;
class FWorldPartitionActorDesc;
class IStreamingGenerationErrorHandler;
class UActorDescContainer;
class UWorldPartition;
enum class EContainerClusterMode : uint8;

/**
 * A view on top of an actor desc, used to cache information that can be (potentially) different than the actor desc
 * itself due to streaming generation logic, etc.
 */
class ENGINE_API FWorldPartitionActorDescView
{
	friend class UWorldPartitionRuntimeHash;

public:
	FWorldPartitionActorDescView();
	FWorldPartitionActorDescView(const FWorldPartitionActorDesc* InActorDesc);

	const FGuid& GetGuid() const;
	FTopLevelAssetPath GetBaseClass() const;
	FTopLevelAssetPath GetNativeClass() const;
	UClass* GetActorNativeClass() const;
	UE_DEPRECATED(5.2, "GetOrigin is deprecated.")
	FVector GetOrigin() const;
	FName GetRuntimeGrid() const;
	bool GetIsSpatiallyLoaded() const;
	bool GetActorIsEditorOnly() const;
	bool GetActorIsRuntimeOnly() const;
	bool GetActorIsHLODRelevant() const;
	FName GetHLODLayer() const;
	const TArray<FName>& GetDataLayers() const;
	const TArray<FName>& GetDataLayerInstanceNames() const;
	const TArray<FName>& GetRuntimeDataLayers() const;
	const TArray<FName>& GetTags() const;
	FName GetActorPackage() const;
	
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.1, "GetActorPath is deprecated, GetActorSoftPath should be used instead.")
	inline FName GetActorPath() const { return GetActorSoftPath().ToFName(); }
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	FSoftObjectPath GetActorSoftPath() const;
	FName GetActorLabel() const;

	UE_DEPRECATED(5.2, "GetBounds is deprecated, GetEditorBounds or GetRuntimeBounds should be used instead.")
	FBox GetBounds() const;

	FBox GetEditorBounds() const;
	FBox GetRuntimeBounds() const;

	const TArray<FGuid>& GetReferences() const;
	FString ToString() const;
	const FGuid& GetParentActor() const;
	FName GetActorName() const;
	const FGuid& GetFolderGuid() const;

	FGuid GetContentBundleGuid() const;

	bool IsContainerInstance() const;
	FName GetLevelPackage() const;
	bool GetContainerInstance(const UActorDescContainer*& OutLevelContainer, FTransform& OutLevelTransform, EContainerClusterMode& OutClusterMode) const;

	void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const;

	FName GetActorLabelOrName() const;

	UE_DEPRECATED(5.2, "ShouldValidateRuntimeGrid is deprecated and should not be used.")
	bool ShouldValidateRuntimeGrid() const;

	void SetForcedNonSpatiallyLoaded();
	void SetForcedNoRuntimeGrid();
	void SetInvalidDataLayers();
	void SetRuntimeDataLayers(TArray<FName>& InRuntimeDataLayers);
	void SetRuntimeReferences(TArray<FGuid>& InRuntimeReferences);

	AActor* GetActor() const;

	bool operator==(const FWorldPartitionActorDescView& Other) const
	{
		return GetGuid() == Other.GetGuid();
	}

	friend uint32 GetTypeHash(const FWorldPartitionActorDescView& Key)
	{
		return GetTypeHash(Key.GetGuid());
	}

	const FWorldPartitionActorDesc* GetActorDesc() const { return ActorDesc; }

protected:
	const FWorldPartitionActorDesc* ActorDesc;
	bool bIsForcedNonSpatiallyLoaded;
	bool bIsForcedNoRuntimeGrid;
	bool bInvalidDataLayers;	
	TOptional<TArray<FName>> RuntimeDataLayers;
	TOptional<TArray<FGuid>> RuntimeReferences;
};
#endif