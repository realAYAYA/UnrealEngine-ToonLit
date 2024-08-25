// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMModel/RigVMNode.h"
#include "RigVMStringUtils.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/Nodes/RigVMLibraryNode.h"
#include "RigVMModel/RigVMFunctionLibrary.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "RigVMCore/RigVMStruct.h"
#include "RigVMUserWorkflowRegistry.h"
#include "Logging/LogScopedVerbosityOverride.h"

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
	return RigVMStringUtils::SplitNodePathAtStart(InNodePath, LeftMost, Right);
}

bool URigVMNode::SplitNodePathAtEnd(const FString& InNodePath, FString& Left, FString& RightMost)
{
	return RigVMStringUtils::SplitNodePathAtEnd(InNodePath, Left, RightMost);
}

bool URigVMNode::SplitNodePath(const FString& InNodePath, TArray<FString>& Parts)
{
	return RigVMStringUtils::SplitNodePath(InNodePath, Parts);
}

FString URigVMNode::JoinNodePath(const FString& Left, const FString& Right)
{
	return RigVMStringUtils::JoinNodePath(Left, Right);
}

FString URigVMNode::JoinNodePath(const TArray<FString>& InParts)
{
	return RigVMStringUtils::JoinNodePath(InParts);
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

int32 URigVMNode::GetGraphDepth() const
{
	return GetGraph()->GetGraphDepth();
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

void URigVMNode::UpdateDecoratorRootPinNames()
{
	TArray<FString> NewDecoratorRootPinNames;
	for(URigVMPin* Pin : GetPins())
	{
		if(Pin->IsDecoratorPin())
		{
			NewDecoratorRootPinNames.Add(Pin->GetName());
		}
	}
	DecoratorRootPinNames = NewDecoratorRootPinNames;
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
	for (const URigVMPin* Pin : GetPins())
	{
		if(const UScriptStruct* ScriptStruct = Pin->GetScriptStruct())
		{
			if (ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
			{
				return true;
			}
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

bool URigVMNode::HasLazyPin(bool bOnlyConsiderPinsWithLinks) const
{
	return Pins.ContainsByPredicate([bOnlyConsiderPinsWithLinks](const URigVMPin* Pin) -> bool
	{
		if(Pin->IsLazy())
		{
			if(bOnlyConsiderPinsWithLinks)
			{
				return Pin->GetLinkedSourcePins(true).Num() > 0;
			}
			return true;
		}
		return false;
	});
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

uint32 URigVMNode::GetStructureHash() const
{
	uint32 Hash = GetTypeHash(GetName());
	for(const URigVMPin* Pin : Pins)
	{
		const uint32 PinHash = Pin->GetStructureHash();
		Hash = HashCombine(Hash, PinHash);
	}
	return Hash;
}

TArray<URigVMPin*> URigVMNode::GetDecoratorPins() const
{
	TArray<URigVMPin*> DecoratorPins;
	DecoratorPins.Reserve(DecoratorRootPinNames.Num());
	
	for(const FString& DecoratorRootPinName : DecoratorRootPinNames)
	{
		URigVMPin* DecoratorPin = FindPin(DecoratorRootPinName);
		check(DecoratorPin);

		DecoratorPins.Add(DecoratorPin);
	}

	return DecoratorPins;
}

bool URigVMNode::IsDecoratorPin(FName InName) const
{
	if(const URigVMPin* Pin = FindPin(InName.ToString()))
	{
		return IsDecoratorPin(Pin);
	}
	return false;
}

bool URigVMNode::IsDecoratorPin(const URigVMPin* InDecoratorPin) const
{
	return FindDecorator(InDecoratorPin) != nullptr;
}

URigVMPin* URigVMNode::FindDecorator(const FName& InName) const
{
	const FString NameString = InName.ToString();
	for(const FString& DecoratorRootPinName : DecoratorRootPinNames)
	{
		if(DecoratorRootPinName.Equals(NameString, ESearchCase::CaseSensitive))
		{
			return FindPin(DecoratorRootPinName);
		}
	}
	return nullptr;
}

URigVMPin* URigVMNode::FindDecorator(const URigVMPin* InDecoratorPin) const
{
	if(InDecoratorPin)
	{
		const URigVMPin* RootPin = InDecoratorPin->GetRootPin();
		if(RootPin->GetNode() == this)
		{
			return FindDecorator(RootPin->GetFName());
		}
	}
	return nullptr;
}

TSharedPtr<FStructOnScope> URigVMNode::GetDecoratorInstance(const FName& InName, bool bUseDefaultValueFromPin) const
{
	return GetDecoratorInstance(FindPin(InName.ToString()), bUseDefaultValueFromPin);
}

TSharedPtr<FStructOnScope> URigVMNode::GetDecoratorInstance(const URigVMPin* InDecoratorPin, bool bUseDefaultValueFromPin) const
{
	if(const URigVMPin* RootPin = FindDecorator(InDecoratorPin))
	{
		check(RootPin->IsStruct());

		UScriptStruct* ScriptStruct = RootPin->GetScriptStruct();
		check(ScriptStruct->IsChildOf(FRigVMDecorator::StaticStruct()));

		TSharedPtr<FStructOnScope> Scope(new FStructOnScope(ScriptStruct));
		FRigVMDecorator* Decorator = (FRigVMDecorator*)Scope->GetStructMemory();

		if(bUseDefaultValueFromPin)
		{
			const FString DefaultValue = RootPin->GetDefaultValue();
			if(!DefaultValue.IsEmpty())
			{
				FRigVMPinDefaultValueImportErrorContext ErrorPipe;
				{
					// force logging to the error pipe for error detection
					LOG_SCOPE_VERBOSITY_OVERRIDE(LogExec, ELogVerbosity::Verbose); 
					ScriptStruct->ImportText(*DefaultValue, Decorator, nullptr, PPF_None, &ErrorPipe, ScriptStruct->GetName()); 
				}
			}
		}

		Decorator->Name = RootPin->GetFName();
		Decorator->DecoratorStruct = ScriptStruct;
		
		return Scope;
	}

	static const TSharedPtr<FStructOnScope> EmptyScope;
	return EmptyScope;
}

UScriptStruct* URigVMNode::GetDecoratorScriptStruct(const FName& InName) const
{
	return GetDecoratorScriptStruct(FindPin(InName.ToString()));
}

UScriptStruct* URigVMNode::GetDecoratorScriptStruct(const URigVMPin* InDecoratorPin) const
{
	if(const URigVMPin* RootPin = FindDecorator(InDecoratorPin))
	{
		check(RootPin->IsStruct());

		UScriptStruct* ScriptStruct = RootPin->GetScriptStruct();
		check(ScriptStruct->IsChildOf(FRigVMDecorator::StaticStruct()));
		return ScriptStruct;
	}

	return nullptr;
}

URigVMLibraryNode* URigVMNode::FindFunctionForNode() const  
{
	const UObject* Subject = this;
	while (Subject->GetOuter() && !Subject->GetOuter()->IsA<URigVMFunctionLibrary>())
	{
		Subject = Subject->GetOuter();
		if(Subject == nullptr)
		{
			return nullptr;
		}
	}

	return const_cast<URigVMLibraryNode*>(Cast<URigVMLibraryNode>(Subject));
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

const TArray<int32>& URigVMNode::GetInstructionsForVM(const FRigVMExtendedExecuteContext& Context, URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
	if(const FProfilingCache* Cache = UpdateProfilingCacheIfNeeded(Context, InVM, InProxy))
	{
		return Cache->Instructions;
	}
	return EmptyInstructionArray;
}

TArray<int32> URigVMNode::GetInstructionsForVMImpl(const FRigVMExtendedExecuteContext& Context, URigVM* InVM, const FRigVMASTProxy& InProxy) const
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
#else
	return Instructions;
#endif
}

int32 URigVMNode::GetInstructionVisitedCount(const FRigVMExtendedExecuteContext& Context, URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
#if WITH_EDITOR
	if(InVM)
	{
		if(const FProfilingCache* Cache = UpdateProfilingCacheIfNeeded(Context, InVM, InProxy))
		{
			return Cache->VisitedCount;
		}
	}
#endif
	return 0;
}

double URigVMNode::GetInstructionMicroSeconds(const FRigVMExtendedExecuteContext& Context, URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
#if WITH_EDITOR
	if(InVM)
	{
		if(const FProfilingCache* Cache = UpdateProfilingCacheIfNeeded(Context, InVM, InProxy))
		{
			return Cache->MicroSeconds;
		}
	}
#endif
	return -1.0;
}

bool URigVMNode::IsLoopNode() const
{
	if(IsControlFlowNode())
	{
		static const TArray<FName> ExpectedLoopBlocks = {FRigVMStruct::ExecuteContextName, FRigVMStruct::ForLoopCompletedPinName};
		const TArray<FName>& Blocks = GetControlFlowBlocks();
		if(Blocks.Num() == ExpectedLoopBlocks.Num())
		{
			return Blocks[0] == ExpectedLoopBlocks[0] && Blocks[1] == ExpectedLoopBlocks[1];
		}
	}

	return false;
}

bool URigVMNode::IsControlFlowNode() const
{
	return !GetControlFlowBlocks().IsEmpty();
}

const TArray<FName>& URigVMNode::GetControlFlowBlocks() const
{
	static const TArray<FName> EmptyArray;
	return EmptyArray;
}

const bool URigVMNode::IsControlFlowBlockSliced(const FName& InBlockName) const
{
	return false;
}

bool URigVMNode::IsWithinLoop() const
{
	for(const URigVMPin* Pin : Pins)
	{
		const TArray<URigVMPin*> SourcePins = Pin->GetLinkedSourcePins(true);
		for(const URigVMPin* SourcePin : SourcePins)
		{
			if(SourcePin->GetNode()->IsLoopNode())
			{
				if(!SourcePin->IsExecuteContext() || SourcePin->GetFName() != FRigVMStruct::ForLoopCompletedPinName)
				{
					return true;
				}
			}
		}

		for(const URigVMPin* SourcePin : SourcePins)
		{
			if(SourcePin->GetNode()->IsWithinLoop())
			{
				return true;
			}
		}
	}
	return false;
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

const URigVMNode::FProfilingCache* URigVMNode::UpdateProfilingCacheIfNeeded(const FRigVMExtendedExecuteContext& Context, URigVM* InVM, const FRigVMASTProxy& InProxy) const
{
	if(InVM == nullptr)
	{
		return nullptr;
	}
	
	const uint32 VMHash = HashCombine(GetTypeHash(InVM), GetTypeHash(Context.GetNumExecutions()));
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

	Cache->Instructions = GetInstructionsForVMImpl(Context, InVM, InProxy);
	Cache->VisitedCount = 0;
	Cache->MicroSeconds = -1.0;

	if(Cache->Instructions.Num() > 0)
	{
		for(const int32 Instruction : Cache->Instructions)
		{
			const int32 CountPerInstruction = InVM->GetInstructionVisitedCount(Context, Instruction);
			Cache->VisitedCount += CountPerInstruction;

			const double MicroSecondsPerInstruction = InVM->GetInstructionMicroSeconds(Context, Instruction);
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
