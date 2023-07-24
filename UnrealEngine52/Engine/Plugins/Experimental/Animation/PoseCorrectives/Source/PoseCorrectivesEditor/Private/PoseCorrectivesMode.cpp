// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesMode.h"

#include "PoseCorrectivesEditorToolkit.h"
#include "CorrectivesEditorTabSummoner.h"
#include "PoseCorrectivesGroupTabSummoner.h"

#include "Modules/ModuleManager.h"
#include "PersonaTabs.h"
#include "PersonaModule.h"


#define LOCTEXT_NAMESPACE "PoseCorrectivesMode"


FPoseCorrectivesMode::FPoseCorrectivesMode(
	TSharedRef<FWorkflowCentricApplication> InHostingApp,  
	TSharedRef<IPersonaPreviewScene> InPreviewScene)
	: FApplicationMode(PoseCorrectivesEditorModes::PoseCorrectivesEditorMode)
{
	PoseCorrectivesEditorPtr = StaticCastSharedRef<FPoseCorrectivesEditorToolkit>(InHostingApp);
	TSharedRef<FPoseCorrectivesEditorToolkit> PoseCorrectivesEditor = StaticCastSharedRef<FPoseCorrectivesEditorToolkit>(InHostingApp);

	FPersonaViewportArgs ViewportArgs(InPreviewScene);
	ViewportArgs.bAlwaysShowTransformToolbar = true;
	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowTurnTable = false;
	ViewportArgs.ContextName = TEXT("PoseCorrectivesEditorToolkit.Viewport");

	// register Persona tabs
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InHostingApp, ViewportArgs));
	TabFactories.RegisterFactory(PersonaModule.CreateDetailsTabFactory(InHostingApp, FOnDetailsCreated::CreateSP(&PoseCorrectivesEditor.Get(), &FPoseCorrectivesEditorToolkit::HandleDetailsCreated)));

	TabFactories.RegisterFactory(PersonaModule.CreateAnimationAssetBrowserTabFactory(InHostingApp, PoseCorrectivesEditor->GetPersonaToolkit(), FOnOpenNewAsset::CreateSP(PoseCorrectivesEditor, &FPoseCorrectivesEditorToolkit::HandleAssetDoubleClicked), FOnAnimationSequenceBrowserCreated::CreateSP(PoseCorrectivesEditor, &FPoseCorrectivesEditorToolkit::HandleAnimationSequenceBrowserCreated), true));

	// register custom tabs
	TabFactories.RegisterFactory(MakeShared<FCorrectivesEditorTabSummoner>(PoseCorrectivesEditor));
	TabFactories.RegisterFactory(MakeShared<FPoseCorrectivesGroupTabSummoner>(PoseCorrectivesEditor));


	// create tab layout
	TabLayout = FTabManager::NewLayout("Standalone_PoseCorrectivesEditor_Layout_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.5f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(FCorrectivesEditorTabSummoner::TabID, ETabState::OpenedTab)->SetForegroundTab(FCorrectivesEditorTabSummoner::TabID)
					->AddTab(FPoseCorrectivesGroupTabSummoner::TabID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(1.0f)
					->SetHideTabWell(true)
					->AddTab(FPersonaTabs::PreviewViewportID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(0.3f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5f)
						->AddTab(FPersonaTabs::DetailsID, ETabState::OpenedTab)
						->SetForegroundTab(FPersonaTabs::DetailsID)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.5)
						->AddTab(FPersonaTabs::AssetBrowserID, ETabState::OpenedTab)
					)
				)
			)
		);

	PersonaModule.OnRegisterTabs().Broadcast(TabFactories, InHostingApp);
	LayoutExtender = MakeShared<FLayoutExtender>();
	PersonaModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender.Get());
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

void FPoseCorrectivesMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FPoseCorrectivesEditorToolkit> PoseCorrectivesEditor = PoseCorrectivesEditorPtr.Pin();
	PoseCorrectivesEditor->RegisterTabSpawners(InTabManager.ToSharedRef());
	PoseCorrectivesEditor->PushTabFactories(TabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

#undef LOCTEXT_NAMESPACE
