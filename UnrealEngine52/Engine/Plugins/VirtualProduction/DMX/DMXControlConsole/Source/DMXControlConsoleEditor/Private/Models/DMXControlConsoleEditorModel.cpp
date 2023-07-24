// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorModel.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorFromLegacyUpgradeHandler.h"
#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"

#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "IContentBrowserSingleton.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/Transactor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorModel"

void UDMXControlConsoleEditorModel::LoadConsoleFromConfig()
{
	if (UDMXControlConsole* DefaultConsole = Cast<UDMXControlConsole>(DefaultConsolePath.TryLoad()))
	{
		EditorConsole = DefaultConsole;
	}
	else
	{
		CreateNewConsole();
	}

	OnConsoleLoadedDelegate.Broadcast();
}

void UDMXControlConsoleEditorModel::CreateNewConsole()
{
	if (EditorConsole) // This may be invalid when the editor starts up and there's no console 
	{
		const FText SaveBeforeNewConsoleDialogText = LOCTEXT("SaveBeforeNewDialog", "Current Control Console has unsaved changes. Would you like to save now?");

		if (EditorConsole->IsAsset() && EditorConsole->GetOutermost()->IsDirty())
		{
			const EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::YesNo, SaveBeforeNewConsoleDialogText);
			if (DialogResult == EAppReturnType::Yes)
			{
				SaveConsole();
			}
		}
		else if(!EditorConsole->IsAsset() && EditorConsole->GetControlConsoleData() && !EditorConsole->GetControlConsoleData()->GetAllFaderGroups().IsEmpty())
		{
			const EAppReturnType::Type DialogResult = FMessageDialog::Open(EAppMsgType::YesNo, SaveBeforeNewConsoleDialogText);
			if (DialogResult == EAppReturnType::Yes)
			{
				SaveConsoleAs();
			}
		}
	}

	const FScopedTransaction CreateNewConsoleTransaction(LOCTEXT("CreateNewConsoleTransaction", "Create new Control Console"));

	Modify();
	EditorConsole = NewObject<UDMXControlConsole>(GetTransientPackage(), NAME_None, RF_Transactional);

	SaveConsoleToConfig();

	OnConsoleLoadedDelegate.Broadcast();
}

void UDMXControlConsoleEditorModel::SaveConsole()
{
	if (!ensureMsgf(EditorConsole, TEXT("Cannot save console as asset. Editor Console is null.")))
	{
		return;
	}

	if (EditorConsole->IsAsset())
	{
		// Save existing console asset
		constexpr bool bCheckDirty = false;
		constexpr bool bPromptToSave = false;
		FEditorFileUtils::EPromptReturnCode PromptReturnCode = FEditorFileUtils::PromptForCheckoutAndSave({ EditorConsole->GetPackage() }, bCheckDirty, bPromptToSave);
	}
	else
	{
		// Save as a new console
		SaveConsoleAs();
	}

	OnConsoleSavedDelegate.Broadcast();
}

void UDMXControlConsoleEditorModel::SaveConsoleAs()
{
	// Save a new console asset
	FString PackagePath;
	FString AssetName;
	if (PromptSaveConsolePackage(PackagePath, AssetName))
	{
		UDMXControlConsole* NewConsole = CreateNewConsoleAsset(PackagePath, AssetName, EditorConsole->GetControlConsoleData());
		if (NewConsole)
		{
			FDMXControlConsoleEditorManager::Get().GetSelectionHandler()->ClearSelection();

			const FScopedTransaction SaveConsoleAsTransaction(LOCTEXT("SaveConsoleAsTransaction", "Save Control Console Console to new Asset"));

			Modify();
			EditorConsole = NewConsole;

			SaveConsoleToConfig();

			OnConsoleLoadedDelegate.Broadcast();
		}
	}
}

void UDMXControlConsoleEditorModel::LoadConsole(const FAssetData& AssetData)
{	
	UDMXControlConsole* NewEditorConsole = Cast<UDMXControlConsole>(AssetData.GetAsset());
	if (NewEditorConsole)
	{
		UDMXControlConsoleData* CurrentControlConsoleData = EditorConsole->GetControlConsoleData();
		if (CurrentControlConsoleData)
		{
			CurrentControlConsoleData->StopSendingDMX();
		}

		FDMXControlConsoleEditorManager::Get().GetSelectionHandler()->ClearSelection();

		const FScopedTransaction LoadConsoleTransaction(LOCTEXT("LoadConsoleTransaction", "Load Control Console"));

		Modify();
		EditorConsole = NewEditorConsole;

		SaveConsoleToConfig();

		OnConsoleLoadedDelegate.Broadcast();
	}
}

UDMXControlConsole* UDMXControlConsoleEditorModel::CreateNewConsoleAsset(const FString& SavePackagePath, const FString& SaveAssetName, UDMXControlConsoleData* SourceControlConsoleData) const
{
	if (!ensureMsgf(SourceControlConsoleData, TEXT("Cannot create a console asset from a null control console")))
	{
		return nullptr;
	}

	if (!ensureMsgf(FPackageName::IsValidLongPackageName(SavePackagePath / SaveAssetName), TEXT("Invalid package name when trying to save create Control Console asset. Failed to create asset.")))
	{
		return nullptr;
	}

	const FString PackageName = SavePackagePath / SaveAssetName;
	UPackage* Package = CreatePackage(*PackageName);
	check(Package);
	Package->FullyLoad();

	UDMXControlConsole* NewConsole = NewObject<UDMXControlConsole>(Package, FName(SaveAssetName), RF_Public | RF_Standalone | RF_Transactional);
	NewConsole->CopyControlConsoleData(SourceControlConsoleData);

	constexpr bool bCheckDirty = false;
	constexpr bool bPromptToSave = false;
	FEditorFileUtils::EPromptReturnCode PromptReturnCode = FEditorFileUtils::PromptForCheckoutAndSave({ NewConsole->GetPackage() }, bCheckDirty, bPromptToSave);
	if (PromptReturnCode != FEditorFileUtils::EPromptReturnCode::PR_Success)
	{
		return nullptr;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetRegistryModule::AssetCreated(NewConsole);

	return NewConsole;
}

void UDMXControlConsoleEditorModel::PostInitProperties()
{
	Super::PostInitProperties();

	if (ensureMsgf(HasAnyFlags(RF_ClassDefaultObject), TEXT("DMXControlConsoleEditorConsoleModel should not be instantiated. Instead refer to the CDO.")))
	{
		// Deffer initialization to engine being fully loaded
		FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UDMXControlConsoleEditorModel::OnFEngineLoopInitComplete);
	}
}

