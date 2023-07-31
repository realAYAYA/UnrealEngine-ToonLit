// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConversationGraphSchema.h"
#include "ConversationGraphTypes.h"
#include "ConversationGraphNode.h"
#include "ConversationGraphConnectionDrawingPolicy.h"

#include "ConversationEntryPointNode.h"
#include "ConversationGraphNode_EntryPoint.h"

#include "ConversationTaskNode.h"
#include "ConversationGraphNode_Task.h"

#include "ConversationRequirementNode.h"
#include "ConversationGraphNode_Requirement.h"

#include "ConversationSideEffectNode.h"
#include "ConversationGraphNode_SideEffect.h"

#include "ConversationChoiceNode.h"
#include "ConversationGraphNode_Choice.h"
#include "ConversationGraphNode_Knot.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditorActions.h"
#include "ToolMenu.h"
#include "ScopedTransaction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ConversationGraphSchema)

#define LOCTEXT_NAMESPACE "ConversationEditor"

TSharedPtr<FGraphNodeClassHelper> ConversationClassCache;

FGraphNodeClassHelper& GetConversationClassCache()
{
	if (!ConversationClassCache.IsValid())
	{
		ConversationClassCache = MakeShareable(new FGraphNodeClassHelper(UConversationNode::StaticClass()));
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UConversationTaskNode::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UConversationEntryPointNode::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UConversationSideEffectNode::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UConversationRequirementNode::StaticClass());
		FGraphNodeClassHelper::AddObservedBlueprintClasses(UConversationChoiceNode::StaticClass());
		ConversationClassCache->UpdateAvailableBlueprintClasses();
	}

	return *ConversationClassCache.Get();
}

//////////////////////////////////////////////////////////////////////
//

UEdGraphNode* FConversationGraphSchemaAction_AutoArrange::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
// 	if (UBehaviorTreeGraph* Graph = Cast<UBehaviorTreeGraph>(ParentGraph))
// 	{
// 		Graph->AutoArrange();
// 	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////
// UConversationGraphSchema

int32 UConversationGraphSchema::CurrentCacheRefreshID = 0;

UConversationGraphSchema::UConversationGraphSchema(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UConversationGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	//@TODO: CONVERSATION: Add an entry point by default
// 	FGraphNodeCreator<UConversationGraphNode_EntryPoint> NodeCreator(Graph);
// 	UConversationGraphNode_EntryPoint* MyNode = NodeCreator.CreateNode();
// 	NodeCreator.Finalize();
// 	SetNodeMetaData(MyNode, FNodeMetadata::DefaultGraphNode);
}

void UConversationGraphSchema::GetGraphNodeContextActions(FGraphContextMenuBuilder& ContextMenuBuilder, int32 SubNodeFlags) const
{
	Super::GetGraphNodeContextActions(ContextMenuBuilder, SubNodeFlags);
}

void UConversationGraphSchema::GetSubNodeClasses(int32 SubNodeFlags, TArray<FGraphNodeClassData>& ClassData, UClass*& GraphNodeClass) const
{
	FGraphNodeClassHelper& ClassCache = GetConversationClassCache();

	switch ((EConversationGraphSubNodeType)SubNodeFlags)
	{
	case EConversationGraphSubNodeType::Requirement:
		ClassCache.GatherClasses(UConversationRequirementNode::StaticClass(), /*out*/ ClassData);
		GraphNodeClass = UConversationGraphNode_Requirement::StaticClass();
		break;
	case EConversationGraphSubNodeType::SideEffect:
		ClassCache.GatherClasses(UConversationSideEffectNode::StaticClass(), /*out*/ ClassData);
		GraphNodeClass = UConversationGraphNode_SideEffect::StaticClass();
		break;
	case EConversationGraphSubNodeType::Choice:
		ClassCache.GatherClasses(UConversationChoiceNode::StaticClass(), /*out*/ ClassData);
		GraphNodeClass = UConversationGraphNode_Choice::StaticClass();
		break;
	default:
		unimplemented();
	}
}

