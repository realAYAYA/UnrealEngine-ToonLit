// Copyright Epic Games, Inc. All Rights Reserved.

#include "DNAAssetImportFactory.h"
#include "DNAAsset.h"

#include "Engine/SkeletalMesh.h"
#include "ReferenceSkeleton.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "ComponentReregisterContext.h"
#include "UObject/ConstructorHelpers.h"
#include "Editor.h"

#include "ActorFactories/ActorFactoryStaticMesh.h"
#include "ProfilingDebugging/ScopedTimers.h"

#include "Engine/StaticMesh.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "AssetImportTask.h"
#include "Misc/FileHelper.h"

#include "DNAAssetImportUI.h"
#include "DNAImporter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DNAAssetImportFactory)

DEFINE_LOG_CATEGORY(LogDNAImportFactory);
#define LOCTEXT_NAMESPACE "DNAAssetImportFactory"

UDNAAssetImportFactory::UDNAAssetImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = false;
	bEditAfterNew = true;
	SupportedClass = UDNAAsset::StaticClass();

	bEditorImport = true;
	bText = false;

	Formats.Add(TEXT("dna;Character DNA file"));
}

void UDNAAssetImportFactory::PostInitProperties()
{
	Super::PostInitProperties();
	bEditorImport = true;
	bText = false;

	ImportUI = NewObject<UDNAAssetImportUI>(this, NAME_None, RF_NoFlags);
}

bool UDNAAssetImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	TArray<FString> FactoryExtensions;
	GetSupportedFileExtensions(FactoryExtensions);

	return FactoryExtensions.Contains(FPaths::GetExtension(PreferredReimportPath));
}

EReimportResult::Type UDNAAssetImportFactory::Reimport(UObject* Obj, int32 SourceFileIndex)
{
	// This is done here in oreder to enable importing of the dna file with the same name as 
	// skeletal mesh. This is treated as an normal import but engine is recognizing it as an skeletal mesh reimport.

	UPackage* Pkg = Obj->GetPackage();
	FString Name = FPaths::GetBaseFilename(PreferredReimportPath);
	bool bImportWasCancelled = false;
	UObject* Result = ImportObject(SupportedClass, Pkg, FName(*Name), RF_Public | RF_Standalone | RF_Transactional, PreferredReimportPath, nullptr, bImportWasCancelled);

	// Clean up and remove the factories we created from the root set
	CleanUp();

	if (Result != nullptr)
	{
		// TODO: Find a better solution to showing the right message, withouth this we get reimport message success for Skeletal Mesh
		FNotificationInfo Info(LOCTEXT("DNA_ReimportSuccessMessage", "DNA file successfully imported"));
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Success);
		}

		//Cancel is sent here because the notification created for successful import ( reimport is only registered because DNA has the same name as SkelMesh ) is already showed with above code
		return EReimportResult::Cancelled;
	}

	return EReimportResult::Failed;
}

int32 UDNAAssetImportFactory::GetPriority() const
{
	return ImportPriority;
}

