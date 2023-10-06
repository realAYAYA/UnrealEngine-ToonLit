// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorModel.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "ContentBrowserModule.h"
#include "DMXControlConsole.h"
#include "DMXControlConsoleEditorFromLegacyUpgradeHandler.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "IContentBrowserSingleton.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXLibrary.h"
#include "Models/Filter/FilterModel.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorModel"

UDMXControlConsoleData* UDMXControlConsoleEditorModel::GetEditorConsoleData() const
{ 
	return EditorConsole ? EditorConsole->GetControlConsoleData() : nullptr;
}

UDMXControlConsoleEditorLayouts* UDMXControlConsoleEditorModel::GetEditorConsoleLayouts() const
{
	return EditorConsole ? Cast<UDMXControlConsoleEditorLayouts>(EditorConsole->ControlConsoleEditorLayouts) : nullptr;
}

TSharedRef<FDMXControlConsoleEditorSelection> UDMXControlConsoleEditorModel::GetSelectionHandler()
{
	if (!SelectionHandler.IsValid())
	{
		SelectionHandler = MakeShareable(new FDMXControlConsoleEditorSelection());
	}

	return SelectionHandler.ToSharedRef();
}

void UDMXControlConsoleEditorModel::SetFaderGroupsViewMode(EDMXControlConsoleEditorViewMode ViewMode)
{
	FaderGroupsViewMode = ViewMode;
	OnFaderGroupsViewModeChanged.Broadcast();
}

void UDMXControlConsoleEditorModel::SetFadersViewMode(EDMXControlConsoleEditorViewMode ViewMode)
{
	FadersViewMode = ViewMode;
	OnFadersViewModeChanged.Broadcast();
}

void UDMXControlConsoleEditorModel::ToggleAutoSelectActivePatches()
{
	bAutoSelectActivePatches = !bAutoSelectActivePatches;
	SaveConfig();
}

void UDMXControlConsoleEditorModel::ToggleAutoSelectFilteredElements()
{
	bAutoSelectFilteredElements = !bAutoSelectFilteredElements;
	SaveConfig();
}

void UDMXControlConsoleEditorModel::ToggleSendDMX()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (!ensureMsgf(EditorConsoleData, TEXT("Invalid Editor Control Console Data, can't send DMX correctly.")))
	{
		return;
	}

	if (EditorConsoleData->IsSendingDMX())
	{
		EditorConsoleData->StopSendingDMX();
	}
	else
	{
		EditorConsoleData->StartSendingDMX();
	}
}

bool UDMXControlConsoleEditorModel::IsSendingDMX() const
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (ensureMsgf(EditorConsoleData, TEXT("Invalid Editor Control Console Data, cannot deduce if it is sending DMX.")))
	{
		return EditorConsoleData->IsSendingDMX();
	}
	return false;
}

