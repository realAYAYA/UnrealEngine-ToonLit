// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintModes/WidgetDebugApplicationMode.h"

#include "BlueprintModes/WidgetBlueprintApplicationModes.h"
#include "SBlueprintEditorToolbar.h"
#include "TabFactory/DebugLogTabSummoner.h"
#include "TabFactory/DetailsTabSummoner.h"
#include "TabFactory/PreviewTabSummoner.h"
#include "ToolMenus.h"
#include "UMGEditorModule.h"
#include "WidgetBlueprintEditorToolbar.h"

FWidgetDebugApplicationMode::FWidgetDebugApplicationMode(TSharedPtr<FWidgetBlueprintEditor> InWidgetEditor)
	: FWidgetBlueprintApplicationMode(InWidgetEditor, FWidgetBlueprintApplicationModes::DebugMode)
{
	TabLayout = FTabManager::NewLayout( "WidgetBlueprintEditor_Debug_Layout_v2" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.7f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8f)
					->AddTab(FPreviewTabSummoner::TabID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(FDebugLogTabSummoner::TabID, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.15f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(FDetailsTabSummoner::TabID, ETabState::OpenedTab)
				)
			)
		);
	
	TabFactories.RegisterFactory(MakeShareable(new FPreviewTabSummoner(InWidgetEditor)));
	TabFactories.RegisterFactory(MakeShareable(new FDebugLogTabSummoner(InWidgetEditor, "Model View Viewmodel")));
	TabFactories.RegisterFactory(MakeShareable(new FDetailsTabSummoner(InWidgetEditor)));

	IUMGEditorModule& UMGEditorModule = FModuleManager::LoadModuleChecked<IUMGEditorModule>("UMGEditor");
	UMGEditorModule.OnRegisterTabsForEditor().Broadcast(*this, TabFactories);

	ToolbarExtender = UMGEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders();
	InWidgetEditor->GetWidgetToolbarBuilder()->AddWidgetBlueprintEditorModesToolbar(ToolbarExtender);

	if (UToolMenu* Toolbar = InWidgetEditor->RegisterModeToolbarIfUnregistered(GetModeName()))
	{
		InWidgetEditor->GetWidgetToolbarBuilder()->AddWidgetReflector(Toolbar);
		InWidgetEditor->GetToolbarBuilder()->AddCompileToolbar(Toolbar);
	}
}

void FWidgetDebugApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FBlueprintEditor> BP = GetBlueprintEditor();
	
	BP->RegisterToolbarTab(InTabManager.ToSharedRef());
	BP->PushTabFactories(TabFactories);
}

void FWidgetDebugApplicationMode::PostActivateMode()
{

}