UObject* UDNAAssetImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	// this one prints all messages that are stored in FFbxImporter
	FDNAImporter* DNAImporter = FDNAImporter::GetInstance();
	FDNAAssetImportOptions* ImportOptions = DNAImporter->GetImportOptions();
	//Clean up the options
	FDNAAssetImportOptions::ResetOptions(ImportOptions);

	struct FRestoreImportUI
	{
		FRestoreImportUI(UDNAAssetImportFactory* InDNAAssetImportFactory)
			: DNAAssetImportFactory(InDNAAssetImportFactory)
		{
			ensure(DNAAssetImportFactory->OriginalImportUI == nullptr);
			DNAAssetImportFactory->OriginalImportUI = DNAAssetImportFactory->ImportUI;
		}

		~FRestoreImportUI()
		{
			DNAAssetImportFactory->ImportUI = DNAAssetImportFactory->OriginalImportUI;
			DNAAssetImportFactory->OriginalImportUI = nullptr;
		}

	private:
		UDNAAssetImportFactory* DNAAssetImportFactory;
	};
	FRestoreImportUI RestoreImportUI(this);
	UDNAAssetImportUI* OverrideImportUI = AssetImportTask ? Cast<UDNAAssetImportUI>(AssetImportTask->Options) : nullptr;
	if (OverrideImportUI)
	{
		ImportUI = OverrideImportUI;
	}
	//We are not re-importing
	ImportUI->bIsReimport = false;
	ImportUI->ReimportMesh = nullptr;

	// Show the import dialog only when not in a "yes to all" state or when automating import
	bool bIsAutomated = IsAutomatedImport();
	bool bShowImportDialog = !bIsAutomated;

	ImportOptions = GetImportOptions(DNAImporter, ImportUI, bShowImportDialog, bIsAutomated, InParent->GetPathName(), bOutOperationCanceled, UFactory::CurrentFilename);

	if (bOutOperationCanceled)
	{
		// User cancelled, clean up and return
		DNAImporter->PartialCleanUp();
		return nullptr;
	}

	if (ImportOptions)
	{
		USkeletalMesh* SkelMesh = ImportOptions->SkeletalMesh;
		if (SkelMesh)
		{
			FString SkelMeshFullPath(SkelMesh->GetPathName());
			FString SkelMeshJustPath(FPaths::GetPath(*SkelMeshFullPath));
			FString SkelMeshFileName(FPaths::GetBaseFilename(*SkelMeshFullPath));
			FString DNAAssetFileName(TEXT("DNA"));
			FString DNAAssetFullName(SkelMeshJustPath);
			DNAAssetFullName.PathAppend(*SkelMeshFileName, SkelMeshFileName.Len());
			DNAAssetFullName.Append(TEXT("."));
			DNAAssetFullName.Append(DNAAssetFileName);

			GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPreImport(this, InClass, InParent, *DNAAssetFullName, TEXT("dna"));

			UDNAAsset* DNAAsset = NewObject< UDNAAsset >(SkelMesh, FName(*DNAAssetFileName)); //SkelMesh has to be its outer, otherwise DNAAsset won't be saved			
			if (DNAAsset->Init(*Filename))
			{
				UAssetUserData* DNAAssetUserData = Cast<UAssetUserData>(DNAAsset);
				SkelMesh->AddAssetUserData(DNAAssetUserData);

				GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, DNAAsset);
				DNAImporter->PartialCleanUp();
				return DNAAsset;
			}
		}
		else
		{
			// Skeletal mesh not found.
			const FText Message = LOCTEXT("DNAImportFailed", "DNA file can not be uploaded without Skeletal Mesh.");
			FMessageDialog::Open(EAppMsgType::Ok, Message);
			UE_LOG(LogDNAImportFactory, Warning, TEXT("%s"), *Message.ToString());
		}
	}

	// Failed to load file or create DNA stream. Clean and return nullptr.
	DNAImporter->PartialCleanUp();
	GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr);
	return nullptr;
}

bool UDNAAssetImportFactory::FactoryCanImport(const FString& Filename)
{
	const FString Extension = FPaths::GetExtension(Filename);

	if (Extension == TEXT("dna"))
	{
		return true;
	}

	return false;
}

// DNA Asset Import UI Implementation.
UDNAAssetImportUI::UDNAAssetImportUI(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsReimport = false;
	ReimportMesh = nullptr;
	//Make sure we are transactional to allow undo redo
	this->SetFlags(RF_Transactional);
}


bool UDNAAssetImportUI::CanEditChange(const FProperty* InProperty) const
{
	bool bIsMutable = Super::CanEditChange(InProperty);
	return bIsMutable;
}

void UDNAAssetImportUI::ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson)
{
}

void UDNAAssetImportUI::ResetToDefault()
{
	ReloadConfig();
}
// END of DNA Asset Import UI Implementation.

#undef LOCTEXT_NAMESPACE

