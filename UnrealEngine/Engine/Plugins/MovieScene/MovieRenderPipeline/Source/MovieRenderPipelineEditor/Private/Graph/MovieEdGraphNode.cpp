// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieEdGraphNode.h"

#include "Graph/MovieGraphPin.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieGraphConfig.h"
#include "EdGraph/EdGraphPin.h"
#include "Misc/TransactionObjectEvent.h"
#include "MovieEdGraph.h"
#include "MovieGraphSchema.h"
#include "PropertyBag.h"
#include "ToolMenu.h"
#include "EdGraph/EdGraphSchema.h"
#include "GraphEditorActions.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "MoviePipelineEdGraphNodeBase"

void UMoviePipelineEdGraphNodeBase::Construct(UMovieGraphNode* InRuntimeNode)
{
	check(InRuntimeNode);
	RuntimeNode = InRuntimeNode;
	RuntimeNode->GraphNode = this;
	RuntimeNode->OnNodeChangedDelegate.AddUObject(this, &UMoviePipelineEdGraphNodeBase::OnRuntimeNodeChanged);
	
	NodePosX = InRuntimeNode->GetNodePosX();
	NodePosY = InRuntimeNode->GetNodePosY();
	
	NodeComment = InRuntimeNode->GetNodeComment();
	bCommentBubblePinned = InRuntimeNode->IsCommentBubblePinned();
	bCommentBubbleVisible = InRuntimeNode->IsCommentBubbleVisible();
}

void UMoviePipelineEdGraphNodeBase::PostTransacted(const FTransactionObjectEvent& TransactionEvent)
{
	const TArray<FName> ChangedProperties = TransactionEvent.GetChangedProperties();

	if (ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(UEdGraphNode, NodePosX)) ||
		ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(UEdGraphNode, NodePosY)))
	{
		UpdatePosition();
	}

	if (ChangedProperties.Contains(GET_MEMBER_NAME_CHECKED(UEdGraphNode, bCommentBubblePinned)))
	{
		UpdateCommentBubblePinned();
	}
}

FEdGraphPinType UMoviePipelineEdGraphNodeBase::GetPinType(EMovieGraphValueType ValueType, bool bIsBranch)
{
	FEdGraphPinType EdPinType;
	EdPinType.ResetToDefaults();
	
	EdPinType.PinCategory = NAME_None;
	EdPinType.PinSubCategory = NAME_None;

	// Special case for branch pins
	if (bIsBranch)
	{
		EdPinType.PinCategory = UMovieGraphSchema::PC_Branch;
		return EdPinType;
	}

	switch (ValueType)
	{
	case EMovieGraphValueType::Bool:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Boolean;
		break;
	case EMovieGraphValueType::Byte:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Byte;
		break;
	case EMovieGraphValueType::Int32:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Integer;
		break;
	case EMovieGraphValueType::Int64:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Int64;
		break;
	case EMovieGraphValueType::Float:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Float;
		break;
	case EMovieGraphValueType::Double:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Double;
		break;
	case EMovieGraphValueType::Name:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Name;
		break;
	case EMovieGraphValueType::String:
		EdPinType.PinCategory = UMovieGraphSchema::PC_String;
		break;
	case EMovieGraphValueType::Text:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Text;
		break;
	case EMovieGraphValueType::Enum:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Enum;
		break;
	case EMovieGraphValueType::Struct:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Struct;
		break;
	case EMovieGraphValueType::Object:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Object;
		break;
	case EMovieGraphValueType::SoftObject:
		EdPinType.PinCategory = UMovieGraphSchema::PC_SoftObject;
		break;
	case EMovieGraphValueType::Class:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Class;
		break;
	case EMovieGraphValueType::SoftClass:
		EdPinType.PinCategory = UMovieGraphSchema::PC_SoftClass;
		break;
	default:
		EdPinType.PinCategory = UMovieGraphSchema::PC_Float;
		break;
	}
	
	return EdPinType;
}

FEdGraphPinType UMoviePipelineEdGraphNodeBase::GetPinType(const UMovieGraphPin* InPin)
{
	return GetPinType(InPin->Properties.Type, InPin->Properties.bIsBranch);
}

void UMoviePipelineEdGraphNodeBase::UpdatePosition() const
{
	if (RuntimeNode)
	{
		RuntimeNode->Modify();
		RuntimeNode->SetNodePosX(NodePosX);
		RuntimeNode->SetNodePosY(NodePosY);
	}
}

