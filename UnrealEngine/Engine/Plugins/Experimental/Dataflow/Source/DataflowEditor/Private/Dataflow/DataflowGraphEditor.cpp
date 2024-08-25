// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowGraphEditor.h"

#include "BoneDragDropOp.h"
#include "Dataflow/DataflowEditorToolkit.h"
#include "Dataflow/DataflowEngine.h"
#include "Dataflow/DataflowSNodeFactories.h"
#include "Dataflow/DataflowSCommentNode.h"
#include "Dataflow/DataflowNodeParameters.h"
#include "Dataflow/DataflowXml.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowSelectionNodes.h"
#include "Dataflow/DataflowSchema.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "IStructureDetailsView.h"
#include "SGraphPanel.h"

#define LOCTEXT_NAMESPACE "DataflowGraphEditor"

void SDataflowGraphEditor::Construct(const FArguments& InArgs, UObject* InAssetOwner)
{
	check(InArgs._GraphToEdit);
	AssetOwner = InAssetOwner; // nullptr is valid
	DataflowAsset = Cast<UDataflow>(InArgs._GraphToEdit);
	DetailsView = InArgs._DetailsView;
	EvaluateGraphCallback = InArgs._EvaluateGraph;
	OnDragDropEventCallback = InArgs._OnDragDropEvent;

	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = FText::FromString("Dataflow");

	FGraphEditorCommands::Register();
	FDataflowEditorCommands::Register();
	if (!GraphEditorCommands.IsValid())
	{
		GraphEditorCommands = MakeShareable(new FUICommandList);
		{
			GraphEditorCommands->MapAction(
				FGenericCommands::Get().Delete,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::DeleteNode)
			);
			GraphEditorCommands->MapAction(
				FDataflowEditorCommands::Get().EvaluateNode,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::EvaluateNode)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().CreateComment,
				FExecuteAction::CreateRaw(this, &SDataflowGraphEditor::CreateComment)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().AlignNodesTop,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::AlignTop)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().AlignNodesMiddle,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::AlignMiddle)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().AlignNodesBottom,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::AlignBottom)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().AlignNodesLeft,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::AlignLeft)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().AlignNodesCenter,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::AlignCenter)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().AlignNodesRight,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::AlignRight)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().StraightenConnections,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::StraightenConnections)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().DistributeNodesHorizontally,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::DistributeHorizontally)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().DistributeNodesVertically,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::DistributeVertically)
			);
			GraphEditorCommands->MapAction(
				FDataflowEditorCommands::Get().ToggleEnabledState,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::ToggleEnabledState)
			);
			GraphEditorCommands->MapAction(
				FDataflowEditorCommands::Get().AddOptionPin,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::OnAddOptionPin),
				FCanExecuteAction::CreateSP(this, &SDataflowGraphEditor::CanAddOptionPin)
			);
			GraphEditorCommands->MapAction(
				FDataflowEditorCommands::Get().RemoveOptionPin,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::OnRemoveOptionPin),
				FCanExecuteAction::CreateSP(this, &SDataflowGraphEditor::CanRemoveOptionPin)
			);
			GraphEditorCommands->MapAction(
				FGenericCommands::Get().Duplicate,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::DuplicateSelectedNodes)
			);
			GraphEditorCommands->MapAction(
				FDataflowEditorCommands::Get().ZoomToFitGraph,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::ZoomToFitGraph)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().ShowAllPins,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::SetPinVisibility, SGraphEditor::Pin_Show),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SDataflowGraphEditor::GetPinVisibility, SGraphEditor::Pin_Show)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().HideNoConnectionPins,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::SetPinVisibility, SGraphEditor::Pin_HideNoConnection),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SDataflowGraphEditor::GetPinVisibility, SGraphEditor::Pin_HideNoConnection)
			);
			GraphEditorCommands->MapAction(
				FGraphEditorCommands::Get().HideNoConnectionNoDefaultPins,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::SetPinVisibility, SGraphEditor::Pin_HideNoConnectionNoDefault),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SDataflowGraphEditor::GetPinVisibility, SGraphEditor::Pin_HideNoConnectionNoDefault)
			);
			GraphEditorCommands->MapAction(
				FGenericCommands::Get().Copy,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::CopySelectedNodes)
			);
			GraphEditorCommands->MapAction(
				FGenericCommands::Get().Cut,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::CutSelectedNodes)
			);
			GraphEditorCommands->MapAction(
				FGenericCommands::Get().Paste,
				FExecuteAction::CreateSP(this, &SDataflowGraphEditor::PasteSelectedNodes)
			);
		}
	}


	SGraphEditor::FArguments Arguments;
	Arguments._AdditionalCommands = GraphEditorCommands;
	Arguments._Appearance = AppearanceInfo;
	Arguments._GraphToEdit = InArgs._GraphToEdit;
	Arguments._GraphEvents = InArgs._GraphEvents;

	ensureMsgf(!Arguments._GraphEvents.OnSelectionChanged.IsBound(), TEXT("DataflowGraphEditor::OnSelectionChanged rebound during construction."));
	Arguments._GraphEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SDataflowGraphEditor::OnSelectedNodesChanged);

	SGraphEditor::Construct(Arguments);

}


