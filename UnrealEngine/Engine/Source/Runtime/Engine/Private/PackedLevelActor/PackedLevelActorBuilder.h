// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PackedLevelActor/PackedLevelActorTypes.h"
#include "PackedLevelActor/IPackedLevelActorBuilder.h"
#include "UObject/SoftObjectPtr.h"
#include "Containers/Set.h"

class APackedLevelActor;
class ALevelInstance;
class AActor;
class UActorComponent;
class UBlueprint;
class FMessageLog;

/**
 * FPackedLevelActorBuilder handles packing of ALevelInstance actors into APackedLevelActor actors and Blueprints.
 */
class ENGINE_API FPackedLevelActorBuilder
{
public:
	FPackedLevelActorBuilder();
	
	static TSharedPtr<FPackedLevelActorBuilder> CreateDefaultBuilder();
	
	/* Packs InPackedLevelActor using InLevelInstanceToPack as its source level actor */
	bool PackActor(APackedLevelActor* InPackedLevelActor, ALevelInstance* InLevelInstanceToPack);
	/* Packs InPackedLevelActor using itself as the source level actor */
	bool PackActor(APackedLevelActor* InPackedLevelActor);
	/* Packs InPackedLevelActor using InWorldAsset as the source level */
	bool PackActor(APackedLevelActor* InPackedLevelActor, TSoftObjectPtr<UWorld> InWorldAsset);

	/* Creates/Updates a APackedLevelActor Blueprint from InLevelInstance (will overwrite existing asset or show a dialog if InBlueprintAsset is null) */
	bool CreateOrUpdateBlueprint(ALevelInstance* InLevelInstance, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave = true, bool bPromptForSave = true);
	/* Creates/Updates a APackedLeveInstance Blueprint from InWorldAsset (will overwrite existing asset or show a dialog if InBlueprintAsset is null) */
	bool CreateOrUpdateBlueprint(TSoftObjectPtr<UWorld> InWorldAsset, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave = true, bool bPromptForSave = true);
	/* Update existing Blueprint */
	void UpdateBlueprint(UBlueprint* Blueprint, bool bCheckoutAndSave = true);

	static const FString& GetPackedBPPrefix();
	/* Creates a new APackedLevelActor Blueprint using InPackagePath/InAssetName as hint for path. Prompts the user to input the final asset name. */
	static UBlueprint* CreatePackedLevelActorBlueprintWithDialog(TSoftObjectPtr<UBlueprint> InBlueprintAsset, TSoftObjectPtr<UWorld> InWorldAsset, bool bInCompile);
	/* Creates a new APackedLevelActor Blueprint using InPackagePath/InAssetName */
	static UBlueprint* CreatePackedLevelActorBlueprint(TSoftObjectPtr<UBlueprint> InBlueprintAsset, TSoftObjectPtr<UWorld> InWorldAsset, bool bInCompile);
	
private:
	/* Create/Updates a APackedLevelActor Blueprint from InPackedActor (will overwrite existing asset or show a dialog if InBlueprintAsset is null) */
	bool CreateOrUpdateBlueprintFromPacked(APackedLevelActor* InPackedActor, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave);
	/* Creates/Updates a APackedLevelActor Blueprint from InLevelInstance (will overwrite existing asset or show a dialog if InBlueprintAsset is null) */
	bool CreateOrUpdateBlueprintFromUnpacked(ALevelInstance* InPackedActor, TSoftObjectPtr<UBlueprint> InBlueprintAsset, bool bCheckoutAndSave, bool bPromptForSave);
	/* Creates and loads a ALevelInstance so it can be used for packing */
	ALevelInstance* CreateTransientLevelInstanceForPacking(TSoftObjectPtr<UWorld> InWorldAsset, const FVector& InLocation, const FRotator& InRotator);

	FPackedLevelActorBuilder(const FPackedLevelActorBuilder&) = delete;
	FPackedLevelActorBuilder& operator=(const FPackedLevelActorBuilder&) = delete;

public:
	friend class FPackedLevelActorBuilderContext;
		
private:
	
	TSet<UClass*> ClassDiscards;
	TMap<FPackedLevelActorBuilderID, TUniquePtr<IPackedLevelActorBuilder>> Builders;
};


class FPackedLevelActorBuilderContext
{
public:
	FPackedLevelActorBuilderContext(const FPackedLevelActorBuilder& InBuilder, APackedLevelActor* InPackedLevelActor) 
		: Builders(InBuilder.Builders), ClassDiscards(InBuilder.ClassDiscards), PackedLevelActor(InPackedLevelActor), RelativePivotTransform(FTransform::Identity) {}

	/* Interface for IPackedLevelActorBuilder's to use */
	void ClusterLevelActor(AActor* InLevelActor);
	void FindOrAddCluster(FPackedLevelActorBuilderClusterID&& InClusterID, UActorComponent* InComponent = nullptr);
	void DiscardActor(AActor* InActor);
	void Report(FMessageLog& Log) const;

	const TMap<FPackedLevelActorBuilderClusterID, TArray<UActorComponent*>>& GetClusters() const { return Clusters; }
	
	void SetRelativePivotTransform(const FTransform& InRelativePivotTransform) { RelativePivotTransform = InRelativePivotTransform; }
	const FTransform GetRelativePivotTransform() const { return RelativePivotTransform; }

	bool ShouldPackComponent(UActorComponent* InActorComponent) const;
private:
	const TMap<FPackedLevelActorBuilderID, TUniquePtr<IPackedLevelActorBuilder>>& Builders;
	const TSet<UClass*>& ClassDiscards;

	APackedLevelActor* PackedLevelActor;
	
	TMap<FPackedLevelActorBuilderClusterID, TArray<UActorComponent*>> Clusters;

	TMap<AActor*, TSet<UActorComponent*>> PerActorClusteredComponents;
	TSet<AActor*> ActorDiscards;

	FTransform RelativePivotTransform;
};

#endif