void UMoviePipelineEdGraphNodeBase::UpdateCommentBubblePinned() const
{
	if (RuntimeNode)
	{
		RuntimeNode->Modify();
		RuntimeNode->SetIsCommentBubblePinned(bCommentBubblePinned);
	}
}

void UMoviePipelineEdGraphNode::AllocateDefaultPins()
{
	if(RuntimeNode)
	{
		for(const UMovieGraphPin* InputPin : RuntimeNode->GetInputPins())
		{
			CreatePin(EEdGraphPinDirection::EGPD_Input, GetPinType(InputPin), InputPin->Properties.Label);
		}
		
		for(const UMovieGraphPin* OutputPin : RuntimeNode->GetOutputPins())
		{
			CreatePin(EEdGraphPinDirection::EGPD_Output, GetPinType(OutputPin), OutputPin->Properties.Label);
		}
	}
}

FText UMoviePipelineEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (RuntimeNode)
	{
		const bool bGetDescriptive = true;
		return RuntimeNode->GetNodeTitle(bGetDescriptive);
	}

	return LOCTEXT("UnknownNodeTitle", "Unknown");
}

FText UMoviePipelineEdGraphNode::GetTooltipText() const
{
	// Return the UObject name for now for debugging purposes
	return FText::FromString(GetName());
}

void UMoviePipelineEdGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);
	
	if (!Context->Node || !RuntimeNode)
	{
		return;
	}

	GetPropertyPromotionContextMenuActions(Menu, Context);
}

void UMoviePipelineEdGraphNode::GetPropertyPromotionContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	FToolMenuSection& Section = Menu->AddSection("MoviePipelineGraphExposeAsPin", LOCTEXT("ExposeAsPin", "Expose Property as Pin"));

	const TArray<FMovieGraphPropertyInfo>& OverrideablePropertyInfo = RuntimeNode->GetOverrideablePropertyInfo();
	for (const FMovieGraphPropertyInfo& PropertyInfo : OverrideablePropertyInfo)
	{
		Section.AddMenuEntry(
			PropertyInfo.Name,
			FText::FromName(PropertyInfo.Name),
			LOCTEXT("PromotePropertyToPin", "Promote this property to a pin on this node."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateUObject(this, &UMoviePipelineEdGraphNode::TogglePromotePropertyToPin, PropertyInfo.Name),
				FCanExecuteAction(),
				FGetActionCheckState::CreateLambda([this, PropertyInfo]()
				{
					const TArray<FMovieGraphPropertyInfo>& ExposedProperties = RuntimeNode->GetExposedProperties();
					return ExposedProperties.Contains(PropertyInfo) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})),
			EUserInterfaceActionType::ToggleButton
		);
	}

	if (OverrideablePropertyInfo.IsEmpty())
	{
		Section.AddMenuEntry(
			"NoPropertiesAvailable",
			FText::FromString("No properties available"),
			LOCTEXT("PromotePropertyToPin_NoneAvailable", "No properties are available to promote."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction(),
				FCanExecuteAction::CreateLambda([]() { return false; }))
		);
	}
}

void UMoviePipelineEdGraphNode::TogglePromotePropertyToPin(const FName PropertyName) const
{
	RuntimeNode->TogglePromotePropertyToPin(PropertyName);
}

bool UMoviePipelineEdGraphNodeBase::ShouldCreatePin(const UMovieGraphPin* InPin) const
{
	return true;
}

