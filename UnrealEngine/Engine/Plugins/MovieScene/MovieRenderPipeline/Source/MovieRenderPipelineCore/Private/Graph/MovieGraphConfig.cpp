// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/MovieGraphConfig.h"

#include "Algo/Transform.h"
#include "Graph/MovieGraphEdge.h"
#include "Graph/Nodes/MovieGraphInputNode.h"
#include "Graph/Nodes/MovieGraphOutputNode.h"
#include "Graph/Nodes/MovieGraphVariableNode.h"
#include "MovieGraphUtils.h"
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
#if WITH_EDITOR
	OnMovieGraphVariableChangedDelegate.Broadcast(this);
#endif
	
	return Super::SetMemberName(InNewName);
}

#if WITH_EDITOR
void UMovieGraphVariable::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnMovieGraphVariableChangedDelegate.Broadcast(this);
}
#endif // WITH_EDITOR

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
#if WITH_EDITOR
	OnMovieGraphInputChangedDelegate.Broadcast(this);
#endif
	
	return Super::SetMemberName(InNewName);
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
#if WITH_EDITOR
	OnMovieGraphOutputChangedDelegate.Broadcast(this);
#endif
	
	return Super::SetMemberName(InNewName);
}

#if WITH_EDITOR
void UMovieGraphOutput::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	OnMovieGraphOutputChangedDelegate.Broadcast(this);
}
#endif // WITH_EDITOR

FName UMovieGraphConfig::GlobalVariable_ShotName = "shot_name";
FName UMovieGraphConfig::GlobalVariable_SequenceName = "seq_name";
FName UMovieGraphConfig::GlobalVariable_FrameNumber = "frame_num";
FName UMovieGraphConfig::GlobalVariable_CameraName = "camera_name";
FName UMovieGraphConfig::GlobalVariable_RenderLayerName = "render_layer_name";

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
		constexpr int32 OutputNodeOffset = 300;
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
#endif
	}
}

UMovieGraphVariable* UMovieGraphConfig::AddGlobalVariable(const FName& InName, EMovieGraphValueType ValueType)
{
	// Don't add duplicate global variables
	const bool VariableExists = Variables.ContainsByPredicate([&InName](const TObjectPtr<UMovieGraphVariable>& Variable)
	{
		return Variable && (Variable->GetMemberName() == InName);
	});

	if (VariableExists)
	{
		return nullptr;
	}
	
	if (UMovieGraphVariable* NewVariable = AddVariable(InName))
	{
		NewVariable->bIsGlobal = true;
		NewVariable->bIsEditable = false;
		NewVariable->SetValueType(ValueType);
		return NewVariable;
	}

	return nullptr;
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

	static const TMap<FName, EMovieGraphValueType> GlobalVariableNamesAndTypes =
	{
		{GlobalVariable_ShotName, EMovieGraphValueType::String},
		{GlobalVariable_SequenceName, EMovieGraphValueType::String},
		{GlobalVariable_FrameNumber, EMovieGraphValueType::Int32},
		{GlobalVariable_CameraName, EMovieGraphValueType::String},
		{GlobalVariable_RenderLayerName, EMovieGraphValueType::String}
	};

	// Add all of the global variables that should be available in the graph
	for (const TTuple<FName, EMovieGraphValueType>& GlobalVariableInfo : GlobalVariableNamesAndTypes)
	{
		AddGlobalVariable(GlobalVariableInfo.Key, GlobalVariableInfo.Value);
	}
}

bool UMovieGraphConfig::AddLabeledEdge(UMovieGraphNode* FromNode, const FName& FromPinLabel, UMovieGraphNode* ToNode, const FName& ToPinLabel)
{
	if (!FromNode || !ToNode)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("AddLabeledEdge: Invalid Edge Nodes"));
		return false;
	}

	UMovieGraphPin* FromPin = FromNode->GetOutputPin(FromPinLabel);
	if (!FromPin)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("AddLabeledEdge: FromNode: %s does not have a pin with the label: %s"), *FromNode->GetName(), *FromPinLabel.ToString());
		return false;
	}

	UMovieGraphPin* ToPin = ToNode->GetInputPin(ToPinLabel);
	if (!ToPin)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("AddLabeledEdge: ToNode: %s does not have a pin with the label: %s"), *ToNode->GetName(), *ToPinLabel.ToString());
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

