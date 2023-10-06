// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"

class FNiagaraCompilationGraph;
class FNiagaraCompilationNode;
class FNiagaraCompilationNodeEmitter;
class FNiagaraCompilationNodeFunctionCall;
class FNiagaraFixedConstantResolver;

struct FNiagaraTraversalStackEntry
{
	FGuid NodeGuid;
	uint32 FullStackHash;
	FString FriendlyName;
};

struct FNiagaraTraversalStateContext
{
	void PushFunction(const FNiagaraCompilationNodeFunctionCall* FunctionCall, const FNiagaraFixedConstantResolver& ConstantResolver);
	void PopFunction(const FNiagaraCompilationNodeFunctionCall* FunctionCall);

	void PushEmitter(const FNiagaraCompilationNodeEmitter* Emitter);
	void PopEmitter(const FNiagaraCompilationNodeEmitter* Emitter);

	bool GetStaticSwitchValue(const FGuid& NodeGuid, int32& StaticSwitchValue) const;
	bool GetFunctionDefaultValue(const FGuid& NodeGuid, FName PinName, FString& FunctionDefaultValue) const;
	bool GetFunctionDebugState(const FGuid& NodeGuid, ENiagaraFunctionDebugState& DebugState) const;

	TArray<FNiagaraTraversalStackEntry> TraversalStack;

	TMap<uint32, int32> StaticSwitchValueMap;
	TMap<uint32, FString> FunctionDefaultValueMap;
	TMap<uint32, ENiagaraFunctionDebugState> FunctionDebugStateMap;

protected:
	void PushGraphInternal(const FNiagaraCompilationNode* CallingNode, const FNiagaraCompilationGraph* Graph, const FNiagaraFixedConstantResolver& ConstantResolver);
};
