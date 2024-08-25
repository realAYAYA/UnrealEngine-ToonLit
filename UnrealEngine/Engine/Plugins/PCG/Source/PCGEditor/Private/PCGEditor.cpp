// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditor.h"

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
#include "Elements/PCGReroute.h"
#include "Helpers/PCGSubgraphHelpers.h"
#include "Rendering/SlateRenderer.h"
#include "Tests/Determinism/PCGDeterminismNativeTests.h"
#include "Tests/Determinism/PCGDeterminismTestBlueprintBase.h"
#include "Tests/Determinism/PCGDeterminismTestsCommon.h"

#include "PCGEditorCommands.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphNode.h"
#include "PCGEditorGraphNodeInput.h"
#include "PCGEditorGraphNodeOutput.h"
#include "PCGEditorGraphNodeReroute.h"
#include "PCGEditorGraphSchema.h"
#include "PCGEditorGraphSchemaActions.h"
#include "PCGEditorMenuContext.h"
#include "PCGEditorSettings.h"
#include "PCGEditorStyle.h"
#include "PCGEditorUtils.h"
#include "SPCGEditorGraphAttributeListView.h"
#include "SPCGEditorGraphDebugObjectTree.h"
#include "SPCGEditorGraphDetailsView.h"
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
#include "Misc/MessageDialog.h"
#include "Misc/TransactionObjectEvent.h"
#include "Preferences/UnrealEdOptions.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "PCGGraphEditor"

namespace FPCGEditor_private
{
	const FName GraphEditorID = FName(TEXT("GraphEditor"));
	const FName PropertyDetailsID[] = {
		FName(TEXT("PropertyDetails")),
		FName(TEXT("PropertyDetails2")),
		FName(TEXT("PropertyDetails3")),
		FName(TEXT("PropertyDetails4")) };
	const FName PaletteID = FName(TEXT("Palette"));
	const FName DebugObjectID = FName(TEXT("DebugObject"));
	const FName AttributesID[] = {
		FName(TEXT("Attributes")),
		FName(TEXT("Attributes2")),
		FName(TEXT("Attributes3")),
		FName(TEXT("Attributes4")) };
	const FName FindID = FName(TEXT("Find"));
	const FName DeterminismID = FName(TEXT("Determinism"));
	const FName ProfilingID = FName(TEXT("Profiling"));
	const FName LogID = FName(TEXT("Log"));
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph(UPCGGraph* InGraph)
{
	if (!InGraph)
	{
		return nullptr;
	}

	if (!InGraph->PCGEditorGraph)
	{
		InGraph->PCGEditorGraph = NewObject<UPCGEditorGraph>(InGraph, UPCGEditorGraph::StaticClass(), NAME_None, RF_Transactional | RF_Transient);
		InGraph->PCGEditorGraph->Schema = UPCGEditorGraphSchema::StaticClass();
		InGraph->PCGEditorGraph->InitFromNodeGraph(InGraph);
	}

	return InGraph->PCGEditorGraph;
}

void FPCGEditor::Initialize(const EToolkitMode::Type InMode, const TSharedPtr<class IToolkitHost>& InToolkitHost, UPCGGraph* InPCGGraph)
{
	PCGGraphBeingEdited = InPCGGraph;

	// Initializes the UPCGEditorGraph if needed
	GetPCGEditorGraph(InPCGGraph);

	PCGGraphBeingEdited->PCGEditorGraph->SetEditor(SharedThis(this));
	PCGEditorGraph = PCGGraphBeingEdited->PCGEditorGraph;

	for (int PropertyDetailsIndex = 0; PropertyDetailsIndex < 4; ++PropertyDetailsIndex)
	{
		TSharedRef<SPCGEditorGraphDetailsView> PropertyDetailsWidget = SNew(SPCGEditorGraphDetailsView);
		PropertyDetailsWidget->SetEditor(SharedThis(this));
		PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);
		PropertyDetailsWidgets.Add(PropertyDetailsWidget);
	}

	GraphEditorWidget = CreateGraphEditorWidget();
	PaletteWidget = CreatePaletteWidget();
	DebugObjectTreeWidget = CreateDebugObjectTreeWidget();
	FindWidget = CreateFindWidget();

	for (int AttributesIndex = 0; AttributesIndex < 4; ++AttributesIndex)
	{
		AttributesWidgets.Add(CreateAttributesWidget());
	}
	
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
					->AddTab(FPCGEditor_private::PropertyDetailsID[0], ETabState::OpenedTab)
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
					->AddTab(FPCGEditor_private::AttributesID[0], ETabState::OpenedTab)
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

	// Hook to PIE start/end to keep callbacks up to date.
	FEditorDelegates::PostPIEStarted.AddRaw(this, &FPCGEditor::OnPostPIEStarted);
	FEditorDelegates::EndPIE.AddRaw(this, &FPCGEditor::OnEndPIE);

	if (GEditor)
	{
		RegisterDelegatesForWorld(GEditor->GetEditorWorldContext().World());

		// In case the editor is opened while in PIE, we should try setting up callbacks for the PIE world.
		RegisterDelegatesForWorld(GEditor->PlayWorld.Get());
	}

	// Clear inspection flag on all nodes.
	for (UEdGraphNode* EdGraphNode : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
		{
			PCGEditorGraphNode->SetInspected(false);
		}
	}
}

UPCGEditorGraph* FPCGEditor::GetPCGEditorGraph()
{
	return PCGEditorGraph;
}

void FPCGEditor::SetStackBeingInspected(const FPCGStack& FullStack)
{
	if (FullStack == StackBeingInspected)
	{
		// No-op if we're already inspecting this stack.
		return;
	}

	UPCGComponent* OldComponent = PCGComponentBeingInspected.Get();
	UPCGComponent* NewComponent = const_cast<UPCGComponent*>(FullStack.GetRootComponent());
	const bool bComponentChanged = (NewComponent != OldComponent);

	if (OldComponent)
	{
		if (bComponentChanged)
		{
			OldComponent->DisableInspection();
		}

		if (PCGGraphBeingEdited)
		{
			PCGGraphBeingEdited->DisableInspection();
		}
	}

	const bool bNewComponentStartedInspecting = NewComponent && !NewComponent->IsInspecting();

	PCGComponentBeingInspected = NewComponent;

	StackBeingInspected = FullStack;
	OnInspectedStackChangedDelegate.Broadcast(StackBeingInspected);

	if (NewComponent)
	{
		if (bComponentChanged)
		{
			PCGComponentBeingInspected->EnableInspection();
		}

		if (PCGGraphBeingEdited)
		{
			PCGGraphBeingEdited->EnableInspection(StackBeingInspected);
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
			ChangeType |= PCGNode->UpdateStructuralVisualization(NewComponent, &StackBeingInspected);

			if (ChangeType != EPCGChangeType::None)
			{
				PCGNode->ReconstructNode();
			}
		}
	}
}

