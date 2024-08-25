// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphConfig.h"

#include "Algo/Transform.h"
#include "CineCameraComponent.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Graph/MovieGraphEdge.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/Nodes/MovieGraphInputNode.h"
#include "Graph/Nodes/MovieGraphOutputNode.h"
#include "Graph/Nodes/MovieGraphGlobalOutputSettingNode.h"
#include "Graph/Nodes/MovieGraphRemoveRenderSettingNode.h"
#include "Graph/Nodes/MovieGraphRenderLayerNode.h"
#include "Graph/Nodes/MovieGraphSubgraphNode.h"
#include "Graph/Nodes/MovieGraphVariableNode.h"
#include "Graph/Nodes/MovieGraphSelectNode.h"
#include "Graph/MovieGraphUtils.h"
#include "MovieGraphUtils.h"
#include "MoviePipelineQueue.h"
#include "MovieRenderPipelineCoreModule.h"

#define LOCTEXT_NAMESPACE "MovieGraphConfig"

UMovieGraphConfig* UMovieGraphMember::GetOwningGraph() const
{
	return GetTypedOuter<UMovieGraphConfig>();
}

bool UMovieGraphMember::SetMemberName(const FString& InNewName)
{
	FText UnusedError;
	if (CanRename(FText::FromString(InNewName), UnusedError))
	{
		Name = InNewName;
		return true;
	}

	return false;
}

bool UMovieGraphMember::CanRename(const FText& InNewName, FText& OutError) const
{
	static const FString InvalidChars("\"',\n\r\t");
	
	if (InNewName.IsEmptyOrWhitespace())
	{
		OutError = LOCTEXT("InvalidMemberRename_Empty", "The name cannot be empty.");
		return false;
	}

	if (InNewName.ToString() == UMovieGraphNode::GlobalsPinNameString)
	{
		OutError = LOCTEXT("InvalidMemberRename_Globals", "The name cannot be 'Globals'.");
		return false;
	}

	const FString NewNameString = InNewName.ToString();
	if (!FName::IsValidXName(NewNameString, InvalidChars, &OutError))
	{
		return false;
	}

	return true;
}

bool UMovieGraphVariable::IsGlobal() const
{
	return IsA<UMovieGraphGlobalVariable>();
}

bool UMovieGraphVariable::IsDeletable() const
{
	return true;
}

bool UMovieGraphVariable::CanRename(const FText& InNewName, FText& OutError) const
{
	if (!Super::CanRename(InNewName, OutError))
	{
		return false;
	}

	if (const UMovieGraphConfig* Graph = GetOwningGraph())
	{
		constexpr bool bIncludeGlobal = true;
		if (!IsUniqueNameInMemberArray(InNewName, Graph->GetVariables(bIncludeGlobal)))
		{
			OutError = LOCTEXT("InvalidVariableRename_Exists", "A variable with this name already exists.");
			return false;
		}
	}

	return true;
}

bool UMovieGraphVariable::SetMemberName(const FString& InNewName)
{
	bool bSuccess = Super::SetMemberName(InNewName);

#if WITH_EDITOR
	OnMovieGraphVariableChangedDelegate.Broadcast(this);
#endif
	
	return bSuccess;
}

#if WITH_EDITOR
void UMovieGraphVariable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnMovieGraphVariableChangedDelegate.Broadcast(this);
}
#endif // WITH_EDITOR

UMovieGraphGlobalVariable::UMovieGraphGlobalVariable()
{
	bIsEditable = false;
}

bool UMovieGraphGlobalVariable::IsDeletable() const
{
	return false;
}

bool UMovieGraphGlobalVariable::CanRename(const FText& InNewName, FText& OutError) const
{
	return false;
}

UMovieGraphGlobalVariable_ShotName::UMovieGraphGlobalVariable_ShotName()
{
	Name = FString(TEXT("shot_name"));
	SetValueType(EMovieGraphValueType::String);
}

UMovieGraphGlobalVariable_SequenceName::UMovieGraphGlobalVariable_SequenceName()
{
	Name = FString(TEXT("seq_name"));
	SetValueType(EMovieGraphValueType::String);
}

UMovieGraphGlobalVariable_FrameNumber::UMovieGraphGlobalVariable_FrameNumber()
{
	Name = FString(TEXT("frame_num"));
	SetValueType(EMovieGraphValueType::Int32);
}

UMovieGraphGlobalVariable_CameraName::UMovieGraphGlobalVariable_CameraName()
{
	Name = FString(TEXT("camera_name"));
	SetValueType(EMovieGraphValueType::String);
}

void UMovieGraphGlobalVariable_ShotName::UpdateValue(const FMovieGraphTraversalContext* InTraversalContext, const UMovieGraphPipeline* InPipeline)
{
	const TArray<TObjectPtr<UMoviePipelineExecutorShot>>& ShotList = InPipeline->GetActiveShotList();
	
	if (ShotList.IsValidIndex(InTraversalContext->ShotIndex))
	{
		if (const TObjectPtr<UMoviePipelineExecutorShot>& Shot = ShotList[InTraversalContext->ShotIndex])
		{
			SetValueString(Shot->OuterName);
		}
	}
}

void UMovieGraphGlobalVariable_SequenceName::UpdateValue(const FMovieGraphTraversalContext* InTraversalContext, const UMovieGraphPipeline* InPipeline)
{
	SetValueString(InTraversalContext->Job->Sequence.GetAssetName());
}

void UMovieGraphGlobalVariable_FrameNumber::UpdateValue(const FMovieGraphTraversalContext* InTraversalContext, const UMovieGraphPipeline* InPipeline)
{
	SetValueInt32(InTraversalContext->Time.ShotFrameNumber.Value);
}

void UMovieGraphGlobalVariable_CameraName::UpdateValue(const FMovieGraphTraversalContext* InTraversalContext, const UMovieGraphPipeline* InPipeline)
{
	const TArray<TObjectPtr<UMoviePipelineExecutorShot>>& ShotList = InPipeline->GetActiveShotList();
	
	if (ShotList.IsValidIndex(InTraversalContext->ShotIndex))
	{
		if (const TObjectPtr<UMoviePipelineExecutorShot>& Shot = ShotList[InTraversalContext->ShotIndex])
		{
			SetValueString(Shot->InnerName);
		}
	}
}

bool UMovieGraphInput::IsDeletable() const
{
	// The input is deletable as long as it's not the Globals input
	return Name != UMovieGraphNode::GlobalsPinNameString;
}

bool UMovieGraphInput::CanRename(const FText& InNewName, FText& OutError) const
{
	if (!Super::CanRename(InNewName, OutError))
	{
		return false;
	}

	if (const UMovieGraphConfig* Graph = GetOwningGraph())
	{
		if (!IsUniqueNameInMemberArray(InNewName, Graph->GetInputs()))
		{
			OutError = LOCTEXT("InvalidInputRename_Exists", "An input with this name already exists.");
			return false;
		}
	}

	return true;
}

bool UMovieGraphInput::SetMemberName(const FString& InNewName)
{
	bool bSuccess = Super::SetMemberName(InNewName);

#if WITH_EDITOR
	OnMovieGraphInputChangedDelegate.Broadcast(this);
#endif
	return bSuccess;
}

#if WITH_EDITOR
void UMovieGraphInput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	OnMovieGraphInputChangedDelegate.Broadcast(this);
}
#endif // WITH_EDITOR

bool UMovieGraphOutput::IsDeletable() const
{
	// The output is deletable as long as it's not the Globals output
	return Name != UMovieGraphNode::GlobalsPinNameString;
}

