// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphSchema.h"

#include "EdGraphSchema_K2.h"
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
#include "MoviePipelineEdGraphSubgraphNode.h"

TArray<UClass*> UMovieGraphSchema::MoviePipelineNodeClasses;

#define LOCTEXT_NAMESPACE "MoviePipelineGraphSchema"

const FName UMovieGraphSchema::PC_Branch(TEXT("branch"));	// The branch looks like an Exec pin, but isn't the same thing, so we don't use the BP Exec type
const FName UMovieGraphSchema::PC_Boolean(UEdGraphSchema_K2::PC_Boolean);
const FName UMovieGraphSchema::PC_Byte(UEdGraphSchema_K2::PC_Byte);
const FName UMovieGraphSchema::PC_Integer(UEdGraphSchema_K2::PC_Int);
const FName UMovieGraphSchema::PC_Int64(UEdGraphSchema_K2::PC_Int64);
const FName UMovieGraphSchema::PC_Real(UEdGraphSchema_K2::PC_Real);
const FName UMovieGraphSchema::PC_Float(UEdGraphSchema_K2::PC_Float);
const FName UMovieGraphSchema::PC_Double(UEdGraphSchema_K2::PC_Double);
const FName UMovieGraphSchema::PC_Name(UEdGraphSchema_K2::PC_Name);
const FName UMovieGraphSchema::PC_String(UEdGraphSchema_K2::PC_String);
const FName UMovieGraphSchema::PC_Text(UEdGraphSchema_K2::PC_Text);
const FName UMovieGraphSchema::PC_Enum(UEdGraphSchema_K2::PC_Enum);
const FName UMovieGraphSchema::PC_Struct(UEdGraphSchema_K2::PC_Struct);
const FName UMovieGraphSchema::PC_Object(UEdGraphSchema_K2::PC_Object);
const FName UMovieGraphSchema::PC_SoftObject(UEdGraphSchema_K2::PC_SoftObject);
const FName UMovieGraphSchema::PC_Class(UEdGraphSchema_K2::PC_Class);
const FName UMovieGraphSchema::PC_SoftClass(UEdGraphSchema_K2::PC_SoftClass);

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

bool UMovieGraphSchema::SupportsPinTypeContainer(TWeakPtr<const FEdGraphSchemaAction> SchemaAction, const FEdGraphPinType& PinType, const EPinContainerType& ContainerType) const
{
	// No maps, sets, or arrays
	return ContainerType == EPinContainerType::None;
}

bool UMovieGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	// The graph doesn't support editing default values for pins yet
	return true;
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
			&& !It->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_Hidden | CLASS_HideDropDown))
		{
			MoviePipelineNodeClasses.Add(*It);
		}
	}

	MoviePipelineNodeClasses.Sort();
}

bool UMovieGraphSchema::IsConnectionToBranchAllowed(const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin, FText& OutError) const
{
	const UMovieGraphPin* ToPin = UE::MovieGraph::Private::GetGraphPinFromEdPin(InputPin);
	const UMovieGraphPin* FromPin = UE::MovieGraph::Private::GetGraphPinFromEdPin(OutputPin);
	
	return FromPin->IsConnectionToBranchAllowed(ToPin, OutError);
}

void UMovieGraphSchema::AddExtraMenuActions(FGraphActionMenuBuilder& ActionMenuBuilder) const
{
	// Comment action
	ActionMenuBuilder.AddAction(CreateCommentMenuAction());
}

TSharedRef<FMovieGraphSchemaAction_NewComment> UMovieGraphSchema::CreateCommentMenuAction() const
{
	const FText CommentMenuDesc = LOCTEXT("AddComment", "Add Comment");
	const FText CommentCategory;
	const FText CommentDescription = LOCTEXT("AddCommentTooltip", "Create a resizable comment box.");

	const TSharedRef<FMovieGraphSchemaAction_NewComment> NewCommentAction = MakeShared<FMovieGraphSchemaAction_NewComment>(CommentCategory, CommentMenuDesc, CommentDescription, 0);
	return NewCommentAction;
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
			constexpr int32 Grouping = 0;
			const FText Keywords = PipelineNode->GetKeywords();
			
			TSharedPtr<FMovieGraphSchemaAction> NewAction = MakeShared<FMovieGraphSchemaAction_NewNode>(Category, Name, Tooltip, Grouping, Keywords); 
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

	AddExtraMenuActions(ContextMenuBuilder);
}

