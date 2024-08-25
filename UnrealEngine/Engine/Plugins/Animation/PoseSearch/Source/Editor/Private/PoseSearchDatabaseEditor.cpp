// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditor.h"
#include "AdvancedPreviewSceneModule.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/WorldSettings.h"
#include "IStructureDetailsView.h"
#include "InstancedStruct.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearchDatabaseAssetTree.h"
#include "PoseSearchDatabaseDataDetails.h"
#include "PoseSearchDatabaseEditorCommands.h"
#include "PoseSearchDatabaseEditorReflection.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchEditor.h"
#include "PropertyEditorModule.h"
#include "SPoseSearchDatabaseViewport.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"

#define LOCTEXT_NAMESPACE "PoseSearchDatabaseEditor"

namespace UE::PoseSearch
{
	const FName PoseSearchDatabaseEditorAppName = FName(TEXT("PoseSearchDatabaseEditorApp"));

	// Tab identifiers
	struct FDatabaseEditorTabs
	{
		static const FName AssetDetailsID;
		static const FName ViewportID;
		static const FName PreviewSettingsID;
		static const FName AssetTreeViewID;
		static const FName SelectionDetailsID;
		static const FName StatisticsOverview;
		static const FName DataDetailsID;
	};

	const FName FDatabaseEditorTabs::AssetDetailsID(TEXT("PoseSearchDatabaseEditorAssetDetailsTabID"));
	const FName FDatabaseEditorTabs::ViewportID(TEXT("PoseSearchDatabaseEditorViewportTabID"));
	const FName FDatabaseEditorTabs::PreviewSettingsID(TEXT("PoseSearchDatabaseEditorPreviewSettingsTabID"));
	const FName FDatabaseEditorTabs::AssetTreeViewID(TEXT("PoseSearchDatabaseEditorAssetTreeViewTabID"));
	const FName FDatabaseEditorTabs::SelectionDetailsID(TEXT("PoseSearchDatabaseEditorSelectionDetailsID"));
	const FName FDatabaseEditorTabs::StatisticsOverview(TEXT("PoseSearchDatabaseEditorStatisticsOverviewID"));
	const FName FDatabaseEditorTabs::DataDetailsID(TEXT("PoseSearchDatabaseEditorDataDetailsTabID"));

	const UPoseSearchDatabase* FDatabaseEditor::GetPoseSearchDatabase() const
	{
		return ViewModel.IsValid() ? ViewModel->GetPoseSearchDatabase() : nullptr;
	}

	UPoseSearchDatabase* FDatabaseEditor::GetPoseSearchDatabase()
	{
		return ViewModel.IsValid() ? ViewModel->GetPoseSearchDatabase() : nullptr;
	}

	void FDatabaseEditor::SetSelectedPoseIdx(int32 PoseIdx, bool bDrawQuery, TConstArrayView<float> InQueryVector)
	{
		if (ViewModel.IsValid())
		{
			const bool bClearSelection = !FSlateApplication::Get().GetModifierKeys().IsControlDown() || ViewModel->IsEditorSelection();
			const int32 SourceAssetIdx = ViewModel->SetSelectedNode(PoseIdx, bClearSelection, bDrawQuery, InQueryVector);
			AssetTreeWidget->SetSelectedItem(SourceAssetIdx, bClearSelection);
		}
	}

	void FDatabaseEditor::SetDrawQueryVector(bool bValue)
	{
		if (ViewModel.IsValid())
		{
			ViewModel->SetDrawQueryVector(bValue);
		}
	}

	void FDatabaseEditor::PreviewBackwardEnd()
	{
		ViewModel->PreviewBackwardEnd();
	}

	void FDatabaseEditor::PreviewBackwardStep()
	{
		ViewModel->PreviewBackwardStep();
	}

	void FDatabaseEditor::PreviewBackward()
	{
		ViewModel->PreviewBackward();
	}

	void FDatabaseEditor::PreviewPause()
	{
		ViewModel->PreviewPause();
	}

	void FDatabaseEditor::PreviewForward()
	{
		ViewModel->PreviewForward();
	}

	void FDatabaseEditor::PreviewForwardStep()
	{
		ViewModel->PreviewForwardStep();
	}

	void FDatabaseEditor::PreviewForwardEnd()
	{
		ViewModel->PreviewForwardEnd();
	}

