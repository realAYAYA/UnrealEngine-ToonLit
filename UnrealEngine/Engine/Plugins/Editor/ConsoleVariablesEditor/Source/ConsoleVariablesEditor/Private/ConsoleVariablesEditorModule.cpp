// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConsoleVariablesEditorModule.h"

#include "AssetTypeActions/AssetTypeActions_ConsoleVariables.h"
#include "ConsoleVariablesEditorCommandInfo.h"
#include "ConsoleVariablesEditorLog.h"
#include "ConsoleVariablesEditorProjectSettings.h"
#include "ConsoleVariablesEditorStyle.h"
#include "MultiUser/ConsoleVariableSync.h"
#include "MultiUser/ConsoleVariableSyncData.h"
#include "Views/MainPanel/ConsoleVariablesEditorMainPanel.h"

#include "Algo/AllOf.h"
#include "Algo/Find.h"
#include "Framework/Docking/TabManager.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FConsoleVariablesEditorModule"

const FName FConsoleVariablesEditorModule::ConsoleVariablesToolkitPanelTabId(TEXT("ConsoleVariablesToolkitPanel"));

FConsoleVariablesEditorModule& FConsoleVariablesEditorModule::Get()
{
	return FModuleManager::GetModuleChecked<FConsoleVariablesEditorModule>("ConsoleVariablesEditor");
}

void FConsoleVariablesEditorModule::StartupModule()
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_ConsoleVariables>());

	FConsoleVariablesEditorStyle::Initialize();

	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FConsoleVariablesEditorModule::OnFEngineLoopInitComplete);
}

void FConsoleVariablesEditorModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(this);

	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

	FConsoleVariablesEditorStyle::Shutdown();
	
	MainPanel.Reset();

	ConsoleObjectsMainReference.Empty();

	// Unregister project settings
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	{
		SettingsModule.UnregisterSettings("Project", "Plugins", "Console Variables Editor");
	}
}

void FConsoleVariablesEditorModule::SavePreset() const
{
	if (MainPanel.IsValid())
	{
		MainPanel->SaveCurrentPreset();
	}
}

void FConsoleVariablesEditorModule::SaveSpecificPreset(const TObjectPtr<UConsoleVariablesAsset> Preset) const
{
	if (MainPanel.IsValid())
	{
		MainPanel->SaveSpecificPreset(Preset);
	}
}

void FConsoleVariablesEditorModule::SavePresetAs() const
{
	if (MainPanel.IsValid())
	{
		MainPanel->SaveCurrentPresetAs();
	}
}

void FConsoleVariablesEditorModule::SaveSpecificPresetAs(const TObjectPtr<UConsoleVariablesAsset> Preset) const
{
	if (MainPanel.IsValid())
	{
		MainPanel->SaveSpecificPresetAs(Preset);
	}
}

void FConsoleVariablesEditorModule::OpenConsoleVariablesDialogWithPreset(
	const TObjectPtr<UConsoleVariablesAsset> Preset) const
{
	if (MainPanel.IsValid())
	{
		MainPanel->ImportPreset(Preset);
	}
}

void FConsoleVariablesEditorModule::OpenConsoleVariablesDialogWithAssetSelected(const FAssetData& InAssetData) const
{
	if (InAssetData.IsValid())
	{
		OpenConsoleVariablesEditor();
	}

	if (MainPanel.IsValid())
	{
		MainPanel->ImportPreset(InAssetData);
	}
}

void FConsoleVariablesEditorModule::QueryAndBeginTrackingConsoleVariables()
{
	const int32 VariableCount = ConsoleObjectsMainReference.Num();
	
	ConsoleObjectsMainReference.Empty(VariableCount);
	
	IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda(
		[this] (const TCHAR* Key, IConsoleObject* ConsoleObject)
		{
			if (!ConsoleObject || ConsoleObject->TestFlags(ECVF_Unregistered))
			{
				return;
			}

			const TSharedRef<FConsoleVariablesEditorCommandInfo> Info =
				MakeShared<FConsoleVariablesEditorCommandInfo>(Key);
			
			Info->StartupSource = Info->GetSource();
			Info->OnDetectConsoleObjectUnregisteredHandle = Info->OnDetectConsoleObjectUnregistered.AddRaw(
				this, &FConsoleVariablesEditorModule::OnDetectConsoleObjectUnregistered);

			if (IConsoleVariable* AsVariable = ConsoleObject->AsVariable())
			{
				Info->OnVariableChangedCallbackHandle = AsVariable->OnChangedDelegate().AddRaw(
					this, &FConsoleVariablesEditorModule::OnConsoleVariableChanged);
			}
			
			AddConsoleObjectCommandInfoToMainReference(Info);
		}),
		TEXT(""));
}

