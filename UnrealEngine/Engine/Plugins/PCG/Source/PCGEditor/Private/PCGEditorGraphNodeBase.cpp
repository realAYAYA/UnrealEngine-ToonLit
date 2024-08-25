// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNodeBase.h"

#include "PCGComponent.h"
#include "PCGEdge.h"
#include "PCGEngineSettings.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "PCGSettingsWithDynamicInputs.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Elements/PCGHiGenGridSize.h"
#include "Elements/PCGReroute.h"

#include "PCGEditor.h"
#include "PCGEditorCommands.h"
#include "PCGEditorCommon.h"
#include "PCGEditorGraph.h"
#include "PCGEditorGraphSchema.h"
#include "PCGEditorSettings.h"
#include "PCGEditorStyle.h"

#include "GraphEditorActions.h"
#include "ScopedTransaction.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/TransactionObjectEvent.h"
#include "Widgets/Colors/SColorPicker.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphNodeBase"

namespace PCGEditorGraphSwitches
{
	TAutoConsoleVariable<bool> CVarCheckConnectionCycles{
		TEXT("pcg.Editor.CheckConnectionCycles"),
		true,
		TEXT("Prevents user from creating cycles in graph")
	};
}

namespace PCGEditorGraphNodeBase
{
	/** Whether this node was culled during graph compilation or during graph execution. */
	bool ShouldDisplayAsActive(const UPCGEditorGraphNodeBase* InNode, const UPCGComponent* InComponentBeingDebugged, const FPCGStack* InStackBeingInspected)
	{
		if (!InNode)
		{
			return true;
		}

		const UPCGNode* PCGNode = InNode->GetPCGNode();
		if (!PCGNode)
		{
			return true;
		}

		// Don't display as culled while component is executing or about to refresh as nodes will flash to culled state and back
		// which looks disturbing.
		if (!InComponentBeingDebugged || InComponentBeingDebugged->IsGenerating() || InComponentBeingDebugged->IsRefreshInProgress())
		{
			return true;
		}

		const UPCGEngineSettings* EngineSettings = GetDefault<UPCGEngineSettings>();
		const bool bActiveVisualizationEnabled = !ensure(EngineSettings) || EngineSettings->bDisplayCullingStateWhenDebugging;
		const UPCGSettings* Settings = PCGNode ? PCGNode->GetSettings() : nullptr;

		// Display whether node was culled dynamically or statically.
		if (InStackBeingInspected && bActiveVisualizationEnabled)
		{
			if (!Settings || !Settings->IsA<UPCGRerouteSettings>())
			{
				// Task will be displayed as active if it was executed.
				return InComponentBeingDebugged->WasNodeExecuted(PCGNode, *InStackBeingInspected);
			}
			else
			{
				// Named reroute usages mirror the enabled state of the upstream declaration.
				if (const UPCGNamedRerouteUsageSettings* RerouteUsageSettings = Cast<UPCGNamedRerouteUsageSettings>(Settings))
				{
					const UPCGNode* DeclarationPCGNode = RerouteUsageSettings->Declaration ? Cast<UPCGNode>(RerouteUsageSettings->Declaration->GetOuter()) : nullptr;
					const UPCGEditorGraph* EditorGraph = Cast<UPCGEditorGraph>(InNode->GetOuter());
					const UPCGEditorGraphNodeBase* DeclarationNode = EditorGraph ? EditorGraph->GetEditorNodeFromPCGNode(DeclarationPCGNode) : nullptr;
					return !DeclarationNode || ShouldDisplayAsActive(DeclarationNode, InComponentBeingDebugged, InStackBeingInspected);
				}

				// Special case - reroute culled state is evaluated here based on upstream connections. Reroutes are always culled/never executed, but still need
				// to reflect the active/inactive state to not look wrong/confusing.
				for (const UEdGraphPin* Pin : InNode->Pins)
				{
					if (Pin && Pin->Direction == EEdGraphPinDirection::EGPD_Input)
					{
						for (const UEdGraphPin* LinkedPin : Pin->LinkedTo)
						{
							if (const UPCGEditorGraphNodeBase* UpstreamNode = LinkedPin ? Cast<UPCGEditorGraphNodeBase>(LinkedPin->GetOwningNode()) : nullptr)
							{
								const bool bUpstreamNodeActive = ShouldDisplayAsActive(UpstreamNode, InComponentBeingDebugged, InStackBeingInspected);
								const bool bUpstreamPinActive = UpstreamNode->IsOutputPinActive(LinkedPin);

								if (bUpstreamNodeActive && bUpstreamPinActive)
								{
									// Active if any input is active.
									return true;
								}
							}
						}
					}
				}

				return false;
			}
		}

		return true;
	}
}

void UPCGEditorGraphNodeBase::Construct(UPCGNode* InPCGNode)
{
	check(InPCGNode);
	PCGNode = InPCGNode;
	InPCGNode->OnNodeChangedDelegate.AddUObject(this, &UPCGEditorGraphNodeBase::OnNodeChanged);

	NodePosX = InPCGNode->PositionX;
	NodePosY = InPCGNode->PositionY;
	NodeComment = InPCGNode->NodeComment;
	bCommentBubblePinned = InPCGNode->bCommentBubblePinned;
	bCommentBubbleVisible = InPCGNode->bCommentBubbleVisible;
	bCanRenameNode = false;

	if (const UPCGSettingsInterface* PCGSettingsInterface = InPCGNode->GetSettingsInterface())
	{
		const ENodeEnabledState NewEnabledState = !PCGSettingsInterface->bEnabled ? ENodeEnabledState::Disabled : ENodeEnabledState::Enabled;
		SetEnabledState(NewEnabledState);

		bCanUserAddRemoveSourcePins = (InPCGNode->GetSettings() && InPCGNode->GetSettings()->IsA<UPCGSettingsWithDynamicInputs>());
	}

	// Update to current graph/inspection state.
	const UPCGEditorGraph* Graph = Cast<UPCGEditorGraph>(GetOuter());
	const FPCGEditor* Editor = Graph ? Graph->GetEditor().Pin().Get() : nullptr;
	UpdateStructuralVisualization(Editor ? Editor->GetPCGComponentBeingInspected() : nullptr, Editor ? Editor->GetStackBeingInspected() : nullptr, /*bNewlyPlaced=*/true);
}