void SDataflowGraphEditor::EvaluateNode()
{
	if (EvaluateGraphCallback)
	{
		FDataflowEditorCommands::EvaluateSelectedNodes(GetSelectedNodes(), EvaluateGraphCallback);
	}
	else
	{
		FDataflowEditorCommands::FGraphEvaluationCallback LocalEvaluateCallback = [](FDataflowNode* Node, FDataflowOutput* Out)
		{
			using namespace Dataflow;
			FContextThreaded(FPlatformTime::Cycles64()).Evaluate(Node, Out);
		};

		FDataflowEditorCommands::EvaluateSelectedNodes(GetSelectedNodes(), LocalEvaluateCallback);
	}
}

void SDataflowGraphEditor::DeleteNode()
{
	if (DataflowAsset.Get())
	{
		if (DetailsView)
		{
			DetailsView->SetStructureData(nullptr);
		}

		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();
		if (SelectedNodes.Num() > 0)
		{
			FDataflowEditorCommands::DeleteNodes(DataflowAsset.Get(), SelectedNodes);

			OnNodeDeletedMulticast.Broadcast(SelectedNodes);
		}
	}
}

void SDataflowGraphEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	OnSelectionChangedMulticast.Broadcast(NewSelection);  // Broadcast the selection change before refreshing the DetailsView, the nodes' specific UI data have to be updated before the UI is being redrawn

	if (DataflowAsset.Get() && DetailsView)
	{
		FDataflowEditorCommands::OnSelectedNodesChanged(DetailsView, AssetOwner.Get(), DataflowAsset.Get(), NewSelection);
	}
}

FReply SDataflowGraphEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::LeftControl)
	{
		LeftControlKeyDown = true;
	}
	if (InKeyEvent.GetKey() == EKeys::RightControl)
	{
		RightControlKeyDown = true;
	}
	if (InKeyEvent.GetKey() == EKeys::V)
	{
		VKeyDown = true;
	}
	return SGraphEditor::OnKeyUp(MyGeometry, InKeyEvent);
}


