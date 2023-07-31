// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RenderUtils.h"
#include "RenderingThread.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRHIUnitTestCommandlet, Log, All);

#define RUN_TEST(x) do { bool Ret = x; bResult = bResult && Ret; } while (false)

bool IsZeroMem(const void* Ptr, uint32 Size);

bool RunOnRenderThreadSynchronous(TFunctionRef<bool(FRHICommandListImmediate&)> TestFunc);

template <typename ValueType>
static inline FString ClearValueToString(const ValueType & ClearValue)
{
	if (TAreTypesEqual<ValueType, FVector4>::Value)
	{
		return FString::Printf(TEXT("%f %f %f %f"), ClearValue.X, ClearValue.Y, ClearValue.Z, ClearValue.W);
	}
	else
	{
		return FString::Printf(TEXT("0x%08x 0x%08x 0x%08x 0x%08x"), ClearValue.X, ClearValue.Y, ClearValue.Z, ClearValue.W);
	}
}