void UPCGEditorGraphNodeBase::BeginDestroy()
{
	if (PCGNode)
	{
		PCGNode->OnNodeChangedDelegate.RemoveAll(this);
	}

	Super::BeginDestroy();
}

void UPCGEditorGraphNodeBase::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);

	TArray<FName> PropertiesChanged = TransactionEvent.GetChangedProperties();

	if (PropertiesChanged.Contains(TEXT("bCommentBubblePinned")))
	{
		UpdateCommentBubblePinned();
	}

	if (PropertiesChanged.Contains(TEXT("NodePosX")) || PropertiesChanged.Contains(TEXT("NodePosY")))
	{
		UpdatePosition();
	}
}

void UPCGEditorGraphNodeBase::GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->Node)
	{
		return;
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaPinActions", LOCTEXT("PinActionsMenuHeader", "Pin Actions"));
		if (Context->Pin && bCanUserAddRemoveSourcePins)
		{
			Section.AddMenuEntry(FPCGEditorCommands::Get().AddSourcePin);
			Section.AddMenuEntry("RemovePin",
				LOCTEXT("RemovePin", "Remove Source Pin"),
				LOCTEXT("RemovePinTooltip", "Remove this source pin from the current node"),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([Pin = Context->Pin, this]
					{
						const_cast<UPCGEditorGraphNodeBase*>(this)->OnUserRemoveDynamicInputPin(const_cast<UEdGraphPin*>(Pin));
					}),
					FCanExecuteAction::CreateLambda([Pin = Context->Pin, this]
					{
						return const_cast<UPCGEditorGraphNodeBase*>(this)->CanUserRemoveDynamicInputPin(const_cast<UEdGraphPin*>(Pin));
					})));
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsHeader", "Node Actions"));
		Section.AddMenuEntry(FPCGEditorCommands::Get().ToggleEnabled, LOCTEXT("ToggleEnabledLabel", "Enable"));
		Section.AddMenuEntry(FPCGEditorCommands::Get().ToggleDebug, LOCTEXT("ToggleDebugLabel", "Debug"));
		Section.AddMenuEntry(FPCGEditorCommands::Get().DebugOnlySelected);
		Section.AddMenuEntry(FPCGEditorCommands::Get().DisableDebugOnAllNodes);
		Section.AddMenuEntry(FPCGEditorCommands::Get().ToggleInspect, LOCTEXT("ToggleinspectionLabel", "Inspect"));
		Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
		Section.AddMenuEntry(FPCGEditorCommands::Get().CollapseNodes);
		Section.AddMenuEntry(FPCGEditorCommands::Get().ExportNodes, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "ClassIcon.PCGSettings"));
		Section.AddMenuEntry(FPCGEditorCommands::Get().ConvertToStandaloneNodes);
		Section.AddMenuEntry(FPCGEditorCommands::Get().RenameNode, LOCTEXT("RenameNode", "Rename"));

		// Special nodes operations
		if (PCGNode && PCGNode->GetSettings())
		{
			if (PCGNode->GetSettings()->IsA<UPCGNamedRerouteDeclarationSettings>())
			{
				Section.AddMenuEntry(FPCGEditorCommands::Get().ConvertNamedRerouteToReroute);
				Section.AddMenuEntry(FPCGEditorCommands::Get().SelectNamedRerouteUsages);
			}
			else if (PCGNode->GetSettings()->IsA<UPCGNamedRerouteUsageSettings>())
			{
				Section.AddMenuEntry(FPCGEditorCommands::Get().SelectNamedRerouteDeclaration);
			}
			else if (PCGNode->GetSettings()->IsA<UPCGRerouteSettings>())
			{
				Section.AddMenuEntry(FPCGEditorCommands::Get().ConvertRerouteToNamedReroute);
			}
		}
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaOrganization", LOCTEXT("OrganizationHeader", "Organization"));
		Section.AddMenuEntry(
			"PCGNode_SetColor",
			LOCTEXT("PCGNode_SetColor", "Set Node Color"),
			LOCTEXT("PCGNode_SetColorTooltip", "Sets a specific color on the given node. Note that white maps to the default value"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ColorPicker.Mode"),
			FUIAction(FExecuteAction::CreateUObject(const_cast<UPCGEditorGraphNodeBase*>(this), &UPCGEditorGraphNodeBase::OnPickColor),
				FCanExecuteAction::CreateUObject(const_cast<UPCGEditorGraphNodeBase*>(this), &UPCGEditorGraphNodeBase::CanPickColor)));

		Section.AddSubMenu("Alignment", LOCTEXT("AlignmentHeader", "Alignment"), FText(), FNewToolMenuDelegate::CreateLambda([](UToolMenu* AlignmentMenu)
		{
			{
				FToolMenuSection& SubSection = AlignmentMenu->AddSection("EdGraphSchemaAlignment", LOCTEXT("AlignHeader", "Align"));
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesTop);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesMiddle);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesBottom);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesLeft);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesCenter);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().AlignNodesRight);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().StraightenConnections);
			}

			{
				FToolMenuSection& SubSection = AlignmentMenu->AddSection("EdGraphSchemaDistribution", LOCTEXT("DistributionHeader", "Distribution"));
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesHorizontally);
				SubSection.AddMenuEntry(FGraphEditorCommands::Get().DistributeNodesVertically);
			}
		}));
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaCommentGroup", LOCTEXT("CommentGroupHeader", "Comment Group"));
		Section.AddMenuEntry(FGraphEditorCommands::Get().CreateComment,
			LOCTEXT("MultiCommentDesc", "Create Comment from Selection"),
			LOCTEXT("CommentToolTip", "Create a resizable comment box around selection."));
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaDeterminism", LOCTEXT("DeterminismHeader", "Determinism"));
		Section.AddMenuEntry(FPCGEditorCommands::Get().RunDeterminismNodeTest,
			LOCTEXT("Determinism_RunTest", "Validate Determinism on Selection"),
			LOCTEXT("Determinism_RunTestToolTip", "Run a test to validate the selected nodes for determinism."));
	}
}