void FPCGEditor::ClearStackBeingInspected()
{
	if (GetStackBeingInspected())
	{
		SetStackBeingInspected(FPCGStack());
	}
}

void FPCGEditor::UpdateDebugAfterComponentSelection(UPCGComponent* InOldComponent, UPCGComponent* InNewComponent, bool bInNewComponentStartedInspecting)
{
	if (!ensure(PCGGraphBeingEdited) || (InOldComponent == InNewComponent))
	{
		return;
	}

	auto RefreshComponent = [](UPCGComponent* Component)
	{
		if (!ensure(Component))
		{
			return;
		}

		// GenerateAtRuntime components should be refreshed through the runtime gen scheduler.
		if (Component->IsManagedByRuntimeGenSystem())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				// We don't want to do a full cleanup if we're setting the debug object, since full cleanup destroys the component, which is the debug object itself!
				Subsystem->RefreshRuntimeGenComponent(Component);
			}
		}
		else
		{
			Component->GenerateLocal(/*bForce=*/true);
		}
	};

	// If individual component debugging is disabled, just generate the new component if required.
	if (!PCGGraphBeingEdited->DebugFlagAppliesToIndividualComponents())
	{
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			RefreshComponent(InNewComponent);
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
			RefreshComponent(InNewComponent->GetOriginalComponent());
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
			RefreshComponent(InOldComponent->GetOriginalComponent());
		}

		// Debug new component if it wasn't already
		if (InNewComponent && bInNewComponentStartedInspecting)
		{
			// Use original component - debug can be displayed both by the local component and parent local components.
			RefreshComponent(InNewComponent->GetOriginalComponent());
		}
	}
}

const FPCGStack* FPCGEditor::GetStackBeingInspected() const
{
	return StackBeingInspected.GetStackFrames().IsEmpty() ? nullptr : &StackBeingInspected;
}

void FPCGEditor::JumpToNode(const UEdGraphNode* InNode)
{
	if (GraphEditorWidget.IsValid())
	{
		GraphEditorWidget->JumpToNode(InNode);
	}
}

UPCGEditorGraphNodeBase* FPCGEditor::GetEditorNode(const UPCGNode* InNode)
{
	if (!ensure(PCGEditorGraph) || !InNode)
	{
		return nullptr;
	}

	for (UEdGraphNode* EdGraphNode : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEdGraphNode = Cast<UPCGEditorGraphNodeBase>(EdGraphNode))
		{
			if (PCGEdGraphNode->GetPCGNode() == InNode)
			{
				return PCGEdGraphNode;
			}
		}
	}

	return nullptr;
}

void FPCGEditor::JumpToNode(const UPCGNode* InNode)
{
	if (const UPCGEditorGraphNodeBase* EditorNode = GetEditorNode(InNode))
	{
		JumpToNode(EditorNode);
	}
}

void FPCGEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_PCGEditor", "PCG Editor"));
	TSharedRef<FWorkspaceItem> DetailsGroup = WorkspaceMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_PCGEditor_Details", "Details"));
	TSharedRef<FWorkspaceItem> AttributesGroup = WorkspaceMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_PCGEditor_Attributes", "Attributes"));
	const TSharedRef<FWorkspaceItem>& WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	//TODO: Add Icons
	InTabManager->RegisterTabSpawner(FPCGEditor_private::GraphEditorID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_GraphEditor))
		.SetDisplayName(LOCTEXT("GraphTab", "Graph"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[0], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 0))
		.SetDisplayName(LOCTEXT("DetailsTab1", "Details 1"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[1], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 1))
		.SetDisplayName(LOCTEXT("DetailsTab2", "Details 2"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[2], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 2))
		.SetDisplayName(LOCTEXT("DetailsTab3", "Details 3"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PropertyDetailsID[3], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_PropertyDetails, 3))
		.SetDisplayName(LOCTEXT("DetailsTab4", "Details 4"))
		.SetGroup(DetailsGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::PaletteID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Palette))
		.SetDisplayName(LOCTEXT("PaletteTab", "Palette"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::DebugObjectID, FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_DebugObjectTree))
		.SetDisplayName(LOCTEXT("DebugTab", "Debug Object Tree"))
		.SetGroup(WorkspaceMenuCategoryRef);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[0], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 0))
		.SetDisplayName(LOCTEXT("AttributesTab1", "Attributes 1"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[1], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 1))
		.SetDisplayName(LOCTEXT("AttributesTab2", "Attributes 2"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[2], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 2))
		.SetDisplayName(LOCTEXT("AttributesTab3", "Attributes 3"))
		.SetGroup(AttributesGroup);

	InTabManager->RegisterTabSpawner(FPCGEditor_private::AttributesID[3], FOnSpawnTab::CreateSP(this, &FPCGEditor::SpawnTab_Attributes, 3))
		.SetDisplayName(LOCTEXT("AttributesTab4", "Attributes 4"))
		.SetGroup(AttributesGroup);

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
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[0]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[1]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[2]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PropertyDetailsID[3]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::PaletteID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::DebugObjectID);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[0]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[1]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[2]);
	InTabManager->UnregisterTabSpawner(FPCGEditor_private::AttributesID[3]);
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
			// Deepest change type to catch all types of change (like redoing adding a grid size node or etc).
			PCGGraphBeingEdited->NotifyGraphChanged(EPCGChangeType::Structural | EPCGChangeType::GenerationGrid);
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
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* ToolBar = ToolMenus->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);

		const FPCGEditorCommands& PCGEditorCommands = FPCGEditorCommands::Get();
		const FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);
		FToolMenuSection& Section = ToolBar->AddSection("PCGToolbar", TAttribute<FText>(), InsertAfterAssetSection);

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			PCGEditorCommands.Find,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.Find")));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().PauseAutoRegeneration,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.PauseRegen")));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().ForceGraphRegeneration,
			TAttribute<FText>(),
			TAttribute<FText>(),
			TAttribute<FSlateIcon>::CreateLambda([]()
			{
				static const FSlateIcon ForceRegen = FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.ForceRegen");
				static const FSlateIcon ForceRegenClearCache = FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.ForceRegenClearCache");
					
				FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
				return ModifierKeys.IsControlDown() ? ForceRegenClearCache : ForceRegen;
			})));

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().CancelExecution,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.StopRegen")));
				
		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			FPCGEditorCommands::Get().OpenDebugObjectTreeTab,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.OpenDebugTreeTab")));

		Section.AddSeparator(NAME_None);

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			 PCGEditorCommands.RunDeterminismGraphTest,
			 TAttribute<FText>(),
			 TAttribute<FText>(),
			 FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.RunDeterminismTest")));

		Section.AddSeparator(NAME_None);

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			 PCGEditorCommands.EditGraphSettings,
			 TAttribute<FText>(),
			 TAttribute<FText>(),
			 FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "PCG.Command.GraphSettings")));
	}
}

