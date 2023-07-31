// Copyright Epic Games, Inc. All Rights Reserved.

#include "MoviePipelineEditorBlueprintLibrary.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "MoviePipelineQueue.h"
#include "MoviePipelineQueueSubsystem.h"
#include "MovieRenderPipelineSettings.h"
#include "Misc/MessageDialog.h"
#include "PackageHelperFunctions.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Editor.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "SequencerUtilities.h"
#include "MoviePipelineBlueprintLibrary.h"
#include "MoviePipelineOutputSetting.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MoviePipelineEditorBlueprintLibrary)

#define LOCTEXT_NAMESPACE "MoviePipelineEditorBlueprintLibrary"

bool UMoviePipelineEditorBlueprintLibrary::ExportConfigToAsset(const UMoviePipelineMasterConfig* InConfig, const FString& InPackagePath, const FString& InFileName, const bool bInSaveAsset, UMoviePipelineMasterConfig*& OutAsset, FText& OutErrorReason)
{
	if(!InConfig)
	{
		OutErrorReason = LOCTEXT("CantExportNullConfigToPackage", "Can't export a null configuration to a package.");
		return false;
	}
	
	FString FixedAssetName = ObjectTools::SanitizeObjectName(InFileName);
	FString NewPackageName = FPackageName::GetLongPackagePath(InPackagePath) + TEXT("/") + FixedAssetName;

	if (!FPackageName::IsValidLongPackageName(NewPackageName, false, &OutErrorReason))
	{
		return false;
	}

	UPackage* NewPackage = CreatePackage(*NewPackageName);
	NewPackage->MarkAsFullyLoaded();
	NewPackage->AddToRoot();
	
	// Duplicate the provided config into this package.
	UMoviePipelineMasterConfig* NewConfig = Cast<UMoviePipelineMasterConfig>(StaticDuplicateObject(InConfig, NewPackage, FName(*InFileName), RF_NoFlags));
	NewConfig->SetFlags(RF_Public | RF_Transactional | RF_Standalone);
	NewConfig->MarkPackageDirty();

	// Mark it so it shows up in the Content Browser immediately
	FAssetRegistryModule::AssetCreated(NewConfig);

	// If they want to save, ask them to save (and add to version control)
	if (bInSaveAsset)
	{
		TArray<UPackage*> Packages;
		Packages.Add(NewConfig->GetOutermost());

		return UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
	}

	return true;
}

bool UMoviePipelineEditorBlueprintLibrary::IsMapValidForRemoteRender(const TArray<UMoviePipelineExecutorJob*>& InJobs)
{
	for (const UMoviePipelineExecutorJob* Job : InJobs)
	{
		FString PackageName = Job->Map.GetLongPackageName();
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			return false;
		}
	}
	return true;
}

void UMoviePipelineEditorBlueprintLibrary::WarnUserOfUnsavedMap()
{
	FText FailureReason = LOCTEXT("UnsavedMapFailureDialog", "One or more jobs in the queue have an unsaved map as their target map. These unsaved maps cannot be loaded by an external process, and the render has been aborted.");
	FMessageDialog::Open(EAppMsgType::Ok, FailureReason);
}