void UMoviePipelineEdGraphNodeBase::CreatePins(const TArray<UMovieGraphPin*>& InInputPins, const TArray<UMovieGraphPin*>& InOutputPins)
{
	bool bHasAdvancedPin = false;

	for (const UMovieGraphPin* InputPin : InInputPins)
	{
		if (!ShouldCreatePin(InputPin))
		{
			continue;
		}

		UEdGraphPin* Pin = CreatePin(EGPD_Input, GetPinType(InputPin), InputPin->Properties.Label);
		// Pin->bAdvancedView = InputPin->Properties.bAdvancedPin;
		bHasAdvancedPin |= Pin->bAdvancedView;
	}

	for (const UMovieGraphPin* OutputPin : InOutputPins)
	{
		if (!ShouldCreatePin(OutputPin))
		{
			continue;
		}

		UEdGraphPin* Pin = CreatePin(EGPD_Output, GetPinType(OutputPin), OutputPin->Properties.Label);
		// Pin->bAdvancedView = OutputPin->Properties.bAdvancedPin;
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

void UMoviePipelineEdGraphNodeBase::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (RuntimeNode == nullptr || FromPin == nullptr)
	{
		return;
	}

	const bool bFromPinIsInput = FromPin->Direction == EEdGraphPinDirection::EGPD_Input;
	const TArray<TObjectPtr<UMovieGraphPin>>& OtherPinsList = bFromPinIsInput ? RuntimeNode->GetOutputPins() : RuntimeNode->GetInputPins();

	// Try to connect to the first compatible pin
	bool bDidAutoconnect = false;
	for (const TObjectPtr<UMovieGraphPin>& OtherPin : OtherPinsList)
	{
		check(OtherPin);

		const FName& OtherPinName = OtherPin->Properties.Label;
		UEdGraphPin* ToPin = FindPinChecked(OtherPinName, bFromPinIsInput ? EEdGraphPinDirection::EGPD_Output : EEdGraphPinDirection::EGPD_Input);
		if (ToPin && GetSchema()->TryCreateConnection(FromPin, ToPin))
		{
			// Connection succeeded. Notify our other node that their connections changed.
			if (ToPin->GetOwningNode())
			{
				ToPin->GetOwningNode()->NodeConnectionListChanged();
			}
			bDidAutoconnect = true;
			break;
		}
	}

	// Notify ourself of the connection list changing too.
	if (bDidAutoconnect)
	{
		NodeConnectionListChanged();
	}
}

FLinearColor UMoviePipelineEdGraphNodeBase::GetNodeTitleColor() const
{
	if (RuntimeNode)
	{
		return RuntimeNode->GetNodeTitleColor();
	}

	return FLinearColor::Black;
}

FSlateIcon UMoviePipelineEdGraphNodeBase::GetIconAndTint(FLinearColor& OutColor) const
{
	if (RuntimeNode)
	{
		return RuntimeNode->GetIconAndTint(OutColor);
	}

	OutColor = FLinearColor::White;
	return FSlateIcon();
}

bool UMoviePipelineEdGraphNodeBase::ShowPaletteIconOnNode() const
{
	// Reveals the icon set by GetIconAndTint() in the top-left corner of the node
	return true;
}

void UMoviePipelineEdGraphNodeBase::OnUpdateCommentText(const FString& NewComment)
{
	Super::OnUpdateCommentText(NewComment);

	if (RuntimeNode && (RuntimeNode->GetNodeComment() != NewComment))
	{
		RuntimeNode->SetNodeComment(NewComment);
	}
}

void UMoviePipelineEdGraphNodeBase::OnCommentBubbleToggled(bool bInCommentBubbleVisible)
{
	Super::OnCommentBubbleToggled(bInCommentBubbleVisible);

	if (RuntimeNode && (RuntimeNode->IsCommentBubbleVisible() != bInCommentBubbleVisible))
	{
		RuntimeNode->SetIsCommentBubbleVisible(bInCommentBubbleVisible);
	}
}

void UMoviePipelineEdGraphNodeBase::OnRuntimeNodeChanged(const UMovieGraphNode* InChangedNode)
{
	if (InChangedNode == GetRuntimeNode())
	{
		ReconstructNode();
	}
}

void UMoviePipelineEdGraphNodeBase::ReconstructNode()
{
	// Don't reconstruct the node during copy-paste. If we allow reconstruction,
	// then the editor graph reconstructs connections to previous nodes that were
	// not included in the copy/paste. This does not affect connections within the copy/pasted nodes.
	if (bDisableReconstructNode)
	{
		return;
	}

	ReconstructPins();

	UMoviePipelineEdGraph* Graph = CastChecked<UMoviePipelineEdGraph>(GetGraph());
	
	// Reconstruct connections
	const bool bCreateInbound = true;
	const bool bCreateOutbound = true;
	Graph->CreateLinks(this, bCreateInbound, bCreateOutbound);

	Graph->NotifyGraphChanged();
}

void UMoviePipelineEdGraphNodeBase::ReconstructPins()
{
	// Store copy of old pins
	TArray<UEdGraphPin*> OldPins = MoveTemp(Pins);
	Pins.Reset();
	
	// Generate new pins
	CreatePins(RuntimeNode->GetInputPins(), RuntimeNode->GetOutputPins());
	
	// Transfer persistent data from old to new pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		for (UEdGraphPin* NewPin : Pins)
		{
			if ((OldPin->PinName == NewPin->PinName) && (OldPin->Direction == NewPin->Direction))
			{
				// Remove invalid entries
				OldPin->LinkedTo.Remove(nullptr);

				NewPin->MovePersistentDataFromOldPin(*OldPin);
				break;
			}
		}
	}
	
	// Remove old pins
	for (UEdGraphPin* OldPin : OldPins)
	{
		OldPin->BreakAllPinLinks();
		OldPin->SubPins.Remove(nullptr);
		DestroyPin(OldPin);
	}
}

