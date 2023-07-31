// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "TickableEditorObject.h"

class FExtender;
class FMenuBuilder;
class SWidget;
class SWindow;
class UToolMenu;

class CONTENTBROWSERASSETDATASOURCE_API FAssetFolderContextMenu : public TSharedFromThis<FAssetFolderContextMenu>, public FTickableEditorObject
{
public:
	virtual ~FAssetFolderContextMenu() = default;

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

	/** Handler for when "Checkout from source control" is selected */
	void ExecuteSCCCheckOut();

	/** Handler for when "Open for Add to source control" is selected */
	void ExecuteSCCOpenForAdd();

	/** Handler for when "Checkin to source control" is selected */
	void ExecuteSCCCheckIn();

	/** Handler for when "Sync from source control" is selected */
	void ExecuteSCCSync() const;

	/** Handler for when "Connect to source control" is selected */
	void ExecuteSCCConnect() const;

	/** Handler to check to see if "Checkout from source control" can be executed */
	bool CanExecuteSCCCheckOut() const;

	/** Handler to check to see if "Open for Add to source control" can be executed */
	bool CanExecuteSCCOpenForAdd() const;

	/** Handler to check to see if "Checkin to source control" can be executed */
	bool CanExecuteSCCCheckIn() const;

	/** Handler to check to see if "Sync" can be executed */
	bool CanExecuteSCCSync() const;

	/** Handler to check to see if "Connect to source control" can be executed */
	bool CanExecuteSCCConnect() const;	

	bool IsProcessingSCCCheckOut() const;

	bool IsProcessingSCCOpenForAdd() const;

	bool IsProcessingSCCCheckIn() const;

	/** Initializes and start to process some variable used to in "CanExecute" checks that won't change at runtime or are too expensive to check every frame. */
	void StartProcessCanExecuteVars();

	/** Returns a list of names of packages in all selected paths in the sources view */
	void GetPackageNamesInSelectedPaths(TArray<FString>& OutPackageNames) const;

	/** Gets the first selected path, if it exists */
	FString GetFirstSelectedPath() const;

	/** Begin FTickableEditorObject interface */
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !PackageNamesToProcess.IsEmpty(); }
	virtual TStatId GetStatId() const override;
	/** End FTickableEditorObject interface */
private:

	void ProcessCanExecuteVars();

	/** Stop or cancel the process of some variable used to in "CanExecute" checks */
	void StopProcessCanExecuteVars();

	TArray<FString> SelectedPaths;

	// Not null if the selection contained some assets also
	TArray<FString> SelectedPackages;

	TWeakPtr<SWidget> ParentWidget;

	/** Cached SCC CanExecute vars */
	bool bCanExecuteSCCCheckOut = false;
	bool bCanExecuteSCCOpenForAdd = false;
	bool bCanExecuteSCCCheckIn = false;

	TArray<FString> PackageNamesToProcess;
	int32 CurrentPackageIndex = 0;
};