void FPCGEditor::BindCommands()
{
	const FPCGEditorCommands& PCGEditorCommands = FPCGEditorCommands::Get();

	ToolkitCommands->MapAction(
		PCGEditorCommands.Find,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnFind));

	ToolkitCommands->MapAction(
		PCGEditorCommands.ShowSelectedDetails,
		FExecuteAction::CreateSP(this, &FPCGEditor::OpenDetailsView));

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

	// Left on UI as a disabled button if debug object tree tab already open. This is a deliberate
	// hint for 5.4 to help direct users to use the tree.
	ToolkitCommands->MapAction(
		PCGEditorCommands.OpenDebugObjectTreeTab,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnOpenDebugObjectTreeTab_Clicked),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::IsDebugObjectTreeTabClosed));

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
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleEnabled),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetEnabledCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ToggleDebug,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnToggleDebug),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleDebug),
		FGetActionCheckState::CreateSP(this, &FPCGEditor::GetDebugCheckState));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.DebugOnlySelected,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDebugOnlySelected),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleDebug));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.DisableDebugOnAllNodes,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnDisableDebugOnAllNodes),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanToggleDebug));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.AddSourcePin,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnAddDynamicInputPin),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanAddDynamicInputPin));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.RenameNode,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnRenameNode),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanRenameNode));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ConvertNamedRerouteToReroute,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnConvertNamedRerouteToReroute),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanConvertNamedRerouteToReroute));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.SelectNamedRerouteUsages,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSelectNamedRerouteUsages),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanSelectNamedRerouteUsages));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.SelectNamedRerouteDeclaration,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnSelectNamedRerouteDeclaration),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanSelectNamedRerouteDeclaration));

	GraphEditorCommands->MapAction(
		PCGEditorCommands.ConvertRerouteToNamedReroute,
		FExecuteAction::CreateSP(this, &FPCGEditor::OnConvertRerouteToNamedReroute),
		FCanExecuteAction::CreateSP(this, &FPCGEditor::CanConvertRerouteToNamedReroute));
}

void FPCGEditor::OnFind()
{
	if (TabManager.IsValid() && FindWidget.IsValid())
	{
		TabManager->TryInvokeTab(FPCGEditor_private::FindID);
		FindWidget->FocusForUse();
	}
}

void FPCGEditor::OpenDetailsView()
{
	if (TabManager.IsValid())
	{
		auto InvokeFirstUnlockedTab = [this](bool bVisibleOnly) -> bool
		{
			for (int DetailsViewIndex = 0; DetailsViewIndex < PropertyDetailsWidgets.Num(); ++DetailsViewIndex)
			{
				TSharedPtr<SPCGEditorGraphDetailsView> DetailsView = PropertyDetailsWidgets[DetailsViewIndex];
				if (DetailsView.IsValid() && !DetailsView->IsLocked())
				{
					if (!bVisibleOnly || TabManager->FindExistingLiveTab(FPCGEditor_private::PropertyDetailsID[DetailsViewIndex]))
					{
						TabManager->TryInvokeTab(FPCGEditor_private::PropertyDetailsID[DetailsViewIndex]);
						return true;
					}
				}
			}

			return false;
		};

		if (InvokeFirstUnlockedTab(true) || InvokeFirstUnlockedTab(false))
		{
			return;
		}

		// Default to first if they are all locked
		if (PropertyDetailsWidgets[0].IsValid())
		{
			TabManager->TryInvokeTab(FPCGEditor_private::PropertyDetailsID[0]);
		}
	}
}

void FPCGEditor::OnDetailsViewTabClosed(TSharedRef<SDockTab> DockTab, int Index)
{
	if (!PropertyDetailsWidgets.IsValidIndex(Index))
	{
		return;
	}

	TSharedPtr<SPCGEditorGraphDetailsView> DetailsView = PropertyDetailsWidgets[Index];
	if (DetailsView.IsValid() && DetailsView->IsLocked())
	{
		DetailsView->SetIsLocked(false);
	}
}

void FPCGEditor::OnAttributeListViewTabClosed(TSharedRef<SDockTab> DockTab, int Index)
{
	if (!AttributesWidgets.IsValidIndex(Index))
	{
		return;
	}

	TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView = AttributesWidgets[Index];
	if (AttributeListView.IsValid())
	{
		if (AttributeListView->IsLocked())
		{
			AttributeListView->SetIsLocked(false);
		}

		UPCGEditorGraphNodeBase* NodeInspected = AttributeListView->GetNodeBeingInspected();
		AttributeListView->SetNodeBeingInspected(nullptr);

		if (NodeInspected)
		{
			bool bIsStillInspectedOnVisibleTabs = false;
			for (int OtherTabIndex = 0; OtherTabIndex < AttributesWidgets.Num(); ++OtherTabIndex)
			{
				TSharedPtr<SPCGEditorGraphAttributeListView> ALV = AttributesWidgets[OtherTabIndex];
				if (ALV.IsValid() && ALV->GetNodeBeingInspected() == NodeInspected && TabManager->FindExistingLiveTab(FPCGEditor_private::AttributesID[OtherTabIndex]))
				{
					bIsStillInspectedOnVisibleTabs = true;
					break;
				}
			}

			if (!bIsStillInspectedOnVisibleTabs)
			{
				NodeInspected->SetInspected(false);

				for (TSharedPtr<SPCGEditorGraphAttributeListView> ALV : AttributesWidgets)
				{
					if (ALV.IsValid() && ALV->GetNodeBeingInspected() == NodeInspected)
					{
						ALV->SetNodeBeingInspected(nullptr);
					}
				}
			}
		}
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
		EPCGChangeType ChangeType = EPCGChangeType::Structural;

		FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
		if (ModifierKeys.IsControlDown())
		{
			if (UPCGSubsystem* Subsystem = GetSubsystem())
			{
				Subsystem->FlushCache();
			}

			ChangeType |= EPCGChangeType::GenerationGrid;
		}

		PCGGraphBeingEdited->ForceNotificationForEditor(ChangeType);
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

bool FPCGEditor::IsDebugObjectTreeTabClosed() const
{
	return !TabManager.IsValid() || !TabManager->FindExistingLiveTab(FPCGEditor_private::DebugObjectID).IsValid();
}

void FPCGEditor::OnOpenDebugObjectTreeTab_Clicked()
{
	TabManager->TryInvokeTab(FPCGEditor_private::DebugObjectID);
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
				NodeResult->TestResultTitle = FName(*PCGNode->GetNodeTitle(EPCGNodeTitleType::ListView).ToString());
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
	for (TSharedPtr<SPCGEditorGraphDetailsView> PropertyDetailsWidget : PropertyDetailsWidgets)
	{
		PropertyDetailsWidget->SetObject(PCGGraphBeingEdited);
	}
}

bool FPCGEditor::IsEditGraphSettingsToggled() const
{
	for (TSharedPtr<SPCGEditorGraphDetailsView> PropertyDetailsWidget : PropertyDetailsWidgets)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = PropertyDetailsWidget->GetSelectedObjects();
		if (SelectedObjects.Num() == 1 && SelectedObjects[0] == PCGGraphBeingEdited.Get())
		{
			return true;
		}
	}

	return false;
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

void FPCGEditor::OnRenameNode()
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	if (!ensure(SelectedNodes.Num() == 1))
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Attempting to rename multiple nodes."));
		return;
	}

	UPCGEditorGraphNodeBase* Node = CastChecked<UPCGEditorGraphNodeBase>(*SelectedNodes.CreateConstIterator());
	Node->EnterRenamingMode();
}