void UMoviePipelineEdGraphNodeBase::PrepareForCopying()
{
	if (RuntimeNode)
	{
		// Temporarily take ownership of the model's node, so that it is not deleted when copying.
		// This is restored in PostCopy
		RuntimeNode->Rename(nullptr, this, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void UMoviePipelineEdGraphNodeBase::PostCopy()
{
	if (RuntimeNode)
	{
		// We briefly took ownership of the runtime node to create the copy/paste buffer,
		// restore the ownership back to the owning graph.
		UMoviePipelineEdGraph* MovieGraphEditorGraph = CastChecked<UMoviePipelineEdGraph>(GetGraph());
		UMovieGraphConfig* RuntimeGraph = MovieGraphEditorGraph->GetPipelineGraph();
		check(RuntimeGraph);
		RuntimeNode->Rename(nullptr, RuntimeGraph, REN_DontCreateRedirectors | REN_DoNotDirty);
	}
}

void UMoviePipelineEdGraphNodeBase::PostPasteNode()
{
	bDisableReconstructNode = true;
}

void UMoviePipelineEdGraphNodeBase::PostPaste()
{
	if (RuntimeNode)
	{
		// The editor nodes preserved the connections between nodes when copying/pasting
		// but we intentionally don't preserve the edges of the runtime graph when copying
		// (as the ownership isn't always clear given both input/output edges, which node owns
		// the edge, the one inside the copied graph? Or the one outside it?), so instead 
		// we just rebuild the runtime edge connections based on the editor graph connectivity. 
		RebuildRuntimeEdgesFromPins();

		// Ensure we're listening to the delegate for this pasted node, because we may have skipped ::Construct
		RuntimeNode->OnNodeChangedDelegate.AddUObject(this, &UMoviePipelineEdGraphNodeBase::OnRuntimeNodeChanged);
		RuntimeNode->SetNodePosX(NodePosX);
		RuntimeNode->SetNodePosY(NodePosY);
	}

	bDisableReconstructNode = false;
}

void UMoviePipelineEdGraphNodeBase::RebuildRuntimeEdgesFromPins()
{
	check(RuntimeNode);
	
	for (UEdGraphPin* Pin : Pins)
	{
		// For each of our output pins, find the other editor node it's connected to, then
		// translate that to runtime components and reconnect the runtime components. We only do
		// the output side because it creates a two-way connection, and we're not worried about
		// the nodes outside the copy/pasted nodes, as we won't have reconstructed the connection to them
		// (so the resulting pasted nodes have no connection outside their "island" of copy/pasted nodes)
		if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
		{
			for (UEdGraphPin* LinkedToPin : Pin->LinkedTo)
			{
				UEdGraphNode* ConnectedEdGraphNode = LinkedToPin->GetOwningNode();
				UMoviePipelineEdGraphNodeBase* ConnectedMovieGraphNode = CastChecked<UMoviePipelineEdGraphNodeBase>(ConnectedEdGraphNode);
				
				if (UMovieGraphNode* ConnectedRuntimeNode = ConnectedMovieGraphNode->GetRuntimeNode())
				{
					UMovieGraphConfig* Graph = RuntimeNode->GetGraph();
					check(Graph);

					Graph->AddLabeledEdge(RuntimeNode, Pin->PinName, ConnectedRuntimeNode, LinkedToPin->PinName);
				}
			}
		}
	}

}

void UMoviePipelineEdGraphNodeBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	if (!Context->Node)
	{
		return;
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaNodeActions", LOCTEXT("NodeActionsHeader", "Node Actions"));
		Section.AddMenuEntry(FGraphEditorCommands::Get().BreakNodeLinks);
		Section.AddMenuEntry(FGenericCommands::Get().Delete);
		Section.AddMenuEntry(FGenericCommands::Get().Cut);
		Section.AddMenuEntry(FGenericCommands::Get().Copy);
		Section.AddMenuEntry(FGenericCommands::Get().Duplicate);
	}

	{
		FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaOrganization", LOCTEXT("OrganizationHeader", "Organization"));
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
}

#undef LOCTEXT_NAMESPACE