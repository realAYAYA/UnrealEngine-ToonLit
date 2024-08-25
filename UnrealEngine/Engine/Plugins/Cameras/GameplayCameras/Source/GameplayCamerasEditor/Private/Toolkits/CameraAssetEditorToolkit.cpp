// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraAssetEditorToolkit.h"

#include "Core/CameraAsset.h"
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

#define LOCTEXT_NAMESPACE "CameraAssetEditorToolkit"

const FName FCameraAssetEditorTabs::DetailsViewTabId(TEXT("CameraAssetEditor_DetailsView"));

const FName FCameraAssetEditorApplicationModes::StandardCameraAssetEditorMode(TEXT("StandardCameraAssetEditorMode"));

TSharedPtr<FTabManager::FLayout> FCameraAssetEditorApplicationModes::GetDefaultEditorLayout(TSharedPtr<ICameraAssetEditorToolkit> InCameraAssetEditor)
{
	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("CameraAssetEditor_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(FCameraAssetEditorTabs::DetailsViewTabId, ETabState::OpenedTab)
					->SetForegroundTab(FCameraAssetEditorTabs::DetailsViewTabId)
				)
			)
		);
	return DefaultLayout;
}

FCameraAssetEditorApplicationMode::FCameraAssetEditorApplicationMode(TSharedPtr<ICameraAssetEditorToolkit> InCameraAssetEditor)
	: FApplicationMode(FCameraAssetEditorApplicationModes::StandardCameraAssetEditorMode)
{
	WeakCameraAssetEditor = InCameraAssetEditor;
	//CameraAssetEditorTabFactories.RegisterFactory(InCameraAssetEditor->GraphEditorTabFactoryPtr.Pin().ToSharedRef());
	TabLayout = FCameraAssetEditorApplicationModes::GetDefaultEditorLayout(InCameraAssetEditor);
}

void FCameraAssetEditorApplicationMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	if (TSharedPtr<ICameraAssetEditorToolkit> CameraAssetEditor = WeakCameraAssetEditor.Pin())
	{
		// Hack?
		StaticCastSharedPtr<FCameraAssetEditorToolkit>(CameraAssetEditor)->InternalRegisterTabSpawners(InTabManager.ToSharedRef());

		CameraAssetEditor->PushTabFactories(CameraAssetEditorTabFactories);
	}

	FApplicationMode::RegisterTabFactories(InTabManager);
}

FCameraAssetEditorToolkit::FCameraAssetEditorToolkit(const TSharedRef<ISlateStyle>& InStyle)
	: CameraAsset(nullptr)
	, Style(InStyle)
{
	DocumentManager = MakeShareable(new FDocumentTracker());
}

FCameraAssetEditorToolkit::~FCameraAssetEditorToolkit()
{
}

void FCameraAssetEditorToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraAsset* InCameraAsset)
{
	CameraAsset = InCameraAsset;

	CreateWidgets();

	TSharedPtr<FCameraAssetEditorToolkit> ThisPtr(SharedThis(this));
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
			CameraAsset);

	
	AddApplicationMode(
		FCameraAssetEditorApplicationModes::StandardCameraAssetEditorMode,
		MakeShareable(new FCameraAssetEditorApplicationMode(SharedThis(this))));
	SetCurrentMode(FCameraAssetEditorApplicationModes::StandardCameraAssetEditorMode);
}

void FCameraAssetEditorToolkit::CreateWidgets()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsView = PropertyEditorModule.CreateDetailView( DetailsViewArgs );

	TArray<TWeakObjectPtr<>> SelectedObjects { CameraAsset };
	DetailsView->SetObjects(SelectedObjects, true);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false;
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;
	StatsListing = MessageLogModule.CreateLogListing("CameraAssetEditorStats", LogOptions);

	Stats = MessageLogModule.CreateLogListingWidget(StatsListing.ToSharedRef());
}

void FCameraAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	DocumentManager->SetTabManager(InTabManager);
	FWorkflowCentricApplication::RegisterTabSpawners(InTabManager);
}

void FCameraAssetEditorToolkit::InternalRegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_CameraAssetEditor", "Camera Asset Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	InTabManager->RegisterTabSpawner(FCameraAssetEditorTabs::DetailsViewTabId, FOnSpawnTab::CreateSP(this, &FCameraAssetEditorToolkit::SpawnTab_DetailsView))
		.SetDisplayName(LOCTEXT("DetailsViewTabTitle", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FCameraAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FWorkflowCentricApplication::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(FCameraAssetEditorTabs::DetailsViewTabId);
}

TSharedRef<SDockTab> FCameraAssetEditorToolkit::SpawnTab_DetailsView(const FSpawnTabArgs& Args)
{
	TSharedPtr<SDockTab> DetailsTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsViewTabTitle", "Details"))
		[
			DetailsView.ToSharedRef()
		];
	DetailsViewTab = DetailsTab;
	return DetailsTab.ToSharedRef();
}

FText FCameraAssetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Camera Asset");
}

FName FCameraAssetEditorToolkit::GetToolkitFName() const
{
	static FName SequencerName("CameraAssetEditor");
	return SequencerName;
}

FString FCameraAssetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Camera Asset ").ToString();
}

FText FCameraAssetEditorToolkit::GetTabSuffix() const
{
	if (CameraAsset)
	{
		const bool bIsDirty = CameraAsset->GetOutermost()->IsDirty();
		if (bIsDirty)
		{
			return LOCTEXT("TabSuffixAsterix", "*");
		}
	}

	return FText::GetEmpty();
}

FLinearColor FCameraAssetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.7, 0.0f, 0.0f, 0.5f);
}

#undef LOCTEXT_NAMESPACE

