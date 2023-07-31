// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataValidationModule.h"

#include "UObject/Object.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/HUD.h"

#include "Framework/Application/SlateApplication.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
#include "DataValidationCommandlet.h"
#include "LevelEditor.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceConsole.h"
#include "Modules/ModuleManager.h"

#include "ContentBrowserMenuContexts.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserDelegates.h"

#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "EditorValidatorSubsystem.h"
#include "ISettingsModule.h"
#include "Algo/RemoveIf.h"

#define LOCTEXT_NAMESPACE "DataValidationModule"

class FDataValidationModule : public IDataValidationModule
{
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

	/** Validates selected assets and opens a window to report the results. If bValidateDependencies is true it will also validate any assets that the selected assets depend on. */
	virtual void ValidateAssets(const TArray<FAssetData>& SelectedAssets, bool bValidateDependencies, const EDataValidationUsecase InValidationUsecase) override;

	void ValidateFolders(const TArray<FString>& SelectedFolders, const EDataValidationUsecase InValidationUsecase);

private:
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext);

	// Adds Asset and any assets it depends on to the set DependentAssets
	void FindAssetDependencies(const FAssetRegistryModule& AssetRegistryModule, const FAssetData& Asset, TSet<FAssetData>& DependentAssets);

	void RegisterMenus();
	static FText Menu_ValidateDataGetTitle();
	static void Menu_ValidateData();
};

IMPLEMENT_MODULE(FDataValidationModule, DataValidation)

void FDataValidationModule::StartupModule()
{	
	if (!IsRunningCommandlet() && !IsRunningGame() && FSlateApplication::IsInitialized())
	{
		// add the File->DataValidation menu subsection
		UToolMenus::Get()->RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDataValidationModule::RegisterMenus));

		// Add save callback
		UPackage::PackageSavedWithContextEvent.AddRaw(this, &FDataValidationModule::OnPackageSaved);

		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings("Editor", "Advanced", "DataValidation",
			LOCTEXT("DataValidationName", "Data Validation"),
			LOCTEXT("DataValidationDescription", "Settings related to validating assets in the editor."),
			GetMutableDefault<UDataValidationSettings>()
		);
	}
}

void FDataValidationModule::ShutdownModule()
{
	if (!IsRunningCommandlet() && !IsRunningGame() && !IsRunningDedicatedServer())
	{
		// remove menu extension
		UToolMenus::UnregisterOwner(this);

		UPackage::PackageSavedWithContextEvent.RemoveAll(this);
	}
}

void FDataValidationModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("DataValidation", LOCTEXT("DataValidation", "DataValidation"));
		Section.AddEntry(FToolMenuEntry::InitMenuEntry(
			"ValidateData",
			TAttribute<FText>::Create(&Menu_ValidateDataGetTitle),
			LOCTEXT("ValidateDataTooltip", "Validates all user data in content directory."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "DeveloperTools.MenuIcon"),
			FUIAction(FExecuteAction::CreateStatic(&FDataValidationModule::Menu_ValidateData))
		));
	}

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.AssetActionsSubMenu");
		FToolMenuSection& Section = Menu->FindOrAddSection("AssetContextAdvancedActions");
		Section.AddMenuEntry(
			"ValidateAssets",
			LOCTEXT("ValidateAssetsTabTitle", "Validate Assets"),
			LOCTEXT("ValidateAssetsTooltipText", "Runs data validation on these assets."),
			FSlateIcon(),
			FToolMenuExecuteAction::CreateLambda([this](const FToolMenuContext& InContext)
			{
				if (UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					ValidateAssets(Context->SelectedAssets, false, EDataValidationUsecase::Manual);
				}
			})
		);

		Section.AddMenuEntry(
			"ValidateAssetsAndDependencies",
			LOCTEXT("ValidateAssetsAndDependenciesTabTitle", "Validate Assets and Dependencies"),
			LOCTEXT("ValidateAssetsAndDependenciesTooltipText", "Runs data validation on these assets and all assets they depend on."),
			FSlateIcon(),
			FToolMenuExecuteAction::CreateLambda([this](const FToolMenuContext& InContext)
			{
				if (UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					ValidateAssets(Context->SelectedAssets, true, EDataValidationUsecase::Manual);
				}
			})
		);
	}

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.FolderContextMenu");
		FToolMenuSection& Section = Menu->FindOrAddSection("PathContextBulkOperations");
		Section.AddMenuEntry(
			"ValidateAssetsPath",
			LOCTEXT("ValidateAssetsPathTabTitle", "Validate Assets in Folder"),
			LOCTEXT("ValidateAssetsPathTooltipText", "Runs data validation on the assets in the selected folder."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Validate"),
			FToolMenuExecuteAction::CreateLambda([this](const FToolMenuContext& InContext)
			{
				if (UContentBrowserFolderContext* Context = InContext.FindContext<UContentBrowserFolderContext>())
				{
					const TArray<FString>& SelectedPaths = Context->GetSelectedPackagePaths();

					FString FormattedSelectedPaths;
					for (int32 i = 0; i < SelectedPaths.Num(); ++i)
					{
						FormattedSelectedPaths.Append(SelectedPaths[i]);
						if (i < SelectedPaths.Num() - 1)
						{
							FormattedSelectedPaths.Append(LINE_TERMINATOR);
						}
					}
					FFormatNamedArguments Args;
					Args.Add(TEXT("Paths"), FText::FromString(FormattedSelectedPaths));
					const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, FText::Format(LOCTEXT("DataValidationConfirmation", "Are you sure you want to proceed with validating the following folders?\n\n{Paths}"), Args));
					if (Result == EAppReturnType::Yes)
					{
						ValidateFolders(SelectedPaths, EDataValidationUsecase::Manual);
					}
				}
			})
		);
	}
}

