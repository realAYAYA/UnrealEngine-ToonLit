// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "AssetTypeCategories.h"
#include "Framework/Commands/UIAction.h"

class UToolMenu;
struct FCategorySubMenuItem;

class CONTENTBROWSERASSETDATASOURCE_API FNewAssetContextMenu
{
public:
	DECLARE_DELEGATE_OneParam(FOnImportAssetRequested, const FName /*SelectedPath*/);
	DECLARE_DELEGATE_TwoParams(FOnNewAssetRequested, const FName /*SelectedPath*/, TWeakObjectPtr<UClass> /*FactoryClass*/);

	/** Makes the context menu widget */
	static void MakeContextMenu(
		UToolMenu* Menu,
		const TArray<FName>& InSelectedAssetPaths,
		const FOnImportAssetRequested& InOnImportAssetRequested,
		const FOnNewAssetRequested& InOnNewAssetRequested
		);

private:
	/** Handle creating a new asset from an asset category */
	static void CreateNewAssetMenuCategory(UToolMenu* Menu, FName SectionName, EAssetTypeCategories::Type AssetTypeCategory, FName InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction);
	static void CreateNewAssetMenus(UToolMenu* Menu, FName SectionName, TSharedPtr<FCategorySubMenuItem> SubMenuData, FName InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction);

	/** Handle when the "Import" button is clicked */
	static void ExecuteImportAsset(FOnImportAssetRequested InOnImportAssetRequested, FName InPath);

	/** Create a new asset using the specified factory at the specified path */
	static void ExecuteNewAsset(FOnNewAssetRequested InOnNewAssetRequested, FName InPath, TWeakObjectPtr<UClass> FactoryClass);
};
