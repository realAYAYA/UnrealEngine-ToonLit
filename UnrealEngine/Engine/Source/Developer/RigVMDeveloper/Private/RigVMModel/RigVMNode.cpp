// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMUserWorkflowRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMNode)

const FString URigVMNode::NodeColorName = TEXT("NodeColor");

#if WITH_EDITOR
TArray<int32> URigVMNode::EmptyInstructionArray;
#endif

URigVMNode::URigVMNode()
: UObject()
, Position(FVector2D::ZeroVector)
, Size(FVector2D::ZeroVector)
, NodeColor(FLinearColor::White)
, bHasBreakpoint(false)
, bHaltedAtThisNode(false)
#if WITH_EDITOR
, ProfilingHash(0)
#endif
{

}

URigVMNode::~URigVMNode()
{
}

FString URigVMNode::GetNodePath(bool bRecursive) const
{
	if (bRecursive)
	{
		if(URigVMGraph* Graph = GetGraph())
		{
			const FString ParentNodePath = Graph->GetNodePath();
			if (!ParentNodePath.IsEmpty())
			{
				return JoinNodePath(ParentNodePath, GetName());
			}
		}
	}
	return GetName();
}

bool URigVMNode::SplitNodePathAtStart(const FString& InNodePath, FString& LeftMost, FString& Right)
{
	return InNodePath.Split(TEXT("|"), &LeftMost, &Right, ESearchCase::IgnoreCase, ESearchDir::FromStart);
}

bool URigVMNode::SplitNodePathAtEnd(const FString& InNodePath, FString& Left, FString& RightMost)
{
	return InNodePath.Split(TEXT("|"), &Left, &RightMost, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
}

bool URigVMNode::SplitNodePath(const FString& InNodePath, TArray<FString>& Parts)
{
	int32 OriginalPartsCount = Parts.Num();
	FString NodePathRemaining = InNodePath;
	FString Left, Right;
	Right = NodePathRemaining;

	while (SplitNodePathAtStart(NodePathRemaining, Left, Right))
	{
		Parts.Add(Left);
		Left.Empty();
		NodePathRemaining = Right;
	}

	if (!Right.IsEmpty())
	{
		Parts.Add(Right);
	}

	return Parts.Num() > OriginalPartsCount;
}

FString URigVMNode::JoinNodePath(const FString& Left, const FString& Right)
{
	ensure(!Left.IsEmpty() && !Right.IsEmpty());
	return Left + TEXT("|") + Right;
}

FString URigVMNode::JoinNodePath(const TArray<FString>& InParts)
{
	if (InParts.Num() == 0)
	{
		return FString();
	}

	FString Result = InParts[0];
	for (int32 PartIndex = 1; PartIndex < InParts.Num(); PartIndex++)
	{
		Result += TEXT("|") + InParts[PartIndex];
	}

	return Result;
}

int32 URigVMNode::GetNodeIndex() const
{
	int32 Index = INDEX_NONE;
	URigVMGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		Graph->GetNodes().Find((URigVMNode*)this, Index);
	}
	return Index;
}

const TArray<URigVMPin*>& URigVMNode::GetPins() const
{
	return Pins;
}

TArray<URigVMPin*> URigVMNode::GetAllPinsRecursively() const
{
	struct Local
	{
		static void VisitPinRecursively(URigVMPin* InPin, TArray<URigVMPin*>& OutPins)
		{
			OutPins.Add(InPin);
			for (URigVMPin* SubPin : InPin->GetSubPins())
			{
				VisitPinRecursively(SubPin, OutPins);
			}
		}
	};

	TArray<URigVMPin*> Result;
	for (URigVMPin* Pin : GetPins())
	{
		Local::VisitPinRecursively(Pin, Result);
	}
	return Result;
}

URigVMPin* URigVMNode::FindPin(const FString& InPinPath) const
{
	FString Left, Right;
	if (!URigVMPin::SplitPinPathAtStart(InPinPath, Left, Right))
	{
		Left = InPinPath;
	}

	for (URigVMPin* Pin : GetPins())
	{
		if (Pin->NameEquals(Left, true))
		{
			if (Right.IsEmpty())
			{
				return Pin;
			}
			return Pin->FindSubPin(Right);
		}
	}

	if(Left.StartsWith(URigVMPin::OrphanPinPrefix))
	{
		for (URigVMPin* Pin : OrphanedPins)
		{
			if (Pin->GetName() == Left)
			{
				if (Right.IsEmpty())
				{
					return Pin;
				}
				return Pin->FindSubPin(Right);
			}
		}
	}
	
	return nullptr;
}

