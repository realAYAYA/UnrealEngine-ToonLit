// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/World.h"
#include "WorldPartitionEditorPerProjectUserSettings.generated.h"

USTRUCT()
struct FWorldPartitionPerWorldSettings
{
	GENERATED_BODY();

#if WITH_EDITOR
	FWorldPartitionPerWorldSettings()
	{}

	void Reset()
	{
		LoadedEditorRegions.Empty();
		LoadedEditorLocationVolumes.Empty();
		NotLoadedDataLayers.Empty();
		LoadedDataLayers.Empty();
	}
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FBox> LoadedEditorRegions;

	UPROPERTY()
	TArray<FName> LoadedEditorLocationVolumes;

	UPROPERTY()
	TArray<FName> NotLoadedDataLayers;

	UPROPERTY()
	TArray<FName> LoadedDataLayers;
#endif
};

UCLASS(config = EditorPerProjectUserSettings)
class ENGINE_API UWorldPartitionEditorPerProjectUserSettings : public UObject
{
	GENERATED_BODY()

public:
	UWorldPartitionEditorPerProjectUserSettings(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
#if WITH_EDITORONLY_DATA
		, bHideEditorDataLayers(false)
		, bHideRuntimeDataLayers(false)
		, bHideDataLayerActors(true)
		, bHideUnloadedActors(false)
		, bShowOnlySelectedActors(false)
		, bHighlightSelectedDataLayers(true)
		, bHideLevelInstanceContent(true)
		, bDisableLoadingOfLastLoadedRegions(false)
#endif
	{}

#if WITH_EDITOR
	TArray<FBox> GetEditorLoadedRegions(UWorld* InWorld) const;
	void SetEditorLoadedRegions(UWorld* InWorld, const TArray<FBox>& InEditorLoadedRegions);

	TArray<FName> GetEditorLoadedLocationVolumes(UWorld* InWorld) const;
	void SetEditorLoadedLocationVolumes(UWorld* InWorld, const TArray<FName>& InEditorLoadedLocationVolumes);

	bool GetEnableLoadingOfLastLoadedRegions() const
	{
		return !bDisableLoadingOfLastLoadedRegions;
	}

	bool GetBugItGoLoadRegion() const
	{
		return bBugItGoLoadRegion;
	}

	void SetBugItGoLoadRegion(bool bInBugItGoLoadRegion)
	{
		if (bBugItGoLoadRegion != bInBugItGoLoadRegion)
		{
			bBugItGoLoadRegion = bInBugItGoLoadRegion;
			SaveConfig();
		}
	}

	bool GetShowCellCoords() const
	{
		return bShowCellCoords;
	}

	void SetShowCellCoords(bool bInShowCellCoords)
	{
		if (bShowCellCoords != bInShowCellCoords)
		{
			bShowCellCoords = bInShowCellCoords;
			SaveConfig();
		}
	}

	TArray<FName> GetWorldDataLayersNotLoadedInEditor(UWorld* InWorld) const;
	TArray<FName> GetWorldDataLayersLoadedInEditor(UWorld* InWorld) const;
	
	void SetWorldDataLayersNonDefaultEditorLoadStates(UWorld* InWorld, const TArray<FName>& InDataLayersLoadedInEditor, const TArray<FName>& InDataLayersNotLoadedInEditor);

private:
	const FWorldPartitionPerWorldSettings* GetWorldPartitionPerWorldSettings(UWorld* InWorld) const;
#endif

#if WITH_EDITORONLY_DATA
public:
	/** True when the Data Layer Outliner is displaying Editor Data Layers */
	UPROPERTY(config)
	uint32 bHideEditorDataLayers : 1;

	/** True when the Data Layer Outliner is displaying Runtime Data Layers */
	UPROPERTY(config)
	uint32 bHideRuntimeDataLayers : 1;

	/** True when the Data Layer Outliner is not displaying actors */
	UPROPERTY(config)
	uint32 bHideDataLayerActors : 1;

	/** True when the Data Layer Outliner is not displaying unloaded actors */
	UPROPERTY(config)
	uint32 bHideUnloadedActors : 1;

	/** True when the Data Layer Outliner is only displaying actors and datalayers for selected actors */
	UPROPERTY(config)
	uint32 bShowOnlySelectedActors : 1;

	/** True when the Data Layer Outliner highlights Data Layers containing actors that are currently selected */
	UPROPERTY(config)
	uint32 bHighlightSelectedDataLayers : 1;

	/** True when the Data Layer Outliner is not displaying Level Instance content */
	UPROPERTY(config)
	uint32 bHideLevelInstanceContent : 1;

private:
	bool ShouldSaveSettings(const UWorld* InWorld) const
	{
		return InWorld && !InWorld->IsGameWorld() && FPackageName::DoesPackageExist(InWorld->GetPackage()->GetName());
	}

	UPROPERTY(config)
	uint32 bDisableLoadingOfLastLoadedRegions : 1;

	UPROPERTY(config)
	uint32 bBugItGoLoadRegion : 1;

	UPROPERTY(config)
	uint32 bShowCellCoords : 1;

	UPROPERTY(config)
	TMap<TSoftObjectPtr<UWorld>, FWorldPartitionPerWorldSettings> PerWorldEditorSettings;
#endif
};