// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphSchema.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/Nodes/MovieGraphInputNode.h"
#include "Graph/Nodes/MovieGraphOutputNode.h"
#include "Graph/Nodes/MovieGraphSubgraphNode.h"
#include "Graph/Nodes/MovieGraphVariableNode.h"
#include "Graph/MovieGraphNode.h"
#include "Graph/MovieEdGraph.h"
#include "Graph/MovieEdGraphConnectionPolicy.h"
#include "Graph/MovieEdGraphNode.h"
#include "UObject/Class.h"
#include "UObject/UObjectIterator.h"
#include "ScopedTransaction.h"
#include "GraphEditor.h"
#include "MovieEdGraphVariableNode.h"

TArray<UClass*> UMovieGraphSchema::MoviePipelineNodeClasses;

#define LOCTEXT_NAMESPACE "MoviePipelineGraphSchema"

const FName UMovieGraphSchema::PC_Branch(TEXT("branch"));
const FName UMovieGraphSchema::PC_Boolean(TEXT("boolean"));
const FName UMovieGraphSchema::PC_Byte(TEXT("byte"));
const FName UMovieGraphSchema::PC_Integer(TEXT("integer"));
const FName UMovieGraphSchema::PC_Int64(TEXT("int64"));
const FName UMovieGraphSchema::PC_Float(TEXT("float"));
const FName UMovieGraphSchema::PC_Double(TEXT("double"));
const FName UMovieGraphSchema::PC_Name(TEXT("name"));
const FName UMovieGraphSchema::PC_String(TEXT("string"));
const FName UMovieGraphSchema::PC_Text(TEXT("text"));
const FName UMovieGraphSchema::PC_Enum(TEXT("enum"));
const FName UMovieGraphSchema::PC_Struct(TEXT("struct"));
const FName UMovieGraphSchema::PC_Object(TEXT("object"));
const FName UMovieGraphSchema::PC_SoftObject(TEXT("softobject"));
const FName UMovieGraphSchema::PC_Class(TEXT("class"));
const FName UMovieGraphSchema::PC_SoftClass(TEXT("softclass"));

namespace UE::MovieGraph::Private
{
	UMovieGraphNode* GetGraphNodeFromEdPin(const UEdGraphPin* InPin)
	{
		const UMoviePipelineEdGraphNodeBase* EdGraphNode = CastChecked<UMoviePipelineEdGraphNodeBase>(InPin->GetOwningNode());

		UMovieGraphNode* RuntimeNode = EdGraphNode->GetRuntimeNode();
		check(RuntimeNode);

		return RuntimeNode;
	}

	UMovieGraphPin* GetGraphPinFromEdPin(const UEdGraphPin* InPin)
	{
		const UMovieGraphNode* GraphNode = GetGraphNodeFromEdPin(InPin);
		UMovieGraphPin* GraphPin = (InPin->Direction == EGPD_Input) ? GraphNode->GetInputPin(InPin->PinName) : GraphNode->GetOutputPin(InPin->PinName);
		check(GraphPin);

		return GraphPin;
	}
	
	UMovieGraphConfig* GetGraphFromEdPin(const UEdGraphPin* InPin)
	{
		const UMovieGraphNode* RuntimeNode = GetGraphNodeFromEdPin(InPin);

		UMovieGraphConfig* RuntimeGraph = RuntimeNode->GetGraph();
		check(RuntimeGraph);

		return RuntimeGraph;
	}
}