FReply SDataflowGraphEditor::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// process paste
	if ( (VKeyDown && LeftControlKeyDown) || (VKeyDown && RightControlKeyDown) )
	{
		if (InKeyEvent.GetKey() == EKeys::LeftControl
			|| InKeyEvent.GetKey() == EKeys::RightControl
			|| InKeyEvent.GetKey() == EKeys::V)
		{
			FString XmlBuffer;
			FPlatformApplicationMisc::ClipboardPaste(XmlBuffer);
			FDataflowXmlRead Xml(this); 
			if (Xml.LoadFromBuffer(XmlBuffer))
			{
				Xml.ParseXmlFile();
			}
		}
	}

	if (InKeyEvent.GetKey() == EKeys::LeftControl)
	{
		LeftControlKeyDown = false;
	}
	if (InKeyEvent.GetKey() == EKeys::RightControl)
	{
		RightControlKeyDown = false;
	}
	if (InKeyEvent.GetKey() == EKeys::V)
	{
		VKeyDown = false;
	}
	if (InKeyEvent.GetKey() == EKeys::LeftControl)
	{
		return FReply::Unhandled();
	}
	return SGraphEditor::OnKeyUp(MyGeometry, InKeyEvent);
}


FReply SDataflowGraphEditor::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FBoneDragDropOp> SchemeDragDropOp = DragDropEvent.GetOperationAs<FBoneDragDropOp>())
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}


FReply SDataflowGraphEditor::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FBoneDragDropOp> SchemeDragDropOp = DragDropEvent.GetOperationAs<FBoneDragDropOp>())
	{
		if (OnDragDropEventCallback)
		{
			OnDragDropEventCallback(MyGeometry,DragDropEvent);
		}
	}
	return SGraphEditor::OnDrop(MyGeometry, DragDropEvent);
}

/*
void SDataflowGraphEditor::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{

	TSharedPtr<FGraphSchemaActionDragDropAction> SchemeDragDropOp = DragDropEvent.GetOperationAs<FGraphSchemaActionDragDropAction>();
	{
	//	FDataprepSchemaActionContext ActionContext;
	//	ActionContext.DataprepActionPtr = StepData->DataprepActionPtr;
	//	ActionContext.DataprepActionStepPtr = StepData->DataprepActionStepPtr;
	//	ActionContext.StepIndex = StepData->StepIndex;
	//	DataprepDragDropOp->SetHoveredDataprepActionContext(ActionContext);
	}
	UE_LOG(SDataflowGraphEditorLog, All, TEXT("SDataflowGraphEditor::OnDragEnter"));

}
void SDataflowGraphEditor::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FGraphSchemaActionDragDropAction> SchemeDragDropOp = DragDropEvent.GetOperationAs<FGraphSchemaActionDragDropAction>();
	{
		// DataflowDragDropOp->SetHoveredDataprepActionContext(TOptional<FDataprepSchemaActionContext>());
	}
	UE_LOG(SDataflowGraphEditorLog, All, TEXT("SDataflowGraphEditor::OnDragLeave"));
}
*/

void SDataflowGraphEditor::CreateComment()
{
	UDataflow* Graph = DataflowAsset.Get();
	const TSharedPtr<SGraphEditor>& InGraphEditor = SharedThis(GetGraphEditor());

	TSharedPtr<FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode> CommentAction = FAssetSchemaAction_Dataflow_CreateCommentNode_DataflowEdNode::CreateAction(Graph, InGraphEditor);
	CommentAction->PerformAction(Graph, nullptr, GetGraphEditor()->GetPasteLocation(), false);
}

void SDataflowGraphEditor::CreateVertexSelectionNode(const FString & InArray)
{
	UDataflow* Graph = DataflowAsset.Get();
	const TSharedPtr<SGraphEditor>& InGraphEditor = SharedThis(GetGraphEditor());

	if (TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> NodeAction = FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::CreateAction(Graph, "FSelectionSetDataflowNode"))
	{
		if (UEdGraphNode* NewEdNode = NodeAction->PerformAction(Graph, nullptr, GetGraphEditor()->GetPasteLocation(), false))
		{
			FDataflowAssetEdit Edit = Graph->EditDataflow();
			if (Dataflow::FGraph* DataflowGraph = Edit.GetGraph())
			{
				if (TSharedPtr<FDataflowNode> Node = DataflowGraph->FindBaseNode(((UDataflowEdNode*)NewEdNode)->DataflowNodeGuid))
				{
					if (FSelectionSetDataflowNode * SelectionNode = Node->AsType<FSelectionSetDataflowNode>())
					{
						SelectionNode->Indices = InArray;
					}
				}
			}
		}
	}
}