FText FDataValidationModule::Menu_ValidateDataGetTitle()
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		return LOCTEXT("ValidateDataTitleDA", "Validate Data [Discovering Assets]");
	}
	return LOCTEXT("ValidateDataTitle", "Validate Data...");
}

void FDataValidationModule::Menu_ValidateData()
{
	// make sure the asset registry is finished building
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AssetsStillScanningError", "Cannot run data validation while still discovering assets."));
		return;
	}

	// validate the data
	bool bSuccess = UDataValidationCommandlet::ValidateData(FString());

	// display an error if the task failed
	if (!bSuccess)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("DataValidationError", "An error was encountered during data validation. See the log for details."));
		return;
	}
}

void FDataValidationModule::FindAssetDependencies(const FAssetRegistryModule& AssetRegistryModule, const FAssetData& Asset, TSet<FAssetData>& DependentAssets)
{
	if (Asset.IsValid())
	{
		UObject* Obj = Asset.GetAsset();
		if (Obj)
		{
			const FName SelectedPackageName = Obj->GetOutermost()->GetFName();
			FString PackageString = SelectedPackageName.ToString();
			FString ObjectString = FString::Printf(TEXT("%s.%s"), *PackageString, *FPackageName::GetLongPackageAssetName(PackageString));

			if (!DependentAssets.Contains(Asset))
			{
				DependentAssets.Add(Asset);

				TArray<FName> Dependencies;
				AssetRegistryModule.Get().GetDependencies(SelectedPackageName, Dependencies, UE::AssetRegistry::EDependencyCategory::Package);

				for (const FName& Dependency : Dependencies)
				{
					const FString DependencyPackageString = Dependency.ToString();
					FString DependencyObjectString = FString::Printf(TEXT("%s.%s"), *DependencyPackageString, *FPackageName::GetLongPackageAssetName(DependencyPackageString));

					// recurse on each dependency
					FAssetData DependentAsset = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(DependencyObjectString));

					FindAssetDependencies(AssetRegistryModule, DependentAsset, DependentAssets);
				}
			}
		}
	}
}

void FDataValidationModule::ValidateAssets(const TArray<FAssetData>& SelectedAssets, bool bValidateDependencies, const EDataValidationUsecase InValidationUsecase)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	TSet<FAssetData> DependentAssets;

	if (bValidateDependencies)
	{
		for (const FAssetData& Asset : SelectedAssets)
		{
			FindAssetDependencies(AssetRegistryModule, Asset, DependentAssets);
		}
	}

	UEditorValidatorSubsystem* EditorValidationSubsystem = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	if (EditorValidationSubsystem)
	{
		FValidateAssetsSettings Settings;
		FValidateAssetsResults Results;

		Settings.bSkipExcludedDirectories = false;
		Settings.bShowIfNoFailures = true;
		Settings.ValidationUsecase = InValidationUsecase;

		EditorValidationSubsystem->ValidateAssetsWithSettings(bValidateDependencies ? DependentAssets.Array() : SelectedAssets, Settings, Results);
	}
}

void FDataValidationModule::ValidateFolders(const TArray<FString>& SelectedFolders, const EDataValidationUsecase InValidationUsecase)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

	// Form a filter from the paths
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	for (const FString& Folder : SelectedFolders)
	{
		Filter.PackagePaths.Emplace(*Folder);
	}

	// Query for a list of assets in the selected paths
	TArray<FAssetData> AssetList;
	AssetRegistryModule.Get().GetAssets(Filter, AssetList);

	// UE-144978 : Remove ExternalActors & ExternalObjects from assets to be validated.
	// If ExternalActors are not loaded, they will spam the validation log as they can't
	// be loaded on the fly like other assets.
	auto IsAssetPackageExternal = [](const FAssetData& AssetData)
	{
		FString ObjectPath = AssetData.GetObjectPathString();
		FStringView ClassName, PackageName, ObjectName, SubObjectName;
		FPackageName::SplitFullObjectPath(FStringView(ObjectPath), ClassName, PackageName, ObjectName, SubObjectName);

		return FName(PackageName) != AssetData.PackageName;
	};
	AssetList.SetNum(Algo::RemoveIf(AssetList, IsAssetPackageExternal));

	ValidateAssets(AssetList, false, InValidationUsecase);
}

void FDataValidationModule::OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext)
{
	UEditorValidatorSubsystem* EditorValidationSubsystem = GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();
	if (EditorValidationSubsystem && Package)
	{
		EditorValidationSubsystem->ValidateSavedPackage(Package->GetFName(), ObjectSaveContext.IsProceduralSave());
	}
}

#undef LOCTEXT_NAMESPACE
