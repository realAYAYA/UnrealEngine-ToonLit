// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSchedulingPolicyBase.h"

#include "PCGSchedulingPolicyDistanceAndDirection.generated.h"

class IPCGGenSourceBase;

/**
 * SchedulingPolicyDistanceAndDirection uses distance from the generating volume 
 * and alignment with view direction to choose the most important volumes to generate.
 *
 * Distance and Direction are calculated with respect to the Generation Source.
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSchedulingPolicyDistanceAndDirection : public UPCGSchedulingPolicyBase
{
	GENERATED_BODY()

public:
	/** Calculate the runtime scheduling priority with respect to a Generation Source. Should return a value in the range [0, 1], where higher values will be scheduled sooner. */
	virtual double CalculatePriority(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const override;

	/** A SchedulingPolicy is equivalent to another SchedulingPolicy if they are the same (same ptr), or if they have the same type and parameter values. */
	virtual bool IsEquivalent(const UPCGSchedulingPolicyBase* OtherSchedulingPolicy) const;

public:
	/** Toggle whether or not distance is used to calculate the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides, HideEditConditionToggle))
	bool bUseDistance = true;

	/** Scalar value used to increase/decrease the impact of distance in the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bShouldDisplayProperties && bUseDistance", EditConditionHides))
	float DistanceWeight = 1.0f;

	/** Toggle whether or not direction is used to calculate the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (EditCondition = "bShouldDisplayProperties", EditConditionHides, HideEditConditionToggle))
	bool bUseDirection = true;

	/** Scalar value used to increase/decrease the impact of direction in the scheduling priority. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "RuntimeGeneration|Scheduling Policy Parameters", meta = (ClampMin = 0.0f, ClampMax = 1.0f, EditCondition = "bShouldDisplayProperties && bUseDirection", EditConditionHides))
	float DirectionWeight = 0.0025f;
};
