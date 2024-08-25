// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraModeEditorToolkit.h"

#include "Core/CameraMode.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IGameplayCamerasEditorModule.h"
#include "IMessageLogListing.h"
#include "MessageLogInitializationOptions.h"
#include "MessageLogModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkflowOrientedApp/WorkflowTabFactory.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"

#define LOCTEXT_NAMESPACE "CameraModeEditorToolkit"

const FName FCameraModeEditorTabs::DetailsViewTabId(TEXT("CameraModeEditor_DetailsView"));

const FName FCameraModeEditorApplicationModes::StandardCameraModeEditorMode(TEXT("StandardCameraModeEditorMode"));

TSharedPtr<FTabManager::FLayout> FCameraModeEditorApplicationModes::GetDefaultEditorLayout(TSharedPtr<ICameraModeEditorToolkit> InCameraModeEditor)
{
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("CameraModeEditor_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(FCameraModeEditorTabs::DetailsViewTabId, ETabState::OpenedTab)
					->SetForegroundTab(FCameraModeEditorTabs::DetailsViewTabId)
				)
			)
		);
	return DefaultLayout;
}

FCameraModeEditorApplicationMode::FCameraModeEditorApplicationMode(TSharedPtr<ICameraModeEditorToolkit> InCameraModeEditor)
	: FApplicationMode(FCameraModeEditorApplicationModes::StandardCameraModeEditorMode)
{
	WeakCameraModeEditor = InCameraModeEditor;
	//CameraModeEditorTabFactories.RegisterFactory(InCameraModeEditor->GraphEditorTabFactoryPtr.Pin().ToSharedRef());
	TabLayout = FCameraModeEditorApplicationModes::GetDefaultEditorLayout(InCameraModeEditor);
}

void FCameraModeEditorApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	if (TSharedPtr<ICameraModeEditorToolkit> CameraModeEditor = WeakCameraModeEditor.Pin())
	{
		// Hack?
		StaticCastSharedPtr<FCameraModeEditorToolkit>(CameraModeEditor)->InternalRegisterTabSpawners(InTabManager.ToSharedRef());

		CameraModeEditor->PushTabFactories(CameraModeEditorTabFactories);
	}

	FApplicationMode::RegisterTabFactories(InTabManager);
}

FCameraModeEditorToolkit::FCameraModeEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: CameraMode(nullptr)
	, Style(InStyle)
{
	DocumentManager = MakeShareable(new FDocumentTracker());
}

FCameraModeEditorToolkit::~FCameraModeEditorToolkit()
{
}

void FCameraModeEditorToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraMode* InCameraMode)
{
	CameraMode = InCameraMode;

	CreateWidgets();

	TSharedPtr<FCameraModeEditorToolkit> ThisPtr(SharedThis(this));
	DocumentManager->Initialize(ThisPtr);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;

	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());

	FWorkflowCentricApplication::InitAssetEditor(
			Mode,
			InitToolkitHost, 
			IGameplayCamerasEditorModule::GameplayCamerasEditorAppIdentifier, 
			DummyLayout, 
			bCreateDefaultStandaloneMenu, 
			bCreateDefaultToolbar,
			CameraMode);

	
	AddApplicationMode(
		FCameraModeEditorApplicationModes::StandardCameraModeEditorMode,
		MakeShareable(new FCameraModeEditorApplicationMode(SharedThis(this))));
	SetCurrentMode(FCameraModeEditorApplicationModes::StandardCameraModeEditorMode);
}

void FCameraModeEditorToolkit::CreateWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

	TArray<TWeakObjectPtr<>> SelectedObjects { CameraMode };
	DetailsView->SetObjects(SelectedObjects, true);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false;
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;
	StatsListing = MessageLogModule.CreateLogListing("CameraModeEditorStats", LogOptions);

	Stats = MessageLogModule.CreateLogListingWidget(StatsListing.ToSharedRef());
}

void FCameraModeEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);
	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FCameraModeEditorToolkit::InternalRegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_CameraModeEditor", "Camera Mode Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	InTabManager->RegisterTabSpawner(FCameraModeEditorTabs::DetailsViewTabId, FOnSpawnTab::CreateSP(this, &FCameraModeEditorToolkit::SpawnTab_DetailsView))
		.SetDisplayName(LOCTEXT("DetailsViewTabTitle", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FCameraModeEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FWorkflowCentricApplication::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FCameraModeEditorTabs::DetailsViewTabId);
}

TSharedRef<SDockTab> FCameraModeEditorToolkit::SpawnTab_DetailsView(const FSpawnTabArgs& Args)
{
	TSharedPtr<SDockTab> DetailsTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsViewTabTitle", "Details"))
		[
			DetailsView.ToSharedRef()
		];
	DetailsViewTab = DetailsTab;
	return DetailsTab.ToSharedRef();
}

FText FCameraModeEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Camera Mode");
}

FName FCameraModeEditorToolkit::GetToolkitFName() const
{
	static FName SequencerName("CameraModeEditor");
	return SequencerName;
}

FString FCameraModeEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Camera Mode ").ToString();
}

FText FCameraModeEditorToolkit::GetTabSuffix() const
{
	if (CameraMode)
	{
		const bool bIsDirty = CameraMode->GetOutermost()->IsDirty();
		if (bIsDirty)
		{
			return LOCTEXT("TabSuffixAsterix", "*");
		}
	}

	return FText::GetEmpty();
}

FLinearColor FCameraModeEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.7, 0.0f, 0.0f, 0.5f);
}

#undef LOCTEXT_NAMESPACE

