// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * WorldPartitionLevelHelper
 *
 * Helper class to build Levels for World Partition
 *
 */

#pragma once

#include "Engine/World.h"
#include "UObject/LinkerInstancingContext.h"
#include "WorldPartition/WorldPartitionRuntimeCell.h"

class FWorldPartitionPackageHelper;
class UWorldPartition;
struct FActorContainerID;

class FWorldPartitionLevelHelper
{
public:
	ENGINE_API static FString AddActorContainerIDToSubPathString(const FActorContainerID& InContainerID, const FString& InSubPathString);
	ENGINE_API static FString AddActorContainerID(const FActorContainerID& InContainerID, const FString& InActorName);

#if WITH_EDITOR
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
	
	UE_DEPRECATED(5.4, "LoadActors is deprecated, LoadActors with FLoadActorsParams should be used instead.")
	static bool LoadActors(UWorld* InOuterWorld, ULevel* InDestLevel, TArrayView<FWorldPartitionRuntimeCellObjectMapping> InActorPackages, FPackageReferencer& InPackageReferencer, TFunction<void(bool)> InCompletionCallback, bool bInLoadAsync, FLinkerInstancingContext InInstancingContext);

	/* Struct of optional parameters passed to foreach actordesc functions. */
	struct FLoadActorsParams
	{
		FLoadActorsParams()
			: OuterWorld(nullptr)
			, DestLevel(nullptr)
			, PackageReferencer(nullptr)
			, bLoadAsync(false)
		{}

		UWorld* OuterWorld;
		ULevel* DestLevel;
		TArrayView<FWorldPartitionRuntimeCellObjectMapping> ActorPackages;
		FPackageReferencer* PackageReferencer;
		TFunction<void(bool)> CompletionCallback;
		bool bLoadAsync;
		mutable FLinkerInstancingContext InstancingContext;

		FLoadActorsParams& SetOuterWorld(UWorld* InOuterWorld) { OuterWorld = InOuterWorld; return *this; }
		FLoadActorsParams& SetDestLevel(ULevel* InDestLevel) { DestLevel = InDestLevel; return *this; }
		FLoadActorsParams& SetActorPackages(TArrayView<FWorldPartitionRuntimeCellObjectMapping> InActorPackages) { ActorPackages = InActorPackages; return *this; }
		FLoadActorsParams& SetPackageReferencer(FPackageReferencer* InPackageReferencer) { PackageReferencer = InPackageReferencer; return *this; }
		FLoadActorsParams& SetCompletionCallback(TFunction<void(bool)> InCompletionCallback) { CompletionCallback = InCompletionCallback; return *this; }
		FLoadActorsParams& SetLoadAsync(bool bInLoadAsync) { bLoadAsync = bInLoadAsync; return *this; }
		FLoadActorsParams& SetInstancingContext(FLinkerInstancingContext InInstancingContext) { InstancingContext = InInstancingContext; return *this; }
	};

	static bool LoadActors(const FLoadActorsParams& InParams);
	
	static FSoftObjectPath RemapActorPath(const FActorContainerID& InContainerID, const FString& SourceWorldName, const FSoftObjectPath& InActorPath);

private:
	FWorldPartitionLevelHelper();

	void AddReference(UPackage* InPackage, FPackageReferencer* InReferencer);
	void RemoveReferences(FPackageReferencer* InReferencer);
	void PreGarbageCollect();

	static FString GetContainerPackage(const FActorContainerID& InContainerID, const FString& InPackageName, const FString& InDestLevelPackageName = FString());
	static UWorld::InitializationValues GetWorldInitializationValues();


	friend class FContentBundleEditor;
	static bool RemapLevelCellPathInContentBundle(ULevel* Level, const class FContentBundleEditor* ContentBundleEditor, const UWorldPartitionRuntimeCell* Cell);

	struct FPackageReference
	{
		TSet<FPackageReferencer*> Referencers;
		TWeakObjectPtr<UPackage> Package;
	};

	friend struct FPackageReferencer;

	TMap<FName, FPackageReference> PackageReferences;

	TSet<TWeakObjectPtr<UPackage>> PreGCPackagesToUnload;
#endif
};

