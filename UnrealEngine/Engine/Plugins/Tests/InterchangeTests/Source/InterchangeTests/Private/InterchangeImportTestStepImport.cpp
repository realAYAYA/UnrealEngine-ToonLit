// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeImportTestStepImport.h"
#include "InterchangeImportTestData.h"
#include "InterchangeTestFunction.h"
#include "UObject/SavePackage.h"
#include "UObject/ReferenceChainSearch.h"
#include "PackageTools.h"
#include "Editor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeImportTestStepImport)


TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr> UInterchangeImportTestStepImport::StartStep(FInterchangeImportTestData& Data)
{
	// Empty the destination folder here if requested
	if (bEmptyDestinationFolderPriorToImport)
	{
		Data.ResultObjects.Empty();
		Data.ImportedAssets.Empty();

		const bool bRequireExists = true;
		const bool bDeleteRecursively = true;
		IFileManager::Get().DeleteDirectory(*Data.DestAssetFilePath, bRequireExists, bDeleteRecursively);

		const bool bAddRecursively = true;
		IFileManager::Get().MakeDirectory(*Data.DestAssetFilePath, bAddRecursively);

		// @todo: Is there a better way of deleting all the files inside a directory without also deleting the directory?
	}

	// Start the Interchange import
	UE::Interchange::FScopedSourceData ScopedSourceData(SourceFile.FilePath);

	FImportAssetParameters Params;
	Params.OverridePipelines = PipelineStack;
	Params.bIsAutomated = true;

	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();

	if (bImportIntoLevel)
	{
		return InterchangeManager.ImportSceneAsync(Data.DestAssetPackagePath, ScopedSourceData.GetSourceData(), Params);
	}
	else
	{
		UE::Interchange::FAssetImportResultPtr AssetImportResult = InterchangeManager.ImportAssetAsync(Data.DestAssetPackagePath, ScopedSourceData.GetSourceData(), Params);
		return {AssetImportResult, nullptr};
	}
}


FTestStepResults UInterchangeImportTestStepImport::FinishStep(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo)
{
	FTestStepResults Results;

	// If we need to perform a save and fresh reload of everything we imported, do it here
	// Incompatible with bImportIntoLevel for now, since we'd need to patch all references to the assets
	if (bSaveThenReloadImportedAssets && !bImportIntoLevel)
	{
		// First save
		for (const FAssetData& AssetData : Data.ImportedAssets)
		{
			const bool bLoadAsset = false;
			UObject* AssetObject = AssetData.FastGetAsset(bLoadAsset);
			UPackage* PackageObject = AssetObject->GetPackage();
			check(PackageObject);

			AssetObject->MarkPackageDirty();

			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			UPackage::SavePackage(PackageObject, AssetObject,
				*FPackageName::LongPackageNameToFilename(AssetData.PackageName.ToString(), FPackageName::GetAssetPackageExtension()),
				SaveArgs);
		}

		// Then rename original objects and their packages, and mark as garbage
		for (const FAssetData& AssetData : Data.ImportedAssets)
		{
			const bool bLoadAsset = false;
			UObject* AssetObject = AssetData.FastGetAsset(bLoadAsset);
			UPackage* PackageObject = AssetObject->GetPackage();
			check(PackageObject);
			check(PackageObject == AssetData.GetPackage());

			// Mark all objects in the package as garbage, and remove the standalone flag, so that GC can remove the package later
			TArray<UObject*> ObjectsInPackage;
			GetObjectsWithPackage(PackageObject, ObjectsInPackage, true);
			for (UObject* ObjectInPackage : ObjectsInPackage)
			{
				ObjectInPackage->ClearFlags(RF_Standalone | RF_Public);
				ObjectInPackage->MarkAsGarbage();
			}

			// Renaming the original objects avoids having to do a GC sweep here (this is done at the end of each test step)
			// Any existing references to them will be retained but irrelevant.
			// Then the new object can be loaded in their place, as if it were being loaded for the first time.
			const ERenameFlags RenameFlags = REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty;
			PackageObject->Rename(*(PackageObject->GetName() + TEXT("_TRASH")), nullptr, RenameFlags);
			PackageObject->RemoveFromRoot();
			PackageObject->MarkAsGarbage();

			// Remove the old version of the asset object from the results
			Data.ResultObjects.Remove(AssetObject);
		}

		// Now reload
		for (const FAssetData& AssetData : Data.ImportedAssets)
		{
			check(!AssetData.IsAssetLoaded());
			UObject* AssetObject = AssetData.GetAsset();
			check(AssetObject);
			Data.ResultObjects.Add(AssetObject);
		}
	}

	// Run all the tests
	bool bSuccess = PerformTests(Data, ExecutionInfo);

	Results.bTestStepSuccess = bSuccess;
	return Results;
}


FString UInterchangeImportTestStepImport::GetContextString() const
{
	return FString(TEXT("Importing ")) + FPaths::GetCleanFilename(SourceFile.FilePath);
}

