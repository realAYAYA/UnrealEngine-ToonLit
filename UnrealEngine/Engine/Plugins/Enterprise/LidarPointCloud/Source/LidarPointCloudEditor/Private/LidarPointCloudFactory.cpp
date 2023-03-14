// Copyright Epic Games, Inc. All Rights Reserved.

#include "LidarPointCloudFactory.h"
#include "LidarPointCloudShared.h"
#include "LidarPointCloudSettings.h"
#include "LidarPointCloudImportUI.h"
#include "LidarPointCloudEditor.h"
#include "IO/LidarPointCloudFileIO.h"
#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "LidarPointCloudEditorHelper.h"

#include "Misc/ScopedSlowTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "LidarPointCloud"

FText FAssetTypeActions_LidarPointCloud::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_LidarPointCloud", "LiDAR Point Cloud");
}

void FAssetTypeActions_LidarPointCloud::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	TArray<ULidarPointCloud*> PointClouds;
	for (UObject* Object : InObjects)
	{
		PointClouds.Add(CastChecked<ULidarPointCloud>(Object));
	}

	if (PointClouds.Num() > 1)
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("LidarPointCloud_Merge", "Merge Selected"),
			LOCTEXT("LidarPointCloud_MergeTooltip", "Merges selected point cloud assets."),
			FSlateIcon("LidarPointCloudStyle", "LidarPointCloudEditor.ToolkitMerge"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_LidarPointCloud::ExecuteMerge, PointClouds),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("LidarPointCloud_Align", "Align Selected"),
			LOCTEXT("LidarPointCloud_AlignTooltip", "Aligns selected point cloud assets."),
			FSlateIcon("LidarPointCloudStyle", "LidarPointCloudEditor.ToolkitAlign"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_LidarPointCloud::ExecuteAlign, PointClouds),
				FCanExecuteAction()
			)
		);
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LidarPointCloud_BuildCollision", "Build Collision"),
		LOCTEXT("LidarPointCloud_BuildCollisionTooltip", "Builds collision for all selected point cloud assets."),
		FSlateIcon("LidarPointCloudStyle", "LidarPointCloudEditor.ToolkitCollision"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_LidarPointCloud::ExecuteCollision, PointClouds),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("LidarPointCloud_CalculateNormals", "Calculate Normals"),
		LOCTEXT("LidarPointCloud_CalculateNormalsTooltip", "Calculates normals for all selected point cloud assets."),
		FSlateIcon("LidarPointCloudStyle", "LidarPointCloudEditor.ToolkitNormals"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_LidarPointCloud::ExecuteNormals, PointClouds),
			FCanExecuteAction()
		)
	);
}

void FAssetTypeActions_LidarPointCloud::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (ULidarPointCloud* PointCloud = Cast<ULidarPointCloud>(Object))
		{
			TSharedRef<FLidarPointCloudEditor> NewPointCloudEditor(new FLidarPointCloudEditor());
			NewPointCloudEditor->InitPointCloudEditor(Mode, EditWithinLevelEditor, PointCloud);
		}
	}
}

void FAssetTypeActions_LidarPointCloud::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		if (const ULidarPointCloud* PointCloud = CastChecked<ULidarPointCloud>(Asset))
		{
			OutSourceFilePaths.Add(PointCloud->GetSourcePath());
		}
	}
}

void FAssetTypeActions_LidarPointCloud::ExecuteMerge(TArray<ULidarPointCloud*> PointClouds)
{
	if (PointClouds.Num() < 2)
	{
		return;
	}

	FString PackageName = PointClouds[0]->GetOutermost()->GetName() + TEXT("_Merged");
	UPackage* MergedCloudPackage = UPackageTools::FindOrCreatePackageForAssetType(FName(*PackageName), ULidarPointCloud::StaticClass());
	if (IsValid(MergedCloudPackage))
	{
		MergedCloudPackage->SetPackageFlags(PKG_NewlyCreated);

		ULidarPointCloud* PC = NewObject<ULidarPointCloud>(MergedCloudPackage, FName(*FPaths::GetBaseFilename(MergedCloudPackage->GetName())), EObjectFlags::RF_Public | EObjectFlags::RF_Standalone | EObjectFlags::RF_Transactional);

		FLidarPointCloudEditorHelper::MergeLidar(PC, PointClouds);
	}
}

void FAssetTypeActions_LidarPointCloud::ExecuteAlign(TArray<ULidarPointCloud*> PointClouds)
{
	FScopedSlowTask ProgressDialog(1, LOCTEXT("Align", "Aligning Point Clouds..."));
	ProgressDialog.MakeDialog();
	ProgressDialog.EnterProgressFrame(1.f);
	ULidarPointCloud::AlignClouds(PointClouds);
}

