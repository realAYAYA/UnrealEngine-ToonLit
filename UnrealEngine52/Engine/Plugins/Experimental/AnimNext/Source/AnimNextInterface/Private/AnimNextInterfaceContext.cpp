// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextInterfaceContext.h"
#include "IAnimNextInterface.h"
#include "IAnimNextInterfaceParameters.h"
#include "AnimNextInterfaceTypes.h"

namespace UE::AnimNext::Interface
{

struct FThreadContextData : TThreadSingleton<FThreadContextData>
{
	TArray<const FContext*> ContextStack;
};

// @TODO: we might need to do this for all FContext constructions?
// As parameters are gathered using the FContext linked list, we always need to access the top of the stack to get 
// correct parameters. Shortcutting to the bottom of the stack via FThreadContext misses intervening parameters
FThreadContext::FThreadContext(const FContext& InContext)
{
	FThreadContextData& ContextData = FThreadContextData::Get();
	ContextData.ContextStack.Push(&InContext);
}

FThreadContext::~FThreadContext()
{
	FThreadContextData& ContextData = FThreadContextData::Get();
	ContextData.ContextStack.Pop();
}

const FContext& FThreadContext::Get()
{
	const FThreadContextData& ContextData = FThreadContextData::Get();
	return *ContextData.ContextStack.Top();
}

FContext::FContext(float InDeltaTime, FState& InState, FParamStorage& InParamStorage, IAnimNextInterfaceParameters* InParameters)
	: State(&InState)
	, ParamStorage(&InParamStorage)
	, Parameters(InParameters)
	, DeltaTime(InDeltaTime)
	, BranchingMask(MAX_uint64)
{
}

FContext::~FContext()
{
	// Remove any handles before the storage goes out of scope
	AdditionalParameterHandles.Empty();

	if (BlockHandle != InvalidBlockHandle)
	{
		ParamStorage->ReleaseBlock(BlockHandle);
	}
}

FContext FContext::WithResult(FParam& InResult) const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, InResult);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;
	NewContext.BranchingMask = BranchingMask;

	return NewContext;
}

FContext FContext::WithParameter(FName ParameterId, const FParam& InParameter) const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, *Result);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;
	NewContext.BranchingMask = BranchingMask;
	NewContext.AddParameters({ TPairInitializer(ParameterId, InParameter) });
	return NewContext;
}

FContext FContext::WithParameters(TArrayView<const TPair<FName, FParam>> InParameters) const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, *Result);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;
	NewContext.BranchingMask = BranchingMask;
	NewContext.AddParameters(InParameters);

	return NewContext;
}

FContext FContext::WithResultAndParameters(FParam& InResult, TArrayView<const TPair<FName, FParam>> InParameters) const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, InResult);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;
	NewContext.BranchingMask = BranchingMask;
	NewContext.AddParameters(InParameters);

	return NewContext;
}

int32 FContext::GetParametersSize(TArrayView<const TPair<FName, FParam>> InParameters, TArray<int32> &ParamAllocSizes) const
{
	int32 TotalParamAllocSize = 0;

	const int32 NumParameters = InParameters.Num();

	for (int i = 0; i < NumParameters; i++)
	{
		const TPair<FName, FParam>& ParamPair = InParameters[i];
		const FParam& SourceParam = ParamPair.Value;

		if (EnumHasAnyFlags(SourceParam.GetFlags(), FParam::EFlags::Stored))
		{
			const FParamType& ParamType = SourceParam.GetType();

			const int32 NumElem = SourceParam.GetNumElements();
			const int32 ParamAlignment = ParamType.GetAlignment();
			const int32 ParamSize = ParamType.GetSize();

			const int32 ParamAllocSize = NumElem * Align(ParamSize, ParamAlignment);

			ParamAllocSizes[i] = ParamAllocSize;
			TotalParamAllocSize += ParamAllocSize;
		}
	}

	return TotalParamAllocSize;
}

