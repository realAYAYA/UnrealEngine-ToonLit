// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FRedirectorRefs;
enum class ERedirectFixupMode;

class FAssetFixUpRedirectors : public TSharedFromThis<FAssetFixUpRedirectors>
{
public:
	/**
	 * Fix up references to the specified redirectors.
	 * @param bCheckoutDialogPrompt indicates whether to prompt the user with files checkout dialog or silently attempt to checkout all necessary files.
	 */
	void FixupReferencers(const TArray<UObjectRedirector*>& Objects, bool bCheckoutDialogPrompt, ERedirectFixupMode FixupMode) const;

	/** Returns whether redirectors are being fixed up. */
	bool IsFixupReferencersInProgress() const { return bIsFixupReferencersInProgress; }

private:

	/** The core code of the fixup operation */
	void ExecuteFixUp(TArray<TWeakObjectPtr<UObjectRedirector>> Objects, bool bCheckoutDialogPrompt, ERedirectFixupMode FixupMode) const;

	/** Fills out the Referencing packages for all the redirectors described in AssetsToPopulate */
	void PopulateRedirectorReferencers(TArray<FRedirectorRefs>& RedirectorsToPopulate) const;

	/** Updates the source control status of the packages containing the assets to rename */
	bool UpdatePackageStatus(const TArray<FRedirectorRefs>& RedirectorsToFix) const;

	/** 
	  * Loads all referencing packages to redirectors in RedirectorsToFix, finds redirectors whose references can
	  * not be fixed up, and returns a list of referencing packages to save.
	  */
	void LoadReferencingPackages(TArray<FRedirectorRefs>& RedirectorsToFix, TArray<UPackage*>& OutReferencingPackagesToSave, TArray<UPackage*>& OutLoadedPackages)const;

	/** 
	  * Check out referencing packages and marks assets whose referencing packages were not checked out to not fix the redirector.
	  * Trims PackagesToSave when necessary.
	  * Returns true if the user opted to continue the operation or no dialog was required.
	  */
	bool CheckOutReferencingPackages(TArray<FRedirectorRefs>& RedirectorsToFix, TArray<UPackage*>& InOutReferencingPackagesToSave, bool bCheckoutDialogPrompt) const;

	/** Finds any read only packages and removes them from the save list. Redirectors referenced by these packages will not be fixed up. */ 
	void DetectReadOnlyPackages(TArray<FRedirectorRefs>& RedirectorsToFix, TArray<UPackage*>& InOutReferencingPackagesToSave) const;

	/** FixUp soft object paths */
	void FixUpSoftObjectPaths(const TArray<FRedirectorRefs>& RedirectorsToFix, const TArray<UPackage*>& InReferencingPackagesToSave) const;

	/** Saves all the referencing packages and updates SCC state */
	void SaveReferencingPackages(const TArray<UPackage*>& ReferencingPackagesToSave, TArray<UPackage*>& OutFailedToSave) const;

	/** Saves any collections that were referencing any of the redirectors and updates SCC state */
	void SaveReferencingCollections(TArray<FRedirectorRefs>& RedirectorsToFix) const;

	/** Waits for the asset registry to update its asset referencer cache */
	void UpdateAssetReferencers(const TArray<FRedirectorRefs>& RedirectorsToFix) const;

	/** Deletes redirectors that are valid to delete */
	void DeleteRedirectors(TArray<FRedirectorRefs>& RedirectorsToFix, const TArray<UPackage*>& FailedToSave) const;

	/** Report any failures that may have happened during the rename */
	void ReportFailures(const TArray<FRedirectorRefs>& RedirectorsToFix) const;

private:

	mutable bool bIsFixupReferencersInProgress = false;
};
