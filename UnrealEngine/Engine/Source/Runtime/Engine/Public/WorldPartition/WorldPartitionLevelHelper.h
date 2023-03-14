// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * WorldPartitionLevelHelper
 *
 * Helper class to build Levels for World Partition
 *
 */

#pragma once

#if WITH_EDITOR

#include "Engine/World.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"

class FWorldPartitionPackageHelper;
class UWorldPartition;
struct FActorContainerID;

class FWorldPartitionLevelHelper
{
public:
	static FWorldPartitionLevelHelper& Get();

	struct FPackageReferencer
	{
		~FPackageReferencer() { RemoveReferences(); }

		void AddReference(UPackage* InPackage);
		void RemoveReferences();
	};

	static ULevel* CreateEmptyLevelForRuntimeCell(const UWorldPartitionRuntimeCell* Cell, const UWorld* InWorld, const FString& InWorldAssetName, UPackage* DestPackage = nullptr);
	static void MoveExternalActorsToLevel(const TArray<FWorldPartitionRuntimeCellObjectMapping>& InChildPackages, ULevel* InLevel, TArray<UPackage*>& OutModifiedPackages);
	static void RemapLevelSoftObjectPaths(ULevel* InLevel, UWorldPartition* InWorldPartition);
	
	static bool LoadActors(UWorld* InOwningWorld, ULevel* InDestLevel, TArrayView<FWorldPartitionRuntimeCellObjectMapping> InActorPackages, FPackageReferencer& InPackageReferencer, TFunction<void(bool)> InCompletionCallback, bool bInLoadAsync, FLinkerInstancingContext InOutInstancingContext);
	
	static bool RemapActorPath(const FActorContainerID& InContainerID, const FString& InActorPath, FString& OutActorPath);
	static FString AddActorContainerIDToSubPathString(const FActorContainerID& InContainerID, const FString& InSubPathString);
	static FString GetContainerPackage(const FActorContainerID& InContainerID, const FString& InPackageName, int32 InPIEInstanceID);
private:
	FWorldPartitionLevelHelper();

	void AddReference(UPackage* InPackage, FPackageReferencer* InReferencer);
	void RemoveReferences(FPackageReferencer* InReferencer);
	void PreGarbageCollect();

	static UWorld::InitializationValues GetWorldInitializationValues();

	struct FPackageReference
	{
		TSet<FPackageReferencer*> Referencers;
		TWeakObjectPtr<UPackage> Package;
	};

	friend struct FPackageReferencer;

	TMap<FName, FPackageReference> PackageReferences;

	TSet<TWeakObjectPtr<UPackage>> PreGCPackagesToUnload;
};

#endif