UMoviePipelineQueue* UMoviePipelineEditorBlueprintLibrary::SaveQueueToManifestFile(UMoviePipelineQueue* InPipelineQueue, FString& OutManifestFilePath)
{
	FString InFileName = TEXT("QueueManifest");
	FString InPackagePath = TEXT("/Engine/MovieRenderPipeline/Editor/Transient");

	FString FixedAssetName = ObjectTools::SanitizeObjectName(InFileName);
	FString NewPackageName = FPackageName::GetLongPackagePath(InPackagePath) + TEXT("/") + FixedAssetName;

	// If there's already a package with this name, rename it so that the newly created one can always get a fixed name.
	// The fixed name is important because in the new process it'll start the unique name count over.
	if (UPackage* OldPackage = FindObject<UPackage>(nullptr, *NewPackageName))
	{
		FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UPackage::StaticClass(), "DEAD_NewProcessExecutor_SerializedPackage");
		OldPackage->Rename(*UniqueName.ToString());
		OldPackage->SetFlags(RF_Transient);
	}

	UPackage* NewPackage = CreatePackage(*NewPackageName);

	// Duplicate the Queue into this package as we don't want to just rename the existing that belongs to the editor subsystem.
	UMoviePipelineQueue* DuplicatedQueue = CastChecked<UMoviePipelineQueue>(StaticDuplicateObject(InPipelineQueue, NewPackage));
	DuplicatedQueue->SetFlags(RF_Public | RF_Transactional | RF_Standalone);

	// Save the package to disk.
	FString ManifestFileName = TEXT("MovieRenderPipeline/QueueManifest") + FPackageName::GetTextAssetPackageExtension();
	OutManifestFilePath = FPaths::ProjectSavedDir() / ManifestFileName;

	// Fully load the package before trying to save.
	LoadPackage(NewPackage, *NewPackageName, LOAD_None);

	{
		UEditorLoadingSavingSettings* SaveSettings = GetMutableDefault<UEditorLoadingSavingSettings>();
		uint32 bSCCAutoAddNewFiles = SaveSettings->bSCCAutoAddNewFiles;
		SaveSettings->bSCCAutoAddNewFiles = 0;
		bool bSuccess = SavePackageHelper(NewPackage, *OutManifestFilePath);
		SaveSettings->bSCCAutoAddNewFiles = bSCCAutoAddNewFiles;
		
		if(!bSuccess)
		{
			return nullptr;
		}
	}

	NewPackage->SetFlags(RF_Transient);
	NewPackage->ClearFlags(RF_Standalone);
	DuplicatedQueue->SetFlags(RF_Transient);
	DuplicatedQueue->ClearFlags(RF_Public | RF_Transactional | RF_Standalone);

	return DuplicatedQueue;
}

FString UMoviePipelineEditorBlueprintLibrary::ConvertManifestFileToString(const FString& InManifestFilePath)
{
	// Due to API limitations we can't convert package -> text directly and instead need to re-load it, escape it, and then put it onto the command line :-)
	FString OutString;
	FFileHelper::LoadFileToString(OutString, *InManifestFilePath);

	return OutString;
}

UMoviePipelineExecutorJob* UMoviePipelineEditorBlueprintLibrary::CreateJobFromSequence(UMoviePipelineQueue* InPipelineQueue, const ULevelSequence* InSequence)
{
	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();

	InPipelineQueue->Modify();

	UMoviePipelineExecutorJob* NewJob = InPipelineQueue->AllocateNewJob(ProjectSettings->DefaultExecutorJob.TryLoadClass<UMoviePipelineExecutorJob>());
	if (!ensureAlwaysMsgf(NewJob, TEXT("Failed to allocate new job! Check the DefaultExecutorJob is not null in Project Settings!")))
	{
		return nullptr;
	}

	NewJob->Modify();

	TArray<FString> AssociatedMaps = FSequencerUtilities::GetAssociatedMapPackages(InSequence);
	FSoftObjectPath CurrentWorld;

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;

	// We'll assume they went to render from the current world if it's been saved.
	if (EditorWorld && (!EditorWorld->GetOutermost()->GetPathName().StartsWith(TEXT("/Temp/Untitled")) || AssociatedMaps.Num() == 0))
	{
		CurrentWorld = FSoftObjectPath(EditorWorld);
	}
	else if (AssociatedMaps.Num() > 0)
	{
		// So associated maps are only packages and not assets, but FSoftObjectPath needs assets.
		// We know that they are map packages, and map packages should be /Game/Foo.Foo, so we can
		// just do some string manipulation here as there isn't a generic way to go from Package->Object.
		FString MapPackage = AssociatedMaps[0];
		MapPackage = FString::Printf(TEXT("%s.%s"), *MapPackage, *FPackageName::GetShortName(MapPackage));

		CurrentWorld = FSoftObjectPath(MapPackage);
	}

	// Job author is intentionally left blank so that it doesn't get saved into queues. It will
	// be resolved into a username when the Movie Pipeline starts if it is blank.
	FSoftObjectPath Sequence(InSequence);
	NewJob->Map = CurrentWorld;
	NewJob->SetSequence(Sequence);
	NewJob->JobName = NewJob->Sequence.GetAssetName();

	return NewJob;
}