bool FPCGEditor::CanRenameNode() const
{
	check(GraphEditorWidget.IsValid());

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	// You cannot enter renaming mode on multiple nodes at once, since they will not all enter synchronously.
	// Simultaneous editing of multiple InlineEditableTextBlocks may not even be possible with default behavior.
	if (SelectedNodes.Num() != 1)
	{
		return false;
	}

	if (const UPCGEditorGraphNodeBase* SelectedNode = Cast<UPCGEditorGraphNodeBase>(*SelectedNodes.CreateConstIterator()))
	{
		return !SelectedNode->GetPCGNode() || !SelectedNode->GetPCGNode()->GetSettings() || SelectedNode->GetPCGNode()->GetSettings()->CanUserEditTitle();
	}
	else
	{
		return false;
	}
}

bool FPCGEditor::InternalValidationOnAction()
{
	if (!GraphEditorWidget.IsValid() || PCGEditorGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("GraphEditorWidget or PCGEditorGraph is null, aborting"));
		return false;
	}

	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	if (PCGGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("PCGGraph is null, aborting"));
		return false;
	}

	return true;
}

void FPCGEditor::OnConvertNamedRerouteToReroute()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	check(PCGGraph);

	TArray<UPCGNode*> DeclarationsToConvert;
	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		if (!Object)
		{
			continue;
		}

		if (UPCGEditorGraphNodeNamedRerouteDeclaration* DeclarationNode = Cast<UPCGEditorGraphNodeNamedRerouteDeclaration>(Object))
		{
			check(DeclarationNode->GetPCGNode());
			DeclarationsToConvert.Add(DeclarationNode->GetPCGNode());
		}
	}

	if (DeclarationsToConvert.IsEmpty())
	{
		return;
	}

	// Disable graph notifications
	PCGGraph->DisableNotificationsForEditor();

	// For all found declarations, replace by a normal reroute node and forward the connection from the usages to this reroute instead.
	TArray<UPCGNode*> NodesToDelete;

	for (UPCGNode* Declaration : DeclarationsToConvert)
	{
		NodesToDelete.Add(Declaration);

		// Create reroute node
		UPCGSettings* RerouteNodeSettings = nullptr;
		UPCGNode* RerouteNode = PCGGraph->AddNodeOfType(UPCGRerouteSettings::StaticClass(), RerouteNodeSettings);

		// Copy any non-functional information
		Declaration->TransferEditorProperties(RerouteNode);

		// Force a graph refresh - needed for the other operations/editor callbacks to go through, a bit unfortunate.
		// TODO: improve this
		PCGEditorGraph->ReconstructGraph();

		UPCGPin* RerouteInput = RerouteNode->GetInputPin(PCGPinConstants::DefaultInputLabel);
		UPCGPin* RerouteOutput = RerouteNode->GetOutputPin(PCGPinConstants::DefaultOutputLabel);

		// Copy input edge to the declaration to the new reroute node
		check(Declaration->GetInputPin(PCGPinConstants::DefaultInputLabel));
		
		TArray<UPCGEdge*> InputEdges = Declaration->GetInputPin(PCGPinConstants::DefaultInputLabel)->Edges;
		ensure(InputEdges.Num() <= 1);

		if(InputEdges.Num() >= 1)
		{
			PCGGraph->AddEdge(InputEdges[0]->InputPin->Node, InputEdges[0]->InputPin->Properties.Label, RerouteNode, RerouteInput->Properties.Label);
		}

		// The reroute declaration has edges to all its usages, and potentially other edges through its out pin.
		// Keep track of all usages to be deleted but also forward edges from the usages to the newly created reroute.
		if (UPCGPin* OutputPin = Declaration->GetOutputPin(PCGPinConstants::DefaultOutputLabel))
		{
			TArray<UPCGEdge*> Edges = OutputPin->Edges;
			for(UPCGEdge* Edge : Edges)
			{
				PCGGraph->AddEdge(RerouteNode, RerouteOutput->Properties.Label, Edge->OutputPin->Node, Edge->OutputPin->Properties.Label);
			}
		}

		if (UPCGPin* InvisiblePin = Declaration->GetOutputPin(PCGNamedRerouteConstants::InvisiblePinLabel))
		{
			TArray<UPCGEdge*> Edges = InvisiblePin->Edges;
			for(UPCGEdge* Edge : Edges)
			{
				check(Edge->OutputPin && Edge->OutputPin->Node);
				UPCGNode* Usage = Edge->OutputPin->Node;
				NodesToDelete.Add(Usage);

				check(Usage->GetOutputPin(PCGPinConstants::DefaultOutputLabel));
				TArray<UPCGEdge*> UsageEdges = Usage->GetOutputPin(PCGPinConstants::DefaultOutputLabel)->Edges;
				for(UPCGEdge* UsageEdge : UsageEdges)
				{
					PCGGraph->AddEdge(RerouteNode, RerouteOutput->Properties.Label, UsageEdge->OutputPin->Node, UsageEdge->OutputPin->Properties.Label);
				}
			}
		}
	}

	// Delete removed ndoes
	PCGGraph->RemoveNodes(NodesToDelete);

	// Re-enable graph notifications
	PCGGraph->EnableNotificationsForEditor();

	// Force a graph refresh
	PCGEditorGraph->ReconstructGraph();

	// Notify the widget
	GraphEditorWidget->NotifyGraphChanged();	
}