void UMovieGraphSchema::CreateDefaultNodesForGraph(UEdGraph& Graph) const
{
	/*UMovieGraphConfig* RuntimeGraph = CastChecked<UMoviePipelineEdGraph>(&Graph)->GetPipelineGraph();
	const FScopedTransaction Transaction(LOCTEXT("GraphEditor_NewNode", "Create Pipeline Graph Node."));
	RuntimeGraph->Modify();
	const bool bSelectNewNode = false;

	// Input Node
	{
		UMovieGraphNode* RuntimeNode = RuntimeGraph->ConstructRuntimeNode<UMovieGraphInputNode>();

		// Now create the editor graph node
		FGraphNodeCreator<UMoviePipelineEdGraphNodeInput> NodeCreator(Graph);
		UMoviePipelineEdGraphNodeBase* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
		GraphNode->SetRuntimeNode(RuntimeNode);
		NodeCreator.Finalize();
	}

	// Output Node
	{
		UMovieGraphNode* RuntimeNode = RuntimeGraph->ConstructRuntimeNode<UMovieGraphOutputNode>();

		// Now create the editor graph node
		FGraphNodeCreator<UMoviePipelineEdGraphNodeOutput> NodeCreator(Graph);
		UMoviePipelineEdGraphNodeBase* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
		GraphNode->SetRuntimeNode(RuntimeNode);
		NodeCreator.Finalize();
	}*/
}

void UMovieGraphSchema::InitMoviePipelineNodeClasses()
{
	if (MoviePipelineNodeClasses.Num() > 0)
	{
		return;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UMovieGraphNode::StaticClass())
			&& !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			MoviePipelineNodeClasses.Add(*It);
		}
	}

	MoviePipelineNodeClasses.Sort();
}