void UDMXControlConsoleEditorModel::RemoveAllSelectedElements()
{
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = GetEditorConsoleLayouts();
	if (!EditorConsoleLayouts || !SelectionHandler)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout || ActiveLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupsObjects = SelectionHandler->GetSelectedFaderGroups();
	if (SelectedFaderGroupsObjects.IsEmpty())
	{
		return;
	}

	const FScopedTransaction RemoveAllSelectedElementsTransaction(LOCTEXT("RemoveAllSelectedElementsTransaction", "Selected Elements removed"));

	// Delete all selected fader groups
	for (const TWeakObjectPtr<UObject>& SelectedFaderGroupObject : SelectedFaderGroupsObjects)
	{
		UDMXControlConsoleFaderGroup* SelectedFaderGroup = Cast<UDMXControlConsoleFaderGroup>(SelectedFaderGroupObject);
		if (SelectedFaderGroup &&
			SelectionHandler->GetSelectedFadersFromFaderGroup(SelectedFaderGroup).IsEmpty())
		{
			// If there's only one fader group to delete, replace it in selection
			if (SelectedFaderGroupsObjects.Num() == 1)
			{
				SelectionHandler->ReplaceInSelection(SelectedFaderGroup);
			}

			constexpr bool bNotifySelectedFaderGroupChange = false;
			SelectionHandler->RemoveFromSelection(SelectedFaderGroup, bNotifySelectedFaderGroupChange);

			ActiveLayout->PreEditChange(nullptr);
			ActiveLayout->RemoveFromLayout(SelectedFaderGroup);
			ActiveLayout->PostEditChange();

			if (!SelectedFaderGroup->HasFixturePatch())
			{
				SelectedFaderGroup->Destroy();
			}
		}
	}

	// Delete all selected faders
	const TArray<TWeakObjectPtr<UObject>> SelectedFadersObjects = SelectionHandler->GetSelectedFaders();
	if (!SelectedFadersObjects.IsEmpty())
	{
		for (TWeakObjectPtr<UObject> SelectedFaderObject : SelectedFadersObjects)
		{
			UDMXControlConsoleFaderBase* SelectedFader = Cast<UDMXControlConsoleFaderBase>(SelectedFaderObject);
			const bool bValidFaderWithPatch = SelectedFader && !SelectedFader->GetOwnerFaderGroupChecked().HasFixturePatch();
			if (!bValidFaderWithPatch)
			{
				continue;
			}

			// If there's only one fader to delete, replace it in selection
			if (SelectedFadersObjects.Num() == 1)
			{
				SelectionHandler->ReplaceInSelection(SelectedFader);
			}

			constexpr bool bNotifyFaderSelectionChange = false;
			SelectionHandler->RemoveFromSelection(SelectedFader, bNotifyFaderSelectionChange);

			SelectedFader->Destroy();
		}
	}

	SelectionHandler->RemoveInvalidObjectsFromSelection();
}

void UDMXControlConsoleEditorModel::ClearAll()
{
	const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = GetEditorConsoleLayouts();
	if (!ensureMsgf(EditorConsoleLayouts, TEXT("Invalid Editor Console Layouts, cannot clear the active layout.")))
	{
		return;
	}

	SelectionHandler->ClearSelection();
	UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
	if (!ActiveLayout)
	{
		return;
	}

	const FScopedTransaction ClearAllTransaction(LOCTEXT("ClearAllTransaction", "Clear All"));
	ActiveLayout->Modify();
	ActiveLayout->ClearAll();
	if (ActiveLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass())
	{
		const TArray<UDMXControlConsoleEditorGlobalLayoutUser*> UserLayouts = EditorConsoleLayouts->GetUserLayouts();
		for (UDMXControlConsoleEditorGlobalLayoutUser* UserLayout : UserLayouts)
		{
			UserLayout->Modify();
			UserLayout->ClearAllPatchedFaderGroups();
			UserLayout->ClearEmptyLayoutRows();
		}

		if (UDMXControlConsoleData* ControlconsoleData = GetEditorConsoleData())
		{
			constexpr bool bOnlyPatchedFaderGroups = true;
			ControlconsoleData->Modify();
			ControlconsoleData->ClearAll(bOnlyPatchedFaderGroups);
		}
	}
}

void UDMXControlConsoleEditorModel::ScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	OnScrollFaderGroupIntoView.Broadcast(FaderGroup);
}

void UDMXControlConsoleEditorModel::LoadConsoleFromConfig()
{
	if (UDMXControlConsole* DefaultConsole = Cast<UDMXControlConsole>(DefaultConsolePath.TryLoad()))
	{
		EditorConsole = DefaultConsole;

		InitializeEditorLayouts();
		BindToDMXLibraryChanges();
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

	UnregisterEditorLayouts();
	UnbindFromDMXLibraryChanges();

	const FScopedTransaction CreateNewConsoleTransaction(LOCTEXT("CreateNewConsoleTransaction", "Create new Control Console"));
	UDMXControlConsole* NewConsole = NewObject<UDMXControlConsole>(GetTransientPackage(), NAME_None, RF_Transactional);
	if (EditorConsole)
	{
		NewConsole->CopyControlConsoleData(EditorConsole->GetControlConsoleData());
	}
	Modify();
	EditorConsole = NewConsole;

	InitializeEditorLayouts();
	BindToDMXLibraryChanges();
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
		if (!NewConsole)
		{
			return;
		}

		UnregisterEditorLayouts();
		UnbindFromDMXLibraryChanges();
		SelectionHandler->ClearSelection();

		const FScopedTransaction SaveConsoleAsTransaction(LOCTEXT("SaveConsoleAsTransaction", "Save Control Console Console to new Asset"));

		Modify();
		EditorConsole = NewConsole;

		InitializeEditorLayouts();
		BindToDMXLibraryChanges();
		SaveConsoleToConfig();

		OnConsoleLoadedDelegate.Broadcast();
	}
}

