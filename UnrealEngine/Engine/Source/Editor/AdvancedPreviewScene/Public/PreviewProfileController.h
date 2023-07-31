// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPreviewProfileController.h"

class UAssetViewerSettings;
class UEditorPerProjectUserSettings;

/**
 * Controls the access to the preview profiles. It serves as a bridge between the AdvancedPreviewScene
 * and the UnrealEd modules and enable UnrealEd to change or observe change to the active profile.
 * 
 * @note This class was created to decouple UnrealEd from AdvancedPreviewScene and prevent
 *       circular dependencies between the modules.
 */
class ADVANCEDPREVIEWSCENE_API FPreviewProfileController : public IPreviewProfileController
{
public:
	FPreviewProfileController();
	virtual ~FPreviewProfileController();

	/** Returns the list of available preview profiles names. */
	virtual TArray<FString> GetPreviewProfiles(int32& OutCurrentProfileIndex) const override;

	/** Set the specified preview profiles as the active one. */
	virtual bool SetActiveProfile(const FString& ProfileName) override;

	/** Returns the preview profiles currently active. */
	virtual FString GetActiveProfile() const override;

	/** Invoked after the list of available profiles has changed. */
	virtual FOnPreviewProfileListChanged& OnPreviewProfileListChanged() override { return OnPreviewProfileListChangedDelegate; }

	/** Invoked after the active preview profile changed. */
	virtual FOnPreviewProfileChanged& OnPreviewProfileChanged() override { return OnPreviewSettingChangedDelegate; }

private:
	void UpdateAssetViewerProfiles();
	void EnsureProfilesStateCoherence() const;

private:
	/** Provides the list of available preview profiles along with the profile change delegates (to update the profile combo box selection) */
	UAssetViewerSettings* AssetViewerSettings = nullptr;

	/** Holds the asset viewer profile currently used by the project. */
	UEditorPerProjectUserSettings* PerProjectSettings = nullptr;

	/** The list of available profiles. */
	TArray<FString> AssetViewerProfileNames;

	/** Holds the current profile index in the list. This is kept consistent with the cache list of names. */
	int32 CurrentProfileIndex = 0;

	FOnPreviewProfileListChanged OnPreviewProfileListChangedDelegate;
	FOnPreviewProfileChanged OnPreviewSettingChangedDelegate;

	FDelegateHandle AssetViewerSettingsProfileAddRemoveHandle;
	FDelegateHandle AssetViewerSettingsChangedHandle;
};