void UMoviePipelineEditorBlueprintLibrary::EnsureJobHasDefaultSettings(UMoviePipelineExecutorJob* NewJob)
{
	const UMovieRenderPipelineProjectSettings* ProjectSettings = GetDefault<UMovieRenderPipelineProjectSettings>();
	for (FSoftClassPath SettingClassPath : ProjectSettings->DefaultClasses)
	{
		TSubclassOf<UMoviePipelineSetting> SettingClass = SettingClassPath.TryLoadClass<UMoviePipelineSetting>();
		if (!SettingClass)
		{
			continue;
		}

		if (SettingClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}

		const bool bIncludeDisabledSettings = true;
		UMoviePipelineSetting* ExistingSetting = NewJob->GetConfiguration()->FindSettingByClass(SettingClass, bIncludeDisabledSettings);
		if (!ExistingSetting)
		{
			NewJob->GetConfiguration()->FindOrAddSettingByClass(SettingClass);
		}
	}
}

FString UMoviePipelineEditorBlueprintLibrary::ResolveOutputDirectoryFromJob(UMoviePipelineExecutorJob* InJob)
{
	UMoviePipelineOutputSetting* OutputSetting = InJob->GetConfiguration()->FindSetting<UMoviePipelineOutputSetting>();
	check(OutputSetting);

	// Set up as many parameters as we can to try and resolve most of the string.
	FString FormatString = OutputSetting->OutputDirectory.Path / OutputSetting->FileNameFormat;

	// If they've set up any folders within the filename portion of it, let's be nice and resolve that.
	FPaths::NormalizeFilename(FormatString);
	int32 LastSlashIndex;
	if (FormatString.FindLastChar(TEXT('/'), LastSlashIndex))
	{
		FormatString.LeftInline(LastSlashIndex + 1);
	}

	// By having it swap {camera_name} and {shot_name} with an unresolvable tag, it will
	// stay in the resolved path and can be removed using the code below.
	static const FString DummyTag = TEXT("{dontresolvethis}");
	FMoviePipelineFilenameResolveParams Params;
	Params.Job = InJob;
	Params.ShotNameOverride = DummyTag;
	Params.CameraNameOverride = DummyTag;
	Params.InitializationTime = FDateTime::UtcNow();

	FString OutResolvedPath;
	FMoviePipelineFormatArgs Dummy;
	UMoviePipelineBlueprintLibrary::ResolveFilenameFormatArguments(FormatString, Params, OutResolvedPath, Dummy);

	// Drop the .{ext} resolving always puts on.
	OutResolvedPath.LeftChopInline(6);

	if (FPaths::IsRelative(OutResolvedPath))
	{
		OutResolvedPath = FPaths::ConvertRelativePathToFull(OutResolvedPath);
	}

	// In the event that they used a {format_string} we couldn't resolve (such as shot name), then
	// we'll trim off anything after the format string.
	int32 FormatStringToken;
	if (OutResolvedPath.FindChar(TEXT('{'), FormatStringToken))
	{
		// Just as a last bit of saftey, we'll trim anything between the { and the preceeding /. This is
		// in case they did something like Render_{Date}, we wouldn't want to make a folder named Render_.
		// We search backwards from where we found the first { brace, so that will get us the last usable slash.
		LastSlashIndex = OutResolvedPath.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd, FormatStringToken);
		if (LastSlashIndex != INDEX_NONE)
		{
			OutResolvedPath.LeftInline(LastSlashIndex + 1);
		}
	}

	return OutResolvedPath;
}


#undef LOCTEXT_NAMESPACE // "MoviePipelineEditorBlueprintLibrary"
