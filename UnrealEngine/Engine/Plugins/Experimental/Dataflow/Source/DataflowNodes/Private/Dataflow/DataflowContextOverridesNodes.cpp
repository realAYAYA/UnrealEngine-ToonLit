// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowContextOverridesNodes.h"

#include "Dataflow/DataflowEngineUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowContextOverridesNodes)

namespace Dataflow
{
	void RegisterContextOverridesNodes()
	{
		DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FFloatOverrideDataflowNode);
	}
}

void FFloatOverrideDataflowNode::Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const
{
	if (Out->IsA<float>(&ValueOut))
	{
		float Value = 0.f;
		if (const Dataflow::FEngineContext* EngineContext = Context.AsType<Dataflow::FEngineContext>())
		{
			FString Result = Dataflow::Reflection::FindOverrideProperty< FString >(EngineContext->Owner, PropertyName, KeyName);
			Value = FCString::Atof(*Result);
		}
		SetValue(Context, Value, &ValueOut);
	}
}


