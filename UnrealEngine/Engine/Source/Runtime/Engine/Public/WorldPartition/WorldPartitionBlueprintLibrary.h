// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/LoaderAdapter/LoaderAdapterActorList.h"
#include "WorldPartitionBlueprintLibrary.generated.h"

class FWorldPartitionActorDesc;

/**
 * Snapshot of an actor descriptor, which represents the state of an actor on disk.
 * The actor may or may not be loaded.
 */
USTRUCT(BlueprintType)
struct FActorDesc
{
	GENERATED_USTRUCT_BODY()

	FActorDesc();

#if WITH_EDITOR
	FActorDesc(const FWorldPartitionActorDesc& InActorDesc, const FTransform& InTransform);
#endif

	/** The actor GUID of this descriptor. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FGuid Guid;

	/** Actor class, can point to a native or Blueprint class and may be redirected. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FSoftObjectPath Class;

	/** Internal name of the acgor. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FName Name;

	/** Actor's label. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FName Label;

	/** Streaming bounds of this actor. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FBox Bounds;

	/** Actor's target runtime grid. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FName RuntimeGrid;

	/** Actor's streaming state. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	bool bIsSpatiallyLoaded;

	/** Actor's editor-only property. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	bool bActorIsEditorOnly;

	/** Actor's package name. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FName ActorPackage;

	/** Actor's path name. */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category=Actor)
	FName ActorPath;
};

UCLASS(MinimalAPI)
class UWorldPartitionBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#if WITH_EDITOR
	static UWorld* GetEditorWorld();
	static UWorldPartition* GetWorldPartition();

	static void OnWorldPartitionUninitialized(UWorldPartition* InWorldPartition);
	static TMap<UWorldPartition*, TUniquePtr<FLoaderAdapterActorList>> LoaderAdapterActorListMap;
	static FDelegateHandle OnWorldPartitionUninitializedHandle;

	static bool GetActorDescs(const UWorldPartition* WorldPartition, TArray<FActorDesc>& OutActorDescs);
	static bool GetActorDescs(const UActorDescContainer* InContainer, const FTransform& InTransform, TArray<FActorDesc>& OutActorDescs);
	static bool GetIntersectingActorDescs(UWorldPartition* WorldPartition, const FBox& InBox, TArray<FActorDesc>& OutActorDescs);
	static bool GetIntersectingActorDescs(const UActorDescContainer* InContainer, const FBox& InBox, const FTransform& InTransform, TArray<FActorDesc>& OutActorDescs);
	static bool HandleIntersectingActorDesc(const FWorldPartitionActorDesc* ActorDesc, const FBox& InBox, const FTransform& InTransform, TArray<FActorDesc>& OutActorDescs);
#endif

public:
	/**
	 * Gets the editor world bounds, which includes all actor descriptors.
	 * @return The editor world bounds.
	 */
	UFUNCTION(BlueprintCallable, Category="World Partition")
	static FBox GetEditorWorldBounds();

	/**
	 * Gets the runtime world bounds, which only includes actor descriptors that aren't editor only.
	 * @return The runtime world bounds.
	 */
	UFUNCTION(BlueprintCallable, Category="World Partition")
	static FBox GetRuntimeWorldBounds();

	/**
	 * Load actors
	 */
	UFUNCTION(BlueprintCallable, Category="World Partition")
	static void LoadActors(const TArray<FGuid>& InActorsToLoad);

	/**
	 * Unload actors
	 */
	UFUNCTION(BlueprintCallable, Category="World Partition")
	static void UnloadActors(const TArray<FGuid>& InActorsToLoad);

	/**
	 * Gets all the actor descriptors into the provided array, recursing into actor containers.
	 * @return True if the operation was successful.
	 */
	UFUNCTION(BlueprintCallable, Category="World Partition")
	static bool GetActorDescs(TArray<FActorDesc>& OutActorDescs);

	/**
	 * Gets all the actor descriptors intersecting the provided box into the provided array, recursing into actor containers.
	 * @return True if the operation was successful.
	 */
	UFUNCTION(BlueprintCallable, Category="World Partition")
	static bool GetIntersectingActorDescs(const FBox& InBox, TArray<FActorDesc>& OutActorDescs);
};