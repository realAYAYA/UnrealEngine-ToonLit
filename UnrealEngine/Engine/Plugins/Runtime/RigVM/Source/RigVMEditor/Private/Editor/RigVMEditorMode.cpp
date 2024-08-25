// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMEditorMode.h" 
#include "BlueprintEditorTabs.h"
#include "SBlueprintEditorToolbar.h"
#include "Editor/RigVMExecutionStackTabSummoner.h"
#include "ToolMenus.h"

FRigVMEditorMode::FRigVMEditorMode(const TSharedRef<FRigVMEditor>& InRigVMEditor)
	: FBlueprintEditorApplicationMode(InRigVMEditor, FRigVMEditorModes::RigVMEditorMode, FRigVMEditorModes::GetLocalizedMode, false, false)
{
	RigVMBlueprintPtr = CastChecked<URigVMBlueprint>(InRigVMEditor->GetBlueprintObj());

	TabFactories.RegisterFactory(MakeShared<FRigVMExecutionStackTabSummoner>(InRigVMEditor));

	TabLayout = FTabManager::NewLayout("Standalone_RigVMEditMode_Layout_v1.5")
		->AddArea
		(
			// Main application area
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					//	Left bottom - rig/hierarchy
					FTabManager::NewStack()
					->SetSizeCoefficient(0.5f)
					->AddTab(FRigVMExecutionStackTabSummoner::TabID, ETabState::OpenedTab)
					->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
				)
				->Split
				(
					// Middle 
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f)
					->Split
					(
						// Middle top - document edit area
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->AddTab("Document", ETabState::ClosedTab)
					)
					->Split
					(
						// Middle bottom - compiler results & find
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(FBlueprintEditorTabs::CompilerResultsID, ETabState::ClosedTab)
						->AddTab(FBlueprintEditorTabs::FindResultsID, ETabState::ClosedTab)
					)
				)
				->Split
				(
					// Right side
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						// Right top
						FTabManager::NewStack()
						->SetHideTabWell(false)
						->SetSizeCoefficient(1.f)
						->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
						->SetForegroundTab(FBlueprintEditorTabs::DetailsID)
					)
				)
			)
		);

	if (UToolMenu* Toolbar = InRigVMEditor->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		InRigVMEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		InRigVMEditor->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		InRigVMEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
	}
}

void FRigVMEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);
	BP->PushTabFactories(TabFactories);
}
