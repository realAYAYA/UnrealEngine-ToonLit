// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEngine.h"

#include "DataflowContextOverridesNodes.generated.h"

USTRUCT(meta = (DataflowFlesh))
struct DATAFLOWNODES_API FFloatOverrideDataflowNode : public FDataflowNode
{
	GENERATED_USTRUCT_BODY()
	DATAFLOW_NODE_DEFINE_INTERNAL(FFloatOverrideDataflowNode, "FloatOverride", "Dataflow", "")

public:
	UPROPERTY(EditAnywhere, Category = "Overrides")
	FName PropertyName = "Overrides";

	UPROPERTY(EditAnywhere, Category = "Overrides")
	FName KeyName = "FloatAttr";

	UPROPERTY(meta = (DataflowOutput, DisplayName = "Float"))
	float ValueOut = 0.f;

	FFloatOverrideDataflowNode(const Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid())
		: FDataflowNode(InParam, InGuid)
	{
		RegisterOutputConnection(&ValueOut);
	}

	virtual void Evaluate(Dataflow::FContext& Context, const FDataflowOutput* Out) const override;
};


namespace Dataflow
{
	void RegisterContextOverridesNodes();
}