const TArray<URigVMPin*>& URigVMNode::GetOrphanedPins() const
{
	return OrphanedPins;
}

URigVMGraph* URigVMNode::GetGraph() const
{
	if (URigVMGraph* Graph = Cast<URigVMGraph>(GetOuter()))
	{
		return Graph;
	}
	if (URigVMInjectionInfo* InjectionInfo = GetInjectionInfo())
	{
		return InjectionInfo->GetGraph();
	}
	return nullptr;
}

URigVMGraph* URigVMNode::GetRootGraph() const
{
	if (URigVMGraph* Graph = GetGraph())
	{
		return Graph->GetRootGraph();
	}
	return nullptr;
}

URigVMInjectionInfo* URigVMNode::GetInjectionInfo() const
{
	return Cast<URigVMInjectionInfo>(GetOuter());
}

FString URigVMNode::GetNodeTitle() const
{
	if (!NodeTitle.IsEmpty())
	{
		return NodeTitle;
	}
	return GetName();
}

FVector2D URigVMNode::GetPosition() const
{
	return Position;
}

FVector2D URigVMNode::GetSize() const
{
	return Size;
}

FLinearColor URigVMNode::GetNodeColor() const
{
	return NodeColor;
}

FText URigVMNode::GetToolTipText() const
{
	return FText::FromName(GetFName());
}

FText URigVMNode::GetToolTipTextForPin(const URigVMPin* InPin) const
{
	return FText::FromName(InPin->GetFName());
}

bool URigVMNode::IsSelected() const
{
	URigVMGraph* Graph = GetGraph();
	if (Graph)
	{
		return Graph->IsNodeSelected(GetFName());
	}
	return false;
}

bool URigVMNode::IsInjected() const
{
	return Cast<URigVMInjectionInfo>(GetOuter()) != nullptr;
}

bool URigVMNode::IsVisibleInUI() const
{
	return !IsInjected();
}

bool URigVMNode::IsPure() const
{
	if(IsMutable())
	{
		return false;
	}

	for (URigVMPin* Pin : GetPins())
	{
		if(Pin->GetDirection() == ERigVMPinDirection::Hidden)
		{
			return false;
		}
	}

	return true;
}

bool URigVMNode::IsMutable() const
{
	if (const URigVMPin* ExecutePin = FindPin(FRigVMStruct::ExecuteContextName.ToString()))
	{
		if(const UScriptStruct* ScriptStruct = ExecutePin->GetScriptStruct())
		{
			return ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct());
		}
	}
	return false;
}

bool URigVMNode::HasWildCardPin() const
{
	for (const URigVMPin* Pin : GetPins())
	{
		if (Pin->IsWildCard())
		{
			return true;
		}
	}
	return false;
}

bool URigVMNode::IsEvent() const
{
	return IsMutable() && !GetEventName().IsNone();
}

FName URigVMNode::GetEventName() const
{
	return NAME_None;
}

bool URigVMNode::CanOnlyExistOnce() const
{
	return false;
}

bool URigVMNode::HasInputPin(bool bIncludeIO) const
{
	if (HasPinOfDirection(ERigVMPinDirection::Input))
	{
		return true;
	}
	if (bIncludeIO)
	{
		return HasPinOfDirection(ERigVMPinDirection::IO);
	}
	return false;

}

bool URigVMNode::HasIOPin() const
{
	return HasPinOfDirection(ERigVMPinDirection::IO);
}

bool URigVMNode::HasOutputPin(bool bIncludeIO) const
{
	if (HasPinOfDirection(ERigVMPinDirection::Output))
	{
		return true;
	}
	if (bIncludeIO)
	{
		return HasPinOfDirection(ERigVMPinDirection::IO);
	}
	return false;
}

bool URigVMNode::HasPinOfDirection(ERigVMPinDirection InDirection) const
{
	for (URigVMPin* Pin : GetPins())
	{
		if (Pin->GetDirection() == InDirection)
		{
			return true;
		}
	}
	return false;
}

bool URigVMNode::IsLinkedTo(URigVMNode* InNode) const
{
	if (InNode == nullptr)
	{
		return false;
	}
	if (InNode == this)
	{
		return false;
	}
	if (GetGraph() != InNode->GetGraph())
	{
		return false;
	}
	for (URigVMPin* Pin : GetPins())
	{
		if (IsLinkedToRecursive(Pin, InNode))
		{
			return true;
		}
	}
	return false;
}

bool URigVMNode::IsLinkedToRecursive(URigVMPin* InPin, URigVMNode* InNode) const
{
	for (URigVMPin* LinkedPin : InPin->GetLinkedSourcePins())
	{
		if (LinkedPin->GetNode() == InNode)
		{
			return true;
		}
	}
	for (URigVMPin* LinkedPin : InPin->GetLinkedTargetPins())
	{
		if (LinkedPin->GetNode() == InNode)
		{
			return true;
		}
	}
	for (URigVMPin* SubPin : InPin->GetSubPins())
	{
		if (IsLinkedToRecursive(SubPin, InNode))
		{
			return true;
		}
	}
	return false;
}

TArray<URigVMLink*> URigVMNode::GetLinks() const
{
	TArray<URigVMLink*> Links;

	struct Local
	{
		static void Traverse(URigVMPin* InPin, TArray<URigVMLink*>& Links)
		{
			Links.Append(InPin->GetLinks());
			for (URigVMPin* SubPin : InPin->GetSubPins())
			{
				Local::Traverse(SubPin, Links);
			}
		}
	};

	for (URigVMPin* Pin : GetPins())
	{
		Local::Traverse(Pin, Links);
	}

	return Links;
}

TArray<URigVMNode*> URigVMNode::GetLinkedSourceNodes() const
{
	TArray<URigVMNode*> Nodes;
	for (URigVMPin* Pin : GetPins())
	{
		GetLinkedNodesRecursive(Pin, true, Nodes);
	}
	return Nodes;
}

TArray<URigVMNode*> URigVMNode::GetLinkedTargetNodes() const
{
	TArray<URigVMNode*> Nodes;
	for (URigVMPin* Pin : GetPins())
	{
		GetLinkedNodesRecursive(Pin, false, Nodes);
	}
	return Nodes;
}

void URigVMNode::GetLinkedNodesRecursive(URigVMPin* InPin, bool bLookForSources, TArray<URigVMNode*>& OutNodes) const
{
	TArray<URigVMPin*> LinkedPins = bLookForSources ? InPin->GetLinkedSourcePins() : InPin->GetLinkedTargetPins();
	for (URigVMPin* LinkedPin : LinkedPins)
	{
		OutNodes.AddUnique(LinkedPin->GetNode());
	}
	for (URigVMPin* SubPin : InPin->GetSubPins())
	{
		GetLinkedNodesRecursive(SubPin, bLookForSources, OutNodes);
	}
}

const TArray<int32>& URigVMNode::GetInstructionsForVM(URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
	if(const FProfilingCache* Cache = UpdateProfilingCacheIfNeeded(InVM, InProxy))
	{
		return Cache->Instructions;
	}
	return EmptyInstructionArray;
}

TArray<int32> URigVMNode::GetInstructionsForVMImpl(URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
	TArray<int32> Instructions;

#if WITH_EDITOR

	if(InVM == nullptr)
	{
		return Instructions;
	}
	
	if(InProxy.IsValid())
	{
		const FRigVMASTProxy Proxy = InProxy.GetChild((UObject*)this);
		return InVM->GetByteCode().GetAllInstructionIndicesForCallstack(Proxy.GetCallstack().GetStack());
	}
	else
	{
		return InVM->GetByteCode().GetAllInstructionIndicesForSubject((URigVMNode*)this);
	}
	
#endif

	return Instructions;
}

int32 URigVMNode::GetInstructionVisitedCount(URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
#if WITH_EDITOR
	if(InVM)
	{
		if(const FProfilingCache* Cache = UpdateProfilingCacheIfNeeded(InVM, InProxy))
		{
			return Cache->VisitedCount;
		}
	}
#endif
	return 0;
}

double URigVMNode::GetInstructionMicroSeconds(URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
#if WITH_EDITOR
	if(InVM)
	{
		if(const FProfilingCache* Cache = UpdateProfilingCacheIfNeeded(InVM, InProxy))
		{
			return Cache->MicroSeconds;
		}
	}
#endif
	return -1.0;
}

TArray<FRigVMUserWorkflow> URigVMNode::GetSupportedWorkflows(ERigVMUserWorkflowType InType, const UObject* InSubject) const
{
	if(InSubject == nullptr)
	{
		InSubject = this;
	}

	const UScriptStruct* Struct = nullptr;
	if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(this))
	{
		Struct = UnitNode->GetScriptStruct();
	}
	return URigVMUserWorkflowRegistry::Get()->GetWorkflows(InType, Struct, InSubject);
}

bool URigVMNode::IsAggregate() const
{
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	const TArray<URigVMPin*> AggregateInputs = GetAggregateInputs();
	const TArray<URigVMPin*> AggregateOutputs = GetAggregateOutputs();

	if ((AggregateInputs.Num() == 2 && AggregateOutputs.Num() == 1) ||
		(AggregateInputs.Num() == 1 && AggregateOutputs.Num() == 2))
	{
		TArray<URigVMPin*> AggregateAll = AggregateInputs;
		AggregateAll.Append(AggregateOutputs);
		for (int32 i = 1; i < 3; ++i)
		{
			if (AggregateAll[0]->GetCPPType() != AggregateAll[i]->GetCPPType() ||
				AggregateAll[0]->GetCPPTypeObject() != AggregateAll[i]->GetCPPTypeObject())
			{
				return false;
			}
		}
		
		return true;
	}
#endif
	
	return false;
}

URigVMPin* URigVMNode::GetFirstAggregatePin() const
{
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	const TArray<URigVMPin*> Inputs = GetAggregateInputs();
	const TArray<URigVMPin*> Outputs = GetAggregateOutputs();
	if (Inputs.Num() == 2 && Outputs.Num() == 1)
	{
		return Inputs[0];
	}
	if (Inputs.Num() == 1 && Outputs.Num() == 2)
	{
		return Outputs[0];
	}
#endif
	return nullptr;
}

URigVMPin* URigVMNode::GetSecondAggregatePin() const
{
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	const TArray<URigVMPin*> Inputs = GetAggregateInputs();
	const TArray<URigVMPin*> Outputs = GetAggregateOutputs();
	if (Inputs.Num() == 2 && Outputs.Num() == 1)
	{
		return Inputs[1];
	}
	if (Inputs.Num() == 1 && Outputs.Num() == 2)
	{
		return Outputs[1];
	}
#endif
	return nullptr;
}

URigVMPin* URigVMNode::GetOppositeAggregatePin() const
{
#if UE_RIGVM_AGGREGATE_NODES_ENABLED
	const TArray<URigVMPin*> Inputs = GetAggregateInputs();
	const TArray<URigVMPin*> Outputs = GetAggregateOutputs();
	if (Inputs.Num() == 2 && Outputs.Num() == 1)
	{
		return Outputs[0];
	}
	if (Inputs.Num() == 1 && Outputs.Num() == 2)
	{
		return Inputs[0];
	}
#endif
	return nullptr;
}

bool URigVMNode::IsInputAggregate() const
{
	return GetAggregateInputs().Num() == 2;
}

#if WITH_EDITOR

const URigVMNode::FProfilingCache* URigVMNode::UpdateProfilingCacheIfNeeded(URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
	if(InVM == nullptr)
	{
		return nullptr;
	}
	
	const uint32 VMHash = HashCombine(GetTypeHash(InVM), GetTypeHash(InVM->GetNumExecutions()));
	if(VMHash != ProfilingHash)
	{
		ProfilingCache.Reset();
	}
	ProfilingHash = VMHash;

	const uint32 ProxyHash = InProxy.IsValid() ? GetTypeHash(InProxy) : GetTypeHash(this);

	const TSharedPtr<FProfilingCache>* ExistingCache = ProfilingCache.Find(ProxyHash);
	if(ExistingCache)
	{
		return ExistingCache->Get();
	}

	TSharedPtr<FProfilingCache> Cache(new FProfilingCache);

	Cache->Instructions = GetInstructionsForVMImpl(InVM, InProxy);
	Cache->VisitedCount = 0;
	Cache->MicroSeconds = -1.0;

	if(Cache->Instructions.Num() > 0)
	{
		for(const int32 Instruction : Cache->Instructions)
		{
			const int32 CountPerInstruction = InVM->GetInstructionVisitedCount(Instruction);
			Cache->VisitedCount += CountPerInstruction;

			const double MicroSecondsPerInstruction = InVM->GetInstructionMicroSeconds(Instruction);
			if(MicroSecondsPerInstruction >= 0.0)
			{
				if(Cache->MicroSeconds < 0.0)
				{
					Cache->MicroSeconds = MicroSecondsPerInstruction;
				}
				else
				{
					Cache->MicroSeconds += MicroSecondsPerInstruction;
				}
			}
		}
	}

	ProfilingCache.Add(ProxyHash, Cache);
	return Cache.Get();;
}

#endif
