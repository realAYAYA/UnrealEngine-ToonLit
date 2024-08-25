// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/AnimNextExecuteContext.h"
#include "AnimNextParameterExecuteContext.generated.h"

class UAnimNextParameterBlock;

namespace UE::AnimNext
{
	struct FParamStackLayerHandle;
}

USTRUCT(BlueprintType)
struct FAnimNextParameterExecuteContext : public FAnimNextExecuteContext
{
	GENERATED_BODY()

	FAnimNextParameterExecuteContext()
		: FAnimNextExecuteContext()
	{
	}

	virtual void Copy(const FRigVMExecuteContext* InOtherContext) override
	{
		Super::Copy(InOtherContext);

		const FAnimNextParameterExecuteContext* OtherContext = (const FAnimNextParameterExecuteContext*)InOtherContext;
		LayerHandle = OtherContext->LayerHandle;
	}

	UE::AnimNext::FParamStackLayerHandle& GetLayerHandle() const
	{
		return *LayerHandle;
	}

private:
	friend class UAnimNextParameterBlock;

	void SetParamContextData(UE::AnimNext::FParamStackLayerHandle& InLayerHandle)
	{
		LayerHandle = &InLayerHandle;
	}

	// Parameter scope context
	UE::AnimNext::FParamStackLayerHandle* LayerHandle = nullptr;
};

USTRUCT(meta = (ExecuteContext = "FAnimNextParameterExecuteContext"))
struct FRigUnit_AnimNextParameterBase : public FRigUnit
{
	GENERATED_BODY()
};
