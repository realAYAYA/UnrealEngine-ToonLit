// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomResourcePool.h"
#include "CoreGlobals.h"

ICustomResourcePool* GCustomResourcePool = nullptr;

void ICustomResourcePool::TickPoolElements(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());
	if (GCustomResourcePool)
	{
		GCustomResourcePool->Tick(RHICmdList);
	}
}
