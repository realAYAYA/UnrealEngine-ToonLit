// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraTraversalStateContext.h"

#include "NiagaraCompilationPrivate.h"
#include "NiagaraConstants.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraphDigest.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeStaticSwitch.h"
#include "NiagaraParameterMapHistory.h"

void FNiagaraTraversalStateContext::PushGraphInternal(const FNiagaraCompilationNode* CallingNode, const FNiagaraCompilationGraph* Graph, const FNiagaraFixedConstantResolver& ConstantResolver)
{
	if (!Graph)
	{
		return;
	}

	// now we actually go through the graph and populate the values
	for (const TUniquePtr<FNiagaraCompilationNode>& Node : Graph->Nodes)
	{
		if (const FNiagaraCompilationNodeStaticSwitch* StaticSwitchNode = Node->AsType<FNiagaraCompilationNodeStaticSwitch>())
		{
			int32 SwitchValue = 0;
			bool IsValueSet = false;

			if (StaticSwitchNode->bSetByCompiler || StaticSwitchNode->bSetByPin)
			{
				const FNiagaraVariable* Found = FNiagaraConstants::FindStaticSwitchConstant(StaticSwitchNode->SwitchConstant);
				FNiagaraVariable Constant = Found ? *Found : FNiagaraVariable();
				if (Found && ConstantResolver.ResolveConstant(Constant))
				{
					if (StaticSwitchNode->SwitchType == ENiagaraStaticSwitchType::Bool)
					{
						SwitchValue = Constant.GetValue<bool>();
						IsValueSet = true;
					}
					else if (StaticSwitchNode->SwitchType == ENiagaraStaticSwitchType::Integer ||
						StaticSwitchNode->SwitchType == ENiagaraStaticSwitchType::Enum)
					{
						SwitchValue = Constant.GetValue<int32>();
						IsValueSet = true;
					}
				}
			}
			else if (!StaticSwitchNode->bSetByCompiler)
			{
				for (const FNiagaraCompilationInputPin& InputPin : CallingNode->InputPins)
				{
					if (InputPin.PinName.IsEqual(StaticSwitchNode->InputParameterName) && InputPin.Variable.GetType() == StaticSwitchNode->InputType)
					{
						if (FNiagaraCompilationNodeStaticSwitch::ResolveConstantValue(InputPin, SwitchValue))
						{
							IsValueSet = true;
						}
					}
				}
			}

			if (IsValueSet)
			{
				const uint32 SwitchNodeHash = HashCombine(TraversalStack.Top().FullStackHash, GetTypeHash(StaticSwitchNode->NodeGuid));
				if (ensure(!StaticSwitchValueMap.Contains(SwitchNodeHash)))
				{
					StaticSwitchValueMap.Add(SwitchNodeHash, SwitchValue);
				}
			}
			else
			{
				// value doesn't have to be set; it could be set as we process the pins for this graph for real and end up
				// gathering the value by it's static pin
				ensure(StaticSwitchNode->bSetByPin);
			}
		}
		else if (const FNiagaraCompilationNodeFunctionCall* InnerFunctionNode = Node->AsType<FNiagaraCompilationNodeFunctionCall>())
		{
			const uint32 InnerFunctionNodeHash = HashCombine(TraversalStack.Top().FullStackHash, GetTypeHash(InnerFunctionNode->NodeGuid));

			// based on the original code bInheritDebugState drives whether we use the system value vs NoDebug.  Note
			// that the serialized Debugstate is never used.
			ENiagaraFunctionDebugState CachedDebugState = InnerFunctionNode->bInheritDebugState
				? ConstantResolver.GetDebugState()
				: ENiagaraFunctionDebugState::NoDebug;

			if (ensure(!FunctionDebugStateMap.Contains(InnerFunctionNodeHash)))
			{
				FunctionDebugStateMap.Add(InnerFunctionNodeHash, CachedDebugState);
			}

			for (const FNiagaraCompilationNodeFunctionCall::FTaggedVariable& TaggedVariable : InnerFunctionNode->PropagatedStaticSwitchParameters)
			{
				const FNiagaraCompilationInputPin* ValuePin = InnerFunctionNode->InputPins.FindByPredicate([&TaggedVariable](const FNiagaraCompilationInputPin& InputPin) -> bool
					{
						return InputPin.PinName == TaggedVariable.Key.GetName();
					});

				if (ValuePin)
				{
					const FNiagaraCompilationInputPin* CallerInputPin = CallingNode->InputPins.FindByPredicate([&TaggedVariable](const FNiagaraCompilationInputPin& InputPin) -> bool
						{
							return InputPin.PinName == TaggedVariable.Value;
						});

					if (CallerInputPin)
					{
						const uint32 FunctionPinNodeHash = HashCombine(InnerFunctionNodeHash, GetTypeHash(ValuePin->PinName));
						if (ensure(!FunctionDefaultValueMap.Contains(FunctionPinNodeHash)))
						{
							FunctionDefaultValueMap.Add(FunctionPinNodeHash, CallerInputPin->DefaultValue);
						}
					}
				}
			}
		}
	}
}

