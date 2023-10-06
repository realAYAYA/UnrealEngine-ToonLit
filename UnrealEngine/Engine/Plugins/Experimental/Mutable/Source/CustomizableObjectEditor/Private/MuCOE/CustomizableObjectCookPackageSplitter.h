// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CookPackageSplitter.h"
#include "MuCO/CustomizableObject.h"

/** Handles splitting the streamable Extension Data constants into their own packages */
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

private:
	TArray<TSoftObjectPtr<UCustomizableObjectExtensionDataContainer>> SavedStreamedExtensionData;
	TArray<FString> SavedContainerNames;
};
