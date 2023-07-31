// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeImportTestStepReimport.h"
#include "InterchangeImportTestData.h"
#include "InterchangeTestFunction.h"
#include "UObject/SavePackage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeImportTestStepReimport)


TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr> UInterchangeImportTestStepReimport::StartStep(FInterchangeImportTestData& Data)
{
	// Find the object we wish to reimport
	TArray<UObject*> PotentialObjectsToReimport;
	PotentialObjectsToReimport.Reserve(Data.ResultObjects.Num());

	for (UObject* ResultObject : Data.ResultObjects)
	{
		if (ResultObject->GetClass() == AssetTypeToReimport.Get())
		{
			PotentialObjectsToReimport.Add(ResultObject);
		}
	}

	UObject* AssetToReimport = nullptr;

	if (PotentialObjectsToReimport.Num() == 1)
	{
		AssetToReimport = PotentialObjectsToReimport[0];
	}
	else if (PotentialObjectsToReimport.Num() > 1 && !AssetNameToReimport.IsEmpty())
	{
		for (UObject* Object : PotentialObjectsToReimport)
		{
			if (Object->GetName() == AssetNameToReimport)
			{
				AssetToReimport = Object;
				break;
			}
		}
	}

	if (AssetToReimport == nullptr)
	{
		return {nullptr, nullptr};
	}

	// Start the Interchange import
	UE::Interchange::FScopedSourceData ScopedSourceData(SourceFileToReimport.FilePath);

	FImportAssetParameters Params;
	Params.bIsAutomated = true;
	Params.ReimportAsset = AssetToReimport;

	UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
	UE::Interchange::FAssetImportResultPtr AssetImportResult = InterchangeManager.ImportAssetAsync(Data.DestAssetPackagePath, ScopedSourceData.GetSourceData(), Params);
	return {AssetImportResult, nullptr};
}


FTestStepResults UInterchangeImportTestStepReimport::FinishStep(FInterchangeImportTestData& Data, FAutomationTestExecutionInfo& ExecutionInfo)
{
	FTestStepResults Results;

	// If we need to perform a save and fresh reload of everything we imported, do it here
	if (bSaveThenReloadImportedAssets)
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

			// Renaming the original objects avoids having to do a GC sweep here.
			// Any existing references to them will be retained but irrelevant.
			// Then the new object can be loaded in their place, as if it were being loaded for the first time.
			const ERenameFlags RenameFlags = REN_ForceNoResetLoaders | REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty;
			PackageObject->Rename(*(PackageObject->GetName() + TEXT("_TRASH")), nullptr, RenameFlags);
			PackageObject->RemoveFromRoot();
			PackageObject->MarkAsGarbage();

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


FString UInterchangeImportTestStepReimport::GetContextString() const
{
	return FString(TEXT("Reimporting ")) + FPaths::GetCleanFilename(SourceFileToReimport.FilePath);
}