void UDMXControlConsoleEditorModel::BeginDestroy()
{
	Super::BeginDestroy();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		SaveConsoleToConfig();
	}
}

void UDMXControlConsoleEditorModel::SaveConsoleToConfig()
{
	if (DefaultConsolePath != EditorConsole)
	{
		DefaultConsolePath = IsValid(EditorConsole) && EditorConsole->IsAsset() ? EditorConsole : nullptr;
		SaveConfig();
	}
}

void UDMXControlConsoleEditorModel::FinalizeLoadConsole(UDMXControlConsole* ConsoleToLoad)
{
	if (!ensureMsgf(ConsoleToLoad, TEXT("Cannot load null control console. Finalize Load Console failed.")))
	{
		return;
	}

	// Stop sending DMX before loading a new console
	UDMXControlConsoleData* CurrentControlConsoleData = EditorConsole->GetControlConsoleData();
	if (CurrentControlConsoleData)
	{
		CurrentControlConsoleData->StopSendingDMX();
	}

	FDMXControlConsoleEditorManager::Get().GetSelectionHandler()->ClearSelection();

	Modify();
	EditorConsole = ConsoleToLoad;

	SaveConsoleToConfig();

	OnConsoleLoadedDelegate.Broadcast();
}

void UDMXControlConsoleEditorModel::OnLoadDialogEnterPressedConsole(const TArray<FAssetData>& InAssets)
{
	if (!ensureMsgf(InAssets.Num() == 1, TEXT("Unexpected none or many assets selected when loading Control Console.")))
	{
		return;
	}

	// Continue as if the console was selected direct
	OnLoadDialogSelectedConsole(InAssets[0]);
}

void UDMXControlConsoleEditorModel::OnLoadDialogSelectedConsole(const FAssetData& Asset)
{
	UDMXControlConsole* ConsoleToLoad = Cast<UDMXControlConsole>(Asset.GetAsset());
	if (!ensureAlwaysMsgf(ConsoleToLoad && ConsoleToLoad->GetControlConsoleData(), TEXT("Invalid Control Console or Data in Console when loading console.")))
	{
		return;
	}

	FinalizeLoadConsole(ConsoleToLoad);
}

bool UDMXControlConsoleEditorModel::OpenSaveDialog(const FString& InDefaultPath, const FString& InNewNameSuggestion, FString& OutPackageName) const
{
	FSaveAssetDialogConfig SaveAssetDialogConfig;
	{
		SaveAssetDialogConfig.DefaultPath = InDefaultPath;
		SaveAssetDialogConfig.DefaultAssetName = InNewNameSuggestion;
		SaveAssetDialogConfig.AssetClassNames.Add(UDMXControlConsole::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveConsoleDialogTitle", "Save Control Console");
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

bool UDMXControlConsoleEditorModel::PromptSaveConsolePackage(FString& OutSavePackagePath, FString& OutSaveAssetName) const
{
	FString AssetName;
	FString DialogStartPath;
	if (EditorConsole->GetPackage() == GetTransientPackage())
	{
		DialogStartPath = FPaths::ProjectContentDir();
	}
	else
	{
		AssetName = EditorConsole->GetName();
		DialogStartPath = EditorConsole->GetOutermost()->GetName();
	}

	FString UniquePackageName;
	FString UniqueAssetName;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(DialogStartPath / AssetName, TEXT(""), UniquePackageName, UniqueAssetName);

	const FString DialogStartName = FPaths::GetCleanFilename(DialogStartPath / AssetName);

	// Show a save dialog and let the user select a package name
	FString SaveObjectPath;
	bool bFilenameValid = false;
	while (!bFilenameValid)
	{
		if (!OpenSaveDialog(DialogStartPath, DialogStartName, SaveObjectPath))
		{
			return false;
		}

		FText OutError;
		bFilenameValid = FFileHelper::IsFilenameValidForSaving(SaveObjectPath, OutError);
	}

	const FString SavePackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
	OutSavePackagePath = FPaths::GetPath(SavePackageName);
	OutSaveAssetName = FPaths::GetBaseFilename(SavePackageName);

	return true;
}

void UDMXControlConsoleEditorModel::OnFEngineLoopInitComplete()
{
	if (ensureMsgf(HasAnyFlags(RF_ClassDefaultObject), TEXT("DMXControlConsoleEditorConsoleModel should not be instantiated. Use the CDO instead.")))
	{
		SetFlags(GetFlags() | RF_Transactional);

		// Try UpgradePath if configurations settings have data from Output Consoles, the Console that was used before 5.2.
		// Otherwise load the default console
		if (!FDMXControlConsoleEditorFromLegacyUpgradeHandler::TryUpgradePathFromLegacy())
		{
			LoadConsoleFromConfig();
		}
	}
}

#undef LOCTEXT_NAMESPACE
