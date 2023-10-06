// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetEditorModeUILayer.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "UMGEditorModule.h"
#include "Toolkits/IToolkit.h"
#include "TabFactory/DesignerTabSummoner.h"

void UWidgetEditorModeUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	IUMGEditorModule& UMGEditorModule = FModuleManager::GetModuleChecked<IUMGEditorModule>("UMGEditor");
	UMGEditorModule.OnRegisterLayoutExtensions().AddUObject(this, &UWidgetEditorModeUISubsystem::RegisterLayoutExtensions);
}

void UWidgetEditorModeUISubsystem::Deinitialize()
{
	IUMGEditorModule& UMGEditorModule = FModuleManager::GetModuleChecked<IUMGEditorModule>("UMGEditor");
	UMGEditorModule.OnRegisterLayoutExtensions().RemoveAll(this);
}

void UWidgetEditorModeUISubsystem::RegisterLayoutExtensions(FLayoutExtender& Extender)
{
	FTabManager::FTab NewTab(FTabId(UAssetEditorUISubsystem::TopLeftTabID, ETabIdFlags::SaveLayout), ETabState::ClosedTab);
	Extender.ExtendLayout(FDesignerTabSummoner::ToolPaletteTabID, ELayoutExtensionPosition::After, NewTab);
}

FWidgetEditorModeUILayer::FWidgetEditorModeUILayer(const IToolkitHost* InToolkitHost) :
	FAssetEditorModeUILayer(InToolkitHost)
{}

void FWidgetEditorModeUILayer::OnToolkitHostingStarted(const TSharedRef<IToolkit>& Toolkit)
{
	if (!Toolkit->IsAssetEditor())
	{
		FAssetEditorModeUILayer::OnToolkitHostingStarted(Toolkit);
		HostedToolkit = Toolkit;
		Toolkit->SetModeUILayer(SharedThis(this));
		Toolkit->RegisterTabSpawners(ToolkitHost->GetTabManager().ToSharedRef());
		RegisterModeTabSpawners();
		OnToolkitHostReadyForUI.ExecuteIfBound();
	}
}

void FWidgetEditorModeUILayer::OnToolkitHostingFinished(const TSharedRef<IToolkit>& Toolkit)
{
	if (HostedToolkit.IsValid() && HostedToolkit.Pin() == Toolkit)
	{
		FAssetEditorModeUILayer::OnToolkitHostingFinished(Toolkit);
	}
}

TSharedPtr<FWorkspaceItem> FWidgetEditorModeUILayer::GetModeMenuCategory() const
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	return MenuStructure.GetLevelEditorModesCategory();
}