void SDataflowGraphEditor::AlignTop()
{
	GetGraphEditor()->OnAlignTop();
}

void SDataflowGraphEditor::AlignMiddle()
{
	GetGraphEditor()->OnAlignMiddle();
}

void SDataflowGraphEditor::AlignBottom()
{
	GetGraphEditor()->OnAlignBottom();
}

void SDataflowGraphEditor::AlignLeft()
{
	GetGraphEditor()->OnAlignLeft();
}

void SDataflowGraphEditor::AlignCenter()
{
	GetGraphEditor()->OnAlignCenter();
}

void SDataflowGraphEditor::AlignRight()
{
	GetGraphEditor()->OnAlignRight();
}

void SDataflowGraphEditor::StraightenConnections()
{
	GetGraphEditor()->OnStraightenConnections();
}

void SDataflowGraphEditor::DistributeHorizontally()
{
	GetGraphEditor()->OnDistributeNodesH();
}

void SDataflowGraphEditor::DistributeVertically()
{
	GetGraphEditor()->OnDistributeNodesV();
}

void SDataflowGraphEditor::ToggleEnabledState()
{
	FDataflowEditorCommands::ToggleEnabledState(DataflowAsset.Get());
}

void SDataflowGraphEditor::OnAddOptionPin()
{
	UDataflow* const Graph = DataflowAsset.Get();
	FDataflowAssetEdit Edit = Graph->EditDataflow();
	if (Dataflow::FGraph* const DataflowGraph = Edit.GetGraph())
	{
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		// Iterate over all nodes, and add the pin
		for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
		{
			UDataflowEdNode* const EdNode = CastChecked<UDataflowEdNode>(*It);

			if (const TSharedPtr<FDataflowNode> Node = DataflowGraph->FindBaseNode(EdNode->DataflowNodeGuid))
			{
				if (Node->CanAddPin())
				{
					const FScopedTransaction Transaction(LOCTEXT("AddOptionPin", "Add Option Pin"));
					EdNode->Modify();

					EdNode->AddOptionPin();

					const UDataflowSchema* const Schema = CastChecked<UDataflowSchema>(Graph->GetSchema());
					Schema->ReconstructNode(*EdNode);
				}
			}
		}
	}
}

bool SDataflowGraphEditor::CanAddOptionPin() const
{
	bool bCanAddOptionPin = false;

	const UDataflow* const Graph = DataflowAsset.Get();
	if (const Dataflow::FGraph* const DataflowGraph = Graph->GetDataflow().Get())
	{
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		// Iterate over all nodes, and add the pin
		for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
		{
			if (const UDataflowEdNode* const EdNode = Cast<UDataflowEdNode>(*It))
			{
				if (const TSharedPtr<const FDataflowNode> Node = DataflowGraph->FindBaseNode(EdNode->DataflowNodeGuid))
				{
					bCanAddOptionPin = Node->CanAddPin();
				}
				else
				{
					bCanAddOptionPin = false;
				}

				if (!bCanAddOptionPin)
				{
					break;  // One bad node is good enough to return false
				}
			}
		}
	}

	return bCanAddOptionPin;
}