void FAssetTypeActions_LidarPointCloud::ExecuteCollision(TArray<ULidarPointCloud*> PointClouds)
{
	for (ULidarPointCloud* PC : PointClouds)
	{
		PC->BuildCollision();
	}
}

void FAssetTypeActions_LidarPointCloud::ExecuteNormals(TArray<ULidarPointCloud*> PointClouds)
{
	for (ULidarPointCloud* PC : PointClouds)
	{
		// Data needs to be persistently loaded to calculate normals
		if (!PC->IsFullyLoaded())
		{
			PC->LoadAllNodes();
		}

		PC->CalculateNormals(nullptr, nullptr);
	}
}

ULidarPointCloudFactory::ULidarPointCloudFactory()
{
	bImportingAll = false;
	bCreateNew = true;
	bEditorImport = true;
	SupportedClass = ULidarPointCloud::StaticClass();

	TArray<FString> exts = ULidarPointCloudFileIO::GetSupportedImportExtensions();
	for (FString ext : exts)
	{
		Formats.Add(*ext.Append(";LiDAR Point Cloud"));
	}
}

UObject* ULidarPointCloudFactory::ImportObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, bool& OutCanceled)
{
	bCreateNew = false;
	UObject* NewPC = Super::ImportObject(InClass, InOuter, InName, Flags, Filename, Parms, OutCanceled);
	bCreateNew = true;

	return NewPC;
}

UObject* ULidarPointCloudFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	UObject *OutObject = nullptr;

	UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>();
	ImportSubsystem->OnAssetPreImport.Broadcast(this, InClass, InParent, InName, *FPaths::GetExtension(Filename));

	// Check if the headers differ between files. Log occurrence and prompt the user for interaction if necessary
	if (bImportingAll && ImportSettings.IsValid())
	{
		if (!ImportSettings->IsFileCompatible(Filename))
		{
			PC_WARNING("Inconsistent header information between files - batch import cancelled.");
			bImportingAll = false;
		}
		else
		{
			ImportSettings->SetNewFilename(Filename);
		}
	}

	if (!bImportingAll)
	{
		ImportSettings = FLidarPointCloudImportUI::ShowImportDialog(Filename, false);
		bImportingAll = ImportSettings.IsValid() && ImportSettings->bImportAll;
	}

	if (ImportSettings.IsValid())
	{
		OutObject = ULidarPointCloud::CreateFromFile(Filename, ImportSettings->Clone(), InParent, InName, Flags);
	}
	else
	{
		bOutOperationCanceled = true;
	}

	ImportSubsystem->OnAssetPostImport.Broadcast(this, OutObject);

	return OutObject;
}

UObject* ULidarPointCloudFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<ULidarPointCloud>(InParent, InName, Flags);
}

bool ULidarPointCloudFactory::DoesSupportClass(UClass* Class) { return Class == ULidarPointCloud::StaticClass(); }

bool ULidarPointCloudFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{	
	ULidarPointCloud* PC = Cast<ULidarPointCloud>(Obj);
	if (PC)
	{
		OutFilenames.Add(*PC->GetSourcePath());
		return true;
	}
	return false;
}

void ULidarPointCloudFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	ULidarPointCloud *PC = Cast<ULidarPointCloud>(Obj);
	if (PC && NewReimportPaths.Num())
	{
		PC->SetSourcePath(NewReimportPaths[0]);
	}
}

EReimportResult::Type ULidarPointCloudFactory::Reimport(UObject* Obj)
{
	ULidarPointCloud *PC = Cast<ULidarPointCloud>(Obj);

	if (PC)
	{	
		bool bSuccess = false;

		// Show existing settings, if the cloud has any
		if (PC->ImportSettings.IsValid())
		{
			bSuccess = FLidarPointCloudImportUI::ShowImportDialog(PC->ImportSettings, true);
		}
		// ... otherwise attempt to generate new, based on the source path (if valid)
		else if (FPaths::FileExists(PC->GetSourcePath()))
		{
			PC->ImportSettings = FLidarPointCloudImportUI::ShowImportDialog(PC->GetSourcePath(), true);
			bSuccess = PC->ImportSettings.IsValid();
		}
		else
		{
			PC_ERROR("Cannot reimport, source path is incorrect.");
		}

		if (bSuccess)
		{
			PC->Reimport(GetDefault<ULidarPointCloudSettings>()->bUseAsyncImport);
		}
	}

	// Return cancelled, to avoid showing 2 notifications
	return EReimportResult::Cancelled;
}

#undef LOCTEXT_NAMESPACE