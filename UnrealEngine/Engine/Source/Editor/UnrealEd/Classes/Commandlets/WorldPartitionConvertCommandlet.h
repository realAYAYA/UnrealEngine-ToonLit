// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectAnnotation.h"
#include "Commandlets/Commandlet.h"
#include "PackageSourceControlHelper.h"
#include "WorldPartition/WorldPartitionEditorHash.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartitionConvertCommandlet.generated.h"

UNREALED_API DECLARE_LOG_CATEGORY_EXTERN(LogWorldPartitionConvertCommandlet, Log, All);

class ULevelStreaming;
class UWorldPartition;
class ULevelStreaming;
class UDataLayerFactory;

USTRUCT()
struct FHLODLayerActorMapping
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY()
	FString HLODLayer;
};

UCLASS(Config = Engine)
class UNREALED_API UWorldPartitionConvertCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	static const FString GetConversionSuffix(const bool bInOnlyMergeSubLevels);

private:
	void GatherAndPrepareSubLevelsToConvert(ULevel* Level, TArray<ULevel*>& SubLevels);

protected:
	virtual bool GetAdditionalLevelsToConvert(ULevel* Level, TArray<ULevel*>& SubLevels);
	virtual bool PrepareStreamingLevelForConversion(ULevelStreaming* StreamingLevel);
	virtual bool ShouldConvertStreamingLevel(ULevelStreaming* StreamingLevel);
	virtual bool ShouldDeleteActor(AActor* Actor, bool bMainLevel) const;
	virtual void PerformAdditionalWorldCleanup(UWorld* World) const;
	virtual void PerformAdditionalActorChanges(AActor* Actor) const {}
	virtual void OutputConversionReport() const;
	virtual void OnWorldLoaded(UWorld* World);
	virtual void ReadAdditionalTokensAndSwitches(const TArray<FString>& Tokens, const TArray<FString>& Switches) {}

	UWorldPartition* CreateWorldPartition(class AWorldSettings* MainWorldSettings);
	UWorld* LoadWorld(const FString& LevelToLoad);
	ULevel* InitWorld(UWorld* World);

	void ChangeObjectOuter(UObject* Object, UObject* NewOuter);
	void FixupSoftObjectPaths(UPackage* OuterPackage);

	bool DetachDependantLevelPackages(ULevel* Level);
	bool RenameWorldPackageWithSuffix(UWorld* World);

	void SetupHLOD();
	void SetupHLODLayerAssets();
	UHLODLayer* CreateHLODLayerFromINI(const FString& InHLODLayerName);

	void SetActorGuid(AActor* Actor, const FGuid& NewGuid);
	void CreateWorldMiniMapTexture(UWorld* World);

	// Conversion report
	TSet<FString> MapsWithLevelScriptsBPs;
	TSet<FString> MapsWithMapBuildData;
	TSet<FString> ActorsWithChildActors;
	TSet<FString> GroupActors;
	TSet<FString> ActorsInGroupActors;
	TSet<FString> ActorsReferencesToActors;

	TMap<FString, FString> RemapSoftObjectPaths;

	FString LevelConfigFilename;
	TArray<UPackage*> PackagesToSave;
	TArray<UPackage*> PackagesToDelete;

	bool bOnlyMergeSubLevels;
	bool bDeleteSourceLevels;
	bool bGenerateIni;
	bool bReportOnly;
	bool bVerbose;
	bool bConversionSuffix;
	bool bDisableStreaming;
	FString ConversionSuffix;

	UPROPERTY(Config)
	TSubclassOf<UWorldPartitionEditorHash> EditorHashClass;

	UPROPERTY(Config)
	TSubclassOf<UWorldPartitionRuntimeHash> RuntimeHashClass;

	// Levels excluded from conversion.
	UPROPERTY(Config)
	TArray<FString> ExcludedLevels;

	UPROPERTY(Config)
	bool bConvertActorsNotReferencedByLevelScript;

	UPROPERTY(Config)
	FVector WorldOrigin;
	
	UPROPERTY(Config)
	FVector WorldExtent;

	UPROPERTY(Config)
	FString HLODLayerAssetsPath;

	UPROPERTY(Config)
	FString DefaultHLODLayerName;

	UPROPERTY(Config)
	FString DefaultHLODLayerAsset;

	UPROPERTY(Config)
	FString FoliageTypePath;

	UPROPERTY(Config)
	TArray<FHLODLayerActorMapping> HLODLayersForActorClasses;

	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UHLODLayer>> HLODLayers;

	UPROPERTY(Config)
	uint32 LandscapeGridSize;

	UPROPERTY(Config)
	FString DataLayerAssetFolder;

	UPROPERTY()
	TObjectPtr<UDataLayerFactory> DataLayerFactory;
};
