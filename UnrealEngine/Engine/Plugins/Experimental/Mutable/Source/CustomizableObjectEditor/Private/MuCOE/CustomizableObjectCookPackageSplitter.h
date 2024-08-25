// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CookPackageSplitter.h"
#include "UObject/StrongObjectPtr.h"
#include "MuCO/CustomizableObject.h"

/** Handles splitting the streamable Data constants into their own packages */
class FCustomizableObjectCookPackageSplitter : public ICookPackageSplitter
{
public:
	/** ICookPackageSplitter interface */
	static bool ShouldSplit(UObject* SplitData);
	static FString GetSplitterDebugName() { return TEXT("FCustomizableObjectCookPackageSplitter"); }

	virtual TArray<ICookPackageSplitter::FGeneratedPackage> GetGenerateList(
		const UPackage* OwnerPackage,
		const UObject* OwnerObject) override;
	
	virtual bool PreSaveGeneratorPackage(
		UPackage* OwnerPackage,
		UObject* OwnerObject,
		const TArray<ICookPackageSplitter::FGeneratedPackageForPreSave>& PlaceholderPackages,
		TArray<UPackage*>& OutKeepReferencedPackages) override;

	virtual void PostSaveGeneratorPackage(UPackage* OwnerPackage, UObject* OwnerObject) override;

	virtual bool PopulateGeneratedPackage(
		UPackage* OwnerPackage,
		UObject* OwnerObject,
		const FGeneratedPackageForPopulate& GeneratedPackage,
		TArray<UObject*>& OutObjectsToMove,
		TArray<UPackage*>& OutKeepReferencedPackages) override;

	virtual void PostSaveGeneratedPackage(
		UPackage* OwnerPackage,
		UObject* OwnerObject,
		const FGeneratedPackageForPopulate& GeneratedPackage) override;

	/** Do teardown actions after all packages have saved, or when the cook is cancelled. Always called before destruction. */
	virtual void Teardown(ETeardown Status) override;

	/**
	 * If true, this splitter forces the Generator package objects it needs to remain referenced, and the cooker
	 * should expect them to still be in memory after a garbage collect so long as the splitter is alive.
	 */
	virtual bool UseInternalReferenceToAvoidGarbageCollect() override { return true; }

private:

	TArray<FString> SavedContainerNames;
	TArray<FString> SavedExtensionContainerNames;

	// Keep a strong reference to the CO to protect it from garbage collector.
	TStrongObjectPtr<const UObject> StrongObject;
};
