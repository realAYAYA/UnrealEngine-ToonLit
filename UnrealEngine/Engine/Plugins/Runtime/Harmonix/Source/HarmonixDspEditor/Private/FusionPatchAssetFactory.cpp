// Copyright Epic Games, Inc. All Rights Reserved.

#include "FusionPatchAssetFactory.h"
#include "FusionPatchImportOptions.h"
#include "EditorDialogLibrary.h"

#include "HarmonixDsp/FusionSampler/FusionPatch.h"
#include "Dta/DtaParser.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "JsonImporterHelper.h"
#include "FusionPatchJsonImporter.h"

#include "Misc/FeedbackContext.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "EditorFramework/AssetImportData.h"
#include "UObject/Package.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "FusionPatchAssetFactory"

DEFINE_LOG_CATEGORY(LogFusionPatchAssetFactory)

UFusionPatchAssetFactory::UFusionPatchAssetFactory()
{
	SupportedClass = UFusionPatch::StaticClass();
	Formats.Add(TEXT("fusion;Fusion Patch"));
	bText = true;
	bCreateNew = false;
	bEditorImport = true;
	ImportPriority = DefaultImportPriority + 20;
}

bool UFusionPatchAssetFactory::FactoryCanImport(const FString& Filename)
{
	return true;
}

