// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterfaceContext.h"
#include "IDataInterface.h"
#include "IDataInterfaceParameters.h"
#include "DataInterfaceTypes.h"

namespace UE::DataInterface
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

FContext::FContext(float InDeltaTime, FState& InState, IDataInterfaceParameters* InParameters)
	: State(InState)
	, Result(nullptr)
	, Parameters(InParameters)
	, DeltaTime(InDeltaTime)
{
}

FContext FContext::WithResult(FParam& InResult) const
{
	FContext NewContext(DeltaTime, State, &InResult);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;

	return NewContext;
}

FContext FContext::WithParameter(FName ParameterId, const FParam& InParameter) const
{
	FContext NewContext(DeltaTime, State, Result);
	NewContext.AdditionalParameters.Add(ParameterId, InParameter);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;

	return NewContext;
}

FContext FContext::WithParameters(TArrayView<const TPair<FName, FParam>> InParameters) const
{
	FContext NewContext(DeltaTime, State, Result);
	for(const TPair<FName, FParam>& ParamPair : InParameters)
	{
		NewContext.AdditionalParameters.Add(ParamPair.Key, ParamPair.Value);
	}
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;

	return NewContext;
}

FContext FContext::WithParameters(IDataInterfaceParameters* InParameters) const
{
	FContext NewContext(DeltaTime, State, Result);
	NewContext.Parameters = InParameters;
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = CallstackHash;

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
		// Note this is currently 'else if' because we dont allow creating new contexts with both a IDataInterfaceParameters
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
	}

	return false;
}

TParam<const float> FContext::GetDeltaTimeParam() const
{
	return TWrapParam<const float>(*this, &DeltaTime);
}

FContext FContext::WithCallRaw(const IDataInterface* InDataInterface) const
{
	FContext NewContext(DeltaTime, State, Result);
	NewContext.Parent = this;
	NewContext.Root = (Root == nullptr) ? this : Root;
	NewContext.CallstackHash = HashCombineFast(CallstackHash, GetTypeHash(InDataInterface));
	return NewContext;
}

void FContext::FlushRelevancy() const
{
	// Flush all relevancy-based allocations if they were not used this update
	// @TODO: this could be more efficient if we use a more linear iteration here 
	for(auto It = State.RelevancyValueMap.CreateIterator(); It; ++It)
	{
		if(It->Value.UpdateCounter != UpdateCounter)
		{
			It.RemoveCurrent();
		}
	}
}

}