bool FPCGEditor::CanConvertNamedRerouteToReroute() const
{
	if (!GraphEditorWidget)
	{
		return false;
	}

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		if (UPCGEditorGraphNodeNamedRerouteDeclaration* DeclarationNode = Cast<UPCGEditorGraphNodeNamedRerouteDeclaration>(Object))
		{
			return true;
		}
	}

	return false;
}

void FPCGEditor::OnSelectNamedRerouteUsages()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();

	if (SelectedNodes.Num() != 1)
	{
		return;
	}

	const UPCGEditorGraphNodeNamedRerouteDeclaration* DeclarationNode = nullptr;

	for (const UObject* Object : SelectedNodes)
	{
		DeclarationNode = Cast<UPCGEditorGraphNodeNamedRerouteDeclaration>(Object);
	}

	if (!DeclarationNode || !DeclarationNode->GetPCGNode())
	{
		return;
	}

	GraphEditorWidget->ClearSelectionSet();

	// Some assumptions below - that only usages are connected to the invisible pin.
	if (const UPCGPin* InvisiblePin = DeclarationNode->GetPCGNode()->GetOutputPin(PCGNamedRerouteConstants::InvisiblePinLabel))
	{
		for (const UPCGEdge* Edge : InvisiblePin->Edges)
		{
			if (const UPCGNode* Usage = Edge->OutputPin->Node)
			{
				GraphEditorWidget->SetNodeSelection(GetEditorNode(Usage), true);
			}
		}
	}

	GraphEditorWidget->ZoomToFit(true);
}

bool FPCGEditor::CanSelectNamedRerouteUsages() const
{
	if (!GraphEditorWidget || GraphEditorWidget->GetSelectedNodes().Num() != 1)
	{
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		return Object && Object->IsA<UPCGEditorGraphNodeNamedRerouteDeclaration>();
	}

	return false;
}

void FPCGEditor::OnSelectNamedRerouteDeclaration()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	
	if (SelectedNodes.Num() != 1)
	{
		return;
	}

	for (const UObject* Object : SelectedNodes)
	{
		const UPCGEditorGraphNodeNamedRerouteUsage* UsageNode = Cast<UPCGEditorGraphNodeNamedRerouteUsage>(Object);

		if (!UsageNode)
		{
			continue;
		}

		GraphEditorWidget->ClearSelectionSet();

		if (!UsageNode->GetPCGNode())
		{
			continue;
		}

		// Find the declaration node that matches the settings in the Usage node.
		if (UPCGNamedRerouteUsageSettings* UsageSettings = Cast<UPCGNamedRerouteUsageSettings>(UsageNode->GetPCGNode()->GetSettings()))
		{
			if (UsageSettings->Declaration && UsageSettings->Declaration->GetOuter() && UsageSettings->Declaration->GetOuter()->IsA<UPCGNode>())
			{
				JumpToNode(Cast<UPCGNode>(UsageSettings->Declaration->GetOuter()));
				break;
			}
		}
	}
}

bool FPCGEditor::CanSelectNamedRerouteDeclaration() const
{
	if (!GraphEditorWidget || GraphEditorWidget->GetSelectedNodes().Num() != 1)
	{
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		return Object && Object->IsA<UPCGEditorGraphNodeNamedRerouteUsage>();
	}

	return false;
}

void FPCGEditor::OnConvertRerouteToNamedReroute()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	check(PCGGraph);

	TArray<UPCGNode*> ReroutesToConvert;
	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		if(UPCGEditorGraphNodeReroute* RerouteNode = Cast<UPCGEditorGraphNodeReroute>(Object))
		{
			// Skip named reroutes
			if (RerouteNode->IsA<UPCGEditorGraphNodeNamedRerouteBase>())
			{
				continue;
			}

			check(RerouteNode->GetPCGNode());
			ReroutesToConvert.Add(RerouteNode->GetPCGNode());
		}
	}

	if (ReroutesToConvert.IsEmpty())
	{
		return;
	}

	// Disable graph notifications
	PCGGraph->DisableNotificationsForEditor();

	// For all found reroutes, replace by a named reroute pair and forward inputs to the declaration and outputs to the usage.
	TArray<UPCGNode*> NodesToDelete;

	for (UPCGNode* Reroute : ReroutesToConvert)
	{
		NodesToDelete.Add(Reroute);

		// Create named reroute declaration
		UPCGSettings* DeclarationSettings = nullptr;
		UPCGNode* Declaration = PCGGraph->AddNodeOfType(UPCGNamedRerouteDeclarationSettings::StaticClass(), DeclarationSettings);

		// Create named reroute usage
		UPCGSettings* UsageSettings = nullptr;
		UPCGNode* Usage = PCGGraph->AddNodeOfType(UPCGNamedRerouteUsageSettings::StaticClass(), UsageSettings);
		Cast<UPCGNamedRerouteUsageSettings>(UsageSettings)->Declaration = Cast<UPCGNamedRerouteDeclarationSettings>(DeclarationSettings);

		// Setup non-fonctional information
		Reroute->TransferEditorProperties(Declaration);
		Reroute->TransferEditorProperties(Usage);
		constexpr float PositionOffsetIncrementY = 50.f;
		Usage->PositionY += PositionOffsetIncrementY;

		// Force a graph refresh - needed for the other operations to go through which a bit unfortunate
		// TODO improve this
		PCGEditorGraph->ReconstructGraph();

		// Copy all inputs to the reroute to the declaration inputs
		check(Reroute->GetInputPin(PCGPinConstants::DefaultInputLabel));
		TArray<UPCGEdge*> RerouteInputEdges = Reroute->GetInputPin(PCGPinConstants::DefaultInputLabel)->Edges;
		ensure(RerouteInputEdges.Num() <= 1);

		if(RerouteInputEdges.Num() >= 1)
		{
			UPCGEdge* InputEdge = RerouteInputEdges[0];
			PCGGraph->AddEdge(InputEdge->InputPin->Node, InputEdge->InputPin->Properties.Label, Declaration, PCGPinConstants::DefaultInputLabel);
			CastChecked<UPCGEditorGraphNodeNamedRerouteDeclaration>(GetEditorNode(Declaration))->SetNodeName(InputEdge->InputPin->Node, InputEdge->InputPin->Properties.Label);
		}

		// Add invisible edge between declaration and usage
		PCGGraph->AddEdge(Declaration, PCGNamedRerouteConstants::InvisiblePinLabel, Usage, PCGPinConstants::DefaultInputLabel);

		// Copy all outputs from the reroute to the usage outputs
		check(Reroute->GetOutputPin(PCGPinConstants::DefaultOutputLabel));
		TArray<UPCGEdge*> OutputEdges = Reroute->GetOutputPin(PCGPinConstants::DefaultOutputLabel)->Edges;
		for (UPCGEdge* OutputEdge : OutputEdges)
		{
			PCGGraph->AddEdge(Usage, PCGPinConstants::DefaultOutputLabel, OutputEdge->OutputPin->Node, OutputEdge->OutputPin->Properties.Label);
		}
	}

	// Delete removed ndoes
	PCGGraph->RemoveNodes(NodesToDelete);

	// Re-enable graph notifications
	PCGGraph->EnableNotificationsForEditor();

	// Force a graph refresh
	PCGEditorGraph->ReconstructGraph();

	// Notify the widget
	GraphEditorWidget->NotifyGraphChanged();
}

