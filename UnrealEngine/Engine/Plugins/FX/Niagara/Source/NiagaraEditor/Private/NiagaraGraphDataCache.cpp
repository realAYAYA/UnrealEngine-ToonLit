// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraGraphDataCache.h"

#include "NiagaraNodeFunctionCall.h"

static int32 GNiagaraGraphDataCacheSize = 16384;
static FAutoConsoleVariableRef CVarNiagaraGraphDataCacheSize(
	TEXT("fx.Niagara.GraphDataCacheSize"),
	GNiagaraGraphDataCacheSize,
	TEXT("Maximum number of elements to store within the GraphDataCache."),
	ECVF_ReadOnly
);

static bool GNiagaraGraphDataCacheValidation = false;
static FAutoConsoleVariableRef CVarNiagaraGraphDataCacheValidation(
	TEXT("fx.Niagara.GraphDataCacheValidation"),
	GNiagaraGraphDataCacheValidation,
	TEXT("If true will perform validation on retrieving data from the data FNiagaraGraphDataCache."),
	ECVF_Default
);

FNiagaraGraphDataCache::FNiagaraGraphDataCache()
	: StackFunctionInputPinCache(GNiagaraGraphDataCacheSize)
{
}

void FNiagaraGraphDataCache::GetStackFunctionInputPinsInternal(
	UNiagaraNodeFunctionCall& FunctionCallNode,
	const UNiagaraGraph* CalledGraph,
	TConstArrayView<FNiagaraVariable> StaticVars,
	TArray<const UEdGraphPin*>& OutInputPins,
	FCompileConstantResolver ConstantResolver,
	FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options,
	bool bIgnoreDisabled,
	bool bFilterForCompilation)
{
	// we don't currently support hashing the internal details of the translator within FStackFunctionInputPinKey so in the case
	// we've been supplied a translator we'll just fall back to not using the cache
	if (const FHlslNiagaraTranslator* Translator = ConstantResolver.GetTranslator())
	{
		FNiagaraStackGraphUtilities::GetStackFunctionInputPinsWithoutCache(
			FunctionCallNode,
			StaticVars,
			OutInputPins,
			ConstantResolver,
			Options,
			bIgnoreDisabled,
			bFilterForCompilation);

		return;
	}

	FStackFunctionInputPinKey CacheKey(FunctionCallNode, CalledGraph, StaticVars, ConstantResolver, Options, bIgnoreDisabled, bFilterForCompilation);

	bool FoundInCache = false;

	if (const FStackFunctionInputPinValue* CachedValue = StackFunctionInputPinCache.FindAndTouch(CacheKey))
	{
		FoundInCache = true;
		OutInputPins = CachedValue->InputPins;
	}
	else
	{
		FStackFunctionInputPinValue NewValue;
		FNiagaraStackGraphUtilities::GetStackFunctionInputPinsWithoutCache(
			FunctionCallNode,
			StaticVars,
			NewValue.InputPins,
			ConstantResolver,
			Options,
			bIgnoreDisabled,
			bFilterForCompilation);

		OutInputPins = NewValue.InputPins;

		StackFunctionInputPinCache.Add(CacheKey, NewValue);
	}

	if (GNiagaraGraphDataCacheValidation)
	{
		TArray<const UEdGraphPin*> ValidationPins;
		FNiagaraStackGraphUtilities::GetStackFunctionInputPinsWithoutCache(
			FunctionCallNode,
			StaticVars,
			ValidationPins,
			ConstantResolver,
			Options,
			bIgnoreDisabled,
			bFilterForCompilation);

		if (!ensureMsgf(ValidationPins == OutInputPins, TEXT("FNiagaraGraphDataCache failed to accurately collect StackFunctionInputPins for Function {0}"), *FunctionCallNode.GetPathName()))
		{
			OutInputPins = ValidationPins;
		}
	}
}

void FNiagaraGraphDataCache::GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TConstArrayView<FNiagaraVariable> StaticVars, TArray<const UEdGraphPin*>& OutInputPins, FCompileConstantResolver ConstantResolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options, bool bIgnoreDisabled)
{
	if (const UNiagaraGraph* CalledGraph = FunctionCallNode.GetCalledGraph())
	{
		GetStackFunctionInputPinsInternal(FunctionCallNode, CalledGraph, StaticVars, OutInputPins, ConstantResolver, Options, bIgnoreDisabled, false /*bFilterForCompilation*/);
	}
}