bool UMovieGraphOutput::CanRename(const FText& InNewName, FText& OutError) const
{
	if (!Super::CanRename(InNewName, OutError))
	{
		return false;
	}

	if (const UMovieGraphConfig* Graph = GetOwningGraph())
	{
		if (!IsUniqueNameInMemberArray(InNewName, Graph->GetOutputs()))
		{
			OutError = LOCTEXT("InvalidOutputRename_Exists", "An output with this name already exists.");
			return false;
		}
	}

	return true;
}

bool UMovieGraphOutput::SetMemberName(const FString& InNewName)
{
	bool bSuccess = Super::SetMemberName(InNewName);

#if WITH_EDITOR
	OnMovieGraphOutputChangedDelegate.Broadcast(this);
#endif
	
	return bSuccess;
}

#if WITH_EDITOR
void UMovieGraphOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnMovieGraphOutputChangedDelegate.Broadcast(this);
}
#endif // WITH_EDITOR

UMovieGraphConfig::UMovieGraphConfig()
{
	InputNode = CreateDefaultSubobject<UMovieGraphInputNode>(TEXT("DefaultInputNode"));
	OutputNode = CreateDefaultSubobject<UMovieGraphOutputNode>(TEXT("DefaultOutputNode"));

	// Don't add default members in the ctor if this object is being loaded (ie, it's not a new object). Defer that
	// until PostLoad(), otherwise the default members may be overwritten when properties are loaded.
	const bool bIsNewObject = !HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad | RF_NeedPostLoad);
	if (bIsNewObject)
	{
		AddDefaultMembers();
		InputNode->UpdatePins();
		OutputNode->UpdatePins();

		// Offset the default output node so it doesn't overlap the default input node
#if WITH_EDITOR
		constexpr int32 OutputNodeOffset = 900;
		OutputNode->SetNodePosX(OutputNodeOffset);
#endif
	}
}

void UMovieGraphConfig::PostLoad()
{
	Super::PostLoad();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// TODO: When the graph has stabilized, we can remove this and replace with a system that solely performs
		// upgrades/deprecations. For now, we assume that each load of the graph should re-initialize all default
		// members.
		AddDefaultMembers();

		// Fire OnGraphVariablesChangedDelegate when the variable changes (name, value, type, etc)
#if WITH_EDITOR
		for (const TObjectPtr<UMovieGraphVariable>& Variable : Variables)
		{
			Variable->OnMovieGraphVariableChangedDelegate.AddWeakLambda(this, [this](UMovieGraphMember*)
			{
				OnGraphVariablesChangedDelegate.Broadcast();
			});
		}

		// Remove all null nodes
		AllNodes.RemoveAll(
			[](UMovieGraphNode* Node)
			{
				if (!Node)
				{
					UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Encountered invalid source node (nullptr) when building Movie Pipeline Editor graph, skipping creating an editor graph node for the invalid source node."))
					return true;
				}
				return false;
			});
#endif
	}
}

template<typename T>
T* UMovieGraphConfig::AddGlobalVariable()
{
	// Don't add duplicate global variables
	const bool VariableExists = GlobalVariables.ContainsByPredicate([](const TObjectPtr<UMovieGraphVariable>& Variable)
	{
		return Variable && (Variable->GetClass() == T::StaticClass());
	});

	if (VariableExists)
	{
		// Don't log here; graphs will typically try to add all available global variables on start-up, even if they
		// already exist in the current graph
		return nullptr;
	}

	// Pass an empty name to AddMember() since globals set their name upon construction
	return AddMember<T>(GlobalVariables, FName());
}

void UMovieGraphConfig::AddDefaultMembers()
{
	const bool InputGlobalsExists = Inputs.ContainsByPredicate([](const UMovieGraphMember* Member)
	{
		return Member && (Member->GetMemberName() == UMovieGraphNode::GlobalsPinNameString);
	});

	const bool OutputGlobalsExists = Outputs.ContainsByPredicate([](const UMovieGraphMember* Member)
	{
		return Member && (Member->GetMemberName() == UMovieGraphNode::GlobalsPinNameString);
	});

	// Ensure there is a Globals input member
	if (!InputGlobalsExists)
	{
		UMovieGraphInput* NewInput = AddInput();

		// Don't call SetMemberName() here, because that will reject setting the name to Globals
		NewInput->Name = UMovieGraphNode::GlobalsPinNameString;

		InputNode->UpdatePins();
	}

	// Ensure there is a Globals output member
	if (!OutputGlobalsExists)
	{
		UMovieGraphOutput* NewOutput = AddOutput();

		// Don't call SetMemberName() here, because that will reject setting the name to Globals
		NewOutput->Name = UMovieGraphNode::GlobalsPinNameString;

		OutputNode->UpdatePins();
	}

	AddGlobalVariable<UMovieGraphGlobalVariable_CameraName>();
	AddGlobalVariable<UMovieGraphGlobalVariable_FrameNumber>();
	AddGlobalVariable<UMovieGraphGlobalVariable_SequenceName>();
	AddGlobalVariable<UMovieGraphGlobalVariable_ShotName>();
}

bool UMovieGraphConfig::AddLabeledEdge(UMovieGraphNode* FromNode, const FName& FromPinLabel, UMovieGraphNode* ToNode, const FName& ToPinLabel)
{
	if (!FromNode || !ToNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: Invalid Edge Nodes"), __FUNCTION__),
				ELogVerbosity::Error);
		return false;
	}
	

	UMovieGraphPin* FromPin = FromNode->GetOutputPin(FromPinLabel);
	if (!FromPin)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: FromNode: %s does not have a pin with the label: %s"), __FUNCTION__, *FromNode->GetName(), *FromPinLabel.ToString()),
				ELogVerbosity::Error);
		return false;
	}

	UMovieGraphPin* ToPin = ToNode->GetInputPin(ToPinLabel);
	if (!ToPin)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: ToNode: %s does not have a pin with the label: %s"), __FUNCTION__, *ToNode->GetName(), *ToPinLabel.ToString()),
				ELogVerbosity::Error);
		return false;
	}

	if (!FromPin->CanCreateConnection(ToPin))
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: FromNode %s's pin %s cannot be connected to ToNode %s's pin %s "), __FUNCTION__, *FromNode->GetName(), *FromPinLabel.ToString(), *ToNode->GetName(), *ToPinLabel.ToString()),
				ELogVerbosity::Error);
		return false;
	}

	bool bConnectionBrokeOtherEdges = false;

	// Input pins can only have one edge connected to them at once, so if the pin we're connecting to already
	// has a connection, then we break the existing connection.
	if (!ToPin->AllowsMultipleConnections() && ToPin->EdgeCount() > 0)
	{
		ToPin->BreakAllEdges();
		bConnectionBrokeOtherEdges = true;
	}

	// Add the edge. We do this after the above
	// since that will break all edges first if there's already one.
	FromPin->AddEdgeTo(ToPin);
//
//#if WITH_EDITOR
//	NotifyGraphChanged(EMoviePipelineGraphChangeType::Structural);
//#endif

	return bConnectionBrokeOtherEdges;
}

bool UMovieGraphConfig::RemoveLabeledEdge(UMovieGraphNode* FromNode, const FName& FromPinLabel, UMovieGraphNode* ToNode, const FName& ToPinLabel)
{
	if (!FromNode || !ToNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(TEXT("%hs: Invalid Edge Nodes"), __FUNCTION__), ELogVerbosity::Error);
		return false;
	}

	UMovieGraphPin* FromPin = FromNode->GetOutputPin(FromPinLabel);
	if (!FromPin)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: FromNode: %s does not have a pin with the label: %s"), __FUNCTION__, *FromNode->GetName(), *FromPinLabel.ToString()),
				ELogVerbosity::Error);
		return false;
	}

	UMovieGraphPin* ToPin = ToNode->GetInputPin(ToPinLabel);
	if (!ToPin)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: ToNode: %s does not have a pin with the label: %s"), __FUNCTION__, *ToNode->GetName(), *ToPinLabel.ToString()),
				ELogVerbosity::Error);
		return false;
	}

	bool bChanged = ToPin->BreakEdgeTo(FromPin);

