// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGResetPointCenter.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"

#define LOCTEXT_NAMESPACE "PCGResetPointCenterElement"

FPCGElementPtr UPCGResetPointCenterSettings::CreateElement() const
{
	return MakeShared<FPCGResetPointCenterElement>();
}

bool FPCGResetPointCenterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGResetPointCenterElement::Execute);
	check(Context);

	FPCGResetPointCenterElement::ContextType* PointCenterContext = static_cast<FPCGResetPointCenterElement::ContextType*>(Context);

	const UPCGResetPointCenterSettings* Settings = Context->GetInputSettings<UPCGResetPointCenterSettings>();
	check(Settings);

	return ExecutePointOperation(PointCenterContext, [&PointCenterLocation = Settings->PointCenterLocation](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;

		OutPoint.ResetPointCenter(PointCenterLocation);

		return true;
	});
}

#undef LOCTEXT_NAMESPACE