void UDMXControlConsoleEditorModel::LoadConsole(const FAssetData& AssetData)
{	
	UDMXControlConsole* NewEditorConsole = Cast<UDMXControlConsole>(AssetData.GetAsset());
	if (!NewEditorConsole)
	{
		return;
	}

	UnregisterEditorLayouts();
	UnbindFromDMXLibraryChanges();

	UDMXControlConsoleData* CurrentControlConsoleData = EditorConsole ? EditorConsole->GetControlConsoleData() : nullptr;
	if (CurrentControlConsoleData)
	{
		CurrentControlConsoleData->StopSendingDMX();
	}

	// Selection handler may not yet be valid, as the getter creates it
	if (!SelectionHandler.IsValid())
	{
		SelectionHandler = MakeShared<FDMXControlConsoleEditorSelection>();
	}
	SelectionHandler->ClearSelection();

	const FScopedTransaction LoadConsoleTransaction(LOCTEXT("LoadConsoleTransaction", "Load Control Console"));

	Modify();
	EditorConsole = NewEditorConsole;

	InitializeEditorLayouts();
	BindToDMXLibraryChanges();
	SaveConsoleToConfig();

	OnConsoleLoadedDelegate.Broadcast();
}

void UDMXControlConsoleEditorModel::RequestRefresh()
{
	if (!ForceRefreshTimerHandle.IsValid())
	{
		ForceRefreshTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UDMXControlConsoleEditorModel::ForceRefresh));
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

void UDMXControlConsoleEditorModel::SaveFixturePatchListDescriptorToConfig(const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor)
{
	FixturePatchListDescriptor = ListDescriptor;
	SaveConfig();
}

void UDMXControlConsoleEditorModel::PostInitProperties()
{
	Super::PostInitProperties();

	if (ensureMsgf(HasAnyFlags(RF_ClassDefaultObject), TEXT("DMXControlConsoleEditorConsoleModel should not be instantiated. Instead refer to the CDO.")))
	{
		// Deffer initialization to engine being fully loaded
		FCoreDelegates::OnFEngineLoopInitComplete.AddUObject(this, &UDMXControlConsoleEditorModel::OnFEngineLoopInitComplete);
		FCoreDelegates::OnEnginePreExit.AddUObject(this, &UDMXControlConsoleEditorModel::OnEnginePreExit);
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

void UDMXControlConsoleEditorModel::ForceRefresh()
{
	ForceRefreshTimerHandle.Invalidate();
	OnControlConsoleForceRefresh.Broadcast();
}

void UDMXControlConsoleEditorModel::BindToDMXLibraryChanges()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (EditorConsoleData && !EditorConsoleData->GetOnDMXLibraryChanged().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnDMXLibraryChanged().AddUObject(this, &UDMXControlConsoleEditorModel::OnDMXLibraryChanged);
	}
}

void UDMXControlConsoleEditorModel::UnbindFromDMXLibraryChanges()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	if (EditorConsoleData && EditorConsoleData->GetOnDMXLibraryChanged().IsBoundToObject(this))
	{
		EditorConsoleData->GetOnDMXLibraryChanged().RemoveAll(this);
	}
}