bool FPCGEditor::CanConvertRerouteToNamedReroute() const
{
	if (!GraphEditorWidget)
	{
		return false;
	}

	for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		if (UPCGEditorGraphNodeReroute* RerouteNode = Cast<UPCGEditorGraphNodeReroute>(Object))
		{
			if (!RerouteNode->IsA<UPCGEditorGraphNodeNamedRerouteBase>())
			{
				return true;
			}
		}
	}

	return false;
}

void FPCGEditor::OnCollapseNodesInSubgraph()
{
	if (!InternalValidationOnAction())
	{
		return;
	}

	UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	check(PCGGraph);

	// Gather all nodes that will be included in the subgraph, and the extra nodes
	TArray<UPCGNode*> NodesToCollapse;
	TArray<UObject*> ExtraNodesToCollapse;

	check(GraphEditorWidget);
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

	IAssetTools& AssetTools = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TObjectPtr<UPCGGraphFactory> Factory = NewObject<UPCGGraphFactory>();

	FString NewPackageName;
	FString NewAssetName;
	PCGEditorUtils::GetParentPackagePathAndUniqueName(PCGGraph, LOCTEXT("NewPCGSubgraphAsset", "NewPCGSubgraph").ToString(), NewPackageName, NewAssetName);

	NewPCGGraph = Cast<UPCGGraph>(AssetTools.CreateAssetWithDialog(NewAssetName, NewPackageName, PCGGraph->GetClass(), Factory, "PCGEditor_CollapseInSubgraph"));

	if (NewPCGGraph == nullptr)
	{
		UE_LOG(LogPCGEditor, Warning, TEXT("Subgraph asset creation was aborted or failed, abort."));
		return;
	}

	{
		FScopedTransaction Transaction(LOCTEXT("PCGCollapseInSubgraphMessage", "[PCG] Collapse into Subgraph"));
		FText OutFailReason;
		NewPCGGraph = FPCGSubgraphHelpers::CollapseIntoSubgraphWithReason(PCGGraph, NodesToCollapse, ExtraNodesToCollapse, OutFailReason, NewPCGGraph);

		if (NewPCGGraph == nullptr)
		{
			FMessageDialog::Open(EAppMsgType::Ok, OutFailReason, LOCTEXT("PCGCollapseInSubgraphFailed", "PCG Subgraph Collapse Failed"));
			Transaction.Cancel();
			return;
		}

		// Force a refresh
		PCGEditorGraph->ReconstructGraph();
	}

	if (NewPCGGraph)
	{
		// Save the new asset
		UEditorAssetLibrary::SaveLoadedAsset(NewPCGGraph);

		// Notify the widget
		GraphEditorWidget->NotifyGraphChanged();
	}
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

		// Also exclude reroute nodes
		if (Object->IsA<UPCGEditorGraphNodeReroute>() || Object->IsA<UPCGEditorGraphNodeNamedRerouteBase>())
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

	UEdGraphNode* GraphNode = GraphEditorWidget->GetSingleSelectedNode();
	UPCGEditorGraphNodeBase* PCGGraphNodeBase = Cast<UPCGEditorGraphNodeBase>(GraphNode);

	const UPCGNode* PCGNode = PCGGraphNodeBase ? PCGGraphNodeBase->GetPCGNode() : nullptr;
	const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

	if (PCGSettingsInterface && !PCGSettingsInterface->CanBeDebugged())
	{
		return;
	}

	TArray<UPCGEditorGraphNodeBase*, TInlineAllocator<4>> InspectedNodesBefore;
	for (TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView : AttributesWidgets)
	{
		InspectedNodesBefore.Add(AttributeListView->GetNodeBeingInspected());
	}

	bool bIsInspecting = false;

	// If the selected node was previously inspected, stop inspecting it, and unselect it from the attribute list views
	if (InspectedNodesBefore.Contains(PCGGraphNodeBase))
	{
		PCGGraphNodeBase->SetInspected(false);

		for (TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView : AttributesWidgets)
		{
			if (AttributeListView->GetNodeBeingInspected() == PCGGraphNodeBase)
			{
				AttributeListView->SetNodeBeingInspected(nullptr);
			}
		}
	}
	else
	{
		TArray<UPCGEditorGraphNodeBase*, TInlineAllocator<4>> InspectedNodesAfter;

		for (TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView : AttributesWidgets)
		{
			if (!AttributeListView->IsLocked())
			{
				AttributeListView->SetNodeBeingInspected(PCGGraphNodeBase);
			}

			InspectedNodesAfter.Add(AttributeListView->GetNodeBeingInspected());
		}

		if (InspectedNodesAfter.Contains(PCGGraphNodeBase))
		{
			PCGGraphNodeBase->SetInspected(true);
			bIsInspecting = true;
		}

		for (UPCGEditorGraphNodeBase* BeforeNode : InspectedNodesBefore)
		{
			if (!InspectedNodesAfter.Contains(BeforeNode) && BeforeNode)
			{
				BeforeNode->SetInspected(false);
			}
		}
	}

	if (bIsInspecting)
	{
		// Summon the first attribute list view that is inspecting this node
		auto InvokeFirstTab = [this, PCGGraphNodeBase](bool bVisibleOnly) -> bool
		{
			for (int AttributeListViewIndex = 0; AttributeListViewIndex < AttributesWidgets.Num(); ++AttributeListViewIndex)
			{
				TSharedPtr<SPCGEditorGraphAttributeListView> AttributeListView = AttributesWidgets[AttributeListViewIndex];
				if (AttributeListView->GetNodeBeingInspected() == PCGGraphNodeBase)
				{
					if (!bVisibleOnly || TabManager->FindExistingLiveTab(FPCGEditor_private::AttributesID[AttributeListViewIndex]))
					{
						GetTabManager()->TryInvokeTab(FPCGEditor_private::AttributesID[AttributeListViewIndex]);
						return true;
					}
				}
			}

			return false;
		};

		const bool bTabSummoned = (InvokeFirstTab(true) || InvokeFirstTab(false));

		// Default to first if they are all locked
		if (!bTabSummoned)
		{
			GetTabManager()->TryInvokeTab(FPCGEditor_private::AttributesID[0]);
		}

		DebugObjectTreeWidget->SetNodeBeingInspected(PCGNode);
	}
	else
	{
		DebugObjectTreeWidget->SetNodeBeingInspected(nullptr);
	}
}