TWeakPtr<FConsoleVariablesEditorCommandInfo> FConsoleVariablesEditorModule::FindCommandInfoByName(const FString& NameToSearch, ESearchCase::Type InSearchCase)
{
	TSharedPtr<FConsoleVariablesEditorCommandInfo>* Match = Algo::FindByPredicate(
		ConsoleObjectsMainReference,
		[&NameToSearch, InSearchCase](const TSharedPtr<FConsoleVariablesEditorCommandInfo> Comparator)
		{
			return Comparator->Command.Equals(NameToSearch, InSearchCase);
		});

	return Match ? *Match : nullptr;
}

TArray<TWeakPtr<FConsoleVariablesEditorCommandInfo>> FConsoleVariablesEditorModule::FindCommandInfosMatchingTokens(
	const TArray<FString>& InTokens, ESearchCase::Type InSearchCase)
{
	TArray<TWeakPtr<FConsoleVariablesEditorCommandInfo>> ReturnValue;
	
	for (const TSharedPtr<FConsoleVariablesEditorCommandInfo>& CommandInfo : ConsoleObjectsMainReference)
	{
		FString CommandSearchableText = CommandInfo->Command + " " + CommandInfo->GetHelpText() + " " + CommandInfo->GetSourceAsText().ToString();
		// Match any
		for (const FString& Token : InTokens)
		{
			bool bMatchFound = false;
			
			// Match all of these
			const FString SpaceDelimiter = " ";
			TArray<FString> OutSpacedArray;
			if (Token.Contains(SpaceDelimiter) && Token.ParseIntoArray(OutSpacedArray, *SpaceDelimiter, true) > 1)
			{
				bMatchFound = Algo::AllOf(OutSpacedArray, [&CommandSearchableText, InSearchCase](const FString& Comparator)
				{
					return CommandSearchableText.Contains(Comparator, InSearchCase);
				});
			}
			else
			{
				bMatchFound = CommandSearchableText.Contains(Token, InSearchCase);
			}

			if (bMatchFound)
			{
				ReturnValue.Add(CommandInfo);
			}
		}
	}

	return ReturnValue;
}

TWeakPtr<FConsoleVariablesEditorCommandInfo> FConsoleVariablesEditorModule::FindCommandInfoByConsoleObjectReference(
	IConsoleObject* InConsoleObjectReference)
{
	TSharedPtr<FConsoleVariablesEditorCommandInfo>* Match = Algo::FindByPredicate(
	ConsoleObjectsMainReference,
	[InConsoleObjectReference](const TSharedPtr<FConsoleVariablesEditorCommandInfo> Comparator)
	{
		return Comparator->GetConsoleObjectPtr() == InConsoleObjectReference;
	});

	return Match ? *Match : nullptr;
}

TObjectPtr<UConsoleVariablesAsset> FConsoleVariablesEditorModule::GetPresetAsset() const
{
	return EditingPresetAsset;
}

TObjectPtr<UConsoleVariablesAsset> FConsoleVariablesEditorModule::GetGlobalSearchAsset() const
{
	return EditingGlobalSearchAsset;
}

FReply FConsoleVariablesEditorModule::ValidateConsoleInputAndAddToCurrentPreset(const FText& CommittedText) const
{
	return MainPanel->ValidateConsoleInputAndAddToCurrentPreset(CommittedText);
}

void FConsoleVariablesEditorModule::RebuildList(const FString InConsoleCommandToScrollTo, bool bShouldCacheValues) const
{
	MainPanel->RebuildList(InConsoleCommandToScrollTo, bShouldCacheValues);
}

void FConsoleVariablesEditorModule::RefreshList() const
{
	MainPanel->RefreshList();
}

void FConsoleVariablesEditorModule::UpdatePresetValuesForSave(TObjectPtr<UConsoleVariablesAsset> InAsset)
{
	MainPanel->UpdatePresetValuesForSave(InAsset);
}

bool FConsoleVariablesEditorModule::PopulateGlobalSearchAssetWithVariablesMatchingTokens(const TArray<FString>& InTokens)
{
	// Remove existing commands
	TArray<FConsoleVariablesEditorAssetSaveData> NullList;
	EditingGlobalSearchAsset->ReplaceSavedCommands(NullList);
	
	for (const TWeakPtr<FConsoleVariablesEditorCommandInfo>& CommandInfo : FindCommandInfosMatchingTokens(InTokens))
	{
		EditingGlobalSearchAsset->AddOrSetConsoleObjectSavedData(
			{
				CommandInfo.Pin()->Command,
				CommandInfo.Pin()->GetConsoleVariablePtr() ?
					CommandInfo.Pin()->GetConsoleVariablePtr()->GetString() : "",
				ECheckBoxState::Checked
			}
		);
	}

	return EditingGlobalSearchAsset->GetSavedCommandsCount() > 0;
}

