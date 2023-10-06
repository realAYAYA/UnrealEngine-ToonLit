// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorCommands.h"

#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowOverrideNode.h"
#include "EdGraph/EdGraphNode.h"
#include "IStructureDetailsView.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowSCommentNode.h"
#include "Dataflow/DataflowSNode.h"

#define LOCTEXT_NAMESPACE "DataflowEditorCommands"

void FDataflowEditorCommandsImpl::RegisterCommands()
{
	UI_COMMAND(EvaluateNode, "Evaluate", "Trigger an evaluation of the selected node.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateComment, "CreateComment", "Create a Comment node.", EUserInterfaceActionType::None, FInputChord());
	UI_COMMAND(ToggleEnabledState, "ToggleEnabledState", "Toggle node between Enabled/Disabled state.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleObjectSelection, "ToggleObjectSelection", "Enable object selection in editor.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleFaceSelection, "ToggleFaceSelection", "Enable face selection in editor.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleVertexSelection, "ToggleVertexSelection", "Enable vertex selection in editor.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(AddOptionPin, "AddOptionPin", "Add an option pin to the selected nodes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveOptionPin, "RemoveOptionPin", "Remove the last option pin from the selected nodes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ZoomToFitGraph, "ZoomToFitGraph", "Fit the graph in the graph editor viewport.", EUserInterfaceActionType::None, FInputChord(EKeys::F));
	

	if (Dataflow::FNodeFactory* Factory = Dataflow::FNodeFactory::GetInstance())
	{
		for (Dataflow::FFactoryParameters Parameters : Factory->RegisteredParameters())
		{
			TSharedPtr< FUICommandInfo > AddNode;
			FUICommandInfo::MakeCommandInfo(
				this->AsShared(),
				AddNode,
				Parameters.TypeName,
				LOCTEXT("DataflowButton", "New Dataflow Node"),
				LOCTEXT("NewDataflowNodeTooltip", "New Dataflow Node Tooltip"),
				FSlateIcon(),
				EUserInterfaceActionType::Button,
				FInputChord()
			);
			CreateNodesMap.Add(Parameters.TypeName, AddNode);
		}
	}

		
}

const FDataflowEditorCommandsImpl& FDataflowEditorCommands::Get()
{
	return FDataflowEditorCommandsImpl::Get();
}

void FDataflowEditorCommands::Register()
{
	return FDataflowEditorCommandsImpl::Register();
}

void FDataflowEditorCommands::Unregister()
{
	return FDataflowEditorCommandsImpl::Unregister();
}



void FDataflowEditorCommands::EvaluateSelectedNodes(const FGraphPanelSelectionSet& SelectedNodes, FDataflowEditorCommands::FGraphEvaluationCallback Evaluate)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;
	if (!bIsInPIEOrSimulate)
	{
		for (UObject* Node : SelectedNodes)
		{
			if (UDataflowEdNode* EdNode = dynamic_cast<UDataflowEdNode*>(Node))
			{
				if (const TSharedPtr<Dataflow::FGraph> DataflowGraph = EdNode->GetDataflowGraph())
				{
					if (const TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(EdNode->GetDataflowNodeGuid()))
					{
						if (DataflowNode->bActive)
						{
							if (DataflowNode->GetOutputs().Num())
							{
								for (FDataflowConnection* NodeOutput : DataflowNode->GetOutputs())
								{
									Evaluate(DataflowNode.Get(), (FDataflowOutput*)NodeOutput);
								}
							}
							else
							{
								Evaluate(DataflowNode.Get(), nullptr);
							}
						}
					}
				}
			}
		}
	}
}

void FDataflowEditorCommands::EvaluateNode(Dataflow::FContext& Context, Dataflow::FTimestamp& OutLastNodeTimestamp,
	const UDataflow* Dataflow, const FDataflowNode* InNode, const FDataflowOutput* Output, FString NodeName)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;
	if (!bIsInPIEOrSimulate)
	{
		if (Dataflow)
		{
			const FDataflowNode* Node = InNode;
			if (Node == nullptr)
			{
				if (const TSharedPtr<const Dataflow::FGraph> Graph = Dataflow->GetDataflow())
				{
					if (TSharedPtr<const FDataflowNode> GraphNode = Graph->FindBaseNode(FName(NodeName)))
					{
						Node = GraphNode.Get();
					}
				}
			}

			if (Node != nullptr)
			{
				if (Output == nullptr)
				{
					if (Node->GetTimestamp() >= OutLastNodeTimestamp)
					{
						Context.Evaluate(Node, nullptr);
						OutLastNodeTimestamp = Context.GetTimestamp();
					}
				}
				else // Output != nullptr
				{
					if (!Context.HasData(Output->CacheKey(), Context.GetTimestamp()))
					{
						Context.Evaluate(Node, Output);
					}
				}
			}
		}
	}
}