bool UMovieGraphSchema::IsConnectionToBranchAllowed(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin, FText& OutError) const
{
	UMovieGraphNode* ToNode = UE::MovieGraph::Private::GetGraphNodeFromEdPin(InputPin);
	UMovieGraphNode* FromNode = UE::MovieGraph::Private::GetGraphNodeFromEdPin(OutputPin);
	const UMovieGraphPin* ToPin = UE::MovieGraph::Private::GetGraphPinFromEdPin(InputPin);
	const UMovieGraphPin* FromPin = UE::MovieGraph::Private::GetGraphPinFromEdPin(OutputPin);
	const UMovieGraphConfig* GraphConfig = UE::MovieGraph::Private::GetGraphFromEdPin(InputPin);

	// Get all upstream/downstream nodes that occur on the connection -- these are the nodes that need to be checked for branch restrictions.
	// FromNode/ToNode themselves also needs to be part of the validation checks.
	TArray<UMovieGraphNode*> NodesToCheck = {FromNode, ToNode};
	GraphConfig->VisitUpstreamNodes(FromNode, UMovieGraphConfig::FVisitNodesCallback::CreateLambda(
		[&NodesToCheck](UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
		{
			NodesToCheck.Add(VisitedNode);
		}));

	GraphConfig->VisitDownstreamNodes(ToNode, UMovieGraphConfig::FVisitNodesCallback::CreateLambda(
		[&NodesToCheck](UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
		{
			NodesToCheck.Add(VisitedNode);
		}));

	// Determine which branch(es) are connected to this node up/downstream.
	const TArray<FString> DownstreamBranchNames = GraphConfig->GetDownstreamBranchNames(ToNode, ToPin);
	const TArray<FString> UpstreamBranchNames = GraphConfig->GetUpstreamBranchNames(FromNode, FromPin);
	const bool bGlobalsIsDownstream = DownstreamBranchNames.Contains(UMovieGraphNode::GlobalsPinNameString);
	const bool bGlobalsIsUpstream = UpstreamBranchNames.Contains(UMovieGraphNode::GlobalsPinNameString);
	const bool bDownstreamBranchExistsAndIsntOnlyGlobals =
		!DownstreamBranchNames.IsEmpty() && ((DownstreamBranchNames.Num() != 1) || (DownstreamBranchNames[0] != UMovieGraphNode::GlobalsPinNameString));
	const bool bUpstreamBranchExistsAndIsntOnlyGlobals =
		!UpstreamBranchNames.IsEmpty() && ((UpstreamBranchNames.Num() != 1) || (UpstreamBranchNames[0] != UMovieGraphNode::GlobalsPinNameString));

	// Globals branches can only be connected to Globals branches
	if ((bGlobalsIsDownstream && bUpstreamBranchExistsAndIsntOnlyGlobals) || (bGlobalsIsUpstream && bDownstreamBranchExistsAndIsntOnlyGlobals))
	{
		OutError = NSLOCTEXT("MoviePipeline", "GlobalsBranchMismatchError", "Globals branches can only be connected to other Globals branches.");
		return false;
	}

	// Error out if any of the nodes that are part of the connection cannot be connected to the upstream/downstream branches.
	for (const UMovieGraphNode* NodeToCheck : NodesToCheck)
	{
		if (NodeToCheck->GetBranchRestriction() == EMovieGraphBranchRestriction::Globals)
		{
			// Globals-specific nodes have to be connected such that the only upstream/downstream branches are Globals.
			// If either the upstream/downstream branches are empty (ie, the node isn't connected to Inputs/Outputs yet)
			// then the connection is OK for now -- the branch restriction will be enforced when nodes are connected to
			// Inputs/Outputs.
			if (bDownstreamBranchExistsAndIsntOnlyGlobals || bUpstreamBranchExistsAndIsntOnlyGlobals)
			{
				OutError = FText::Format(
					NSLOCTEXT("MoviePipeline", "GlobalsBranchRestrictionError", "The node '{0}' can only be connected to the Globals branch."),
						NodeToCheck->GetNodeTitle());
				return false;
			}
		}

		// Check that render-layer-only nodes aren't connected to Globals.
		if (NodeToCheck->GetBranchRestriction() == EMovieGraphBranchRestriction::RenderLayer)
		{
			if (bGlobalsIsDownstream || bGlobalsIsUpstream)
			{
				OutError = FText::Format(
					NSLOCTEXT("MoviePipeline", "RenderLayerBranchRestrictionError", "The node '{0}' can only be connected to a render layer branch."),
						NodeToCheck->GetNodeTitle());
				return false;
			}
		}
	}

	return true;
}

void UMovieGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	InitMoviePipelineNodeClasses();

	const UMoviePipelineEdGraph* Graph = Cast<UMoviePipelineEdGraph>(ContextMenuBuilder.CurrentGraph);
	if (!Graph)
	{
		return;
	}

	const UMovieGraphConfig* RuntimeGraph = Graph->GetPipelineGraph();
	if (!RuntimeGraph)
	{
		return;
	}

	for (UClass* PipelineNodeClass : MoviePipelineNodeClasses)
	{
		const UMovieGraphNode* PipelineNode = PipelineNodeClass->GetDefaultObject<UMovieGraphNode>();
		if (PipelineNodeClass == UMovieGraphVariableNode::StaticClass())
		{
			// Add variable actions separately
			continue;
		}
		if(PipelineNodeClass == UMovieGraphInputNode::StaticClass() ||
			PipelineNodeClass == UMovieGraphOutputNode::StaticClass())
		{
			// Can't place Input and Output nodes manually.
			continue;
		}

		// This can be used to sort whether or not an option shows up. For now there's no restrictions
		// on where nodes can be made, but eventually we might check which branch they're on (if from pin)
		// to filter out incompatible nodes.
		// if (!ContextMenuBuilder.FromPin || ContextMenuBuilder.FromPin->Direction == EGPD_Input)
		{
			const FText Name = PipelineNode->GetNodeTitle();
			const FText Category = PipelineNode->GetMenuCategory();
			const FText Tooltip = LOCTEXT("CreateNode_Tooltip", "Create a node of this type.");
			
			TSharedPtr<FMovieGraphSchemaAction> NewAction = MakeShared<FMovieGraphSchemaAction_NewNode>(Category, Name, Tooltip); 
			NewAction->NodeClass = PipelineNodeClass;

			ContextMenuBuilder.AddAction(NewAction);
		}
	}

	// Create an accessor node action for each variable the graph has
	const bool bIncludeGlobal = true;
	for (const UMovieGraphVariable* Variable : RuntimeGraph->GetVariables(bIncludeGlobal))
	{
		const FText Name = FText::Format(LOCTEXT("CreateVariable_Name", "Get {0}"), FText::FromString(Variable->GetMemberName()));
		const FText Category = Variable->IsGlobal() ? LOCTEXT("CreateGlobalVariable_Category", "Global Variables") : LOCTEXT("CreateVariable_Category", "Variables");
		const FText Tooltip = LOCTEXT("CreateVariable_Tooltip", "Create an accessor node for this variable.");
		
		TSharedPtr<FMovieGraphSchemaAction> NewAction = MakeShared<FMovieGraphSchemaAction_NewVariableNode>(Category, Name, Variable->GetGuid(), Tooltip);
		NewAction->NodeClass = UMovieGraphVariableNode::StaticClass();
		
		ContextMenuBuilder.AddAction(NewAction);
	}
}

const FPinConnectionResponse UMovieGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	// No Circular Connections
	if (PinA->GetOwningNode() == PinB->GetOwningNode())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "CircularPinError", "No Circular Connections!"));
	}

	// Pins need to be the same type
	if (PinA->PinType.PinCategory != PinB->PinType.PinCategory)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "PinTypeMismatchError", "Pin types don't match!"));
	}

	// Re-organize PinA/PinB to Input/Output by comparing internal directions to avoid having to check both cases depending on which
	// direction the conneciton was made.
	const UEdGraphPin* InputPin = nullptr;
	const UEdGraphPin* OutputPin = nullptr;

	if (!CategorizePinsByDirection(PinA, PinB, /*out*/ InputPin, /*out*/ OutputPin))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, NSLOCTEXT("MoviePipeline", "PinDirectionMismatchError", "Directions are not compatible!"));
	}

	// Determine if the connection would violate branch restrictions enforced by the nodes involved in the connection.
	FText BranchRestrictionError;
	if (!IsConnectionToBranchAllowed(InputPin, OutputPin, BranchRestrictionError))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, BranchRestrictionError);
	}

	// We don't allow multiple things to be connected to an Input Pin
	if(InputPin->HasAnyConnections())
	{
		const ECanCreateConnectionResponse ReplyBreakInputs = (PinA == InputPin) ? CONNECT_RESPONSE_BREAK_OTHERS_A : CONNECT_RESPONSE_BREAK_OTHERS_B;
		return FPinConnectionResponse(ReplyBreakInputs, NSLOCTEXT("MoviePipeline", "PinInputReplaceExisting","Replace existing input connections"));
	}
	
	// Make sure the pins are not on the same node
	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE, NSLOCTEXT("MoviePipeline", "PinConnect", "Connect nodes"));
}

