// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "ObjectFilter/ObjectMixerEditorObjectFilter.h"
#include "Widgets/Docking/SDockTab.h"

class FObjectMixerEditorMainPanel;

class OBJECTMIXEREDITOR_API FObjectMixerEditorModule : public IModuleInterface
{
public:

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	virtual void Initialize();
	virtual void Teardown();

	static FObjectMixerEditorModule& Get();

	static void OpenProjectSettings();

	virtual FName GetModuleName();
	
	virtual TSharedPtr<SWidget> MakeObjectMixerDialog() const;

	/**
	 * Regenerate the list items and refresh the list. Call when adding or removing variables.
	 */
	virtual void RequestRebuildList() const;
	
	/**
	 * Refresh filters and sorting.
	 * Useful for when the list state has gone stale but the variable count has not changed.
	 */
	virtual void RefreshList() const;
	
	void RequestSyncEditorSelectionToListSelection();

	void RegisterMenuGroup();
	void UnregisterMenuGroup();
	virtual void SetupMenuItemVariables();
	virtual void RegisterTabSpawner();
	virtual FName GetTabSpawnerId();
	
	/**
	 * Add a tab spawner to the Object Mixer menu group.
	 * @return If adding the item to the menu was successful
	 */
	bool RegisterItemInMenuGroup(FWorkspaceItem& InItem);
	
	virtual void UnregisterTabSpawner();
	virtual void RegisterSettings() const;
	virtual void UnregisterSettings() const;

	virtual TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	TSharedPtr<FWorkspaceItem> GetWorkspaceGroup();

	const static FName BaseObjectMixerModuleName;

protected:

	virtual TSharedRef<SDockTab> SpawnMainPanelTab();

	virtual void BindDelegates();
	
	/** Lives for as long as the module is loaded. */
	TSharedPtr<FObjectMixerEditorMainPanel> MainPanel;

	/** The text that appears on the spawned nomad tab */
	FText TabLabel;

	/** Menu Item variables */
	FText MenuItemName;
	FSlateIcon MenuItemIcon;
	FText MenuItemTooltip;
	ETabSpawnerMenuType::Type TabSpawnerType = ETabSpawnerMenuType::Enabled;

	// If set, this is the filter class used to initialize the MainPanel
	TSubclassOf<UObjectMixerObjectFilter> DefaultFilterClass;
	
	TSet<FDelegateHandle> DelegateHandles;

private:

	TSharedPtr<FWorkspaceItem> WorkspaceGroup;
};