void FContext::AddParameters(TArrayView<const TPair<FName, FParam>> InParameters)
{
	int32 TotalParamAllocSize = 0;

	const int32 NumParameters = InParameters.Num();

	TArray<int32> ParamAllocSizes;
	ParamAllocSizes.Reserve(NumParameters);
	ParamAllocSizes.AddZeroed(NumParameters);

	TotalParamAllocSize = GetParametersSize(InParameters, ParamAllocSizes);

	uint8* TargetMemory = nullptr;
	check(BlockHandle == InvalidBlockHandle);

	if (TotalParamAllocSize > 0)
	{
		const FParamStorage::TBlockDataPair BlockData = ParamStorage->RequestBlock(TotalParamAllocSize);

		BlockHandle = BlockData.Key;
		TargetMemory = BlockData.Value;
	}

	for(int i = 0; i < NumParameters; i++)
	{
		const TPair<FName, FParam>& ParamPair = InParameters[i];
		const FParam& SourceParam = ParamPair.Value;
		const int32 ParamAllocatedSize = ParamAllocSizes[i];

		if (ParamAllocatedSize > 0 && EnumHasAnyFlags(SourceParam.GetFlags(), FParam::EFlags::Stored))
		{
			const FParamType& ParamType = ParamPair.Value.GetType();

			const FParamType::ParamCopyFunction ParamCopyFunction = ParamType.GetParamCopyFunction();

			FParam ClonedParam = ParamCopyFunction(SourceParam, TargetMemory, ParamAllocatedSize);

			TargetMemory += ParamAllocatedSize;

			AdditionalParameters.Add(ParamPair.Key, ClonedParam);
		}
		else
		{
			AdditionalParameters.Add(ParamPair.Key, ParamPair.Value);
		}
	}
}

FContext FContext::WithParameters(IAnimNextInterfaceParameters* InParameters) const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, *Result);
	NewContext.Parameters = InParameters;
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;
	NewContext.BranchingMask = BranchingMask;

	return NewContext;
}

FContext FContext::CreateSubContext() const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, *Result);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;
	NewContext.BranchingMask = BranchingMask;

	return NewContext;
}

bool FContext::GetParameter(FName InKey, FParam& OutParam) const
{
	// Check parent contexts in turn. Parameters can be overriden by each scoped context.
	for(const FContext* CurrentContext = this; CurrentContext != nullptr; CurrentContext = CurrentContext->Parent)
	{
		// Check parameter provider
		if(CurrentContext->Parameters)
		{
			if(CurrentContext->Parameters->GetParameter(InKey, OutParam))
			{
				return true;
			}
		}
		// Check additional parameters
		// Note this is currently 'else if' because we dont allow creating new contexts with both a IAnimNextInterfaceParameters
		// interface and AdditionalParameters
		else if(!CurrentContext->AdditionalParameters.IsEmpty())
		{
			// Find the parameter
			if(const FParam* FoundParameter = CurrentContext->AdditionalParameters.Find(InKey))
			{
				// Check type compatibility
				if(FoundParameter->CanAssignTo(OutParam))
				{
					OutParam = *FoundParameter;
					return true;
				}
			}
		}
		// Check additional parameter handles
		// Note this is currently 'else if' because we dont allow creating new contexts with both 
		// AdditionalParameters and AdditionalParametersHandles
		else if (!CurrentContext->AdditionalParameterHandles.IsEmpty())
		{
			// Find the parameter
			if (const FHParam* HParam = CurrentContext->AdditionalParameterHandles.Find(InKey))
			{
				const FParam* Param = ParamStorage->GetParam(HParam->ParamHandle);
				if (Param != nullptr && Param->CanAssignTo(OutParam))
				{
					OutParam = Param;
					return true;
				}
			}
		}
	}

	return false;
}


TParam<const float> FContext::GetDeltaTimeParam() const
{
	return TWrapParam<const float>(&DeltaTime);
}

FContext FContext::WithCallRaw(const IAnimNextInterface* InAnimNextInterface) const
{
	FContext NewContext(DeltaTime, *State, *ParamStorage, *Result);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = HashCombineFast(CallstackHash, GetTypeHash(InAnimNextInterface));
	NewContext.BranchingMask = BranchingMask;
	return NewContext;
}

void FContext::FlushRelevancy() const
{
	// Flush all relevancy-based allocations if they were not used this update
	// @TODO: this could be more efficient if we use a more linear iteration here 
	for(auto It = State->RelevancyValueMap.CreateIterator(); It; ++It)
	{
		if(It->Value.UpdateCounter != UpdateCounter)
		{
			It.RemoveCurrent();
		}
	}
}

}