void UConversationGraphSchema::AddConversationNodeOptions(const FString& CategoryName, FGraphContextMenuBuilder& ContextMenuBuilder, TSubclassOf<UConversationNode> RuntimeNodeType, TSubclassOf<UConversationGraphNode> EditorNodeType) const
{
	FCategorizedGraphActionListBuilder ListBuilder(CategoryName);

	TArray<FGraphNodeClassData> NodeClasses;
	GetConversationClassCache().GatherClasses(RuntimeNodeType, /*out*/ NodeClasses);

	for (const FGraphNodeClassData& NodeClass : NodeClasses)
	{
		const FText NodeTypeName = FText::FromString(FName::NameToDisplayString(NodeClass.ToString(), false));

		TSharedPtr<FAISchemaAction_NewNode> AddOpAction = UAIGraphSchema::AddNewNodeAction(ListBuilder, NodeClass.GetCategory(), NodeTypeName, FText::GetEmpty());

		UConversationGraphNode* OpNode = NewObject<UConversationGraphNode>(ContextMenuBuilder.OwnerOfTemporaries, EditorNodeType);
		OpNode->ClassData = NodeClass;
		AddOpAction->NodeTemplate = OpNode;
	}

	ContextMenuBuilder.Append(ListBuilder);
}

void UConversationGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const FName PinCategory = ContextMenuBuilder.FromPin ?
		ContextMenuBuilder.FromPin->PinType.PinCategory :
		UConversationGraphTypes::PinCategory_MultipleNodes;

	const bool bNoParent = (ContextMenuBuilder.FromPin == NULL);
	const bool bOnlyTasks = (PinCategory == UConversationGraphTypes::PinCategory_SingleTask);
	const bool bOnlyComposites = (PinCategory == UConversationGraphTypes::PinCategory_SingleComposite);
	const bool bAllowComposites = bNoParent || !bOnlyTasks || bOnlyComposites;
	const bool bAllowTasks = bNoParent || !bOnlyComposites || bOnlyTasks;

	FGraphNodeClassHelper& ClassCache = GetConversationClassCache();

	if (bAllowTasks)
	{
		AddConversationNodeOptions(TEXT("Tasks"), ContextMenuBuilder, UConversationTaskNode::StaticClass(), UConversationGraphNode_Task::StaticClass());
	}

	if (bNoParent || (ContextMenuBuilder.FromPin && (ContextMenuBuilder.FromPin->Direction == EGPD_Input)))
	{
		AddConversationNodeOptions(TEXT("Entry Point"), ContextMenuBuilder, UConversationEntryPointNode::StaticClass(), UConversationGraphNode_EntryPoint::StaticClass());
	}

	if (bNoParent)
	{
		TSharedPtr<FConversationGraphSchemaAction_AutoArrange> Action( new FConversationGraphSchemaAction_AutoArrange(FText::GetEmpty(), LOCTEXT("AutoArrange", "Auto Arrange"), FText::GetEmpty(), 0) );
		ContextMenuBuilder.AddAction(Action);
	}
}

void UConversationGraphSchema::GetContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (Context->Node && !Context->Pin)
	{
		const UConversationGraphNode* ConversationGraphNode = Cast<const UConversationGraphNode>(Context->Node);
		if (ConversationGraphNode && ConversationGraphNode->CanPlaceBreakpoints())
		{
			FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaBreakpoints", LOCTEXT("BreakpointsHeader", "Breakpoints"));
			Section.AddMenuEntry(FGraphEditorCommands::Get().ToggleBreakpoint);
			Section.AddMenuEntry(FGraphEditorCommands::Get().AddBreakpoint);
			Section.AddMenuEntry(FGraphEditorCommands::Get().RemoveBreakpoint);
			Section.AddMenuEntry(FGraphEditorCommands::Get().EnableBreakpoint);
			Section.AddMenuEntry(FGraphEditorCommands::Get().DisableBreakpoint);
		}
	}

	Super::GetContextMenuActions(Menu, Context);
}

