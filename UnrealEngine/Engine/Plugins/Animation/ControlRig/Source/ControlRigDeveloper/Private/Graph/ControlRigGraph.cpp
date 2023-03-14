// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/ControlRigGraph.h"
#include "ControlRigBlueprint.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Graph/ControlRigGraphSchema.h"
#include "ControlRig.h"
#include "RigVMModel/RigVMGraph.h"
#include "ControlRigObjectVersion.h"
#include "Units/RigUnit.h"
#include "EdGraphNode_Comment.h"
#include "GraphEditAction.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "RigVMModel/Nodes/RigVMRerouteNode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigGraph)

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "ControlRigBlueprintUtils.h"
#include "BlueprintCompilationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#endif

#define LOCTEXT_NAMESPACE "ControlRigGraph"

UControlRigGraph::UControlRigGraph()
{
	bSuspendModelNotifications = false;
	bIsTemporaryGraphForCopyPaste = false;
	bIsSelecting = false;
	LastHierarchyTopologyVersion = INDEX_NONE;
	bIsFunctionDefinition = false;
}

FRigVMClient* UControlRigGraph::GetRigVMClient() const
{
	if (const IRigVMClientHost* Host = GetImplementingOuter<IRigVMClientHost>())
	{
		return (FRigVMClient*)Host->GetRigVMClient();
	}
	return nullptr;
}

FString UControlRigGraph::GetRigVMNodePath() const
{
	return ModelNodePath;
}

void UControlRigGraph::HandleRigVMGraphRenamed(const FString& InOldNodePath, const FString& InNewNodePath)
{
	static constexpr TCHAR NodePathPrefixFormat[] = TEXT("%s|");
	const FString OldPrefix = FString::Printf(NodePathPrefixFormat, *InOldNodePath); 
	const FString NewPrefix = FString::Printf(NodePathPrefixFormat, *InNewNodePath);
	
	if(ModelNodePath == InOldNodePath)
	{
		Modify();
		ModelNodePath = InNewNodePath;

		FString GraphName;
		if(!ModelNodePath.Split(TEXT("|"), nullptr, &GraphName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			GraphName = ModelNodePath;
		}
		GraphName.RemoveFromEnd(TEXT("::"));
		GraphName.RemoveFromStart(UControlRigBlueprint::RigVMModelPrefix);
		GraphName.TrimStartAndEndInline();

		if(GraphName.IsEmpty())
		{
			GraphName = UControlRigGraphSchema::GraphName_ControlRig.ToString(); 
		}
		GraphName = FRigVMClient::GetUniqueName(GetOuter(), *GraphName).ToString();

		Rename(*GraphName, nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);
	}
	else if(ModelNodePath.StartsWith(OldPrefix))
	{
		Modify();
		ModelNodePath = NewPrefix + ModelNodePath.RightChop(OldPrefix.Len() - 1);
	}
	else
	{
		return;
	}

	for(UEdGraphNode* Node : Nodes)
	{
		if(UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Node))
		{
			if(RigNode->ModelNodePath.StartsWith(OldPrefix))
			{
				RigNode->Modify();
				RigNode->ModelNodePath = NewPrefix + RigNode->ModelNodePath.RightChop(OldPrefix.Len() - 1);
			}
		}
	}
}

void UControlRigGraph::Initialize(UControlRigBlueprint* InBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	InBlueprint->OnModified().RemoveAll(this);
	InBlueprint->OnModified().AddUObject(this, &UControlRigGraph::HandleModifiedEvent);
	InBlueprint->OnVMCompiled().RemoveAll(this);
	InBlueprint->OnVMCompiled().AddUObject(this, &UControlRigGraph::HandleVMCompiledEvent);

	URigHierarchy* Hierarchy = InBlueprint->Hierarchy;

	if(UControlRig* ControlRig = Cast<UControlRig>(InBlueprint->GetObjectBeingDebugged()))
	{
		Hierarchy = ControlRig->GetHierarchy();
	}

	if(Hierarchy)
	{
		CacheNameLists(Hierarchy, &InBlueprint->DrawContainer, InBlueprint->ShapeLibraries);
	}
}

const UControlRigGraphSchema* UControlRigGraph::GetControlRigGraphSchema()
{
	return CastChecked<const UControlRigGraphSchema>(GetSchema());
}

#if WITH_EDITORONLY_DATA
void UControlRigGraph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FControlRigObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		if(Schema == nullptr || !Schema->IsChildOf(UControlRigGraphSchema::StaticClass()))
		{
			Schema = UControlRigGraphSchema::StaticClass();
		}
	}
}
#endif

#if WITH_EDITOR

TArray<TSharedPtr<FString>> UControlRigGraph::EmptyElementNameList;

