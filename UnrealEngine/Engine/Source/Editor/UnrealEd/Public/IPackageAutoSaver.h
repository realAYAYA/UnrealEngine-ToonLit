// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** An interface to handle the creation, destruction, and restoration of auto-saved packages */
class IPackageAutoSaver
{
public:
	IPackageAutoSaver()
	{
	}

	virtual ~IPackageAutoSaver()
	{
	}

	/**
	 * Update the auto-save count based on the delta-value provided
	 *
	 * @param DeltaSeconds Delta-time (in seconds) since the last update
	 */
	virtual void UpdateAutoSaveCount(const float DeltaSeconds) = 0;

	/** Resets the auto-save timer */
	virtual void ResetAutoSaveTimer() = 0;

	/** Forces auto-save timer to equal auto-save time limit, causing an auto-save attempt */
	virtual void ForceAutoSaveTimer() = 0;

	/**
	 * Forces the auto-save timer to be the auto-save time limit less the passed in value
	 *
	 * @param TimeTillAutoSave The time that will remain until the auto-save limit.
	 */
	virtual void ForceMinimumTimeTillAutoSave(const float TimeTillAutoSave = 10.0f) = 0;

	/** Attempts to auto-save the level and/or content packages, if those features are enabled. */
	virtual void AttemptAutoSave() = 0;

	/** @return If we are currently auto-saving */
	virtual bool IsAutoSaving() const = 0;

	/** Load the restore file from disk (if present) */
	virtual void LoadRestoreFile() = 0;

	/**
	 * Update the file on disk that's used to restore auto-saved packages in the event of a crash
	 * 
	 * @param bRestoreEnabled Is the restore enabled, or is it disabled because we've shut-down cleanly, or are running under the debugger?
	 */
	virtual void UpdateRestoreFile(const bool bRestoreEnabled) = 0;

	/** @return Does we have any information about packages that can be restored */
	virtual bool HasPackagesToRestore() const = 0;

	/** Offer the user the chance to restore any packages that were dirty and have auto-saves */
	virtual void OfferToRestorePackages() = 0;

	/** Called when packages are deleted in the editor */
	virtual void OnPackagesDeleted(const TArray<UPackage*>& DeletedPackages) = 0;

	/**
	 * Set a flag to bypass the recovery UI prompt and to automatically decline package recovery. The auto-saver
	 * flow runs as usual but when the user is normally prompted for recovery, the system will bypass the prompt and select
	 * to dismiss automatically if the flag it set. The flag must be raised before the Engine reach the recovery point
	 * during the boot process, otherwise, it has no effect.
	 */
	virtual void DisableRestorePromptAndDeclinePackageRecovery() = 0;
};