void UPCGEditorGraphNodeBase::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (PCGNode == nullptr || FromPin == nullptr)
	{
		return;
	}

	const bool bFromPinIsInput = FromPin->Direction == EEdGraphPinDirection::EGPD_Input;
	const TArray<TObjectPtr<UPCGPin>>& OtherPinsList = bFromPinIsInput ? PCGNode->GetOutputPins() : PCGNode->GetInputPins();

	// Try to connect to the first compatible pin
	for (const TObjectPtr<UPCGPin>& OtherPin : OtherPinsList)
	{
		check(OtherPin);

		const FName& OtherPinName = OtherPin->Properties.Label;
		UEdGraphPin* ToPin = FindPinChecked(OtherPinName, bFromPinIsInput ? EEdGraphPinDirection::EGPD_Output : EEdGraphPinDirection::EGPD_Input);
		if (ToPin && GetSchema()->TryCreateConnection(FromPin, ToPin))
		{
			// Connection succeeded
			break;
		}
	}

	NodeConnectionListChanged();
}

void UPCGEditorGraphNodeBase::PrepareForCopying()
{
	if (PCGNode)
	{
		// Temporarily take ownership of the MaterialExpression, so that it is not deleted when cutting
		PCGNode->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

bool UPCGEditorGraphNodeBase::CanCreateUnderSpecifiedSchema(const UEdGraphSchema* Schema) const
{
	return Schema->IsA(UPCGEditorGraphSchema::StaticClass());
}

void UPCGEditorGraphNodeBase::PostCopy()
{
	if (PCGNode)
	{
		UPCGEditorGraph* PCGEditorGraph = CastChecked<UPCGEditorGraph>(GetGraph());
		UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
		check(PCGGraph);
		PCGNode->Rename(nullptr, PCGGraph, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void UPCGEditorGraphNodeBase::PostPasteNode()
{
	bDisableReconstructFromNode = true;
}

void UPCGEditorGraphNodeBase::RebuildAfterPaste()
{
	if (PCGNode)
	{
		PCGNode->RebuildAfterPaste();

		RebuildEdgesFromPins();

		PCGNode->OnNodeChangedDelegate.AddUObject(this, &UPCGEditorGraphNodeBase::OnNodeChanged);
		PCGNode->PositionX = NodePosX;
		PCGNode->PositionY = NodePosY;

		// Refresh the node if it has dynamic pins
		if (const UPCGSettings* Settings = PCGNode->GetSettings())
		{
			if (Settings->HasDynamicPins())
			{
				PCGNode->OnNodeChangedDelegate.Broadcast(PCGNode, EPCGChangeType::Node);
			}
		}
	}
}

void UPCGEditorGraphNodeBase::PostPaste()
{
	bDisableReconstructFromNode = false;
}

void UPCGEditorGraphNodeBase::EnableDeferredReconstruct()
{
	ensure(DeferredReconstructCounter >= 0);
	++DeferredReconstructCounter;
}

void UPCGEditorGraphNodeBase::DisableDeferredReconstruct()
{
	ensure(DeferredReconstructCounter > 0);
	--DeferredReconstructCounter;

	if (DeferredReconstructCounter == 0 && bDeferredReconstruct)
	{
		ReconstructNode();
		bDeferredReconstruct = false;
	}
}

void UPCGEditorGraphNodeBase::RebuildEdgesFromPins()
{
	check(PCGNode);
	check(bDisableReconstructFromNode);

	if (PCGNode->GetGraph())
	{
		PCGNode->GetGraph()->DisableNotificationsForEditor();
	}

	RebuildEdgesFromPins_Internal();
	
	if (PCGNode->GetGraph())
	{
		PCGNode->GetGraph()->EnableNotificationsForEditor();
	}
}

void UPCGEditorGraphNodeBase::RebuildEdgesFromPins_Internal()
{
	check(PCGNode);
	check(bDisableReconstructFromNode);

	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			for (UEdGraphPin* ConnectedPin : Pin->LinkedTo)
			{
				UEdGraphNode* ConnectedGraphNode = ConnectedPin->GetOwningNode();
				UPCGEditorGraphNodeBase* ConnectedPCGGraphNode = CastChecked<UPCGEditorGraphNodeBase>(ConnectedGraphNode);

				if (UPCGNode* ConnectedPCGNode = ConnectedPCGGraphNode->GetPCGNode())
				{
					PCGNode->AddEdgeTo(Pin->PinName, ConnectedPCGNode, ConnectedPin->PinName);
				}
			}
		}
	}
}

void UPCGEditorGraphNodeBase::OnNodeChanged(UPCGNode* InNode, EPCGChangeType ChangeType)
{
	if (InNode == PCGNode)
	{
		if (!!(ChangeType & EPCGChangeType::Settings))
		{
			if (const UPCGSettingsInterface* PCGSettingsInterface = InNode->GetSettingsInterface())
			{
				const ENodeEnabledState NewEnabledState = PCGSettingsInterface->bEnabled ? ENodeEnabledState::Enabled : ENodeEnabledState::Disabled;
				if (NewEnabledState != GetDesiredEnabledState())
				{
					SetEnabledState(NewEnabledState);
				}
			}
		}

		ChangeType |= UpdateErrorsAndWarnings();

		UPCGComponent* ComponentBeingDebugged = nullptr;
		const FPCGStack* StackBeingDebugged = nullptr;

		{
			const UPCGEditorGraph* EditorGraph = CastChecked<UPCGEditorGraph>(GetGraph());
			const FPCGEditor* Editor = EditorGraph ? EditorGraph->GetEditor().Pin().Get() : nullptr;
			ComponentBeingDebugged = Editor ? Editor->GetPCGComponentBeingInspected() : nullptr;
			StackBeingDebugged = Editor ? Editor->GetStackBeingInspected() : nullptr;
		}

		if (!!(ChangeType & (EPCGChangeType::Structural | EPCGChangeType::Node | EPCGChangeType::Edge | EPCGChangeType::Cosmetic)))
		{
			ReconstructNodeOnChange();
		}
	}
}

EPCGChangeType UPCGEditorGraphNodeBase::UpdateErrorsAndWarnings()
{
	const UPCGSubsystem* Subsystem = UPCGSubsystem::GetActiveEditorInstance();
	if (!PCGNode || !Subsystem)
	{
		return EPCGChangeType::None;
	}
	
	const FPCGStack* InspectedStack = nullptr;
	{
		const UPCGEditorGraph* EditorGraph = CastChecked<UPCGEditorGraph>(GetGraph());
		const FPCGEditor* Editor = (EditorGraph && EditorGraph->GetEditor().IsValid()) ? EditorGraph->GetEditor().Pin().Get() : nullptr;
		InspectedStack = Editor ? Editor->GetStackBeingInspected() : nullptr;
	}

	const bool bOldHasCompilerMessage = bHasCompilerMessage;
	const int32 OldErrorType = ErrorType;
	const FString OldErrorMsg = ErrorMsg;
		
	if (InspectedStack)
	{
		// Get errors/warnings for the inspected stack.
		FPCGStack StackWithNode = *InspectedStack;
		StackWithNode.PushFrame(PCGNode);
		bHasCompilerMessage = Subsystem->GetNodeVisualLogs().HasLogs(StackWithNode);

		if (bHasCompilerMessage)
		{
			ErrorMsg = Subsystem->GetNodeVisualLogs().GetLogsSummaryText(StackWithNode).ToString();

			const bool bHasErrors = Subsystem->GetNodeVisualLogs().HasLogsOfVerbosity(StackWithNode, ELogVerbosity::Error);
			ErrorType = bHasErrors ? EMessageSeverity::Error : EMessageSeverity::Warning;
		}
		else
		{
			ErrorMsg.Empty();
			ErrorType = 0;
		}
	}
	else
	{
		// Collect all errors/warnings for this node.
		ELogVerbosity::Type MinimumVerbosity;
		ErrorMsg = Subsystem->GetNodeVisualLogs().GetLogsSummaryText(PCGNode.Get(), MinimumVerbosity).ToString();

		bHasCompilerMessage = !ErrorMsg.IsEmpty();

		if (bHasCompilerMessage)
		{
			ErrorType = MinimumVerbosity < ELogVerbosity::Warning ? EMessageSeverity::Error : EMessageSeverity::Warning;
		}
		else
		{
			ErrorType = 0;
		}
	}

	const bool bStateChanged = (bHasCompilerMessage != bOldHasCompilerMessage) || (ErrorType != OldErrorType) || (ErrorMsg != OldErrorMsg);
	return bStateChanged ? EPCGChangeType::Cosmetic : EPCGChangeType::None;
}

EPCGChangeType UPCGEditorGraphNodeBase::UpdateStructuralVisualization(UPCGComponent* InComponentBeingDebugged, const FPCGStack* InStackBeingInspected, bool bNewlyPlaced)
{
	const UPCGGraph* Graph = PCGNode ? PCGNode->GetGraph() : nullptr;
	if (!Graph)
	{
		return EPCGChangeType::None;
	}

	const bool bInspecting = InComponentBeingDebugged && InStackBeingInspected && !InStackBeingInspected->GetStackFrames().IsEmpty();

	EPCGChangeType ChangeType = EPCGChangeType::None;

	const uint64 NewInactiveMask = bInspecting ? InComponentBeingDebugged->GetNodeInactivePinMask(PCGNode, *InStackBeingInspected) : 0;
	if (NewInactiveMask != InactiveOutputPinMask)
	{
		InactiveOutputPinMask = NewInactiveMask;
		ChangeType |= EPCGChangeType::Cosmetic;
	}

	// Check top graph for higen enable - subgraphs always inherit higen state from the top graph.
	const UPCGGraph* TopGraph = bInspecting ? InStackBeingInspected->GetRootGraph() : Graph;
	const bool HiGenEnabled = TopGraph && TopGraph->IsHierarchicalGenerationEnabled();

	// Set the inspected grid size - this is used for grid size visualization.
	uint32 InspectingGridSize = PCGHiGenGrid::UninitializedGridSize();
	EPCGHiGenGrid InspectingGrid = EPCGHiGenGrid::Uninitialized;
	if (TopGraph && TopGraph->IsHierarchicalGenerationEnabled() && InComponentBeingDebugged && (InComponentBeingDebugged->IsPartitioned() || InComponentBeingDebugged->IsLocalComponent()))
	{
		InspectingGridSize = InComponentBeingDebugged->GetGenerationGridSize();
		InspectingGrid = InComponentBeingDebugged->GetGenerationGrid();
	}

	if (InspectedGenerationGrid != InspectingGrid)
	{
		InspectedGenerationGrid = InspectingGrid;
		ChangeType |= EPCGChangeType::Cosmetic;
	}

	bool bShouldDisplayAsDisabled = false;

	// Special treatment for higen grid sizes nodes which do nothing if higen is disabled.
	// TODO: Drive this from an API on settings as we add more higen-specific functionality.
	if (PCGNode && Cast<UPCGHiGenGridSizeSettings>(PCGNode->GetSettings()))
	{
		// Higen must be enabled on graph, and we must be editing top graph.
		bShouldDisplayAsDisabled = (Graph != TopGraph) || !TopGraph->IsHierarchicalGenerationEnabled();

		// If we're inspecting a component, it must either be a partitioned OC or an LC (because higen requires partitioning).
		if (!bShouldDisplayAsDisabled && InComponentBeingDebugged)
		{
			bShouldDisplayAsDisabled = !InComponentBeingDebugged->IsPartitioned() && !InComponentBeingDebugged->IsLocalComponent();
		}
	}

	// Don't do culling visualization on newly placed nodes. Let the execution complete notification update that.
	bool bIsCulled = !bNewlyPlaced && !PCGEditorGraphNodeBase::ShouldDisplayAsActive(this, InComponentBeingDebugged, InStackBeingInspected);

	EPCGHiGenGrid ThisGrid = EPCGHiGenGrid::Uninitialized;

	// Show grid size visualization if higen is enabled and if we're inspecting a specific grid, and we're inspecting a subgraph since subgraphs
	// execute at the invoked grid level.
	if (HiGenEnabled && InspectingGridSize != PCGHiGenGrid::UninitializedGridSize())
	{
		if (InStackBeingInspected && InStackBeingInspected->IsCurrentFrameInRootGraph())
		{
			const uint32 DefaultGridSize = TopGraph->GetDefaultGridSize();
			const uint32 NodeGridSize = Graph->GetNodeGenerationGridSize(PCGNode, DefaultGridSize);

			if (NodeGridSize < InspectingGridSize)
			{
				// Disable nodes that are on a smaller grid
				bShouldDisplayAsDisabled |= NodeGridSize < InspectingGridSize;

				// We don't know if the node was culled or not on that grid, disable visualization.
				bIsCulled = false;
			}
			else if (NodeGridSize > InspectingGridSize)
			{
				// We don't know if the node was culled or not on that grid, disable visualization.
				bIsCulled = false;
			}

			ThisGrid = PCGHiGenGrid::GridSizeToGrid(NodeGridSize);
		}
		else
		{
			// If higen is enabled then we are inspecting an invoked subgraph. Display the inspected grid size so that the user still
			// gets the execution grid information.
			ThisGrid = PCGHiGenGrid::GridSizeToGrid(InspectingGridSize);
		}
	}

	if (GenerationGrid != ThisGrid)
	{
		GenerationGrid = ThisGrid;
		ChangeType |= EPCGChangeType::Cosmetic;
	}

	SetIsCulledFromExecution(bIsCulled);

	if (bIsCulled)
	{
		bShouldDisplayAsDisabled = true;
	}

	if (IsDisplayAsDisabledForced() != bShouldDisplayAsDisabled)
	{
		SetForceDisplayAsDisabled(bShouldDisplayAsDisabled);
		ChangeType |= EPCGChangeType::Cosmetic;
	}

	return ChangeType;
}

bool UPCGEditorGraphNodeBase::ShouldDrawCompact() const
{
	UPCGSettings* Settings = PCGNode ? PCGNode->GetSettings() : nullptr;
	return Settings && Settings->ShouldDrawNodeCompact();
}

bool UPCGEditorGraphNodeBase::GetCompactNodeIcon(FName& OutCompactNodeIcon) const
{
	UPCGSettings* Settings = PCGNode ? PCGNode->GetSettings() : nullptr;
	return Settings && Settings->GetCompactNodeIcon(OutCompactNodeIcon);
}

bool UPCGEditorGraphNodeBase::HasFlippedTitleLines() const
{
	return PCGNode ? PCGNode->HasFlippedTitleLines() : false;
}

FText UPCGEditorGraphNodeBase::GetAuthoredTitleLine() const
{
	return PCGNode ? PCGNode->GetAuthoredTitleLine() : FText();
}

FText UPCGEditorGraphNodeBase::GetGeneratedTitleLine() const
{
	return PCGNode ? PCGNode->GetGeneratedTitleLine() : FText();
}

bool UPCGEditorGraphNodeBase::IsOutputPinActive(const UEdGraphPin* InOutputPin) const
{
	bool bPinActive = true;

	if (InactiveOutputPinMask != 0)
	{
		bool bFoundPin = false;
		int OutputPinIndex = 0;

		for (const UEdGraphPin* NodePin : Pins)
		{
			if (NodePin == InOutputPin)
			{
				bFoundPin = true;
				break;
			}

			if (NodePin->Direction == EEdGraphPinDirection::EGPD_Output)
			{
				++OutputPinIndex;
			}
		}

		if (bFoundPin)
		{
			bPinActive = !((1ULL << OutputPinIndex) & InactiveOutputPinMask);
		}
	}

	return bPinActive;
}

void UPCGEditorGraphNodeBase::EnterRenamingMode()
{
	bCanRenameNode = true;

	// Notify the SPCGEditorGraphNode that the user is trying to rename it.
	OnNodeRenameInitiatedDelegate.ExecuteIfBound();
}

void UPCGEditorGraphNodeBase::ExitRenamingMode()
{
	bCanRenameNode = false;

	// Update so that the node renders with the new node title.
	OnNodeChangedDelegate.ExecuteIfBound();
}

bool UPCGEditorGraphNodeBase::IsCompatible(const UPCGPin* InputPin, const UPCGPin* OutputPin, FText& OutReason) const
{
	if (PCGEditorGraphSwitches::CVarCheckConnectionCycles.GetValueOnAnyThread() && InputPin && OutputPin && InputPin->Node == PCGNode)
	{
		// Upstream Visitor
		auto Visitor = [ThisPCGNode = PCGNode](const UPCGNode* InNode, auto VisitorLambda) -> bool
		{
			if (InNode)
			{
				if (InNode == ThisPCGNode)
				{
					return false;
				}

				for (const TObjectPtr<UPCGPin>& InputPin : InNode->GetInputPins())
				{
					if (InputPin)
					{
						for (const TObjectPtr<UPCGEdge>& Edge : InputPin->Edges)
						{
							if (Edge)
							{
								if (const UPCGPin* OtherPin = Edge->GetOtherPin(InputPin.Get()))
								{
									if (!VisitorLambda(OtherPin->Node, VisitorLambda))
									{
										return false;
									}
								}
							}
						}
					}
				}
			}

			return true;
		};

		// OutputPin is trying to connect to this nodes InputPin so visit the OutputPin upstream and try to find
		// a existing connection to this UPCGEditorGraphNodeNamedRerouteDeclaration's PCGNode. If we do deny connection which would create cycle.
		if (!Visitor(OutputPin->Node, Visitor))
		{
			OutReason = LOCTEXT("ConnectionFailedCyclic", "Connection would create cycle");
			return false;
		}
	}

	return true;
}

void UPCGEditorGraphNodeBase::OnPickColor()
{
	FColorPickerArgs PickerArgs;
	PickerArgs.bIsModal = true;
	PickerArgs.bUseAlpha = false;
	PickerArgs.InitialColor = GetNodeTitleColor();
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateUObject(this, &UPCGEditorGraphNodeBase::OnColorPicked);

	OpenColorPicker(PickerArgs);
}

void UPCGEditorGraphNodeBase::OnColorPicked(FLinearColor NewColor)
{
	if (PCGNode && GetNodeTitleColor() != NewColor)
	{
		PCGNode->Modify();
		PCGNode->NodeTitleColor = NewColor;
	}
}

void UPCGEditorGraphNodeBase::ReconstructNode()
{
	// In copy-paste cases, we don't want to remove the pins
	if (bDisableReconstructFromNode)
	{
		return;
	}

	if (DeferredReconstructCounter > 0)
	{
		bDeferredReconstruct = true;
		return;
	}
	
	Modify();

	// Store copy of old pins
	TArray<UEdGraphPin*> OldPins = MoveTemp(Pins);
	Pins.Reset();

	// Generate new pins
	AllocateDefaultPins();

	// Transfer persistent data from old to new pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		const FName& OldPinName = OldPin->PinName;
		if (UEdGraphPin** NewPin = Pins.FindByPredicate([&OldPinName](UEdGraphPin* InPin) { return InPin->PinName == OldPinName; }))
		{
			(*NewPin)->MovePersistentDataFromOldPin(*OldPin);
		}
	}

	// Remove old pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		RemovePin(OldPin);
	}

	// Generate new links
	// TODO: we should either keep a map in the PCGEditorGraph or do this elsewhere
	// TODO: this will not work if we have non-PCG nodes in the graph
	if (PCGNode)
	{
		for (UEdGraphPin* Pin : Pins)
		{
			Pin->BreakAllPinLinks();
		}
		
		UPCGEditorGraph* PCGEditorGraph = CastChecked<UPCGEditorGraph>(GetGraph());
		PCGEditorGraph->CreateLinks(this, /*bCreateInbound=*/true, /*bCreateOutbound=*/true);
	}

	// Notify editor
	OnNodeChangedDelegate.ExecuteIfBound();
}

FLinearColor UPCGEditorGraphNodeBase::GetNodeTitleColor() const
{
	if (PCGNode)
	{
		const UPCGSettingsInterface* PCGSettingsInterface = PCGNode->GetSettingsInterface();
		const UPCGSettings* PCGSettings = PCGSettingsInterface ? PCGSettingsInterface->GetSettings() : nullptr;

		if (PCGNode->NodeTitleColor != FLinearColor::White)
		{
			return PCGNode->NodeTitleColor;
		}
		else if (PCGSettings)
		{
			FLinearColor SettingsColor = PCGNode->GetSettings()->GetNodeTitleColor();
			if (SettingsColor == FLinearColor::White)
			{
				SettingsColor = GetDefault<UPCGEditorSettings>()->GetColor(PCGNode->GetSettings());
			}

			if (SettingsColor != FLinearColor::White)
			{
				return SettingsColor;
			}
		}
	}

	return GetDefault<UPCGEditorSettings>()->DefaultNodeColor;
}

FLinearColor UPCGEditorGraphNodeBase::GetNodeBodyTintColor() const
{
	if (PCGNode)
	{
		const UPCGSettingsInterface* PCGSettingsInterface = PCGNode->GetSettingsInterface();
		if (PCGSettingsInterface)
		{
			if (PCGSettingsInterface->IsInstance())
			{
				return GetDefault<UPCGEditorSettings>()->InstancedNodeBodyTintColor;
			}
		}
	}

	return Super::GetNodeBodyTintColor();
}

FEdGraphPinType UPCGEditorGraphNodeBase::GetPinType(const UPCGPin* InPin)
{
	FEdGraphPinType EdPinType;
	EdPinType.ResetToDefaults();
	EdPinType.PinCategory = NAME_None;
	EdPinType.PinSubCategory = NAME_None;
	EdPinType.ContainerType = EPinContainerType::None;

	check(InPin);
	const EPCGDataType PinType = InPin->GetCurrentTypes();

	auto CheckType = [PinType](EPCGDataType AllowedType)
	{
		return !!(PinType & AllowedType) && !(PinType & ~AllowedType);
	};

	if (CheckType(EPCGDataType::Concrete))
	{
		EdPinType.PinCategory = FPCGEditorCommon::ConcreteDataType;

		// Assign subcategory if we have precise information
		if (CheckType(EPCGDataType::Point))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::PointDataType;
		}
		else if (CheckType(EPCGDataType::PolyLine))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::PolyLineDataType;
		}
		else if (CheckType(EPCGDataType::Landscape))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::LandscapeDataType;
		}
		else if (CheckType(EPCGDataType::Texture))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::TextureDataType;
		}
		else if (CheckType(EPCGDataType::RenderTarget))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::RenderTargetDataType;
		}
		else if (CheckType(EPCGDataType::Surface))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::SurfaceDataType;
		}
		else if (CheckType(EPCGDataType::Volume))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::VolumeDataType;
		}
		else if (CheckType(EPCGDataType::Primitive))
		{
			EdPinType.PinSubCategory = FPCGEditorCommon::PrimitiveDataType;
		}
	}
	else if (CheckType(EPCGDataType::Spatial))
	{
		EdPinType.PinCategory = FPCGEditorCommon::SpatialDataType;
	}
	else if (CheckType(EPCGDataType::Param))
	{
		EdPinType.PinCategory = FPCGEditorCommon::ParamDataType;
	}
	else if (CheckType(EPCGDataType::Settings))
	{
		EdPinType.PinCategory = FPCGEditorCommon::SettingsDataType;
	}
	else if (CheckType(EPCGDataType::Other))
	{
		EdPinType.PinCategory = FPCGEditorCommon::OtherDataType;
	}

	return EdPinType;
}