	void FDatabaseEditor::InitAssetEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UPoseSearchDatabase* DatabaseAsset)
	{
		// Create Preview Scene
		if (!PreviewScene.IsValid())
		{
			PreviewScene = MakeShareable(
				new FDatabasePreviewScene(
					FPreviewScene::ConstructionValues()
					.SetCreatePhysicsScene(false)
					.SetTransactional(false)
					.ForceUseMovementComponentInNonGameWorld(true),
					StaticCastSharedRef<FDatabaseEditor>(AsShared())));

			//Temporary fix for missing attached assets - MDW (Copied from FPersonaToolkit::CreatePreviewScene)
			PreviewScene->GetWorld()->GetWorldSettings()->SetIsTemporarilyHiddenInEditor(false);
		}

		// Create view model
		ViewModel = MakeShared<FDatabaseViewModel>();

		// Create Data Details widget
		DataDetails = SNew(SDatabaseDataDetails, ViewModel.ToSharedRef());

		// Initialize view model
		ViewModel->Initialize(DatabaseAsset, PreviewScene.ToSharedRef(), DataDetails.ToSharedRef());

		// Initialize DataDetails
		DataDetails->Reconstruct();

		// Create viewport widget
		{
			FDatabasePreviewRequiredArgs PreviewArgs(
				StaticCastSharedRef<FDatabaseEditor>(AsShared()),
				PreviewScene.ToSharedRef());
			
			PreviewWidget = SNew(SDatabasePreview, PreviewArgs)
				.SliderColor_Lambda([this]()
					{
						return ViewModel->IsEditorSelection() ? FLinearColor::Red.CopyWithNewOpacity(0.5f) : FLinearColor::Blue.CopyWithNewOpacity(0.5f);
					})
				.SliderScrubTime_Lambda([this]()
					{
						return ViewModel->GetPlayTime();
					})
				.SliderViewRange_Lambda([this]() 
					{ 
						return ViewModel->GetPreviewPlayRange();
					})
				.OnSliderScrubPositionChanged_Lambda([this](float NewScrubPosition, bool bScrubbing)
					{
						ViewModel->SetPlayTime(NewScrubPosition, !bScrubbing);
					})
				.OnBackwardEnd_Raw(this, &FDatabaseEditor::PreviewBackwardEnd)
				.OnBackwardStep_Raw(this, &FDatabaseEditor::PreviewBackwardStep)
				.OnBackward_Raw(this, &FDatabaseEditor::PreviewBackward)
				.OnPause_Raw(this, &FDatabaseEditor::PreviewPause)
				.OnForward_Raw(this, &FDatabaseEditor::PreviewForward)
				.OnForwardStep_Raw(this, &FDatabaseEditor::PreviewForwardStep)
				.OnForwardEnd_Raw(this, &FDatabaseEditor::PreviewForwardEnd);
		}

		// Create asset tree widget
		AssetTreeWidget = SNew(SDatabaseAssetTree, ViewModel.ToSharedRef());
		AssetTreeWidget->RegisterOnSelectionChanged(
			SDatabaseAssetTree::FOnSelectionChanged::CreateSP(
				this,
				&FDatabaseEditor::OnAssetTreeSelectionChanged));
		
		// Create Asset Details widget
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Database details widget
		{
			FDetailsViewArgs DatabaseDetailsArgs;
			{
				DatabaseDetailsArgs.bHideSelectionTip = true;
				DatabaseDetailsArgs.NotifyHook = this;
			}

			EditingAssetWidget = PropertyModule.CreateDetailView(DatabaseDetailsArgs);
			EditingAssetWidget->SetObject(DatabaseAsset);
		}

		// Statistics details widgets
		{
			FDetailsViewArgs StatisticsOverviewDetailsArgs;
			{
				StatisticsOverviewDetailsArgs.bHideSelectionTip = true;
				StatisticsOverviewDetailsArgs.NotifyHook = this;
				StatisticsOverviewDetailsArgs.bAllowSearch = false;
			}
			
			StatisticsOverviewWidget = PropertyModule.CreateDetailView(StatisticsOverviewDetailsArgs);
			StatisticsOverviewWidget->SetObject(nullptr);

			// Ensure statistics information get updated/populated
			if (DatabaseAsset)
			{
				// Init statistics
				if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(GetPoseSearchDatabase(), ERequestAsyncBuildFlag::ContinueRequest))
				{
					RefreshStatisticsWidgetInformation();
				}

				// Ensure any database changes are reflected
				DatabaseAsset->UnregisterOnDerivedDataRebuild(this);
				DatabaseAsset->RegisterOnDerivedDataRebuild(UPoseSearchDatabase::FOnDerivedDataRebuild::CreateSP(this, &FDatabaseEditor::RefreshStatisticsWidgetInformation));

				DatabaseAsset->UnregisterOnSynchronizeWithExternalDependencies(this);
				DatabaseAsset->RegisterOnSynchronizeWithExternalDependencies(UPoseSearchDatabase::FOnDerivedDataRebuild::CreateSP(this, &FDatabaseEditor::RefreshEditor));
			}
		}
		
		// Define Editor Layout
		const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
		FTabManager::NewLayout("Standalone_PoseSearchDatabaseEditor_Layout_v0.13")
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.4f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.6f)
						->AddTab(FDatabaseEditorTabs::AssetTreeViewID, ETabState::OpenedTab)
						->SetHideTabWell(false)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.4f)
						->AddTab(FDatabaseEditorTabs::AssetDetailsID, ETabState::OpenedTab)
						->SetHideTabWell(false)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.6f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.6f)
						->AddTab(FDatabaseEditorTabs::ViewportID, ETabState::OpenedTab)
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.4f)
						->AddTab(FDatabaseEditorTabs::StatisticsOverview, ETabState::OpenedTab)
						->AddTab(FDatabaseEditorTabs::PreviewSettingsID, ETabState::OpenedTab)
						->AddTab(FDatabaseEditorTabs::DataDetailsID, ETabState::OpenedTab)
						->AddTab(FDatabaseEditorTabs::SelectionDetailsID, ETabState::OpenedTab)
					)
				)
			);

		const bool bCreateDefaultStandaloneMenuParam = true;
		const bool bCreateDefaultToolbarParam = true;
		const bool bIsToolbarFocusableParam = false;
		FAssetEditorToolkit::InitAssetEditor(
			Mode,
			InitToolkitHost,
			PoseSearchDatabaseEditorAppName,
			StandaloneDefaultLayout,
			bCreateDefaultStandaloneMenuParam,
			bCreateDefaultToolbarParam,
			DatabaseAsset,
			bIsToolbarFocusableParam);

		RegenerateMenusAndToolbars();
	}

	FDatabaseEditor::~FDatabaseEditor()
	{
		if (ViewModel)
		{
			if (UPoseSearchDatabase* DatabaseAsset = ViewModel->GetPoseSearchDatabase())
			{
				DatabaseAsset->UnregisterOnDerivedDataRebuild(this);
				DatabaseAsset->UnregisterOnSynchronizeWithExternalDependencies(this);
			}
		}
	}

	void FDatabaseEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
			LOCTEXT("WorkspaceMenu_PoseSearchDbEditor", "Pose Search Database Editor"));
		TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

		FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::ViewportID,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_Viewport))
			.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::AssetDetailsID,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_AssetDetails))
			.SetDisplayName(LOCTEXT("DatabaseDetailsTab", "Database Details"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::PreviewSettingsID,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_PreviewSettings))
			.SetDisplayName(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::AssetTreeViewID,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_AssetTreeView))
			.SetDisplayName(LOCTEXT("TreeViewTab", "Tree View"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::SelectionDetailsID,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_SelectionDetails))
			.SetDisplayName(LOCTEXT("SelectionDetailsTab", "Selection Details"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::StatisticsOverview,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_StatisticsOverview))
			.SetDisplayName(LOCTEXT("StatisticsOverviewTab", "Statistics Overview"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

		InTabManager->RegisterTabSpawner(
			FDatabaseEditorTabs::DataDetailsID,
			FOnSpawnTab::CreateSP(this, &FDatabaseEditor::SpawnTab_DataDetails))
			.SetDisplayName(LOCTEXT("DataDetailsTab", "Data Details"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));
	}

	void FDatabaseEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::ViewportID);
		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::AssetDetailsID);
		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::PreviewSettingsID);
		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::AssetTreeViewID);
		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::SelectionDetailsID);
		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::StatisticsOverview);
		InTabManager->UnregisterTabSpawner(FDatabaseEditorTabs::DataDetailsID);
	}

	FName FDatabaseEditor::GetToolkitFName() const
	{
		return FName("PoseSearchDatabaseEditor");
	}

	FText FDatabaseEditor::GetBaseToolkitName() const
	{
		return LOCTEXT("PoseSearchDatabaseEditorAppLabel", "Pose Search Database Editor");
	}

	FText FDatabaseEditor::GetToolkitName() const
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("AssetName"), FText::FromString(GetNameSafe(GetPoseSearchDatabase())));
		return FText::Format(LOCTEXT("PoseSearchDatabaseEditorToolkitName", "{AssetName}"), Args);
	}

	FLinearColor FDatabaseEditor::GetWorldCentricTabColorScale() const
	{
		return FLinearColor::White;
	}

	FString FDatabaseEditor::GetWorldCentricTabPrefix() const
	{
		return TEXT("PoseSearchDatabaseEditor");
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::ViewportID);

		TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).Label(LOCTEXT("ViewportTab_Title", "Viewport"));

		if (PreviewWidget.IsValid())
		{
			SpawnedTab->SetContent(PreviewWidget.ToSharedRef());
		}

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::AssetDetailsID);

		return SNew(SDockTab)
			.Label(LOCTEXT("Database_Details_Title", "Database Details"))
			[
				EditingAssetWidget.ToSharedRef()
			];
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::PreviewSettingsID);

		FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = 
			FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
		TSharedRef<SWidget> InWidget = 
			AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(PreviewScene.ToSharedRef());

		TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
			[
				InWidget
			];

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_AssetTreeView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::AssetTreeViewID);

		return SNew(SDockTab)
			.Label(LOCTEXT("AssetTreeView_Title", "Asset List"))
			[
				AssetTreeWidget.ToSharedRef()
			];
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_SelectionDetails(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::SelectionDetailsID);

		SAssignNew(DetailsContainer, SVerticalBox);

		return SNew(SDockTab)
			.Label(LOCTEXT("SelectionDetails_Title", "Selection Details"))
			[
				SNew(SScrollBox)
				+SScrollBox::Slot()
				[
					DetailsContainer.ToSharedRef()
				]
			];
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_StatisticsOverview(const FSpawnTabArgs& Args) const
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::StatisticsOverview);
		
		return SNew(SDockTab)
		.Label(LOCTEXT("StatisticsOverview_Title", "Statistics Overview"))
		[
			SNew(SScrollBox)
			+SScrollBox::Slot()
			[
				StatisticsOverviewWidget.ToSharedRef()
			]
		];
	}

	TSharedRef<SDockTab> FDatabaseEditor::SpawnTab_DataDetails(const FSpawnTabArgs& Args) const
	{
		check(Args.GetTabId() == FDatabaseEditorTabs::DataDetailsID);
		
		return SNew(SDockTab)
			.Label(LOCTEXT("DataDetails_Title", "Data Details"))
			[
				DataDetails.ToSharedRef()
			];
	}

	void FDatabaseEditor::OnFinishedChangingSelectionProperties(const FPropertyChangedEvent& PropertyChangedEvent)
	{
	}

	void FDatabaseEditor::OnAssetTreeSelectionChanged(
		const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& SelectedItems,
		ESelectInfo::Type SelectionType)
	{
		// Reset selected objects on all selection widgets
		for (TPair<const UScriptStruct*, FSelectionWidget>& SelectionWidgetPair : SelectionWidgets)
		{
			SelectionWidgetPair.Value.SelectedReflections.Reset();
		}

		const UPoseSearchDatabase* PoseSearchDatabase = GetPoseSearchDatabase();
		if (PoseSearchDatabase && SelectedItems.Num() > 0)
		{
			for (TSharedPtr<FDatabaseAssetTreeNode>& SelectedItem : SelectedItems)
			{
				if (!SelectedItem.IsValid() || !PoseSearchDatabase->GetAnimationAssets().IsValidIndex(SelectedItem->SourceAssetIdx))
				{
					continue;
				}

				const FInstancedStruct& DatabaseAsset = PoseSearchDatabase->GetAnimationAssetStruct(SelectedItem->SourceAssetIdx);
				const UScriptStruct* ScriptStruct = DatabaseAsset.GetScriptStruct();
				FSelectionWidget& SelectionWidget = FindOrAddSelectionWidget(ScriptStruct);

				if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetPtr<FPoseSearchDatabaseSequence>())
				{
					UPoseSearchDatabaseSequenceReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseSequenceReflection>();
					NewSelectionReflection->AddToRoot();
					static_cast<FPoseSearchDatabaseSequence&>(NewSelectionReflection->Sequence) = *DatabaseSequence;
					NewSelectionReflection->Sequence.bLooping = DatabaseSequence->IsLooping();
					NewSelectionReflection->Sequence.bHasRootMotion = DatabaseSequence->IsRootMotionEnabled();
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);

					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimComposite>())
				{
					UPoseSearchDatabaseAnimCompositeReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseAnimCompositeReflection>();
					NewSelectionReflection->AddToRoot();
					static_cast<FPoseSearchDatabaseAnimComposite&>(NewSelectionReflection->AnimComposite) = *DatabaseAnimComposite;
					NewSelectionReflection->AnimComposite.bLooping = DatabaseAnimComposite->IsLooping();
					NewSelectionReflection->AnimComposite.bHasRootMotion = DatabaseAnimComposite->IsRootMotionEnabled();
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);
					
					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
				{
					UPoseSearchDatabaseBlendSpaceReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseBlendSpaceReflection>();
					NewSelectionReflection->AddToRoot();
					static_cast<FPoseSearchDatabaseBlendSpace&>(NewSelectionReflection->BlendSpace) = *DatabaseBlendSpace;
					NewSelectionReflection->BlendSpace.bLooping = DatabaseBlendSpace->IsLooping();
					NewSelectionReflection->BlendSpace.bHasRootMotion = DatabaseBlendSpace->IsRootMotionEnabled();
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);
					
					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else if (const FPoseSearchDatabaseAnimMontage* DatabaseAnimMontage = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimMontage>())
				{
					UPoseSearchDatabaseAnimMontageReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseAnimMontageReflection>();
					NewSelectionReflection->AddToRoot();
					static_cast<FPoseSearchDatabaseAnimMontage&>(NewSelectionReflection->AnimMontage) = *DatabaseAnimMontage;
					NewSelectionReflection->AnimMontage.bLooping = DatabaseAnimMontage->IsLooping();
					NewSelectionReflection->AnimMontage.bHasRootMotion = DatabaseAnimMontage->IsRootMotionEnabled();
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);

					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else if (const FPoseSearchDatabaseMultiSequence* DatabaseMultiSequence = DatabaseAsset.GetPtr<FPoseSearchDatabaseMultiSequence>())
				{
					UPoseSearchDatabaseMultiSequenceReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseMultiSequenceReflection>();
					NewSelectionReflection->AddToRoot();
					static_cast<FPoseSearchDatabaseMultiSequence&>(NewSelectionReflection->MultiSequence) = *DatabaseMultiSequence;
					NewSelectionReflection->MultiSequence.bLooping = DatabaseMultiSequence->IsLooping();
					NewSelectionReflection->MultiSequence.bHasRootMotion = DatabaseMultiSequence->IsRootMotionEnabled();
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);

					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else
				{
					checkNoEntry();
				}
			}
		}

		for (TPair<const UScriptStruct*, FSelectionWidget>& SelectionWidgetPair : SelectionWidgets)
		{
			FSelectionWidget& SelectionWidget = SelectionWidgetPair.Value;

			if (SelectionWidget.SelectedReflections.IsEmpty())
			{
				SelectionWidget.DetailView->SetObject(nullptr, true);
			}
			else
			{
				SelectionWidget.DetailView->SetObjects(SelectionWidget.SelectedReflections);
			}
		}

		if (SelectionType != ESelectInfo::Direct)
		{
			ViewModel->SetSelectedNodes(SelectedItems);
		}
	}

	FDatabaseEditor::FSelectionWidget& FDatabaseEditor::FindOrAddSelectionWidget(const UScriptStruct* ScriptStructType)
	{
		if (SelectionWidgets.Contains(ScriptStructType))
		{
			return SelectionWidgets[ScriptStructType];
		}

		FSelectionWidget& SelectionWidget = SelectionWidgets.Add(ScriptStructType);

		FDetailsViewArgs SelectionDetailsArgs;
		{
			SelectionDetailsArgs.bHideSelectionTip = true;
			SelectionDetailsArgs.NotifyHook = this;
			SelectionDetailsArgs.bShowScrollBar = false;
		}

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		SelectionWidget.DetailView = PropertyModule.CreateDetailView(SelectionDetailsArgs);
		SelectionWidget.DetailView->SetObject(nullptr);
		SelectionWidget.DetailView->OnFinishedChangingProperties().AddSP(
			this,
			&FDatabaseEditor::OnFinishedChangingSelectionProperties);

		if (DetailsContainer)
		{
			DetailsContainer->AddSlot()
				.AutoHeight()
				[
					SelectionWidget.DetailView.ToSharedRef()
				];
		}

		return SelectionWidget;
	}

	void FDatabaseEditor::RefreshStatisticsWidgetInformation()
	{
		UPoseSearchDatabaseStatistics* Statistics = NewObject<UPoseSearchDatabaseStatistics>();
		Statistics->AddToRoot();
		Statistics->Initialize(GetPoseSearchDatabase());
		StatisticsOverviewWidget->SetObject(Statistics);
	}

	void FDatabaseEditor::RefreshEditor()
	{
		AssetTreeWidget->RefreshTreeView(false, true);
	}
}

#undef LOCTEXT_NAMESPACE