const FPinConnectionResponse UConversationGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	if (PinA == nullptr || PinB == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinNull", "One or Both of the pins was null"));
	}

	// Make sure the pins are not on the same node
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorSameNode", "Both are on the same node"));
	}

	const bool bPinAIsSingleComposite = (PinA->PinType.PinCategory == UConversationGraphTypes::PinCategory_SingleComposite);
	const bool bPinAIsSingleTask = (PinA->PinType.PinCategory == UConversationGraphTypes::PinCategory_SingleTask);
	const bool bPinAIsSingleNode = (PinA->PinType.PinCategory == UConversationGraphTypes::PinCategory_SingleNode);

	const bool bPinBIsSingleComposite = (PinB->PinType.PinCategory == UConversationGraphTypes::PinCategory_SingleComposite);
	const bool bPinBIsSingleTask = (PinB->PinType.PinCategory == UConversationGraphTypes::PinCategory_SingleTask);
	const bool bPinBIsSingleNode = (PinB->PinType.PinCategory == UConversationGraphTypes::PinCategory_SingleNode);

	const bool bPinAIsTask = PinA->GetOwningNode()->IsA(UConversationGraphNode_Task::StaticClass());
	const bool bPinAIsComposite = false;// PinA->GetOwningNode()->IsA(UConversationGraphNode_Composite::StaticClass());

	const bool bPinBIsTask = PinB->GetOwningNode()->IsA(UConversationGraphNode_Task::StaticClass());
	const bool bPinBIsComposite = false;// PinB->GetOwningNode()->IsA(UConversationGraphNode_Composite::StaticClass());

	if ((bPinAIsSingleComposite && !bPinBIsComposite) || (bPinBIsSingleComposite && !bPinAIsComposite))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOnlyComposite", "Only composite nodes are allowed"));
	}

	if ((bPinAIsSingleTask && !bPinBIsTask) || (bPinBIsSingleTask && !bPinAIsTask))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOnlyTask", "Only task nodes are allowed"));
	}

	// Compare the directions
	if ((PinA->Direction == EGPD_Input) && (PinB->Direction == EGPD_Input))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorInput", "Can't connect input node to input node"));
	}
	else if ((PinB->Direction == EGPD_Output) && (PinA->Direction == EGPD_Output))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorOutput", "Can't connect output node to output node"));
	}

	class FNodeVisitorCycleChecker
	{
	public:
		/** Check whether a loop in the graph would be caused by linking the passed-in nodes */
		bool CheckForLoop(UEdGraphNode* StartNode, UEdGraphNode* EndNode)
		{
			VisitedNodes.Add(EndNode);
			return TraverseInputNodesToRoot(StartNode);
		}

	private:
		/**
		 * Helper function for CheckForLoop()
		 * @param	Node	The node to start traversal at
		 * @return true if we reached a root node (i.e. a node with no input pins), false if we encounter a node we have already seen
		 */
		bool TraverseInputNodesToRoot(UEdGraphNode* Node)
		{
			VisitedNodes.Add(Node);

			// Follow every input pin until we cant any more ('root') or we reach a node we have seen (cycle)
			for (int32 PinIndex = 0; PinIndex < Node->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* MyPin = Node->Pins[PinIndex];

				if (MyPin->Direction == EGPD_Input)
				{
					for (int32 LinkedPinIndex = 0; LinkedPinIndex < MyPin->LinkedTo.Num(); ++LinkedPinIndex)
					{
						UEdGraphPin* OtherPin = MyPin->LinkedTo[LinkedPinIndex];
						if (OtherPin)
						{
							UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
							if (VisitedNodes.Contains(OtherNode))
							{
								return false;
							}
							else
							{
								return TraverseInputNodesToRoot(OtherNode);
							}
						}
					}
				}
			}

			return true;
		}

		TSet<UEdGraphNode*> VisitedNodes;
	};

	// check for cycles
	FNodeVisitorCycleChecker CycleChecker;
	if (!CycleChecker.CheckForLoop(PinA->GetOwningNode(), PinB->GetOwningNode()))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, LOCTEXT("PinErrorcycle", "Can't create a graph cycle"));
	}

	const bool bPinASingleLink = bPinAIsSingleComposite || bPinAIsSingleTask || bPinAIsSingleNode;
	const bool bPinBSingleLink = bPinBIsSingleComposite || bPinBIsSingleTask || bPinBIsSingleNode;

	if (PinB->Direction == EGPD_Input && PinB->LinkedTo.Num() > 0)
	{
		if (bPinASingleLink)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
		else
		{
			//@TODO: CONVERSATION: I think this is safe...
			//return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
	}
	else if (PinA->Direction == EGPD_Input && PinA->LinkedTo.Num() > 0)
	{
		if (bPinBSingleLink)
		{
			return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_AB, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
		else
		{
			//@TODO: CONVERSATION: I think this is safe...
			//return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("PinConnectReplace", "Replace connection"));
		}
	}

	if (bPinASingleLink && PinA->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_A, LOCTEXT("PinConnectReplace", "Replace connection"));
	}
	else if (bPinBSingleLink && PinB->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B, LOCTEXT("PinConnectReplace", "Replace connection"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, LOCTEXT("PinConnect", "Connect nodes"));
}

const FPinConnectionResponse UConversationGraphSchema::CanMergeNodes(const UEdGraphNode* NodeA, const UEdGraphNode* NodeB) const
{
	// Make sure the nodes are not the same 
	if (NodeA == NodeB)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT("Both are the same node"));
	}

	const bool bIsSubnode_A = Cast<UConversationGraphNode>(NodeA) && Cast<UConversationGraphNode>(NodeA)->IsSubNode();
	const bool bIsSubnode_B = Cast<UConversationGraphNode>(NodeB) && Cast<UConversationGraphNode>(NodeB)->IsSubNode();
	const bool bIsTask_B = NodeB->IsA(UConversationGraphNode_Task::StaticClass());

	if (bIsSubnode_A && (bIsSubnode_B || bIsTask_B))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, TEXT(""));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, TEXT(""));
}

