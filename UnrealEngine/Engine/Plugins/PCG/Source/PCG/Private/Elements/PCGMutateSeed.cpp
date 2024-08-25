// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGMutateSeed.h"

#include "PCGContext.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGMutateSeedSettings"

namespace PCGMutateSeedConstants
{
	// TODO: Evaluate this value for optimization
	// An evolving best guess for the most optimized number of points to operate per thread per slice
	static constexpr int32 PointsPerChunk = 98304;
}

UPCGMutateSeedSettings::UPCGMutateSeedSettings()
{
	bUseSeed = true;
}

FPCGElementPtr UPCGMutateSeedSettings::CreateElement() const
{
	return MakeShared<FPCGMutateSeedElement>();
}

bool FPCGMutateSeedElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMutateSeedElement::Execute);

	check(Context);
	ContextType* MutateSeedContext = static_cast<ContextType*>(Context);

	return ExecutePointOperation(MutateSeedContext, [Seed = Context->GetSeed()](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
	{
		OutPoint = InPoint;
		OutPoint.Seed = PCGHelpers::ComputeSeed(PCGHelpers::ComputeSeedFromPosition(OutPoint.Transform.GetLocation()), Seed, OutPoint.Seed);
		return true;
	}, PCGMutateSeedConstants::PointsPerChunk);
}

#undef LOCTEXT_NAMESPACE