//#if WITH_EDITOR
// 	   if(bChanged) {
//	NotifyGraphChanged(EMoviePipelineGraphChangeType::Structural);
// 	   }
//#endif

	return bChanged;
}

bool UMovieGraphConfig::RemoveAllInboundEdges(UMovieGraphNode* InNode)
{
	if (!InNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(TEXT("%hs: Invalid Edge Nodes"), __FUNCTION__ ), ELogVerbosity::Error);
		return false;
	}

	bool bChanged = false;
	for (UMovieGraphPin* InputPin : InNode->InputPins)
	{
		bChanged |= InputPin->BreakAllEdges();
	}

	//#if WITH_EDITOR
	// 	   if(bChanged) {
	//	NotifyGraphChanged(EMoviePipelineGraphChangeType::Structural);
	// 	   }
	//#endif

	return bChanged;
}

bool UMovieGraphConfig::RemoveAllOutboundEdges(UMovieGraphNode* InNode)
{
	if (!InNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(TEXT("%hs: Invalid Edge Nodes"), __FUNCTION__), ELogVerbosity::Error);
		return false;
	}

	bool bChanged = false;
	for (UMovieGraphPin* OutputPin : InNode->OutputPins)
	{
		bChanged |= OutputPin->BreakAllEdges();
	}

	//#if WITH_EDITOR
	// 	   if(bChanged) {
	//	NotifyGraphChanged(EMoviePipelineGraphChangeType::Structural);
	// 	   }
	//#endif

	return bChanged;
}

bool UMovieGraphConfig::RemoveInboundEdges(UMovieGraphNode* InNode, const FName& InPinName)
{
	if (!InNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: Invalid Edge Nodes"), __FUNCTION__),
				ELogVerbosity::Error);
		return false;
	}

	bool bChanged = false;
	if(UMovieGraphPin* InputPin = InNode->GetInputPin(InPinName))
	{
		bChanged |= InputPin->BreakAllEdges();
	}

	//#if WITH_EDITOR
	// 	   if(bChanged) {
	//	NotifyGraphChanged(EMoviePipelineGraphChangeType::Structural);
	// 	   }
	//#endif

	return bChanged;
}

bool UMovieGraphConfig::RemoveOutboundEdges(UMovieGraphNode* InNode, const FName& InPinName)
{
	if (!InNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: Invalid Edge Nodes"), __FUNCTION__),
				ELogVerbosity::Error);
		return false;
	}

	bool bChanged = false;
	if (UMovieGraphPin* OutputPin = InNode->GetOutputPin(InPinName))
	{
		bChanged |= OutputPin->BreakAllEdges();
	}

	//#if WITH_EDITOR
	// 	   if(bChanged) {
	//	NotifyGraphChanged(EMoviePipelineGraphChangeType::Structural);
	// 	   }
	//#endif

	return bChanged;
}

void UMovieGraphConfig::AddNode(UMovieGraphNode* InNode)
{
	if (!InNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: No node was specified for add."), __FUNCTION__),
				ELogVerbosity::Error);
		return;
	}

	if (!InNode->CanBeAddedByUser())
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs:Cannot add node of type %s."), __FUNCTION__, *InNode->GetClass()->GetName()),
				ELogVerbosity::Error);
		return;
	}

	InNode->SetFlags(RF_Transactional);

	Modify();

	// Reparent node to this graph
	InNode->Rename(nullptr, this);

	AllNodes.Add(InNode);
}

bool UMovieGraphConfig::RemoveNodes(TArray<UMovieGraphNode*> InNodes)
{
	bool bChanged = false;
	for (UMovieGraphNode* Node : InNodes)
	{
		bChanged |= RemoveNode(Node);
	}
	return bChanged;
}

bool UMovieGraphConfig::RemoveNode(UMovieGraphNode* InNode)
{
	if (!InNode)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: Could not remove invalid InNode"), __FUNCTION__),
				ELogVerbosity::Error);
		return false;
	}

	Modify();

	RemoveAllInboundEdges(InNode);
	RemoveAllOutboundEdges(InNode);

#if WITH_EDITOR
	TArray<UMovieGraphNode*> RemovedNodes;
	RemovedNodes.Add(InNode);
	OnGraphNodesDeletedDelegate.Broadcast(RemovedNodes);
#endif

	return AllNodes.RemoveSingle(InNode) == 1;
}

template<typename RetType, typename ArrType>
RetType* UMovieGraphConfig::AddMember(TArray<TObjectPtr<ArrType>>& InMemberArray, const FName& InBaseName)
{
	static_assert(std::is_base_of_v<UMovieGraphMember, RetType>, "RetType is not derived from UMovieGraphMember");
	
	Modify();
	
	// TODO: This can be replaced with just CreateDefaultSubobject() when AddDefaultMembers() isn't called from PostLoad()
	//
	// This method will be called in two cases: 1) when default members are being added to a new graph when it is being
	// initially created or loaded via PostLoad(), or 2) a member is being added to the graph by the user. For case 1,
	// when the constructor is running, RF_NeedInitialization will be set. CreateDefaultSubobject() needs to be called
	// in this scenario instead of NewObject().
	const bool bIsNewObject = HasAnyFlags(RF_NeedInitialization);
	RetType* NewMember = bIsNewObject
		? CreateDefaultSubobject<RetType>(MakeUniqueObjectName(this, RetType::StaticClass()))
		: NewObject<RetType>(this, NAME_None);
	
	if (!NewMember)
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: Unable to create new member object in the graph."), __FUNCTION__),
				ELogVerbosity::Error);
		return nullptr;
	}

	InMemberArray.Add(NewMember);
	NewMember->SetFlags(RF_Transactional);
	NewMember->SetGuid(FGuid::NewGuid());

	// Generate and set a unique name. Globals set their name at construction time, so no need to set their name.
	if (!NewMember->template IsA<UMovieGraphGlobalVariable>())
	{
		TArray<FString> ExistingMemberNames;
		Algo::Transform(InMemberArray, ExistingMemberNames, [](const ArrType* Member) { return Member->GetMemberName(); });
		NewMember->SetMemberName(UE::MovieGraph::GetUniqueName(ExistingMemberNames, InBaseName.ToString()));
	}

	return NewMember;
}

UMovieGraphVariable* UMovieGraphConfig::AddVariable(const FName InCustomBaseName)
{
	static const FText VariableBaseName = LOCTEXT("VariableBaseName", "Variable");
	
	UMovieGraphVariable* NewVariable = AddMember<UMovieGraphVariable>(
		Variables, !InCustomBaseName.IsNone() ? InCustomBaseName : FName(*VariableBaseName.ToString()));

	if (NewVariable)
	{
		// Fire OnGraphVariablesChangedDelegate when the variable changes (name, value, type, etc)
#if WITH_EDITOR
		NewVariable->OnMovieGraphVariableChangedDelegate.AddWeakLambda(this, [this](UMovieGraphMember*)
		{
			OnGraphVariablesChangedDelegate.Broadcast();
		});
#endif
	}

#if WITH_EDITOR
	OnGraphVariablesChangedDelegate.Broadcast();
#endif

	return NewVariable;
}

UMovieGraphInput* UMovieGraphConfig::AddInput()
{
	static const FText InputBaseName = LOCTEXT("InputBaseName", "Input");

	UMovieGraphInput* NewInput = AddMember<UMovieGraphInput>(Inputs, FName(*InputBaseName.ToString()));
	InputNode->UpdatePins();
	
#if WITH_EDITOR
	OnGraphInputAddedDelegate.Broadcast(NewInput);
#endif

	return NewInput;
}

