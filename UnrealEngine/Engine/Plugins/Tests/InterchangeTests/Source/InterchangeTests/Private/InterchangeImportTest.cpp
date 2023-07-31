// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/WildcardString.h"
#include "Misc/AutomationTest.h"
#include "InterchangeTestsLog.h"
#include "ImportTestFunctions/StaticMeshImportTestFunctions.h"
#include "InterchangeDispatcher.h"
#include "InterchangeImportTestData.h"
#include "InterchangeImportTestSettings.h"
#include "InterchangeImportTestPlan.h"
#include "InterchangeImportTestStepBase.h"
#include "InterchangeManager.h"
#include "InterchangeTestsModule.h"
#include "UObject/StrongObjectPtr.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Editor.h"
#include "Editor/Transactor.h"

#include "UObject/SavePackage.h"
#include "ObjectTools.h"

#include "Engine/StaticMesh.h"
#include "InterchangeGenericAssetsPipeline.h"


IMPLEMENT_COMPLEX_AUTOMATION_TEST(FInterchangeImportTest, "Editor.Interchange.Import", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)


void FInterchangeImportTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	// For now, we can't run Interchange automation tests on Mac, as there's a problem with the InterchangeWorker. For now, do nothing on Mac.
	// @todo: find a solution to this
#if PLATFORM_MAC || PLATFORM_LINUX
	return;
#endif

	//Make sure interchange worker is available, do not run the test if unavailable
	if (!UE::Interchange::FInterchangeDispatcher::IsInterchangeWorkerAvailable())
	{
		return;
	}

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AllTestPlans;
	AssetRegistryModule.Get().GetAssetsByClass(UInterchangeImportTestPlan::StaticClass()->GetClassPathName(), AllTestPlans, true);

	// Get a list of all paths containing InterchangeTestPlan assets
	TSet<FName> Paths;
	for (const FAssetData& TestPlan : AllTestPlans)
	{
		Paths.Add(TestPlan.PackagePath);
	}

	// And create a sorted list from the set.
	// Each unique path will be a sub-entry in the automated test list.
	// All test plans within a given folder will be executed in parallel.

	Paths.Sort([](const FName& A, const FName& B) { return A.ToString() < B.ToString(); });

	for (const FName& Path : Paths)
	{
		FString PathAsString = Path.ToString();
		OutTestCommands.Add(PathAsString);
		OutBeautifiedNames.Add(FPaths::GetBaseFilename(PathAsString));
	}
}