void UDMXControlConsoleEditorModel::InitializeEditorLayouts()
{
	if (EditorConsole && !EditorConsole->ControlConsoleEditorLayouts)
	{
		UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = NewObject<UDMXControlConsoleEditorLayouts>(EditorConsole, NAME_None, RF_Transactional);
		EditorConsole->ControlConsoleEditorLayouts = EditorConsoleLayouts;
		
		UDMXControlConsoleEditorGlobalLayoutDefault* DefaultLayout = EditorConsoleLayouts->GetDefaultLayout();
		EditorConsoleLayouts->SetActiveLayout(DefaultLayout);
		EditorConsoleLayouts->UpdateDefaultLayout(GetEditorConsoleData());
	}

	RegisterEditorLayouts();
}

void UDMXControlConsoleEditorModel::RegisterEditorLayouts()
{
	UDMXControlConsoleEditorLayouts* EditorLayouts = GetEditorConsoleLayouts();
	if (!EditorLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutDefault* DefaultLayout = EditorLayouts->GetDefaultLayout();
	if (DefaultLayout)
	{
		DefaultLayout->Register();
	}
}

void UDMXControlConsoleEditorModel::UnregisterEditorLayouts()
{
	UDMXControlConsoleEditorLayouts* EditorLayouts = GetEditorConsoleLayouts();
	if (!EditorLayouts)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutDefault* DefaultLayout = EditorLayouts->GetDefaultLayout();
	if (DefaultLayout)
	{
		DefaultLayout->Unregister();
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

	UnregisterEditorLayouts();
	UnbindFromDMXLibraryChanges();
	SelectionHandler->ClearSelection();

	Modify();
	EditorConsole = ConsoleToLoad;

	InitializeEditorLayouts();
	BindToDMXLibraryChanges();
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

void UDMXControlConsoleEditorModel::OnDMXLibraryChanged()
{
	UDMXControlConsoleData* EditorConsoleData = GetEditorConsoleData();
	UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = GetEditorConsoleLayouts();
	if (!EditorConsoleData || !EditorConsoleLayouts)
	{
		return;
	}

	// Clear user layouts from patched fader groups
	const TArray<UDMXControlConsoleEditorGlobalLayoutUser*> UserLayouts = EditorConsoleLayouts->GetUserLayouts();
	for (UDMXControlConsoleEditorGlobalLayoutUser* UserLayout : UserLayouts)
	{
		if (!UserLayout)
		{
			continue;
		}

		UserLayout->PreEditChange(nullptr);
		UserLayout->ClearAllPatchedFaderGroups();
		UserLayout->ClearEmptyLayoutRows();
		UserLayout->PostEditChange();
	}

	// Regenerate control console data with new library data
	EditorConsoleData->PreEditChange(nullptr);
	EditorConsoleData->ClearPatchedFaderGroups();
	EditorConsoleData->GenerateFromDMXLibrary();
	EditorConsoleData->PostEditChange();

	// Update current console default layout
	EditorConsoleLayouts->PreEditChange(nullptr);
	EditorConsoleLayouts->UpdateDefaultLayout(EditorConsoleData);
	EditorConsoleLayouts->PostEditChange();

	RequestRefresh();
}

void UDMXControlConsoleEditorModel::OnFEngineLoopInitComplete()
{
	using namespace UE::DMXControlConsoleEditor::FilterModel::Private;

	if (ensureMsgf(HasAnyFlags(RF_ClassDefaultObject), TEXT("DMXControlConsoleEditorConsoleModel should not be instantiated. Use the CDO instead.")))
	{
		SetFlags(GetFlags() | RF_Transactional);

		// Try UpgradePath if configurations settings have data from Output Consoles, the Console that was used before 5.2.
		// Otherwise load the default console
		if (!FDMXControlConsoleEditorFromLegacyUpgradeHandler::TryUpgradePathFromLegacy())
		{
			LoadConsoleFromConfig();
			
			FilterModel = MakeShared<FFilterModel>();
			FilterModel->Initialize();
		}
	}
}

void UDMXControlConsoleEditorModel::OnEnginePreExit()
{
	UDMXControlConsoleData* EditorConsoleData = EditorConsole ? EditorConsole->GetControlConsoleData() : nullptr;
	if (EditorConsoleData)
	{
		EditorConsoleData->StopSendingDMX();
	}
}

#undef LOCTEXT_NAMESPACE