bool UMovieGraphSchema::TryCreateConnection(UEdGraphPin* InA, UEdGraphPin* InB) const
{
	// See if the native UEdGraph connection goes through
	const bool bModified = Super::TryCreateConnection(InA, InB);

	// If it does, try to propagate the change to our runtime graph
	if (bModified)
	{
		check(InA && InB);
		const UEdGraphPin* A = (InA->Direction == EGPD_Output) ? InA : InB;
		const UEdGraphPin* B = (InA->Direction == EGPD_Input) ? InA : InB;
		check(A->Direction == EGPD_Output && B->Direction == EGPD_Input);

		UMoviePipelineEdGraphNodeBase* EdGraphNodeA = CastChecked<UMoviePipelineEdGraphNodeBase>(A->GetOwningNode());
		UMoviePipelineEdGraphNodeBase* EdGraphNodeB = CastChecked<UMoviePipelineEdGraphNodeBase>(B->GetOwningNode());

		UMovieGraphNode* RuntimeNodeA = EdGraphNodeA->GetRuntimeNode();
		UMovieGraphNode* RuntimeNodeB = EdGraphNodeB->GetRuntimeNode();
		check(RuntimeNodeA && RuntimeNodeB);

		UMovieGraphConfig* RuntimeGraph = RuntimeNodeA->GetGraph();
		check(RuntimeGraph);

		const bool bReconstructNodeB = RuntimeGraph->AddLabeledEdge(RuntimeNodeA, A->PinName, RuntimeNodeB, B->PinName);
		//if (bReconstructNodeB)
		//{
		//	RuntimeNodeB->ReconstructNode();
		//}
	}

	return bModified;
}

void UMovieGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
	const FScopedTransaction Transaction(LOCTEXT("MoviePipelineGraphEditor_BreakPinLinks", "Break Pin Links"));
	Super::BreakPinLinks(TargetPin, bSendsNodeNotification);

	UEdGraphNode* GraphNode = TargetPin.GetOwningNode();
	UMoviePipelineEdGraphNodeBase* MoviePipelineEdGraphNode = CastChecked<UMoviePipelineEdGraphNodeBase>(GraphNode);

	UMovieGraphNode* RuntimeNode = MoviePipelineEdGraphNode->GetRuntimeNode();
	check(RuntimeNode);

	UMovieGraphConfig* RuntimeGraph = RuntimeNode->GetGraph();
	check(RuntimeGraph);

	if (TargetPin.Direction == EEdGraphPinDirection::EGPD_Input)
	{
		RuntimeGraph->RemoveInboundEdges(RuntimeNode, TargetPin.PinName);
	}
	else if (TargetPin.Direction == EEdGraphPinDirection::EGPD_Output)
	{
		RuntimeGraph->RemoveOutboundEdges(RuntimeNode, TargetPin.PinName);
	}
}

void UMovieGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	const FScopedTransaction Transaction(LOCTEXT("MoviePipelineGraphEditor_BreakSinglePinLinks", "Break Single Pin Link"));
	Super::BreakSinglePinLink(SourcePin, TargetPin);

	UEdGraphNode* SourceGraphNode = SourcePin->GetOwningNode();
	UEdGraphNode* TargetGraphNode = TargetPin->GetOwningNode();

	UMoviePipelineEdGraphNodeBase* SourcePipelineGraphNode = CastChecked<UMoviePipelineEdGraphNodeBase>(SourceGraphNode);
	UMoviePipelineEdGraphNodeBase* TargetPipelineGraphNode = CastChecked<UMoviePipelineEdGraphNodeBase>(TargetGraphNode);

	UMovieGraphNode* SourceRuntimeNode = SourcePipelineGraphNode->GetRuntimeNode();
	UMovieGraphNode* TargetRuntimeNode = TargetPipelineGraphNode->GetRuntimeNode();
	check(SourceRuntimeNode && TargetRuntimeNode);

	UMovieGraphConfig* RuntimeGraph = SourceRuntimeNode->GetGraph();
	check(RuntimeGraph);

	RuntimeGraph->RemoveEdge(SourceRuntimeNode, SourcePin->PinName, TargetRuntimeNode, TargetPin->PinName);
}

FLinearColor UMovieGraphSchema::GetTypeColor(const FName& InType)
{
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();

	if (InType == PC_Branch)
	{
		return Settings->ExecutionPinTypeColor;
	}

	if (InType == PC_Boolean)
	{
		return Settings->BooleanPinTypeColor;
	}
	
	if (InType == PC_Byte)
	{
		return Settings->BytePinTypeColor;
	}

	if (InType == PC_Integer)
	{
		return Settings->IntPinTypeColor;
	}

	if (InType == PC_Int64)
	{
		return Settings->Int64PinTypeColor;
	}

	if (InType == PC_Float)
	{
		return Settings->FloatPinTypeColor;
	}

	if (InType == PC_Double)
	{
		return Settings->DoublePinTypeColor;
	}

	if (InType == PC_Name)
	{
		return Settings->NamePinTypeColor;
	}

	if (InType == PC_String)
	{
		return Settings->StringPinTypeColor;
	}

	if (InType == PC_Text)
	{
		return Settings->TextPinTypeColor;
	}

	if (InType == PC_Enum)
	{
		return Settings->BytePinTypeColor;
	}

	if (InType == PC_Struct)
	{
		return Settings->StructPinTypeColor;
	}
	
	if (InType == PC_Object)
	{
		return Settings->ObjectPinTypeColor;
	}

	if (InType == PC_SoftObject)
	{
		return Settings->SoftObjectPinTypeColor;
	}
	
	if (InType == PC_Class)
    {
    	return Settings->ClassPinTypeColor;
    }

	if (InType == PC_SoftClass)
	{
		return Settings->SoftClassPinTypeColor;
	}
	
	return Settings->DefaultPinTypeColor;
}