bool FInterchangeImportTest::RunTest(const FString& Path)
{
	// For now, we can't run Interchange automation tests on Mac, as there's a problem with the InterchangeWorker. For now, do nothing on Mac.
	// @todo: find a solution to this
#if PLATFORM_MAC || PLATFORM_LINUX
	return true;
#endif

	// Determine the test plan assets within the given path which will be run in parallel

	TArray<FInterchangeImportTestData> TestPlans;

	if (Path.Contains(TEXT(".")))
	{
		// Run test on a single TestPlan asset
		FName PackageName = FName(FPaths::GetBaseFilename(Path, false));
		FName PackagePath = FName(FPaths::GetPath(Path));
		FName AssetName = FName(FPaths::GetBaseFilename(Path, true));
		FTopLevelAssetPath ClassName = UInterchangeImportTestPlan::StaticClass()->GetClassPathName();
		FInterchangeImportTestData& TP = TestPlans.AddDefaulted_GetRef();
		TP.AssetData = FAssetData(PackageName, PackagePath, AssetName, ClassName);
		TP.TestPlan = CastChecked<UInterchangeImportTestPlan>(TP.AssetData.GetAsset());
	}
	else
	{
		// Run tests in parallel on all TestPlan assets in the given directory
		FARFilter AssetRegistryFilter;
		AssetRegistryFilter.PackagePaths.Add(FName(Path));
		AssetRegistryFilter.ClassPaths.Add(UInterchangeImportTestPlan::StaticClass()->GetClassPathName());
		AssetRegistryFilter.bRecursivePaths = false;

		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

		AssetRegistryModule.Get().EnumerateAssets(AssetRegistryFilter,
			[this, &TestPlans](const FAssetData& AssetData)
			{
				UInterchangeImportTestPlan* AssetObject = CastChecked<UInterchangeImportTestPlan>(AssetData.GetAsset());

				if (AssetObject->bIsEnabledInAutomationTests)
				{
					FInterchangeImportTestData& TP = TestPlans.AddDefaulted_GetRef();
					TP.AssetData = AssetData;
					TP.TestPlan = AssetObject;
				}
				else
				{
					FString InfoMessage;
					if (!AssetObject->DisabledTestReason.IsEmpty())
					{
						InfoMessage = FString::Printf(TEXT("InterchangeImportTestPlan %s is disabled because [%s]."), *AssetObject->GetName(), *AssetObject->DisabledTestReason);
						
					}
					else
					{
						InfoMessage = FString::Printf(TEXT("InterchangeImportTestPlan %s is disabled."), *AssetObject->GetName());
					}
					//Add the disable test as an informative event
					ExecutionInfo.AddEvent(FAutomationEvent(EAutomationEventType::Info, InfoMessage));
				}

				return true;
			}
		);
	}

	// Base path to import assets to

	FString SubDirToUse = TEXT("Interchange/ImportTest/");
	FString BasePackagePath = FString(TEXT("/Temp/")) / SubDirToUse;
	FString BaseFilePath = FPaths::ProjectSavedDir() / SubDirToUse;

	// Clear out the folder contents before we do anything else

	const bool bRequireExists = false;
	const bool bDeleteRecursively = true;
	IFileManager::Get().DeleteDirectory(*BaseFilePath, bRequireExists, bDeleteRecursively);

	TArray<TTuple<UE::Interchange::FAssetImportResultPtr, UE::Interchange::FSceneImportResultPtr>> Results;

	int32 StepIndex = 0;
	bool bFoundValidSteps = true;
	bool bSuccess = true;

	while (bFoundValidSteps)
	{
		Results.Reset();
		Results.SetNum(TestPlans.Num());

		bFoundValidSteps = false;

		// Start parallel import tasks in the interchange manager

		for (int32 PlanIndex = 0; PlanIndex < TestPlans.Num(); PlanIndex++)
		{
			FInterchangeImportTestData& Data = TestPlans[PlanIndex];
			Data.ImportedAssets.Empty();

			FString AssetName = Data.AssetData.AssetName.ToString().Replace(TEXT("/"), TEXT("_"));
			Data.DestAssetPackagePath = BasePackagePath / AssetName;
			Data.DestAssetFilePath = BaseFilePath / AssetName;

			const bool bAddRecursively = true;
			IFileManager::Get().MakeDirectory(*Data.DestAssetFilePath, bAddRecursively);

			if (Data.TestPlan)
			{
				if (Data.TestPlan->Steps.IsValidIndex(StepIndex))
				{
					bFoundValidSteps = true;
					Results[PlanIndex] = Data.TestPlan->Steps[StepIndex]->StartStep(Data);
				}
			}
		}

		if (bFoundValidSteps)
		{
			// If we started any test steps, join all the threads here, before proceeding with operations which have to be performed
			// on the main thread, and which could potentially trigger GC.

			for (int32 PlanIndex = 0; PlanIndex < TestPlans.Num(); PlanIndex++)
			{
				if (Results[PlanIndex].Get<0>())
				{
					Results[PlanIndex].Get<0>()->WaitUntilDone();
				}

				if (Results[PlanIndex].Get<1>())
				{
					Results[PlanIndex].Get<1>()->WaitUntilDone();
				}
			}

			// Start post-steps

			for (int32 PlanIndex = 0; PlanIndex < TestPlans.Num(); PlanIndex++)
			{
				FInterchangeImportTestData& Data = TestPlans[PlanIndex];

				if (Data.TestPlan)
				{
					if (Data.TestPlan->Steps.IsValidIndex(StepIndex))
					{
						FString Context = Data.AssetData.AssetName.ToString() + TEXT(": ") + Data.TestPlan->Steps[StepIndex]->GetContextString();
						ExecutionInfo.PushContext(Context);

						// Fill out list of result objects in the data object.
						// These are the UObject* results corresponding to imported assets.
						if (Results[PlanIndex].Get<0>())
						{
							for (UObject* ImportedObject : Results[PlanIndex].Get<0>()->GetImportedObjects())
							{
								Data.ResultObjects.AddUnique(ImportedObject);
							}

							// Also add the InterchangeResultsContainer to the data so that tests can be run on it
							// (e.g. to check whether something imported with a specific expected error)
							Data.InterchangeResults = Results[PlanIndex].Get<0>()->GetResults();

							// Fill out list of imported assets as FAssetData
							for (UObject* Object : Results[PlanIndex].Get<0>()->GetImportedObjects())
							{
								Data.ImportedAssets.Emplace(Object);
							}
						}

						if (Results[PlanIndex].Get<1>())
						{
							for (UObject* ImportedObject : Results[PlanIndex].Get<1>()->GetImportedObjects())
							{
								Data.ResultObjects.AddUnique(ImportedObject);
							}

							if (Data.InterchangeResults)
							{
								Data.InterchangeResults->Append(Results[PlanIndex].Get<1>()->GetResults());
							}
							else
							{
								Data.InterchangeResults = Results[PlanIndex].Get<1>()->GetResults();
							}
						}

						// We don't need the Interchange results object any more, as we've already taken everything from it that we need.
						// If we don't reset it, it will hold on to the trashed versions of the imported assets during GC.
						Results[PlanIndex] = {};

						Data.TestPlan->Steps[StepIndex]->FinishStep(Data, ExecutionInfo);

						if (Data.InterchangeResults)
						{
							// Populate the automation test execution info with the interchange import results
							for (UInterchangeResult* Result : Data.InterchangeResults->GetResults())
							{
								switch (Result->GetResultType())
								{
								case EInterchangeResultType::Error:
									ExecutionInfo.AddError(Result->GetText().ToString());
									bSuccess = false;
									break;

								case EInterchangeResultType::Warning:
									ExecutionInfo.AddWarning(Result->GetText().ToString());
									break;
								}
							}
						}

						// Finished with the interchange results - null it so it will be GC'd later
						Data.InterchangeResults = nullptr;

						ExecutionInfo.PopContext();
					}
				}
			}

			// Collect garbage between every step, so that we remove renamed packages which come from the save+reload operation
			// Note we also reset the transaction buffer here to stop it from holding onto references which would prevent garbage collection.
			// @todo: not really a big fan of this; is there a better way of just disabling transactions?
			if ((GEditor != nullptr) && (GEditor->Trans != nullptr))
			{
				GEditor->Trans->Reset(FText::FromString("Discard undo history during FBX Automation testing."));
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		StepIndex++;
	}

	TArray<UObject*> ObjectsToDelete;
	for (FInterchangeImportTestData& Data : TestPlans)
	{
		ObjectsToDelete.Reserve(ObjectsToDelete.Max() + Data.ResultObjects.Num());

		for (UObject* ResultObject : Data.ResultObjects)
		{
			if (AActor* Actor = Cast<AActor>(ResultObject))
			{
				constexpr bool bShouldModifyLevel = false;
				Actor->GetWorld()->RemoveActor(Actor, bShouldModifyLevel);
			}
			else
			{
				ObjectsToDelete.Add(ResultObject);
			}
		}
	}

	constexpr bool bShowConfirmation = false;
	ObjectTools::ForceDeleteObjects(ObjectsToDelete, bShowConfirmation);

	return bSuccess;
}