bool FPCGEditor::CanToggleInspected() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet& SelectedNodes = GraphEditorWidget->GetSelectedNodes();
	if (SelectedNodes.Num() != 1)
	{
		// Can only inspect one node.
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
		if (!PCGEditorGraphNode)
		{
			return false;
		}

		const UPCGSettingsInterface* PCGSettingsInterface = PCGEditorGraphNode->GetPCGNode() ? PCGEditorGraphNode->GetPCGNode()->GetSettingsInterface() : nullptr;
		if (PCGSettingsInterface && PCGSettingsInterface->CanBeDebugged())
		{
			return true;
		}
	}

	return false;
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

	// To prevent the changes on the editor node from being in the transaction, we delay reconstruction.
	TArray<FPCGDeferNodeReconstructScope> DeferredEditorNodes;

	if (GraphEditorWidget.IsValid())
	{
		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorToggleEnableTransactionMessage", "PCG Editor: Toggle Enable Nodes"), nullptr);

		bool bChanged = false;
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDisabled())
			{
				continue;
			}

			if (PCGSettingsInterface->bEnabled != bNewCheckState)
			{
				DeferredEditorNodes.Emplace(PCGEditorGraphNode);
				PCGSettingsInterface->Modify();
				PCGSettingsInterface->SetEnabled(bNewCheckState);
				bChanged = true;
			}
		}

		if (bChanged)
		{
			GraphEditorWidget->NotifyGraphChanged();
		}
		else
		{
			Transaction.Cancel();
		}
	}
}

bool FPCGEditor::CanToggleEnabled() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
		const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
		if (!PCGNode)
		{
			continue;
		}

		if (PCGNode->GetSettingsInterface() && PCGNode->GetSettingsInterface()->CanBeDisabled())
		{
			return true;
		}
	}

	// Could not toggle enabled on anything in selection.
	return false;
}

ECheckBoxState FPCGEditor::GetEnabledCheckState() const
{
	if (GraphEditorWidget.IsValid())
	{
		bool bAllEnabled = true;
		bool bAnyEnabled = false;

		for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDisabled())
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
		FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorToggleDebugTransactionMessage", "PCG Editor: Toggle Debug Nodes"), nullptr);

		bool bChanged = false;
		for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDebugged())
			{
				continue;
			}

			if (PCGSettingsInterface->bDebug != bNewCheckState)
			{
				PCGSettingsInterface->Modify(/*bAlwaysMarkDirty=*/false);
				PCGSettingsInterface->bDebug = bNewCheckState;
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

bool FPCGEditor::CanToggleDebug() const
{
	if (!GraphEditorWidget.IsValid())
	{
		return false;
	}

	for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
	{
		const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
		const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;

		if (PCGNode && PCGNode->GetSettingsInterface()->CanBeDebugged())
		{
			return true;
		}
	}

	// Could not toggle debug on anything in selection.
	return false;
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
		for (const UEdGraphNode* Node : PCGEditorGraph->Nodes)
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;
			if (!PCGSettingsInterface)
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

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDebugged())
			{
				continue;
			}

			// Selected set to target state, non-selected should not be debugged.
			const bool bShouldBeDebug = SelectedNodes.Contains(PCGEditorGraphNode) ? bTargetDebugState : false;

			if (PCGSettingsInterface->bDebug != bShouldBeDebug)
			{
				PCGSettingsInterface->Modify(/*bAlwaysMarkDirty=*/false);
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
			if (!PCGSettingsInterface)
			{
				continue;
			}

			if (PCGSettingsInterface->bDebug)
			{
				PCGSettingsInterface->Modify(/*bAlwaysMarkDirty=*/false);
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

		for (const UObject* Object : GraphEditorWidget->GetSelectedNodes())
		{
			const UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object);
			const UPCGNode* PCGNode = PCGEditorGraphNode ? PCGEditorGraphNode->GetPCGNode() : nullptr;
			const UPCGSettingsInterface* PCGSettingsInterface = PCGNode ? PCGNode->GetSettingsInterface() : nullptr;

			if (!PCGSettingsInterface || !PCGSettingsInterface->CanBeDebugged())
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

		// DeleteSelectedNodes is called directly from UI command 
		PCGGraph->PrimeGraphCompilationCache();

		bool bChanged = false;

		{
			const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorDeleteTransactionMessage", "PCG Editor: Delete"), nullptr);
			PCGEditorGraph->Modify();
		
			TArray<UPCGNode*> NodesToRemove;

			for (UObject* Object : GraphEditorWidget->GetSelectedNodes())
			{
				if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Object))
				{
					if (PCGEditorGraphNode->CanUserDeleteNode())
					{
						UPCGNode* PCGNode = PCGEditorGraphNode->GetPCGNode();
						check(PCGNode);

						NodesToRemove.Add(PCGNode);

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

			if (bChanged)
			{
				PCGGraph->RemoveNodes(NodesToRemove);
			}
		}

		if (bChanged)
		{
			GraphEditorWidget->ClearSelectionSet();
			GraphEditorWidget->NotifyGraphChanged();
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

	TArray<UPCGNode*> NodesToPaste;

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
			NodesToPaste.Add(PastedPCGNode);
		}
	}

	PCGGraphBeingEdited->AddNodes(NodesToPaste);

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

	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FEditorDelegates::EndPIE.RemoveAll(this);

	// Extra nodes are replicated on editor close, to be saved in the underlying PCGGraph
	ReplicateExtraNodes();

	FAssetEditorToolkit::OnClose();

	if (PCGComponentBeingInspected.IsValid())
	{
		if (PCGComponentBeingInspected->IsInspecting())
		{
			PCGComponentBeingInspected->DisableInspection();
		}

		if (PCGGraphBeingEdited && PCGGraphBeingEdited->IsInspecting())
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
	}

	if (GEditor)
	{
		UnregisterDelegatesForWorld(GEditor->GetEditorWorldContext().World());
		UnregisterDelegatesForWorld(GEditor->PlayWorld.Get());
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

	if (NewSelection.Num() == 0)
	{
		SelectedObjects.Add(PCGGraphBeingEdited);
	}
	else
	{
		for (UObject* Object : NewSelection)
		{
			if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object))
			{
				SelectedObjects.Add(GraphNode);
			}
		}
	}

	for (TSharedPtr<SPCGEditorGraphDetailsView> PropertyDetailsWidget : PropertyDetailsWidgets)
	{
		PropertyDetailsWidget->SetObjects(SelectedObjects, /*bForceRefresh=*/true);
	}
}

void FPCGEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		if (CommitInfo == ETextCommit::OnEnter || CommitInfo == ETextCommit::OnUserMovedFocus)
		{
			const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorRenameNode", "PCG Editor: Rename Node"), nullptr);
			NodeBeingChanged->OnRenameNode(NewText.ToString());

			// Implementation detail: In UPCGEditorGraphNode we only set the title under certain conditions, so it calls Modify() itself.
			// However, UEdGraphNode does not call Modify() on its own, so we should still call it in this case.
			if (!NodeBeingChanged->IsA<UPCGEditorGraphNode>())
			{
				NodeBeingChanged->Modify();
			}
		}

		if (UPCGEditorGraphNodeBase* PCGEditorNode = Cast<UPCGEditorGraphNodeBase>(NodeBeingChanged))
		{
			PCGEditorNode->ExitRenamingMode();
		}
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

void FPCGEditor::OnComponentGenerationCompleteOrCancelled(UPCGSubsystem* Subsystem)
{
	DebugObjectTreeWidget->RequestRefresh();

	const bool CacheDebuggingEnabled = Subsystem && Subsystem->IsGraphCacheDebuggingEnabled();

	// Refresh nodes to report any errors/warnings, and to display culling state after execution.
	check(PCGEditorGraph);
	for (UEdGraphNode* Node : PCGEditorGraph->Nodes)
	{
		if (UPCGEditorGraphNodeBase* PCGEditorGraphNode = Cast<UPCGEditorGraphNodeBase>(Node))
		{
			// If we are debugging the graph cache then we need to refresh the cache count displayed in the title after every generation.
			EPCGChangeType ChangeType = CacheDebuggingEnabled ? EPCGChangeType::Cosmetic : EPCGChangeType::None;

			ChangeType |= PCGEditorGraphNode->UpdateErrorsAndWarnings();
			ChangeType |= PCGEditorGraphNode->UpdateStructuralVisualization(PCGComponentBeingInspected.Get(), &StackBeingInspected);

			if (ChangeType != EPCGChangeType::None)
			{
				PCGEditorGraphNode->ReconstructNode();
			}
		}
	}
}

UPCGSubsystem* FPCGEditor::GetSubsystem()
{
	UWorld* World = (GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr);
	return UPCGSubsystem::GetInstance(World);
}

void FPCGEditor::RegisterDelegatesForWorld(UWorld* World)
{
	UnregisterDelegatesForWorld(World);

	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World))
	{
		Subsystem->OnComponentGenerationCompleteOrCancelled.AddRaw(this, &FPCGEditor::OnComponentGenerationCompleteOrCancelled);
	}
}