void FNiagaraTraversalStateContext::PushFunction(const FNiagaraCompilationNodeFunctionCall* FunctionCall, const FNiagaraFixedConstantResolver& ConstantResolver)
{
	TOptional<uint32> CurrentStackHash;
	if (!TraversalStack.IsEmpty())
	{
		CurrentStackHash = TraversalStack.Top().FullStackHash;
	}

	FNiagaraTraversalStackEntry& StackTop = TraversalStack.AddDefaulted_GetRef();
	StackTop.FriendlyName = FString::Printf(TEXT("FuncionName - %s | FullName - %s | FullTitle - %s | NodeType - %d"),
		*FunctionCall->FunctionName,
		*FunctionCall->FullName,
		*FunctionCall->FullTitle,
		(int32)FunctionCall->NodeType);

	StackTop.NodeGuid = FunctionCall->NodeGuid;
	const uint32 NodeGuidHash = GetTypeHash(StackTop.NodeGuid);
	StackTop.FullStackHash = CurrentStackHash.IsSet()
		? HashCombine(*CurrentStackHash, NodeGuidHash)
		: NodeGuidHash;

	if (FunctionCall->CalledGraph)
	{
		PushGraphInternal(FunctionCall, FunctionCall->CalledGraph.Get(), ConstantResolver);
	}
}

void FNiagaraTraversalStateContext::PushEmitter(const FNiagaraCompilationNodeEmitter* Emitter)
{
	TOptional<uint32> CurrentStackHash;
	if (!TraversalStack.IsEmpty())
	{
		CurrentStackHash = TraversalStack.Top().FullStackHash;
	}

	FNiagaraTraversalStackEntry& StackTop = TraversalStack.AddDefaulted_GetRef();
	StackTop.FriendlyName = FString::Printf(TEXT("EmitterName - %s | FullName - %s | FullTitle - %s | NodeType - %d"),
		*Emitter->EmitterUniqueName,
		*Emitter->FullName,
		*Emitter->FullTitle,
		(int32)Emitter->NodeType);

	StackTop.NodeGuid = Emitter->NodeGuid;
	const uint32 NodeGuidHash = GetTypeHash(StackTop.NodeGuid);
	StackTop.FullStackHash = CurrentStackHash.IsSet()
		? HashCombine(*CurrentStackHash, NodeGuidHash)
		: NodeGuidHash;
}

void FNiagaraTraversalStateContext::PopFunction(const FNiagaraCompilationNodeFunctionCall* FunctionCall)
{
	check(!TraversalStack.IsEmpty() && TraversalStack.Top().NodeGuid == FunctionCall->NodeGuid);
	TraversalStack.Pop();
}

void FNiagaraTraversalStateContext::PopEmitter(const FNiagaraCompilationNodeEmitter* Emitter)
{
	check(!TraversalStack.IsEmpty() && TraversalStack.Top().NodeGuid == Emitter->NodeGuid);
	TraversalStack.Pop();
}

bool FNiagaraTraversalStateContext::GetStaticSwitchValue(const FGuid& NodeGuid, int32& StaticSwitchValue) const
{
	if (!TraversalStack.IsEmpty())
	{
		if (const int32* ValuePtr = StaticSwitchValueMap.Find(HashCombine(TraversalStack.Top().FullStackHash, GetTypeHash(NodeGuid))))
		{
			StaticSwitchValue = *ValuePtr;
			return true;
		}
	}
	return false;
}

bool FNiagaraTraversalStateContext::GetFunctionDefaultValue(const FGuid& NodeGuid, FName PinName, FString& FunctionDefaultValue) const
{
	if (!TraversalStack.IsEmpty())
	{
		const uint32 NodeHash = HashCombine(TraversalStack.Top().FullStackHash, GetTypeHash(NodeGuid));
		const uint32 PinHash = HashCombine(NodeHash, GetTypeHash(PinName));
		if (const FString* ValuePtr = FunctionDefaultValueMap.Find(PinHash))
		{
			FunctionDefaultValue = *ValuePtr;
			return true;
		}
	}
	return false;
}

bool FNiagaraTraversalStateContext::GetFunctionDebugState(const FGuid& NodeGuid, ENiagaraFunctionDebugState& DebugState) const
{
	if (!TraversalStack.IsEmpty())
	{
		const uint32 NodeHash = HashCombine(TraversalStack.Top().FullStackHash, GetTypeHash(NodeGuid));
		if (const ENiagaraFunctionDebugState* ValuePtr = FunctionDebugStateMap.Find(NodeHash))
		{
			DebugState = *ValuePtr;
			return true;
		}
	}
	return false;
}

FNiagaraFixedConstantResolver::FNiagaraFixedConstantResolver()
{
	InitConstants();
	SetScriptUsage(ENiagaraScriptUsage::Function);
	SetDebugState(ENiagaraFunctionDebugState::NoDebug);
}

FNiagaraFixedConstantResolver::FNiagaraFixedConstantResolver(const FTranslator* InTranslator, ENiagaraScriptUsage ScriptUsage, ENiagaraFunctionDebugState DebugState)
	: Translator(InTranslator)
{
	InitConstants();
	SetScriptUsage(ScriptUsage);
	SetDebugState(DebugState);
}

FNiagaraFixedConstantResolver::FNiagaraFixedConstantResolver(const FCompileConstantResolver& SrcConstantResolver)
{
	InitConstants();
	SetScriptUsage(SrcConstantResolver.GetUsage());
	SetDebugState(SrcConstantResolver.CalculateDebugState());

	for (FNiagaraVariable& ResolvedConstant : ResolvedConstants)
	{
		SrcConstantResolver.ResolveConstant(ResolvedConstant);
	}
}

void FNiagaraFixedConstantResolver::InitConstants()
{
	static const FName ConstantNames[(uint8)EResolvedConstant::Count] =
	{
		TEXT("Function.DebugState"),
		TEXT("Script.Usage"),
		TEXT("Script.Context"),
		TEXT("Emitter.Localspace"),
		TEXT("Emitter.Determinism"),
		TEXT("Emitter.InterpolatedSpawn"),
		TEXT("Emitter.SimulationTarget")
	};

	ResolvedConstants =
	{
		{ FNiagaraTypeDefinition::GetFunctionDebugStateEnum(), ConstantNames[(uint8)EResolvedConstant::FunctionDebugState] },
		{ FNiagaraTypeDefinition::GetScriptUsageEnum(), ConstantNames[(uint8)EResolvedConstant::ScriptUsage] },
		{ FNiagaraTypeDefinition::GetScriptContextEnum(), ConstantNames[(uint8)EResolvedConstant::ScriptContext] },
		{ FNiagaraTypeDefinition::GetBoolDef(), ConstantNames[(uint8)EResolvedConstant::EmitterLocalspace] },
		{ FNiagaraTypeDefinition::GetBoolDef(), ConstantNames[(uint8)EResolvedConstant::EmitterDeterminism] },
		{ FNiagaraTypeDefinition::GetBoolDef(), ConstantNames[(uint8)EResolvedConstant::EmitterInterpolatedSpawn] },
		{ FNiagaraTypeDefinition::GetSimulationTargetEnum(), ConstantNames[(uint8)EResolvedConstant::EmitterSimulationTarget] }
	};
}

void FNiagaraFixedConstantResolver::SetScriptUsage(ENiagaraScriptUsage ScriptUsage)
{
	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)FNiagaraUtilities::ConvertScriptUsageToStaticSwitchUsage(ScriptUsage);
		ResolvedConstants[(uint8)EResolvedConstant::ScriptUsage].SetValue(EnumValue);
	}

	{
		FNiagaraInt32 EnumValue;
		EnumValue.Value = (uint8)FNiagaraUtilities::ConvertScriptUsageToStaticSwitchContext(ScriptUsage);
		ResolvedConstants[(uint8)EResolvedConstant::ScriptContext].SetValue(EnumValue);
	}
}

void FNiagaraFixedConstantResolver::SetDebugState(ENiagaraFunctionDebugState DebugState)
{
	FNiagaraInt32 EnumValue;
	EnumValue.Value = (uint8)DebugState;
	ResolvedConstants[(uint8)EResolvedConstant::FunctionDebugState].SetValue(EnumValue);
}

bool FNiagaraFixedConstantResolver::ResolveConstant(FNiagaraVariable& OutConstant) const
{
	// handle translator case
	if (Translator && Translator->GetLiteralConstantVariable(OutConstant))
	{
		return true;
	}

	if (const FNiagaraVariable* ResolvedConstant = ResolvedConstants.FindByKey(OutConstant))
	{
		if (ResolvedConstant->IsDataAllocated())
		{
			OutConstant.SetData(ResolvedConstant->GetData());
			return true;
		}
	}

	return false;
}

FNiagaraFixedConstantResolver FNiagaraFixedConstantResolver::WithDebugState(ENiagaraFunctionDebugState InDebugState) const
{
	FNiagaraFixedConstantResolver Copy = *this;
	Copy.SetDebugState(InDebugState);
	return Copy;
}

FNiagaraFixedConstantResolver FNiagaraFixedConstantResolver::WithUsage(ENiagaraScriptUsage ScriptUsage) const
{
	FNiagaraFixedConstantResolver Copy = *this;
	Copy.SetScriptUsage(ScriptUsage);
	return Copy;
}

ENiagaraFunctionDebugState FNiagaraFixedConstantResolver::GetDebugState() const
{
	FNiagaraInt32 EnumValue = ResolvedConstants[(uint8)EResolvedConstant::FunctionDebugState].GetValue<FNiagaraInt32>();
	return (ENiagaraFunctionDebugState)EnumValue.Value;
}

void FNiagaraFixedConstantResolver::AddNamedChildResolver(FName ScopeName, const FNiagaraFixedConstantResolver& ChildResolver)
{
	if (ensure(FindChildResolver(ScopeName) == nullptr))
	{
		ChildResolversByName.Emplace(ScopeName, ChildResolver);
	}
}

const FNiagaraFixedConstantResolver* FNiagaraFixedConstantResolver::FindChildResolver(FName ScopeName) const
{
	const FNamedResolverPair* ChildResolver = ChildResolversByName.FindByPredicate([ScopeName](const FNamedResolverPair& NamedPair) -> bool
	{
			return NamedPair.Key == ScopeName;
	});

	return ChildResolver ? &ChildResolver->Value : nullptr;
}
