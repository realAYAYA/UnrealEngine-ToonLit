// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStormSyncDrivesModule.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
#include "IMessageLogListing.h"
#endif

struct FStormSyncMountPointConfig;
class UStormSyncDrivesSettings;

/** Main entry point and implementation of StormSync Mounted Drives Runtime module. */
class FStormSyncDrivesModule : public IStormSyncDrivesModule
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

	//~ Begin IStormSyncDrivesModule interface
	virtual bool RegisterMountPoint(const FStormSyncMountPointConfig& InMountPoint, FText& ErrorText) override;
	virtual bool UnregisterMountPoint(const FStormSyncMountPointConfig& InMountPoint, FText& ErrorText) override;
	//~ End IStormSyncDrivesModule interface

private:
	/** The name of the message output log we will send messages to */
	static constexpr const TCHAR* LogName = TEXT("StormSyncDrives");
	
	/**
	 * Array of Mount Points templates populated from UStormSyncDrivesSettings.
	 * 
	 * Allows projects to specify reusable mount points templates for the content browser.
	*/
	TArray<TSharedPtr<FStormSyncMountPointConfig>> MountedDrives;

#if WITH_EDITORONLY_DATA
	/** Listing used in the editor by the Message Log. */
	TSharedPtr<IMessageLogListing> LogListing;
#endif

	/** Core OnEngineLoopInitComplete delegate used to mount any stored config in developer settings */
	void OnEngineLoopInitComplete();

	/**
	 * Developer settings changed handler. Called anytime settings changed.
	 *
	 * Used to validate user config and if successful, mount the set of Mount Points to Mount Directories.
	 */
	void OnSettingsChanged(UObject* InSettings, FPropertyChangedEvent& InPropertyChangedEvent);

	/** Performs validation for a single mount point config and log any errors to the message log */
	bool ValidateMountPoint(const FStormSyncMountPointConfig& InMountPoint, int32 Index) const;
	
	/**
	 * Performs validation for the provided list of mount point config. Logs any errors to the message log.
	 *
	 * Happens once on engine init, and anytime the user settings changed.
	 */
	bool ValidateMountPoints(const TArray<FStormSyncMountPointConfig>& InMountPoints) const;

	/**
	 * Unregisters previously mounted drives, cache (update internal list of MountedDrives shared pointers),
	 * and register / mount the new config.
	 * 
	 * Happens once on engine init, and anytime the user settings changed.
	 */
	void ResetMountedDrivesFromSettings(const UStormSyncDrivesSettings* InSettings);

	/** Unregisters any previously mounted drives */
	void UnregisterMountedDrives();

	/** Reset and updates internal list of registered drives */
	void CacheMountedDrives(const TArray<FStormSyncMountPointConfig>& InMountPoints);

	/** Tries to mount previously cached list of mount point config */
	void RegisterMountedDrives();

	/** Static helper to log an error to the message log. Used during validation process. */
	static void AddMessageError(const FText& Text);
};
