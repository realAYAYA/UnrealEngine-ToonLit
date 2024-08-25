// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"


struct FPCGPoint;
struct FPCGTaggedData;

// TODO: Deprecate this class once all of the internal inheritors have been cleaned up
// class UE_DEPRECATED(5.4, "FPCGPointProcessingElementBase is deprecated. Please use 'FPCGPointOperationElementBase' instead.") PCG_API FPCGPointProcessingElementBase : public IPCGElement
class PCG_API FPCGPointProcessingElementBase : public IPCGElement
{
protected:
	void ProcessPoints(FPCGContext* Context, const TArray<FPCGTaggedData>& Inputs, TArray<FPCGTaggedData>& Outputs, const TFunction<bool(const FPCGPoint&, FPCGPoint&)>& PointFunc) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Helpers/PCGAsync.h"
#endif