void FDataflowEditorCommands::EvaluateTerminalNode(Dataflow::FContext& Context, Dataflow::FTimestamp& OutLastNodeTimestamp,
	const UDataflow* Dataflow, const FDataflowNode* InNode, const FDataflowOutput* Output, UObject* InAsset, FString NodeName)
{
	const bool bIsInPIEOrSimulate = GEditor->PlayWorld != NULL || GEditor->bIsSimulatingInEditor;
	if (!bIsInPIEOrSimulate)
	{
		if (Dataflow)
		{
			const FDataflowNode* Node = InNode;
			if (Node == nullptr)
			{
				if (const TSharedPtr<const Dataflow::FGraph> Graph = Dataflow->GetDataflow())
				{
					if (TSharedPtr<const FDataflowNode> GraphNode = Graph->FindBaseNode(FName(NodeName)))
					{
						Node = GraphNode.Get();
					}
				}
			}

			if (Node != nullptr)
			{
				if (Output == nullptr)
				{
					if (Node->GetTimestamp() >= OutLastNodeTimestamp)
					{
						Context.Evaluate(Node, nullptr);
						OutLastNodeTimestamp = Context.GetTimestamp();

						if (const FDataflowTerminalNode* TerminalNode = Node->AsType<const FDataflowTerminalNode>())
						{
							if (InAsset)
							{
								TerminalNode->SetAssetValue(InAsset, Context);
							}
						}
					}
				}
				else // Output != nullptr
				{
					if (!Context.HasData(Output->CacheKey(), Context.GetTimestamp()))
					{
						Context.Evaluate(Node, Output);
					}
				}
			}
		}
	}
}


bool FDataflowEditorCommands::OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage)
{
	if (GraphNode)
	{
		if (UDataflowEdNode* DataflowNode = Cast<UDataflowEdNode>(GraphNode))
		{
			if (TSharedPtr<Dataflow::FGraph> Graph = DataflowNode->GetDataflowGraph())
			{
				if( Graph->FindBaseNode(FName(NewText.ToString())).Get()==nullptr )
				{
					return true;
				}
			}
		}
		else if( Cast<UEdGraphNode_Comment>(GraphNode))
		{
			return true;
		}
	}
	OutErrorMessage = FText::FromString(FString::Printf(TEXT("Non-unique name for graph node (%s)"), *NewText.ToString()));
	return false;
}


void FDataflowEditorCommands::OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode)
{
	if (InCommitType == ETextCommit::OnCleared)
	{
		return;
	}

	if (GraphNode)
	{
		if (UDataflowEdNode* DataflowNode = Cast<UDataflowEdNode>(GraphNode))
		{
			if (TSharedPtr<Dataflow::FGraph> Graph = DataflowNode->GetDataflowGraph())
			{
				if (TSharedPtr<FDataflowNode> Node = Graph->FindBaseNode(DataflowNode->GetDataflowNodeGuid()))
				{
					GraphNode->Rename(*InNewText.ToString());
					Node->SetName(FName(InNewText.ToString()));
				}
			}
		}
		else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(GraphNode))
		{
			GraphNode->NodeComment = InNewText.ToString();
		}
	}
}