const FPinConnectionResponse UMovieGraphSchema::CanCreateConnection(const UEdGraphPin* PinA, const UEdGraphPin* PinB) const
{
	const UMovieGraphPin* FromPin = UE::MovieGraph::Private::GetGraphPinFromEdPin(PinA);
	const UMovieGraphPin* ToPin = UE::MovieGraph::Private::GetGraphPinFromEdPin(PinB);
	
	return FromPin->CanCreateConnection_PinConnectionResponse(ToPin);
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

	RuntimeGraph->RemoveLabeledEdge(SourceRuntimeNode, SourcePin->PinName, TargetRuntimeNode, TargetPin->PinName);
}

FLinearColor UMovieGraphSchema::GetTypeColor(const FName& InPinCategory, const FName& InPinSubCategory)
{
	const UGraphEditorSettings* Settings = GetDefault<UGraphEditorSettings>();

	if (InPinCategory == PC_Branch)
	{
		return Settings->ExecutionPinTypeColor;
	}

	if (InPinCategory == PC_Boolean)
	{
		return Settings->BooleanPinTypeColor;
	}
	
	if (InPinCategory == PC_Byte)
	{
		return Settings->BytePinTypeColor;
	}

	if (InPinCategory == PC_Integer)
	{
		return Settings->IntPinTypeColor;
	}

	if (InPinCategory == PC_Int64)
	{
		return Settings->Int64PinTypeColor;
	}

	if (InPinCategory == PC_Float)
	{
		return Settings->FloatPinTypeColor;
	}

	if (InPinCategory == PC_Double)
	{
		return Settings->DoublePinTypeColor;
	}

	if (InPinCategory == PC_Real)
	{
		if (InPinSubCategory == PC_Float)
		{
			return Settings->FloatPinTypeColor;
		}

		if (InPinSubCategory == PC_Double)
		{
			return Settings->DoublePinTypeColor;
		}
	}

	if (InPinCategory == PC_Name)
	{
		return Settings->NamePinTypeColor;
	}

	if (InPinCategory == PC_String)
	{
		return Settings->StringPinTypeColor;
	}

	if (InPinCategory == PC_Text)
	{
		return Settings->TextPinTypeColor;
	}

	if (InPinCategory == PC_Enum)
	{
		return Settings->BytePinTypeColor;
	}

	if (InPinCategory == PC_Struct)
	{
		return Settings->StructPinTypeColor;
	}
	
	if (InPinCategory == PC_Object)
	{
		return Settings->ObjectPinTypeColor;
	}

	if (InPinCategory == PC_SoftObject)
	{
		return Settings->SoftObjectPinTypeColor;
	}
	
	if (InPinCategory == PC_Class)
    {
    	return Settings->ClassPinTypeColor;
    }

	if (InPinCategory == PC_SoftClass)
	{
		return Settings->SoftClassPinTypeColor;
	}
	
	return Settings->DefaultPinTypeColor;
}

FLinearColor UMovieGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetTypeColor(PinType.PinCategory, PinType.PinSubCategory);
}

FConnectionDrawingPolicy* UMovieGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID,
	float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj) const
{
	return new FMovieEdGraphConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

FMovieGraphSchemaAction_NewNode::FMovieGraphSchemaAction_NewNode(FText InNodeCategory, FText InDisplayName, FText InToolTip, int32 InGrouping, FText InKeywords)
	: FMovieGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InDisplayName), MoveTemp(InToolTip), InGrouping, MoveTemp(InKeywords))
{
	
}

UEdGraphNode* FMovieGraphSchemaAction_NewNode::PerformAction(UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode)
{
	UMovieGraphConfig* RuntimeGraph = CastChecked<UMoviePipelineEdGraph>(ParentGraph)->GetPipelineGraph();
	const FScopedTransaction Transaction(LOCTEXT("GraphEditor_NewNode", "Create Pipeline Graph Node."));
	RuntimeGraph->Modify();
	ParentGraph->Modify();

	UMovieGraphNode* RuntimeNode = RuntimeGraph->ConstructRuntimeNode<UMovieGraphNode>(NodeClass);

	// Now create the editor graph node
	FGraphNodeCreator<UMoviePipelineEdGraphNode> NodeCreator(*ParentGraph);

	// Define the ed graph node type here if it differs from UMoviePipelineEdGraphNode
	// If other ed node class types are needed here,
	// we should let ed nodes declare their equivalent runtime node,
	// and use that mapping to determine the applicable ed node type rather than hard-coding.
	TSubclassOf<UMoviePipelineEdGraphNode> InvokableEdGraphNodeClass = UMoviePipelineEdGraphNode::StaticClass();
	if (RuntimeNode->IsA(UMovieGraphSubgraphNode::StaticClass()))
	{
		InvokableEdGraphNodeClass = UMoviePipelineEdGraphSubgraphNode::StaticClass();
	}
	
	UMoviePipelineEdGraphNode* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode, InvokableEdGraphNodeClass);
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
	ParentGraph->Modify();

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
