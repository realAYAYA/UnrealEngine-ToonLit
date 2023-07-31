// Copyright Epic Games, Inc. All Rights Reserved.

#include "ObjectMixerEditorModule.h"

#include "ObjectMixerEditorLog.h"
#include "ObjectMixerEditorSettings.h"
#include "ObjectMixerEditorStyle.h"
#include "Views/MainPanel/ObjectMixerEditorMainPanel.h"

#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "ISettingsModule.h"
#include "LevelEditor.h"
#include "ObjectMixerEditorSerializedData.h"
#include "Selection.h"
#include "Misc/TransactionObjectEvent.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

const FName FObjectMixerEditorModule::BaseObjectMixerModuleName("ObjectMixerEditor");

#define LOCTEXT_NAMESPACE "FObjectMixerEditorModule"

IMPLEMENT_MODULE(FObjectMixerEditorModule, ObjectMixerEditor)

void FObjectMixerEditorModule::StartupModule()
{
	FObjectMixerEditorStyle::Initialize();

	// In the future, Object Mixer and Light Mixer tabs may go into an Object Mixer group
	//RegisterMenuGroup();
	
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FObjectMixerEditorModule::Initialize);
}

void FObjectMixerEditorModule::ShutdownModule()
{
	FObjectMixerEditorStyle::Shutdown();

	UnregisterMenuGroup();
	
	Teardown();
}

void FObjectMixerEditorModule::Initialize()
{
	BindDelegates();
	
	SetupMenuItemVariables();
	
	RegisterTabSpawner();
	RegisterSettings();
}

void FObjectMixerEditorModule::Teardown()
{
	// Unbind Delegates
	if (GEngine)
	{
		GEngine->OnLevelActorFolderChanged().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
		GEngine->OnActorFolderAdded().RemoveAll(this);
		GEngine->OnLevelActorDetached().RemoveAll(this);
		GEngine->OnLevelActorAttached().RemoveAll(this);
		GEngine->OnActorFolderAdded().RemoveAll(this);
		GEngine->OnActorFoldersUpdatedEvent().RemoveAll(this);
	}

	if (GEditor)
	{
		GEditor->OnBlueprintCompiled().RemoveAll(this);
		GEditor->OnLevelActorAdded().RemoveAll(this);
		GEditor->OnLevelActorDeleted().RemoveAll(this);
		GEditor->GetSelectedActors()->SelectionChangedEvent.RemoveAll(this);
	}
	
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::PostUndoRedo.RemoveAll(this);
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectTransacted.RemoveAll(this);

	if (FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
	{
		LevelEditorModule->OnComponentsEdited().RemoveAll(this);
	}

	for (FDelegateHandle& Delegate : DelegateHandles)
	{
		Delegate.Reset();
	}
	DelegateHandles.Empty();
	
	MainPanel.Reset();

	UToolMenus::UnregisterOwner(this);
	
	UnregisterTabSpawner();
	UnregisterSettings();
}

FObjectMixerEditorModule& FObjectMixerEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked< FObjectMixerEditorModule >("ObjectMixerEditor");
}

void FObjectMixerEditorModule::OpenProjectSettings()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings")
		.ShowViewer("Editor", "Plugins", "Object Mixer");
}

FName FObjectMixerEditorModule::GetModuleName()
{
	return "ObjectMixerEditor";
}

TSharedPtr<SWidget> FObjectMixerEditorModule::MakeObjectMixerDialog() const
{
	if (MainPanel.IsValid())
	{
		return MainPanel->GetOrCreateWidget();
	}

	return nullptr;
}

void FObjectMixerEditorModule::RequestRebuildList() const
{
	if (MainPanel.IsValid())
	{
		MainPanel->RequestRebuildList();
	}
}

void FObjectMixerEditorModule::RefreshList() const
{
	if (MainPanel.IsValid())
	{
		MainPanel->RefreshList();
	}
}

void FObjectMixerEditorModule::RequestSyncEditorSelectionToListSelection()
{
	if (MainPanel.IsValid())
	{
		MainPanel->RequestSyncEditorSelectionToListSelection();
	}
}

void FObjectMixerEditorModule::RegisterMenuGroup()
{
	WorkspaceGroup = WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory()->AddGroup(
		LOCTEXT("ObjectMixerMenuGroupItemName", "Object Mixer"), 
		FSlateIcon(FObjectMixerEditorStyle::Get().GetStyleSetName(),
			"ObjectMixer.ToolbarButton", "ObjectMixer.ToolbarButton.Small"));
}

void FObjectMixerEditorModule::UnregisterMenuGroup()
{
	if (WorkspaceGroup)
	{
		for (const TSharedRef<FWorkspaceItem>& ChildItem : WorkspaceGroup->GetChildItems())
		{
			WorkspaceGroup->RemoveItem(ChildItem);
		}
		
		WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory()->RemoveItem(WorkspaceGroup.ToSharedRef());
		WorkspaceGroup.Reset();
	}
}

void FObjectMixerEditorModule::SetupMenuItemVariables()
{
	TabLabel = LOCTEXT("ObjectMixerTabLabel", "Object Mixer");

	MenuItemName = LOCTEXT("ObjectMixerEditorMenuItem", "Object Mixer");
	MenuItemIcon =
		FSlateIcon(FObjectMixerEditorStyle::Get().GetStyleSetName(), "ObjectMixer.ToolbarButton", "ObjectMixer.ToolbarButton.Small");
	MenuItemTooltip = LOCTEXT("ObjectMixerEditorMenuItemTooltip", "Open an Object Mixer instance.");

	// Should be hidden for now since it's not ready yet for public release
	TabSpawnerType = ETabSpawnerMenuType::Hidden;
}