FText UPCGEditorGraphNodeBase::GetTooltipText() const
{
	// Either use specified tooltip for description, or fall back to node name if none given.
	const FText Description = (PCGNode && !PCGNode->GetNodeTooltipText().IsEmpty()) ? PCGNode->GetNodeTooltipText() : GetNodeTitle(ENodeTitleType::FullTitle);

	return FText::Format(LOCTEXT("NodeTooltip", "{0}\n\n{1} - Node index {2}"),
		Description,
		PCGNode ? FText::FromName(PCGNode->GetFName()) : LOCTEXT("InvalidNodeName", "Unbound node"),
		PCGNode ? FText::AsNumber(PCGNode->GetGraph()->GetNodes().IndexOfByKey(PCGNode)) : LOCTEXT("InvalidNodeIndex", "Invalid index"));
}

void UPCGEditorGraphNodeBase::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	const bool bIsInputPin = (Pin.Direction == EGPD_Input);
	UPCGPin* MatchingPin = (PCGNode ? (bIsInputPin ? PCGNode->GetInputPin(Pin.PinName) : PCGNode->GetOutputPin(Pin.PinName)) : nullptr);

	auto PCGDataTypeToText = [](EPCGDataType DataType)
	{
		TArray<FText> BitFlags;

		for (uint64 BitIndex = 1; BitIndex < 8 * sizeof(EPCGDataType); ++BitIndex)
		{
			const int64 BitValue = static_cast<__underlying_type(EPCGDataType)>(DataType) & (1 << BitIndex);
			if (BitValue)
			{
				BitFlags.Add(StaticEnum<EPCGDataType>()->GetDisplayNameTextByValue(BitValue));
			}
		}

		return FText::Join(FText(LOCTEXT("Delimiter", " | ")), BitFlags);
	};

	auto PinTypeToText = [&PCGDataTypeToText](const FName& Category, UPCGPin* MatchingPin)
	{
		if (Category != NAME_None)
		{
			return FText::FromName(Category);
		}
		else if (MatchingPin && MatchingPin->GetCurrentTypes() == EPCGDataType::Any)
		{
			return FText::FromName(FName("Any"));
		}
		else if (MatchingPin)
		{
			return PCGDataTypeToText(MatchingPin->GetCurrentTypes());
		}
		else
		{
			return FText(LOCTEXT("Unknown data type", "Unknown data type"));
		}
	};

	const FText DataTypeText = PinTypeToText(Pin.PinType.PinCategory, MatchingPin);
	const FText DataSubtypeText = PinTypeToText(Pin.PinType.PinSubCategory, MatchingPin);

	FText Description;
	if (MatchingPin)
	{
		Description = MatchingPin->Properties.Tooltip.IsEmpty() ? FText::FromName(MatchingPin->Properties.Label) : MatchingPin->Properties.Tooltip;
	}

	FText Required;
	FText MultiDataSupport;
	FText MultiConnectionSupport;	

	if (MatchingPin)
	{
		if (bIsInputPin)
		{
			if (PCGNode && PCGNode->IsInputPinRequiredByExecution(MatchingPin))
			{
				Required = LOCTEXT("InputIsRequired", "Required input. ");
			}

			MultiDataSupport = MatchingPin->Properties.bAllowMultipleData ? LOCTEXT("InputSupportsMultiData", "Supports multiple data in input(s). ") : LOCTEXT("InputSingleDataOnly", "Supports only single data in input(s). ");

			MultiConnectionSupport = MatchingPin->Properties.AllowsMultipleConnections() ? LOCTEXT("SupportsMultiInput", "Supports multiple inputs.") : LOCTEXT("SingleInputOnly", "Supports only one input.");
		}
		else
		{
			MultiDataSupport = MatchingPin->Properties.bAllowMultipleData ? LOCTEXT("OutputSupportsMultiData", "Can generate multiple data.") : LOCTEXT("OutputSingleDataOnly", "Generates only single data.");
		}
	}

	HoverTextOut = FText::Format(LOCTEXT("PinHoverToolTipFull", "{0}\n\nType: {1}\nSubtype: {2}\nAdditional information: {3}{4}{5}"),
		Description,
		DataTypeText,
		DataSubtypeText,
		Required,
		MultiDataSupport,
		MultiConnectionSupport).ToString();
}