void FConsoleVariablesEditorModule::SendMultiUserConsoleVariableChange(ERemoteCVarChangeType InChangeType, const FString& InVariableName, const FString& InValueAsString)
{
	if (GEditor && GEditor->IsPlaySessionInProgress() && !bHaveWarnedAboutPIE)
	{
		UE_LOG(LogConsoleVariablesEditor, Warning, TEXT("%hs: Play In Editor is about to start or has started; Multi-User Cvar sync is suspended during PIE."), __FUNCTION__);
		bHaveWarnedAboutPIE = true;
		return;
	}

	bHaveWarnedAboutPIE = false;
	MainPanel->GetMultiUserManager().SendConsoleVariableChange(InChangeType, InVariableName, InValueAsString);
}

void FConsoleVariablesEditorModule::OnRemoteCvarChanged(ERemoteCVarChangeType InChangeType, const FString InName, const FString InValue)
{
	if (GEditor && GEditor->IsPlaySessionInProgress() && !bHaveWarnedAboutPIE)
	{
		UE_LOG(LogConsoleVariablesEditor, Warning, TEXT("%hs: Play In Editor is about to start or has started; Multi-User Cvar sync is suspended during PIE."), __FUNCTION__);
		bHaveWarnedAboutPIE = true;
		return;
	}

	bHaveWarnedAboutPIE = false;
	UE_LOG(LogConsoleVariablesEditor, VeryVerbose, TEXT("Remote set console variable %s = %s"), *InName, *InValue);

	if (GetDefault<UConcertCVarSynchronization>()->bSyncCVarTransactions)
	{
		FScopeMultiUserReceiveCVar CVarChange(*InName, CommandsReceivedFromMultiUser);

		GEngine->Exec(FConsoleVariablesEditorCommandInfo::GetCurrentWorld(),
					  *FString::Printf(TEXT("%s %s"), *InName, *InValue));

		if (InChangeType == ERemoteCVarChangeType::Remove)
		{
			EditingPresetAsset->RemoveConsoleVariable(InName);
		}
		else
		{
			EditingPresetAsset->AddOrSetConsoleObjectSavedData(
			{
				InName,
				InValue,
				ECheckBoxState::Checked
			}
			);
		}

		if (MainPanel->GetEditorListMode() == FConsoleVariablesEditorList::EConsoleVariablesEditorListMode::Preset)
		{
			MainPanel->RebuildList();
		}
	}
}

void FConsoleVariablesEditorModule::OnFEngineLoopInitComplete()
{
	RegisterMenuItem();
	RegisterProjectSettings();
	QueryAndBeginTrackingConsoleVariables();
	CreateEditingPresets();
	
	MainPanel = MakeShared<FConsoleVariablesEditorMainPanel>();
}

void FConsoleVariablesEditorModule::RegisterMenuItem()
{
	FTabSpawnerEntry& BrowserSpawnerEntry = FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		ConsoleVariablesToolkitPanelTabId,
		FOnSpawnTab::CreateRaw(this, & FConsoleVariablesEditorModule::SpawnMainPanelTab))
			.SetIcon(FSlateIcon(FConsoleVariablesEditorStyle::Get().GetStyleSetName(), "ConsoleVariables.ToolbarButton", "ConsoleVariables.ToolbarButton.Small"))
			.SetDisplayName(LOCTEXT("OpenConsoleVariablesEditorMenuItem", "Console Variables"))
			.SetTooltipText(LOCTEXT("OpenConsoleVariablesEditorTooltip", "Open the Console Variables Editor"))
			.SetMenuType(ETabSpawnerMenuType::Enabled);

	BrowserSpawnerEntry.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());
}

void FConsoleVariablesEditorModule::RegisterProjectSettings() const
{
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	{
		// User Project Settings
		const TSharedPtr<ISettingsSection> ProjectSettingsSectionPtr = SettingsModule.RegisterSettings(
			"Project", "Plugins", "Console Variables Editor",
			LOCTEXT("ConsoleVariablesSettingsCategoryDisplayName", "Console Variables Editor"),
			LOCTEXT("ConsoleVariablesSettingsDescription", "Configure the Console Variables Editor user settings"),
			GetMutableDefault<UConsoleVariablesEditorProjectSettings>());
	}
}