void FNiagaraGraphDataCache::GetStackFunctionInputPins(UNiagaraNodeFunctionCall& FunctionCallNode, TConstArrayView<FNiagaraVariable> StaticVars, TArray<const UEdGraphPin*>& OutInputPins, TSet<const UEdGraphPin*>& OutHiddenPins, FCompileConstantResolver ConstantResolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions Options, bool bIgnoreDisabled)
{
	if (const UNiagaraGraph* CalledGraph = FunctionCallNode.GetCalledGraph())
	{
		TArray<const UEdGraphPin*> FilteredPins;

		GetStackFunctionInputPinsInternal(FunctionCallNode, CalledGraph, StaticVars, OutInputPins, ConstantResolver, Options, bIgnoreDisabled, false /*bFilterForCompilation*/);
		GetStackFunctionInputPinsInternal(FunctionCallNode, CalledGraph, StaticVars, FilteredPins, ConstantResolver, Options, bIgnoreDisabled, true /*bFilterForCompilation*/);

		// generate hidden pins
		auto PinsMatch = [](const UEdGraphPin* Lhs, const UEdGraphPin* Rhs) -> bool
		{
			return Lhs->GetFName() == Rhs->GetFName()
				&& Lhs->PinType.PinCategory == Rhs->PinType.PinCategory
				&& Lhs->PinType.PinSubCategoryObject == Rhs->PinType.PinSubCategoryObject;
		};

		for (const UEdGraphPin* InputPin : OutInputPins)
		{
			if (!FilteredPins.ContainsByPredicate([&](const UEdGraphPin* CompilationPin) { return PinsMatch(InputPin, CompilationPin); }))
			{
				OutHiddenPins.Add(InputPin);
			}
		}
	}
}

FNiagaraGraphDataCache::FStackFunctionInputPinKey::FStackFunctionInputPinKey(const UNiagaraNodeFunctionCall& FunctionCallNode, const UNiagaraGraph* CalledGraph, TConstArrayView<FNiagaraVariable> StaticVariables, const FCompileConstantResolver& InConstantResolver, FNiagaraStackGraphUtilities::ENiagaraGetStackFunctionInputPinsOptions InOptions, bool bInIgnoreDisabled, bool bInFilterForCompilation)
{
	if (ensure(CalledGraph))
	{
		FunctionCallGraphKey = FObjectKey(CalledGraph);
		FunctionCallGraphChangeId = CalledGraph->GetChangeID();
		ContextStaticVariables = StaticVariables;
	}

	InputPinOption = (uint8)InOptions;
	bIgnoreDisabled = bInIgnoreDisabled;
	bFilterForCompilation = bInFilterForCompilation;
	ConstantResolverHash = InConstantResolver.BuildTypeHash();

	Hash = GetTypeHash(FunctionCallGraphKey);
	Hash = HashCombine(Hash, GetTypeHash(FunctionCallGraphChangeId));
	for (const FNiagaraVariable& StaticVariable : ContextStaticVariables)
	{
		Hash = HashCombine(Hash, GetTypeHash(StaticVariable));
		const uint8* StaticVariableData = StaticVariable.GetData();
		const int32 ByteCount = StaticVariable.GetAllocatedSizeInBytes();
		for (int32 ByteIt = 0; ByteIt < ByteCount; ++ByteIt)
		{
			Hash = HashCombine(Hash, GetTypeHash(StaticVariableData[ByteIt]));
		}
	}
	Hash = HashCombine(Hash, ConstantResolverHash);
	Hash = HashCombine(Hash, InputPinOption);
	Hash = HashCombine(Hash, bIgnoreDisabled);
	Hash = HashCombine(Hash, bFilterForCompilation);

	// the default values of the input pins can be relevant when it comes to static switch values defined within the graph
	FPinCollectorArray InputPins;
	FunctionCallNode.GetInputPins(InputPins);
	for (const UEdGraphPin* InputPin : InputPins)
	{
		Hash = HashCombine(Hash, GetTypeHash(InputPin->PinName));
		Hash = HashCombine(Hash, GetTypeHash(InputPin->DefaultValue));
	}
}

bool FNiagaraGraphDataCache::FStackFunctionInputPinKey::operator==(const FStackFunctionInputPinKey& Other) const
{
	return FunctionCallGraphKey == Other.FunctionCallGraphKey
		&& FunctionCallGraphChangeId == Other.FunctionCallGraphChangeId
		&& ContextStaticVariables == Other.ContextStaticVariables
		&& ConstantResolverHash == Other.ConstantResolverHash
		&& InputPinOption == Other.InputPinOption
		&& bIgnoreDisabled == Other.bIgnoreDisabled
		&& bFilterForCompilation == Other.bFilterForCompilation;
}
