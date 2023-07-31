// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/LruCache.h"
#include "NiagaraCommon.h"
#include "NiagaraEditorUtilities.h"
#include "UObject/ObjectKey.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"

class FHlslNiagaraTranslator;
class UEdGraphPin;
class UNiagaraGraph;
class UNiagaraNodeFunctionCall;

class FNiagaraGraphDataCache
{
public:
	FNiagaraGraphDataCache();

	void GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TConstArrayView<FNiagaraVariable> StaticVars, TArray<const UEdGraphPin*>& OutInputPins, FCompileConstantResolver ConstantResolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options, bool bIgnoreDisabled);
	void GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TConstArrayView<FNiagaraVariable> StaticVars, TArray<const UEdGraphPin*>& OutInputPins, TSet<const UEdGraphPin*>& OutHiddenPins, FCompileConstantResolver ConstantResolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options, bool bIgnoreDisabled);

private:
	void GetStackFunctionInputPinsInternal(
		UNiagaraNodeFunctionCall& FunctionCallNode,
		const UNiagaraGraph* CalledGraph,
		TConstArrayView<FNiagaraVariable> StaticVars,
		TArray<const UEdGraphPin*>& OutInputPins,
		FCompileConstantResolver ConstantResolver,
		FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options,
		bool bIgnoreDisabled,
		bool bFilterForCompilation);

	struct FStackFunctionInputPinKey
	{
		FStackFunctionInputPinKey(const UNiagaraNodeFunctionCall& FunctionCallNode, const UNiagaraGraph* CalledGraph, TConstArrayView<FNiagaraVariable> StaticVazriables, const FCompileConstantResolver& InConstantResolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions InOptions, bool bInIgnoreDisabled, bool bInFilterForCompilation);
		bool operator==(const FStackFunctionInputPinKey& Other) const;

		friend uint32 GetTypeHash(const FStackFunctionInputPinKey& Key)
		{
			return Key.Hash;
		}

		const FGuid& GetGraphChangeId() const
		{
			return FunctionCallGraphChangeId;
		}

	private:
		FObjectKey FunctionCallGraphKey;
		FGuid FunctionCallGraphChangeId;
		TArray<FNiagaraVariable> ContextStaticVariables;
		uint32 ConstantResolverHash = 0;
		uint8 InputPinOption = 0;
		bool bIgnoreDisabled = false;
		bool bFilterForCompilation = false;

		int32 Hash = 0;
	};

	struct FStackFunctionInputPinValue
	{
		TArray<const UEdGraphPin*> InputPins;
	};

	// cache holding a finite number (specific value pulled from a cvar) of results to previous calls
	// to retrieve the input pins
	TLruCache<FStackFunctionInputPinKey, FStackFunctionInputPinValue> StackFunctionInputPinCache;
};