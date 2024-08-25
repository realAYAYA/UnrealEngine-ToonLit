// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkUAssetRecorder.h"

#include "Recording/LiveLinkRecording.h"
#include "Recording/Implementations/LiveLinkUAssetRecording.h"

#include "AssetToolsModule.h"
#include "Containers/Map.h"
#include "ContentBrowserModule.h"
#include "Features/IModularFeatures.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "FileHelpers.h"
#include "InstancedStruct.h"
#include "IContentBrowserSingleton.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubClient.h"
#include "LiveLinkHubModule.h"
#include "LiveLinkTypes.h"
#include "LiveLinkPreset.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UI/Window/LiveLinkHubWindowController.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWidget.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "LiveLinkHub.RecordingController"


namespace UAssetRecorderUtils
{
	TOptional<FLiveLinkRecordingStaticDataContainer> CreateStaticDataContainerFromFrameData(const FLiveLinkSubjectKey& SubjectKey)
	{
		TOptional<FLiveLinkRecordingStaticDataContainer> StaticDataContainer;
		FLiveLinkHubClient* LiveLinkClient = static_cast<FLiveLinkHubClient*>(&IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName));
		TSubclassOf<ULiveLinkRole> LiveLinkRole = LiveLinkClient->GetSubjectRole_AnyThread(SubjectKey);
		
		if (const FLiveLinkStaticDataStruct* StaticData = LiveLinkClient->GetSubjectStaticData(SubjectKey))
		{
			FInstancedStruct StaticDataInstancedStruct;
			StaticDataInstancedStruct.InitializeAs(StaticData->GetStruct(), (uint8*)StaticData->GetBaseData());

			StaticDataContainer = FLiveLinkRecordingStaticDataContainer();
			StaticDataContainer->Role = LiveLinkRole;
			StaticDataContainer->RecordedData.Insert(MoveTemp(StaticDataInstancedStruct), 0);
			StaticDataContainer->Timestamps.Add(0.0);
		}

		return StaticDataContainer;
	}
}

void FLiveLinkUAssetRecorder::StartRecording()
{
	check(!CurrentRecording.IsValid());
	CurrentRecording = MakePimpl<FLiveLinkUAssetRecordingData>();
	RecordInitialStaticData();

	bIsRecording = true;
	TimeRecordingStarted = FPlatformTime::Seconds();
}

void FLiveLinkUAssetRecorder::StopRecording()
{
	if (CurrentRecording)
	{
		bIsRecording = false;

		TimeRecordingEnded = FPlatformTime::Seconds();

		SaveRecording();
		CurrentRecording.Reset();
	}
}

bool FLiveLinkUAssetRecorder::IsRecording() const
{
	return bIsRecording;
}

void FLiveLinkUAssetRecorder::RecordBaseData(FLiveLinkRecordingBaseDataContainer& StaticDataContainer, FInstancedStruct&& DataToRecord)
{
	const double TimeNowInSeconds = FPlatformTime::Seconds();
	StaticDataContainer.RecordedData.Add(MoveTemp(DataToRecord));
	StaticDataContainer.Timestamps.Add(TimeNowInSeconds - TimeRecordingStarted);
}

void FLiveLinkUAssetRecorder::RecordStaticData(const FLiveLinkSubjectKey& SubjectKey, TSubclassOf<ULiveLinkRole> Role, const FLiveLinkStaticDataStruct& StaticData)
{
	if (bIsRecording && CurrentRecording)
	{
		FLiveLinkRecordingStaticDataContainer& StaticDataContainer = CurrentRecording->StaticData.FindOrAdd(SubjectKey);
		FInstancedStruct NewData;
		NewData.InitializeAs(StaticData.GetStruct(), (uint8*)StaticData.GetBaseData());
		StaticDataContainer.Role = Role;

		RecordBaseData(StaticDataContainer, MoveTemp(NewData));
	}
}

