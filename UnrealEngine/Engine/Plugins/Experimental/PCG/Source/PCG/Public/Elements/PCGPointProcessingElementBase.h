// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGElement.h"


struct FPCGPoint;
struct FPCGTaggedData;

class PCG_API FPCGPointProcessingElementBase : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const = 0;
	void ProcessPoints(FPCGContext* Context, const TArray<FPCGTaggedData>& Inputs, TArray<FPCGTaggedData>& Outputs, const TFunction<bool(const FPCGPoint&, FPCGPoint&)>& PointFunc) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Helpers/PCGAsync.h"
#endif
