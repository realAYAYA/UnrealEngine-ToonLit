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
	FBox GetBounds() const;
	const TArray<FGuid>& GetReferences() const;
	FString ToString() const;
	const FGuid& GetParentActor() const;
	FName GetActorName() const;
	const FGuid& GetFolderGuid() const;

	FGuid GetContentBundleGuid() const;

	bool IsContainerInstance() const;
	bool GetContainerInstance(const UActorDescContainer*& OutLevelContainer, FTransform& OutLevelTransform, EContainerClusterMode& OutClusterMode) const;

	void CheckForErrors(IStreamingGenerationErrorHandler* ErrorHandler) const;

	FName GetActorLabelOrName() const;

	void SetForcedNonSpatiallyLoaded();
	void SetInvalidRuntimeGrid();
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
	bool bInvalidDataLayers;
	bool bInvalidRuntimeGrid;
	TOptional<TArray<FName>> RuntimeDataLayers;
	TOptional<TArray<FGuid>> RuntimeReferences;
};
#endif