UObject* UPCGEditorGraphNodeBase::GetJumpTargetForDoubleClick() const
{
	if (PCGNode)
	{
		if (UPCGSettings* Settings = PCGNode->GetSettings())
		{
			return Settings->GetJumpTargetForDoubleClick();
		}
	}

	return nullptr;
}

void UPCGEditorGraphNodeBase::OnUpdateCommentText(const FString& NewComment)
{
	Super::OnUpdateCommentText(NewComment);

	if (PCGNode && PCGNode->NodeComment != NewComment)
	{
		PCGNode->Modify();
		PCGNode->NodeComment = NewComment;
	}
}

void UPCGEditorGraphNodeBase::OnCommentBubbleToggled(bool bInCommentBubbleVisible)
{
	Super::OnCommentBubbleToggled(bInCommentBubbleVisible);

	if (PCGNode && PCGNode->bCommentBubbleVisible != bInCommentBubbleVisible)
	{
		PCGNode->Modify();
		PCGNode->bCommentBubbleVisible = bInCommentBubbleVisible;
	}
}

void UPCGEditorGraphNodeBase::OnUserAddDynamicInputPin()
{
	check(PCGNode);
	
	if (UPCGSettingsWithDynamicInputs* DynamicNodeSettings = Cast<UPCGSettingsWithDynamicInputs>(PCGNode->GetSettings()))
	{
		const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorUserAddDynamicInputPin", "Add Source Pin"), DynamicNodeSettings);
		DynamicNodeSettings->Modify();
		DynamicNodeSettings->OnUserAddDynamicInputPin();
	}
}