void FPCGEditor::UnregisterDelegatesForWorld(UWorld* World)
{
	if (UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World))
	{
		Subsystem->OnComponentGenerationCompleteOrCancelled.RemoveAll(this);
	}
}

void FPCGEditor::OnMapChanged(UWorld* InWorld, EMapChangeType InMapChangedType)
{
	if (InMapChangedType != EMapChangeType::SaveMap)
	{
		if (DebugObjectTreeWidget.IsValid())
		{
			DebugObjectTreeWidget->RequestRefresh();
		}

		// Subsystem has been torn down and rebuilt.
		if (GEditor)
		{
			RegisterDelegatesForWorld(GEditor->GetEditorWorldContext().World());
			RegisterDelegatesForWorld(GEditor->PlayWorld.Get());
		}
	}
}

void FPCGEditor::OnPostPIEStarted(bool bIsSimulating)
{
	RegisterDelegatesForWorld(GEditor ? GEditor->PlayWorld.Get() : nullptr);
}

void FPCGEditor::OnEndPIE(bool bIsSimulating)
{
	UnregisterDelegatesForWorld(GEditor ? GEditor->PlayWorld.Get() : nullptr);
}

void FPCGEditor::OnLevelActorDeleted(AActor* InActor)
{
	// Forward call as this makes an effort to retain the selection if the selected component has not been deleted.
	if (DebugObjectTreeWidget.IsValid())
	{
		DebugObjectTreeWidget->RequestRefresh();
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

TSharedRef<SDockTab> FPCGEditor::SpawnTab_PropertyDetails(const FSpawnTabArgs& Args, int PropertyDetailsIndex)
{
	TAttribute<FText> Label = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FPCGEditor::GetDetailsTabLabel, PropertyDetailsIndex));
	TSharedPtr<SPCGEditorGraphDetailsView> DetailsView = PropertyDetailsWidgets[PropertyDetailsIndex];

	return SNew(SDockTab)
		.Label(Label)
		.OnTabClosed_Raw(this, &FPCGEditor::OnDetailsViewTabClosed, PropertyDetailsIndex)
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
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

TSharedRef<SDockTab> FPCGEditor::SpawnTab_DebugObjectTree(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PCGDebugObjectTitle", "Debug Object"))
		.TabColorScale(GetTabColorScale())
		[
			DebugObjectTreeWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FPCGEditor::SpawnTab_Attributes(const FSpawnTabArgs& Args, int AttributesIndex)
{
	TAttribute<FText> Label = TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateRaw(this, &FPCGEditor::GetAttributesTabLabel, AttributesIndex));

	return SNew(SDockTab)
		.Label(Label)
		.OnTabClosed_Raw(this, &FPCGEditor::OnAttributeListViewTabClosed, AttributesIndex)
		.TabColorScale(GetTabColorScale())
		[
			AttributesWidgets[AttributesIndex].ToSharedRef()
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

FText FPCGEditor::GetDetailsTabLabel(int DetailsIndex)
{
	if (DetailsIndex == 0)
	{
		return LOCTEXT("PCGDetailsTitle", "Details");
	}
	else
	{
		return FText::Format(LOCTEXT("PCGDetailsTitle_Multi", "Details {0}"), DetailsIndex + 1);
	}
}

FText FPCGEditor::GetDetailsViewObjectName(int DetailsIndex)
{
	return LOCTEXT("PCGDetailsName", "This is a node name placeholder");
}

FText FPCGEditor::GetAttributesTabLabel(int AttributesIndex)
{
	if (AttributesIndex == 0)
	{
		return LOCTEXT("PCGAttributesTitle", "Attributes");
	}
	else
	{
		return FText::Format(LOCTEXT("PCGAttributesTitle_Multi", "Attributes {0}"), AttributesIndex + 1);
	}
}

#undef LOCTEXT_NAMESPACE
