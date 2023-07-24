// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditor.h"
#include "PoseSearchDatabaseAssetTree.h"
#include "SPoseSearchDatabaseViewport.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchDatabaseEditorCommands.h"
#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchDatabaseEditorReflection.h"
#include "PoseSearchEditor.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "GameFramework/WorldSettings.h"
#include "AdvancedPreviewSceneModule.h"
#include "InstancedStruct.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "PoseSearchDatabaseEditor"

namespace UE::PoseSearch
{
	const FName PoseSearchDatabaseEditorAppName = FName(TEXT("PoseSearchDatabaseEditorApp"));
	constexpr double ViewRangeSlack = 0.25;

	// Tab identifiers
	struct FDatabaseEditorTabs
	{
		static const FName AssetDetailsID;
		static const FName ViewportID;
		static const FName PreviewSettingsID;
		static const FName AssetTreeViewID;
		static const FName SelectionDetailsID;
		static const FName StatisticsOverview;
	};

	const FName FDatabaseEditorTabs::AssetDetailsID(TEXT("PoseSearchDatabaseEditorAssetDetailsTabID"));
	const FName FDatabaseEditorTabs::ViewportID(TEXT("PoseSearchDatabaseEditorViewportTabID"));
	const FName FDatabaseEditorTabs::PreviewSettingsID(TEXT("PoseSearchDatabaseEditorPreviewSettingsTabID"));
	const FName FDatabaseEditorTabs::AssetTreeViewID(TEXT("PoseSearchDatabaseEditorAssetTreeViewTabID"));
	const FName FDatabaseEditorTabs::SelectionDetailsID(TEXT("PoseSearchDatabaseEditorSelectionDetailsID"));
	const FName FDatabaseEditorTabs::StatisticsOverview(TEXT("PoseSearchDatabaseEditorStatisticsOverviewID"));
	
	const UPoseSearchDatabase* FDatabaseEditor::GetPoseSearchDatabase() const
	{
		return ViewModel.IsValid() ? ViewModel->GetPoseSearchDatabase() : nullptr;
	}

	UPoseSearchDatabase* FDatabaseEditor::GetPoseSearchDatabase()
	{
		return ViewModel.IsValid() ? ViewModel->GetPoseSearchDatabase() : nullptr;
	}

	void FDatabaseEditor::SetSelectedAsset(int32 SourceAssetIdx)
	{
		if (ViewModel.IsValid())
		{
			const TWeakPtr<FDatabaseAssetTreeNode> Node = AssetTreeWidget->SetSelectedItem(SourceAssetIdx);
			
			if (Node.IsValid())
			{
				ViewModel->SetSelectedNode(Node.Pin());
			}
		}
	}

	void FDatabaseEditor::BuildSearchIndex()
	{
		ViewModel->BuildSearchIndex();
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
		// Bind Commands
		BindCommands();

		// Create Preview Scene
		if (!PreviewScene.IsValid())
		{
			PreviewScene = MakeShareable(
				new FDatabasePreviewScene(
					FPreviewScene::ConstructionValues()
					.AllowAudioPlayback(true)
					.ShouldSimulatePhysics(true)
					.ForceUseMovementComponentInNonGameWorld(true),
					StaticCastSharedRef<FDatabaseEditor>(AsShared())));

			//Temporary fix for missing attached assets - MDW (Copied from FPersonaToolkit::CreatePreviewScene)
			PreviewScene->GetWorld()->GetWorldSettings()->SetIsTemporarilyHiddenInEditor(false);
		}

		// Create view model
		ViewModel = MakeShared<FDatabaseViewModel>();
		ViewModel->Initialize(DatabaseAsset, PreviewScene.ToSharedRef());

		// Create viewport widget
		{
			FDatabasePreviewRequiredArgs PreviewArgs(
				StaticCastSharedRef<FDatabaseEditor>(AsShared()),
				PreviewScene.ToSharedRef());
			
			PreviewWidget = SNew(SDatabasePreview, PreviewArgs)
				.SliderScrubTime_Lambda([this]() { return ViewModel->GetPlayTime(); })
				.SliderViewRange_Lambda([this]() 
				{ 
					return TRange<double>(-ViewRangeSlack, ViewModel->GetMaxPreviewPlayLength() + ViewRangeSlack);
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
				if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(GetPoseSearchDatabase(), ERequestAsyncBuildFlag::ContinueRequest))
				{
					RefreshStatisticsWidgetInformation();
				}

				// Ensure any database changes are reflected
				DatabaseAsset->RegisterOnDerivedDataRebuild(UPoseSearchDatabase::FOnDerivedDataRebuild::CreateSP(this, &FDatabaseEditor::RefreshStatisticsWidgetInformation));
			}
		}
		
		// Define Editor Layout
		const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
		FTabManager::NewLayout("Standalone_PoseSearchDatabaseEditor_Layout_v0.08")
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.9f)
					->Split
					(
						FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
						->Split
						(
							FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.9f)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.5f)
								->AddTab(FDatabaseEditorTabs::AssetTreeViewID, ETabState::OpenedTab)
								->SetHideTabWell(false)
							)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.5f)
								->AddTab(FDatabaseEditorTabs::AssetDetailsID, ETabState::OpenedTab)
								->SetHideTabWell(false)
							)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.4f)
							->AddTab(FDatabaseEditorTabs::ViewportID, ETabState::OpenedTab)
							->SetHideTabWell(true)
						)
						->Split
						(
							FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.3f)
								->AddTab(FDatabaseEditorTabs::SelectionDetailsID, ETabState::OpenedTab)
								->AddTab(FDatabaseEditorTabs::PreviewSettingsID, ETabState::OpenedTab)
							)
							->Split
							(
								FTabManager::NewStack()
								->SetSizeCoefficient(0.5f)
								->AddTab(FDatabaseEditorTabs::StatisticsOverview, ETabState::OpenedTab)
							)
						)
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

		ExtendToolbar();

		RegenerateMenusAndToolbars();
	}

	void FDatabaseEditor::BindCommands()
	{
		const FDatabaseEditorCommands& Commands = FDatabaseEditorCommands::Get();

		ToolkitCommands->MapAction(
			Commands.BuildSearchIndex,
			FExecuteAction::CreateSP(this, &FDatabaseEditor::BuildSearchIndex),
			EUIActionRepeatMode::RepeatDisabled);
	}

	void FDatabaseEditor::ExtendToolbar()
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

		AddToolbarExtender(ToolbarExtender);

		ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			GetToolkitCommands(),
			FToolBarExtensionDelegate::CreateSP(this, &FDatabaseEditor::FillToolbar));
	}

	void FDatabaseEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
	{
		ToolbarBuilder.AddToolBarButton(
			FDatabaseEditorCommands::Get().BuildSearchIndex,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon());
	}

	void FDatabaseEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
			LOCTEXT("WorkspaceMenu_PoseSearchDbEditor", "Pose Search Database Editor"));
		auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

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
		Args.Add(TEXT("AssetName"), FText::FromString(GetPoseSearchDatabase()->GetName()));
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
			.Label(LOCTEXT("AssetTreeView_Title", "Asset Tree"))
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

	void FDatabaseEditor::OnFinishedChangingSelectionProperties(const FPropertyChangedEvent& PropertyChangedEvent)
	{
	}

	void FDatabaseEditor::OnAssetTreeSelectionChanged(
		const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& SelectedItems,
		ESelectInfo::Type SelectionType)
	{
		// Reset selected objects on all selection widgets
		for (auto& SelectionWidgetPair : SelectionWidgets)
		{
			SelectionWidgetPair.Value.SelectedReflections.Reset();
		}

		if (SelectedItems.Num() > 0)
		{
			for (TSharedPtr<FDatabaseAssetTreeNode>& SelectedItem : SelectedItems)
			{
				if (!SelectedItem.IsValid() ||
					!GetPoseSearchDatabase()->AnimationAssets.IsValidIndex(SelectedItem->SourceAssetIdx))
				{
					continue;
				}

				const FInstancedStruct& DatabaseAsset = GetPoseSearchDatabase()->AnimationAssets[SelectedItem->SourceAssetIdx];
				const UScriptStruct* ScriptStruct = DatabaseAsset.GetScriptStruct();
				FSelectionWidget& SelectionWidget = FindOrAddSelectionWidget(ScriptStruct);

				if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetPtr<FPoseSearchDatabaseSequence>())
				{
					UPoseSearchDatabaseSequenceReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseSequenceReflection>();
					NewSelectionReflection->AddToRoot();
					NewSelectionReflection->Sequence = *DatabaseSequence;
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);

					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimComposite>())
				{
					UPoseSearchDatabaseAnimCompositeReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseAnimCompositeReflection>();
					NewSelectionReflection->AddToRoot();
					NewSelectionReflection->AnimComposite = *DatabaseAnimComposite;
					NewSelectionReflection->SetSourceLink(SelectedItem, AssetTreeWidget);
					NewSelectionReflection->SetFlags(RF_Transactional);
					
					SelectionWidget.SelectedReflections.Add(NewSelectionReflection);
				}
				else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
				{
					UPoseSearchDatabaseBlendSpaceReflection* NewSelectionReflection = NewObject<UPoseSearchDatabaseBlendSpaceReflection>();
					NewSelectionReflection->AddToRoot();
					NewSelectionReflection->BlendSpace = *DatabaseBlendSpace;
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

		for (auto& SelectionWidgetPair : SelectionWidgets)
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

		ViewModel->SetSelectedNodes(SelectedItems);
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

		DetailsContainer->AddSlot()
			.AutoHeight()
			[
				SelectionWidget.DetailView.ToSharedRef()
			];

		return SelectionWidget;
	}

	void FDatabaseEditor::RefreshStatisticsWidgetInformation()
	{
		UPoseSearchDatabaseStatistics* Statistics = NewObject<UPoseSearchDatabaseStatistics>();
		Statistics->AddToRoot();
		Statistics->Initialize(GetPoseSearchDatabase());
		StatisticsOverviewWidget->SetObject(Statistics);
	}
}

#undef LOCTEXT_NAMESPACE
