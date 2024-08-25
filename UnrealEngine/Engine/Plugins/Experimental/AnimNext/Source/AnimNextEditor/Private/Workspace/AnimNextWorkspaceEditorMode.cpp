// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextWorkspaceEditorMode.h"

#include "AnimNextWorkspaceEditor.h"
#include "DetailsTabSummoner.h"
#include "WorkspaceTabSummoner.h"
#include "Widgets/Docking/SDockTab.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/Layout/SSpacer.h"
#include "Modules/ModuleManager.h"
#include "Graph/GraphEditorMode.h"

#define LOCTEXT_NAMESPACE "WorkspaceEditorMode"

namespace UE::AnimNext::Editor
{

FWorkspaceEditorMode::FWorkspaceEditorMode(TSharedRef<FWorkspaceEditor> InHostingApp)
	: FApplicationMode(WorkspaceModes::WorkspaceEditor)
	, HostingAppPtr(InHostingApp)
{
	TSharedRef<FWorkspaceEditor> WorkspaceEditor = StaticCastSharedRef<FWorkspaceEditor>(InHostingApp);
	
	TabFactories.RegisterFactory(MakeShared<FDetailsTabSummoner>(WorkspaceEditor, FOnDetailsViewCreated::CreateSP(&WorkspaceEditor.Get(), &FWorkspaceEditor::HandleDetailsViewCreated)));
	TabFactories.RegisterFactory(MakeShared<FWorkspaceTabSummoner>(WorkspaceEditor));

	TabLayout = FTabManager::NewLayout("Standalone_AnimNextWorkspaceEditor_Layout_v1.1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(1.0f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->SetHideTabWell(false)
					->AddTab(WorkspaceTabs::WorkspaceView, ETabState::OpenedTab)
					->AddTab(WorkspaceTabs::LeftAssetDocument, ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->SetHideTabWell(false)
					->AddTab(WorkspaceTabs::ParameterBlockGraphDocument, ETabState::ClosedTab)
					->AddTab(WorkspaceTabs::AnimNextGraphDocument, ETabState::ClosedTab)
					->AddTab(WorkspaceTabs::MiddleAssetDocument, ETabState::ClosedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->SetHideTabWell(false)
					->AddTab(WorkspaceTabs::Details, ETabState::OpenedTab)
				)
			)
		);

	WorkspaceEditor->RegisterModeToolbarIfUnregistered(GetModeName());
}

void FWorkspaceEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkspaceEditor> HostingApp = HostingAppPtr.Pin();
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}

void FWorkspaceEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FWorkspaceEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

void FWorkspaceEditorMode::PreDeactivateMode()
{
	FApplicationMode::PreDeactivateMode();

	TSharedPtr<FWorkspaceEditor> WorkspaceEditor = HostingAppPtr.Pin();

	WorkspaceEditor->SaveEditedObjectState();
}

void FWorkspaceEditorMode::PostActivateMode()
{
	// Reopen any documents that were open when the blueprint was last saved
	TSharedPtr<FWorkspaceEditor> WorkspaceEditor = HostingAppPtr.Pin();
	WorkspaceEditor->RestoreEditedObjectState();

	FApplicationMode::PostActivateMode();
}


}

#undef LOCTEXT_NAMESPACE