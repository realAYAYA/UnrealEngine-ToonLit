// Copyright Epic Games, Inc. All Rights Reserved.
#include "CinePrestreamingEditorSubsystem.h"
#include "CinePrestreamingData.h"

#include "MoviePipelineMasterConfig.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelinePIEExecutor.h"
#include "CinePrestreamingRecorderSetting.h"
#include "MoviePipelineQueueSubsystem.h"

#include "ObjectTools.h"
#include "PackageTools.h"
#include "PackageHelperFunctions.h"
#include "PackageTools.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"


void UCinePrestreamingEditorSubsystem::GeneratePrestreamingAsset(const FCinePrestreamingGenerateAssetArgs& InArgs)
{
	if(!ensureMsgf(!IsRendering(), TEXT("GeneratePrestreamingAsset cannot be called while already generating assets!")))
	{
		FFrame::KismetExecutionMessage(TEXT("Asset generation is already in progress."), ELogVerbosity::Error);
		return;
	}

	UMoviePipelineQueue* Queue = NewObject<UMoviePipelineQueue>(this);
	UMoviePipelineExecutorJob* Job = Queue->AllocateNewJob(UMoviePipelineExecutorJob::StaticClass());
	Job->Sequence = InArgs.Sequence;
	Job->Map = InArgs.Map;
	
	UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(Job->GetConfiguration()->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
	OutputSetting->OutputResolution = InArgs.Resolution;
	
	// Add the deferred renderer setting
	Job->GetConfiguration()->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass());
	
	// Add our cinematic prestreaming setting to record the requests.
	UCinePrestreamingRecorderSetting* PrestreamingSetting = Cast<UCinePrestreamingRecorderSetting>(Job->GetConfiguration()->FindOrAddSettingByClass(UCinePrestreamingRecorderSetting::StaticClass()));
	if (InArgs.OutputDirectoryOverride.Path.Len() > 0)
	{
		PrestreamingSetting->PackageDirectory.Path = InArgs.OutputDirectoryOverride.Path;
	}
	
	// No need to add a output like PNG as we don't want to save the rendered images (they aren't
	// very good looking anyways due to having stuff turned off for perf reasons) and saving them is slow.
	ActiveAssetArgs = InArgs;
	
	ActiveExecutor = NewObject<UMoviePipelinePIEExecutor>(this);
	ActiveExecutor->OnExecutorFinished().AddUObject(this, &UCinePrestreamingEditorSubsystem::OnBuildPrestreamingComplete);
	ActiveExecutor->Execute(Queue);

	UMoviePipelineQueueSubsystem* Subsystem = GEditor->GetEditorSubsystem<UMoviePipelineQueueSubsystem>();
	check(Subsystem);

	// ToDo:
	// The subsystem only renders the internal queue, but we want to use the subsystem so it appropriately locks the UI out.
	// OriginalQueue->CopyFrom(Subsystem->GetQueue());
	// Subsystem->GetQueue()->CopyFrom(Queue);
	// Subsystem->RenderQueueWithExecutorInstance(LocalExecutor);
}

void UCinePrestreamingEditorSubsystem::CreatePackagesFromGeneratedData(TArray<FMoviePipelineCinePrestreamingGeneratedData>& InOutData)
{
	for (FMoviePipelineCinePrestreamingGeneratedData& Data : InOutData)
	{
		const FString FixedAssetName = ObjectTools::SanitizeObjectName(Data.AssetName);
		FString NewPackageName = FPackageName::GetLongPackagePath(Data.PackagePath) + TEXT("/") + FixedAssetName;

		NewPackageName = UPackageTools::SanitizePackageName(NewPackageName);

		FText OutErrorReason;
		if (!FPackageName::IsValidLongPackageName(NewPackageName, false, &OutErrorReason))
		{
			return;
		}

		if (UPackage* OldPackage = FindObject<UPackage>(nullptr, *NewPackageName))
		{
			FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass(), "DEAD_CinePrestreamingSettingAsset");
			OldPackage->Rename(*UniqueName.ToString());
			OldPackage->SetFlags(RF_Transient);
		}

		UPackage* NewPackage = CreatePackage(*NewPackageName);
		NewPackage->AddToRoot();

		// Try to fully load the package (if it already exists) so we can save over it.
		LoadPackage(NewPackage, *NewPackageName, LOAD_None);

		// Duplicate the data asset into this package
		UCinePrestreamingData* NewPrestreamingData = Cast<UCinePrestreamingData>(StaticDuplicateObject(Data.StreamingData, NewPackage, FName(*FixedAssetName), RF_NoFlags));
		NewPrestreamingData->SetFlags(RF_Public | RF_Transactional | RF_Standalone);
		NewPrestreamingData->MarkPackageDirty();

		Data.StreamingData = NewPrestreamingData;

		// Mark it so it shows up in the Content Browser immediately
		FAssetRegistryModule::AssetCreated(NewPrestreamingData);

		// If they want to save, ask them to save (and add to version control)
		TArray<UPackage*> Packages;
		Packages.Add(NewPrestreamingData->GetOutermost());

		UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
	}
}

void UCinePrestreamingEditorSubsystem::OnBuildPrestreamingComplete(UMoviePipelineExecutorBase* InPipelineExecutor, bool bSuccess)
{
	OnAssetGenerated.Broadcast(ActiveAssetArgs.GetValue());
	ActiveAssetArgs.Reset();
	ActiveExecutor = nullptr;
}