void FConsoleVariablesEditorModule::OnConsoleVariableChanged(IConsoleVariable* ChangedVariable)
{
	if (IsEngineExitRequested())
	{
		return;
	}

	check(EditingPresetAsset);

	if (const TWeakPtr<FConsoleVariablesEditorCommandInfo> CommandInfo =
		FindCommandInfoByConsoleObjectReference(ChangedVariable); CommandInfo.IsValid())
	{
		const TSharedPtr<FConsoleVariablesEditorCommandInfo>& PinnedCommand = CommandInfo.Pin();
		const FString& Key = PinnedCommand->Command;

		FConsoleVariablesEditorAssetSaveData FoundData;
		bool bIsVariableCurrentlyTracked = EditingPresetAsset->FindSavedDataByCommandString(Key, FoundData, ESearchCase::IgnoreCase);

		const UConsoleVariablesEditorProjectSettings* Settings = GetDefault<UConsoleVariablesEditorProjectSettings>();
		check(Settings);

		if (!bIsVariableCurrentlyTracked)
		{
			// If not yet tracked and we want to track variable changes from outside the dialogue,
			// Check if the changed value differs from the startup value before tracking it
			if (Settings->bAddAllChangedConsoleVariablesToCurrentPreset &&
				!Settings->ChangedConsoleVariableSkipList.Contains(Key) && 
				PinnedCommand->IsCurrentValueDifferentFromInputValue(PinnedCommand->StartupValueAsString))
			{
				if (MainPanel.IsValid())
				{
					MainPanel->AddConsoleObjectToCurrentPreset(
						Key,
						// If we're not in preset mode then pass empty value
						// This forces the row to get the current value at the time it's generated
						MainPanel->GetEditorListMode() ==
						FConsoleVariablesEditorList::EConsoleVariablesEditorListMode::Preset ?
						ChangedVariable->GetString() : "",
						true
					);

					bIsVariableCurrentlyTracked = true;
				}
			}
		}

		if (MainPanel.IsValid())
		{
			MainPanel->RefreshList();
		}

		// If the variable is already tracked or was just added to tracking, run the following code
		if (bIsVariableCurrentlyTracked)
		{
			/**
			 * Here we check the map of recently received variables from other nodes
			 * If the command is in the map and the value is similar, we won't send the value to other nodes
			 * because we can assume that this value came from another node.
			 * This prevents a feedback loop.
			 */
			if (!CommandsReceivedFromMultiUser.Find(Key))
			{
				SendMultiUserConsoleVariableChange(ERemoteCVarChangeType::Update, Key, ChangedVariable->GetString());
			}
		}
	}
}

void FConsoleVariablesEditorModule::OnDetectConsoleObjectUnregistered(FString CommandName)
{
	check(EditingPresetAsset);

	EditingPresetAsset->RemoveConsoleVariable(CommandName);

	if (MainPanel.IsValid())
	{
		MainPanel->RefreshList();
	}

	if (const TWeakPtr<FConsoleVariablesEditorCommandInfo> CommandInfo =
		FindCommandInfoByName(CommandName); CommandInfo.IsValid())
	{
		ConsoleObjectsMainReference.Remove(CommandInfo.Pin());
	}
}

TObjectPtr<UConsoleVariablesAsset> FConsoleVariablesEditorModule::AllocateTransientPreset(const FName DesiredName) const
{
	const FString PackageName = "/Temp/ConsoleVariablesEditor/PendingConsoleVariablesPresets";

	UPackage* NewPackage = CreatePackage(*PackageName);
	NewPackage->SetFlags(RF_Transient);
	NewPackage->AddToRoot();

	return NewObject<UConsoleVariablesAsset>(
		NewPackage, DesiredName, RF_Transient | RF_Transactional | RF_Standalone);
}

void FConsoleVariablesEditorModule::CreateEditingPresets()
{
	EditingPresetAsset = AllocateTransientPreset("ConsoleVariablesPreset_PendingPreset");
	
	EditingGlobalSearchAsset = AllocateTransientPreset("ConsoleVariablesPreset_GlobalSearch");
}

TSharedRef<SDockTab> FConsoleVariablesEditorModule::SpawnMainPanelTab(const FSpawnTabArgs& Args)
{
	const TSharedRef<SDockTab> DockTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	DockTab->SetContent(MainPanel->GetOrCreateWidget());
	MainPanel->RebuildList();
			
	return DockTab;
}

void FConsoleVariablesEditorModule::OpenConsoleVariablesEditor()
{
	FGlobalTabmanager::Get()->TryInvokeTab(ConsoleVariablesToolkitPanelTabId);
}

void FConsoleVariablesEditorModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditingPresetAsset);
	Collector.AddReferencedObject(EditingGlobalSearchAsset);
}

FString FConsoleVariablesEditorModule::GetReferencerName() const
{
	return TEXT("FConsoleVariablesEditorModule");
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FConsoleVariablesEditorModule, ConsoleVariablesEditor)