void SDataflowGraphEditor::OnRemoveOptionPin()
{
	UDataflow* const Graph = DataflowAsset.Get();
	FDataflowAssetEdit Edit = Graph->EditDataflow();
	if (Dataflow::FGraph* const DataflowGraph = Edit.GetGraph())
	{
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		// Iterate over all nodes, and remove a pin
		for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
		{
			UDataflowEdNode* const EdNode = CastChecked<UDataflowEdNode>(*It);

			if (const TSharedPtr<FDataflowNode> Node = DataflowGraph->FindBaseNode(EdNode->DataflowNodeGuid))
			{
				if (Node->CanRemovePin())
				{
					const FScopedTransaction Transaction(LOCTEXT("RemoveOptionPin", "Remove Option Pin"));
					EdNode->Modify();

					EdNode->RemoveOptionPin();

					const UDataflowSchema* const Schema = CastChecked<UDataflowSchema>(Graph->GetSchema());
					Schema->ReconstructNode(*EdNode);
				}
			}
		}
	}
}

bool SDataflowGraphEditor::CanRemoveOptionPin() const
{
	bool bCanRemoveOptionPin = false;

	const UDataflow* const Graph = DataflowAsset.Get();
	if (const Dataflow::FGraph* const DataflowGraph = Graph->GetDataflow().Get())
	{
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		// Iterate over all nodes, and add the pin
		for (FGraphPanelSelectionSet::TConstIterator It(SelectedNodes); It; ++It)
		{
			if (const UDataflowEdNode* const EdNode = Cast<UDataflowEdNode>(*It))
			{
				if (const TSharedPtr<const FDataflowNode> Node = DataflowGraph->FindBaseNode(EdNode->DataflowNodeGuid))
				{
					bCanRemoveOptionPin = Node->CanRemovePin();
				}
				else
				{
					bCanRemoveOptionPin = false;
				}

				if (!bCanRemoveOptionPin)
				{
					break;  // One bad node is good enough to return false
				}
			}
		}
	}

	return bCanRemoveOptionPin;
}

void SDataflowGraphEditor::DuplicateSelectedNodes()
{
	if (UDataflow* Graph = DataflowAsset.Get())
	{
		const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor = SharedThis(this);
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		if (SelectedNodes.Num() > 0)
		{
			FDataflowEditorCommands::DuplicateNodes(Graph, DataflowGraphEditor, SelectedNodes);
		}
	}
}

void SDataflowGraphEditor::ZoomToFitGraph()
{
	constexpr bool bOnlySelection = true;	// This will focus on the selected nodes, if any. If no nodes are selected, it will focus the whole graph.
	ZoomToFit(bOnlySelection);
}

bool SDataflowGraphEditor::GetPinVisibility(SGraphEditor::EPinVisibility PinVisibility) const
{
	if (const SGraphPanel* GraphPanel = GetGraphPanel())
	{
		return GraphPanel->GetPinVisibility() == PinVisibility;
	}
	return false;
}

void SDataflowGraphEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(DataflowAsset);
	Collector.AddReferencedObject(AssetOwner);
}

void SDataflowGraphEditor::CopySelectedNodes()
{
	if (UDataflow* Graph = DataflowAsset.Get())
	{
		const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor = SharedThis(this);
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		if (SelectedNodes.Num() > 0)
		{
			FDataflowEditorCommands::CopyNodes(Graph, DataflowGraphEditor, SelectedNodes);
		}
	}
}

void SDataflowGraphEditor::CutSelectedNodes()
{
	if (UDataflow* Graph = DataflowAsset.Get())
	{
		const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor = SharedThis(this);
		const FGraphPanelSelectionSet& SelectedNodes = GetSelectedNodes();

		if (SelectedNodes.Num() > 0)
		{
			FDataflowEditorCommands::CopyNodes(Graph, DataflowGraphEditor, SelectedNodes);

			FDataflowEditorCommands::DeleteNodes(DataflowAsset.Get(), SelectedNodes);
		}
	}
}

void SDataflowGraphEditor::PasteSelectedNodes()
{
	if (UDataflow* Graph = DataflowAsset.Get())
	{
		const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor = SharedThis(this);

		FDataflowEditorCommands::PasteNodes(Graph, DataflowGraphEditor);
	}
}

#undef LOCTEXT_NAMESPACE
