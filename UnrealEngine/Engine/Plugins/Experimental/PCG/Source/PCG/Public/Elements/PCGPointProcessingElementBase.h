// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PCGElement.h"

#include "Helpers/PCGAsync.h"

class FPCGPointProcessingElementBase : public FSimplePCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const = 0;
	void ProcessPoints(FPCGContext* Context, const TArray<FPCGTaggedData>& Inputs, TArray<FPCGTaggedData>& Outputs, const TFunction<bool(const FPCGPoint&, FPCGPoint&)>& PointFunc) const;
};
