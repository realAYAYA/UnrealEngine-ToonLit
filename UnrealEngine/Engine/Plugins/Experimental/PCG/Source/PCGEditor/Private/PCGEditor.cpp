// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditor.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGGraph.h"
#include "PCGGraphFactory.h"
#include "PCGInputOutputSettings.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"
#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDeterminismTestBlueprintBase.h"
#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGEditorCommands.h"
#include "PCGEditorCommon.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNodeBase.h"
#include "PCGEditorGraphNodeInput.h"
#include "PCGEditorGraphNodeOutput.h"
#include "PCGEditorGraphSchema.h"
#include "PCGEditorGraphSchemaActions.h"
#include "PCGEditorMenuContext.h"
#include "PCGEditorSettings.h"
#include "PCGEditorUtils.h"
#include "SPCGEditorGraphAttributeListView.h"
#include "SPCGEditorGraphDebugObjectWidget.h"
#include "SPCGEditorGraphDeterminism.h"
#include "SPCGEditorGraphFind.h"
#include "SPCGEditorGraphNodePalette.h"
#include "SPCGEditorGraphProfilingView.h"

#include "AssetToolsModule.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EditorAssetLibrary.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GraphEditAction.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Preferences/UnrealEdOptions.h"
#include "PropertyEditorModule.h"
#include "SNodePanel.h"
#include "Misc/ITransaction.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "PCGGraphEditor"

namespace FPCGEditor_private
{
	const FName GraphEditorID = FName(TEXT("GraphEditor"));
	const FName PropertyDetailsID = FName(TEXT("PropertyDetails"));
	const FName PaletteID = FName(TEXT("Palette"));
	const FName AttributesID = FName(TEXT("Attributes"));
	const FName FindID = FName(TEXT("Find"));
	const FName DeterminismID = FName(TEXT("Determinism"));
	const FName ProfilingID = FName(TEXT("Profiling"));
}

void FPCGEditor::Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph)
{
	PCGGraphBeingEdited = InPCGGraph;

	PCGEditorGraph = NewObject<UPCGEditorGraph>(PCGGraphBeingEdited, UPCGEditorGraph::StaticClass(), NAME_None, RF_Transactional | RF_Transient);
	PCGEditorGraph->Schema = UPCGEditorGraphSchema::StaticClass();
	PCGEditorGraph->InitFromNodeGraph(InPCGGraph);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	PropertyDetailsWidget = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);

	GraphEditorWidget = CreateGraphEditorWidget();
	PaletteWidget = CreatePaletteWidget();
	FindWidget = CreateFindWidget();
	AttributesWidget = CreateAttributesWidget();
	DeterminismWidget = CreateDeterminismWidget();
	ProfilingWidget = CreateProfilingWidget();

	BindCommands();
	RegisterToolbar();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_PCGGraphEditor_Layout_v0.4")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(				
				FTabManager::NewStack()
				->SetSizeCoefficient(0.10f)
				->SetHideTabWell(true)
				->AddTab(FPCGEditor_private::PaletteID, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.70f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.72)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::GraphEditorID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.28)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::AttributesID, ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::DeterminismID, ETabState::ClosedTab)
					->AddTab(FPCGEditor_private::FindID, ETabState::ClosedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.20f)
				->SetHideTabWell(true)
				->AddTab(FPCGEditor_private::PropertyDetailsID, ETabState::OpenedTab)
			)
		);

	const FName PCGGraphEditorAppName = FName(TEXT("PCGEditorApp"));

	InitAssetEditor(InMode, InToolkitHost, PCGGraphEditorAppName, StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, InPCGGraph);
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph()
{
	return PCGEditorGraph;
}

void FPCGEditor::SetPCGComponentBeingDebugged(UPCGComponent* InPCGComponent)
{
	if (PCGComponentBeingDebugged != InPCGComponent)
	{
		PCGComponentBeingDebugged = InPCGComponent;

		OnDebugObjectChangedDelegate.Broadcast(PCGComponentBeingDebugged); 

		if (PCGComponentBeingDebugged)
		{
			// We need to force generation so that we create debug data
			PCGComponentBeingDebugged->GenerateLocal(/*bForce=*/true);
		}
	}
}

void FPCGEditor::SetPCGNodeBeingInspected(UPCGNode* InPCGNode)
{
	if (GetPCGNodeBeingInspected() != InPCGNode)
	{
		PCGNodeBeingInspected = InPCGNode;
		OnInspectedNodeChangedDelegate.Broadcast(PCGNodeBeingInspected);	
	}
}

void FPCGEditor::JumpToNode(const UEdGraphNode* InNode)
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->JumpToNode(InNode);
	}
}

void FPCGEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_PCGEditor", "PCG Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	//TODO: Add Icons
	InTabManager->RegisterTabSpawner(FPCGEditor_private::GraphEditorID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_GraphEditor))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PaletteID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Palette))
		.SetDisplayName(LOCTEXT("PaletteTab", "Palette"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes))
		.SetDisplayName(LOCTEXT("AttributesTab", "Attributes"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::FindID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Find))
		.SetDisplayName(LOCTEXT("FindTab", "Find"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::DeterminismID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Determinism))
		.SetDisplayName(LOCTEXT("DeterminismTab", "Determinism"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::ProfilingID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Profiling))
		.SetDisplayName(LOCTEXT("ProfilingTab", "Profiling"))
		.SetGroup(WorkspaceMenuCategoryRef);
}

void FPCGEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::GraphEditorID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PaletteID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::FindID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::DeterminismID);

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FPCGEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PCGGraphBeingEdited);
}

bool FPCGEditor::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
{
	return InContext.Context == FPCGEditorCommon::ContextIdentifier;
}

void FPCGEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		if (PCGGraphBeingEdited)
		{
			PCGGraphBeingEdited->NotifyGraphChanged(EPCGChangeType::Structural);
		}

		if (GraphEditorWidget.IsValid())
		{
			GraphEditorWidget->ClearSelectionSet();
			GraphEditorWidget->NotifyGraphChanged();

			FSlateApplication::Get().DismissAllMenus();
		}
	}
}

FName FPCGEditor::GetToolkitFName() const
{
	return FName(TEXT("PCGEditor"));
}

FText FPCGEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "PCG Editor");
}

FLinearColor FPCGEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FPCGEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "PCG ").ToString();
}

void FPCGEditor::RegisterToolbar() const
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);

	UToolMenu* ToolBar = ToolMenus->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);

	const FPCGEditorCommands& PCGEditorCommands = FPCGEditorCommands::Get();
	const FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
	{
		FToolMenuSection& Section = ToolBar->AddSection("PCGToolbar", TAttribute<FText>(), InsertAfterAssetSection);
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			PCGEditorCommands.Find,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintEditor.FindInBlueprint"))); // TODO, use own application style?

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().PauseAutoRegeneration,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.PausePlaySession")));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().ForceGraphRegeneration,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SourceControl.Actions.Refresh")));

		Section.AddSeparator(NAME_None);
		Section.AddDynamicEntry("Debugging", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UPCGEditorMenuContext* Context = InSection.FindContext<UPCGEditorMenuContext>();
			if (Context && Context->PCGEditor.IsValid())
			{
				InSection.AddEntry(FToolMenuEntry::InitWidget("SelectedDebugObjectWidget", SNew(SPCGEditorGraphDebugObjectWidget, Context->PCGEditor.Pin()), FText::GetEmpty()));	
			}
		}));

		Section.AddSeparator(NAME_None);
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			 PCGEditorCommands.RunDeterminismGraphTest,
			 TAttribute<FText>(),
			 TAttribute<FText>(),
			 FSlateIcon(FAppStyle::GetAppStyleSetName(), "BlueprintDebugger.TabIcon")));

		Section.AddSeparator(NAME_None);
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			 PCGEditorCommands.EditClassDefaults,
			 TAttribute<FText>(),
			 TAttribute<FText>(),
			 FSlateIcon(FAppStyle::GetAppStyleSetName(), "FullBlueprintEditor.EditClassDefaults")));
	}
}

void FPCGEditor::BindCommands()
{
	const FPCGEditorCommands& PCGEditorCommands = FPCGEditorCommands::Get();

	ToolkitCommands->MapAction(
		PCGEditorCommands.Find,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnFind));

	ToolkitCommands->MapAction(
		PCGEditorCommands.PauseAutoRegeneration,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnPauseAutomaticRegeneration_Clicked),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsAutomaticRegenerationPaused));

	ToolkitCommands->MapAction(
		PCGEditorCommands.ForceGraphRegeneration,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnForceGraphRegeneration_Clicked));

	ToolkitCommands->MapAction(
		PCGEditorCommands.RunDeterminismGraphTest,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDeterminismGraphTest),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRunDeterminismGraphTest));

	ToolkitCommands->MapAction(
		PCGEditorCommands.EditClassDefaults,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnEditClassDefaults),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsEditClassDefaultsToggled));
	
	GraphEditorCommands->MapAction(
		PCGEditorCommands.CollapseNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCollapseNodesInSubgraph),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCollapseNodesInSubgraph));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.StartInspectNode,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnStartInspectNode));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.StopInspectNode,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnStopInspectNode));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.RunDeterminismNodeTest,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDeterminismNodeTest),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRunDeterminismNodeTest));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ExecutionModeEnabled,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSetExecutionMode, EPCGSettingsExecutionMode::Enabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsExecutionModeActive, EPCGSettingsExecutionMode::Enabled));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ExecutionModeDebug,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSetExecutionMode, EPCGSettingsExecutionMode::Debug),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsExecutionModeActive, EPCGSettingsExecutionMode::Debug));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ExecutionModeDisabled,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSetExecutionMode, EPCGSettingsExecutionMode::Disabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsExecutionModeActive, EPCGSettingsExecutionMode::Disabled));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ExecutionModeIsolated,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSetExecutionMode, EPCGSettingsExecutionMode::Isolated),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsExecutionModeActive, EPCGSettingsExecutionMode::Isolated));
}

void FPCGEditor::OnFind()
{
	if (TabManager.IsValid() && FindWidget.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::FindID);
		FindWidget->FocusForUse();
	}
}

void FPCGEditor::OnPauseAutomaticRegeneration_Clicked()
{
	if (!PCGGraphBeingEdited)
	{
		return;
	}

	PCGGraphBeingEdited->ToggleUserPausedNotificationsForEditor();
}

bool FPCGEditor::IsAutomaticRegenerationPaused() const
{
	return PCGGraphBeingEdited && PCGGraphBeingEdited->NotificationsForEditorArePausedByUser();
}

void FPCGEditor::OnForceGraphRegeneration_Clicked()
{
	if (PCGGraphBeingEdited)
	{
		PCGGraphBeingEdited->ForceNotificationForEditor();
	}
}

bool FPCGEditor::CanRunDeterminismNodeTest() const
{
	check(GraphEditorWidget.IsValid());

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		if (Cast<const UPCGEditorGraphNodeBase>(Object) && !Cast<const UPCGEditorGraphNodeInput>(Object) && !Cast<const UPCGEditorGraphNodeOutput>(Object))
		{
			return true;
		}
	}

	return false;
}

void FPCGEditor::OnDeterminismNodeTest()
{
	check(GraphEditorWidget.IsValid());

	if (!DeterminismWidget.IsValid() || !DeterminismWidget->WidgetIsConstructed())
	{
		return;
	}

	TMap<FName, FTestColumnInfo> TestsConducted;
	DeterminismWidget->ClearItems();
	DeterminismWidget->BuildBaseColumns();

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Gets an appropriate width for each new column
		auto GetSlateTextWidth = [](const FText& Text) -> float
		{
			check(FSlateApplication::Get().GetRenderer());
			const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			// TODO: Verify the below property for this part of the UI
			FSlateFontInfo FontInfo(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"));
			constexpr float Padding = 30.f;
			return Padding + FontMeasure->Measure(Text, FontInfo).X;
		};

		if (!Object->IsA<UPCGEditorGraphNodeInput>() && !Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			if (const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<const UPCGEditorGraphNodeBase>(Object))
			{
				const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
				check(PCGNode && PCGNode->DefaultSettings);

				TSharedPtr<FDeterminismTestResult> NodeResult = MakeShared<FDeterminismTestResult>();
				NodeResult->TestResultTitle = PCGNode->GetNodeTitle();
				NodeResult->TestResultName = PCGNode->GetName();
				NodeResult->Seed = PCGNode->DefaultSettings->Seed;

				if (PCGNode->DefaultSettings->DeterminismSettings.bNativeTests)
				{
					// If the settings has a native test suite
					if (TFunction<bool()> NativeTestSuite = PCGDeterminismTests::FNativeTestRegistry::GetNativeTestFunction(PCGNode->DefaultSettings))
					{
						FName NodeName(PCGNode->GetName());

						bool bSuccess = NativeTestSuite();
						NodeResult->TestResults.Emplace(NodeName, bSuccess ? EDeterminismLevel::Basic : EDeterminismLevel::NoDeterminism);
						NodeResult->AdditionalDetails.Emplace(FString(TEXT("Native test conducted for - ")) + NodeName.ToString());
						NodeResult->bFlagRaised = !bSuccess;

						FText ColumnText = NSLOCTEXT("PCGDeterminism", "NativeTest", "Native Test");
						TestsConducted.FindOrAdd(NodeName, {NodeName, ColumnText, GetSlateTextWidth(ColumnText), HAlign_Center});
					}
					else // There is no native test suite, so run the basic tests
					{
						PCGDeterminismTests::FNodeTestInfo BasicTestInfo = PCGDeterminismTests::Defaults::DeterminismBasicTestInfo;
						PCGDeterminismTests::RunDeterminismTest(PCGNode, *NodeResult, BasicTestInfo);
						TestsConducted.FindOrAdd(BasicTestInfo.TestName, {BasicTestInfo.TestName, BasicTestInfo.TestLabel, BasicTestInfo.TestLabelWidth, HAlign_Center});

						PCGDeterminismTests::FNodeTestInfo OrderIndependenceTestInfo = PCGDeterminismTests::Defaults::DeterminismOrderIndependenceInfo;
						PCGDeterminismTests::RunDeterminismTest(PCGNode, *NodeResult, OrderIndependenceTestInfo);
						TestsConducted.FindOrAdd(OrderIndependenceTestInfo.TestName, {OrderIndependenceTestInfo.TestName, OrderIndependenceTestInfo.TestLabel, OrderIndependenceTestInfo.TestLabelWidth, HAlign_Center});
					}
				}

				// Custom tests
				if (PCGNode->DefaultSettings->DeterminismSettings.bUseBlueprintDeterminismTest)
				{
					TSubclassOf<UPCGDeterminismTestBlueprintBase> Blueprint = PCGNode->DefaultSettings->DeterminismSettings.DeterminismTestBlueprint;
					Blueprint.GetDefaultObject()->ExecuteTest(PCGNode, *NodeResult);
					FName BlueprintName(Blueprint->GetName());

					FText ColumnText = FText::FromString(Blueprint->GetName());
					TestsConducted.FindOrAdd(BlueprintName, {BlueprintName, ColumnText, GetSlateTextWidth(ColumnText), HAlign_Center});
				}

				DeterminismWidget->AddItem(NodeResult);
			}
		}
	}

	for (const TTuple<FName, FTestColumnInfo>& Test : TestsConducted)
	{
		DeterminismWidget->AddColumn(Test.Value);
	}

	DeterminismWidget->AddDetailsColumn();
	DeterminismWidget->RefreshItems();

	// Give focus to the Determinism Output Tab
	if (TabManager.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::DeterminismID);
	}
}

bool FPCGEditor::CanRunDeterminismGraphTest() const
{
	return PCGEditorGraph && PCGComponentBeingDebugged;
}

void FPCGEditor::OnDeterminismGraphTest()
{
	check(GraphEditorWidget.IsValid());

	if (!DeterminismWidget.IsValid() || !DeterminismWidget->WidgetIsConstructed() || !PCGGraphBeingEdited || !PCGComponentBeingDebugged)
	{
		return;
	}

	if (PCGComponentBeingDebugged->GetGraph() != PCGGraphBeingEdited)
	{
		// TODO: Should we alert the user more directly or disable this altogether?
		UE_LOG(LogPCGEditor, Warning, TEXT("Running Determinism on a PCG Component with different/no attached PCG Graph"));
	}

	DeterminismWidget->ClearItems();
	DeterminismWidget->BuildBaseColumns();

	FTestColumnInfo ColumnInfo({PCGDeterminismTests::Defaults::GraphResultName, NSLOCTEXT("PCGDeterminism", "Result", "Result"), 120.f, HAlign_Center});
	DeterminismWidget->AddColumn(ColumnInfo);

	TSharedPtr<FDeterminismTestResult> TestResult = MakeShared<FDeterminismTestResult>();
	TestResult->TestResultTitle = TEXT("Full Graph Test");
	TestResult->TestResultName = PCGGraphBeingEdited->GetName();
	TestResult->Seed = PCGComponentBeingDebugged->Seed;

	PCGDeterminismTests::RunDeterminismTest(PCGGraphBeingEdited, PCGComponentBeingDebugged, *TestResult);

	DeterminismWidget->AddItem(TestResult);
	DeterminismWidget->AddDetailsColumn();
	DeterminismWidget->RefreshItems();

	// Give focus to the Determinism Output Tab
	if (TabManager.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::DeterminismID);
	}
}

void FPCGEditor::OnEditClassDefaults() const
{
	PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);
}

bool FPCGEditor::IsEditClassDefaultsToggled() const
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyDetailsWidget->GetSelectedObjects();
	return SelectedObjects.Num() == 1 && SelectedObjects[0] == PCGGraphBeingEdited;
}

bool FPCGEditor::CanCollapseNodesInSubgraph() const
{
	bool HasPCGNode = false;

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (Object->IsA<UPCGEditorGraphNodeBase>())
		{
			if (HasPCGNode)
			{
				return true;
			}

			HasPCGNode = true;
		}
	}

	return false;
}

void FPCGEditor::OnCollapseNodesInSubgraph()
{
	if (!GraphEditorWidget.IsValid() || PCGEditorGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("GraphEditorWidget or PCGEditorGraph is null, aborting"));
		return;
	}
	
	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	if (PCGGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("PCGGraph is null, aborting"));
		return;
	}

	// 1. Gather all nodes that will be included in the subgraph, and the extra nodes
	// Also keep track of all node positions to know where to spawn our subgraph node
	FVector2D AveragePosition = FVector2D::ZeroVector;
	int32 MinX = std::numeric_limits<int32>::max();
	int32 MaxX = std::numeric_limits<int32>::min();

	// We keep track of all the PCG nodes that will go into the subgraph
	TArray<TObjectPtr<UPCGEditorGraphNodeBase>> PCGEditorGraphNodes;
	// Also move the extra nodes (like comments)
	TArray<TObjectPtr<UEdGraphNode>> ExtraGraphNodes;

	// Those 3 sets are used to keep track of the subgraph pins
	// and will be used to extract pins that will be outside the subgraph
	// and will need special treatment.
	TSet<TObjectPtr<UPCGPin>> InputFromSubgraphPins;
	TSet<TObjectPtr<UPCGPin>> OutputToSubgraphPins;
	TSet<TObjectPtr<UPCGPin>> AllSubgraphPins;

	// Also keep track of all the edges in the subgraph. Use a set to be able to add the
	// same edge twice without duplicates.
	TSet<TObjectPtr<UPCGEdge>> PCGGraphEdges;

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			check(PCGNode);

			// For each pin of the subgraph, gather all its edges in a set and store the other pin of
			// the edge as an input from or output to the subgraph. It will be useful for tracking pins
			// that are outside the subgraph.
			for (UPCGPin* InputPin : PCGNode->GetInputPins())
			{
				check(InputPin);

				for (UPCGEdge* Edge : InputPin->Edges)
				{
					check(Edge);
					PCGGraphEdges.Add(Edge);
					OutputToSubgraphPins.Add(Edge->InputPin);
				}

				AllSubgraphPins.Add(InputPin);
			}

			for (UPCGPin* OutputPin : PCGNode->GetOutputPins())
			{
				check(OutputPin);

				for (UPCGEdge* Edge : OutputPin->Edges)
				{
					check(Edge);
					PCGGraphEdges.Add(Edge);
					InputFromSubgraphPins.Add(Edge->OutputPin);
				}

				AllSubgraphPins.Add(OutputPin);
			}

			PCGEditorGraphNodes.Add(PCGEditorGraphNode);

			// And do all the computation to get the min, max and mean position.
			AveragePosition.X += PCGEditorGraphNode->NodePosX;
			AveragePosition.Y += PCGEditorGraphNode->NodePosY;
			MinX = FMath::Min(MinX, PCGEditorGraphNode->NodePosX);
			MaxX = FMath::Max(MaxX, PCGEditorGraphNode->NodePosX);
		}
		else if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
		{
			ExtraGraphNodes.Add(GraphNode);
		}
	}

	// If we have at most 1 node to collapse, just exit
	if (PCGEditorGraphNodes.Num() <= 1)
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("There were less than 2 PCG nodes selected, abort"));
		return;
	}

	// Compute the average position
	AveragePosition /= PCGEditorGraphNodes.Num();

	// 2. Create a new subgraph, by creating a new PCGGraph asset.
	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TObjectPtr<UPCGGraphFactory> Factory = NewObject<UPCGGraphFactory>();

	FString NewPackageName;
	FString NewAssetName;
	PCGEditorUtils::GetParentPackagePathAndUniqueName(PCGGraph, LOCTEXT("NewPCGSubgraphAsset", "NewPCGSubgraph").ToString(), NewPackageName, NewAssetName);

	TObjectPtr<UPCGGraph> NewPCGGraph = Cast<UPCGGraph>(AssetTools.CreateAssetWithDialog(NewAssetName, NewPackageName, PCGGraph->GetClass(), Factory, "PCGEditor_CollapseInSubgraph"));

	if (NewPCGGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Subgraph asset creation was aborted or failed, abort."));
		return;
	}

	// Do some clean-up on input/output nodes
	constexpr int32 Padding = 200;
	NewPCGGraph->GetInputNode()->PositionX = MinX - Padding;
	NewPCGGraph->GetInputNode()->PositionY = AveragePosition.Y;
	NewPCGGraph->GetOutputNode()->PositionX = MaxX + Padding;
	NewPCGGraph->GetOutputNode()->PositionY = AveragePosition.Y;

	// 3. Define some lambdas to create/adapt pins on the new subgraph.

	// Gather the pins that are outside the subgraph
	InputFromSubgraphPins = InputFromSubgraphPins.Difference(AllSubgraphPins);
	OutputToSubgraphPins = OutputToSubgraphPins.Difference(AllSubgraphPins);

	// Function that check if the pin is part of the advanced pins of the input/output node of the PCGGraph.
	// If so, enable advanced pins on the subgraph.
	auto IsAdvancedAndSet = [&PCGGraph, &NewPCGGraph](UPCGPin* Pin, bool bIsInput)
	{
		TObjectPtr<UPCGNode> InputOutputNode = bIsInput ? PCGGraph->GetInputNode() : PCGGraph->GetOutputNode();
		TObjectPtr<UPCGNode> NewInputOutputNode = bIsInput ? NewPCGGraph->GetInputNode() : NewPCGGraph->GetOutputNode();

		TObjectPtr<UPCGGraphInputOutputSettings> InputOutputSettings = CastChecked<UPCGGraphInputOutputSettings>(InputOutputNode->DefaultSettings);
		if (InputOutputSettings->IsPinAdvanced(Pin))
		{
			TObjectPtr<UPCGGraphInputOutputSettings> NewInputOutputSettings = CastChecked<UPCGGraphInputOutputSettings>(NewInputOutputNode->DefaultSettings);
			NewInputOutputSettings->SetShowAdvancedPins(true);
			NewInputOutputNode->UpdateAfterSettingsChangeDuringCreation();

			return true;
		}

		return false;
	};

	// Function that will create a new pin in the input/output node of the subgraph, with a unique name that will
	// be the related to the pin passed as argument. It will be formatted like this:
	// "{NodeName} {PinName} {OptionalIndex}"
	TMap<FString, int> NameCollisionMapping;
	auto CreateNewCustomPin = [&NewPCGGraph, &NameCollisionMapping](UPCGPin* Pin, bool bIsInput)
	{
		TObjectPtr<UPCGNode> NewInputOutputNode = bIsInput ? NewPCGGraph->GetInputNode() : NewPCGGraph->GetOutputNode();
		TObjectPtr<UPCGGraphInputOutputSettings> NewInputOutputSettings = CastChecked<UPCGGraphInputOutputSettings>(NewInputOutputNode->DefaultSettings);
		FString NewName = Pin->Node->GetNodeTitle().ToString() + " " + Pin->Properties.Label.ToString();
		if (NameCollisionMapping.Contains(NewName))
		{
			NewName += " " + FString::FormatAsNumber(++NameCollisionMapping[NewName]);
		}
		else
		{
			NameCollisionMapping.Emplace(NewName, 1);
		}

		FPCGPinProperties NewProperties = Pin->Properties;
		NewProperties.Label = FName(NewName);
		NewInputOutputSettings->AddCustomPin(NewProperties);
		NewInputOutputNode->UpdateAfterSettingsChangeDuringCreation();
		return NewProperties.Label;
	};

	// 4. Duplicate all the nodes, and keep a mapping between the old pins and new pins
	TMap<TObjectPtr<UPCGPin>, TObjectPtr<UPCGPin>> PinMapping;
	for (UPCGEditorGraphNodeBase* PCGEditorGraphNode : PCGEditorGraphNodes)
	{
		const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
		check(PCGNode);

		// Reconstruct a new node, same as PCGNode, but without any edges in the new graph 
		TObjectPtr<UPCGNode> NewNode = NewPCGGraph->ReconstructNewNode(PCGNode);
		
		for (int i = 0; i < PCGNode->GetInputPins().Num(); ++i)
		{
			PinMapping.Emplace(PCGNode->GetInputPins()[i], NewNode->GetInputPins()[i]);
		}
		for (int i = 0; i < PCGNode->GetOutputPins().Num(); ++i)
		{
			PinMapping.Emplace(PCGNode->GetOutputPins()[i], NewNode->GetOutputPins()[i]);
		}
	}

	// Also duplicate the extra nodes and assign them to the new graph
	TArray<TObjectPtr<const UObject>> NewExtraGraphNodes;
	for (const UEdGraphNode* ExtraNode : ExtraGraphNodes)
	{
		NewExtraGraphNodes.Add(DuplicateObject(ExtraNode, NewPCGGraph));
	}

	NewPCGGraph->SetExtraEditorNodes(NewExtraGraphNodes);

	// 5. Iterate over all the edges and create edges "placeholders"
	// Most of them will already be complete, but for those that needs to be connected to the new
	// subgraph nodes, the pins don't exist yet. Therefore we identify them with their pin labels.
	//
	// The logic behind the new pins is this:
	// -> If the pin is connected to the simple pin of the input/output node, let it like this
	// -> If the pin is part of the original input/output advanced pins, we will trigger the advanced pins flag on the input/output node in the subgraph
	// -> Otherwise, we add a new custom pin, with the name of the node, the name of the pin and a number if there is name collision
	struct EdgePlaceholder
	{
		TObjectPtr<UPCGPin> InputPin = nullptr;
		TObjectPtr<UPCGPin> OutputPin = nullptr;
		FName InputPinLabel;
		FName OutputPinLabel;
	};

	TArray<EdgePlaceholder> EdgePlaceholders;
	for (UPCGEdge* Edge : PCGGraphEdges)
	{
		const TObjectPtr<UPCGPin>* InPin = PinMapping.Find(Edge->InputPin);
		const TObjectPtr<UPCGPin>* OutPin = PinMapping.Find(Edge->OutputPin);

		check(InPin || OutPin)

		if (InPin == nullptr)
		{
			// The edge comes from outside the graph.
			// If it is from the input node, we have a special behavior
			EdgePlaceholder OutsideSubgraphEdge;
			EdgePlaceholder InsideSubgraphEdge;
			bool bProcessed = false;

			OutsideSubgraphEdge.InputPin = Edge->InputPin;
			InsideSubgraphEdge.OutputPin = *OutPin;

			if (Edge->InputPin->Node == PCGGraph->GetInputNode())
			{
				if (Edge->InputPin == PCGGraph->GetInputNode()->GetOutputPins()[0] || IsAdvancedAndSet(Edge->InputPin, true))
				{
					OutsideSubgraphEdge.OutputPinLabel = Edge->InputPin->Properties.Label;
					InsideSubgraphEdge.InputPin = NewPCGGraph->GetInputNode()->GetOutputPin(Edge->InputPin->Properties.Label);
					bProcessed = true;
				}
			}

			if (!bProcessed)
			{
				FName NewPinName = CreateNewCustomPin(Edge->OutputPin, true);
				OutsideSubgraphEdge.OutputPinLabel = NewPinName;
				InsideSubgraphEdge.InputPin = NewPCGGraph->GetInputNode()->GetOutputPin(NewPinName);
			}

			EdgePlaceholders.Add(OutsideSubgraphEdge);
			EdgePlaceholders.Add(InsideSubgraphEdge);
		}
		else if (OutPin == nullptr)
		{
			// The edge comes from outside the graph.
			// If it is from the output node, we have a special behavior
			EdgePlaceholder OutsideSubgraphEdge;
			EdgePlaceholder InsideSubgraphEdge;
			bool bProcessed = false;

			OutsideSubgraphEdge.OutputPin = Edge->OutputPin;
			InsideSubgraphEdge.InputPin = *InPin;

			if (Edge->OutputPin->Node == PCGGraph->GetOutputNode())
			{
				if (Edge->OutputPin == PCGGraph->GetOutputNode()->GetInputPins()[0] || IsAdvancedAndSet(Edge->OutputPin, false))
				{
					OutsideSubgraphEdge.InputPinLabel = Edge->OutputPin->Properties.Label;
					InsideSubgraphEdge.OutputPin = NewPCGGraph->GetOutputNode()->GetInputPin(Edge->OutputPin->Properties.Label);
					bProcessed = true;
				}
			}

			if (!bProcessed)
			{
				FName NewPinName = CreateNewCustomPin(Edge->InputPin, false);
				OutsideSubgraphEdge.InputPinLabel = NewPinName;
				InsideSubgraphEdge.OutputPin = NewPCGGraph->GetOutputNode()->GetInputPin(NewPinName);
			}

			EdgePlaceholders.Add(OutsideSubgraphEdge);
			EdgePlaceholders.Add(InsideSubgraphEdge);
		}
		else
		{
			// Both nodes are inside
			EdgePlaceholders.Add(EdgePlaceholder{*InPin, *OutPin});
		}
	}

	// 6. Create subgraph and delete old nodes
	// Done within a transaction to be undoable (subgraph will stay though)
	TObjectPtr<UPCGEditorGraphNodeBase> SubgraphEditorNode;
	TObjectPtr<UPCGNode> SubgraphNode;
	{
		const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorCollapseInSubgraphMessage", "PCG Editor: Collapse into Subgraph"), nullptr);

		DeleteSelectedNodes();

		FPCGEditorGraphSchemaAction_NewSubgraphElement SubgraphAddAction;
		SubgraphAddAction.SubgraphObjectPath = NewPCGGraph->GetPathName();
		SubgraphEditorNode = Cast<UPCGEditorGraphNodeBase>(SubgraphAddAction.PerformAction(PCGEditorGraph, nullptr, AveragePosition, true));
		SubgraphNode = SubgraphEditorNode->GetPCGNode();
	}

	// 7. Connect all the edges
	for (EdgePlaceholder& Edge : EdgePlaceholders)
	{
		if (Edge.InputPin == nullptr)
		{
			SubgraphNode->GetOutputPin(Edge.InputPinLabel)->AddEdgeTo(Edge.OutputPin);
		}
		else if (Edge.OutputPin == nullptr)
		{
			Edge.InputPin->AddEdgeTo(SubgraphNode->GetInputPin(Edge.OutputPinLabel));
		}
		else
		{
			Edge.InputPin->AddEdgeTo(Edge.OutputPin);
		}
	}

	// 8. Finalize the operation
	SubgraphEditorNode->ReconstructNode();

	// Save the new asset
	UEditorAssetLibrary::SaveLoadedAsset(NewPCGGraph);

	// And notify everyone
	GraphEditorWidget->NotifyGraphChanged();
	PCGGraph->NotifyGraphChanged(EPCGChangeType::Structural);
	NewPCGGraph->NotifyGraphChanged(EPCGChangeType::Structural);
}

void FPCGEditor::OnStartInspectNode()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	UEdGraphNode* GraphNode = GraphEditorWidget->GetSingleSelectedNode();
	if (!GraphNode)
	{
		return;
	}

	UPCGEditorGraphNodeBase* PCGGraphNodeBase = Cast<UPCGEditorGraphNodeBase>(GraphNode);
	if (!PCGGraphNodeBase)
	{
		return;
	}
	
	if (PCGGraphNodeBase == PCGGraphNodeBeingInspected)
	{
		return;
	}
	
	if (PCGGraphNodeBeingInspected)
	{
		PCGGraphNodeBeingInspected->SetInspected(false);
	}	

	PCGGraphNodeBeingInspected = PCGGraphNodeBase;
	PCGGraphNodeBeingInspected->SetInspected(true);
	SetPCGNodeBeingInspected(PCGGraphNodeBase->GetPCGNode());
}

void FPCGEditor::OnStopInspectNode()
{
	if (PCGGraphNodeBeingInspected)
	{
		PCGGraphNodeBeingInspected->SetInspected(false);
		PCGGraphNodeBeingInspected = nullptr;
		SetPCGNodeBeingInspected(nullptr);
	}
}

bool FPCGEditor::IsExecutionModeActive(EPCGSettingsExecutionMode InExecutionMode) const
{
	if (GraphEditorWidget.IsValid())
	{
		for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<const UPCGEditorGraphNodeBase>(Object);
			if (!PCGEditorGraphNode)
			{
				continue;
			}

			const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			if (!PCGNode)
			{
				continue;
			}

			const UPCGSettings* PCGSettings = PCGNode->DefaultSettings;
			if (!PCGSettings)
			{
				continue;
			}

			if (PCGSettings->ExecutionMode == InExecutionMode)
			{
				return true;
			}
		}
	}

	return false;
}

void FPCGEditor::OnSetExecutionMode(EPCGSettingsExecutionMode InExecutionMode)
{
	// todo should perhaps make a utility function to get all selected nodes.
	if (GraphEditorWidget.IsValid())
	{
		const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorExecutionModeTransactionMessage", "PCG Editor: ExecutionMode"), nullptr);
		
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			if (!PCGEditorGraphNode)
			{
				continue;
			}

			UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			if (!PCGNode)
			{
				continue;
			}

			UPCGSettings* PCGSettings = PCGNode->DefaultSettings;
			if (!PCGSettings)
			{
				continue;
			}

			if (PCGSettings->ExecutionMode != InExecutionMode)
			{
				PCGSettings->Modify();
				PCGSettings->ExecutionMode = InExecutionMode;
				PCGSettings->OnSettingsChangedDelegate.Broadcast(PCGSettings, EPCGChangeType::Settings);
			}
		}
	}
}

void FPCGEditor::SelectAllNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->SelectAllNodes();
	}
}

bool FPCGEditor::CanSelectAllNodes() const
{
	return GraphEditorWidget.IsValid();
}

void FPCGEditor::DeleteSelectedNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
		check(PCGEditorGraph && PCGGraph);

		bool bChanged = false;
		{
			const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDeleteTransactionMessage", "PCG Editor: Delete"), nullptr);
			PCGEditorGraph->Modify();

			for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
			{
				if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
				{
					if (PCGEditorGraphNode->CanUserDeleteNode())
					{
						UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
						check(PCGNode);

						PCGGraph->RemoveNode(PCGNode);
						PCGEditorGraphNode->DestroyNode();
						bChanged = true;
					}
				}
				else if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
				{
					if (GraphNode->CanUserDeleteNode())
					{
						GraphNode->DestroyNode();
						bChanged = true;
					}
				}
			}

			PCGEditorGraph->Modify();
		}

		if (bChanged)
		{
			GraphEditorWidget->ClearSelectionSet();
			GraphEditorWidget->NotifyGraphChanged();
			PCGGraphBeingEdited->NotifyGraphChanged(EPCGChangeType::Structural);
		}
	}
}

bool FPCGEditor::CanDeleteSelectedNodes() const
{
	if (GraphEditorWidget.IsValid())
	{
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UEdGraphNode* GraphNode = CastChecked<UEdGraphNode>(Object);

			if (GraphNode->CanUserDeleteNode())
			{
				return true;
			}
		}
	}

	return false;
}

void FPCGEditor::CopySelectedNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

		//TODO: evaluate creating a clipboard object instead of ownership hack
		for (UObject* SelectedNode : SelectedNodes)
		{
			UEdGraphNode* GraphNode = CastChecked<UEdGraphNode>(SelectedNode);
			GraphNode->PrepareForCopying();
		}

		FString ExportedText;
		FEdGraphUtilities::ExportNodesToText(SelectedNodes, ExportedText);
		FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

		for (UObject* SelectedNode : SelectedNodes)
		{
			if (UPCGEditorGraphNodeBase* PCGGraphNode = Cast<UPCGEditorGraphNodeBase>(SelectedNode))
			{
				PCGGraphNode->PostCopy();
			}
		}
	}
}

bool FPCGEditor::CanCopySelectedNodes() const
{
	if (GraphEditorWidget.IsValid())
	{
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			if (UEdGraphNode* GraphNode = CastChecked<UEdGraphNode>(Object))
			{
				if (GraphNode->CanDuplicateNode())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPCGEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	DeleteSelectedNodes();
}

bool FPCGEditor::CanCutSelectedNodes() const
{
	return CanCopySelectedNodes() && CanDeleteSelectedNodes();
}

void FPCGEditor::PasteNodes()
{
	if (GraphEditorWidget.IsValid())
	{
		PasteNodesHere(GraphEditorWidget->GetPasteLocation());
	}
}

void FPCGEditor::PasteNodesHere(const FVector2D& Location)
{
	if (!GraphEditorWidget.IsValid() || !PCGEditorGraph)
	{
		return;
	}

	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorPasteTransactionMessage", "PCG Editor: Paste"), nullptr);
	PCGEditorGraph->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditorWidget->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(PCGEditorGraph, TextToImport, /*out*/ PastedNodes);

	//Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2D AvgNodePosition(0.0f, 0.0f);

	// Number of nodes used to calculate AvgNodePosition
	int32 AvgCount = 0;

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		if (PastedNode)
		{
			AvgNodePosition.X += PastedNode->NodePosX;
			AvgNodePosition.Y += PastedNode->NodePosY;
			++AvgCount;
		}
	}

	if (AvgCount > 0)
	{
		float InvNumNodes = 1.0f / float(AvgCount);
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		GraphEditorWidget->SetNodeSelection(PastedNode, true);

		PastedNode->NodePosX = (PastedNode->NodePosX - AvgNodePosition.X) + Location.X;
		PastedNode->NodePosY = (PastedNode->NodePosY - AvgNodePosition.Y) + Location.Y;

		PastedNode->SnapToGrid(SNodePanel::GetSnapGridSize());

		PastedNode->CreateNewGuid();

		if (UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode))
		{
			if (UPCGNode* PastedPCGNode = PastedPCGGraphNode->GetPCGNode())
			{
				PCGGraphBeingEdited->AddNode(PastedPCGNode);

				PastedPCGGraphNode->PostPaste();
			}
		}
	}

	GraphEditorWidget->NotifyGraphChanged();
}

bool FPCGEditor::CanPasteNodes() const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(PCGEditorGraph, ClipboardContent);
}

void FPCGEditor::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FPCGEditor::CanDuplicateNodes() const
{
	return CanCopySelectedNodes();
}

void FPCGEditor::OnAlignTop()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignTop();
	}
}

void FPCGEditor::OnAlignMiddle()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignMiddle();
	}
}

void FPCGEditor::OnAlignBottom()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignBottom();
	}
}

void FPCGEditor::OnAlignLeft()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignLeft();
	}
}

void FPCGEditor::OnAlignCenter()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignCenter();
	}
}

void FPCGEditor::OnAlignRight()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnAlignRight();
	}
}

void FPCGEditor::OnStraightenConnections()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnStraightenConnections();
	}
}

void FPCGEditor::OnDistributeNodesH()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnDistributeNodesH();
	}
}

void FPCGEditor::OnDistributeNodesV()
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->OnDistributeNodesV();
	}
}

void FPCGEditor::OnCreateComment()
{
	if (PCGEditorGraph)
	{
		FPCGEditorGraphSchemaAction_NewComment CommentAction;

		TSharedPtr<SGraphEditor> GraphEditorPtr = SGraphEditor::FindGraphEditorForGraph(PCGEditorGraph);
		FVector2D Location;
		if (GraphEditorPtr)
		{
			Location = GraphEditorPtr->GetPasteLocation();
		}

		CommentAction.PerformAction(PCGEditorGraph, nullptr, Location);
	}
}

TSharedRef<SGraphEditor> FPCGEditor::CreateGraphEditorWidget()
{
	GraphEditorCommands = MakeShareable(new FUICommandList);

	// Editing commands
	GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateSP(this, &FPCGEditor::SelectAllNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanSelectAllNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FPCGEditor::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanDeleteSelectedNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &FPCGEditor::CopySelectedNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCopySelectedNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &FPCGEditor::CutSelectedNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCutSelectedNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &FPCGEditor::PasteNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanPasteNodes));

	GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &FPCGEditor::DuplicateNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanDuplicateNodes));

	// Alignment Commands
	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesTop,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignTop)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesMiddle,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignMiddle)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesBottom,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignBottom)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesLeft,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignLeft)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesCenter,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignCenter)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().AlignNodesRight,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAlignRight)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().StraightenConnections,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnStraightenConnections)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCreateComment)
	);

	// Distribution Commands
	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DistributeNodesHorizontally,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDistributeNodesH)
	);

	GraphEditorCommands->MapAction(FGraphEditorCommands::Get().DistributeNodesVertically,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDistributeNodesV)
	);

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("PCGGraphEditorCornerText", "PCG Graph");

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FPCGEditor::OnSelectedNodesChanged);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FPCGEditor::OnNodeTitleCommitted);
	InEvents.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &FPCGEditor::OnNodeDoubleClicked);

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(PCGEditorGraph)
		.GraphEvents(InEvents)
		.ShowGraphStateOverlay(false);
}

void FPCGEditor::ReplicateExtraNodes() const
{
	if (PCGEditorGraph)
	{
		if (UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph())
		{
			TArray<TObjectPtr<const UObject>> ExtraNodes;
			for (const UEdGraphNode* GraphNode : PCGEditorGraph->Nodes)
			{
				check(GraphNode);
				if (!GraphNode->IsA<UPCGEditorGraphNodeBase>())
				{
					ExtraNodes.Add(GraphNode);
				}

			}

			PCGGraph->SetExtraEditorNodes(ExtraNodes);
		}
	}
}

void FPCGEditor::SaveAsset_Execute()
{
	// Extra nodes are replicated on asset save, to be saved in the underlying PCGGraph
	ReplicateExtraNodes();

	FAssetEditorToolkit::SaveAsset_Execute();
}

void FPCGEditor::OnClose()
{
	// Extra nodes are replicated on editor close, to be saved in the underlying PCGGraph
	ReplicateExtraNodes();

	FAssetEditorToolkit::OnClose();

	if (PCGGraphBeingEdited && PCGGraphBeingEdited->NotificationsForEditorArePausedByUser())
	{
		PCGGraphBeingEdited->ToggleUserPausedNotificationsForEditor();
	}
}

void FPCGEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FAssetEditorToolkit::InitToolMenuContext(MenuContext);

	UPCGEditorMenuContext* Context = NewObject<UPCGEditorMenuContext>();
	Context->PCGEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

TSharedRef<SPCGEditorGraphNodePalette> FPCGEditor::CreatePaletteWidget()
{
	return SNew(SPCGEditorGraphNodePalette);
}

TSharedRef<SPCGEditorGraphFind> FPCGEditor::CreateFindWidget()
{
	return SNew(SPCGEditorGraphFind, SharedThis(this));
}

TSharedRef<SPCGEditorGraphAttributeListView> FPCGEditor::CreateAttributesWidget()
{
	return SNew(SPCGEditorGraphAttributeListView, SharedThis(this));
}

TSharedRef<SPCGEditorGraphDeterminismListView> FPCGEditor::CreateDeterminismWidget()
{
	return SNew(SPCGEditorGraphDeterminismListView, SharedThis(this));
}

TSharedRef<SPCGEditorGraphProfilingView> FPCGEditor::CreateProfilingWidget()
{
	return SNew(SPCGEditorGraphProfilingView, SharedThis(this));
}

void FPCGEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	
	for (UObject* Object : NewSelection)
	{
		if (UPCGEditorGraphNodeBase* PCGGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			if (UPCGNode* PCGNode = PCGGraphNode->GetPCGNode())
			{
				SelectedObjects.Add(PCGNode->DefaultSettings);
			}
		}
		else if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
		{
			SelectedObjects.Add(GraphNode);
		}
	}

	PropertyDetailsWidget->SetObjects(SelectedObjects, /*bForceRefresh=*/true);

	GetTabManager()->TryInvokeTab(FPCGEditor_private::PropertyDetailsID);
}

void FPCGEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorRenameNode", "PCG Editor: Rename Node"), nullptr);
		NodeBeingChanged->Modify();
		NodeBeingChanged->OnRenameNode(NewText.ToString());
	}
}

void FPCGEditor::OnNodeDoubleClicked(UEdGraphNode* Node)
{
	if (Node != nullptr)
	{
		if (UObject* Object = Node->GetJumpTargetForDoubleClick())
		{
			if (UPCGSettings* PCGSettings = Cast<UPCGSettings>(Object))
			{
				JumpToDefinition(PCGSettings->GetClass());
			}
			else
			{
				GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Object);
			}
		}
	}
}

void FPCGEditor::JumpToDefinition(const UClass* Class) const
{
	if (ensure(GUnrealEd) && GUnrealEd->GetUnrealEdOptions()->IsCPPAllowed())
	{
		const bool bEnableNavigateToNativeNodes = GetDefault<UPCGEditorSettings>()->bEnableNavigateToNativeNodes;
		if (bEnableNavigateToNativeNodes)
		{
			FSourceCodeNavigation::NavigateToClass(Class);
		}
		else
		{
			// Inform user that the node is native, give them opportunity to enable navigation to native nodes:
			FNotificationInfo Info(LOCTEXT("NavigateToNativeDisabled", "Navigation to Native (c++) PCG Nodes Disabled"));
			Info.ExpireDuration = 10.0f;
			Info.CheckBoxState = bEnableNavigateToNativeNodes ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

			Info.CheckBoxStateChanged = FOnCheckStateChanged::CreateStatic(
				[](ECheckBoxState NewState)
				{
					const FScopedTransaction Transaction(LOCTEXT("ChangeEnableNavigateToNativeNodes", "Change Enable Navigate to Native Nodes Setting"));

					UPCGEditorSettings* MutableEditorSetings = GetMutableDefault<UPCGEditorSettings>();
					MutableEditorSetings->Modify();
					MutableEditorSetings->bEnableNavigateToNativeNodes = (NewState == ECheckBoxState::Checked) ? true : false;
					MutableEditorSetings->SaveConfig();
				}
			);
			Info.CheckBoxText = LOCTEXT("EnableNavigationToNative", "Enable Navigate to Native Nodes?");

			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_GraphEditor(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGGraphTitle", "Graph"))
		.TabColorScale(GetTabColorScale())
		[
			GraphEditorWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_PropertyDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGDetailsTitle", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			PropertyDetailsWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Palette(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGPaletteTitle", "Palette"))
		.TabColorScale(GetTabColorScale())
		[
			PaletteWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Attributes(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGAttributesTitle", "Attributes"))
		.TabColorScale(GetTabColorScale())
		[
			AttributesWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Find(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGFindTitle", "Find"))
		.TabColorScale(GetTabColorScale())
		[
			FindWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Determinism(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGDeterminismTitle", "Determinism"))
		.TabColorScale(GetTabColorScale())
		[
			DeterminismWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Profiling(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGProfilingTitle", "Profiling"))
		.TabColorScale(GetTabColorScale())
		[
			ProfilingWidget.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
