// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/SchedulingPolicies/PCGSchedulingPolicyDistanceAndDirection.h"

#include "PCGCommon.h"
#include "RuntimeGen/GenSources/PCGGenSourceBase.h"

#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSchedulingPolicyDistanceAndDirection)

double UPCGSchedulingPolicyDistanceAndDirection::CalculatePriority(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const
{
	check(InGenSource);

	double Priority = 0.0;

	if (bUseDistance || bUseDirection)
	{
		TOptional<FVector> GenSourcePositionOptional = InGenSource->GetPosition();

		if (ensure(GenSourcePositionOptional.IsSet()))
		{
			FVector GenSourcePosition = GenSourcePositionOptional.GetValue();
			if (bUse2DGrid)
			{
				GenSourcePosition.Z = 0.0;
			}

			const FVector GenSourceToComponent = GenerationBounds.GetClosestPointTo(GenSourcePosition) - GenSourcePosition;

			if (bUseDistance)
			{
				const double MaxRadius = 2.0 * PCGHiGenGrid::GridToGridSize(EPCGHiGenGrid::GridMax);
				const double Distance = 1.0 - FMath::Clamp(GenSourceToComponent.Length() / MaxRadius, 0.0, 1.0);
				Priority += Distance * FMath::Clamp(DistanceWeight, 0.0f, 1.0f);
			}

			if (bUseDirection)
			{
				TOptional<FVector> GenSourceDirectionOptional = InGenSource->GetDirection();

				if ((GenSourceDirectionOptional.IsSet()))
				{
					const double DirectionDotProd = FVector::DotProduct(GenSourceToComponent.GetSafeNormal(), GenSourceDirectionOptional.GetValue());
					Priority += DirectionDotProd * FMath::Clamp(DirectionWeight, 0.0f, 1.0f);
				}
			}
		}
	}

	// Remap range from [-1, 2] to [0, 1].
	Priority = (Priority + 1.0) / 3.0;
	ensure(Priority >= 0.0 && Priority <= 1.0);

	return Priority;
}

bool UPCGSchedulingPolicyDistanceAndDirection::IsEquivalent(const UPCGSchedulingPolicyBase* OtherSchedulingPolicy) const
{
	if (!OtherSchedulingPolicy)
	{
		return false;
	}

	if (this == OtherSchedulingPolicy)
	{
		return true;
	}

	if (const UPCGSchedulingPolicyDistanceAndDirection* Other = Cast<UPCGSchedulingPolicyDistanceAndDirection>(OtherSchedulingPolicy))
	{
		return bUseDistance == Other->bUseDistance
			&& DistanceWeight == Other->DistanceWeight
			&& bUseDirection == Other->bUseDirection
			&& DirectionWeight == Other->DirectionWeight;
	}
	else
	{
		return false;
	}
}