void UConversationGraphSchema::OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const
{
	const FScopedTransaction Transaction(LOCTEXT("CreateRerouteNodeOnWire", "Create Reroute Node"));

	//@TODO: This constant is duplicated from inside of SGraphNodeKnot
	const FVector2D NodeSpacerSize(42.0f, 24.0f);
	const FVector2D KnotTopLeft = GraphPosition - (NodeSpacerSize * 0.5f);

	// Create a new knot
	UEdGraph* OwningGraph = PinA->GetOwningNode()->GetGraph();

	if (ensure(OwningGraph))
	{
		FGraphNodeCreator<UConversationGraphNode_Knot> NodeCreator(*OwningGraph);
		UConversationGraphNode_Knot* MyNode = NodeCreator.CreateNode();
		MyNode->NodePosX = KnotTopLeft.X;
		MyNode->NodePosY = KnotTopLeft.Y;
		//MyNode->SnapToGrid(SNAP_GRID);
	 	NodeCreator.Finalize();

		//UK2Node_Knot* NewKnot = FEdGraphSchemaAction_K2NewNode::SpawnNode<UK2Node_Knot>(ParentGraph, KnotTopLeft, EK2NewNodeFlags::SelectNewNode);

		// Move the connections across (only notifying the knot, as the other two didn't really change)
		PinA->BreakLinkTo(PinB);
		PinA->MakeLinkTo((PinA->Direction == EGPD_Output) ? CastChecked<UConversationGraphNode_Knot>(MyNode)->GetInputPin() : CastChecked<UConversationGraphNode_Knot>(MyNode)->GetOutputPin());
		PinB->MakeLinkTo((PinB->Direction == EGPD_Output) ? CastChecked<UConversationGraphNode_Knot>(MyNode)->GetInputPin() : CastChecked<UConversationGraphNode_Knot>(MyNode)->GetOutputPin());
	}
}

FLinearColor UConversationGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FColor::White;
}

class FConnectionDrawingPolicy* UConversationGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FConversationGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

bool UConversationGraphSchema::IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const
{
	return CurrentCacheRefreshID != InVisualizationCacheID;
}

int32 UConversationGraphSchema::GetCurrentVisualizationCacheID() const
{
	return CurrentCacheRefreshID;
}

void UConversationGraphSchema::ForceVisualizationCacheClear() const
{
	++CurrentCacheRefreshID;
}

//////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

