// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

class AActor;
struct FAssetData;

struct ENGINE_API FWorldPartitionActorDescUtils
{
	/** 
	 * Checks if the provided asset data contains a valid actor descriptor.
	 * @param InAssetData	The asset data to look into.
	 * @return				Whether the provideed asset data contains a valid actor descriptor or not.
	 */
	static bool IsValidActorDescriptorFromAssetData(const FAssetData& InAssetData);

	/** 
	 * Retrieve the actor's native class from the asset data.
	 * @param InAssetData	The asset data to look into.
	 * @return				The actor's native class.
	 */
	static UClass* GetActorNativeClassFromAssetData(const FAssetData& InAssetData);

	/** 
	 * Creates a valid actor descriptor from the provided asset data.
	 * @param InAssetData	The asset data to look into.
	 * @return				Actor descriptor retrieved from the provided asset data..
	 */
	static TUniquePtr<FWorldPartitionActorDesc> GetActorDescriptorFromAssetData(const FAssetData& InAssetData);

	/** 
	 * Appends the actor's actor descriptor data into the provided asset registry tags.
	 * @param InActor		The actor that will append its actor descriptor.
	 * @param OutTags		Output tags to output into.
	 */
	static void AppendAssetDataTagsFromActor(const AActor* InActor, TArray<UObject::FAssetRegistryTag>& OutTags);

	/** 
	 * Update an actor descriptor with new values coming from the provided actor.
	 * @param InActor		The actor to update from..
	 * @param OutActorDesc	Actor descriptor unique pointer that will get updated with the new actor descriptor.
	 */
	static void UpdateActorDescriptorFomActor(const AActor* InActor, TUniquePtr<FWorldPartitionActorDesc>& OutActorDesc);

	/** 
	 * Replaces the actor descriptor's actor pointer with the provided new actor pointer.
	 * @param InOldActor	The old actor that the provided actor descriptor was representing.
	 * @param InNewActor	The new actor that the provided actor descriptor should be representing.
	 * @param InActorDesc	Actor descriptor that will get its actor pointer updated.
	 */
	static void ReplaceActorDescriptorPointerFromActor(const AActor* InOldActor, AActor* InNewActor, FWorldPartitionActorDesc* InActorDesc);
};
#endif