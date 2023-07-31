// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernel.h"
#include "ComputeFramework/ComputeKernelSource.h"

#if WITH_EDITOR

void UComputeKernel::PostLoad()
{
	Super::PostLoad();
	if (KernelSource != nullptr)
	{
		KernelSource->ConditionalPostLoad();
	}
}

#endif // WITH_EDITOR