void FDataflowEditorCommands::OnAssetPropertyValueChanged(UDataflow* Graph, TSharedPtr<Dataflow::FEngineContext>& Context, Dataflow::FTimestamp& OutLastNodeTimestamp, const FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (ensureMsgf(Graph != nullptr, TEXT("Warning : Failed to find valid graph.")))
	{
		if (InPropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet ||
			InPropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove ||
			InPropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
		{
			if (InPropertyChangedEvent.GetPropertyName() == FName("Overrides_Key") ||
				InPropertyChangedEvent.GetPropertyName() == FName("Overrides"))
			{
				for (const TSharedPtr<FDataflowNode>& DataflowNode : Graph->Dataflow->GetNodes())
				{
					if (DataflowNode->IsA(FDataflowOverrideNode::StaticType()))
					{
						// TODO: For now we invalidate all the FDataflowOverrideNode nodes
						// Once the Variable system will be in place only the neccessary nodes
						// will be invalidated
						DataflowNode->Invalidate();
					}
				}
			}
		}
	}
}

void FDataflowEditorCommands::OnPropertyValueChanged(UDataflow* OutDataflow, TSharedPtr<Dataflow::FEngineContext>& Context, Dataflow::FTimestamp& OutLastNodeTimestamp, const FPropertyChangedEvent& InPropertyChangedEvent, const TSet<UObject*>& SelectedNodes)
{
	if (InPropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet)
	{
		TSharedPtr<const FDataflowNode> UpdatedNode = nullptr;
		if (OutDataflow && InPropertyChangedEvent.Property && InPropertyChangedEvent.Property->GetOwnerUObject())
		{
//			OutDataflow->MarkPackageDirty();
			OutDataflow->Modify();

			for (UObject* SelectedNode : SelectedNodes)
			{
				if (UDataflowEdNode* Node = Cast<UDataflowEdNode>(SelectedNode))
				{
					if (TSharedPtr<FDataflowNode> DataflowNode = Node->GetDataflowNode())
					{
						UpdatedNode = DataflowNode;
						DataflowNode->Invalidate();
					}
				}
			}
		}

		if (!UpdatedNode && Context)
		{
			// Some base properties dont link back to the parent, so just clobber the cache for now. 
			Context.Reset();
		}
		OutLastNodeTimestamp = Dataflow::FTimestamp::Invalid;
	}
}


void FDataflowEditorCommands::DeleteNodes(UDataflow* Graph, const FGraphPanelSelectionSet& SelectedNodes)
{
	if (ensureMsgf(Graph != nullptr, TEXT("Warning : Failed to find valid graph.")))
	{
		for (UObject* Node : SelectedNodes)
		{
			if (UDataflowEdNode* EdNode = dynamic_cast<UDataflowEdNode*>(Node))
			{
				if (const TSharedPtr<Dataflow::FGraph> DataflowGraph = EdNode->GetDataflowGraph())
				{
					Graph->RemoveNode(EdNode);
					if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(EdNode->GetDataflowNodeGuid()))
					{
						DataflowGraph->RemoveNode(DataflowNode);
					}
				}
			}
			else if (UEdGraphNode_Comment* CommentNode = dynamic_cast<UEdGraphNode_Comment*>(Node))
			{
				Graph->RemoveNode(CommentNode);
			}

			// Auto-rename node so that its current name is made available until it is garbage collected
			Node->Rename();
		}
	}
}

void FDataflowEditorCommands::OnSelectedNodesChanged(TSharedPtr<IStructureDetailsView> PropertiesEditor, UObject* Asset, UDataflow* Graph, const TSet<UObject*>& NewSelection)
{
	PropertiesEditor->SetStructureData(nullptr);

	if (Graph && PropertiesEditor)
	{
		if (const TSharedPtr<Dataflow::FGraph> DataflowGraph = Graph->GetDataflow())
		{
			FGraphPanelSelectionSet SelectedNodes = NewSelection;
			if (SelectedNodes.Num())
			{
				TArray<UObject*> Objects;
				for (UObject* SelectedObject : SelectedNodes)
				{
					if (UDataflowEdNode* EdNode = Cast<UDataflowEdNode>(SelectedObject))
					{
						if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(EdNode->GetDataflowNodeGuid()))
						{
							TSharedPtr<FStructOnScope> Struct(DataflowNode->NewStructOnScope());
							PropertiesEditor->SetStructureData(Struct);
						}
					}
				}
				
			}
		}
	}
}

void FDataflowEditorCommands::ToggleEnabledState(UDataflow* Graph)
{
}

static UEdGraphPin* GetPin(const UDataflowEdNode* Node, const EEdGraphPinDirection Direction, const FName Name)
{
	for (UEdGraphPin* Pin : Node->GetAllPins())
	{
		if (Pin->PinName == Name && Pin->Direction == Direction)
		{
			return Pin;
		}
	}

	return nullptr;
}

void FDataflowEditorCommands::DuplicateNodes(UDataflow* Graph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor, const FGraphPanelSelectionSet& SelectedNodes)
{
	if (ensureMsgf(Graph != nullptr, TEXT("Warning : Failed to find valid graph.")))
	{
		// Separate selected nodes into an array of UDataflowEdNodes and
		// an array of UEdGraphNode_Comments
		TSet<UDataflowEdNode*> SelectedEdNodes;
		TSet<UEdGraphNode_Comment*> SelectedEdCommentNodes;

		for (UObject* Node : SelectedNodes)
		{
			if (UDataflowEdNode* EdNode = dynamic_cast<UDataflowEdNode*>(Node))
			{
				SelectedEdNodes.Add(EdNode);
			}
			else if (UEdGraphNode_Comment* EdCommentNode = dynamic_cast<UEdGraphNode_Comment*>(Node))
			{
				SelectedEdCommentNodes.Add(EdCommentNode);
			}
		}

		FDataflowAssetEdit Edit = Graph->EditDataflow();
		if (Dataflow::FGraph* DataflowGraph = Edit.GetGraph())
		{
			// Process Dataflow nodes
			TSet<UDataflowEdNode*> DuplicatedEdNodes;
			FVector2D RefLocation;

			if (SelectedEdNodes.Num() > 0)
			{
				// Store the location of the first selected node for recreating spatial relationships
				RefLocation.X = SelectedEdNodes.Array()[0]->NodePosX; RefLocation.Y = SelectedEdNodes.Array()[0]->NodePosY;

				//
				// Create a map to record OriginalNode.Guid -> DuplicatedNode.Guid
				//
				TMap<FGuid, FGuid> NodeGuidMap;				// [OriginalDataflowNodeGuid -> DuplicatedDataflowNodeGuid]
				TMap<FGuid, UDataflowEdNode*> EdNodeMap;	// [OriginalDataflowNodeGuid -> DuplicatedEdNode]

				// Duplicate selected nodes
				for (UDataflowEdNode* SelectedEdNode : SelectedEdNodes)
				{
					if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(SelectedEdNode->DataflowNodeGuid))
					{
						if (TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode> DuplicateNodeAction = FAssetSchemaAction_Dataflow_DuplicateNode_DataflowEdNode::CreateAction(Graph, DataflowNode->GetType()))
						{
							DuplicateNodeAction->DataflowNodeToDuplicate = DataflowNode;

							FVector2D SelectedEdNodeLocation(SelectedEdNode->NodePosX, SelectedEdNode->NodePosY);
							FVector2D DeltaLocation = SelectedEdNodeLocation - RefLocation;

							if (UDataflowEdNode* NewEdNode = (UDataflowEdNode*)DuplicateNodeAction->PerformAction(Graph, nullptr, DataflowGraphEditor->GetPasteLocation() + DeltaLocation, false))
							{
								DuplicatedEdNodes.Add(NewEdNode);

								// Record Guids
								NodeGuidMap.Add(SelectedEdNode->DataflowNodeGuid, NewEdNode->DataflowNodeGuid);
								EdNodeMap.Add(SelectedEdNode->DataflowNodeGuid, NewEdNode);
							}
						}
					}
				}

				// Recreate connections between duplicated nodes
				for (UDataflowEdNode* SelectedEdNode : SelectedEdNodes)
				{
					if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(SelectedEdNode->DataflowNodeGuid))
					{
						FGuid DataflowNodeAGuid = DataflowNode->GetGuid();

						for (FDataflowOutput* Output : DataflowNode->GetOutputs())
						{
							for (FDataflowInput* Connection : Output->Connections)
							{
								const FName OutputputName = Connection->GetConnection()->GetName();

								// Check if the node on the end of the conmnection was duplicated
								FGuid DataflowNodeBGuid = Connection->GetOwningNode()->GetGuid();

								if (NodeGuidMap.Contains(DataflowNodeBGuid))
								{
									const FName InputputName = Connection->GetName();

									if (TSharedPtr<FDataflowNode> DuplicatedDataflowNodeA = DataflowGraph->FindBaseNode(NodeGuidMap[DataflowNodeAGuid]))
									{
										FDataflowOutput* OutputConnection = DuplicatedDataflowNodeA->FindOutput(OutputputName);

										if (TSharedPtr<FDataflowNode> DuplicatedDataflowNodeB = DataflowGraph->FindBaseNode(NodeGuidMap[DataflowNodeBGuid]))
										{
											FDataflowInput* InputConnection = DuplicatedDataflowNodeB->FindInput(InputputName);

											DataflowGraph->Connect(OutputConnection, InputConnection);

											// Connect the UDataflowEdNode FPins as well
											if (UEdGraphPin* OutputPin = GetPin(EdNodeMap[DataflowNodeAGuid], EEdGraphPinDirection::EGPD_Output, OutputputName))
											{
												if (UEdGraphPin* InputPin = GetPin(EdNodeMap[DataflowNodeBGuid], EEdGraphPinDirection::EGPD_Input, InputputName))
												{
													OutputPin->MakeLinkTo(InputPin);
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

			// Process Comment nodes
			TSet<UEdGraphNode*> DuplicatedEdCommentNodes;

			if (SelectedEdCommentNodes.Num() > 0)
			{
				if (SelectedEdNodes.Num() == 0)
				{
					RefLocation.X = SelectedEdCommentNodes.Array()[0]->NodePosX; RefLocation.Y = SelectedEdCommentNodes.Array()[0]->NodePosY;
				}

				const TSharedPtr<SGraphEditor>& InGraphEditor = (TSharedPtr<SGraphEditor>)DataflowGraphEditor;

				for (UEdGraphNode_Comment* SelectedCommentNode : SelectedEdCommentNodes)
				{
					if (TSharedPtr<FAssetSchemaAction_Dataflow_DuplicateCommentNode_DataflowEdNode> DuplicateCommentNodeAction = FAssetSchemaAction_Dataflow_DuplicateCommentNode_DataflowEdNode::CreateAction(Graph, InGraphEditor))
					{
						DuplicateCommentNodeAction->CommentNodeToDuplicate = SelectedCommentNode;

						FVector2D SelectedCommentNodeLocation(SelectedCommentNode->NodePosX, SelectedCommentNode->NodePosY);
						FVector2D DeltaLocation = SelectedCommentNodeLocation - RefLocation;

						if (UEdGraphNode* NewCommentNode = DuplicateCommentNodeAction->PerformAction(Graph, nullptr, DataflowGraphEditor->GetPasteLocation() + DeltaLocation, false))
						{
							DuplicatedEdCommentNodes.Add(NewCommentNode);
						}
					}
				}
			}

			// Update the selection in the Editor
			if (SelectedEdNodes.Num() > 0 || SelectedEdCommentNodes.Num() > 0)
			{
				DataflowGraphEditor->ClearSelectionSet();

				if (SelectedEdNodes.Num() > 0)
				{
					for (UDataflowEdNode* Node : DuplicatedEdNodes)
					{
						DataflowGraphEditor->SetNodeSelection(Node, true);
					}
				}

				if (SelectedEdCommentNodes.Num() > 0)
				{
					for (UEdGraphNode* Node : DuplicatedEdCommentNodes)
					{
						DataflowGraphEditor->SetNodeSelection(Node, true);
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