bool UPCGEditorGraphNodeBase::CanUserRemoveDynamicInputPin(UEdGraphPin* InPinToRemove)
{
	check(PCGNode && InPinToRemove);

	if (UPCGSettingsWithDynamicInputs* DynamicNodeSettings = Cast<UPCGSettingsWithDynamicInputs>(PCGNode->GetSettings()))
	{
		const UPCGEditorGraphNodeBase* PCGGraphNode = CastChecked<const UPCGEditorGraphNodeBase>(InPinToRemove->GetOwningNode());
		return DynamicNodeSettings->CanUserRemoveDynamicInputPin(PCGGraphNode->GetPinIndex(InPinToRemove));
	}
	
	return false;
}

void UPCGEditorGraphNodeBase::OnUserRemoveDynamicInputPin(UEdGraphPin* InRemovedPin)
{
	check(PCGNode && InRemovedPin);

	if (UPCGSettingsWithDynamicInputs* DynamicNodeSettings = Cast<UPCGSettingsWithDynamicInputs>(PCGNode->GetSettings()))
	{
		if (UPCGEditorGraphNodeBase* PCGGraphNode = Cast<UPCGEditorGraphNodeBase>(InRemovedPin->GetOwningNode()))
		{
			const FScopedTransaction Transaction(*FPCGEditorCommon::ContextIdentifier, LOCTEXT("PCGEditorUserRemoveDynamicInputPin", "Remove Source Pin"), DynamicNodeSettings);
			DynamicNodeSettings->Modify();
			DynamicNodeSettings->OnUserRemoveDynamicInputPin(PCGGraphNode->GetPCGNode(), PCGGraphNode->GetPinIndex(InRemovedPin));
		}
	}
}

