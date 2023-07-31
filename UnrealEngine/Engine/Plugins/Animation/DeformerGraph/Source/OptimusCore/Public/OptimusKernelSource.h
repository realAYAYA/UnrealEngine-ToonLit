// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeKernelSource.h"
#include "OptimusKernelSource.generated.h"

UCLASS()
class OPTIMUSCORE_API UOptimusKernelSource :
	public UComputeKernelSource
{
	GENERATED_BODY()

public:
	void SetSource(FString const& InSource)
	{
		Source = InSource;
	}
	
	//~ Begin UComputeKernelSource Interface.
	FString GetSource() const override
	{
		return Source;
	}
	//~ End UComputeKernelSource Interface.

protected:
	UPROPERTY()
	FString Source;
};
