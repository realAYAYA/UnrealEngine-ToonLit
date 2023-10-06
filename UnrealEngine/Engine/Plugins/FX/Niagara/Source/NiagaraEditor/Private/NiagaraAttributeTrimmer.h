// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraCommon.h"

template<typename GraphBridge>
class FNiagaraAttributeTrimmerHelper : public GraphBridge
{
	using typename GraphBridge::FGraph;
	using typename GraphBridge::FPin;
	using typename GraphBridge::FCustomHlslNode;
	using typename GraphBridge::FNode;
	using typename GraphBridge::FOutputNode;
	using typename GraphBridge::FInputNode;
	using typename GraphBridge::FFunctionCallNode;
	using typename GraphBridge::FParamMapGetNode;
	using typename GraphBridge::FParamMapSetNode;
	using typename GraphBridge::FInputPin;
	using typename GraphBridge::FOutputPin;
	using typename GraphBridge::FParamMapHistory;
	using typename GraphBridge::FCompilationCopy;
	using typename GraphBridge::FModuleScopedPin;

	using FDependencySet = TSet<FModuleScopedPin>;
	struct FCustomHlslNodeInfo
	{
		bool bHasDataInterfaceInputs = false;
		bool bHasImpureFunctionText = false;
	};

	using FCustomHlslNodeMap = TMap<const FCustomHlslNode*, FCustomHlslNodeInfo>;

	struct FDependencyChain
	{
		FDependencySet Pins;
		FCustomHlslNodeMap CustomNodes;
	};

	using FDependencyMap = TMap<FModuleScopedPin, FDependencyChain>;

	class FFunctionInputResolver;
	class FImpureFunctionParser;
	class FExpressionBuilder;

	// For a specific read of a variable finds the corresponding PreviousWritePin if one exists (only considers actual writes
	// rather than default pins on a MapGet)
	static FModuleScopedPin FindWriteForRead(const FParamMapHistory& ParamMap, const FModuleScopedPin& ReadPin, int32* OutVariableIndex);
	static int32 FindDefaultBinding(const FParamMapHistory& ParamMap, int32 VariableIndex, const FModuleScopedPin& ReadPin);

	// for a specific read of a variable finds the actual name of the attribute being read
	static FName FindAttributeForRead(const FParamMapHistory& ParamMap, const FModuleScopedPin& ReadPin);

	// given the set of expressions (as defined in FindDependencies above) we resolve the named attribute aggregating the dependent reads and custom nodes
	static void ResolveDependencyChain(const FParamMapHistory& ParamMap, const FDependencyMap& DependencyData, const FName& AttributeName, FDependencyChain& ResolvedDependencies);

public:
	static void TrimAttributes_Safe(TConstArrayView<const FParamMapHistory*> LocalParamHistories, TSet<FName>& AttributesToPreserve, TArray<FNiagaraVariable>& Attributes);
	static void TrimAttributes_Aggressive(const FCompilationCopy* CompileDuplicateData, TConstArrayView<const FParamMapHistory*> LocalParamHistories, TSet<FName>& AttributesToPreserve, TArray<FNiagaraVariable>& Attributes);
};


