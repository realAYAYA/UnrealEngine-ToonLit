// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXGDTFFactory.h"
#include "Factories/DMXGDTFImportUI.h"
#include "DMXEditorLog.h"
#include "Factories/DMXGDTFImporter.h"
#include "Library/DMXGDTFAssetImportData.h"
#include "Library/DMXImportGDTF.h"

#include "Editor.h"
#include "EditorReimportHandler.h"
#include "AssetImportTask.h"
#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"


const TCHAR* UDMXGDTFFactory::Extension = TEXT("gdtf");

UDMXGDTFImportUI::UDMXGDTFImportUI()
    : bUseSubDirectory(false)
    , bImportXML(true)
    , bImportTextures(false)
    , bImportModels(false)
{
}

void UDMXGDTFImportUI::ResetToDefault()
{
    bUseSubDirectory = false;
    bImportXML = true;
    bImportTextures = false;
    bImportModels = false;
}

UDMXGDTFFactory::UDMXGDTFFactory()
{
    SupportedClass = nullptr;
	Formats.Add(TEXT("gdtf;General Device Type Format"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
	bOperationCanceled = false;
}

void UDMXGDTFFactory::CleanUp()
{
	Super::CleanUp();

    bShowOption = true;
}

bool UDMXGDTFFactory::ConfigureProperties()
{
    Super::ConfigureProperties();
    EnableShowOption();

    return true;
}

void UDMXGDTFFactory::PostInitProperties()
{
	Super::PostInitProperties();

    ImportUI = NewObject<UDMXGDTFImportUI>(this, NAME_None, RF_NoFlags);
}

bool UDMXGDTFFactory::DoesSupportClass(UClass* Class)
{
    return Class == UDMXImportGDTF::StaticClass();
}

UClass* UDMXGDTFFactory::ResolveSupportedClass()
{
    return UDMXImportGDTF::StaticClass();
}

UObject* UDMXGDTFFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& InFilename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    FString FileExtension = FPaths::GetExtension(InFilename);
    const TCHAR* Type = *FileExtension;

    if (!IFileManager::Get().FileExists(*InFilename))
    {
        UE_LOG_DMXEDITOR(Error, TEXT("Failed to load file '%s'"), *InFilename)
        return nullptr;
    }

    ParseParms(Parms);

    CA_ASSUME(InParent);

    if( bOperationCanceled )
    {
        bOutOperationCanceled = true;
        GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
        return nullptr;
    }

    GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);

    UObject* ExistingObject = nullptr;
    if (InParent != nullptr)
    {
        ExistingObject = StaticFindObject(UObject::StaticClass(), InParent, *(InName.ToString()));
        if (ExistingObject)
        {
            bShowOption = false;
        }
    }

    // Prepare import options
    FDMXGDTFImportArgs ImportArgs;
    ImportArgs.ImportUI = ImportUI;
    ImportArgs.Name = InName;
    ImportArgs.Parent = InParent;
    ImportArgs.CurrentFilename = InFilename;
    ImportArgs.Flags = Flags;
    ImportArgs.bCancelOperation = bOperationCanceled;
    const TUniquePtr<FDMXGDTFImporter> Importer = MakeUnique<FDMXGDTFImporter>(ImportArgs);

    // Set Import UI
    if (FParse::Param(FCommandLine::Get(), TEXT("NoDMXImportOption")))
    {
        bShowOption = false;
    }
    bool bIsAutomated = IsAutomatedImport();
    bool bShowImportDialog = bShowOption && !bIsAutomated;
    bool bImportAll = false;
    FDMXGDTFImporter::GetImportOptions(Importer, ImportUI, bShowImportDialog, InParent->GetPathName(), bOperationCanceled, bImportAll, UFactory::CurrentFilename);
    bOutOperationCanceled = bOperationCanceled;
    if( bImportAll )
    {
        // If the user chose to import all, we don't show the dialog again and use the same settings for each object until importing another set of files
        bShowOption = false;
    }

    if (!ImportUI->bImportXML && !ImportUI->bImportModels && !ImportUI->bImportTextures)
    {
        Warn->Log(ELogVerbosity::Error, TEXT("Nothing to Import") );
		return nullptr;
    }

    // Try to load and parse the content
    if (!Importer->AttemptImportFromFile())
    {
        Warn->Log(ELogVerbosity::Error, TEXT("Failed to import GDTF") );
		return nullptr;
    }

	// Import to the Editor
	UDMXImportGDTF* GDTF = Importer->Import();
	if (!GDTF)
	{
		return nullptr;
	}

	// Set Asset Import Data
	UDMXGDTFAssetImportData* GDTFAssetImportData = GDTF->GetGDTFAssetImportData();
	if (!ensureAlwaysMsgf(GDTFAssetImportData, TEXT("Unexpected missing Asset Import Data for newly created GDTF %s"), *GDTF->GetName()))
	{
		return nullptr;
	}
	GDTFAssetImportData->SetSourceFile(InFilename);

	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, GDTF);

    return GDTF;
}

bool UDMXGDTFFactory::FactoryCanImport(const FString& Filename)
{
	const FString TargetExtension = FPaths::GetExtension(Filename);

	if(TargetExtension == UDMXGDTFFactory::Extension)
	{
		return true;
	}

	return false;
}

bool UDMXGDTFFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(Obj);
	if (GDTF && GDTF->GetGDTFAssetImportData())
	{
		const FString SourceFilename = GDTF->GetGDTFAssetImportData()->GetFilePathAndName();
		OutFilenames.Add(SourceFilename);
		return true;
	}
	return false;
}

void UDMXGDTFFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(Obj);
	if (GDTF && GDTF->GetGDTFAssetImportData() && ensure(NewReimportPaths.Num() == 1))
	{
		GDTF->GetGDTFAssetImportData()->SetSourceFile(NewReimportPaths[0]);
	}
}

EReimportResult::Type UDMXGDTFFactory::Reimport(UObject* InObject)
{
	UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(InObject);

	if (!GDTF || !GDTF->GetGDTFAssetImportData())
	{
		return EReimportResult::Failed;
	}

	const FString SourceFilename = GDTF->GetGDTFAssetImportData()->GetFilePathAndName();
	if (!FPaths::FileExists(SourceFilename))
	{
		return EReimportResult::Failed;
	}

	bool bOutCanceled = false;
	if (ImportObject(InObject->GetClass(), InObject->GetOuter(), *InObject->GetName(), RF_Public | RF_Standalone, SourceFilename, nullptr, bOutCanceled))
	{
		return EReimportResult::Succeeded;
	}

	return bOutCanceled ? EReimportResult::Cancelled : EReimportResult::Failed;
}

int32 UDMXGDTFFactory::GetPriority() const
{
    return ImportPriority;
}
