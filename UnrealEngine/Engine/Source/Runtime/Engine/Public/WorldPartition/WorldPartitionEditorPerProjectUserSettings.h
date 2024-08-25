// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Engine/World.h"
#include "Engine/DeveloperSettings.h"
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

UCLASS(config = EditorPerProjectUserSettings, MinimalAPI, meta = (DisplayName = "World Partition (Local)"))
class UWorldPartitionEditorPerProjectUserSettings : public UDeveloperSettings
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
		, MinimapUnloadedOpacity(0.66f)
#endif
	{}

#if WITH_EDITOR
	ENGINE_API TArray<FBox> GetEditorLoadedRegions(UWorld* InWorld) const;
	ENGINE_API void SetEditorLoadedRegions(UWorld* InWorld, const TArray<FBox>& InEditorLoadedRegions);

	ENGINE_API TArray<FName> GetEditorLoadedLocationVolumes(UWorld* InWorld) const;
	ENGINE_API void SetEditorLoadedLocationVolumes(UWorld* InWorld, const TArray<FName>& InEditorLoadedLocationVolumes);

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

	float GetMinimapUnloadedOpacity() const
	{
		return MinimapUnloadedOpacity;
	}

	void SetMinimapUnloadedOpacity(float InMinimapUnloadedOpacity)
	{
		if (MinimapUnloadedOpacity != InMinimapUnloadedOpacity)
		{
			MinimapUnloadedOpacity = InMinimapUnloadedOpacity;
			SaveConfig();
		}
	}

	void ClearPerWorldSettings()
	{
		PerWorldEditorSettings.Empty();
		SaveConfig();
	}

	ENGINE_API TArray<FName> GetWorldDataLayersNotLoadedInEditor(UWorld* InWorld) const;
	ENGINE_API TArray<FName> GetWorldDataLayersLoadedInEditor(UWorld* InWorld) const;
	
	ENGINE_API void SetWorldDataLayersNonDefaultEditorLoadStates(UWorld* InWorld, const TArray<FName>& InDataLayersLoadedInEditor, const TArray<FName>& InDataLayersNotLoadedInEditor);
		
private:
	const FWorldPartitionPerWorldSettings* GetWorldPartitionPerWorldSettings(UWorld* InWorld) const;
#endif

#if WITH_EDITORONLY_DATA
public:
	/** True when the Data Layer Outliner is displaying Editor Data Layers */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bHideEditorDataLayers : 1;

	/** True when the Data Layer Outliner is displaying Runtime Data Layers */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bHideRuntimeDataLayers : 1;

	/** True when the Data Layer Outliner is not displaying actors */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bHideDataLayerActors : 1;

	/** True when the Data Layer Outliner is not displaying unloaded actors */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bHideUnloadedActors : 1;

	/** True when the Data Layer Outliner is only displaying actors and datalayers for selected actors */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bShowOnlySelectedActors : 1;

	/** True when the Data Layer Outliner highlights Data Layers containing actors that are currently selected */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bHighlightSelectedDataLayers : 1;

	/** True when the Data Layer Outliner is not displaying Level Instance content */
	UPROPERTY(config, EditAnywhere, Category = "Data Layer")
	uint32 bHideLevelInstanceContent : 1;

private:
	bool ShouldSaveSettings(const UWorld* InWorld) const;

	bool ShouldLoadSettings(const UWorld* InWorld) const;

	UPROPERTY(config, EditAnywhere, Category = Default)
	uint32 bDisableLoadingOfLastLoadedRegions : 1;

	UPROPERTY(config, EditAnywhere, Category = Default)
	uint32 bBugItGoLoadRegion : 1;

	UPROPERTY(config, EditAnywhere, Category = Default)
	uint32 bShowCellCoords : 1;

	UPROPERTY(config, EditAnywhere, Category = Default, meta = (ClampMin=0.0, ClampMax=1.0))
	float MinimapUnloadedOpacity;

	UPROPERTY(config)
	TMap<TSoftObjectPtr<UWorld>, FWorldPartitionPerWorldSettings> PerWorldEditorSettings;
#endif
};
