// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGAssetExporterUtils.h"

#include "PCGEditorModule.h"

#include "ContentBrowserModule.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "AssetRegistry/AssetRegistryModule.h"

UPackage* UPCGAssetExporterUtils::CreateAsset(UPCGAssetExporter* Exporter, FPCGAssetExporterParameters Parameters)
{
	if (!Exporter || !Exporter->GetAssetType())
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Unable to create asset without an exporter, or exporter is not setup properly."));
		return nullptr;
	}

	const bool bExporterNeedsRooting = !Exporter->IsRooted();
	if (bExporterNeedsRooting)
	{
		Exporter->AddToRoot();
	}

	ON_SCOPE_EXIT
	{
		if (bExporterNeedsRooting)
		{
			Exporter->RemoveFromRoot();
		}
	};

	FString AssetName = Parameters.AssetName;
	FString AssetPath = Parameters.AssetPath;
	FString PackageName = FPaths::Combine(AssetPath, AssetName);

	if (Parameters.bOpenSaveDialog)
	{
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DefaultPath = AssetPath;
		SaveAssetDialogConfig.DefaultAssetName = AssetName;
		SaveAssetDialogConfig.AssetClassNames.Add(Exporter->GetAssetType()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = NSLOCTEXT("PCGAssetExporter", "SaveAssetToFileDialogTitle", "Save PCG Asset");

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			AssetName = FPackageName::ObjectPathToObjectName(SaveObjectPath);
			AssetPath = FString(); // not going to be reused
			PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
		}
		else
		{
			return nullptr;
		}
	}

	UPackage* Package = FPackageName::DoesPackageExist(PackageName) ? LoadPackage(nullptr, *PackageName, LOAD_None) : nullptr;

	UPCGDataAsset* Asset = nullptr;
	bool NewAssetCreated = false;

	if (Package)
	{
		UObject* Object = FindObjectFast<UObject>(Package, *AssetName);
		if (Object && Object->GetClass() != Exporter->GetAssetType())
		{
			Object->SetFlags(RF_Transient);
			Object->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
			NewAssetCreated = true;
		}
		else
		{
			Asset = Cast<UPCGDataAsset>(Object);
		}
	}
	else
	{
		Package = CreatePackage(*PackageName);
		NewAssetCreated = true;
	}

	if (!Asset)
	{
		const EObjectFlags Flags = RF_Public | RF_Standalone | RF_Transactional;
		Asset = NewObject<UPCGDataAsset>(Package, Exporter->GetAssetType(), FName(*AssetName), Flags);
	}

	if (Asset)
	{
		if (NewAssetCreated)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Asset);
		}

		if (Exporter->Export(PackageName, Asset))
		{
			// Make sure everybody knows we changed the asset
			if (!NewAssetCreated)
			{
				FCoreUObjectDelegates::BroadcastOnObjectModified(Asset);
			}
		}
		else
		{
			return nullptr;
		}
	}

	// Save the file
	if (Package && Parameters.bSaveOnExportEnded)
	{
		FEditorFileUtils::PromptForCheckoutAndSave({ Package }, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
	}

	return Package;
}

void UPCGAssetExporterUtils::UpdateAssets(const TArray<FAssetData>& PCGAssets, FPCGAssetExporterParameters InParameters)
{
	TArray<UPackage*> PackagesToSave;
	FPCGAssetExporterParameters Parameters = InParameters;

	// Never open a dialog when updating pre-existing assets
	Parameters.bOpenSaveDialog = false;

	for (const FAssetData& PCGAsset : PCGAssets)
	{
		TSubclassOf<UPCGAssetExporter> ExporterSubclass;

		const FString ExporterClassString = PCGAsset.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UPCGDataAsset, ExporterClass));
		TSoftClassPtr<> ExporterClass(ExporterClassString);
		if (ExporterClass.IsValid() && ExporterClass->IsChildOf<UPCGAssetExporter>())
		{
			ExporterSubclass = ExporterClass.Get();
		}
		else
		{
			UE_LOG(LogPCGEditor, Error, TEXT("Unable to update asset '%s' because exporter isn't valid."), *PCGAsset.AssetName.ToString());
			continue;
		}

		UPCGAssetExporter* Exporter = NewObject<UPCGAssetExporter>(GetTransientPackage(), ExporterSubclass);

		if (!Exporter)
		{
			UE_LOG(LogPCGEditor, Error, TEXT("Unable to create exporter for asset '%s' during update process."), *PCGAsset.AssetName.ToString());
			continue;
		}

		Exporter->AddToRoot();

		if (UPackage* Package = Exporter->Update(PCGAsset))
		{
			FCoreUObjectDelegates::BroadcastOnObjectModified(PCGAsset.GetAsset());
			PackagesToSave.Add(Package);
		}

		Exporter->RemoveFromRoot();
	}

	// Save the file(s)
	if (!PackagesToSave.IsEmpty() && Parameters.bSaveOnExportEnded)
	{
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty=*/false, /*bPromptToSave=*/false);
	}
}