// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectSaveContext.h"

class FUnsavedAssetsTracker;
class SWidget;

/** Invoked when an asset was modified in memory and added to the tracker list. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnsavedAssetAdded, const FString& /*FileAbsPathname*/);

/** Invoked when an asset modified in memory was saved and removed from the tracker list. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUnsavedAssetRemoved, const FString& /*FileAbsPathname*/);


/**
 * Tracks assets that has in-memory modification not saved to disk yet and checks
 * the source control states of those assets when a source control provider is available.
 */
class UNSAVEDASSETSTRACKER_API FUnsavedAssetsTrackerModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Retrieve the module instance.
	 */
	static inline FUnsavedAssetsTrackerModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FUnsavedAssetsTrackerModule>("UnsavedAssetsTracker");
	}

	/** Returns the number of unsaved assets currently tracked. */
	int32 GetUnsavedAssetNum() const;

	/** Returns the list of unsaved assets. */
	TArray<FString> GetUnsavedAssets() const;

	/** Construct the widget for the Editor status bar.*/
	TSharedRef<SWidget> MakeUnsavedAssetsStatusBarWidget();

	/** Invoked when a file is added to the unsaved list. */
	FOnUnsavedAssetAdded OnUnsavedAssetAdded;

	/** Invoked when a file is removed from the unsaved list. */
	FOnUnsavedAssetAdded OnUnsavedAssetRemoved;

private:
	TSharedPtr<FUnsavedAssetsTracker> UnsavedAssetTracker;
};