FLinearColor UMovieGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetTypeColor(PinType.PinCategory);
}

FConnectionDrawingPolicy* UMovieGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID,
	float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj) const
{
	return new FMovieEdGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

FMovieGraphSchemaAction_NewNode::FMovieGraphSchemaAction_NewNode(FText InNodeCategory, FText InDisplayName, FText InToolTip)
	: FMovieGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InToolTip), 0)
{
	
}

UEdGraphNode* FMovieGraphSchemaAction_NewNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UMovieGraphConfig* RuntimeGraph = CastChecked<UMoviePipelineEdGraph>(ParentGraph)->GetPipelineGraph();
	const FScopedTransaction Transaction(LOCTEXT("GraphEditor_NewNode", "Create Pipeline Graph Node."));
	RuntimeGraph->Modify();

	UMovieGraphNode* RuntimeNode = RuntimeGraph->ConstructRuntimeNode<UMovieGraphNode>(NodeClass);

	// Now create the editor graph node
	FGraphNodeCreator<UMoviePipelineEdGraphNode> NodeCreator(*ParentGraph);
	UMoviePipelineEdGraphNode* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
	GraphNode->Construct(RuntimeNode);
	GraphNode->NodePosX = Location.X;
	GraphNode->NodePosY = Location.Y;


	// Finalize generates a guid, calls a post-place callback, and allocates default pins if needed
	NodeCreator.Finalize();

	if (FromPin)
	{
		GraphNode->AutowireNewNode(FromPin);
	}
	return GraphNode;
}

FMovieGraphSchemaAction_NewVariableNode::FMovieGraphSchemaAction_NewVariableNode(FText InNodeCategory, FText InDisplayName, FGuid InVariableGuid, FText InToolTip)
	: FMovieGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InToolTip), 0)
	, VariableGuid(InVariableGuid)
{
	
}

UEdGraphNode* FMovieGraphSchemaAction_NewVariableNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UMovieGraphConfig* RuntimeGraph = CastChecked<UMoviePipelineEdGraph>(ParentGraph)->GetPipelineGraph();
	const FScopedTransaction Transaction(LOCTEXT("GraphEditor_NewVariableNode", "Add New Variable Accessor Node"));
	RuntimeGraph->Modify();

	UMovieGraphNode* RuntimeNode = RuntimeGraph->ConstructRuntimeNode<UMovieGraphNode>(NodeClass);
	if (UMovieGraphVariableNode* VariableNode = Cast<UMovieGraphVariableNode>(RuntimeNode))
	{
		VariableNode->SetVariable(RuntimeGraph->GetVariableByGuid(VariableGuid));
	}

	// Now create the variable node
	FGraphNodeCreator<UMoviePipelineEdGraphVariableNode> NodeCreator(*ParentGraph);
	UMoviePipelineEdGraphVariableNode* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
	GraphNode->Construct(RuntimeNode);
	GraphNode->NodePosX = Location.X;
	GraphNode->NodePosY = Location.Y;
	
	// Finalize generates a guid, calls a post-place callback, and allocates default pins if needed
	NodeCreator.Finalize();

	if (FromPin)
	{
		GraphNode->AutowireNewNode(FromPin);
	}

	return GraphNode;
}

UEdGraphNode* FMovieGraphSchemaAction_NewComment::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UEdGraphNode_Comment* CommentTemplate = NewObject<UEdGraphNode_Comment>();

	const TSharedPtr<SGraphEditor> GraphEditorPtr = SGraphEditor::FindGraphEditorForGraph(ParentGraph);

	FVector2D SpawnLocation = Location;
	FSlateRect Bounds;
	if (GraphEditorPtr && GraphEditorPtr->GetBoundsForSelectedNodes(Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}
	
	const FScopedTransaction Transaction(LOCTEXT("GraphEditor_NewCommentNode", "Add New Comment Node"));
	ParentGraph->Modify();
	
	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);;
}

#undef LOCTEXT_NAMESPACE // "MoviePipelineGraphSchema"
