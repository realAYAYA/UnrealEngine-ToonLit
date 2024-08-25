// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_EDITOR
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

class AActor;
struct FAssetData;
class FWorldPartitionActorDescInstance;

struct FWorldPartitionActorDescUtils
{
	/** @return The asset registry tag name for ActorMetaDataClass */
	static ENGINE_API FName ActorMetaDataClassTagName();
	/** @return The asset registry tag name for ActorMetaData */
	static ENGINE_API FName ActorMetaDataTagName();

	/** 
	 * Checks if the provided asset data contains a valid actor descriptor.
	 * @param InAssetData	The asset data to look into.
	 * @return				Whether the provideed asset data contains a valid actor descriptor or not.
	 */
	static ENGINE_API bool IsValidActorDescriptorFromAssetData(const FAssetData& InAssetData);

	/** 
	 * Retrieve the actor's native class from the asset data.
	 * @param InAssetData	The asset data to look into.
	 * @return				The actor's native class.
	 */
	static ENGINE_API UClass* GetActorNativeClassFromAssetData(const FAssetData& InAssetData);

	/** 
	 * Creates a valid actor descriptor from the provided asset data.
	 * @param InAssetData	The asset data to look into.
	 * @return				Actor descriptor retrieved from the provided asset data.
	 */
	static ENGINE_API TUniquePtr<FWorldPartitionActorDesc> GetActorDescriptorFromAssetData(const FAssetData& InAssetData);

	/**
	 * Appends the actor's actor descriptor data into the provided asset registry tags.
	 * @param InActor		The actor that will append its actor descriptor.
	 * @param OutTags		Output tags to output into.
	 */
	static ENGINE_API void AppendAssetDataTagsFromActor(const AActor* InActor, FAssetRegistryTagsContext Context);
	UE_DEPRECATED(5.4, "Call the version that takes FAssetRegistryTagsContext instead.")
	static ENGINE_API void AppendAssetDataTagsFromActor(const AActor* InActor, TArray<UObject::FAssetRegistryTag>& OutTags);

	/** 
	 * Return the actor descriptor data.
	 * @param InActorDesc	The actor descriptor that will generate the data.
	 * @return				The actor descriptor data.
	 */
	static ENGINE_API FString GetAssetDataFromActorDescriptor(TUniquePtr<FWorldPartitionActorDesc>& InActorDesc);

	/** 
	 * Patches an actor descriptor's asset data
	 * @param InAssetData			The asset data to be patched.
	 * @param OutAssetData			The patched asset data.
	 * @param InAssetDataPatcher	The patcher object.
	 * @return						The actor descriptor patched data.
	 */
	static ENGINE_API bool GetPatchedAssetDataFromAssetData(const FAssetData& InAssetData, FString& OutAssetData, FWorldPartitionAssetDataPatcher* InAssetDataPatcher);

	/** 
	 * Update an actor descriptor with new values coming from the provided actor.
	 * @param InActor		The actor to update from.
	 * @param OutActorDesc	Actor descriptor unique pointer that will get updated with the new actor descriptor.
	 */
	static ENGINE_API void UpdateActorDescriptorFromActor(const AActor* InActor, TUniquePtr<FWorldPartitionActorDesc>& OutActorDesc);

	/** 
	 * Update an actor descriptor with new values coming from the provided actor descriptor.
	 * @param InActorDesc	The actor descriptor to update from.
	 * @param OutActorDesc	Actor descriptor unique pointer that will get updated with the new actor descriptor.
	 */
	static ENGINE_API void UpdateActorDescriptorFromActorDescriptor(TUniquePtr<FWorldPartitionActorDesc>& InActorDesc, TUniquePtr<FWorldPartitionActorDesc>& OutActorDesc);

	/** 
	 * Replaces the actor descriptor's actor pointer with the provided new actor pointer.
	 * @param InOldActor	The old actor that the provided actor descriptor was representing.
	 * @param InNewActor	The new actor that the provided actor descriptor should be representing.
	 * @param InActorDescInstance	Actor descriptor instance that will get its actor pointer updated.
	 */
	static ENGINE_API void ReplaceActorDescriptorPointerFromActor(const AActor* InOldActor, AActor* InNewActor, FWorldPartitionActorDescInstance* InActorDescInstance);

	UE_DEPRECATED(5.4, "Use FWorldPartitionActorDescInstance version instead")
	static ENGINE_API void ReplaceActorDescriptorPointerFromActor(const AActor* InOldActor, AActor* InNewActor, FWorldPartitionActorDesc* InActorDesc) {}

	UE_DEPRECATED(5.3, "Use FWorldPartitionHelpers::FixupRedirectedAssetPath instead")
	static ENGINE_API bool FixupRedirectedAssetPath(FName& InOutAssetPath);
};
#endif
