// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetThumbnail.h"
#include "EditorConfigBase.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "ContentBrowserConfig.generated.h"

USTRUCT()
struct FCollectionsConfig
{
	GENERATED_BODY()

	UPROPERTY()
	bool bExpanded = false;

	UPROPERTY()
	bool bSearchExpanded = false;
	
	UPROPERTY()
	TArray<FString> SelectedCollections;

	UPROPERTY()
	TArray<FString> ExpandedCollections;
};

USTRUCT()
struct FPathViewConfig
{
	GENERATED_BODY()

	UPROPERTY()
	bool bExpanded = false;

	UPROPERTY()
	TArray<FName> SelectedPaths;

	UPROPERTY()
	TArray<FString> PluginFilters;
};

USTRUCT()
struct FContentBrowserInstanceConfig
{
	GENERATED_BODY()

	UPROPERTY()
	FCollectionsConfig Collections;

	UPROPERTY()
	FPathViewConfig PathView;

	UPROPERTY()
	bool bShowFavorites = true;

	UPROPERTY()
	bool bFavoritesExpanded = true;

	UPROPERTY()
	bool bSourcesExpanded = true;

	UPROPERTY()
	bool bFilterRecursively = false;
	
	UPROPERTY()
	bool bShowFolders = true;

	UPROPERTY()
	bool bShowEmptyFolders = true;

	UPROPERTY()
	bool bShowEngineContent = false;

	UPROPERTY()
	bool bShowDeveloperContent = false;

	UPROPERTY()
	bool bShowLocalizedContent = false;

	UPROPERTY()
	bool bShowPluginContent = false;

	UPROPERTY()
	bool bShowCppFolders = false;

	bool bCollectionsDocked = true;

	UPROPERTY()
	bool bSearchClasses = false;

	UPROPERTY()
	bool bSearchAssetPaths = false;

	UPROPERTY()
	bool bSearchCollections = false;
};

UCLASS(EditorConfig="ContentBrowser")
class UContentBrowserConfig : public UEditorConfigBase
{
	GENERATED_BODY()
	
public:

	static void Initialize();
	static UContentBrowserConfig* Get() { return Instance; }

	UPROPERTY()
	TSet<FString> Favorites;

	UPROPERTY(meta=(EditorConfig))
	TMap<FName, FContentBrowserInstanceConfig> Instances;

private:
	static TObjectPtr<UContentBrowserConfig> Instance;
};