bool UFusionPatchAssetFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UFusionPatch* FusionPatch = Cast<UFusionPatch>(Obj);
	if (FusionPatch && FusionPatch->AssetImportData)
	{
		FusionPatch->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UFusionPatchAssetFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UFusionPatch* FusionPatch = Cast<UFusionPatch>(Obj);
	if (FusionPatch && ensure(NewReimportPaths.Num() == 1))
	{
		FusionPatch->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UFusionPatchAssetFactory::Reimport(UObject* Obj)
{
	UFusionPatch* FusionPatch = Cast<UFusionPatch>(Obj);
	if (!FusionPatch)
	{
		return EReimportResult::Failed;
	}

	if (!FusionPatch->AssetImportData)
	{
		return EReimportResult::Failed;
	}

	const FString Filename = FusionPatch->AssetImportData->GetFirstFilename();
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		return EReimportResult::Failed;
	}
	
	bool OutCanceled = false;
	if (UObject* ImportedObject = ImportObject(FusionPatch->GetClass(), FusionPatch->GetOuter(), *FusionPatch->GetName(), RF_Public | RF_Standalone, Filename, nullptr, OutCanceled))
	{
		// The fusion patch should have just been updated, right?
		ensure(ImportedObject == FusionPatch);

		if (FusionPatch->GetOuter())
		{
			FusionPatch->GetOuter()->MarkPackageDirty();
		}
		else
		{
			FusionPatch->MarkPackageDirty();
		}
		return EReimportResult::Succeeded;
	}
	
	if (OutCanceled)
	{
		UE_LOG(LogFusionPatchAssetFactory, Warning, TEXT("import canceled"));
		return EReimportResult::Cancelled;
	}

	UE_LOG(LogFusionPatchAssetFactory, Warning, TEXT("import failed"));
	return EReimportResult::Failed;
}

void UFusionPatchAssetFactory::PostImportCleanUp()
{
	ApplyOptionsToAllImport = false;
	ReplaceExistingSamplesResponse = EAppReturnType::No;
}

bool UFusionPatchAssetFactory::GetReplaceExistingSamplesResponse(const FString& InName)
{
	if (ReplaceExistingSamplesResponse == EAppReturnType::YesAll)
	{
		return true;
	}

	if (ReplaceExistingSamplesResponse == EAppReturnType::NoAll)
	{
		return false;
	}

	const FText ReplaceExistingTitle = NSLOCTEXT("FusionPatchImporter", "ReplaceExistingSamplesTitle", "Replace Existing Samples");
	const FText ReplaceExistingMessage = FText::Format(NSLOCTEXT("FusionPatchImporter", "ReplaceExistingSamplesMsg", 
		"Would you like to reimport and replace existing Sound Wave Assets in the directory with Samples referenced by this Fusion Fatch?" 
		"\n\nPatch Name: {0}"
		"\n\nYes. Reimport and replace existing Samples with the new samples."
		"\n\nNo. New Samples will be imported, but existing Samples will be unchanged. The Fusion Patch will reference any existing Samples in the directory with matching names."), 
		FText::FromString(InName));
	ReplaceExistingSamplesResponse = UEditorDialogLibrary::ShowMessage(ReplaceExistingTitle, ReplaceExistingMessage, EAppMsgType::YesNoYesAllNoAll, ReplaceExistingSamplesResponse, EAppMsgCategory::Info);
	

	switch (ReplaceExistingSamplesResponse)
	{
	case EAppReturnType::Yes:
	case EAppReturnType::YesAll:
		return true;
	case EAppReturnType::No:
	case EAppReturnType::NoAll:
		return false;
	default:
		ensureMsgf(false, TEXT("Unexpected response! default behavior is to NOT replace existing samples"));
		return false;
	}
}

void UFusionPatchAssetFactory::UpdateFusionPatchImportNotificationItem(TSharedPtr<SNotificationItem> InItem, bool bImportSuccessful, FName InName)
{
	if (bImportSuccessful)
	{
		InItem->SetCompletionState(SNotificationItem::ECompletionState::CS_Success);
		InItem->SetText(FText::Format(NSLOCTEXT("FusionPatchImporter", "FusionPatchImportProgressNotification_Success", "Successfully imported Fusion Patch asset: {0}"), FText::FromString(InName.ToString())));
	}
	else {
		InItem->SetCompletionState(SNotificationItem::ECompletionState::CS_Fail);
		InItem->SetText(FText::Format(NSLOCTEXT("FusionPatchImporter", "FusionPatchImportProgressNotification_Failure", "Failed to import Fusion Patch: {0}"), FText::FromString(InName.ToString())));
	}
	InItem->SetExpireDuration(0.2f);
	InItem->ExpireAndFadeout();
}

UObject* UFusionPatchAssetFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	const FString LongPackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetPathName());

	const UFusionPatchImportOptions* ImportOptions = nullptr;
	
	if (!ApplyOptionsToAllImport)
	{
		bool WasOkayPressed = false;
		UFusionPatchImportOptions::FArgs Args;
		Args.Directory = LongPackagePath;
		ImportOptions = UFusionPatchImportOptions::GetWithDialog(MoveTemp(Args), WasOkayPressed);
		if (!WasOkayPressed)
		{
			// import cancelled by user
			return nullptr;
		}
		ApplyOptionsToAllImport = true;
	}
	else
	{
		UFusionPatchImportOptions* MutableOptions = GetMutableDefault<UFusionPatchImportOptions>();
		if (MutableOptions->SamplesImportDir.Path.IsEmpty())
		{
			MutableOptions->SamplesImportDir.Path = LongPackagePath;	
		}
		ImportOptions = MutableOptions;
	}

	if (Warn->ReceivedUserCancel())
	{
		return nullptr;
	}

	const bool ReplaceExistingSamples = GetReplaceExistingSamplesResponse(InName.ToString());
	
	const FString SourceFile = GetCurrentFilename();
	AdditionalImportedObjects.Empty();
	FString JsonString;
	const FString DtaString(BufferEnd - Buffer, Buffer);

	FDtaParser::DtaStringToJsonString(DtaString, JsonString);
	FString ErrorMessage;
	TSharedPtr<FJsonObject> JsonObj = FJsonImporter::ParseJsonString(JsonString, ErrorMessage);
	bool bImportSuccessful = false;
	if (JsonObj.IsValid())
	{
		UFusionPatch* FusionPatch = FindObject<UFusionPatch>(InParent, *InName.ToString());
		if (!FusionPatch)
		{
			FusionPatch = NewObject<UFusionPatch>(InParent, InName, Flags);
		}

		
		const FString SourcePath = FPaths::GetPath(SourceFile);
		
		//create a notification that displays the import progress at the lower right corner
		FNotificationInfo ImportNotificationInfo(NSLOCTEXT("FusionPatchImporter","FusionPatchImportProgressNotification_InProgress","Importing Fusion Asset(s)..."));
		ImportNotificationInfo.bFireAndForget = false;
		TSharedPtr<SNotificationItem> ImportNotificationItem;
		ImportNotificationItem = FSlateNotificationManager::Get().AddNotification(ImportNotificationInfo);


		// Pass import args to parser so it can import sub files
		FFusionPatchJsonImporter::FImportArgs ImportArgs(InName, SourcePath, LongPackagePath, ImportOptions->SamplesImportDir.Path, ReplaceExistingSamples);
		ImportArgs.SampleLoadingBehavior = ImportOptions->SampleLoadingBehavior;
		ImportArgs.SampleCompressionType = ImportOptions->SampleCompressionType;

		TArray<FString> ImportErrors;
		if (FFusionPatchJsonImporter::TryParseJson(JsonObj, FusionPatch, ImportArgs, ImportErrors))
		{
			UE_LOG(LogFusionPatchAssetFactory, Log, TEXT("Successfully imported FusionPatch asset"));

			if (ensure(FusionPatch->AssetImportData))
			{
				FusionPatch->AssetImportData->Update(SourceFile);
			}

			bImportSuccessful = true;
			UpdateFusionPatchImportNotificationItem(ImportNotificationItem, bImportSuccessful, InName);
			return FusionPatch;
		}
		else
		{
			const FText ImportErrorMessage = FText::Format(
				LOCTEXT("ImportFailed_FusionPatchJsonImporter",
					"Failed to import asset:\n'{0}'.\nReasons:\n{1}"),
				FText::FromString(SourceFile),
				FText::FromString(FString::Printf(TEXT("%s"), *FString::Join(ImportErrors, TEXT("\n")))));
			FMessageDialog::Open(EAppMsgType::Ok, ImportErrorMessage);
			UE_LOG(LogFusionPatchAssetFactory, Error, TEXT("Failed to import fusion patch: %s"), *ImportErrorMessage.ToString());
			
			UpdateFusionPatchImportNotificationItem(ImportNotificationItem, bImportSuccessful, InName);
		}

	}
	else
	{
		const FText ImportErrorMessage = FText::Format(LOCTEXT("ImportFailed_Json", "Failed to import asset:\n'{0}'.\nFailed to read Json:\n{1}"), FText::FromString(SourceFile), FText::FromString(ErrorMessage));
		FMessageDialog::Open(EAppMsgType::Ok, ImportErrorMessage);
		UE_LOG(LogFusionPatchAssetFactory, Error, TEXT("Failed to read json: %s"), *ErrorMessage);
	}

	return nullptr;
}

UObject* UFusionPatchAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UFusionPatch* NewAsset = NewObject<UFusionPatch>(InParent, InName, Flags);

	if (!NewAsset)
	{
		return nullptr;
	}

	if (CreateOptions)
	{
		NewAsset->UpdateKeyzones(CreateOptions->Keyzones);
		NewAsset->UpdateSettings(CreateOptions->FusionPatchSettings);
		CreateOptions = nullptr;
	}

	return NewAsset;
}

#undef LOCTEXT_NAMESPACE