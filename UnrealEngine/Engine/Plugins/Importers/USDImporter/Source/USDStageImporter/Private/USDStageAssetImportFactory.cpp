// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDStageAssetImportFactory.h"

#include "USDAssetImportData.h"
#include "USDErrorUtils.h"
#include "USDLog.h"
#include "USDStageImporter.h"
#include "USDStageImporterModule.h"
#include "USDStageImportOptions.h"

#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "AssetImportTask.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "JsonObjectConverter.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/ScopedTimers.h"

#define LOCTEXT_NAMESPACE "USDStageAssetImportFactory"

UUsdStageAssetImportFactory::UUsdStageAssetImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = nullptr;

	// Its ok if we intercept most calls as there aren't other USD importers, and
	// for reimport we can definitely tell that we should be handling an asset, as we
	// use a custom asset import data
	ImportPriority += 100;

	bEditorImport = true;
	bText = false;

	for ( const FString& Extension : UnrealUSDWrapper::GetNativeFileFormats() )
	{
		Formats.Add(FString::Printf(TEXT("%s; Universal Scene Description files"), *Extension));
	}
}

bool UUsdStageAssetImportFactory::DoesSupportClass(UClass* Class)
{
	return (Class == UStaticMesh::StaticClass() || Class == USkeletalMesh::StaticClass());
}

UClass* UUsdStageAssetImportFactory::ResolveSupportedClass()
{
	return UStaticMesh::StaticClass();
}

UObject* UUsdStageAssetImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UObject* ImportedObject = nullptr;

	if ( AssetImportTask && IsAutomatedImport() )
	{
		ImportContext.ImportOptions = Cast<UUsdStageImportOptions>( AssetImportTask->Options );
	}

	// When importing from file we don't want to use any opened stage
	ImportContext.bReadFromStageCache = false;

	const FString InitialPackagePath = InParent ? InParent->GetName() : TEXT( "/Game/" );
	const bool bIsReimport = false;
	const bool bAllowActorImport = false;
	if (ImportContext.Init(InName.ToString(), Filename, InitialPackagePath, Flags, IsAutomatedImport(), bIsReimport, bAllowActorImport))
	{
		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport( this, InClass, InParent, InName, Parms );

		FScopedUsdMessageLog ScopedMessageLog;

		UUsdStageImporter* USDImporter = IUsdStageImporterModule::Get().GetImporter();
		USDImporter->ImportFromFile(ImportContext);

		GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, ImportContext.SceneActor);
		GEditor->BroadcastLevelActorListChanged();

		ImportedObject = ImportContext.ImportedAsset ? ToRawPtr(ImportContext.ImportedAsset) : Cast<UObject>(ImportContext.SceneActor);
	}
	else
	{
		bOutOperationCanceled = true;
	}

	return ImportedObject;
}

bool UUsdStageAssetImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension( Filename );

	for ( const FString& SupportedExtension : UnrealUSDWrapper::GetAllSupportedFileFormats() )
	{
		if ( SupportedExtension.Equals( Extension, ESearchCase::IgnoreCase ) )
		{
			return true;
		}
	}

	return false;
}

void UUsdStageAssetImportFactory::CleanUp()
{
	ImportContext.Reset();
	Super::CleanUp();
}

bool UUsdStageAssetImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (UUsdAssetImportData* ImportData = UsdUtils::GetAssetImportData(Obj))
	{
		OutFilenames.Add(ImportData->GetFirstFilename());
		return true;
	}

	return false;
}

void UUsdStageAssetImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (NewReimportPaths.Num() != 1)
	{
		return;
	}

	if (UUsdAssetImportData* ImportData = UsdUtils::GetAssetImportData(Obj))
	{
		ImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UUsdStageAssetImportFactory::Reimport(UObject* Obj)
{
	if (!Obj)
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Error, LOCTEXT( "ReimportErrorInvalidAsset", "Failed to reimport asset as it is invalid!" ) );
		return EReimportResult::Failed;
	}

	UUsdAssetImportData* ImportData = UsdUtils::GetAssetImportData(Obj);
	if (!ImportData)
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Error, FText::Format( LOCTEXT( "ReimportErrorNoImportData", "Failed to reimport asset '{0}' as it doesn't seem to have import data!" ), FText::FromName( Obj->GetFName() ) ) );
		return EReimportResult::Failed;
	}

	if ( UUsdStageImportOptions* ReimportOptions = Cast<UUsdStageImportOptions>( ImportData->ImportOptions ) )
	{
		// Duplicate this as we may update these options on the Init call below, and if we just imported a scene and
		// all assets are in memory (sharing the same import options object), that update would otherwise affect all the UUsdAssetImportData objects,
		// which is not what we would expect
		ImportContext.ImportOptions = DuplicateObject( ReimportOptions, ImportData );
	}

	ImportContext.bReadFromStageCache = false;

	const bool bIsReimport = true;
	const bool bAllowActorImport = false;
	if (!ImportContext.Init(Obj->GetName(), ImportData->GetFirstFilename(), Obj->GetName(), Obj->GetFlags(), IsAutomatedImport(), bIsReimport, bAllowActorImport))
	{
		FUsdLogManager::LogMessage( EMessageSeverity::Error, FText::Format( LOCTEXT( "ReimportErrorNoContext", "Failed to initialize reimport context for asset '{0}'!" ), FText::FromName( Obj->GetFName() ) ) );
		return EReimportResult::Failed;
	}

	ImportContext.PackagePath = Obj->GetOutermost()->GetPathName();

	FScopedUsdMessageLog ScopedMessageLog;

	UUsdStageImporter* USDImporter = IUsdStageImporterModule::Get().GetImporter();
	UObject* ReimportedAsset = nullptr;
	bool bSuccess = USDImporter->ReimportSingleAsset(ImportContext, Obj, ImportData, ReimportedAsset);

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetReimport(Obj);

	return bSuccess ? EReimportResult::Succeeded : EReimportResult::Failed;
}

int32 UUsdStageAssetImportFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE