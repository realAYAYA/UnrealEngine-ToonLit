// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditor.h"

#include "DetailsViewArgs.h"
#include "PCGComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "PCGEdge.h"
#include "IAssetTools.h"
#include "PCGGraph.h"
#include "PCGEditorModule.h"
#include "PCGGraphFactory.h"
#include "PCGInputOutputSettings.h"
#include "PCGPin.h"
#include "PCGSubsystem.h"
#include "Helpers/PCGSubgraphHelpers.h"
#include "Rendering/SlateRenderer.h"
#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDeterminismTestBlueprintBase.h"
#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGEditorCommands.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNodeInput.h"
#include "PCGEditorGraphNodeOutput.h"
#include "PCGEditorGraphSchema.h"
#include "PCGEditorGraphSchemaActions.h"
#include "PCGEditorMenuContext.h"
#include "PCGEditorSettings.h"
#include "PCGEditorUtils.h"
#include "SPCGEditorGraphAttributeListView.h"
#include "SPCGEditorGraphDebugObjectTree.h"
#include "SPCGEditorGraphDebugObjectWidget.h"
#include "SPCGEditorGraphDeterminism.h"
#include "SPCGEditorGraphFind.h"
#include "SPCGEditorGraphLogView.h"
#include "SPCGEditorGraphNodePalette.h"
#include "SPCGEditorGraphProfilingView.h"

#include "AssetToolsModule.h"
#include "EdGraphUtilities.h"
#include "EditorAssetLibrary.h"
#include "GraphEditorActions.h"
#include "IDetailsView.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"
#include "SNodePanel.h"
#include "ScopedTransaction.h"
#include "SourceCodeNavigation.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "UnrealEdGlobals.h"
#include "Algo/AnyOf.h"
#include "Editor/UnrealEdEngine.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ITransaction.h"
#include "Misc/TransactionObjectEvent.h"
#include "Preferences/UnrealEdOptions.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "PCGGraphEditor"

namespace FPCGEditor_private
{
	const FName GraphEditorID = FName(TEXT("GraphEditor"));
	const FName PropertyDetailsID = FName(TEXT("PropertyDetails"));
	const FName PaletteID = FName(TEXT("Palette"));
	const FName DebugObjectID = FName(TEXT("DebugObject"));
	const FName AttributesID = FName(TEXT("Attributes"));
	const FName FindID = FName(TEXT("Find"));
	const FName DeterminismID = FName(TEXT("Determinism"));
	const FName ProfilingID = FName(TEXT("Profiling"));
	const FName LogID = FName(TEXT("Log"));
}

void FPCGEditor::Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph)
{
	PCGGraphBeingEdited = InPCGGraph;

	if (PCGGraphBeingEdited)
	{
		PCGGraphBeingEdited->OnGraphGridSizesChangedDelegate.AddRaw(this, &FPCGEditor::OnGraphGridSizesChanged);
		PCGGraphBeingEdited->OnGraphDynamicallyExecutedDelegate.AddRaw(this, &FPCGEditor::OnGraphDynamicallyExecuted);
	}

	PCGEditorGraph = NewObject<UPCGEditorGraph>(PCGGraphBeingEdited, UPCGEditorGraph::StaticClass(), NAME_None, RF_Transactional | RF_Transient);
	PCGEditorGraph->Schema = UPCGEditorGraphSchema::StaticClass();
	PCGEditorGraph->InitFromNodeGraph(InPCGGraph);
	PCGEditorGraph->SetEditor(SharedThis(this));

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;
	PropertyDetailsWidget = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);

	IDetailsView* RawPropertyDetailsPtr = PropertyDetailsWidget.Get();
	PropertyDetailsWidget->SetIsPropertyReadOnlyDelegate(FIsPropertyReadOnly::CreateRaw(this, &FPCGEditor::IsReadOnlyProperty, RawPropertyDetailsPtr));
	PropertyDetailsWidget->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateRaw(this, &FPCGEditor::IsVisibleProperty, RawPropertyDetailsPtr));

	GraphEditorWidget = CreateGraphEditorWidget();
	PaletteWidget = CreatePaletteWidget();
	DebugObjectWidget = CreateDebugObjectWidget();
	DebugObjectTreeWidget = CreateDebugObjectTreeWidget();
	FindWidget = CreateFindWidget();
	AttributesWidget = CreateAttributesWidget();
	DeterminismWidget = CreateDeterminismWidget();
	ProfilingWidget = CreateProfilingWidget();
	LogWidget = CreateLogWidget();

	BindCommands();
	RegisterToolbar();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_PCGGraphEditor_Layout_v0.6")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.72)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.10f)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::PaletteID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.70)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::GraphEditorID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.20f)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::PropertyDetailsID, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.28)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::DebugObjectID, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.8)
					->SetHideTabWell(true)
					->AddTab(FPCGEditor_private::AttributesID, ETabState::OpenedTab)
					->AddTab(FPCGEditor_private::DeterminismID, ETabState::ClosedTab)
					->AddTab(FPCGEditor_private::FindID, ETabState::ClosedTab)
				)
			)
		);

	const FName PCGGraphEditorAppName = FName(TEXT("PCGEditorApp"));

	InitAssetEditor(InMode, InToolkitHost, PCGGraphEditorAppName, StandaloneDefaultLayout, /*bCreateDefaultStandaloneMenu=*/ true, /*bCreateDefaultToolbar=*/ true, InPCGGraph);

	// Hook to map change / delete actor to refresh debug object selection list, to help prevent it going stale.
	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditor.OnMapChanged().AddRaw(this, &FPCGEditor::OnMapChanged);
	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().AddRaw(this, &FPCGEditor::OnLevelActorDeleted);
	}
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph()
{
	return PCGEditorGraph;
}

void FPCGEditor::SetComponentAndStackBeingInspected(UPCGComponent* InPCGComponent, const FPCGStack& InPCGStack)
{
	UPCGComponent* OldComponent = PCGComponentBeingInspected.Get();
	UPCGComponent* NewComponent = InPCGComponent;

	if (OldComponent != NewComponent)
	{
		if (OldComponent)
		{
			OldComponent->DisableInspection();
			if (PCGGraphBeingEdited)
			{
				PCGGraphBeingEdited->DisableInspection();
			}
		}

		const bool bNewComponentStartedInspecting = NewComponent && !NewComponent->IsInspecting();

		PCGComponentBeingInspected = NewComponent;
		OnInspectedComponentChangedDelegate.Broadcast(NewComponent);

		if (NewComponent)
		{
			PCGComponentBeingInspected->EnableInspection();
			if (PCGGraphBeingEdited)
			{
				PCGGraphBeingEdited->EnableInspection();
			}
		}

		UpdateDebugAfterComponentSelection(OldComponent, NewComponent, bNewComponentStartedInspecting);

		check(PCGEditorGraph);
		for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			if (UPCGEditorGraphNodeBase* PCGNode = Cast<UPCGEditorGraphNodeBase>(Node))
			{
				// Update now that component has changed. Will fire OnNodeChanged if necessary.
				EPCGChangeType ChangeType = PCGNode->UpdateErrorsAndWarnings();
				ChangeType |= PCGNode->UpdateGridSizeVisualization(InPCGComponent);

				if (ChangeType != EPCGChangeType::None)
				{
					PCGNode->ReconstructNode();
				}
			}
		}
	}

	if (InPCGStack != StackBeingInspected)
	{
		StackBeingInspected = InPCGStack;
		OnInspectedStackChangedDelegate.Broadcast(StackBeingInspected);
	}
}

void FPCGEditor::UpdateDebugAfterComponentSelection(UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool bInNewComponentStartedInspecting)
{
	if (!ensure(PCGGraphBeingEdited))
	{
		return;
	}

	// If individual component debugging is disabled, just generate the new component if required.
	if (!PCGGraphBeingEdited->DebugFlagAppliesToIndividualComponents())
	{
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			InNewComponent->GenerateLocal(/*bForce=*/true);
		}

		return;
	}

	// Trigger necessary generation(s) for per-component debugging.
	if (!InOldComponent)
	{
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			// Transition from 'null' to 'any component not already inspecting' - generate to create debug/inspection info.
			// If we have null selected, all components are displaying debug. Go to Original component so that all refresh.
			InNewComponent->GetOriginalComponent()->GenerateLocal(/*bForce=*/true);
		}
	}
	else
	{
		const bool bDebugFlagSetOnAnyNode = Algo::AnyOf(PCGGraphBeingEdited->GetNodes(), [](const UPCGNode* InNode)
		{
			return InNode && InNode->GetSettings() && InNode->GetSettings()->bDebug;
		});

		// Regenerate to clear debug info if switching components, or if changing from a component to null.
		if (InNewComponent || bDebugFlagSetOnAnyNode)
		{
			// Use original component - debug can be displayed both by the local component and parent local components.
			InOldComponent->GetOriginalComponent()->GenerateLocal(/*bForce=*/true);
		}

		// Debug new component if it wasn't already
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			// Use original component - debug can be displayed both by the local component and parent local components.
			InNewComponent->GetOriginalComponent()->GenerateLocal(/*bForce=*/true);
		}
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
	const TSharedRef<FWorkspaceItem>& WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

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
	
	InTabManager->RegisterTabSpawner(FPCGEditor_private::DebugObjectID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_DebugObject))
		.SetDisplayName(LOCTEXT("DebugTab", "Debug Object Tree"))
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

	InTabManager->RegisterTabSpawner(FPCGEditor_private::LogID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Log))
		.SetDisplayName(LOCTEXT("LogCaptureTab", "Log Capture"))
		.SetGroup(WorkspaceMenuCategoryRef);
}

void FPCGEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::GraphEditorID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PaletteID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::DebugObjectID);
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
	if (InContext.Context == FPCGEditorCommon::ContextIdentifier)
	{
		return true;
	}

	// This is done to catch transaction blocks made outside PCG editor code were we need to trigger PostUndo for our context, i.e. UPCGEditorGraphSchema::TryCreateConnection
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectContext : TransactionObjectContexts)
	{
		const UObject* Object = TransactionObjectContext.Key;
		while (Object != nullptr)
		{
			if (Object == PCGGraphBeingEdited)
			{
				return true;
			}
			Object = Object->GetOuter();
		}
	}

	return false;
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
	UToolMenu* ToolBar;
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolBar = ToolMenus->ExtendMenu(MenuName);
	}
	else
	{
		ToolBar = ToolMenus->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);
	}

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

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().CancelExecution,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Toolbar.Stop")));

		Section.AddSeparator(NAME_None);
		Section.AddDynamicEntry("Debugging", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const UPCGEditorMenuContext* Context = InSection.FindContext<UPCGEditorMenuContext>();
			if (Context && Context->PCGEditor.IsValid())
			{
				InSection.AddEntry(FToolMenuEntry::InitWidget(
					"SelectedDebugObjectWidget",
					Context->PCGEditor.Pin()->DebugObjectWidget.ToSharedRef(),
					FText::GetEmpty()));
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
			 PCGEditorCommands.EditGraphSettings,
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
		PCGEditorCommands.CancelExecution,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCancelExecution_Clicked),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::IsCurrentlyGenerating));

	ToolkitCommands->MapAction(
		PCGEditorCommands.RunDeterminismGraphTest,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDeterminismGraphTest),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRunDeterminismGraphTest));

	ToolkitCommands->MapAction(
		PCGEditorCommands.EditGraphSettings,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnEditGraphSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FPCGEditor::IsEditGraphSettingsToggled));
	
	GraphEditorCommands->MapAction(
		PCGEditorCommands.CollapseNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnCollapseNodesInSubgraph),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanCollapseNodesInSubgraph));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ExportNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnExportNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanExportNodes));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ConvertToStandaloneNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnConvertToStandaloneNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanConvertToStandaloneNodes));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleInspect,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleInspected),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleInspected),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetInspectedCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.RunDeterminismNodeTest,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDeterminismNodeTest),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRunDeterminismNodeTest));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleEnabled,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleEnabled),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetEnabledCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleDebug,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleDebug),
		FCanExecuteAction(),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetDebugCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.DebugOnlySelected,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDebugOnlySelected));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.DisableDebugOnAllNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDisableDebugOnAllNodes));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.AddSourcePin,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAddDynamicInputPin),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanAddDynamicInputPin));
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
		FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		if (ModifierKeys.IsControlDown())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				Subsystem->FlushCache();
			}
		}

		PCGGraphBeingEdited->ForceNotificationForEditor();
	}
}

void FPCGEditor::OnCancelExecution_Clicked()
{
	UPCGSubsystem* Subsystem = GetSubsystem();
	if (PCGEditorGraph && Subsystem)
	{
		Subsystem->CancelGeneration(PCGEditorGraph->GetPCGGraph());
	}
}

bool FPCGEditor::IsCurrentlyGenerating() const
{
	UPCGSubsystem* Subsystem = GetSubsystem();
	if (PCGGraphBeingEdited && Subsystem)
	{
		return Subsystem->IsGraphCurrentlyExecuting(PCGGraphBeingEdited);
	}

	return false;
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
				check(PCGNode && PCGNode->GetSettings());

				TSharedPtr<FDeterminismTestResult> NodeResult = MakeShared<FDeterminismTestResult>();
				NodeResult->TestResultTitle = FName(*PCGNode->GetNodeTitle().ToString());
				NodeResult->TestResultName = PCGNode->GetName();
				NodeResult->Seed = PCGNode->GetSettings()->GetSeed();

				if (PCGNode->GetSettings()->DeterminismSettings.bNativeTests)
				{
					// If the settings has a native test suite
					if (TFunction<bool()> NativeTestSuite = PCGDeterminismTests::FNativeTestRegistry::GetNativeTestFunction(PCGNode->GetSettings()))
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
				if (PCGNode->GetSettings()->DeterminismSettings.bUseBlueprintDeterminismTest)
				{
					TSubclassOf<UPCGDeterminismTestBlueprintBase> Blueprint = PCGNode->GetSettings()->DeterminismSettings.DeterminismTestBlueprint;
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
	return PCGEditorGraph && PCGComponentBeingInspected.IsValid();
}

void FPCGEditor::OnDeterminismGraphTest()
{
	check(GraphEditorWidget.IsValid());

	if (!DeterminismWidget.IsValid() || !DeterminismWidget->WidgetIsConstructed() || !PCGGraphBeingEdited || !PCGComponentBeingInspected.IsValid())
	{
		return;
	}

	if (PCGComponentBeingInspected->GetGraph() != PCGGraphBeingEdited)
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
	TestResult->Seed = PCGComponentBeingInspected->Seed;

	PCGDeterminismTests::RunDeterminismTest(PCGGraphBeingEdited, PCGComponentBeingInspected.Get(), *TestResult);

	DeterminismWidget->AddItem(TestResult);
	DeterminismWidget->AddDetailsColumn();
	DeterminismWidget->RefreshItems();

	// Give focus to the Determinism Output Tab
	if (TabManager.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::DeterminismID);
	}
}

void FPCGEditor::OnEditGraphSettings() const
{
	PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);
}

bool FPCGEditor::IsEditGraphSettingsToggled() const
{
	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyDetailsWidget->GetSelectedObjects();
	return SelectedObjects.Num() == 1 && SelectedObjects[0] == PCGGraphBeingEdited.Get();
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

void FPCGEditor::OnAddDynamicInputPin()
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	
	if (!ensure(SelectedNodes.Num() == 1))
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Attempting to add new input pin to multiple nodes."));
		return;
	}

	UPCGEditorGraphNodeBase* Node = CastChecked<UPCGEditorGraphNodeBase>(*SelectedNodes.CreateConstIterator());
	Node->OnUserAddDynamicInputPin();
}

bool FPCGEditor::CanAddDynamicInputPin() const
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		return false;
	}

	const UPCGEditorGraphNodeBase* Node = Cast<const UPCGEditorGraphNodeBase>(*SelectedNodes.CreateConstIterator());
	return (Node && Node->CanUserAddRemoveDynamicInputPins());
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

	// Gather all nodes that will be included in the subgraph, and the extra nodes
	TArray<UPCGNode*> NodesToCollapse;
	TArray<UObject*> ExtraNodesToCollapse;

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
			NodesToCollapse.Add(PCGNode);
		}
		else if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
		{
			ExtraNodesToCollapse.Add(GraphNode);
		}
	}

	// If we have at most 1 node to collapse, just exit
	if (NodesToCollapse.Num() <= 1)
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("There were less than 2 PCG nodes selected, abort"));
		return;
	}

	// Create a new subgraph, by creating a new PCGGraph asset.
	TObjectPtr<UPCGGraph> NewPCGGraph = nullptr;
	{
		FScopedTransaction Transaction(LOCTEXT("PCGCollapseInSubgraphMessage", "[PCG] Collapse into Subgraph"));

		IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		TObjectPtr<UPCGGraphFactory> Factory = NewObject<UPCGGraphFactory>();

		FString NewPackageName;
		FString NewAssetName;
		PCGEditorUtils::GetParentPackagePathAndUniqueName(PCGGraph, LOCTEXT("NewPCGSubgraphAsset", "NewPCGSubgraph").ToString(), NewPackageName, NewAssetName);

		NewPCGGraph = Cast<UPCGGraph>(AssetTools.CreateAssetWithDialog(NewAssetName, NewPackageName, PCGGraph->GetClass(), Factory, "PCGEditor_CollapseInSubgraph"));

		if (NewPCGGraph == nullptr)
		{
			UE_LOG(LogPCGEditor, Warning, TEXT("Subgraph asset creation was aborted or failed, abort."));
			Transaction.Cancel();
			return;
		}

		NewPCGGraph = FPCGSubgraphHelpers::CollapseIntoSubgraph(PCGGraph, NodesToCollapse, ExtraNodesToCollapse, NewPCGGraph);

		if (NewPCGGraph == nullptr)
		{
			UE_LOG(LogPCGEditor, Warning, TEXT("Subgraph collapse failed, abort."));
			return;
		}

		// Force a refresh
		PCGEditorGraph->ReconstructGraph();
	}

	// Save the new asset
	UEditorAssetLibrary::SaveLoadedAsset(NewPCGGraph);

	// Notify the widget
	GraphEditorWidget->NotifyGraphChanged();
}

bool FPCGEditor::CanExportNodes() const
{
	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (Object->IsA<UPCGEditorGraphNodeBase>())
		{
			return true;
		}
	}

	return false;
}

void FPCGEditor::OnExportNodes()
{
	if (!GraphEditorWidget.IsValid() || PCGEditorGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("GraphEditorWidget or PCGEditorGraph is null, aborting"));
		return;
	}

	if (PCGGraphBeingEdited == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("Editor has no graph loaded, aborting"));
		return;
	}

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		UPCGSettings* Settings = nullptr;

		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			check(PCGNode);
			Settings = PCGNode->GetSettings();
		}

		if (!Settings)
		{
			continue;
		}

		// Create new settings asset
		FString NewPackageName;
		FString NewAssetName;
		PCGEditorUtils::GetParentPackagePathAndUniqueName(PCGGraphBeingEdited, LOCTEXT("NewPCGSettingsAsset", "NewPCGSettings").ToString(), NewPackageName, NewAssetName);

		UObject* NewSettings = AssetTools.DuplicateAssetWithDialogAndTitle(NewAssetName, NewPackageName, Settings, NSLOCTEXT("PCGEditor_ExportNodes", "PCGEditor_ExportNodesTitle", "Export Settings As..."));

		if (NewSettings == nullptr)
		{
			UE_LOG(LogPCGEditor, Warning, TEXT("Settings asset creation was aborted or failed, abort."));
			return;
		}
	}
}

void FPCGEditor::OnConvertToStandaloneNodes()
{
	const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorConvertToStandaloneMessage", "PCG Editor: Converting instanced nodes to standalone"), nullptr);

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);
		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (UPCGEditorGraphNodeBase* Node = Cast<UPCGEditorGraphNodeBase>(Object))
		{
			if (UPCGNode* PCGNode = Node->GetPCGNode())
			{
				if (PCGNode->IsInstance())
				{
					PCGNode->Modify();

					UPCGSettings* SourceSettings = PCGNode->GetSettings();
					check(SourceSettings);

					UPCGSettings* SettingsCopy = DuplicateObject(SourceSettings, PCGNode);
					SettingsCopy->SetFlags(RF_Transactional);

					PCGNode->SetSettingsInterface(SettingsCopy);
				}
			}

			Node->ReconstructNode();
		}
	}

	// Notify the widget
	if (GraphEditorWidget)
	{
		GraphEditorWidget->NotifyGraphChanged();
	}
}

bool FPCGEditor::CanConvertToStandaloneNodes() const
{
	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		check(Object);

		// Exclude input and output nodes from the subgraph.
		if (Object->IsA<UPCGEditorGraphNodeInput>() || Object->IsA<UPCGEditorGraphNodeOutput>())
		{
			continue;
		}

		if (const UPCGEditorGraphNodeBase* Node = Cast<const UPCGEditorGraphNodeBase>(Object))
		{
			if (const UPCGNode* PCGNode = Node->GetPCGNode())
			{
				if (PCGNode->IsInstance())
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FPCGEditor::OnToggleInspected()
{
	if (!GraphEditorWidget.IsValid())
	{
		return;
	}

	if (PCGGraphNodeBeingInspected)
	{
		PCGGraphNodeBeingInspected->SetInspected(false);
	}
	
	UEdGraphNode* GraphNode = GraphEditorWidget->GetSingleSelectedNode();
	UPCGEditorGraphNodeBase* PCGGraphNodeBase = Cast<UPCGEditorGraphNodeBase>(GraphNode);
	if (PCGGraphNodeBase && PCGGraphNodeBase != PCGGraphNodeBeingInspected)
	{
		PCGGraphNodeBeingInspected = PCGGraphNodeBase;
		PCGGraphNodeBeingInspected->SetInspected(true);
	}
	else
	{
		PCGGraphNodeBeingInspected = nullptr;
	}
	
	OnInspectedNodeChangedDelegate.Broadcast(PCGGraphNodeBeingInspected);
	GetTabManager()->TryInvokeTab(FPCGEditor_private::AttributesID);
}

bool FPCGEditor::CanToggleInspected() const
{
	if (PCGGraphNodeBeingInspected)
	{
		return true;
	}

	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const UEdGraphNode* GraphNode = GraphEditorWidget->GetSingleSelectedNode();

	return GraphNode && GraphNode->IsA<UPCGEditorGraphNodeBase>();
}

ECheckBoxState FPCGEditor::GetInspectedCheckState() const
{
	if (GraphEditorWidget.IsValid())
	{
		const FGraphPanelSelectionSet& SelectedNodes = GraphEditorWidget->GetSelectedNodes();

		if (SelectedNodes.IsEmpty())
		{
			return ECheckBoxState::Unchecked;
		}
	
		bool bAllEnabled = true;
		bool bAnyEnabled = false;
		
		for (UObject* Object : SelectedNodes)
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			if (!PCGEditorGraphNode)
			{
				continue;
			}
			
			bAllEnabled &= PCGEditorGraphNode->GetInspected();
			bAnyEnabled |= PCGEditorGraphNode->GetInspected();
		}

		if (bAllEnabled)
		{
			return ECheckBoxState::Checked;
		}
		else if (bAnyEnabled)
		{
			return ECheckBoxState::Undetermined; 
		}
	}
	
	return ECheckBoxState::Unchecked;	
}

void FPCGEditor::OnToggleEnabled()
{
	const ECheckBoxState CheckState = GetEnabledCheckState();
	const bool bNewCheckState = !(CheckState != ECheckBoxState::Unchecked);
	if (GraphEditorWidget.IsValid())
	{
		const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorToggleEnableTransactionMessage", "PCG Editor: Toggle Enable Nodes"), nullptr);

		bool bChanged = false;
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

			UPCGSettingsInterface* PCGSettingsInterface = PCGNode->GetSettingsInterface();
			if (!PCGSettingsInterface)
			{
				continue;
			}

			if (PCGSettingsInterface->bEnabled != bNewCheckState)
			{
				PCGSettingsInterface->Modify();
				PCGSettingsInterface->SetEnabled(bNewCheckState);
				bChanged = true;
			}
		}
		
		if (bChanged)
		{
			GraphEditorWidget->NotifyGraphChanged();
		}
	}
}

ECheckBoxState FPCGEditor::GetEnabledCheckState() const
{	
	if (GraphEditorWidget.IsValid())
	{
		bool bAllEnabled = true;
		bool bAnyEnabled = false;
		
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			if (!PCGEditorGraphNode)
			{
				continue;
			}

			const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			if (!PCGNode)
			{
				continue;
			}

			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode->GetSettingsInterface();
			if (!PCGSettingsInterface)
			{
				continue;
			}

			bAllEnabled &= PCGSettingsInterface->bEnabled;
			bAnyEnabled |= PCGSettingsInterface->bEnabled;
		}

		if (bAllEnabled)
		{
			return ECheckBoxState::Checked;
		}
		else if (bAnyEnabled)
		{
			return ECheckBoxState::Undetermined; 
		}
	}
	
	return ECheckBoxState::Unchecked;
}

void FPCGEditor::OnToggleDebug()
{
	const ECheckBoxState CheckState = GetDebugCheckState();
	const bool bNewCheckState = !(CheckState != ECheckBoxState::Unchecked);
	
	if (GraphEditorWidget.IsValid())
	{
		const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorToggleDebugTransactionMessage", "PCG Editor: Toggle Debug Nodes"), nullptr);
		
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

			UPCGSettingsInterface* PCGSettingsInterface = PCGNode->GetSettingsInterface();
			if (!PCGSettingsInterface)
			{
				continue;
			}

			if (PCGSettingsInterface->bDebug != bNewCheckState)
			{
				PCGSettingsInterface->bDebug = bNewCheckState;
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Settings);
			}
		}
	}
}

void FPCGEditor::OnDebugOnlySelected()
{
	if (GraphEditorWidget.IsValid() && PCGEditorGraph)
	{
		bool bChanged = false;

		const FGraphPanelSelectionSet& SelectedNodes = GraphEditorWidget->GetSelectedNodes();

		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDebugOnlySelectedTransactionMessage", "PCG Editor: Debug only selected nodes"), nullptr);

		bool bAnyNonSelectedNodesDebugged = false;
		bool bAllSelectedNodesDebugged = true;

		// Initial pass - inspect state of selected and non-selected nodes.
		for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGEditorGraphNode || !PCGNode || !PCGSettingsInterface)
			{
				continue;
			}

			if (SelectedNodes.Contains(PCGEditorGraphNode))
			{
				bAllSelectedNodesDebugged &= PCGSettingsInterface->bDebug;
			}
			else
			{
				bAnyNonSelectedNodesDebugged |= PCGSettingsInterface->bDebug;
			}
		}

		// The selected nodes should be debugged if any non-selected nodes are being debugged, or if the selected
		// nodes are partially being debugged.
		const bool bTargetDebugState = bAnyNonSelectedNodesDebugged || !bAllSelectedNodesDebugged;

		for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGEditorGraphNode || !PCGNode || !PCGSettingsInterface)
			{
				continue;
			}

			// Selected set to target state, non-selected should not be debugged.
			const bool bShouldBeDebug = SelectedNodes.Contains(PCGEditorGraphNode) ? bTargetDebugState : false;

			if (PCGSettingsInterface->bDebug != bShouldBeDebug)
			{
				PCGSettingsInterface->Modify();
				PCGSettingsInterface->bDebug = bShouldBeDebug;
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Settings);
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			Transaction.Cancel();
		}
	}
}

void FPCGEditor::OnDisableDebugOnAllNodes()
{
	if (GraphEditorWidget.IsValid() && PCGEditorGraph)
	{
		bool bChanged = false;
		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDisableDebugAllNodesTransactionMessage", "PCG Editor: Disable debug on all nodes"), nullptr);

		for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGEditorGraphNode || !PCGNode || !PCGSettingsInterface)
			{
				continue;
			}

			if (PCGSettingsInterface->bDebug)
			{
				PCGSettingsInterface->Modify();
				PCGSettingsInterface->bDebug = false;
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Settings);
				bChanged = true;
			}
		}

		if (!bChanged)
		{
			Transaction.Cancel();
		}
	}
}

ECheckBoxState FPCGEditor::GetDebugCheckState() const
{
	if (GraphEditorWidget.IsValid())
	{
		bool bAllDebug = true;
		bool bAnyDebug = false;
		
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			if (!PCGEditorGraphNode)
			{
				continue;
			}

			const UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
			if (!PCGNode)
			{
				continue;
			}

			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode->GetSettingsInterface();
			if (!PCGSettingsInterface)
			{
				continue;
			}

			bAllDebug &= PCGSettingsInterface->bDebug;
			bAnyDebug |= PCGSettingsInterface->bDebug;
		}

		if (bAllDebug)
		{
			return ECheckBoxState::Checked;
		}
		else if (bAnyDebug)
		{
			return ECheckBoxState::Undetermined;
		}
	}
	
	return ECheckBoxState::Unchecked;
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

		UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode);
		if (UPCGNode* PastedPCGNode = PastedPCGGraphNode ? PastedPCGGraphNode->GetPCGNode() : nullptr)
		{
			PCGGraphBeingEdited->AddNode(PastedPCGNode);
		}
	}

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode);
		if (UPCGNode* PastedPCGNode = PastedPCGGraphNode ? PastedPCGGraphNode->GetPCGNode() : nullptr)
		{
			PastedPCGGraphNode->RebuildAfterPaste();
		}
	}

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		UPCGEditorGraphNodeBase* PastedPCGGraphNode = Cast<UPCGEditorGraphNodeBase>(PastedNode);
		if (UPCGNode* PastedPCGNode = PastedPCGGraphNode ? PastedPCGGraphNode->GetPCGNode() : nullptr)
		{
			PastedPCGGraphNode->PostPaste();

			if (UPCGSettings* Settings = PastedPCGNode->GetSettings())
			{
				Settings->PostPaste();
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
	if (PCGEditorGraph)
	{
		PCGEditorGraph->OnClose();
	}

	if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
	{
		LevelEditor->OnMapChanged().RemoveAll(this);
	}
	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().RemoveAll(this);
	}

	// Extra nodes are replicated on editor close, to be saved in the underlying PCGGraph
	ReplicateExtraNodes();

	FAssetEditorToolkit::OnClose();

	if (PCGComponentBeingInspected.IsValid())
	{
		PCGComponentBeingInspected->DisableInspection();

		if (PCGGraphBeingEdited)
		{
			PCGGraphBeingEdited->DisableInspection();
		}
	}

	if (PCGGraphBeingEdited)
	{
		if (PCGGraphBeingEdited->NotificationsForEditorArePausedByUser())
		{
			PCGGraphBeingEdited->ToggleUserPausedNotificationsForEditor();
		}

		PCGGraphBeingEdited->OnGraphGridSizesChangedDelegate.RemoveAll(this);
		PCGGraphBeingEdited->OnGraphDynamicallyExecutedDelegate.RemoveAll(this);
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

TSharedRef<SPCGEditorGraphDebugObjectWidget> FPCGEditor::CreateDebugObjectWidget()
{
	return SNew(SPCGEditorGraphDebugObjectWidget, SharedThis(this));
}

TSharedRef<SPCGEditorGraphDebugObjectTree> FPCGEditor::CreateDebugObjectTreeWidget()
{
	return SNew(SPCGEditorGraphDebugObjectTree, SharedThis(this));
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

TSharedRef<SPCGEditorGraphLogView> FPCGEditor::CreateLogWidget()
{
	return SNew(SPCGEditorGraphLogView, SharedThis(this));
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
				if (PCGNode->IsInstance())
				{
					SelectedObjects.Add(Cast<UPCGSettingsInstance>(PCGNode->GetSettingsInterface()));
				}
				else
				{
					SelectedObjects.Add(PCGNode->GetSettings());
				}
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

					UPCGEditorSettings* MutableEditorSettings = GetMutableDefault<UPCGEditorSettings>();
					MutableEditorSettings->Modify();
					MutableEditorSettings->bEnableNavigateToNativeNodes = (NewState == ECheckBoxState::Checked) ? true : false;
					MutableEditorSettings->SaveConfig();
				}
			);
			Info.CheckBoxText = LOCTEXT("EnableNavigationToNative", "Enable Navigate to Native Nodes?");

			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

bool FPCGEditor::IsReadOnlyProperty(const FPropertyAndParent& InPropertyAndParent, IDetailsView* InDetailsView) const
{
	// Everything is writeable when not in an instance
	if (!InDetailsView || 
		InPropertyAndParent.ParentProperties.IsEmpty() || 
		InPropertyAndParent.ParentProperties.Last()->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGSettingsInstance, Settings))
	{
		return false;
	}

	const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = InDetailsView->GetSelectedObjects();
	for (const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjects)
	{
		if (!SelectedObject.IsValid())
		{
			continue;
		}
		
		if (UPCGSettingsInstance* Instance = Cast<UPCGSettingsInstance>(SelectedObject.Get()))
		{
			return true;
		}
	}

	return false;
}

bool FPCGEditor::IsVisibleProperty(const FPropertyAndParent& InPropertyAndParent, IDetailsView* InDetailsView) const
{
	if (!InDetailsView)
	{
		return true;
	}

	// Currently never hide anything from the graph settings
	if (InPropertyAndParent.Objects.Num() == 1 && Cast<UPCGGraph>(InPropertyAndParent.Objects[0]))
	{
		return true;
	}

	// Always hide asset info information
	if (InPropertyAndParent.Property.HasMetaData(TEXT("Category")) &&
		InPropertyAndParent.Property.GetMetaData(TEXT("Category")) == TEXT("AssetInfo"))
	{
		return false;
	}

	// Otherwise, everything is visible when not in an instance
	if(	InPropertyAndParent.ParentProperties.IsEmpty() ||
		InPropertyAndParent.ParentProperties.Last()->GetFName() != GET_MEMBER_NAME_CHECKED(UPCGSettingsInstance, Settings))
	{
		return true;
	}

	// Hide debug settings from the setting when showing the instance settings.
	if (InPropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, bEnabled) ||
		InPropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, bDebug) ||
		(InPropertyAndParent.ParentProperties.Num() >= 2 && 
			InPropertyAndParent.ParentProperties[InPropertyAndParent.ParentProperties.Num() - 2]->GetFName() == GET_MEMBER_NAME_CHECKED(UPCGSettings, DebugSettings)))
	{
		return false;
	}

	return true;
}

