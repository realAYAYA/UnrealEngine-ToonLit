// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class UToolMenu;

class CONTENTBROWSERASSETDATASOURCE_API FAssetFolderContextMenu : public TSharedFromThis<FAssetFolderContextMenu>
{
public:
	UE_DEPRECATED(5.1, "Use the new overload.")
	void MakeContextMenu(
		UToolMenu* InMenu,
		const TArray<FString>& InSelectedPackagePaths
	);

	/** 
	 * Makes the context menu widget
	 * @param InMenu The menu in which the entries will be added
	 * @param InSelectedPackagePaths The path of the selected folders
	 * @param InSelectedPackages The package names of the selected asset item. Empty if the selection is only some folder items.
	 */
	void MakeContextMenu(
		UToolMenu* InMenu,
		const TArray<FString>& InSelectedPackagePaths,
		const TArray<FString>& InSelectedPackages
		);

private:
	/** Makes the asset tree context menu widget */
	void AddMenuOptions(UToolMenu* Menu);

	/** Handler for when "Migrate Folder" is selected */
	void ExecuteMigrateFolder();

	/** Handler for when "Fix up Redirectors in Folder" is selected */
	void ExecuteFixUpRedirectorsInFolder();

	/** Gets the first selected path, if it exists */
	FString GetFirstSelectedPath() const;

private:
	TArray<FString> SelectedPaths;

	// Not null if the selection contained some assets also
	TArray<FString> SelectedPackages;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
