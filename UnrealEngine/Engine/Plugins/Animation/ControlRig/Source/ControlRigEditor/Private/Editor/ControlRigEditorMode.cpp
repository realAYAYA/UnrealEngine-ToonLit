// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ControlRigEditorMode.h" 
#include "BlueprintEditorTabs.h"
#include "SBlueprintEditorToolbar.h"
#include "PersonaModule.h"
#include "IPersonaToolkit.h"
#include "PersonaTabs.h"
#include "Editor/RigHierarchyTabSummoner.h"
#include "Editor/ModularRigModelTabSummoner.h"
#include "Editor/RigModuleAssetBrowserTabSummoner.h"
#include "Editor/RigVMExecutionStackTabSummoner.h"
#include "Editor/RigCurveContainerTabSummoner.h"
#include "Editor/RigValidationTabSummoner.h"
#include "Editor/RigAnimAttributeTabSummoner.h"
#include "ToolMenus.h"

FControlRigEditorMode::FControlRigEditorMode(const TSharedRef<FControlRigEditor>& InControlRigEditor, bool bCreateDefaultLayout)
	: FBlueprintEditorApplicationMode(InControlRigEditor, FControlRigEditorModes::ControlRigEditorMode, FControlRigEditorModes::GetLocalizedMode, false, false)
{
	ControlRigBlueprintPtr = CastChecked<UControlRigBlueprint>(InControlRigEditor->GetBlueprintObj());

	TabFactories.RegisterFactory(MakeShared<FRigHierarchyTabSummoner>(InControlRigEditor));
	TabFactories.RegisterFactory(MakeShared<FRigVMExecutionStackTabSummoner>(InControlRigEditor));
	TabFactories.RegisterFactory(MakeShared<FRigCurveContainerTabSummoner>(InControlRigEditor));
	TabFactories.RegisterFactory(MakeShared<FRigValidationTabSummoner>(InControlRigEditor));
	TabFactories.RegisterFactory(MakeShared<FRigAnimAttributeTabSummoner>(InControlRigEditor));

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");

 	FPersonaViewportArgs ViewportArgs(InControlRigEditor->GetPersonaToolkit()->GetPreviewScene());
 	ViewportArgs.BlueprintEditor = InControlRigEditor;
 	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowPlaySpeedMenu = false;
	ViewportArgs.bShowTimeline = true;
	ViewportArgs.bShowTurnTable = false;
	ViewportArgs.bAlwaysShowTransformToolbar = true;
	ViewportArgs.OnViewportCreated = FOnViewportCreated::CreateSP(InControlRigEditor, &FControlRigEditor::HandleViewportCreated);
 
 	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InControlRigEditor, ViewportArgs));
	TabFactories.RegisterFactory(PersonaModule.CreateAdvancedPreviewSceneTabFactory(InControlRigEditor, InControlRigEditor->GetPersonaToolkit()->GetPreviewScene()));

	if(bCreateDefaultLayout)
	{
		TabLayout = FTabManager::NewLayout("Standalone_ControlRigEditMode_Layout_v1.8")
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
						FTabManager::NewSplitter()
						->SetOrientation(Orient_Vertical)
						->SetSizeCoefficient(0.2f)
						->Split
						(
							//	Left top - viewport
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->SetHideTabWell(true)
							->AddTab(FPersonaTabs::PreviewViewportID, ETabState::OpenedTab)
						
						)
						->Split
						(
							//	Left bottom - rig/hierarchy
							FTabManager::NewStack()
							->SetSizeCoefficient(0.5f)
							->AddTab(FRigHierarchyTabSummoner::TabID, ETabState::OpenedTab)
							->AddTab(FRigVMExecutionStackTabSummoner::TabID, ETabState::OpenedTab)
							->AddTab(FRigCurveContainerTabSummoner::TabID, ETabState::OpenedTab)
							->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
						)
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
							->AddTab(FPersonaTabs::AdvancedPreviewSceneSettingsID, ETabState::OpenedTab)
							->AddTab(FRigAnimAttributeTabSummoner::TabID, ETabState::OpenedTab)
							->SetForegroundTab(FBlueprintEditorTabs::DetailsID)
						)
					)
				)
			);
	}
	
	if (UToolMenu* Toolbar = InControlRigEditor->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		InControlRigEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
		InControlRigEditor->GetToolbarBuilder()->AddScriptingToolbar(Toolbar);
		InControlRigEditor->GetToolbarBuilder()->AddBlueprintGlobalOptionsToolbar(Toolbar);
	}
}

void FControlRigEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = MyBlueprintEditor.Pin();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());

	// Mode-specific setup
	BP->PushTabFactories(CoreTabFactories);
	BP->PushTabFactories(BlueprintEditorTabFactories);
	BP->PushTabFactories(TabFactories);
}

FModularRigEditorMode::FModularRigEditorMode(const TSharedRef<FControlRigEditor>& InControlRigEditor)
	: FControlRigEditorMode(InControlRigEditor, false)
{

	TabFactories.RegisterFactory(MakeShared<FModularRigModelTabSummoner>(InControlRigEditor));
	TabFactories.RegisterFactory(MakeShared<FRigModuleAssetBrowserTabSummoner>(InControlRigEditor));
	
	TabLayout = FTabManager::NewLayout("Standalone_ModularRigEditMode_Layout_v1.3")
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
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.2f)
					->Split
					(
						// Left top - Module Library
						FTabManager::NewStack()
						->SetHideTabWell(false)
						->SetSizeCoefficient(0.5f)
						->AddTab(FRigModuleAssetBrowserTabSummoner::TabID, ETabState::OpenedTab)
						->SetForegroundTab(FBlueprintEditorTabs::DetailsID)
					)
					->Split
					(
						//	Left bottom - rig/hierarchy/modules
						FTabManager::NewStack()
						->SetHideTabWell(false)
						->SetSizeCoefficient(0.5f)
						->SetForegroundTab(FModularRigModelTabSummoner::TabID)
						->AddTab(FModularRigModelTabSummoner::TabID, ETabState::OpenedTab)
						->AddTab(FBlueprintEditorTabs::MyBlueprintID, ETabState::OpenedTab)
					)
				)
				->Split
				(
					//	Center - viewport
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
					->AddTab(FPersonaTabs::PreviewViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					// Right side
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->SetHideTabWell(false)
					->SetForegroundTab(FBlueprintEditorTabs::DetailsID)
					->AddTab(FBlueprintEditorTabs::DetailsID, ETabState::OpenedTab)
					->AddTab(FPersonaTabs::AdvancedPreviewSceneSettingsID, ETabState::OpenedTab)
					->AddTab(FRigHierarchyTabSummoner::TabID, ETabState::OpenedTab)
				)
			)
		);
}