// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeGraph.h"
#include "OptimusShaderText.h"

#include "OptimusComputeGraph.generated.h"

class UOptimusDeformer;
class UOptimusNode;

UCLASS()
class UOptimusComputeGraph :
	public UComputeGraph
{
	GENERATED_BODY()

public:
	// UObject overrides
	void Serialize(FArchive& Ar) override;
	void PostLoad() override;

	// UComputeGraph overrides
	void OnKernelCompilationComplete(int32 InKernelIndex, const TArray<FString>& InCompileOutputMessages) override;

protected:
	// Lookup into Graphs array from the UComputeGraph kernel index. 
	UPROPERTY()
	TArray<TWeakObjectPtr<const UOptimusNode>> KernelToNode;

	friend class UOptimusDeformer;
};