bool UMovieGraphConfig::RemoveEdge(UMovieGraphNode* FromNode, const FName& FromPinLabel, UMovieGraphNode* ToNode, const FName& ToPinLabel)
{
	if (!FromNode || !ToNode)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveEdge: Invalid Edge Nodes"));
		return false;
	}

	UMovieGraphPin* FromPin = FromNode->GetOutputPin(FromPinLabel);
	if (!FromPin)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveEdge: FromNode: %s does not have a pin with the label: %s"), *FromNode->GetName(), *FromPinLabel.ToString());
		return false;
	}

	UMovieGraphPin* ToPin = ToNode->GetInputPin(ToPinLabel);
	if (!ToPin)
	{
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveEdge: ToNode: %s does not have a pin with the label: %s"), *ToNode->GetName(), *ToPinLabel.ToString());
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
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveAllInboundEdges: Invalid Edge Nodes"));
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
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveAllOutboundEdges: Invalid Edge Nodes"));
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
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveInboundEdges: Invalid Edge Nodes"));
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
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveOutboundEdges: Invalid Edge Nodes"));
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
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("RemoveNode: Invalid Node"));
		return false;
	}

	RemoveAllInboundEdges(InNode);
	RemoveAllOutboundEdges(InNode);

#if WITH_EDITOR
	TArray<UMovieGraphNode*> RemovedNodes;
	RemovedNodes.Add(InNode);
	OnGraphNodesDeletedDelegate.Broadcast(RemovedNodes);
#endif

	return AllNodes.RemoveSingle(InNode) == 1;
}

template<typename T>
T* UMovieGraphConfig::AddMember(TArray<TObjectPtr<T>>& InMemberArray, const FName& InBaseName)
{
	static_assert(std::is_base_of_v<UMovieGraphMember, T>, "T is not derived from UMovieGraphMember");
	
	using namespace UE::MoviePipeline::RenderGraph;

	// TODO: This can be replaced with just CreateDefaultSubobject() when AddDefaultMembers() isn't called from PostLoad()
	//
	// This method will be called in two cases: 1) when default members are being added to a new graph when it is being
	// initially created or loaded via PostLoad(), or 2) a member is being added to the graph by the user. For case 1,
	// when the constructor is running, RF_NeedInitialization will be set. CreateDefaultSubobject() needs to be called
	// in this scenario instead of NewObject().
	const bool bIsNewObject = HasAnyFlags(RF_NeedInitialization);
	T* NewMember = bIsNewObject
		? CreateDefaultSubobject<T>(MakeUniqueObjectName(this, T::StaticClass()))
		: NewObject<T>(this, NAME_None);
	
	if (!NewMember)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Unable to create new member object in the graph."));
		return nullptr;
	}

	InMemberArray.Add(NewMember);
	NewMember->SetFlags(RF_Transactional);
	NewMember->SetGuid(FGuid::NewGuid());

	// Generate and set a unique name
	TArray<FString> ExistingMemberNames;
	Algo::Transform(InMemberArray, ExistingMemberNames, [](const T* Member) { return Member->GetMemberName(); });
	NewMember->SetMemberName(GetUniqueName(ExistingMemberNames, InBaseName.ToString()));

	return NewMember;
}

UMovieGraphVariable* UMovieGraphConfig::AddVariable(const FName InCustomBaseName)
{
	static const FText VariableBaseName = LOCTEXT("VariableBaseName", "Variable");
	
	UMovieGraphVariable* NewVariable = AddMember(
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

	UMovieGraphInput* NewInput = AddMember(Inputs, FName(*InputBaseName.ToString()));
	InputNode->UpdatePins();
	
#if WITH_EDITOR
	OnGraphInputAddedDelegate.Broadcast(NewInput);
#endif

	return NewInput;
}

UMovieGraphOutput* UMovieGraphConfig::AddOutput()
{
	static const FText OutputBaseName = LOCTEXT("OutputBaseName", "Output");
	
	UMovieGraphOutput* NewOutput = AddMember(Outputs, FName(*OutputBaseName.ToString()));
	OutputNode->UpdatePins();

#if WITH_EDITOR
	OnGraphOutputAddedDelegate.Broadcast(NewOutput);
#endif

	return NewOutput;
}

UMovieGraphVariable* UMovieGraphConfig::GetVariableByGuid(const FGuid& InGuid) const
{
	for (const TObjectPtr<UMovieGraphVariable> Variable : Variables)
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
	if (bIncludeGlobal)
	{
		return Variables;
	}

	return Variables.FilterByPredicate([](const UMovieGraphVariable* Var) { return Var && !Var->IsGlobal(); });
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
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("DeleteMember: The member '%s' cannot be deleted because it is flagged as non-deletable."), *MemberToDelete->GetMemberName());
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

TArray<FString> UMovieGraphConfig::GetDownstreamBranchNames(UMovieGraphNode* FromNode, const UMovieGraphPin* FromPin) const
{
	TArray<FString> BranchNames;

	// FromNode itself might be the Outputs node, so check before visiting the downstream nodes
	if (FromNode->IsA<UMovieGraphOutputNode>() && FromPin)
	{
		BranchNames.Add(FromPin->Properties.Label.ToString());
	}

	VisitDownstreamNodes(FromNode, FVisitNodesCallback::CreateLambda(
		[&BranchNames](UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
		{
			if (VisitedNode->IsA<UMovieGraphOutputNode>() && VisitedPin)
			{
				BranchNames.Add(VisitedPin->Properties.Label.ToString());
			}
		}));

	return BranchNames;
}

TArray<FString> UMovieGraphConfig::GetUpstreamBranchNames(UMovieGraphNode* FromNode, const UMovieGraphPin* FromPin) const
{
	TArray<FString> BranchNames;

	// FromNode itself might be the Inputs node, so check before visiting the upstream nodes
	if (FromNode->IsA<UMovieGraphInputNode>() && FromPin)
	{
		BranchNames.Add(FromPin->Properties.Label.ToString());
	}

	VisitUpstreamNodes(FromNode, FVisitNodesCallback::CreateLambda(
		[&BranchNames](UMovieGraphNode* VisitedNode, const UMovieGraphPin* VisitedPin)
		{
			if (VisitedNode->IsA<UMovieGraphInputNode>() && VisitedPin)
			{
				BranchNames.Add(VisitedPin->Properties.Label.ToString());
			}
		}));

	return BranchNames;
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

void UMovieGraphConfig::CopyOverriddenProperties(UMovieGraphNode* FromNode, UMovieGraphNode* ToNode, const FMovieGraphTraversalContext* InContext)
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
		// taken priority and set the value to something, so we don't want to override it.
		const bool bAlreadyOverriddenOnDestNode = bIsDynamic
			? ToNode->IsDynamicPropertyOverridden(PropertyName)
			: EditConditionProperty->GetPropertyValue_InContainer(ToNode);
		if (bAlreadyOverriddenOnDestNode)
		{
			continue;
		}

		// If this property (dynamic or not) has been exposed, attempt to get its value via the connection to it (if any)
		if (bIsExposed)
		{
			if (const UMovieGraphPin* InputPin = FromNode->GetInputPin(PropertyName))
			{
				const UMovieGraphPin* ConnectedPin = InputPin->GetFirstConnectedPin();
				if (ConnectedPin && (ConnectedPin->Properties.Type == InputPin->Properties.Type) && ConnectedPin->Node)
				{
					// There was a valid connection to the input pin; resolve the value from the connected output and set the
					// value on this property
					const FString ResolvedValue = ConnectedPin->Node->GetResolvedValueForOutputPin(ConnectedPin->Properties.Label, InContext);
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

					// The property value was set via a connected pin; move on to the next property
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

void UMovieGraphConfig::CreateFlattenedGraph_Recursive(UMovieGraphEvaluatedConfig* InOwningConfig, FMovieGraphEvaluatedBranchConfig& OutBranchConfig,
	FMovieGraphEvaluationContext& InEvaluationContext, UMovieGraphPin* InPinToFollow)
{
	if (!InPinToFollow)
	{
		return;
	}

	UMovieGraphNode* Node = InPinToFollow->Node;
	if (!Node)
	{
		return;
	}

	// We only follow execution pins during traversal.
	if (!ensureMsgf(InPinToFollow->Properties.bIsBranch, TEXT("Only Branch pins should be contained by InPinToFollow!")))
	{
		return;
	}

	InEvaluationContext.PinBeingFollowed = InPinToFollow;

	// Check to see if our flattened evaluation graph already has a copy of this node.
	InEvaluationContext.VisitedNodes.Add(Node);
	const bool bShouldIncludeNode = Node->IsA<UMovieGraphSettingNode>();

	if(bShouldIncludeNode)
	{
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
		// is actually marked for override, and also skip if this has already been overridden.

		// ToDo: Handle "Disable" ndoes that can disable upstream types of nodes, etc. Push disable types into
		// the currently evaluating context
		CopyOverriddenProperties(Node, ExistingNode, &InEvaluationContext.UserContext);
	}
	
	// Now that we've potentially resolved the values on this node, continue to travel up-stream along any execution pins,
	// potentially following re-route nodes, sub-graph nodes, through branches, etc.
	TArray<UMovieGraphPin*> NewPinsToFollow = Node->EvaluatePinsToFollow(InEvaluationContext);
	
	for(UMovieGraphPin* Pin : NewPinsToFollow)
	{
		for (UMovieGraphEdge* Edge : Pin->Edges)
		{
			if (UMovieGraphPin* OtherPin = Edge->GetOtherPin(Pin))
			{
				UMovieGraphNode* OtherNode = OtherPin->Node;

				if (InEvaluationContext.VisitedNodes.Contains(OtherNode))
				{
					// ToDo: This won't work long term if you have two different branches visiting the same node
					// also we need to reset this every time we go start from the root.
					UE_LOG(LogMovieRenderPipeline, Error, TEXT("Circular graph?"));
					continue;
				}

				CreateFlattenedGraph_Recursive(InOwningConfig, OutBranchConfig, InEvaluationContext, OtherPin);
			}
		}
	}
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
				VisitCallback.ExecuteIfBound(ConnectedPin->Node, ConnectedPin);
				VisitUpstreamNodes_Recursive(ConnectedPin->Node, VisitCallback, VisitedNodes);
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
				VisitCallback.ExecuteIfBound(ConnectedPin->Node, ConnectedPin);
				VisitDownstreamNodes_Recursive(ConnectedPin->Node, VisitCallback, VisitedNodes);
			}
		}
	}
}

UMovieGraphEvaluatedConfig* UMovieGraphConfig::CreateFlattenedGraph(const FMovieGraphTraversalContext& InContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MRQ_CreateFlattenedGraph);
	LLM_SCOPE_BYNAME(TEXT("MovieGraph/CreateFlattenedGraph"));

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
					CreateFlattenedGraph_Recursive(NewContext, /*InOut*/ BranchConfig, StackContext, OtherPin);
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
						CreateFlattenedGraph_Recursive(NewContext, /*InOut*/ BranchConfig, GlobalStackContext, OtherPin);
					}
				}
			}
		}		
	}

	UE_LOG(LogTemp, Warning, TEXT("Traversed Graph:"));
	for (const TPair<FName, FMovieGraphEvaluatedBranchConfig>& Pair : NewContext->BranchConfigMapping)
	{
		UE_LOG(LogTemp, Warning, TEXT("\t Branch: %s"), *Pair.Key.ToString());

		for (UMovieGraphNode* Node : Pair.Value.GetNodes())
		{
			UE_LOG(LogTemp, Warning, TEXT("\t\t %s Class:"), *Node->GetClass()->GetName());
			for (TFieldIterator<FProperty> PropertyIterator(Node->GetClass()); PropertyIterator; ++PropertyIterator)
			{
				FProperty* CheckProperty = *PropertyIterator;
				FBoolProperty* EditConditionProperty = FindOverridePropertyForRealProperty(Node->GetClass(), CheckProperty);
				if (EditConditionProperty)
				{
					FString ExportText;
					CheckProperty->ExportText_InContainer(0, ExportText, Node, Node, Node, 0);
					UE_LOG(LogTemp, Warning, TEXT("\t\t\t %s : %s"), *CheckProperty->GetName(), *ExportText);
				}
			}
		}

	}

	return NewContext;
}


#undef LOCTEXT_NAMESPACE
