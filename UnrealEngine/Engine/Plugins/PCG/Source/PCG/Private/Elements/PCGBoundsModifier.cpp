// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGBoundsModifier.h"

#include "PCGContext.h"
#include "PCGPoint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGBoundsModifier)

#define LOCTEXT_NAMESPACE "PCGBoundsModifier"

namespace PCGBoundsModifier
{
	// TODO: Evaluate this value for optimization
	// An evolving best guess for the most optimized number of points to operate per thread per slice
	static constexpr int32 PointsPerChunk = 65536;
}

FPCGElementPtr UPCGBoundsModifierSettings::CreateElement() const
{
	return MakeShared<FPCGBoundsModifier>();
}

#if WITH_EDITOR
FText UPCGBoundsModifierSettings::GetNodeTooltipText() const
{
	return LOCTEXT("BoundsModifierNodeTooltip", "Applies a transformation on the point bounds & optionally its steepness.");
}
#endif // WITH_EDITOR

bool FPCGBoundsModifier::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGBoundsModifier::Execute);

	check(Context);
	FPCGBoundsModifier::ContextType* BoundsModifierContext = static_cast<FPCGBoundsModifier::ContextType*>(Context);

	const UPCGBoundsModifierSettings* Settings = Context->GetInputSettings<UPCGBoundsModifierSettings>();
	check(Settings);

	switch (Settings->Mode)
	{
		case EPCGBoundsModifierMode::Intersect:
		return ExecutePointOperation(BoundsModifierContext, [Settings](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutPoint.SetLocalBounds(InPoint.GetLocalBounds().Overlap(FBox(Settings->BoundsMin, Settings->BoundsMax)));

			if (Settings->bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Min(InPoint.Steepness, Settings->Steepness);
			}

			return true;
		}, PCGBoundsModifier::PointsPerChunk);

	case EPCGBoundsModifierMode::Include:
		return ExecutePointOperation(BoundsModifierContext, [Settings](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutPoint.SetLocalBounds(InPoint.GetLocalBounds() + FBox(Settings->BoundsMin, Settings->BoundsMax));

			if (Settings->bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Max(InPoint.Steepness, Settings->Steepness);
			}

			return true;
		}, PCGBoundsModifier::PointsPerChunk);

	case EPCGBoundsModifierMode::Translate:
		return ExecutePointOperation(BoundsModifierContext, [Settings](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutPoint.BoundsMin += Settings->BoundsMin;
			OutPoint.BoundsMax += Settings->BoundsMax;

			if (Settings->bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Clamp(InPoint.Steepness + Settings->Steepness, 0.0f, 1.0f);
			}

			return true;
		}, PCGBoundsModifier::PointsPerChunk);

	case EPCGBoundsModifierMode::Scale:
		return ExecutePointOperation(BoundsModifierContext, [Settings](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutPoint.BoundsMin *= Settings->BoundsMin;
			OutPoint.BoundsMax *= Settings->BoundsMax;

			if (Settings->bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Clamp(InPoint.Steepness * Settings->Steepness, 0.0f, 1.0f);
			}

			return true;
		}, PCGBoundsModifier::PointsPerChunk);

	case EPCGBoundsModifierMode::Set:
		return ExecutePointOperation(BoundsModifierContext, [Settings](const FPCGPoint& InPoint, FPCGPoint& OutPoint)
		{
			OutPoint = InPoint;
			OutPoint.SetLocalBounds(FBox(Settings->BoundsMin, Settings->BoundsMax));

			if (Settings->bAffectSteepness)
			{
				OutPoint.Steepness = Settings->Steepness;
			}

			return true;
		}, PCGBoundsModifier::PointsPerChunk);

		default:
			checkNoEntry();
			return true;
	}
}

#undef LOCTEXT_NAMESPACE
