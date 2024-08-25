// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorCommands.h"

#include "Dataflow/AssetDefinition_DataflowAsset.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorStyle.h"
#include "Dataflow/DataflowEditorUtil.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowOverrideNode.h"
#include "Dataflow/DataflowSCommentNode.h"
#include "Dataflow/DataflowSNode.h"
#include "DataflowEditorTools/DataflowEditorWeightMapPaintTool.h"
#include "EdGraphNode_Comment.h"
#include "EdGraph/EdGraphNode.h"
#include "Editor.h"
#include "IStructureDetailsView.h"
#include "Serialization/ObjectWriter.h"
#include "Serialization/ObjectReader.h"
#include "Dataflow/DataflowGraph.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#if WITH_EDITOR
#include "HAL/PlatformApplicationMisc.h"
#endif

#define LOCTEXT_NAMESPACE "DataflowEditorCommands"

const FString FDataflowEditorCommandsImpl::BeginWeightMapPaintToolIdentifier = TEXT("BeginWeightMapPaintTool");
const FString FDataflowEditorCommandsImpl::AddWeightMapNodeIdentifier = TEXT("AddWeightMapNode");

// @todo(brice) Remove Example Tools
//const FString FDataflowEditorCommandsImpl::BeginAttributeEditorToolIdentifier = TEXT("BeginAttributeEditorTool");
//const FString FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier = TEXT("BeginMeshSelectionTool");

FDataflowEditorCommandsImpl::FDataflowEditorCommandsImpl()
	: TBaseCharacterFXEditorCommands<FDataflowEditorCommandsImpl>("DataflowEditor", 
		LOCTEXT("ContextDescription", "Dataflow Editor"), 
		NAME_None,
		FDataflowEditorStyle::Get().GetStyleSetName())
{
}