void UControlRigGraph::CacheNameLists(URigHierarchy* InHierarchy, const FControlRigDrawContainer* DrawContainer, TArray<TSoftObjectPtr<UControlRigShapeLibrary>> ShapeLibraries)
{
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return;
	}

	check(InHierarchy);
	check(DrawContainer);

	if(LastHierarchyTopologyVersion != InHierarchy->GetTopologyVersion())
	{
		ElementNameLists.FindOrAdd(ERigElementType::All);
		ElementNameLists.FindOrAdd(ERigElementType::Bone);
		ElementNameLists.FindOrAdd(ERigElementType::Null);
		ElementNameLists.FindOrAdd(ERigElementType::Control);
		ElementNameLists.FindOrAdd(ERigElementType::Curve);
		ElementNameLists.FindOrAdd(ERigElementType::RigidBody);
		ElementNameLists.FindOrAdd(ERigElementType::Reference);

		TArray<TSharedPtr<FString>>& AllNameList = ElementNameLists.FindChecked(ERigElementType::All);
		TArray<TSharedPtr<FString>>& BoneNameList = ElementNameLists.FindChecked(ERigElementType::Bone);
		TArray<TSharedPtr<FString>>& NullNameList = ElementNameLists.FindChecked(ERigElementType::Null);
		TArray<TSharedPtr<FString>>& ControlNameList = ElementNameLists.FindChecked(ERigElementType::Control);
		TArray<TSharedPtr<FString>>& CurveNameList = ElementNameLists.FindChecked(ERigElementType::Curve);
		TArray<TSharedPtr<FString>>& RigidBodyNameList = ElementNameLists.FindChecked(ERigElementType::RigidBody);
		TArray<TSharedPtr<FString>>& ReferenceNameList = ElementNameLists.FindChecked(ERigElementType::Reference);
		
		CacheNameListForHierarchy<FRigBaseElement>(InHierarchy, AllNameList, false);
		CacheNameListForHierarchy<FRigBoneElement>(InHierarchy, BoneNameList, false);
		CacheNameListForHierarchy<FRigNullElement>(InHierarchy, NullNameList, false);
		CacheNameListForHierarchy<FRigControlElement>(InHierarchy, ControlNameList, false);
		CacheNameListForHierarchy<FRigControlElement>(InHierarchy, ControlNameListWithoutAnimationChannels, true);
		CacheNameListForHierarchy<FRigCurveElement>(InHierarchy, CurveNameList, false);
		CacheNameListForHierarchy<FRigRigidBodyElement>(InHierarchy, RigidBodyNameList, false);
		CacheNameListForHierarchy<FRigReferenceElement>(InHierarchy, ReferenceNameList, false);

		LastHierarchyTopologyVersion = InHierarchy->GetTopologyVersion();
	}
	CacheNameList<FControlRigDrawContainer>(*DrawContainer, DrawingNameList);

	EntryNameList.Reset();
	EntryNameList.Add(MakeShared<FString>(FName(NAME_None).ToString()));

	if(const UControlRigBlueprint* Blueprint = GetBlueprint())
	{
		const TArray<FName> EntryNames = Blueprint->GetRigVMClient()->GetEntryNames();
		for (const FName& EntryName : EntryNames)
		{
			EntryNameList.Add(MakeShared<FString>(EntryName.ToString()));
		}
	}

	ShapeNameList.Reset();
	ShapeNameList.Add(MakeShared<FString>(FName(NAME_None).ToString()));

	for(const TSoftObjectPtr<UControlRigShapeLibrary>& ShapeLibrary : ShapeLibraries)
	{
		if(ShapeLibrary.IsNull() || !ShapeLibrary.IsValid())
		{
			ShapeLibrary.LoadSynchronous();
		}

		if(ShapeLibrary.IsNull() || !ShapeLibrary.IsValid())
		{
			continue;
		}

		const bool bUseNameSpace = ShapeLibraries.Num() > 1;
		const FString NameSpace = bUseNameSpace ? ShapeLibrary->GetName() + TEXT(".") : FString();
		ShapeNameList.Add(MakeShared<FString>(NameSpace + ShapeLibrary->DefaultShape.ShapeName.ToString()));
		for (const FControlRigShapeDefinition& Shape : ShapeLibrary->Shapes)
		{
			ShapeNameList.Add(MakeShared<FString>(NameSpace + Shape.ShapeName.ToString()));
		}
	}
}

const TArray<TSharedPtr<FString>>* UControlRigGraph::GetElementNameList(ERigElementType InElementType) const
{
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return OuterGraph->GetElementNameList(InElementType);
	}
	
	if(InElementType == ERigElementType::None)
	{
		return &EmptyElementNameList;
	}
	
	if(!ElementNameLists.Contains(InElementType))
	{
		UControlRigBlueprint* Blueprint = GetBlueprint();
		if(Blueprint == nullptr)
		{
			return &EmptyElementNameList;
		}

		UControlRigGraph* MutableThis = (UControlRigGraph*)this;
		URigHierarchy* Hierarchy = Blueprint->Hierarchy;
		if(UControlRig* ControlRig = Cast<UControlRig>(Blueprint->GetObjectBeingDebugged()))
		{
			Hierarchy = ControlRig->GetHierarchy();
		}	
			
		MutableThis->CacheNameLists(Hierarchy, &Blueprint->DrawContainer, Blueprint->ShapeLibraries);
	}
	return &ElementNameLists.FindChecked(InElementType);
}

const TArray<TSharedPtr<FString>>* UControlRigGraph::GetElementNameList(URigVMPin* InPin) const
{
	if (InPin)
	{
		if (URigVMPin* ParentPin = InPin->GetParentPin())
		{
			if (ParentPin->GetCPPTypeObject() == FRigElementKey::StaticStruct())
			{
				if (URigVMPin* TypePin = ParentPin->FindSubPin(TEXT("Type")))
				{
					FString DefaultValue = TypePin->GetDefaultValue();
					if (!DefaultValue.IsEmpty())
					{
						ERigElementType Type = (ERigElementType)StaticEnum<ERigElementType>()->GetValueByNameString(DefaultValue);
						return GetElementNameList(Type);
					}
				}
			}
		}
	}

	return GetBoneNameList(nullptr);
}

const TArray<TSharedPtr<FString>> UControlRigGraph::GetSelectedElementsNameList() const
{
	TArray<TSharedPtr<FString>> Result;
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return OuterGraph->GetSelectedElementsNameList();
	}
	
	UControlRigBlueprint* Blueprint = GetBlueprint();
	if(Blueprint == nullptr)
	{
		return Result;
	}
	
	TArray<FRigElementKey> Keys = Blueprint->Hierarchy->GetSelectedKeys();
	for (const FRigElementKey& Key : Keys)
	{
		FString ValueStr;
		FRigElementKey::StaticStruct()->ExportText(ValueStr, &Key, nullptr, nullptr, PPF_None, nullptr);
		Result.Add(MakeShared<FString>(ValueStr));
	}
	
	return Result;
}

const TArray<TSharedPtr<FString>>* UControlRigGraph::GetDrawingNameList(URigVMPin* InPin) const
{
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return OuterGraph->GetDrawingNameList(InPin);
	}
	return &DrawingNameList;
}

const TArray<TSharedPtr<FString>>* UControlRigGraph::GetEntryNameList(URigVMPin* InPin) const
{
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return OuterGraph->GetEntryNameList(InPin);
	}
	return &EntryNameList;
}

const TArray<TSharedPtr<FString>>* UControlRigGraph::GetShapeNameList(URigVMPin* InPin) const
{
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return OuterGraph->GetShapeNameList(InPin);
	}
	return &ShapeNameList;
}


void UControlRigGraph::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (bSuspendModelNotifications)
	{
		return;
	}

	// only make sure to receive notifs for this graph - unless
	// we are on a template graph (used by node spawners)
	if (GetModel() != InGraph && TemplateController == nullptr)
	{
		return;
	}

	if(UControlRigGraphSchema* ControlRigSchema = (UControlRigGraphSchema*)GetControlRigGraphSchema())
	{
		ControlRigSchema->HandleModifiedEvent(InNotifType, InGraph, InSubject);
	}

	if(TemplateController)
	{
		switch(InNotifType)
		{
			case ERigVMGraphNotifType::NodeRemoved:
			case ERigVMGraphNotifType::PinTypeChanged:
			case ERigVMGraphNotifType::PinDefaultValueChanged:
			case ERigVMGraphNotifType::PinRemoved:
			case ERigVMGraphNotifType::PinRenamed:
			case ERigVMGraphNotifType::VariableRemoved:
			case ERigVMGraphNotifType::VariableRenamed:
			case ERigVMGraphNotifType::GraphChanged:
			{
				return;
			}
			default:
			{
				break;
			}
		}
	}

	// increment the node topology version for any interaction
	// with a node.
	{
		UControlRigGraphNode* EdNode = nullptr;
		if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
		{
			EdNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelNode->GetFName()));
		}
		else if (URigVMPin* ModelPin = Cast<URigVMPin>(InSubject))
		{
			EdNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelPin->GetNode()->GetFName()));
		}

		if(EdNode)
		{
			EdNode->NodeTopologyVersion++;
		}
	}

	switch (InNotifType)
	{
		case ERigVMGraphNotifType::GraphChanged:
		{
			ModelNodePathToEdNode.Reset();

			for (URigVMNode* Node : InGraph->GetNodes())
			{
				UEdGraphNode* EdNode = FindNodeForModelNodeName(Node->GetFName(), false);
				if (EdNode != nullptr)
				{
					RemoveNode(EdNode);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeSelectionChanged:
		{
			if (bIsSelecting)
			{
				return;
			}
			TGuardValue<bool> SelectionGuard(bIsSelecting, true);

			TSet<const UEdGraphNode*> NodeSelection;
			for (FName NodeName : InGraph->GetSelectNodes())
			{
				if (UEdGraphNode* EdNode = FindNodeForModelNodeName(NodeName))
				{
					NodeSelection.Add(EdNode);
				}
			}
			SelectNodeSet(NodeSelection);
			break;
		}
		case ERigVMGraphNotifType::NodeAdded:
		{
			ModelNodePathToEdNode.Reset();
				
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				if (!ModelNode->IsVisibleInUI())
				{
					if (URigVMInjectionInfo* Injection = ModelNode->GetInjectionInfo())
					{
						if (URigVMPin* ModelPin = Injection->GetPin())
						{
							URigVMNode* ParentModelNode = ModelPin->GetNode();
							if (ParentModelNode)
							{
								UEdGraphNode* EdNode = FindNodeForModelNodeName(ParentModelNode->GetFName());
								if (EdNode)
								{
									if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(EdNode))
									{
										RigNode->ReconstructNode_Internal(true);
									}
								}
							}
						}
					}
					break;
				}

				if (URigVMCommentNode* CommentModelNode = Cast<URigVMCommentNode>(ModelNode))
				{
					UEdGraphNode_Comment* NewNode = NewObject<UEdGraphNode_Comment>(this, CommentModelNode->GetFName());
					AddNode(NewNode, false, false);

					NewNode->CreateNewGuid();
					NewNode->PostPlacedNewNode();
					NewNode->AllocateDefaultPins();

					NewNode->NodePosX = ModelNode->GetPosition().X;
					NewNode->NodePosY = ModelNode->GetPosition().Y;
					NewNode->NodeWidth = ModelNode->GetSize().X;
					NewNode->NodeHeight = ModelNode->GetSize().Y;
					NewNode->CommentColor = ModelNode->GetNodeColor();
					NewNode->NodeComment = CommentModelNode->GetCommentText();
					NewNode->SetFlags(RF_Transactional);
					NewNode->GetNodesUnderComment();
				}
				else if (URigVMRerouteNode* RerouteModelNode = Cast<URigVMRerouteNode>(ModelNode))
				{
					UControlRigGraphNode* NewNode = NewObject<UControlRigGraphNode>(this, GetControlRigGraphSchema()->GetGraphNodeClass(), ModelNode->GetFName());
					AddNode(NewNode, false, false);

					NewNode->ModelNodePath = ModelNode->GetNodePath();
					NewNode->CreateNewGuid();
					NewNode->PostPlacedNewNode();
					NewNode->AllocateDefaultPins();

					NewNode->NodePosX = ModelNode->GetPosition().X;
					NewNode->NodePosY = ModelNode->GetPosition().Y;

					NewNode->SetFlags(RF_Transactional);
					NewNode->AllocateDefaultPins();

					if (UEdGraphPin* ValuePin = NewNode->FindPin(ModelNode->FindPin("Value")->GetPinPath()))
					{
						NewNode->SetColorFromModel(GetSchema()->GetPinTypeColor(ValuePin->PinType));
					}
				}
				else // struct, library, parameter + variable
				{
					UControlRigGraphNode* NewNode = NewObject<UControlRigGraphNode>(this, GetControlRigGraphSchema()->GetGraphNodeClass(), ModelNode->GetFName());
					AddNode(NewNode, false, false);

					NewNode->ModelNodePath = ModelNode->GetNodePath();
					NewNode->CreateNewGuid();
					NewNode->PostPlacedNewNode();
					NewNode->AllocateDefaultPins();

					NewNode->NodePosX = ModelNode->GetPosition().X;
					NewNode->NodePosY = ModelNode->GetPosition().Y;
					NewNode->SetColorFromModel(ModelNode->GetNodeColor());
					NewNode->SetFlags(RF_Transactional);
					NewNode->AllocateDefaultPins();
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeRemoved:
		{
			ModelNodePathToEdNode.Reset();

			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				if (URigVMInjectionInfo* Injection = ModelNode->GetInjectionInfo())
				{
					if (URigVMPin* ModelPin = Injection->GetPin())
					{
						URigVMNode* ParentModelNode = ModelPin->GetNode();
						if (ParentModelNode)
						{
							UEdGraphNode* EdNode = FindNodeForModelNodeName(ParentModelNode->GetFName());
							if (EdNode)
							{
								if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(EdNode))
								{
									RigNode->ReconstructNode_Internal(true);
								}
							}
						}
					}
					break;
				}

				UEdGraphNode* EdNode = FindNodeForModelNodeName(ModelNode->GetFName(), false);
				if (EdNode)
				{
					RemoveNode(EdNode);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodePositionChanged:
		{
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				UEdGraphNode* EdNode = FindNodeForModelNodeName(ModelNode->GetFName());
				if (EdNode)
				{
					// No need to call Node->Modify(), since control rig has its own undo/redo system see RigVMControllerActions.cpp
					EdNode->NodePosX = (int32)ModelNode->GetPosition().X;
					EdNode->NodePosY = (int32)ModelNode->GetPosition().Y;
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeSizeChanged:
		{
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				UEdGraphNode_Comment* EdNode = Cast<UEdGraphNode_Comment>(FindNodeForModelNodeName(ModelNode->GetFName()));
				if (EdNode)
				{
					// No need to call Node->Modify(), since control rig has its own undo/redo system see RigVMControllerActions.cpp
					EdNode->NodeWidth = (int32)ModelNode->GetSize().X;
					EdNode->NodeHeight = (int32)ModelNode->GetSize().Y;
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeDescriptionChanged:
		case ERigVMGraphNotifType::NodeCategoryChanged:
		{
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelNode->GetFName())))
				{
					RigNode->InvalidateNodeTitle();
					RigNode->ReconstructNode_Internal(true);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::RerouteCompactnessChanged:
		{
			if (URigVMRerouteNode* ModelNode = Cast<URigVMRerouteNode>(InSubject))
			{
				UEdGraphNode* EdNode = Cast<UEdGraphNode>(FindNodeForModelNodeName(ModelNode->GetFName()));
				if (EdNode)
				{
					if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(EdNode))
					{
						// start at index 2 (the subpins below the top level value pin)
						// and hide the pins (or show them if they were hidden previously)
						for (int32 PinIndex = 2; PinIndex < RigNode->Pins.Num(); PinIndex++)
						{
							RigNode->Pins[PinIndex]->bHidden = !ModelNode->GetShowsAsFullNode();
						}
						NotifyGraphChanged(FEdGraphEditAction(EEdGraphActionType::GRAPHACTION_RemoveNode, EdNode->GetGraph(), EdNode, true));
						NotifyGraphChanged(FEdGraphEditAction(EEdGraphActionType::GRAPHACTION_AddNode, EdNode->GetGraph(), EdNode, true));
					}
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeColorChanged:
		{
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				if (ModelNode->IsA<URigVMLibraryNode>() || ModelNode->IsA<URigVMTemplateNode>())
				{
					if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelNode->GetFName())))
					{
						RigNode->SetColorFromModel(ModelNode->GetNodeColor());
					}
				}
				else if(UEdGraphNode_Comment * EdComment = Cast<UEdGraphNode_Comment>(FindNodeForModelNodeName(ModelNode->GetFName())))
				{
					EdComment->CommentColor = ModelNode->GetNodeColor();
				}
			}
			break;
		}
		case ERigVMGraphNotifType::CommentTextChanged:
		{
			if (URigVMCommentNode* ModelNode = Cast<URigVMCommentNode>(InSubject))
			{
				UEdGraphNode_Comment* EdNode = Cast<UEdGraphNode_Comment>(FindNodeForModelNodeName(ModelNode->GetFName()));
				if (EdNode)
				{
					EdNode->OnUpdateCommentText(ModelNode->GetCommentText());
					EdNode->FontSize = ModelNode->GetCommentFontSize();
					EdNode->bCommentBubbleVisible = ModelNode->GetCommentBubbleVisible();
					EdNode->bCommentBubbleVisible_InDetailsPanel = ModelNode->GetCommentBubbleVisible();
					EdNode->bColorCommentBubble = ModelNode->GetCommentColorBubble();
				}
			}
			break;
		}
		case ERigVMGraphNotifType::LinkAdded:
		case ERigVMGraphNotifType::LinkRemoved:
		{
			bool AddLink = InNotifType == ERigVMGraphNotifType::LinkAdded;

			if (URigVMLink* Link = Cast<URigVMLink>(InSubject))
			{
				URigVMPin* SourcePin = Link->GetSourcePin();
				URigVMPin* TargetPin = Link->GetTargetPin();

				if (SourcePin)
				{
					SourcePin = SourcePin->GetOriginalPinFromInjectedNode();
				}
				if (TargetPin)
				{
					TargetPin = TargetPin->GetOriginalPinFromInjectedNode();
				}

				if (SourcePin && TargetPin && SourcePin != TargetPin)
				{
					UControlRigGraphNode* SourceRigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(SourcePin->GetNode()->GetFName()));
					UControlRigGraphNode* TargetRigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(TargetPin->GetNode()->GetFName()));

					if (SourceRigNode != nullptr && TargetRigNode != nullptr)
					{
						FString SourcePinPath = SourcePin->GetPinPath();
						FString TargetPinPath = TargetPin->GetPinPath();
						UEdGraphPin* SourceRigPin = SourceRigNode->FindPin(*SourcePinPath, EGPD_Output);
						UEdGraphPin* TargetRigPin = TargetRigNode->FindPin(*TargetPinPath, EGPD_Input);

						if (SourceRigPin != nullptr && TargetRigPin != nullptr)
						{
							if (AddLink)
							{
								SourceRigPin->MakeLinkTo(TargetRigPin);
							}
							else
							{
								SourceRigPin->BreakLinkTo(TargetRigPin);
							}

							SourceRigPin->LinkedTo.Remove(nullptr);
							TargetRigPin->LinkedTo.Remove(nullptr);
						}
					}
				}
			}
			break;
		}
		case ERigVMGraphNotifType::PinDefaultValueChanged:
		{
			if (URigVMPin* ModelPin = Cast<URigVMPin>(InSubject))
			{
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelPin->GetNode()->GetFName())))
				{
					UEdGraphPin* RigNodePin = RigNode->FindPin(ModelPin->GetPinPath());
					if (RigNodePin == nullptr)
					{
						if(ModelPin->GetNode()->IsEvent())
						{
							RigNode->InvalidateNodeTitle();
						}
						break;
					}

					RigNode->SetupPinDefaultsFromModel(RigNodePin);

					if (Cast<URigVMVariableNode>(ModelPin->GetNode()))
					{
						if (ModelPin->GetName() == TEXT("Variable"))
						{
							RigNode->InvalidateNodeTitle();
							RigNode->ReconstructNode_Internal(true);
						}
					}
					else if (Cast<URigVMUnitNode>(ModelPin->GetNode()))
					{
						RigNode->InvalidateNodeTitle();

						// if the node contains a rig element key - invalidate the node
						if(RigNode->GetAllPins().ContainsByPredicate([](UEdGraphPin* Pin) -> bool
						{
							return Pin->PinType.PinSubCategoryObject == FRigElementKey::StaticStruct();
						}))
						{
							// we do this to enforce the refresh of the element name widgets
							RigNode->ReconstructNode_Internal(true);
						}
					}
				}
				else if (URigVMInjectionInfo* Injection = ModelPin->GetNode()->GetInjectionInfo())
				{
					if (Injection->InputPin != ModelPin->GetRootPin())
					{
						if (URigVMPin* InjectionPin = Injection->GetPin())
						{
							URigVMNode* ParentModelNode = InjectionPin->GetNode();
							if (ParentModelNode)
							{
								UEdGraphNode* HostEdNode = FindNodeForModelNodeName(ParentModelNode->GetFName());
								if (HostEdNode)
								{
									if (UControlRigGraphNode* HostRigNode = Cast<UControlRigGraphNode>(HostEdNode))
									{
										HostRigNode->ReconstructNode_Internal(true);
									}
								}
							}
						}
					}
				}
			}
			break;
		}
		case ERigVMGraphNotifType::PinArraySizeChanged:
		case ERigVMGraphNotifType::PinDirectionChanged:
		case ERigVMGraphNotifType::PinTypeChanged:
		case ERigVMGraphNotifType::PinIndexChanged:
		case ERigVMGraphNotifType::PinBoundVariableChanged:
		case ERigVMGraphNotifType::PinAdded:
		case ERigVMGraphNotifType::PinRemoved:
		case ERigVMGraphNotifType::PinRenamed:
		{
			if (URigVMPin* ModelPin = Cast<URigVMPin>(InSubject))
			{
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelPin->GetNode()->GetFName())))
				{
					RigNode->ReconstructNode_Internal(true);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeRenamed:
		{
			ModelNodePathToEdNode.Reset();
				
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelNode->GetPreviousFName())))
				{
					RigNode->Rename(*ModelNode->GetName());
					RigNode->ModelNodePath = ModelNode->GetNodePath();
					RigNode->InvalidateNodeTitle();
					RigNode->ReconstructNode_Internal(true);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::VariableRenamed:
		case ERigVMGraphNotifType::NodeReferenceChanged:
		{
			if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
			{
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(FindNodeForModelNodeName(ModelNode->GetFName())))
				{
					RigNode->InvalidateNodeTitle();
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeSelected:
		{
			if (URigVMCommentNode* ModelNode = Cast<URigVMCommentNode>(InSubject))
			{
				// UEdGraphNode_Comment cannot access RigVMCommentNode's selection state, so we have to manually toggle its selection state
				// UControlRigGraphNode does not need this step because it overrides the IsSelectedInEditor() method
				UEdGraphNode_Comment* EdNode = Cast<UEdGraphNode_Comment>(FindNodeForModelNodeName(ModelNode->GetFName()));
				if (EdNode)
				{
					EdNode->SetSelectionState(UEdGraphNode_Comment::ESelectionState::Selected);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeDeselected:
		{
			if (URigVMCommentNode* ModelNode = Cast<URigVMCommentNode>(InSubject))
			{
				UEdGraphNode_Comment* EdNode = Cast<UEdGraphNode_Comment>(FindNodeForModelNodeName(ModelNode->GetFName()));
				if (EdNode)
				{
					EdNode->SetSelectionState(UEdGraphNode_Comment::ESelectionState::Deselected);
				}
			}
			break;
		}
		case ERigVMGraphNotifType::PinExpansionChanged:
		default:
		{
			break;
		}
	}
}

int32 UControlRigGraph::GetInstructionIndex(const UControlRigGraphNode* InNode, bool bAsInput)
{
	if (const TPair<int32, int32>* FoundIndex = CachedInstructionIndices.Find(InNode->GetModelNode()))
	{
		return bAsInput ? FoundIndex->Key : FoundIndex->Value;
	}

	struct Local
	{
		static int32 GetInstructionIndex(URigVMNode* InModelNode, const FRigVMByteCode* InByteCode, TMap<URigVMNode*, TPair<int32, int32>>& Indices, bool bAsInput)
		{
			if (InModelNode == nullptr)
			{
				return INDEX_NONE;
			}

			if (const TPair<int32, int32>* ExistingIndex = Indices.Find(InModelNode))
			{
				const int32 Index = bAsInput ? ExistingIndex->Key : ExistingIndex->Value;
				if(Index != INDEX_NONE)
				{
					return Index;
				}
			}

			if(const URigVMRerouteNode* RerouteNode = Cast<URigVMRerouteNode>(InModelNode))
			{
				int32 InstructionIndex = INDEX_NONE;
				if(bAsInput)
				{
					TArray<URigVMNode*> SourceNodes = RerouteNode->GetLinkedSourceNodes();
					for(URigVMNode* SourceNode : SourceNodes)
					{
						InstructionIndex = GetInstructionIndex(SourceNode, InByteCode, Indices, bAsInput);
						if(InstructionIndex != INDEX_NONE)
						{
							break;
						}
					}
					Indices.FindOrAdd(InModelNode).Key = InstructionIndex;
				}
				else
				{
					TArray<URigVMNode*> TargetNodes = RerouteNode->GetLinkedTargetNodes();
					for(URigVMNode* TargetNode : TargetNodes)
					{
						InstructionIndex = GetInstructionIndex(TargetNode, InByteCode, Indices, bAsInput);
						if(InstructionIndex != INDEX_NONE)
						{
							break;
						}
					}
					Indices.FindOrAdd(InModelNode).Value = InstructionIndex;
				}

				return InstructionIndex;
			}
			else if(URigVMFunctionEntryNode* EntryNode = Cast<URigVMFunctionEntryNode>(InModelNode))
			{
				int32 InstructionIndex = INDEX_NONE;
				if(!bAsInput)
				{
					TArray<URigVMNode*> TargetNodes = EntryNode->GetLinkedTargetNodes();
					for(URigVMNode* TargetNode : TargetNodes)
					{
						InstructionIndex = GetInstructionIndex(TargetNode, InByteCode, Indices, bAsInput);
						if(InstructionIndex != INDEX_NONE)
						{
							break;
						}
					}
					Indices.FindOrAdd(InModelNode).Key = InstructionIndex;
				}
				return InstructionIndex;
			}
			else if(URigVMFunctionReturnNode* ReturnNode = Cast<URigVMFunctionReturnNode>(InModelNode))
			{
				int32 InstructionIndex = INDEX_NONE;
				if(bAsInput)
				{
					TArray<URigVMNode*> SourceNodes = ReturnNode->GetLinkedSourceNodes();
					for(URigVMNode* SourceNode : SourceNodes)
					{
						InstructionIndex = GetInstructionIndex(SourceNode, InByteCode, Indices, bAsInput);
						if(InstructionIndex != INDEX_NONE)
						{
							break;
						}
					}
					Indices.FindOrAdd(InModelNode).Key = InstructionIndex;
				}
				return InstructionIndex;
			}

			Indices.FindOrAdd(InModelNode, TPair<int32, int32>(INDEX_NONE, INDEX_NONE));

			int32 InstructionIndex = InByteCode->GetFirstInstructionIndexForSubject(InModelNode);
			if (InstructionIndex != INDEX_NONE)
			{
				TPair<int32, int32> Pair(InstructionIndex, InstructionIndex);
				if(bAsInput)
				{
					Indices.FindOrAdd(InModelNode).Key = InstructionIndex;
				}
				else
				{
					Indices.FindOrAdd(InModelNode).Value = InstructionIndex;
				}
				return InstructionIndex;
			}

			FRigVMInstructionArray Instructions = InByteCode->GetInstructions();
			for (int32 i = 0; i < Instructions.Num(); ++i)
			{
				const FRigVMASTProxy Proxy = FRigVMASTProxy::MakeFromCallPath(InByteCode->GetCallPathForInstruction(i), InModelNode->GetRootGraph());
				if (Proxy.GetCallstack().Contains(InModelNode))
				{
					if(bAsInput)
					{
						Indices.FindOrAdd(InModelNode).Key = i;
					}
					else
					{
						Indices.FindOrAdd(InModelNode).Value = i;
					}
					return i;
				}
			}
			return INDEX_NONE;
		}
	};

	if (const FRigVMByteCode* ByteCode = GetController()->GetCurrentByteCode())
	{
		const int32 SourceInstructionIndex = Local::GetInstructionIndex(InNode->GetModelNode(), ByteCode, CachedInstructionIndices, true);
		const int32 TargetInstructionIndex = Local::GetInstructionIndex(InNode->GetModelNode(), ByteCode, CachedInstructionIndices, false);
		return bAsInput ? SourceInstructionIndex : TargetInstructionIndex;
	}

	return INDEX_NONE;
}

UEdGraphNode* UControlRigGraph::FindNodeForModelNodeName(const FName& InModelNodeName, const bool bCacheIfRequired)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if(UEdGraphNode** MappedNode = ModelNodePathToEdNode.Find(InModelNodeName))
	{
		return *MappedNode;
	}
	
	const FString InModelNodePath = InModelNodeName.ToString();
	for (UEdGraphNode* EdNode : Nodes)
	{
		if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(EdNode))
		{
			if (RigNode->ModelNodePath == InModelNodePath)
			{
				if (RigNode->GetOuter() == this)
				{
					if (bCacheIfRequired)
					{
						ModelNodePathToEdNode.Add(InModelNodeName, EdNode);
					}
					return EdNode;
				}
			}
		}
		else if (EdNode)
		{
			if (EdNode->GetFName() == InModelNodeName)
			{
				if (EdNode->GetOuter() == this)
				{
					if (bCacheIfRequired)
					{
						ModelNodePathToEdNode.Add(InModelNodeName, EdNode);
					}
					return EdNode;
				}
			}
		}
	}
	return nullptr;
}

UControlRigBlueprint* UControlRigGraph::GetBlueprint() const
{
	if (UControlRigGraph* OuterGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return OuterGraph->GetBlueprint();
	}
	return Cast<UControlRigBlueprint>(GetOuter());
}

URigVMGraph* UControlRigGraph::GetModel() const
{
	if(CachedModelGraph.IsValid())
	{
		return CachedModelGraph.Get();
	}
	
	if (const FRigVMClient* Client = GetRigVMClient())
	{
		URigVMGraph* Model = Client->GetModel(this);
		CachedModelGraph = Model;
		return Model;
	}
	return nullptr;
}

URigVMController* UControlRigGraph::GetController() const
{
	if (FRigVMClient* Client = GetRigVMClient())
	{
		return Client->GetOrCreateController(GetModel());
	}
	return nullptr;
}

const UControlRigGraph* UControlRigGraph::GetRootGraph() const
{
	if(const UControlRigGraph* ParentGraph = Cast<UControlRigGraph>(GetOuter()))
	{
		return ParentGraph->GetRootGraph();
	}
	return this;
}

void UControlRigGraph::RemoveNode(UEdGraphNode* InNode)
{
	// Make sure EdGraph is not part of the transaction
	TGuardValue<ITransaction*> TransactionGuard(GUndo, nullptr);
					
	// Rename the soon to be deleted object to a unique name, so that other objects can use
	// the old name
	FString DeletedName;
	{
		UObject* ExistingObject = nullptr;
		static int32 DeletedIndex = FMath::Rand();
		do
		{
			DeletedName = FString::Printf(TEXT("%s_Deleted_%d"), *InNode->GetName(), DeletedIndex++); 
			ExistingObject = StaticFindObject(/*Class=*/ NULL, this, *DeletedName, true);						
		}
		while (ExistingObject);
	}
	InNode->Rename(*DeletedName, nullptr, REN_ForceNoResetLoaders | REN_DontCreateRedirectors);	
	Super::RemoveNode(InNode);

	NotifyGraphChanged(FEdGraphEditAction(EEdGraphActionType::GRAPHACTION_RemoveNode, this, InNode, false));
}

URigVMController* UControlRigGraph::GetTemplateController()
{
	if (TemplateController == nullptr)
	{
		TemplateController = GetBlueprint()->GetTemplateController();
		TemplateController->OnModified().RemoveAll(this);
		TemplateController->OnModified().AddUObject(this, &UControlRigGraph::HandleModifiedEvent);
	}
	return TemplateController;
}

void UControlRigGraph::HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM)
{
	CachedInstructionIndices.Reset();
}

#endif

FControlRigPublicFunctionData UControlRigGraph::GetPublicFunctionData() const
{
	FControlRigPublicFunctionData Data;

	FString Prefix, ModelNodeName;
	if(!URigVMNode::SplitNodePathAtEnd(ModelNodePath, Prefix, ModelNodeName))
	{
		ModelNodeName = ModelNodePath;
	}
	Data.Name = *ModelNodeName;

	if(URigVMGraph* RigGraph = GetModel())
	{
		if(URigVMCollapseNode* FunctionNode = Cast<URigVMCollapseNode>(RigGraph->GetOuter()))
		{
			Data.Category = FunctionNode->GetNodeCategory();
			Data.Keywords = FunctionNode->GetNodeKeywords();
			
			for(URigVMPin* Pin : FunctionNode->GetPins())
			{
				FControlRigPublicFunctionArg Arg;
				Arg.Name = Pin->GetFName();
				Arg.bIsArray = Pin->IsArray();
				Arg.Direction = Pin->GetDirection();
				Arg.CPPType = *Pin->GetCPPType();
				if(Pin->GetCPPTypeObject())
				{
					Arg.CPPTypeObjectPath = *Pin->GetCPPTypeObject()->GetPathName();
				}
				Data.Arguments.Add(Arg);
			}
		}
	}

	return Data;
}

#undef LOCTEXT_NAMESPACE

