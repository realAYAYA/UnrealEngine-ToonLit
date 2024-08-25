// Copyright Epic Games, Inc. All Rights Reserved.

#include "OperatorEditor/OperatorEditor.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"

#include "OperatorEditor/SOperatorEditorTabWidget.h"
#include "OperatorEditor/OperatorEditor.h"

// UE_DISABLE_OPTIMIZATION;

static const FName PhysicsControlEditorModule_OperatorNamesTabWidget("PhysicsControlEditorModule_OperatorNamesTabWidget");

#define LOCTEXT_NAMESPACE "PhysicsControlEditor"

void FPhysicsControlOperatorEditor::Startup()
{
	// Editor Physics Operator Names Tool
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(PhysicsControlEditorModule_OperatorNamesTabWidget, FOnSpawnTab::CreateRaw(this, &FPhysicsControlOperatorEditor::OnCreateTab))
		.SetDisplayName(LOCTEXT("PhysicsAnimationEditor_OperatorNamesTabTitle", "Rigid Body With Control"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FPhysicsControlOperatorEditor::Shutdown()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PhysicsControlEditorModule_OperatorNamesTabWidget);
}

void FPhysicsControlOperatorEditor::OpenOperatorNamesTab()
{
	OperatorNamesTab = FGlobalTabmanager::Get()->FindExistingLiveTab(PhysicsControlEditorModule_OperatorNamesTabWidget);

	if (!OperatorNamesTab)
	{
		OperatorNamesTab = FGlobalTabmanager::Get()->TryInvokeTab(PhysicsControlEditorModule_OperatorNamesTabWidget);
	}
}

void FPhysicsControlOperatorEditor::CloseOperatorNamesTab()
{
	if (!OperatorNamesTab)
	{
		OperatorNamesTab = FGlobalTabmanager::Get()->FindExistingLiveTab(PhysicsControlEditorModule_OperatorNamesTabWidget);
	}

	OperatorNamesTab->RequestCloseTab();
	OperatorNamesTab.Reset();
}

void FPhysicsControlOperatorEditor::ToggleOperatorNamesTab()
{
	if (IsOperatorNamesTabOpen())
	{
		CloseOperatorNamesTab();
	}
	else
	{
		OpenOperatorNamesTab();
	}
}

bool FPhysicsControlOperatorEditor::IsOperatorNamesTabOpen()
{
	return OperatorNamesTab.IsValid();
}

void FPhysicsControlOperatorEditor::RequestRefresh()
{
	if (PersistantTabWidget)
	{
		PersistantTabWidget->RequestRefresh();
	}
}

TSharedRef<SDockTab> FPhysicsControlOperatorEditor::OnCreateTab(const FSpawnTabArgs& SpawnTabArgs)
{
	const int TabIndex = 1;

	PersistantTabWidget = SNew(SOperatorEditorTabWidget, TabIndex);

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.OnTabClosed_Lambda([this](TSharedRef<class SDockTab> InParentTab) { this->OnTabClosed(InParentTab); })
		[
			SNew(SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				PersistantTabWidget.ToSharedRef()
			]
		];
}

void FPhysicsControlOperatorEditor::OnTabClosed(TSharedRef<SDockTab> DockTab)
{
	OperatorNamesTab.Reset();
	PersistantTabWidget.Reset();
}

#undef LOCTEXT_NAMESPACE