void FDataflowEditorCommandsImpl::RegisterCommands()
{
	TBaseCharacterFXEditorCommands::RegisterCommands();
	
	UI_COMMAND(EvaluateNode, "Evaluate", "Trigger an evaluation of the selected node.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(CreateComment, "CreateComment", "Create a Comment node.", EUserInterfaceActionType::None, FInputChord());
	UI_COMMAND(ToggleEnabledState, "ToggleEnabledState", "Toggle node between Enabled/Disabled state.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ToggleObjectSelection, "ToggleObjectSelection", "Enable object selection in editor.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleFaceSelection, "ToggleFaceSelection", "Enable face selection in editor.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(ToggleVertexSelection, "ToggleVertexSelection", "Enable vertex selection in editor.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(AddOptionPin, "AddOptionPin", "Add an option pin to the selected nodes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(RemoveOptionPin, "RemoveOptionPin", "Remove the last option pin from the selected nodes.", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(ZoomToFitGraph, "ZoomToFitGraph", "Fit the graph in the graph editor viewport.", EUserInterfaceActionType::None, FInputChord(EKeys::F));

	UI_COMMAND(BeginWeightMapPaintTool, "Add Weight Map", "Paint weight maps on the mesh", EUserInterfaceActionType::None, FInputChord());
	UI_COMMAND(AddWeightMapNode, "Add Weight Map", "Paint weight maps on the mesh", EUserInterfaceActionType::Button, FInputChord());


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

void FDataflowEditorCommandsImpl::GetToolDefaultObjectList(TArray<UInteractiveTool*>& ToolCDOs)
{
	ToolCDOs.Add(GetMutableDefault<UDataflowEditorWeightMapPaintTool>());
}

void FDataflowEditorCommandsImpl::UpdateToolCommandBinding(UInteractiveTool* Tool, TSharedPtr<FUICommandList> UICommandList, bool bUnbind)
{
	if (FDataflowEditorCommandsImpl::IsRegistered())
	{
		if (bUnbind)
		{
			FDataflowEditorCommandsImpl::Get().UnbindActiveCommands(UICommandList);
		}
		else
		{
			FDataflowEditorCommandsImpl::Get().BindCommandsForCurrentTool(UICommandList, Tool);
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
						if (const FDataflowTerminalNode* TerminalNode = Node->AsType<const FDataflowTerminalNode>())
						{
							if (InAsset)
							{
								TerminalNode->SetAssetValue(InAsset, Context);  // Must set asset value before call to Evaluate
							}
						}

						Context.Evaluate(Node, nullptr);
						OutLastNodeTimestamp = Context.GetTimestamp();
					}
				}
				else // Output != nullptr
				{
					if (!Context.HasData(Output->CacheKey(), Context.GetTimestamp()))
					{
						if (const FDataflowTerminalNode* TerminalNode = Node->AsType<const FDataflowTerminalNode>())
						{
							if (InAsset)
							{
								TerminalNode->SetAssetValue(InAsset, Context);  // Must set asset value before call to Evaluate
							}
						}

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

void FDataflowEditorCommands::OnAssetPropertyValueChanged(TObjectPtr<UDataflowBaseContent> Content, const FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (Content)
	{
		const TObjectPtr<UDataflow>& DataflowAsset = Content->GetDataflowAsset();
		if (DataflowAsset)
		{
			if (InPropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet ||
				InPropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayRemove ||
				InPropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayClear)
			{
				if (InPropertyChangedEvent.GetPropertyName() == FName("Overrides_Key") ||
					InPropertyChangedEvent.GetPropertyName() == FName("Overrides"))
				{
					if (ensureMsgf(DataflowAsset != nullptr, TEXT("Warning : Failed to find valid graph.")))
					{
						for (const TSharedPtr<FDataflowNode>& DataflowNode : DataflowAsset->Dataflow->GetNodes())
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
					else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(SelectedObject))
					{
						TSharedPtr<FStructOnScope> Struct(new FStructOnScope(UEdGraphNode_Comment::StaticClass(), (uint8*)CommentNode));
						PropertiesEditor->SetStructureData(Struct);
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

static void ShowNotificationMessage(const FText& Message, const SNotificationItem::ECompletionState CompletionState)
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 5.0f;
	Info.bUseLargeFont = false;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = false;
	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(CompletionState);
	}
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

			// Display message stating that nodes were duplicated
			const int32 NumDuplicatedNodes = SelectedEdNodes.Num();
			if (NumDuplicatedNodes > 0)
			{
				FText MessageFormat;
				if (NumDuplicatedNodes == 1)
				{
					MessageFormat = LOCTEXT("DataflowDuplicatedNodesSingleNode", "{0} node was duplicated");
				}
				else
				{
					MessageFormat = LOCTEXT("DataflowDuplicatedNodesMultipleNodes", "{0} nodes were duplicated");
				}
				ShowNotificationMessage(FText::Format(MessageFormat, NumDuplicatedNodes), SNotificationItem::CS_Success);
			}

			// Display message stating that comment boxe(s) were duplicated
			const int32 NumDuplicatedComments = SelectedEdCommentNodes.Num();
			if (NumDuplicatedComments > 0)
			{
				FText MessageFormat;
				if (NumDuplicatedComments == 1)
				{
					MessageFormat = LOCTEXT("DataflowDuplicatedNodesToClipboardSingleComment", "{0} comment was duplicated");
				}
				else
				{
					MessageFormat = LOCTEXT("DataflowDuplicatedNodesToClipboardMultipleComments", "{0} comments were duplicated");
				}
				ShowNotificationMessage(FText::Format(MessageFormat, NumDuplicatedComments), SNotificationItem::CS_Success);
			}
		}
	}
}

void FDataflowEditorCommands::CopyNodes(UDataflow* InGraph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor, const FGraphPanelSelectionSet& InSelectedNodes)
{
	if (ensureMsgf(InGraph != nullptr, TEXT("Warning : Failed to find valid graph.")))
	{
		// 
		if (InSelectedNodes.Num() > 0)
		{
			// Separate selected nodes into an array of UDataflowEdNodes and
			// an array of UEdGraphNode_Comments
			TSet<UDataflowEdNode*> SelectedEdNodes;
			TSet<UEdGraphNode_Comment*> SelectedEdCommentNodes;

			for (UObject* Node : InSelectedNodes)
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

			FDataflowAssetEdit Edit = InGraph->EditDataflow();
			if (Dataflow::FGraph* DataflowGraph = Edit.GetGraph())
			{
				FDataflowCopyPasteContent CopyPasteContent;

				TSet<FGuid> NodeGuids;
				TArray<FDataflowInput*> NodeInputsToSave;

				// Build node data
				if (SelectedEdNodes.Num() > 0)
				{
					for (UDataflowEdNode* EdNode : SelectedEdNodes)
					{
						if (TSharedPtr<FDataflowNode> DataflowNode = DataflowGraph->FindBaseNode(EdNode->DataflowNodeGuid))
						{
							NodeGuids.Add(DataflowNode->GetGuid());
							NodeInputsToSave.Append(DataflowNode->GetInputs());

							FName NodeType = DataflowNode->GetType();
							FName NodeName = DataflowNode->GetName();

							FDataflowNodeData NodeData;

							NodeData.Type = NodeType.ToString();
							NodeData.Name = NodeName.ToString();
							NodeData.Position.X = EdNode->NodePosX;
							NodeData.Position.Y = EdNode->NodePosY;

							FString ContentString;

							TUniquePtr<FDataflowNode> DefaultElement;
							DataflowNode->TypedScriptStruct()->ExportText(ContentString, DataflowNode.Get(), DataflowNode.Get(), nullptr, PPF_None, nullptr);

							NodeData.Properties = ContentString;

							CopyPasteContent.NodeData.Add(NodeData);
						}
					}				
				}

				// Build comment node data
				if (SelectedEdCommentNodes.Num() > 0)
				{
					for (UEdGraphNode_Comment* CommentEdNode : SelectedEdCommentNodes)
					{
						FDataflowCommentNodeData CommentNodeData;

						CommentNodeData.Name = CommentEdNode->NodeComment;
						CommentNodeData.Size.X = CommentEdNode->NodeWidth;
						CommentNodeData.Size.Y = CommentEdNode->NodeHeight;
						CommentNodeData.Position.X = CommentEdNode->NodePosX;
						CommentNodeData.Position.Y = CommentEdNode->NodePosY;
						CommentNodeData.Color = CommentEdNode->CommentColor;
						CommentNodeData.FontSize = CommentEdNode->FontSize;

						CopyPasteContent.CommentNodeData.Add(CommentNodeData);
					}
				}

				// Build connection data
				for (FDataflowInput* Input : NodeInputsToSave)
				{
					if (!Input) continue;

					const FDataflowNode* InputNode = Input->GetOwningNode();
					if (!InputNode) continue;

					const FDataflowOutput* Output = Input->GetConnection();
					if (!Output) continue;

					const FDataflowNode* OutputNode = Output->GetOwningNode();
					if (!OutputNode) continue;

					if (!NodeGuids.Contains(OutputNode->GetGuid()))
						continue;

					FString InConnection = FString::Format(TEXT("/{0}:{1}"), {InputNode->GetName().ToString(), Input->GetName().ToString()});
					FString OutConnection = FString::Format(TEXT("/{0}:{1}"), { OutputNode->GetName().ToString(), Output->GetName().ToString() });

					FDataflowConnectionData DataflowConnectionData;

					DataflowConnectionData.In = InConnection;
					DataflowConnectionData.Out = OutConnection;

					CopyPasteContent.ConnectionData.Add(DataflowConnectionData);
				}

				FString ClipboardContent;
				FDataflowCopyPasteContent DefaultContent;

				FDataflowCopyPasteContent::StaticStruct()->ExportText(ClipboardContent, &CopyPasteContent, &DefaultContent, nullptr, PPF_None, nullptr);

				// Save to clipboard
				FPlatformApplicationMisc::ClipboardCopy(*ClipboardContent);

				// Display message stating that nodes were copied to clipboard
				const int32 NumCopiedNodes = CopyPasteContent.NodeData.Num();
				if (NumCopiedNodes > 0)
				{
					FText MessageFormat;
					if (NumCopiedNodes == 1)
					{
						MessageFormat = LOCTEXT("DataflowCopiedNodesToClipboardSingleNode", "{0} node was copied to clipboard");
					}
					else
					{
						MessageFormat = LOCTEXT("DataflowCopiedNodesToClipboardMultipleNodes", "{0} nodes were copied to clipboard");
					}
					ShowNotificationMessage(FText::Format(MessageFormat, NumCopiedNodes), SNotificationItem::CS_Success);
				}

				// Display message stating that comment boxe(s) were copied to clipboard
				const int32 NumCopiedComments = CopyPasteContent.CommentNodeData.Num();
				if (NumCopiedComments > 0)
				{
					FText MessageFormat;
					if (NumCopiedComments == 1)
					{
						MessageFormat = LOCTEXT("DataflowCopiedNodesToClipboardSingleComment", "{0} comment was copied to clipboard");
					}
					else
					{
						MessageFormat = LOCTEXT("DataflowCopiedNodesToClipboardMultipleComments", "{0} comments were copied to clipboard");
					}
					ShowNotificationMessage(FText::Format(MessageFormat, NumCopiedComments), SNotificationItem::CS_Success);
				}
			}
		}
	}
}

void FDataflowEditorCommands::PasteNodes(UDataflow* Graph, const TSharedPtr<SDataflowGraphEditor>& DataflowGraphEditor)
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	if (!ClipboardContent.IsEmpty())
	{
		FDataflowCopyPasteContent DefaultContent;
		FDataflowCopyPasteContent CopyPasteContent;

		FDataflowCopyPasteContent::StaticStruct()->ImportText(*ClipboardContent, &CopyPasteContent, nullptr, EPropertyPortFlags::PPF_None, nullptr, FDataflowCopyPasteContent::StaticStruct()->GetName(), true);

		FVector2D AppliedTranslation; // The translation that being applied to everything. for placing comment nodes

		// Paste nodes
		TSet<UDataflowEdNode*> PastedEdNodes;
		TSet<UEdGraphNode*> PastedEdCommentNodes;

		TMap<FString, UDataflowEdNode*> EdNodeMap;	// [NmaeOfNode -> DuplicatedEdNode]

		if (CopyPasteContent.NodeData.Num() > 0)
		{
			// Store the location of the first selected node for recreating spatial relationships
			FVector2D RefLocation;
			RefLocation.X = CopyPasteContent.NodeData[0].Position.X; RefLocation.Y = CopyPasteContent.NodeData[0].Position.Y;

			int32 Idx = 0;
			for (FDataflowNodeData NodeData : CopyPasteContent.NodeData)
			{
				FName NodeType = *NodeData.Type;

				if (TSharedPtr<FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode> PasteNodeAction = FAssetSchemaAction_Dataflow_PasteNode_DataflowEdNode::CreateAction(Graph, NodeType))
				{
					PasteNodeAction->NodeName = *NodeData.Name;
					PasteNodeAction->NodeProperties = NodeData.Properties;

					FVector2D NodeLocation(NodeData.Position.X, NodeData.Position.Y);
					FVector2D DeltaLocation = NodeLocation - RefLocation;

					if (Idx == 0)
					{
						AppliedTranslation = DataflowGraphEditor->GetPasteLocation() + DeltaLocation - NodeData.Position;
					}
					Idx++;

					if (UDataflowEdNode* NewEdNode = (UDataflowEdNode*)PasteNodeAction->PerformAction(Graph, nullptr, DataflowGraphEditor->GetPasteLocation() + DeltaLocation, false))
					{
						PastedEdNodes.Add(NewEdNode);

						EdNodeMap.Add(NodeData.Name, NewEdNode);
					}
				}
			}
		}

		// Paste Comment nodes
		if (CopyPasteContent.CommentNodeData.Num() > 0)
		{
			const TSharedPtr<SGraphEditor>& InGraphEditor = (TSharedPtr<SGraphEditor>)DataflowGraphEditor;

			int32 Idx = 0;

			for (FDataflowCommentNodeData CommentNodeData : CopyPasteContent.CommentNodeData)
			{
				if (TSharedPtr<FAssetSchemaAction_Dataflow_PasteCommentNode_DataflowEdNode> PasteCommentNodeAction = FAssetSchemaAction_Dataflow_PasteCommentNode_DataflowEdNode::CreateAction(Graph, InGraphEditor))
				{
					if (CopyPasteContent.NodeData.Num() == 0)
					{
						if (Idx == 0)
						{
							AppliedTranslation = DataflowGraphEditor->GetPasteLocation() - CommentNodeData.Position;
						}
					}
					Idx++;

					PasteCommentNodeAction->NodeName = *CommentNodeData.Name;
					PasteCommentNodeAction->Size.X = CommentNodeData.Size.X + 50; // Make it longer, because the nodes are longer after copying ('_copy' in their name)
					PasteCommentNodeAction->Size.Y = CommentNodeData.Size.Y + 30;
					PasteCommentNodeAction->Color = CommentNodeData.Color;
					PasteCommentNodeAction->FontSize = CommentNodeData.FontSize;

					FVector2D CommentNodeLocation(CommentNodeData.Position);
					FVector2D NewLocation = CommentNodeLocation + AppliedTranslation;

					if (UEdGraphNode* NewCommentNode = PasteCommentNodeAction->PerformAction(Graph, nullptr, NewLocation, false))
					{
						PastedEdCommentNodes.Add(NewCommentNode);
					}
				}
			}
		}

		// Recreate connections
		if (CopyPasteContent.ConnectionData.Num() > 0)
		{
			for (FDataflowConnectionData Connection : CopyPasteContent.ConnectionData)
			{
				FString NodeIn = FDataflowConnectionData::GetNode(Connection.In);
				FGuid GuidIn = (EdNodeMap[NodeIn])->DataflowNodeGuid;

				FString PropertyIn = FDataflowConnectionData::GetProperty(Connection.In);
				const FName InputputName = *PropertyIn;

				FString NodeOut = FDataflowConnectionData::GetNode(Connection.Out);
				FGuid GuidOut = (EdNodeMap[NodeOut])->DataflowNodeGuid;

				FString PropertyOut = FDataflowConnectionData::GetProperty(Connection.Out);
				const FName OutputputName = *PropertyOut;

				FDataflowAssetEdit Edit = Graph->EditDataflow();
				if (Dataflow::FGraph* DataflowGraph = Edit.GetGraph())
				{
					if (TSharedPtr<FDataflowNode> DataflowNodeFrom = DataflowGraph->FindBaseNode(GuidOut))
					{
						if (TSharedPtr<FDataflowNode> DataflowNodeTo = DataflowGraph->FindBaseNode(GuidIn))
						{
							FDataflowInput* InputConnection = DataflowNodeTo->FindInput(InputputName);
							FDataflowOutput* OutputConnection = DataflowNodeFrom->FindOutput(OutputputName);

							DataflowGraph->Connect(OutputConnection, InputConnection);

							if (UEdGraphPin* OutputPin = GetPin(EdNodeMap[NodeOut], EEdGraphPinDirection::EGPD_Output, OutputputName))
							{
								if (UEdGraphPin* InputPin = GetPin(EdNodeMap[NodeIn], EEdGraphPinDirection::EGPD_Input, InputputName))
								{
									OutputPin->MakeLinkTo(InputPin);
								}
							}
						}
					}
				}
			}
		}

		// Update the selection in the Editor
		if (PastedEdNodes.Num() > 0 || PastedEdCommentNodes.Num() > 0)
		{
			DataflowGraphEditor->ClearSelectionSet();

			for (UDataflowEdNode* Node : PastedEdNodes)
			{
				DataflowGraphEditor->SetNodeSelection(Node, true);
			}

			for (UEdGraphNode* Node : PastedEdCommentNodes)
			{
				DataflowGraphEditor->SetNodeSelection(Node, true);
			}
		}

		// Display message stating that nodes were pasted from clipboard
		const int32 NumPastedNodes = CopyPasteContent.NodeData.Num();
		if (NumPastedNodes > 0)
		{
			FText MessageFormat;
			if (NumPastedNodes == 1)
			{
				MessageFormat = LOCTEXT("DataflowPastedNodesFromClipboardSingleNode", "{0} node was pasted from clipboard");
			}
			else
			{
				MessageFormat = LOCTEXT("DataflowPastedNodesFromClipboardMultipleNodes", "{0} nodes were pasted from clipboard");
			}
			ShowNotificationMessage(FText::Format(MessageFormat, NumPastedNodes), SNotificationItem::CS_Success);
		}

		// Display message stating that comment boxe(s) were pasted to clipboard
		const int32 NumPastedComments = CopyPasteContent.CommentNodeData.Num();
		if (NumPastedComments > 0)
		{
			FText MessageFormat;
			if (NumPastedComments == 1)
			{
				MessageFormat = LOCTEXT("DataflowPastedNodesToClipboardSingleComment", "{0} comment was pasted from clipboard");
			}
			else
			{
				MessageFormat = LOCTEXT("DataflowPastedNodesToClipboardMultipleComments", "{0} comments were pasted from clipboard");
			}
			ShowNotificationMessage(FText::Format(MessageFormat, NumPastedComments), SNotificationItem::CS_Success);
		}
	}
}


#undef LOCTEXT_NAMESPACE