void FLiveLinkUAssetRecorder::RecordFrameData(const FLiveLinkSubjectKey& SubjectKey, const FLiveLinkFrameDataStruct& FrameData)
{
	if (bIsRecording && CurrentRecording)
	{
		FLiveLinkRecordingBaseDataContainer& FrameDataContainer = CurrentRecording->FrameData.FindOrAdd(SubjectKey);
		FInstancedStruct NewData;
		NewData.InitializeAs(FrameData.GetStruct(), (uint8*)FrameData.GetBaseData());

		RecordBaseData(FrameDataContainer, MoveTemp(NewData));
	}
}

bool FLiveLinkUAssetRecorder::OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName)
{
	TSharedRef<SWindow> RootWindow = FModuleManager::Get().GetModuleChecked<FLiveLinkHubModule>("LiveLinkHub").GetLiveLinkHub()->GetRootWindow();

	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = InDefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = InNewNameSuggestion;
		SaveAssetDialogConfig.AssetClassNames.Add(ULiveLinkRecording::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveLiveLinkRecordingDialogTitle", "Save LiveLink Recording");
		SaveAssetDialogConfig.WindowOverride = RootWindow;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);

	if (!SaveObjectPath.IsEmpty())
	{
		OutPackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		return true;
	}

	return false;
}

bool FLiveLinkUAssetRecorder::GetSavePresetPackageName(FString& OutName)
{
	FDateTime Today = FDateTime::Now();

	TMap<FString, FStringFormatArg> FormatArgs;
	FormatArgs.Add(TEXT("date"), Today.ToString());

	FString DialogStartPath = TEXT("/Game/LiveLinkRecordings");

	FString DefaultName = LOCTEXT("NewLiveLinkRecordingName", "NewLiveLinkRecording").ToString();

	FString UniquePackageName;
	FString UniqueAssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(DialogStartPath / DefaultName, TEXT(""), UniquePackageName, UniqueAssetName);

	FString DialogStartName = FPaths::GetCleanFilename(UniqueAssetName);

	FString UserPackageName;
	FString NewPackageName;

	// Get destination for asset
	bool bFilenameValid = false;
	while (!bFilenameValid)
	{
		if (!OpenSaveDialog(DialogStartPath, DialogStartName, UserPackageName))
		{
			return false;
		}

		NewPackageName = FString::Format(*UserPackageName, FormatArgs);

		FText OutError;
		bFilenameValid = FFileHelper::IsFilenameValidForSaving(NewPackageName, OutError);
	}

	OutName = MoveTemp(NewPackageName);
	return true;
}

void FLiveLinkUAssetRecorder::SaveRecording()
{
	FString PackageName;
	if (!GetSavePresetPackageName(PackageName))
	{
		return;
	}

	// Saving into a new package
	const FString NewAssetName = FPackageName::GetLongPackageAssetName(PackageName);
	UPackage* NewPackage = CreatePackage(*PackageName);

	ULiveLinkUAssetRecording* NewRecording = NewObject<ULiveLinkUAssetRecording>(NewPackage, *NewAssetName, RF_Public | RF_Standalone);
	if (NewRecording)
	{
		NewRecording->LengthInSeconds = TimeRecordingEnded - TimeRecordingStarted;
		NewRecording->RecordingPreset->BuildFromClient();
		NewRecording->RecordingData = MoveTemp(*CurrentRecording);
		NewRecording->MarkPackageDirty();

		//FAssetRegistryModule::AssetCreated(NewPreset);  Disabled for now since unsure if needed.
		UEditorLoadingAndSavingUtils::SavePackages({ NewPackage }, false);
	}

	return;
}

void FLiveLinkUAssetRecorder::RecordInitialStaticData()
{
	FLiveLinkHubClient* LiveLinkClient = static_cast<FLiveLinkHubClient*>(&IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName));
	TArray<FLiveLinkSubjectKey> Subjects = LiveLinkClient->GetSubjects(true, true);

	for (const FLiveLinkSubjectKey& Subject : Subjects)
	{
		TOptional<FLiveLinkRecordingStaticDataContainer> StaticDataContainer = UAssetRecorderUtils::CreateStaticDataContainerFromFrameData(Subject);
		if (StaticDataContainer)
		{
			CurrentRecording->StaticData.Add(Subject, MoveTemp(*StaticDataContainer));
		}
	}
}

#undef LOCTEXT_NAMESPACE