void UPCGEditorGraphNodeBase::UpdateCommentBubblePinned()
{
	if (PCGNode)
	{
		PCGNode->Modify();
		PCGNode->bCommentBubblePinned = bCommentBubblePinned;
	}
}

void UPCGEditorGraphNodeBase::UpdatePosition()
{
	if (PCGNode)
	{
		PCGNode->Modify();
		PCGNode->PositionX = NodePosX;
		PCGNode->PositionY = NodePosY;
	}
}

bool UPCGEditorGraphNodeBase::ShouldCreatePin(const UPCGPin* InPin) const
{
	return InPin && !InPin->Properties.bInvisiblePin;
}

void UPCGEditorGraphNodeBase::CreatePins(const TArray<UPCGPin*>& InInputPins, const TArray<UPCGPin*>& InOutputPins)
{
	bool bHasAdvancedPin = false;

	for (const UPCGPin* InputPin : InInputPins)
	{
		if (!ShouldCreatePin(InputPin))
		{
			continue;
		}

		UEdGraphPin* Pin = CreatePin(EEdGraphPinDirection::EGPD_Input, GetPinType(InputPin), InputPin->Properties.Label);
		Pin->PinFriendlyName = GetPinFriendlyName(InputPin);
		Pin->bAdvancedView = InputPin->Properties.IsAdvancedPin();
		bHasAdvancedPin |= Pin->bAdvancedView;
	}

	for (const UPCGPin* OutputPin : InOutputPins)
	{
		if (!ShouldCreatePin(OutputPin))
		{
			continue;
		}

		UEdGraphPin* Pin = CreatePin(EEdGraphPinDirection::EGPD_Output, GetPinType(OutputPin), OutputPin->Properties.Label);
		Pin->PinFriendlyName = GetPinFriendlyName(OutputPin);
		Pin->bAdvancedView = OutputPin->Properties.IsAdvancedPin();
		bHasAdvancedPin |= Pin->bAdvancedView;
	}

	if (bHasAdvancedPin && AdvancedPinDisplay == ENodeAdvancedPins::NoPins)
	{
		AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
	}
	else if (!bHasAdvancedPin)
	{
		AdvancedPinDisplay = ENodeAdvancedPins::NoPins;
	}
}

FText UPCGEditorGraphNodeBase::GetPinFriendlyName(const UPCGPin* InPin) const
{
	check(InPin);

	// For overridable params, use the display name of properties (for localized version or overridden display name in metadata).
	if (InPin->Properties.IsAdvancedPin() && InPin->Properties.AllowedTypes == EPCGDataType::Param)
	{
		const UPCGSettings* Settings = InPin->Node ? InPin->Node->GetSettings() : nullptr;
		if (Settings)
		{
			const FPCGSettingsOverridableParam* Param = Settings->OverridableParams().FindByPredicate([Label = InPin->Properties.Label](const FPCGSettingsOverridableParam& ParamToCheck)
			{
				return ParamToCheck.Label == Label;
			});

			if (Param)
			{
				return Param->GetDisplayPropertyPathText();
			}
		}
	}

	return FText::FromString(FName::NameToDisplayString(InPin->Properties.Label.ToString(), /*bIsBool=*/false));
}

#undef LOCTEXT_NAMESPACE