UMovieGraphOutput* UMovieGraphConfig::AddOutput()
{
	static const FText OutputBaseName = LOCTEXT("OutputBaseName", "Output");
	
	UMovieGraphOutput* NewOutput = AddMember<UMovieGraphOutput>(Outputs, FName(*OutputBaseName.ToString()));
	OutputNode->UpdatePins();

#if WITH_EDITOR
	OnGraphOutputAddedDelegate.Broadcast(NewOutput);
#endif

	return NewOutput;
}

UMovieGraphVariable* UMovieGraphConfig::GetVariableByGuid(const FGuid& InGuid) const
{
	constexpr bool bIncludeGlobal = true;
	for (UMovieGraphVariable* Variable : GetVariables(bIncludeGlobal))
	{
		if (Variable->GetGuid() == InGuid)
		{
			return Variable;
		}
	}

	return nullptr;
}

TArray<UMovieGraphVariable*> UMovieGraphConfig::GetVariables(const bool bIncludeGlobal) const
{
	if (!bIncludeGlobal)
	{
		return Variables;
	}

	TArray<UMovieGraphVariable*> AllVariables = Variables;
	AllVariables.Append(GlobalVariables);

	return AllVariables;
}

void UMovieGraphConfig::UpdateGlobalVariableValues(const UMovieGraphPipeline* InPipeline)
{
	// Note: Although UpdateValue could get the traversal context from the pipeline itself, we fetch it once here
	// to prevent re-creating the context constantly.
	const FMovieGraphTraversalContext TraversalContext = InPipeline->GetCurrentTraversalContext();
	
	for (const TObjectPtr<UMovieGraphGlobalVariable>& GlobalVariable : GlobalVariables)
	{
		GlobalVariable->UpdateValue(&TraversalContext, InPipeline);
	}
}

TArray<UMovieGraphInput*> UMovieGraphConfig::GetInputs() const
{
	return Inputs;
}

TArray<UMovieGraphOutput*> UMovieGraphConfig::GetOutputs() const
{
	return Outputs;
}

bool UMovieGraphConfig::DeleteMember(UMovieGraphMember* MemberToDelete)
{
	if (!MemberToDelete)
	{
		return false;
	}

	if (!MemberToDelete->IsDeletable())
	{
		FFrame::KismetExecutionMessage(
			*FString::Printf(
				TEXT("%hs: The member '%s' cannot be deleted because it is flagged as non-deletable."), __FUNCTION__, *MemberToDelete->GetMemberName()),
				ELogVerbosity::Error);
		return false;
	}

	if (UMovieGraphVariable* GraphVariableToDelete = Cast<UMovieGraphVariable>(MemberToDelete))
	{
		return DeleteVariableMember(GraphVariableToDelete);
	}

	if (UMovieGraphInput* GraphInputToDelete = Cast<UMovieGraphInput>(MemberToDelete))
	{
		return DeleteInputMember(GraphInputToDelete);
	}

	if (UMovieGraphOutput* GraphOutputToDelete = Cast<UMovieGraphOutput>(MemberToDelete))
	{
		return DeleteOutputMember(GraphOutputToDelete);
	}

	return false;
}

bool UMovieGraphConfig::DeleteVariableMember(UMovieGraphVariable* VariableMemberToDelete)
{
	if (!VariableMemberToDelete)
	{
		return false;
	}

	Modify();
	
	// Find all accessor nodes using this graph variable
	TArray<TObjectPtr<UMovieGraphNode>> NodesToRemove =
		AllNodes.FilterByPredicate([VariableMemberToDelete](const TObjectPtr<UMovieGraphNode>& GraphNode)
		{
			if (const UMovieGraphVariableNode* VariableNode = Cast<UMovieGraphVariableNode>(GraphNode))
			{
				const UMovieGraphVariable* GraphVariable = VariableNode->GetVariable();
				if (GraphVariable && (GraphVariable->GetGuid() == VariableMemberToDelete->GetGuid()))
				{
					return true;
				}
			}

			return false;
		});

	// Remove accessor nodes (which broadcasts our node changed delegates)
	TArray<UMovieGraphNode*> RemovedNodes;
	for (const TObjectPtr<UMovieGraphNode>& NodeToRemove : NodesToRemove)
	{
		if (RemoveNode(NodeToRemove.Get()))
		{
			RemovedNodes.Add(NodeToRemove.Get());
		}
	}

	// Remove this variable from the variables tracked by the graph
	Variables.RemoveSingle(VariableMemberToDelete);

#if WITH_EDITOR
	OnGraphVariablesChangedDelegate.Broadcast();
#endif

	return true;
}

#if WITH_EDITOR
void UMovieGraphConfig::SetEditorOnlyNodes(const TArray<TObjectPtr<const UObject>>& InNodes)
{
	EditorOnlyNodes.Empty();

	for (const TObjectPtr<const UObject>& Node : InNodes)
	{
		EditorOnlyNodes.Add(DuplicateObject(Node.Get(), this));
	}
}
#endif	// WITH_EDITOR

bool UMovieGraphConfig::DeleteInputMember(UMovieGraphInput* InputMemberToDelete)
{
	if (InputMemberToDelete)
	{
		Modify();
		
		Inputs.RemoveSingle(InputMemberToDelete);
		RemoveOutboundEdges(InputNode, FName(InputMemberToDelete->GetMemberName()));

		// This calls OnNodeChangedDelegate to update the graph
		InputNode->UpdatePins();

		return true;
	}

	return false;
}

bool UMovieGraphConfig::DeleteOutputMember(UMovieGraphOutput* OutputMemberToDelete)
{
	if (OutputMemberToDelete)
	{
		Modify();
		
		Outputs.RemoveSingle(OutputMemberToDelete);
		RemoveInboundEdges(OutputNode, FName(OutputMemberToDelete->GetMemberName()));

		// This calls OnNodeChangedDelegate to update the graph
		OutputNode->UpdatePins();

		return true;
	}

	return false;
}

FBoolProperty* UMovieGraphConfig::FindOverridePropertyForRealProperty(UClass* InClass, const FProperty* InRealProperty)
{
	if (!ensure(InClass && InRealProperty))
	{
		return nullptr;
	}

	// We can't get access to metadata in shipping builds, so we need to just rely on a naming pattern of bOverride_<PropertyName>
	const FString DesiredPropertyName = FString::Printf(TEXT("bOverride_%s"), *InRealProperty->GetName());

	for (TFieldIterator<FProperty> PropertyIterator(InClass); PropertyIterator; ++PropertyIterator)
	{
		FProperty* CheckProperty = *PropertyIterator;
		if (CheckProperty && CheckProperty->IsA<FBoolProperty>())
		{
			FBoolProperty* PropertyAsBool = CastFieldChecked<FBoolProperty>(CheckProperty);
			if (PropertyAsBool->GetName() == DesiredPropertyName)
			{
				return PropertyAsBool;
			}
		}
	}

	return nullptr;
}

void UMovieGraphConfig::VisitUpstreamNodes(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback) const
{
	TSet<UMovieGraphNode*> VisitedNodes;
	VisitUpstreamNodes_Recursive(FromNode, VisitCallback, VisitedNodes);
}

void UMovieGraphConfig::VisitDownstreamNodes(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback) const
{
	TSet<UMovieGraphNode*> VisitedNodes;
	VisitDownstreamNodes_Recursive(FromNode, VisitCallback, VisitedNodes);
}

TArray<FString> UMovieGraphConfig::GetDownstreamBranchNames(UMovieGraphNode* FromNode, const UMovieGraphPin* FromPin, const bool bStopAtSubgraph) const
{
	TArray<FString> BranchNames;

	// FromNode itself might be the Outputs node, so check before visiting the downstream nodes
	if (FromNode->IsA<UMovieGraphOutputNode>() && FromPin)
	{
		BranchNames.AddUnique(FromPin->Properties.Label.ToString());
	}

	VisitDownstreamNodes(FromNode, FVisitNodesCallback::CreateLambda(
		[&BranchNames, bStopAtSubgraph](const UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
		{
			if (VisitedNode->IsA<UMovieGraphSubgraphNode>() && VisitedPin)
			{
				BranchNames.AddUnique(VisitedPin->Properties.Label.ToString());

				if (bStopAtSubgraph)
				{
					return false;	// Stop traversing nodes
				}
			}
			
			if (VisitedNode->IsA<UMovieGraphOutputNode>() && VisitedPin)
			{
				BranchNames.AddUnique(VisitedPin->Properties.Label.ToString());
			}

			return true;
		}));

	return BranchNames;
}

TArray<FString> UMovieGraphConfig::GetUpstreamBranchNames(UMovieGraphNode* FromNode, const UMovieGraphPin* FromPin, const bool bStopAtSubgraph) const
{
	TArray<FString> BranchNames;

	// FromNode itself might be the Inputs node, so check before visiting the upstream nodes
	if (FromNode->IsA<UMovieGraphInputNode>() && FromPin)
	{
		BranchNames.AddUnique(FromPin->Properties.Label.ToString());
	}

	VisitUpstreamNodes(FromNode, FVisitNodesCallback::CreateLambda(
		[&BranchNames, bStopAtSubgraph](const UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
		{
			if (VisitedNode->IsA<UMovieGraphSubgraphNode>() && VisitedPin)
			{
				BranchNames.AddUnique(VisitedPin->Properties.Label.ToString());

				if (bStopAtSubgraph)
				{
					return false;	// Stop traversing nodes
				}
			}
			
			if (VisitedNode->IsA<UMovieGraphInputNode>() && VisitedPin)
			{
				BranchNames.AddUnique(VisitedPin->Properties.Label.ToString());
			}

			return true;
		}));

	return BranchNames;
}

void UMovieGraphConfig::GetAllContainedSubgraphs(TSet<UMovieGraphConfig*>& OutSubgraphs) const
{
	for (const TObjectPtr<UMovieGraphNode>& Node : GetNodes())
	{
		if (!Node)
		{
			continue;
		}
		
		if (const UMovieGraphSubgraphNode* SubgraphNode = Cast<UMovieGraphSubgraphNode>(Node))
		{
			UMovieGraphConfig* SubgraphConfig = SubgraphNode->GetSubgraphAsset();

			if (!SubgraphConfig) // A subgraph may not have been assigned yet
			{
				continue;
			}
			
			// Don't recurse into this graph if it was already added (to prevent infinite recursion)
			if (!OutSubgraphs.Contains(SubgraphConfig))
			{
				OutSubgraphs.Add(SubgraphConfig);
				SubgraphConfig->GetAllContainedSubgraphs(OutSubgraphs);
			}
		}
	}
}

void UMovieGraphConfig::RecurseUpGlobalsBranchToFindOutputDirectory(const UMovieGraphNode* InNode, FString& OutOutputDirectory, TArray<const UMovieGraphConfig*>& VisitedGraphStack) const
{
	// If there's no Node, no upstream pin or no downstream pin for whatever reason,
	// there is no way to continue so we early out
	if (!InNode) { return; }

	// Only globals can connect to globals linearly, so we only need to look at the first connected input
	// The only exception being subgraph nodes which will need to be separately evaluated
	const UMovieGraphPin* DownstreamGlobalsPin = InNode->GetFirstConnectedInputPin();
	if (!DownstreamGlobalsPin) { return; }

	const UMovieGraphPin* UpstreamGlobalsPin = DownstreamGlobalsPin->GetFirstConnectedPin();
	if (!UpstreamGlobalsPin) { return; }

	UMovieGraphNode* ConnectedNode = UpstreamGlobalsPin->Node;

	VisitedGraphStack.Push(this);

	// Overrides can be set within Subgraphs
	if (const UMovieGraphSubgraphNode* SubgraphNode = Cast<UMovieGraphSubgraphNode>(ConnectedNode))
	{
		const UMovieGraphConfig* SubgraphConfig = SubgraphNode->GetSubgraphAsset();

		// Stop recursing if circular references are found
		if (VisitedGraphStack.Contains(SubgraphConfig))
		{
			return;
		}
		
		if (SubgraphConfig && OutOutputDirectory.IsEmpty())
		{
			const UMovieGraphPin* SubgraphGlobalsPin =
				SubgraphConfig->GetOutputNode()->GetInputPin(UMovieGraphNode::GlobalsPinName);

			if (SubgraphGlobalsPin && SubgraphGlobalsPin->IsConnected())
			{
				SubgraphConfig->RecurseUpGlobalsBranchToFindOutputDirectory(SubgraphGlobalsPin->Node, OutOutputDirectory, VisitedGraphStack);
			}
		}

		VisitedGraphStack.Pop();
	}
	else if (const UMovieGraphGlobalOutputSettingNode* SettingsNode = Cast<UMovieGraphGlobalOutputSettingNode>(InNode))
	{
		if (OutOutputDirectory.IsEmpty() && SettingsNode->bOverride_OutputDirectory)
		{
			OutOutputDirectory = SettingsNode->OutputDirectory.Path;
		}
	}

	// Keep looking upstream if we haven't found any overrides
	if (OutOutputDirectory.IsEmpty())
	{
		RecurseUpGlobalsBranchToFindOutputDirectory(UpstreamGlobalsPin->Node, OutOutputDirectory, VisitedGraphStack);
	}
};

void UMovieGraphConfig::GetOutputDirectory(FString& OutOutputDirectory) const
{
	check (OutputNode);

	// Clear out input strings
	OutOutputDirectory = FString();

	// We only traverse up the globals branch in order to find the output directory and file name format
	const UMovieGraphPin* GlobalsPin = OutputNode->GetInputPin(UMovieGraphNode::GlobalsPinName);

	if (GlobalsPin && GlobalsPin->IsConnected())
	{
		TArray<const UMovieGraphConfig*> VisitedGraphStack;
		RecurseUpGlobalsBranchToFindOutputDirectory(OutputNode, OutOutputDirectory, VisitedGraphStack);

		if (OutOutputDirectory.IsEmpty())
		{
			// If we didn't find any overrides, use the CDO values
			UMovieGraphGlobalOutputSettingNode* CDO = Cast<UMovieGraphGlobalOutputSettingNode>(UMovieGraphGlobalOutputSettingNode::StaticClass()->ClassDefaultObject);
			check(CDO);
			
			OutOutputDirectory = CDO->OutputDirectory.Path;
		}
	}
}

void UMovieGraphConfig::InitializeFlattenedNode(UMovieGraphNode* InNode)
{
	// We go through each of the bOverride_ properties on this new instance and set
	// them all as "not overridden", so that as we traverse the graph, we can only
	// update the node the first time we hit another property that is overridden.
	// If they never override the values anywhere in the chain then it's okay, because
	// it will use the values from the CDO.
	for (TFieldIterator<FProperty> PropertyIterator(InNode->GetClass()); PropertyIterator; ++PropertyIterator)
	{
		FProperty* CheckProperty = *PropertyIterator;
		FBoolProperty* EditConditionProperty = FindOverridePropertyForRealProperty(InNode->GetClass(), CheckProperty);
		if (EditConditionProperty)
		{
			EditConditionProperty->SetPropertyValue_InContainer(InNode, false);
		}
	}

	// Initialize the dynamic properties so they can be updated during traversal like UPROPERTY properties
	InNode->UpdateDynamicProperties();
}

void UMovieGraphConfig::CopyOverriddenProperties(UMovieGraphNode* FromNode, UMovieGraphNode* ToNode, const FMovieGraphEvaluationContext& InEvaluationContext)
{
	if (!ensure(FromNode && ToNode))
	{
		return;
	}

	if (!ensureMsgf(FromNode->GetClass() == ToNode->GetClass(), TEXT("Cross-Class Property copying is not supported at this time.")))
	{
		return;
	}

	for (const FProperty* DestNodeProperty : ToNode->GetAllOverrideableProperties())
	{
		if (!DestNodeProperty)
		{
			continue;
		}

		const FName PropertyName = DestNodeProperty->GetFName();
		
		// For each property on the destination node, decide if we need to try to update it (we don't update bOverride_ properties)
		//
		// We use the existence of a matching edit condition node to signal that this property is overrideable, ie:
		// float MyFoo would have a bOverride_MyFoo, so FindOverridePropertyForRealProperty would match it. However when
		// looking at the bOverride_MyFoo property it would look for bOverride_bOverride_MyFoo, which wouldn't exist, successfully
		// filtering us to only the "real" overrideable properties.

		const bool bIsDynamic = ToNode->GetDynamicPropertyDescriptions().ContainsByPredicate([&PropertyName](const FPropertyBagPropertyDesc& PropDesc)
		{
			return PropDesc.Name == PropertyName;
		});
		
		const bool bIsExposed = FromNode->GetExposedProperties().ContainsByPredicate([&PropertyName](const FMovieGraphPropertyInfo& ExposedPropertyInfo)
		{
			return ExposedPropertyInfo.Name == PropertyName;
		});

		// Ensure there's a property (the "bOverride_*" property) that tracks whether or not this property has already
		// been set/overridden
		const FBoolProperty* EditConditionProperty = bIsDynamic
			? ToNode->FindOverridePropertyForDynamicProperty(PropertyName)
			: FindOverridePropertyForRealProperty(ToNode->GetClass(), DestNodeProperty);
		if (!EditConditionProperty)
		{
			continue;
		}

		// If our destination node already has this property marked as overridden, then some other node in the graph has
		// taken priority and set the value to something, so we don't want to override it. The exception to this is
		// an object implementing IMovieGraphTraversableObject -- they determine when/how property values are updated.
		const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(DestNodeProperty);
		const bool bIsMergeableObject = ObjectProperty && ObjectProperty->PropertyClass->ImplementsInterface(UMovieGraphTraversableObject::StaticClass());
		const bool bAlreadyOverriddenOnDestNode = bIsDynamic
			? ToNode->IsDynamicPropertyOverridden(PropertyName)
			: EditConditionProperty->GetPropertyValue_InContainer(ToNode) && !bIsMergeableObject;
		if (bAlreadyOverriddenOnDestNode)
		{
			continue;
		}

		// If this property (dynamic or not) has been exposed, attempt to get its value via the connection to it (if any)
		if (bIsExposed)
		{
			if (UMovieGraphPin* InputPin = FromNode->GetInputPin(PropertyName))
			{
				TArray<UMovieGraphPin*> ConnectionPath;
				
				// Iterate up the connection chain and find all pins which might have a value that can be resolved.
				FMovieGraphEvaluationContext ValueConnectionContext;
				ValueConnectionContext.PinBeingFollowed = InputPin;
				ValueConnectionContext.SubgraphStack = InEvaluationContext.SubgraphStack;
				TArray<UMovieGraphPin*> ConnectedValuePins = InputPin->Node->EvaluatePinsToFollow(ValueConnectionContext);
				while (!ConnectedValuePins.IsEmpty())
				{
					UMovieGraphPin* ConnectedValuePin = ConnectedValuePins[0];
					if (!ensureMsgf(ConnectedValuePin, TEXT("Found an invalid pin on node '%s'."), *InputPin->Node->GetName()))
					{
						// Can't continue following the connection chain if an invalid pin was found
						break;
					}
					
					if (ConnectionPath.Contains(ConnectedValuePin))
					{
						// Recursive connection found
						UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Found a cycle when following the data connection on pin '%s' for node '%s'. Value will not be resolved."),
							*ConnectedValuePin->Properties.Label.ToString(), *FromNode->GetName());
						break;
					}

					// For the connected value to be used, the type must match and the node the value is originating from must be enabled
					if ((ConnectedValuePin->Properties.Type == InputPin->Properties.Type) && ConnectedValuePin->Node && !ConnectedValuePin->Node->IsDisabled())
					{
						ConnectionPath.Add(ConnectedValuePin);
					}

					ValueConnectionContext.PinBeingFollowed = ConnectedValuePin;
					ConnectedValuePins = ConnectedValuePin->Node->EvaluatePinsToFollow(ValueConnectionContext);
				}

				// Work backwards and use the most upstream value that can be resolved. The most upstream values wins. For example, if a node has an
				// exposed pin, that pin is connected to a subgraph's input, and that input is then connected to a variable node in the parent graph.
				// The variable node's value should be used if it can be resolved, not the subgraph's input value.
				bool bFoundResolvedValue = false;
				for (int32 Index = ConnectionPath.Num() - 1; Index >= 0; --Index)
				{
					const UMovieGraphPin* ConnectedPin = ConnectionPath[Index];
					
					const FString ResolvedValue = ConnectedPin->Node->GetResolvedValueForOutputPin(ConnectedPin->Properties.Label, &InEvaluationContext.UserContext);
					if (ResolvedValue.IsEmpty())
					{
						continue;
					}

					bFoundResolvedValue = true;
					
					if (bIsDynamic)
					{
						ToNode->SetDynamicPropertyValue(PropertyName, ResolvedValue);
						ToNode->SetDynamicPropertyOverridden(PropertyName, true);
					}
					else
					{
						DestNodeProperty->ImportText_Direct(*ResolvedValue, DestNodeProperty->ContainerPtrToValuePtr<uint8>(ToNode), nullptr, PPF_None);
						EditConditionProperty->SetPropertyValue_InContainer(ToNode, true);
					}

					// Resolved a value for this pin; stop iterating over the connection chain
					break;
				}
				
				// The property value was set via a connected pin; move on to the next property
				if (bFoundResolvedValue)
				{
					continue;
				}
			}
		}

		// We know it's not already overridden, so now we should check to see if the incoming node wants to override it.
		const bool bSourceNodeOverwrites = bIsDynamic
			? FromNode->IsDynamicPropertyOverridden(PropertyName)
			: EditConditionProperty->GetPropertyValue_InContainer(FromNode);
		if (!bSourceNodeOverwrites)
		{
			// The source node didn't have the override flag checked, so we don't copy the value from it.
			continue;
		}

		// Okay at this point we know that on our target node, no one has overridden it yet, and our source node wants to override this property.
		// First, we update the booleans to say that yes, this property has been overridden on the target node.
		if (bIsDynamic)
		{
			ToNode->SetDynamicPropertyOverridden(PropertyName, true);
		}
		else
		{
			EditConditionProperty->SetPropertyValue_InContainer(ToNode, true);
		}

		// Before using the normal property copying procedure, check to see if this property is an IMovieGraphTraversableObject.
		// These objects define a particular way they should have their properties merged.
		if (bIsMergeableObject)
		{
			const IMovieGraphTraversableObject* SourceTraversableObject = Cast<IMovieGraphTraversableObject>(
				ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<void>(FromNode)));
			IMovieGraphTraversableObject* DestTraversableObject = Cast<IMovieGraphTraversableObject>(
				ObjectProperty->GetObjectPropertyValue(ObjectProperty->ContainerPtrToValuePtr<void>(ToNode)));
			
			if (DestTraversableObject && SourceTraversableObject)
			{
				DestTraversableObject->Merge(SourceTraversableObject);

				// Property has been copied via Merge(), don't run the normal copy procedure
				continue;
			}
		}

		// Now we need to copy the value from the source to the destination
		if (bIsDynamic)
		{
			FString SourceValue;
			if (FromNode->GetDynamicPropertyValue(PropertyName, SourceValue))
			{
				ToNode->SetDynamicPropertyValue(PropertyName, SourceValue);
			}
		}
		else
		{
			DestNodeProperty->CopyCompleteValue_InContainer(ToNode, FromNode);
		}
	}
}

bool UMovieGraphConfig::CreateFlattenedGraph_Recursive(UMovieGraphEvaluatedConfig* InOwningConfig, FMovieGraphEvaluatedBranchConfig& OutBranchConfig,
	FMovieGraphEvaluationContext& InEvaluationContext, UMovieGraphPin* InPinToFollow)
{
	if (!InPinToFollow)
	{
		InEvaluationContext.TraversalError = LOCTEXT("TraversalError_InvalidPin", "Found an invalid pin during graph traversal.");
		return false;
	}

	UMovieGraphNode* Node = InPinToFollow->Node;
	if (!Node)
	{
		InEvaluationContext.TraversalError = LOCTEXT("TraversalError_InvalidNode", "Found an invalid node during graph traversal.");
		return false;
	}

	// We only follow execution pins during traversal.
	if (!ensureMsgf(InPinToFollow->Properties.bIsBranch, TEXT("Only Branch pins should be contained by InPinToFollow!")))
	{
		InEvaluationContext.TraversalError = LOCTEXT("TraversalError_NonBranchPin", "Attempting to follow a non-branch pin during graph traversal.");
		return false;
	}

	InEvaluationContext.PinBeingFollowed = InPinToFollow;

	// Add this node to the set of visited nodes so it can be checked for cycles later. Get the graph from GetTypedOuter() rather than "this" because
	// this method will be called recursively, potentially on pins within subgraphs.
	const UMovieGraphConfig* OwningGraph = InPinToFollow->GetTypedOuter<UMovieGraphConfig>();
	ensure(OwningGraph);
	TSet<TObjectPtr<UMovieGraphNode>>& VisitedNodeSet = InEvaluationContext.VisitedNodesByOwningGraph.FindOrAdd(OwningGraph).VisitedNodes;
	VisitedNodeSet.Add(Node);
	
	const bool bShouldIncludeNode =
		Node->IsA<UMovieGraphSettingNode>() &&
		!Node->IsDisabled() &&
		!InEvaluationContext.NodeTypesToRemoveStack.Contains(Node->GetClass());

	if (bShouldIncludeNode)
	{
#if WITH_EDITOR
		// Normally we copy properties if we find a matching bOverride_ property. Unfortunately this creates a somewhat common
		// scenario where you've created a bOverride_ property but typo'd the real property name, so the real property doesn't
		// actually get updated, but we don't produce a warning (as it's valid to have properties with no matching bOverride_).
		// So to avoid this we have this editor ensure to prompt you when we find a bOverride_ property with no matching "real" property.
		for (TFieldIterator<FProperty> PropertyIterator(Node->GetClass()); PropertyIterator; ++PropertyIterator)
		{
			// If we're looking at an override property... 
			if (PropertyIterator->GetName().StartsWith(TEXT("bOverride_")))
			{
				const FString RealPropertyName = PropertyIterator->GetName().RightChop(10); // Chop off bOverride_ to get the name of the property we're searching for.
				bool bFoundProperty = false;
				for (TFieldIterator<FProperty> InnerPropertyIterator(Node->GetClass()); InnerPropertyIterator; ++InnerPropertyIterator)
				{
					if (InnerPropertyIterator->GetName() == RealPropertyName)
					{
						bFoundProperty = true;
						break;
					}
				}

				ensureAlwaysMsgf(bFoundProperty, TEXT("Found override property named %s, but could not find real property named %s"), *PropertyIterator->GetName(), *RealPropertyName);
			}
		}
#endif
		const UMovieGraphSettingNode* NodeAsSetting = CastChecked<UMovieGraphSettingNode>(Node);
		const FString& NodeInstanceName = NodeAsSetting->GetNodeInstanceName();
		
		UMovieGraphNode* ExistingNode = OutBranchConfig.GetNodeByClassExactMatch(Node->GetClass(), NodeInstanceName);
		if (!ExistingNode)
		{
			// Create a new instance of this node inside our flattened eval graph
			ExistingNode = NewObject<UMovieGraphNode>(InOwningConfig, Node->GetClass());
			OutBranchConfig.NamedNodes.FindOrAdd(NodeInstanceName).NodeInstances.Add(ExistingNode);

			// Set all of the boolean edit condition values to false, so we can use "true" to indicate
			// that the value was overridden already during traversal.
			InitializeFlattenedNode(ExistingNode);
		}

		// Now do a property-copy from this node onto our flattened one. We don't use the generic property
		// copy routines in the engine because we have special handling (we want to check if the property
		// is actually marked for override, and also skip if this has already been overridden).
		CopyOverriddenProperties(Node, ExistingNode, InEvaluationContext);
	}

	// If this is a special "removal" node, keep track of the type that should be removed. Since this method is recursive,
	// a stack is used to keep track of the types. The graph is iterated starting from the Outputs node, so all matching
	// nodes that are *upstream* of the removal node will be removed.
	const UMovieGraphRemoveRenderSettingNode* RemovalNode = Cast<UMovieGraphRemoveRenderSettingNode>(Node);
	const bool bIsARemovalNode = RemovalNode && !Node->IsDisabled() && (RemovalNode->NodeType.Get() != nullptr);
	if (bIsARemovalNode)
	{
		InEvaluationContext.NodeTypesToRemoveStack.Push(RemovalNode->NodeType);
	}
	
	// Now that we've potentially resolved the values on this node, continue to travel up-stream along any execution pins,
	// potentially following re-route nodes, sub-graph nodes, through branches, etc.
	TArray<UMovieGraphPin*> NewPinsToFollow = Node->EvaluatePinsToFollow(InEvaluationContext);

	// Immediately stop traversal if a circular subgraph reference was found. This is done after EvaluatePinsToFollow() because
	// subgraph nodes will set bCircularGraphReferenceFound in EvaluatePinsToFollow().
	if (InEvaluationContext.bCircularGraphReferenceFound)
	{
		// Generate a string illustrating the problematic subgraph stack
		FString GraphCycleTraversalPath;
		for (const TObjectPtr<const UMovieGraphSubgraphNode>& SubgraphNode : InEvaluationContext.SubgraphStack)
		{
			if (const UMovieGraphConfig* SubgraphAsset = SubgraphNode->GetSubgraphAsset())
			{
				GraphCycleTraversalPath += FString::Printf(TEXT("\n%s -> "), *SubgraphAsset->GetName());
			}
		}

		InEvaluationContext.TraversalError = FText::Format(
			LOCTEXT("TraversalError_CircularGraphReference", "Circular subgraph reference found during traversal.{0}"), FText::FromString(GraphCycleTraversalPath));
		
		return false;
	}
	
	for (UMovieGraphPin* Pin : NewPinsToFollow)
	{
		for (UMovieGraphEdge* Edge : Pin->Edges)
		{
			UMovieGraphPin* OtherPin = Edge->GetOtherPin(Pin);
			if (!OtherPin)
			{
				continue;
			}
			
			UMovieGraphNode* OtherNode = OtherPin->Node;

			// Detect cycles within node connections
			if (VisitedNodeSet.Contains(OtherNode))
			{
				// Generate a string illustrating the problematic node connections
				FString NodeCycleTraversalPath;
				for (const TObjectPtr<UMovieGraphNode>& VisitedNode : VisitedNodeSet)
				{
					NodeCycleTraversalPath += FString::Printf(TEXT("\n%s -> "), *VisitedNode->GetName());
				}
				
				InEvaluationContext.TraversalError = FText::Format(
					LOCTEXT("TraversalError_CircularNodeReference", "Node connection cycle found during traversal.{0}"), FText::FromString(NodeCycleTraversalPath));
				
				return false;
			}

			// If no cycle detected, continue following the pin
			const bool bSuccess = CreateFlattenedGraph_Recursive(InOwningConfig, OutBranchConfig, InEvaluationContext, OtherPin);
			if (!bSuccess)
			{
				return false;
			}
		}
	}

	// Done with this removal node now; pop it off the stack so it doesn't affect other branches
	if (bIsARemovalNode)
	{
		InEvaluationContext.NodeTypesToRemoveStack.Pop();
	}

	return true;
}

void UMovieGraphConfig::VisitUpstreamNodes_Recursive(UMovieGraphNode* FromNode,	const FVisitNodesCallback& VisitCallback, TSet<UMovieGraphNode*>& VisitedNodes) const
{
	if (VisitedNodes.Contains(FromNode))
	{
		// Cycle detected, stop recursing down this path. This is not necessarily an error, so don't log. For example:
		//  |----| ---> N2 
		//  | N1 | ---> N3 
		//  |----| ---> N4
		// Where nodes N2, N3, and N4 are nodes feeding out of node N1. N2 may have visited N1 already, so when N3 visits
		// N1, just stop.
		return;
	}
	
	VisitedNodes.Add(FromNode);
	
	for (const UMovieGraphPin* Pin : FromNode->GetInputPins())
	{
		for (const UMovieGraphPin* ConnectedPin : Pin->GetAllConnectedPins())
		{
			if (ConnectedPin->Properties.bIsBranch)
			{
				bool bContinueVisiting = true;
				if (VisitCallback.IsBound())
				{
					bContinueVisiting = VisitCallback.Execute(ConnectedPin->Node, ConnectedPin);
				}

				if (bContinueVisiting)
				{
					VisitUpstreamNodes_Recursive(ConnectedPin->Node, VisitCallback, VisitedNodes);
				}
			}
		}
	}
}

void UMovieGraphConfig::VisitDownstreamNodes_Recursive(UMovieGraphNode* FromNode, const FVisitNodesCallback& VisitCallback, TSet<UMovieGraphNode*>& VisitedNodes) const
{
	if (VisitedNodes.Contains(FromNode))
	{
		// Cycle detected, stop recursing down this path. This is not necessarily an error, so don't log. For example:
		// N1 ---> |----|
		// N2 ---> | N4 |
		// N3 ---> |----|
		// Where nodes N1, N2, and N3 are nodes feeding into node N4. N1 may have visited N4 already, so when N2 visits
		// N4, just stop.
		return;
	}

	VisitedNodes.Add(FromNode);

	for (const UMovieGraphPin* Pin : FromNode->GetOutputPins())
	{
		for (const UMovieGraphPin* ConnectedPin : Pin->GetAllConnectedPins())
		{
			if (ConnectedPin->Properties.bIsBranch)
			{
				bool bContinueVisiting = true;
				if (VisitCallback.IsBound())
				{
					bContinueVisiting = VisitCallback.Execute(ConnectedPin->Node, ConnectedPin);
				}

				if (bContinueVisiting)
				{
					VisitDownstreamNodes_Recursive(ConnectedPin->Node, VisitCallback, VisitedNodes);
				}
			}
		}
	}
}

UMovieGraphEvaluatedConfig* UMovieGraphConfig::CreateFlattenedGraph(const FMovieGraphTraversalContext& InContext, FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_CreateFlattenedGraph);
	LLM_SCOPE_BYNAME(TEXT("MovieGraph/CreateFlattenedGraph"));

	OutError.Empty();

	UMovieGraphEvaluatedConfig* NewContext = NewObject<UMovieGraphEvaluatedConfig>(this);

	if (OutputNode)
	{
		TArray<UMovieGraphPin*> InputPinsToFollow = OutputNode->GetInputPins();
		UMovieGraphPin* GlobalsPin = OutputNode->GetInputPin(UMovieGraphNode::GlobalsPinName);

		// For each input pin, we create an instance of the config (including Globals, since some queries are made with no context)
		// but we also want each named branch to have a complete, resolved set of configs.
		for (UMovieGraphPin* InputPin : OutputNode->GetInputPins())
		{
			if (!InputPin->Properties.bIsBranch)
			{
				// TODO: For now, only branches can be evaluated
				continue;
			}
			
			const FName BranchName = InputPin->Properties.Label;
			FMovieGraphEvaluatedBranchConfig& BranchConfig = NewContext->BranchConfigMapping.Add(BranchName);
			
			// The stack evaluation context is per-branch
			FMovieGraphEvaluationContext StackContext;
			StackContext.UserContext = InContext;

			// Follow the branch connected to this pin
			for (UMovieGraphEdge* Edge : InputPin->Edges)
			{
				UMovieGraphPin* OtherPin = Edge->GetOtherPin(InputPin);
				if (OtherPin)
				{
					const bool bTraversalSuccessful = CreateFlattenedGraph_Recursive(NewContext, /*InOut*/ BranchConfig, StackContext, OtherPin);
					if (!bTraversalSuccessful)
					{
						UE_LOG(LogMovieRenderPipeline, Error, TEXT("%s"), *StackContext.TraversalError.ToString());
						OutError = StackContext.TraversalError.ToString();
						return nullptr;
					}
				}
			}

			// Now, if this isn't the Globals branch, we apply the Globals branch after the branch settings.
			// This allows users to override things on a per-branch basis, and if they don't set it, then the Globals
			// branch has a chance to set "defaults" (which then fall back to CDO values if the Globals branch doesn't 
			// set it). We skip doing this for the Globals branch because the above loop just did that.
			if (BranchName != UMovieGraphNode::GlobalsPinName && GlobalsPin)
			{
				// We use a new context here as we consider every branch independent.
				FMovieGraphEvaluationContext GlobalStackContext;
				GlobalStackContext.UserContext = InContext;
				for (UMovieGraphEdge* Edge : GlobalsPin->Edges)
				{
					UMovieGraphPin* OtherPin = Edge->GetOtherPin(GlobalsPin);
					if (OtherPin)
					{
						const bool bTraversalSuccessful = CreateFlattenedGraph_Recursive(NewContext, /*InOut*/ BranchConfig, GlobalStackContext, OtherPin);
						if (!bTraversalSuccessful)
						{
							UE_LOG(LogMovieRenderPipeline, Error, TEXT("%s"), *StackContext.TraversalError.ToString());
							OutError = StackContext.TraversalError.ToString();
                            return nullptr;
						}
					}
				}
			}
		}		
	}

	bool bHasRenderLayerNode = false;

	for (const TPair<FName, FMovieGraphEvaluatedBranchConfig>& Pair : NewContext->BranchConfigMapping)
	{
		for (UMovieGraphNode* Node : Pair.Value.GetNodes())
		{
			for (TFieldIterator<FProperty> PropertyIterator(Node->GetClass()); PropertyIterator; ++PropertyIterator)
			{
				FProperty* CheckProperty = *PropertyIterator;
				FBoolProperty* EditConditionProperty = FindOverridePropertyForRealProperty(Node->GetClass(), CheckProperty);
				if (EditConditionProperty)
				{
					FString ExportText;
					CheckProperty->ExportText_InContainer(0, ExportText, Node, Node, Node, 0);
				}
			}
			
			bHasRenderLayerNode |= Node->IsA<UMovieGraphRenderLayerNode>();
		}
	}

	if (!bHasRenderLayerNode)
	{
		// NOTE: While this doesn't cover all cases, we ensure the presence of at least one render layer node.
		UE_CALL_ONCE([] { UE_LOG(LogMovieRenderPipeline, Error, TEXT("For render jobs to succeed, one or more render layer node(s) must be present.")); });
	}

	return NewContext;
}
#undef LOCTEXT_NAMESPACE