void FObjectMixerEditorModule::RegisterTabSpawner()
{
	FTabSpawnerEntry& BrowserSpawnerEntry =
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			GetTabSpawnerId(), 
			FOnSpawnTab::CreateRaw(this, &FObjectMixerEditorModule::SpawnTab)
		)
		.SetIcon(MenuItemIcon)
		.SetDisplayName(MenuItemName)
		.SetTooltipText(MenuItemTooltip)
		.SetMenuType(TabSpawnerType);

	// Always use the base ObjectMixer function call or WorkspaceGroup may be null 
	if (!FObjectMixerEditorModule::Get().RegisterItemInMenuGroup(BrowserSpawnerEntry))
	{
		BrowserSpawnerEntry.SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory());
	}
}

FName FObjectMixerEditorModule::GetTabSpawnerId()
{
	return "ObjectMixerToolkit";
}

bool FObjectMixerEditorModule::RegisterItemInMenuGroup(FWorkspaceItem& InItem)
{
	if (WorkspaceGroup)
	{
		WorkspaceGroup->AddItem(MakeShareable(&InItem));
		
		return true;
	}

	return false;
}

void FObjectMixerEditorModule::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GetTabSpawnerId());
}

void FObjectMixerEditorModule::RegisterSettings() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		// User Project Settings
		const TSharedPtr<ISettingsSection> ProjectSettingsSectionPtr = SettingsModule->RegisterSettings(
			"Editor", "Plugins", "Object Mixer",
			LOCTEXT("ObjectMixerSettingsDisplayName", "Object Mixer"),
			LOCTEXT("ObjectMixerSettingsDescription", "Configure Object Mixer user settings"),
			GetMutableDefault<UObjectMixerEditorSettings>());	
	}
}

void FObjectMixerEditorModule::UnregisterSettings() const
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Editor", "Plugins", "Object Mixer");
	}
}

TSharedRef<SDockTab> FObjectMixerEditorModule::SpawnTab(const FSpawnTabArgs& Args)
{
	 return SpawnMainPanelTab();
}

TSharedPtr<FWorkspaceItem> FObjectMixerEditorModule::GetWorkspaceGroup()
{
	return WorkspaceGroup;
}

TSharedRef<SDockTab> FObjectMixerEditorModule::SpawnMainPanelTab()
{
	MainPanel = MakeShared<FObjectMixerEditorMainPanel>(GetModuleName());
	MainPanel->Init();
	
	const TSharedRef<SDockTab> DockTab =
		SNew(SDockTab)
		.Label(TabLabel)
		.TabRole(ETabRole::NomadTab)
	;
	const TSharedPtr<SWidget> ObjectMixerDialog = MakeObjectMixerDialog();
	DockTab->SetContent(ObjectMixerDialog ? ObjectMixerDialog.ToSharedRef() : SNullWidget::NullWidget);

	if (DefaultFilterClass)
	{
		MainPanel->OnClassSelectionChanged(DefaultFilterClass);
	}
			
	return DockTab;
}

void FObjectMixerEditorModule::BindDelegates()
{
	check(GEngine && GEditor);
	DelegateHandles.Add(GEngine->OnLevelActorListChanged().AddLambda([this] ()
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEngine->OnActorFolderAdded().AddLambda([this] (UActorFolder*)
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEngine->OnActorFoldersUpdatedEvent().AddLambda([this] (ULevel*)
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEngine->OnLevelActorFolderChanged().AddLambda([this] (const AActor*, FName)
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEngine->OnLevelActorAttached().AddLambda([this] (AActor*, const AActor*)
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEngine->OnLevelActorDetached().AddLambda([this] (AActor*, const AActor*)
	{
		RequestRebuildList();
	}));

	DelegateHandles.Add(GEditor->OnBlueprintCompiled().AddLambda([this] ()
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEditor->OnLevelActorAdded().AddLambda([this] (AActor*)
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEditor->OnLevelActorDeleted().AddLambda([this] (AActor*)
	{
		RequestRebuildList();
	}));
	DelegateHandles.Add(GEditor->GetSelectedActors()->SelectionChangedEvent.AddLambda([this] (UObject*)
	{
		RequestSyncEditorSelectionToListSelection();
	}));

	DelegateHandles.Add(FEditorDelegates::MapChange.AddLambda([this](uint32)
	{
		RequestRebuildList();
	}));

	DelegateHandles.Add(FEditorDelegates::PostUndoRedo.AddLambda([this]()
	{
		RequestRebuildList();
		
		// Because we can undo/redo collection adds and removes, we need to save the data after each
		GetMutableDefault<UObjectMixerEditorSerializedData>()->SaveConfig();
	}));

	DelegateHandles.Add(FCoreUObjectDelegates::OnObjectTransacted.AddLambda([this](UObject*, const FTransactionObjectEvent& Event)
	{
		if (Event.GetEventType() == ETransactionObjectEventType::Finalized && Event.HasPropertyChanges())
		{
			RequestRebuildList();
		}
	}));

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.OnComponentsEdited().AddLambda([this]()
	{
		RequestRebuildList();
	});
}

#undef LOCTEXT_NAMESPACE