void FPCGEditor::OnGraphGridSizesChanged(UPCGGraphInterface* InGraph)
{
	check(PCGEditorGraph);
	if (UPCGComponent* PCGComponent = GetPCGComponentBeingInspected())
	{
		PCGEditorGraph->UpdateGridSizeVisualization(PCGComponent);
	}
}

void FPCGEditor::OnGraphDynamicallyExecuted(UPCGGraphInterface* InGraphInterface, const TWeakObjectPtr<UPCGComponent> InSourceComponent, FPCGStack InvocationStack)
{
	InvocationStack.GetStackFramesMutable().Emplace(InGraphInterface);

	if (DebugObjectWidget.IsValid())
	{
		DebugObjectWidget->AddDynamicStack(InSourceComponent, InvocationStack);
	}

	if (DebugObjectTreeWidget.IsValid())
	{
		DebugObjectTreeWidget->AddDynamicStack(InSourceComponent, InvocationStack);
	}
}

UPCGSubsystem* FPCGEditor::GetSubsystem()
{
	UWorld* World = (GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr);
	return UPCGSubsystem::GetInstance(World);
}

void FPCGEditor::OnMapChanged(UWorld* InWorld, EMapChangeType InMapChangedType)
{
	if (InMapChangedType != EMapChangeType::SaveMap)
	{
		if (DebugObjectTreeWidget.IsValid())
		{
			DebugObjectTreeWidget->RequestRefresh();
		}
		if (DebugObjectWidget.IsValid())
		{
			DebugObjectWidget->RefreshDebugObjects();
		}
	}
}

void FPCGEditor::OnLevelActorDeleted(AActor* InActor)
{
	if (DebugObjectTreeWidget.IsValid())
	{
		DebugObjectTreeWidget->RequestRefresh();
	}

	// Forward call as this makes an effort to retain the selection if the selected component has not been deleted.
	if (DebugObjectWidget.IsValid())
	{
		DebugObjectWidget->OnLevelActorDeleted(InActor);
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

TSharedRef<SDockTab> FPCGEditor::SpawnTab_DebugObject(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGDebugObjectTitle", "Debug Object"))
		.TabColorScale(GetTabColorScale())
		[
			DebugObjectTreeWidget.ToSharedRef()
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

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Log(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGLogTitle", "Log Capture"))
		.TabColorScale(GetTabColorScale())
		[
			LogWidget.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
