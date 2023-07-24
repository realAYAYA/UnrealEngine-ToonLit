// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigLogicEditor.h"
#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"
#include "DNAImporter.h"
#include "RigUnit_RigLogic.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISettingsModule.h"
#include "PropertyEditorModule.h"
#include <ContentBrowserModule.h>
#include <DNAAssetImportFactory.h>
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/Paths.h"



IMPLEMENT_MODULE(FRigLogicEditor, RigLogicEditor)

DEFINE_LOG_CATEGORY_STATIC(LogRigLogicEditor, Log, All);

#define LOCTEXT_NAMESPACE "DNAAssetImport"

void FRigLogicEditor::StartupModule()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));	
	TArray<FContentBrowserMenuExtender_SelectedAssets>& MenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders(); 
	
	MenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FRigLogicEditor::OnExtendSkelMeshWithDNASelectionMenu));
}


TSharedRef<FExtender> FRigLogicEditor::OnExtendSkelMeshWithDNASelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateStatic(&FRigLogicEditor::CreateDnaActionsSubMenu, SelectedAssets)
	);
	return Extender;
}

void FRigLogicEditor::CreateDnaActionsSubMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets)
{
	const FAssetData& Asset = SelectedAssets[0];

	if (Asset.GetClass()->IsChildOf(USkeletalMesh::StaticClass()))
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("DNASkeletalMeshSubmenu", "MetaHuman DNA"),
			LOCTEXT("DNAImportSubmenu_ToolTip", "DNA related actions"),
			FNewMenuDelegate::CreateStatic(&FRigLogicEditor::GetDNAMenu, SelectedAssets),
			false,
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import")
		);	
	}
	
}

void FRigLogicEditor::GetDNAMenu(FMenuBuilder& MenuBuilder, const TArray<FAssetData> SelectedAssets)
{
	auto Mesh = SelectedAssets[0].GetAsset();
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("Add DNA", "Import new DNA File"),
		LOCTEXT("ImportDNA_Tooltip", "Import DNA"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
		FUIAction(FExecuteAction::CreateStatic(&FRigLogicEditor::ExecuteDNAImport, static_cast<UObject*>(Mesh)))
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Reimport DNA", "Reimport existing DNA File"),
		LOCTEXT("ReimportDNA_Tooltip ", "Reimport DNA"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
		FUIAction(FExecuteAction::CreateStatic(&FRigLogicEditor::ExecuteDNAReimport, static_cast<UObject*>(Mesh)))
	);
}

void FRigLogicEditor::ExecuteDNAImport(UObject* Mesh)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	
	FDNAImporter* DNAImporter = FDNAImporter::GetInstance();
	const TArray<FString> Filenames = { DNAImporter->PromptForDNAImportFile() };
	UFactory* Factory = Cast<UFactory>(UDNAAssetImportFactory::StaticClass()->GetDefaultObject());
	FString Path = FPaths::GetPath(Mesh->GetPathName());
	
	AssetToolsModule.Get().ImportAssets(Filenames,Path, Factory,true, nullptr,true);
}

void FRigLogicEditor::ExecuteDNAReimport(class UObject* Mesh)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(Mesh);
	const TArray<UAssetUserData*>* AssetData = SkelMesh->GetAssetUserDataArray();

	if(!AssetData->IsEmpty())
	{
		const UDNAAsset* DNAAsset = Cast<UDNAAsset>(&AssetData[0]);
		if(DNAAsset)
		{
			const TArray<FString> Filenames = { DNAAsset->DnaFileName };
			UFactory* Factory = Cast<UFactory>(UDNAAssetImportFactory::StaticClass()->GetDefaultObject());
			FString Path = FPaths::GetPath(Mesh->GetPathName());

			AssetToolsModule.Get().ImportAssets(Filenames, Path, Factory, true, nullptr, true);
		}
	}
	else
	{
		FNotificationInfo Info(LOCTEXT("DNA_ReimportErrorMessage", "There is no DNA file attached to do Reimport"));
		Info.ExpireDuration = 3.0f;
		Info.bUseLargeFont = false;
		TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
		if (Notification.IsValid())
		{
			Notification->SetCompletionState(SNotificationItem::CS_Fail);
		}
	}
}

void FRigLogicEditor::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE