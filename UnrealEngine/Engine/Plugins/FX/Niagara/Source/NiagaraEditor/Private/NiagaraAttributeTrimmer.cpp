// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraHlslTranslator.h"

#include "Algo/ForEach.h"
#include "Algo/RemoveIf.h"
#include "EdGraphSchema_Niagara.h"
#include "NiagaraConstants.h"
#include "NiagaraModule.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeInput.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeParameterMapGet.h"
#include "NiagaraNodeParameterMapSet.h"
#include "NiagaraScriptVariable.h"

uint32 GetTypeHash(const FModuleScopedPin& ScopedPin)
{
	return GetTypeHash(TTuple<const UEdGraphPin*, FName>(ScopedPin.Pin, ScopedPin.ModuleName));
}

namespace NiagaraAttributeTrimming
{
	using FDependencySet = TSet<FModuleScopedPin>;
	struct FCustomHlslNodeInfo
	{
		bool bHasDataInterfaceInputs = false;
		bool bHasImpureFunctionText = false;
	};

	using FCustomHlslNodeMap = TMap<const UNiagaraNodeCustomHlsl*, FCustomHlslNodeInfo>;

	struct FDependencyChain
	{
		FDependencySet Pins;
		FCustomHlslNodeMap CustomNodes;
	};

	using FDependencyMap = TMap<FModuleScopedPin, FDependencyChain>;

	class FFunctionInputResolver
	{
	public:
		FFunctionInputResolver(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
		{
			TMap<FName, FModuleScopedPin> UnresolvedInputs;

			Traverse(TEXT(""), Graph, Usage, UnresolvedInputs);
		}

		FModuleScopedPin ResolveInput(const FModuleScopedPin& Input) const
		{
			if (const FModuleScopedPin* FunctionInput = FunctionInputMap.Find(Input))
			{
				return *FunctionInput;
			}

			return FModuleScopedPin();
		}

	private:
		void Traverse(FString Namespace, UNiagaraGraph* Graph, ENiagaraScriptUsage Usage, TMap<FName, FModuleScopedPin>& UnresolvedInputs)
		{
			const FString NamespacePrefix = (Namespace.Len() > 0) ? (Namespace + TEXT(".")) : TEXT("");

			TArray<UNiagaraNodeOutput*> OutputNodes;
			Graph->FindOutputNodes(Usage, OutputNodes);

			for (UNiagaraNodeOutput* OutputNode : OutputNodes)
			{
				TArray<UNiagaraNode*> TraversedNodes;
				Graph->BuildTraversal(TraversedNodes, OutputNode, true);

				for (UNiagaraNode* Node : TraversedNodes)
				{
					if (UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(Node))
					{
						if (UNiagaraGraph* FunctionGraph = FunctionNode->GetCalledGraph())
						{
							TMap<FName, FModuleScopedPin> FunctionInputs;
							Traverse(NamespacePrefix + FunctionNode->GetFunctionName(), FunctionGraph, FunctionNode->GetCalledUsage(), FunctionInputs);

							// resolve any of the UnresolvedInputs that we've accumulated
							for (UEdGraphPin* NodePin : FunctionNode->GetAllPins())
							{
								if (NodePin->Direction == EEdGraphPinDirection::EGPD_Input)
								{
									if (FModuleScopedPin* InnerPin = FunctionInputs.Find(NodePin->PinName))
									{
										FunctionInputMap.Add(*InnerPin, FModuleScopedPin(NodePin, *Namespace));
										FunctionInputs.Remove(NodePin->PinName);
									}
								}
							}
						}
					}
					if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(Node))
					{
						int32 OutputPinCount = 0;
						for (UEdGraphPin* InputNodePin : InputNode->GetAllPins())
						{
							if (InputNodePin->Direction == EEdGraphPinDirection::EGPD_Output)
							{
								check(OutputPinCount == 0);
								++OutputPinCount;

								UnresolvedInputs.Add(InputNode->Input.GetName(), FModuleScopedPin(InputNodePin, *Namespace));
							}
						}
					}
				}
			}
		}

		TMap<FModuleScopedPin, FModuleScopedPin> FunctionInputMap;
	};

	class FImpureFunctionParser
	{
	public:
		FImpureFunctionParser(UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
		{
			Traverse(TEXT(""), Graph, Usage);
		}

		const TArray<FModuleScopedPin>& ReadFunctionInputs() const
		{
			return ImpureFunctionInputs;
		}

	private:
		void Traverse(FString Namespace, UNiagaraGraph* Graph, ENiagaraScriptUsage Usage)
		{
			const FString NamespacePrefix = (Namespace.Len() > 0) ? (Namespace + TEXT(".")) : TEXT("");

			TArray<UNiagaraNodeOutput*> OutputNodes;
			Graph->FindOutputNodes(Usage, OutputNodes);

			for (UNiagaraNodeOutput* OutputNode : OutputNodes)
			{
				TArray<UNiagaraNode*> TraversedNodes;
				Graph->BuildTraversal(TraversedNodes, OutputNode, true);

				for (UNiagaraNode* Node : TraversedNodes)
				{
					if (UNiagaraNodeFunctionCall* FunctionNode = Cast<UNiagaraNodeFunctionCall>(Node))
					{
						if (UNiagaraGraph* FunctionGraph = FunctionNode->GetCalledGraph())
						{
							Traverse(NamespacePrefix + FunctionNode->GetFunctionName(), FunctionGraph, FunctionNode->GetCalledUsage());
						}

						if (FunctionNode->Signature.bRequiresExecPin)
						{
							for (const UEdGraphPin* Pin : FunctionNode->GetAllPins())
							{
								if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
								{
									ImpureFunctionInputs.Emplace(Pin, *Namespace);
								}
							}
						}
					}
				}
			}
		}

		TArray<FModuleScopedPin> ImpureFunctionInputs;
	};

	class FExpressionBuilder
	{
	public:

		FExpressionBuilder(const FNiagaraParameterMapHistory& InParamMap, const FFunctionInputResolver& InInputResolver, const UEdGraphSchema_Niagara* InNiagaraSchema)
		: ParamMap(InParamMap)
		, InputResolver(InInputResolver)
		, NiagaraSchema(InNiagaraSchema)
		{}

		// generates a set of dependencies for a specific pin
		//	-generates the set of input pins to nodes that contribute to the 'expression' of the supplied output pin (EndPin)
		//		An expression is the set of nodes that define what is being set in a MapSet.  It does not traverse through
		//		Exec pins as those nodes would be part of another expression
		//	-generates the list of custom HLSL nodes that are encounterd in the traversal
		void FindDependencies(const FModuleScopedPin& EndPin, FDependencyChain& Dependencies)
		{
			FindDependenciesInternal(EndPin, Dependencies, nullptr);
		}

	private:
		// generates a set of dependencies for a specific pin
		//	-generates the set of input pins to nodes that contribute to the 'expression' of the supplied output pin (EndPin)
		//		An expression is the set of nodes that define what is being set in a MapSet.  It does not traverse through
		//		Exec pins as those nodes would be part of another expression
		//	-generates the list of custom HLSL nodes that are encounterd in the traversal
		void FindDependenciesInternal(const FModuleScopedPin& EndPin, FDependencyChain& Dependencies, TArray<UNiagaraNodeInput*>* InputNodes)
		{
			TStringBuilder<1024> NamespaceBuilder;

			TArray<FModuleScopedPin> PinsToEvaluate;
			TSet<UNiagaraNode*> EvaluatedNodes;

			FModuleScopedPin CurrentPin = EndPin;
			while (CurrentPin.Pin)
			{
				for (UEdGraphPin* LinkedPin : CurrentPin.Pin->LinkedTo)
				{
					if (LinkedPin->Direction == EEdGraphPinDirection::EGPD_Input)
					{
						continue;
					}

					if (UEdGraphPin* TracedPin = EvaluateStaticSwitches ? UNiagaraNode::TraceOutputPin(LinkedPin) : LinkedPin)
					{
						UNiagaraNode* CurrentNode = CastChecked<UNiagaraNode>(TracedPin->GetOwningNode());

						bool AlreadyEvaluated = false;

						// record any output pins to a parameter get node
						if (UNiagaraNodeParameterMapGet* ParamGetNode = Cast<UNiagaraNodeParameterMapGet>(CurrentNode))
						{
							Dependencies.Pins.Emplace(FModuleScopedPin(TracedPin, CurrentPin.ModuleName));
						}
						else if (UNiagaraNodeFunctionCall* FunctionCallNode = Cast<UNiagaraNodeFunctionCall>(CurrentNode))
						{
							TArray<UNiagaraNodeOutput*> InnerOutputNodes;

							if (const UNiagaraGraph* NodeGraph = FunctionCallNode->GetCalledGraph())
							{
								NodeGraph->FindOutputNodes(InnerOutputNodes);

								NamespaceBuilder.Reset();
								if (CurrentPin.ModuleName != NAME_None)
								{
									NamespaceBuilder << CurrentPin.ModuleName.ToString();
									NamespaceBuilder << TEXT(".");
								}
								NamespaceBuilder << FunctionCallNode->GetFunctionName();

								const FName AggregateModuleName = *NamespaceBuilder;

								for (const UNiagaraNodeOutput* InnerOutputNode : InnerOutputNodes)
								{
									for (const UEdGraphPin* InnerOutputNodePin : InnerOutputNode->GetAllPins())
									{
										if (InnerOutputNodePin->Direction == EEdGraphPinDirection::EGPD_Input
											&& InnerOutputNodePin->PinName == TracedPin->PinName)
										{
											TArray<UNiagaraNodeInput*> InnerInputNodes;
											FindDependenciesInternal(FModuleScopedPin(InnerOutputNodePin, AggregateModuleName), Dependencies, &InnerInputNodes);

											// map the InputNodes we found to the input pins on the FunctionCallNode
											for (const UNiagaraNodeInput* InnerInputNode : InnerInputNodes)
											{
												for (UEdGraphPin* NodePin : FunctionCallNode->GetAllPins())
												{
													if (NodePin->Direction == EEdGraphPinDirection::EGPD_Input
														&& NodePin->PinName == InnerInputNode->Input.GetName())
													{
														PinsToEvaluate.Emplace(NodePin, CurrentPin.ModuleName);
														break;
													}
												}
											}
										}
									}
								}

								// skip adding the inputs to the list of pins we need to evaluate because we've already search through the internal graph
								// to figure out the minimal set of items we care about
								AlreadyEvaluated = true;
							}
						}
						else if (UNiagaraNodeInput* InputNode = Cast<UNiagaraNodeInput>(CurrentNode))
						{
							if (InputNodes)
							{
								InputNodes->AddUnique(InputNode);
							}
							else
							{
								const FModuleScopedPin ResolvedPin = InputResolver.ResolveInput(FModuleScopedPin(TracedPin, CurrentPin.ModuleName));
								if (ResolvedPin.Pin)
								{
									PinsToEvaluate.Add(ResolvedPin);
									EvaluatedNodes.Add(InputNode);
									AlreadyEvaluated = true;
								}
							}
						}

						if (!AlreadyEvaluated)
						{
							EvaluatedNodes.Add(CurrentNode, &AlreadyEvaluated);
						}

						if (!AlreadyEvaluated)
						{
							for (UEdGraphPin* NodePin : CurrentNode->GetAllPins())
							{
								if (NodePin->Direction == EEdGraphPinDirection::EGPD_Output)
								{
									continue;
								}

								// we want to exclude execution pins
								const bool IsExecPin = NodePin->PinName == UNiagaraNodeParameterMapBase::SourcePinName;
								if (IsExecPin)
								{
									continue;
								}

								PinsToEvaluate.Emplace(NodePin, CurrentPin.ModuleName);
							}

							if (const UNiagaraNodeCustomHlsl* CustomHlslNode = Cast<const UNiagaraNodeCustomHlsl>(CurrentNode))
							{
								if (!Dependencies.CustomNodes.Contains(CustomHlslNode))
								{
									FCustomHlslNodeInfo& NodeInfo = Dependencies.CustomNodes.Add(CustomHlslNode);
									BuildCustomHlslNodeInfo(CustomHlslNode, NodeInfo);
								}
							}
						}
					}
				}

				if (PinsToEvaluate.Num())
				{
					CurrentPin = PinsToEvaluate.Pop(false);
				}
				else
				{
					break;
				}
			}
		}

		void BuildCustomHlslNodeInfo(const UNiagaraNodeCustomHlsl* CustomNode, FCustomHlslNodeInfo& NodeInfo)
		{
			check(CustomNode);

			NodeInfo.bHasDataInterfaceInputs = false;
			NodeInfo.bHasImpureFunctionText = false;

			TArray<FString> ImpureFunctionNames;

			FPinCollectorArray InputPins;
			CustomNode->GetInputPins(InputPins);
			for (const UEdGraphPin* InputPin : InputPins)
			{
				FNiagaraTypeDefinition NiagaraType = NiagaraSchema->PinToTypeDefinition(InputPin);
				if (NiagaraType.IsDataInterface())
				{
					NodeInfo.bHasDataInterfaceInputs = true;

					if (UNiagaraDataInterface* DataInterfaceClass = CastChecked<UNiagaraDataInterface>(NiagaraType.GetClass()->ClassDefaultObject))
					{
						TArray<FNiagaraFunctionSignature> FunctionSignatures;
						DataInterfaceClass->GetFunctions(FunctionSignatures);

						for (const FNiagaraFunctionSignature& FunctionSignature : FunctionSignatures)
						{
							if (FunctionSignature.bRequiresExecPin)
							{
								TStringBuilder<256> Builder;
								InputPin->GetFName().AppendString(Builder);
								Builder.AppendChar(TCHAR('.'));
								FunctionSignature.Name.AppendString(Builder);

								ImpureFunctionNames.AddUnique(Builder.ToString());
							}
						}
					}
				}
			}

			if (!ImpureFunctionNames.IsEmpty())
			{
				TArray<FString> StringTokens;
				UNiagaraNodeCustomHlsl::GetTokensFromString(CustomNode->GetCustomHlsl(), StringTokens, false, false);

				for (const FString& FunctionCall : ImpureFunctionNames)
				{
					for (const FString& Token : StringTokens)
					{
						if (Token.Equals(FunctionCall))
						{
							NodeInfo.bHasImpureFunctionText = true;
							return;
						}
					}
				}
			}
		}

		const FNiagaraParameterMapHistory& ParamMap;
		const FFunctionInputResolver& InputResolver;
		const UEdGraphSchema_Niagara* NiagaraSchema = nullptr;

		const bool EvaluateStaticSwitches = false;
	};

	// For a specific read of a variable finds the corresponding PreviousWritePin if one exists (only considers actual writes
	// rather than default pins on a MapGet)
	static FModuleScopedPin FindWriteForRead(const FNiagaraParameterMapHistory& ParamMap, const FModuleScopedPin& ReadPin, int32* OutVariableIndex)
	{
		if (ReadPin.Pin == nullptr)
		{
			return FModuleScopedPin();
		}

		int32 VariableCount = ParamMap.Variables.Num();

		// without more context we need to do a full linear search through the ReadHistory
		for (int32 VariableIt = 0; VariableIt < VariableCount; ++VariableIt)
		{
			const TArray<FNiagaraParameterMapHistory::FReadHistory>& ReadHistoryEntries = ParamMap.PerVariableReadHistory[VariableIt];

			for (const FNiagaraParameterMapHistory::FReadHistory& ReadHistoryEntry : ReadHistoryEntries)
			{
				if (ReadHistoryEntry.ReadPin == ReadPin)
				{
					if (OutVariableIndex)
					{
						*OutVariableIndex = VariableIt;
					}

					if (ReadHistoryEntry.PreviousWritePin.Pin)
					{
						// we only care about writes (MapSets).  This will exclude default variables that are set as the input to the MapGet
						if (const UNiagaraNodeParameterMapSet* WriteNode = Cast<const UNiagaraNodeParameterMapSet>(ReadHistoryEntry.PreviousWritePin.Pin->GetOwningNode()))
						{
							return ReadHistoryEntry.PreviousWritePin;
						}
					}

					return FModuleScopedPin();
				}
			}
		}

		return FModuleScopedPin();
	}

	static void AddDefaultBinding(const FNiagaraParameterMapHistory& ParamMap, int32 VariableIndex, const FModuleScopedPin& ReadPin, FDependencyChain& ResolvedDependencies)
	{
		if (const UNiagaraGraph* OwnerGraph = Cast<const UNiagaraGraph>(ReadPin.Pin->GetOwningNode()->GetGraph()))
		{
			FNiagaraVariable ParamVariable = ParamMap.Variables[VariableIndex];
			FString ParamName = ParamVariable.GetName().ToString();

			// now we need to replace the module name with 'module'
			ParamName.ReplaceInline(*ReadPin.ModuleName.ToString(), *FNiagaraConstants::ModuleNamespace.ToString());

			ParamVariable.SetName(*ParamName);

			FNiagaraScriptVariableBinding DefaultBinding;
			TOptional<ENiagaraDefaultMode> DefaultMode = OwnerGraph->GetDefaultMode(ParamVariable, &DefaultBinding);
			if (DefaultMode.IsSet() && *DefaultMode == ENiagaraDefaultMode::Binding && DefaultBinding.IsValid())
			{
				int32 BoundVariableIndex = ParamMap.FindVariableByName(DefaultBinding.GetName());

				if (BoundVariableIndex != INDEX_NONE)
				{
					for (const FNiagaraParameterMapHistory::FReadHistory& ReadHistoryEntry : ParamMap.PerVariableReadHistory[BoundVariableIndex])
					{
						// for now adding the first pin to the dependencies works, but this needs to be revisited
						ResolvedDependencies.Pins.Add(ReadHistoryEntry.ReadPin);
						break;
					}
				}
			}
		}
	}

	// for a specific read of a variable finds the actual name of the attribute being read
	static FName FindAttributeForRead(const FNiagaraParameterMapHistory& ParamMap, const FModuleScopedPin& ReadPin)
	{
		const int32 VariableCount = ParamMap.Variables.Num();

		// wihtout more context we need to do a full linear search through the ReadHistory
		for (int32 VariableIt = 0; VariableIt < VariableCount; ++VariableIt)
		{
			const TArray<FNiagaraParameterMapHistory::FReadHistory>& ReadHistoryEntries = ParamMap.PerVariableReadHistory[VariableIt];

			for (const FNiagaraParameterMapHistory::FReadHistory& ReadHistoryEntry : ReadHistoryEntries)
			{
				if (ReadHistoryEntry.ReadPin == ReadPin)
				{
					return ParamMap.Variables[VariableIt].GetName();
				}
			}
		}

		return NAME_None;
	}

	// given the set of expressions (as defined in FindDependencies above) we resolve the named attribute aggregating the dependent reads and custom nodes
	static void ResolveDependencyChain(const FNiagaraParameterMapHistory& ParamMap, const FDependencyMap& DependencyData, const FName& AttributeName, FDependencyChain& ResolvedDependencies)
	{
		const int32 VariableIndex = ParamMap.FindVariableByName(AttributeName);
		if (VariableIndex != INDEX_NONE)
		{
			if (ParamMap.PerVariableWriteHistory[VariableIndex].Num())
			{
				const FModuleScopedPin& LastWritePin = ParamMap.PerVariableWriteHistory[VariableIndex].Last();

				TArray<FModuleScopedPin> PinsToResolve;
				PinsToResolve.Add(LastWritePin);

				while (PinsToResolve.Num() > 0)
				{
					const FModuleScopedPin PinToResolve = PinsToResolve.Pop(false);

					if (const FDependencyChain* Dependencies = DependencyData.Find(PinToResolve))
					{
						for (const FModuleScopedPin& ReadPin : Dependencies->Pins)
						{
							bool AlreadyAdded = false;
							ResolvedDependencies.Pins.Add(ReadPin, &AlreadyAdded);

							if (!AlreadyAdded)
							{
								int32 SourceVariableIndex = INDEX_NONE;
								const FModuleScopedPin WritePin = FindWriteForRead(ParamMap, ReadPin, &SourceVariableIndex);

								if (WritePin.Pin)
								{
									PinsToResolve.Add(WritePin);
								}
								else if (SourceVariableIndex != INDEX_NONE)
								{
									AddDefaultBinding(ParamMap, SourceVariableIndex, ReadPin, ResolvedDependencies);
								}
							}
						}

						ResolvedDependencies.CustomNodes.Append(Dependencies->CustomNodes);
					}
				}
			}
		}
	}
}

static void TrimAttributes_Safe(TConstArrayView<const FNiagaraParameterMapHistory*> LocalParamHistories, TSet<FName>& AttributesToPreserve, TArray<FNiagaraVariable>& Attributes)
{
	// go through the ParamMapHistories and collect any CustomHlsl nodes so that we can give a best effort to
	// find references to our variable.  If a reference is found we'll assume that we can't trim the attribute
	TArray<const UNiagaraNodeCustomHlsl*, TInlineAllocator<8>> CustomHlslNodes;
	for (const FNiagaraParameterMapHistory* ParamMap : LocalParamHistories)
	{
		for (const UEdGraphPin* Pin : ParamMap->MapPinHistory)
		{
			if (const UNiagaraNodeCustomHlsl* CustomHlslNode = Cast<const UNiagaraNodeCustomHlsl>(Pin->GetOwningNode()))
			{
				CustomHlslNodes.AddUnique(CustomHlslNode);
			}
		}
	}

	Attributes.SetNum(Algo::StableRemoveIf(Attributes, [=](const FNiagaraVariable& Var)
	{
		// preserve attributes which have a record of being read
		for (const FNiagaraParameterMapHistory* ParamMap : LocalParamHistories)
		{
			const int32 VarIdx = ParamMap->FindVariable(Var.GetName(), Var.GetType());
			if (VarIdx != INDEX_NONE)
			{
				check(ParamMap->PerVariableReadHistory.IsValidIndex(VarIdx));
				if (ParamMap->PerVariableReadHistory[VarIdx].Num())
				{
					return false;
				}
			}
		}

		if (AttributesToPreserve.Contains(Var.GetName()))
		{
			return false;
		}

		for (const UNiagaraNodeCustomHlsl* CustomHlslNode : CustomHlslNodes)
		{
			if (CustomHlslNode->ReferencesVariable(Var))
			{
				return false;
			}
		}

		return true;
	}));
}

static void TrimAttributes_Aggressive(const FNiagaraCompileRequestDuplicateData* CompileDuplicateData, TConstArrayView<const FNiagaraParameterMapHistory*> LocalParamHistories, TSet<FName>& AttributesToPreserve, TArray<FNiagaraVariable>& Attributes)
{
	// variable references hidden in custom hlsl nodes may not be present in a specific stages parameter map
	// so we consolidate all the variables into one list to use when going through custom hlsl nodes
	TSet<FNiagaraVariableBase> UnifiedVariables;

	for (const FNiagaraParameterMapHistory* ParamMap : LocalParamHistories)
	{
		for (const FNiagaraVariableBase& Variable : ParamMap->Variables)
		{
			UnifiedVariables.Add(Variable);
		}
	}

	using namespace NiagaraAttributeTrimming;

	FDependencyMap PerVariableDependencySets;

	const UEdGraphSchema_Niagara* NiagaraSchema = GetDefault<UEdGraphSchema_Niagara>();

	for (const FNiagaraParameterMapHistory* ParamMap : LocalParamHistories)
	{
		FFunctionInputResolver InputResolver(CompileDuplicateData->NodeGraphDeepCopy.Get(), ParamMap->OriginatingScriptUsage);

		const int32 VariableCount = ParamMap->Variables.Num();

		for (int32 VariableIt = 0; VariableIt < VariableCount; ++VariableIt)
		{
			const FNiagaraVariableBase& Var = ParamMap->Variables[VariableIt];

			for (const FModuleScopedPin& WritePin : ParamMap->PerVariableWriteHistory[VariableIt])
			{
				FDependencyChain& Dependencies = PerVariableDependencySets.Add(WritePin);

				FExpressionBuilder Builder(*ParamMap, InputResolver, NiagaraSchema);
				Builder.FindDependencies(WritePin, Dependencies);

				for (FCustomHlslNodeMap::TConstIterator CustomNodeIt = Dependencies.CustomNodes.CreateConstIterator(); CustomNodeIt; ++CustomNodeIt)
				{
					if (CustomNodeIt.Value().bHasImpureFunctionText)
					{
						AttributesToPreserve.Add(Var.GetName());
					}
				}
			}
		}

		FImpureFunctionParser ImpureFunctionParser(CompileDuplicateData->NodeGraphDeepCopy.Get(), ParamMap->OriginatingScriptUsage);
		for (const FModuleScopedPin& RequiredFunctionInput : ImpureFunctionParser.ReadFunctionInputs())
		{
			FDependencyChain Dependencies;

			FExpressionBuilder Builder(*ParamMap, InputResolver, NiagaraSchema);
			Builder.FindDependencies(RequiredFunctionInput, Dependencies);

			for (const FModuleScopedPin& DependentPin : Dependencies.Pins)
			{
				// see if this variable corresponds to something in our parameter map's list of variables
				// if it does then we need to mark it as something that needs to be preserved
				// Note that we need to resolve the Pin name's module namespace before we actually search for it
				// as a variable
				FString VariableNameString = DependentPin.Pin->GetName();
				VariableNameString.ReplaceInline(*DependentPin.ModuleName.ToString(), *FNiagaraConstants::ModuleNamespace.ToString());

				const FName VariableName = *VariableNameString;

				if (VariableNameString.StartsWith(FNiagaraConstants::StackContextNamespaceString))
				{
					const int32 AliasedIndex = ParamMap->VariablesWithOriginalAliasesIntact.IndexOfByPredicate([&](const FNiagaraVariable& Variable)
					{
						return Variable.GetName() == VariableName;
					});

					if (ParamMap->Variables.IsValidIndex(AliasedIndex))
					{
						AttributesToPreserve.Add(ParamMap->Variables[AliasedIndex].GetName());
					}
				}
				else
				if (ParamMap->FindVariableByName(VariableName) != INDEX_NONE)
				{
					AttributesToPreserve.Add(VariableName);
				}
			}
		}
	}

	TArray<FName> SearchList = AttributesToPreserve.Array();

	auto ConditionalAddAttribute = [&](const FName AttributeName)
	{
		bool AlreadyAdded = false;
		AttributesToPreserve.Add(AttributeName, &AlreadyAdded);
		if (!AlreadyAdded)
		{
			SearchList.Add(AttributeName);
		}
	};

	while (SearchList.Num())
	{
		const FName AttributeName = SearchList.Pop(false);

		for (const FNiagaraParameterMapHistory* ParamMap : LocalParamHistories)
		{
			FDependencyChain ResolvedChain;
			ResolveDependencyChain(*ParamMap, PerVariableDependencySets, AttributeName, ResolvedChain);

			for (const FModuleScopedPin ParameterRead : ResolvedChain.Pins)
			{
				const FName DependentAttributeName = FindAttributeForRead(*ParamMap, ParameterRead);

				if (DependentAttributeName != NAME_None)
				{
					// for each of the reads we are dependent on, check to see if we have a write associated with it
					// if we don't then it's value will be coming from the previous frame/stage and so we need to preserve
					// the attribute.
					if (!FindWriteForRead(*ParamMap, ParameterRead, nullptr).Pin)
					{
						ConditionalAddAttribute(DependentAttributeName);
					}

					// if we're dependent on a Previous value, then make sure we add the attribute to the list of things to be preserved as well
					if (FNiagaraParameterMapHistory::IsPreviousName(DependentAttributeName))
					{
						ConditionalAddAttribute(FNiagaraParameterMapHistory::GetSourceForPreviousValue(DependentAttributeName));
					}

					// likewise for Initial value
					if (FNiagaraParameterMapHistory::IsInitialName(DependentAttributeName))
					{
						ConditionalAddAttribute(FNiagaraParameterMapHistory::GetSourceForInitialValue(DependentAttributeName));
					}
				}
			}

			for (const auto& CustomHlslInfo : ResolvedChain.CustomNodes)
			{
				// go through all of the variables in the attributes to see if they are referenced by any encountered CustomNodes
				for (const FNiagaraVariableBase& Variable : UnifiedVariables)
				{
					// skip searching through the hlsl code if we're already included
					if (AttributesToPreserve.Find(Variable.GetName()))
					{
						continue;
					}

					if (CustomHlslInfo.Key->ReferencesVariable(Variable))
					{
						ConditionalAddAttribute(Variable.GetName());
					}
				}
			}
		}
	}

	auto SafeToRemove = [&](const FNiagaraVariable& Var)
	{
		return !AttributesToPreserve.Contains(Var.GetName());
	};

	int32 AttributeIt = 0;
	while (AttributeIt < Attributes.Num())
	{
		const FNiagaraVariableBase& Attribute = Attributes[AttributeIt];

		if (!AttributesToPreserve.Contains(Attribute.GetName()))
		{
			// when we have something to do with this information, we can re-enable the compile tag
			//TranslateResults.CompileTags.Emplace(Attribute, TEXT("Trimmed"));
			Attributes.RemoveAt(AttributeIt, 1, false);
		}
		else
		{
			++AttributeIt;
		}
	}
}

void FHlslNiagaraTranslator::TrimAttributes(const FNiagaraCompileOptions& InCompileOptions, TArray<FNiagaraVariable>& Attributes)
{
	if (!UNiagaraScript::IsParticleScript(InCompileOptions.TargetUsage))
	{
		return;
	}

	const bool SafeTrimAttributesEnabled = InCompileOptions.AdditionalDefines.Contains(TEXT("TrimAttributesSafe"));
	const bool AggressiveTrimAttributesEnabled = InCompileOptions.AdditionalDefines.Contains(TEXT("TrimAttributes"));

	if (SafeTrimAttributesEnabled || AggressiveTrimAttributesEnabled)
	{
		// validate that the attributes have unique sanitized names
		{
			bool HasOverlappingNames = false;

			TMap<FString, FNiagaraVariableBase> SanitizedNames;
			for (const FNiagaraVariableBase& Attribute : Attributes)
			{
				const FString SanitizedName = GetSanitizedSymbolName(Attribute.GetName().ToString());
				if (const FNiagaraVariableBase* ExistingVariable = SanitizedNames.Find(SanitizedName))
				{
					HasOverlappingNames = true;
				}
				else
				{
					SanitizedNames.Add(SanitizedName, Attribute);
				}
			}

			// the trimming algorithm doesn't work when names are overlapping, so just early out of the function
			if (HasOverlappingNames)
			{
				return;
			}
		}

		const bool bRequiresPersistentIDs = InCompileOptions.AdditionalDefines.Contains(TEXT("RequiresPersistentIDs"));

		// we want to use the ParamMapHistories of both the particle update and spawn scripts because they need to
		// agree to define a unified attribute set
		TArray<const FNiagaraParameterMapHistory*, TInlineAllocator<2>> LocalParamHistories;
		for (const FNiagaraParameterMapHistory& History : OtherOutputParamMapHistories)
		{
			if (UNiagaraScript::IsParticleScript(History.OriginatingScriptUsage))
			{
				// for now we'll be disabling attribute trimming if a family of particle scripts contain generation of
				// additional dataset writes (events) as we don't have access to the connectivity of it's variables as
				// we do for the rest of the script
				if (History.AdditionalDataSetWrites.Num())
				{
					return;
				}

				LocalParamHistories.Add(&History);
			}
		}

		// check through the AdditionalDefines to see if any variables have been explicitly preserved
		TSet<FName> AttributesToPreserve;

		for (const FString& AdditionalDefine : InCompileOptions.AdditionalDefines)
		{
			const FString PreserveTag = TEXT("PreserveAttribute=");
			if (AdditionalDefine.StartsWith(PreserveTag))
			{
				AttributesToPreserve.Add(*AdditionalDefine.RightChop(PreserveTag.Len()));
			}
		}

		AttributesToPreserve.Add(SYS_PARAM_INSTANCE_ALIVE.GetName());
		AttributesToPreserve.Add(SYS_PARAM_PARTICLES_UNIQUE_ID.GetName());
		if (bRequiresPersistentIDs)
		{
			AttributesToPreserve.Add(SYS_PARAM_PARTICLES_ID.GetName());
		}

		const TArray<FNiagaraVariable> PreTrimmedAttributes = Attributes;

		if (SafeTrimAttributesEnabled)
		{
			TrimAttributes_Safe(LocalParamHistories, AttributesToPreserve, Attributes);
		}
		else if (AggressiveTrimAttributesEnabled)
		{
			TrimAttributes_Aggressive(CompileDuplicateData, LocalParamHistories, AttributesToPreserve, Attributes);
		}

		for (const FNiagaraVariable& Attribute : PreTrimmedAttributes)
		{
			if (!Attributes.Contains(Attribute))
			{
				TranslateResults.CompileTags.Emplace(Attribute, TEXT("Trimmed"));
			}
		}
	}
}