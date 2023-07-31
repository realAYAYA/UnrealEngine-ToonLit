// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class FRHICommandListImmediate;

class RENDERCORE_API ICustomResourcePool
{
public:
	virtual ~ICustomResourcePool() {}
	virtual void Tick(FRHICommandListImmediate& RHICmdList) = 0;

	static void TickPoolElements(FRHICommandListImmediate& RHICmdList);
};
extern RENDERCORE_API ICustomResourcePool* GCustomResourcePool;
