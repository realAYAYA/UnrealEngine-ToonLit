// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGApplyScaleToBounds.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"

#define LOCTEXT_NAMESPACE "PCGApplyScaleToBoundsElement"

FPCGElementPtr UPCGApplyScaleToBoundsSettings::CreateElement() const
{
	return MakeShared<FPCGApplyScaleToBoundsElement>();
}

bool FPCGApplyScaleToBoundsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGApplyScaleToBoundsElement::Execute);
	check(Context);

	FPCGApplyScaleToBoundsElement::ContextType* ApplyScaleContext = static_cast<FPCGApplyScaleToBoundsElement::ContextType*>(Context);

	return ExecutePointOperation(ApplyScaleContext, [](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;

		OutPoint.ApplyScaleToBounds();
		
		return true;
	});
}

#undef LOCTEXT_NAMESPACE