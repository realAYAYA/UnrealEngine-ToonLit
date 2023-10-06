// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileApplicationMode.h"
#include "PhysicsControlProfileEditorToolkit.h"
#include "PersonaModule.h"
#include "Modules/ModuleManager.h"
#include "PersonaTabs.h"

#define LOCTEXT_NAMESPACE "PhysicsControlProfileApplicationMode"

FName FPhysicsControlProfileApplicationMode::ModeName("PhysicsControlProfileAssetEditMode");

//======================================================================================================================
FPhysicsControlProfileApplicationMode::FPhysicsControlProfileApplicationMode(
	TSharedRef<FWorkflowCentricApplication> InHostingApp,
	TSharedRef<IPersonaPreviewScene>        InPreviewScene)
	: 
	FApplicationMode(PhysicsControlProfileEditorModes::Editor)
{
	EditorToolkit = StaticCastSharedRef<FPhysicsControlProfileEditorToolkit>(InHostingApp);
	TSharedRef<FPhysicsControlProfileEditorToolkit> PhysicsControlProfileEditor = 
		StaticCastSharedRef<FPhysicsControlProfileEditorToolkit>(InHostingApp);

	FPersonaViewportArgs ViewportArgs(InPreviewScene);
	ViewportArgs.bAlwaysShowTransformToolbar = true;
	ViewportArgs.bShowStats = false;
	ViewportArgs.bShowTimeline = false;
	ViewportArgs.bShowLODMenu = true;
	ViewportArgs.bShowPlaySpeedMenu = false;
	ViewportArgs.bShowPhysicsMenu = false;
	ViewportArgs.ContextName = TEXT("PhysicsControlProfileEditor.Viewport");
	ViewportArgs.OnViewportCreated = FOnViewportCreated::CreateSP(
		PhysicsControlProfileEditor, &FPhysicsControlProfileEditorToolkit::HandleViewportCreated);

	// Register Persona tabs.
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TabFactories.RegisterFactory(PersonaModule.CreatePersonaViewportTabFactory(InHostingApp, ViewportArgs));
	TabFactories.RegisterFactory(PersonaModule.CreateDetailsTabFactory(InHostingApp, FOnDetailsCreated::CreateSP(&PhysicsControlProfileEditor.Get(), &FPhysicsControlProfileEditorToolkit::HandleDetailsCreated)));

	// Create tab layout.
	TabLayout = FTabManager::NewLayout("Standalone_PhysicsControlProfileEditor_Layout_v0.001")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.32f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
					->SetSizeCoefficient(1.0f)
					->SetOrientation(Orient_Vertical)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.7f)
						->SetHideTabWell(true)
						->AddTab(FPersonaTabs::PreviewViewportID, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(FPersonaTabs::DetailsID, ETabState::OpenedTab)
					->SetForegroundTab(FPersonaTabs::DetailsID)
				)
			)
		);

	PersonaModule.OnRegisterTabs().Broadcast(TabFactories, InHostingApp);
	LayoutExtender = MakeShared<FLayoutExtender>();
	PersonaModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender.Get());
	TabLayout->ProcessExtensions(*LayoutExtender.Get());
}

//======================================================================================================================
void FPhysicsControlProfileApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FPhysicsControlProfileEditorToolkit> Editor = EditorToolkit.Pin();
	Editor->RegisterTabSpawners(InTabManager.ToSharedRef());
	Editor->PushTabFactories(TabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

#undef LOCTEXT_NAMESPACE
