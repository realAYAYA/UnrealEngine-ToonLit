// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshEditorMode.h"
#include "PersonaModule.h"
#include "SkeletalMeshEditor.h"
#include "ISkeletonTree.h"
#include "ISkeletonEditorModule.h"
#include "IPersonaToolkit.h"
#include "SControlRigMappingWindow.h"
#include "Styling/AppStyle.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "SkeletalMeshEditorMode"

FSkeletalMeshEditorMode::FSkeletalMeshEditorMode(TSharedRef<FWorkflowCentricApplication> InHostingApp, TSharedRef<ISkeletonTree> InSkeletonTree)
	: FApplicationMode(SkeletalMeshEditorModes::SkeletalMeshEditorMode)
{
	HostingAppPtr = InHostingApp;

	TSharedRef<FSkeletalMeshEditor> SkeletalMeshEditor = StaticCastSharedRef<FSkeletalMeshEditor>(InHostingApp);

	ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
	TabFactories.RegisterFactory(SkeletonEditorModule.CreateSkeletonTreeTabFactory(InHostingApp, InSkeletonTree));

	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TabFactories.RegisterFactory(PersonaModule.CreateDetailsTabFactory(InHostingApp, FOnDetailsCreated::CreateSP(&SkeletalMeshEditor.Get(), &FSkeletalMeshEditor::HandleDetailsCreated)));

	FPersonaViewportArgs ViewportArgs(SkeletalMeshEditor->GetPersonaToolkit()->GetPreviewScene());
	ViewportArgs.ContextName = TEXT("SkeletalMeshEditor.Viewport");
	ViewportArgs.OnViewportCreated = FOnViewportCreated::CreateSP(SkeletalMeshEditor, &FSkeletalMeshEditor::HandleViewportCreated);

	PersonaModule.RegisterPersonaViewportTabFactories(TabFactories, InHostingApp, ViewportArgs);

	TabFactories.RegisterFactory(PersonaModule.CreateAdvancedPreviewSceneTabFactory(InHostingApp, SkeletalMeshEditor->GetPersonaToolkit()->GetPreviewScene()));
	TabFactories.RegisterFactory(PersonaModule.CreateAssetDetailsTabFactory(InHostingApp, FOnGetAsset::CreateSP(&SkeletalMeshEditor.Get(), &FSkeletalMeshEditor::HandleGetAsset), FOnDetailsCreated::CreateSP(&SkeletalMeshEditor.Get(), &FSkeletalMeshEditor::HandleMeshDetailsCreated)));
	TabFactories.RegisterFactory(PersonaModule.CreateMorphTargetTabFactory(InHostingApp, SkeletalMeshEditor->GetPersonaToolkit()->GetPreviewScene(), SkeletalMeshEditor->OnPostUndo));

	TabFactories.RegisterFactory(CreateMeshControllerMappingTabFactory(InHostingApp, Cast<USkeletalMesh> (SkeletalMeshEditor->HandleGetAsset()), SkeletalMeshEditor->OnPostUndo));

	TabLayout = FTabManager::NewLayout("Standalone_SkeletalMeshEditor_Layout_v3.4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.9f)
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->SetHideTabWell(false)
					->AddTab(SkeletalMeshEditorTabs::AssetDetailsTab, ETabState::OpenedTab)
					->AddTab(SkeletalMeshEditorTabs::SkeletonTreeTab, ETabState::OpenedTab)
					->SetForegroundTab(SkeletalMeshEditorTabs::AssetDetailsTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->SetHideTabWell(true)
					->AddTab(SkeletalMeshEditorTabs::ViewportTab, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->SetHideTabWell(false)
					->AddTab(SkeletalMeshEditorTabs::MorphTargetsTab, ETabState::OpenedTab)
					->AddTab(SkeletalMeshEditorTabs::DetailsTab, ETabState::ClosedTab)
	                ->AddTab(SkeletalMeshEditorTabs::ToolboxDetailsTab, ETabState::ClosedTab)
					->AddTab(SkeletalMeshEditorTabs::AdvancedPreviewTab, ETabState::OpenedTab)
					->SetForegroundTab(SkeletalMeshEditorTabs::MorphTargetsTab)
				)
			)
		);

	PersonaModule.OnRegisterTabs().Broadcast(TabFactories, InHostingApp);
	LayoutExtender = MakeShared<FLayoutExtender>();
	PersonaModule.OnRegisterLayoutExtensions().Broadcast(*LayoutExtender.Get());
	TabLayout->ProcessExtensions(*LayoutExtender.Get());

	SkeletalMeshEditor->RegisterModeToolbarIfUnregistered(GetModeName());
}

void FSkeletalMeshEditorMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FWorkflowCentricApplication> HostingApp = HostingAppPtr.Pin();
	HostingApp->RegisterTabSpawners(InTabManager.ToSharedRef());
	HostingApp->PushTabFactories(TabFactories);

	FApplicationMode::RegisterTabFactories(InTabManager);
}


void FSkeletalMeshEditorMode::AddTabFactory(FCreateWorkflowTabFactory FactoryCreator)
{
	if (FactoryCreator.IsBound())
	{
		TabFactories.RegisterFactory(FactoryCreator.Execute(HostingAppPtr.Pin()));
	}
}

void FSkeletalMeshEditorMode::RemoveTabFactory(FName TabFactoryID)
{
	TabFactories.UnregisterFactory(TabFactoryID);
}

TSharedRef<class FWorkflowTabFactory> FSkeletalMeshEditorMode::CreateMeshControllerMappingTabFactory(const TSharedRef<class FWorkflowCentricApplication>& InHostingApp, const TWeakObjectPtr<class USkeletalMesh>& InEditingMesh, FSimpleMulticastDelegate& OnPostUndo) const
{
	return MakeShareable(new FMeshControllerMappingTabSummoner(InHostingApp, InEditingMesh, OnPostUndo));
}

/////////////////////////////////////////////////////
// FAnimationMappingWindowTabSummoner
static const FName ControlRigMappingWindowID("ControlRigMappingWindow");

FMeshControllerMappingTabSummoner::FMeshControllerMappingTabSummoner(TSharedPtr<class FAssetEditorToolkit> InHostingApp, const TWeakObjectPtr<class USkeletalMesh>& InEditingMesh, FSimpleMulticastDelegate& InOnPostUndo)
	: FWorkflowTabFactory(ControlRigMappingWindowID, InHostingApp)
	, SkeletalMesh(InEditingMesh)
	, OnPostUndo(InOnPostUndo)
{
	TabLabel = LOCTEXT("ControlRigMappingWindowTabTitle", "Control Rig");
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Persona.Tabs.ControlRigMappingWindow");

	EnableTabPadding();
	bIsSingleton = true;

	ViewMenuDescription = LOCTEXT("ControlRigMappingWindowTabView", "Control Rig");
	ViewMenuTooltip = LOCTEXT("ControlRigMappingWindowTabView_ToolTip", "Configure Animation Controller Settings");
}

TSharedRef<SWidget> FMeshControllerMappingTabSummoner::CreateTabBody(const FWorkflowTabSpawnInfo& Info) const
{
	return SNew(SControlRigMappingWindow, SkeletalMesh, OnPostUndo);
}

TSharedPtr<SToolTip> FMeshControllerMappingTabSummoner::CreateTabToolTipWidget(const FWorkflowTabSpawnInfo& Info) const
{
	return  IDocumentation::Get()->CreateToolTip(LOCTEXT("ControlRigMappingWindowTooltip", "In this panel, you can add new animation controllers and configure settings"), NULL, TEXT("Shared/Editors/Persona"), TEXT("ControlRigMappingWindow"));
}

#undef LOCTEXT_NAMESPACE
