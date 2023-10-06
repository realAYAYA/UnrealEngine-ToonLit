// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGBoundsModifier.h"

#include "PCGContext.h"
#include "PCGPoint.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGBoundsModifier)

#define LOCTEXT_NAMESPACE "PCGBoundsModifier"

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

	const UPCGBoundsModifierSettings* Settings = Context->GetInputSettings<UPCGBoundsModifierSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const EPCGBoundsModifierMode Mode = Settings->Mode;
	const FVector& BoundsMin = Settings->BoundsMin;
	const FVector& BoundsMax = Settings->BoundsMax;
	const bool bAffectSteepness = Settings->bAffectSteepness;
	const float Steepness = Settings->Steepness;

	const FBox Bounds(BoundsMin, BoundsMax);

	switch (Mode)
	{
	case EPCGBoundsModifierMode::Intersect:
		ProcessPoints(Context, Inputs, Outputs, [&Bounds, bAffectSteepness, Steepness](const FPCGPoint& InPoint, FPCGPoint& OutPoint){
			OutPoint = InPoint;
			OutPoint.SetLocalBounds(InPoint.GetLocalBounds().Overlap(Bounds));
			
			if (bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Min(InPoint.Steepness, Steepness);
			}

			return true;
		});
		break;

	case EPCGBoundsModifierMode::Include:
		ProcessPoints(Context, Inputs, Outputs, [&Bounds, bAffectSteepness, Steepness](const FPCGPoint& InPoint, FPCGPoint& OutPoint){
			OutPoint = InPoint;
			OutPoint.SetLocalBounds(InPoint.GetLocalBounds() + Bounds);

			if (bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Max(InPoint.Steepness, Steepness);
			}

			return true;
		});
		break;

	case EPCGBoundsModifierMode::Translate:
		ProcessPoints(Context, Inputs, Outputs, [&BoundsMin, &BoundsMax, bAffectSteepness, Steepness](const FPCGPoint& InPoint, FPCGPoint& OutPoint){
			OutPoint = InPoint;
			OutPoint.BoundsMin += BoundsMin;
			OutPoint.BoundsMax += BoundsMax;

			if (bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Clamp(InPoint.Steepness + Steepness, 0.0f, 1.0f);
			}

			return true;
		});
		break;

	case EPCGBoundsModifierMode::Scale:
		ProcessPoints(Context, Inputs, Outputs, [&BoundsMin, &BoundsMax, bAffectSteepness, Steepness](const FPCGPoint& InPoint, FPCGPoint& OutPoint){
			OutPoint = InPoint;
			OutPoint.BoundsMin *= BoundsMin;
			OutPoint.BoundsMax *= BoundsMax;

			if (bAffectSteepness)
			{
				OutPoint.Steepness = FMath::Clamp(InPoint.Steepness * Steepness, 0.0f, 1.0f);
			}

			return true;
		});
		break;

	case EPCGBoundsModifierMode::Set:
		ProcessPoints(Context, Inputs, Outputs, [&Bounds, bAffectSteepness, Steepness](const FPCGPoint& InPoint, FPCGPoint& OutPoint){
			OutPoint = InPoint;
			OutPoint.SetLocalBounds(Bounds);

			if (bAffectSteepness)
			{
				OutPoint.Steepness = Steepness;
			}

			return true;
		});
		break;
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
