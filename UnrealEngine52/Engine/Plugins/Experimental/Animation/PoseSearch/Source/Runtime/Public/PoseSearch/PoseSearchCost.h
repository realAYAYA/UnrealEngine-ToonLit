// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearchCost.generated.h"

USTRUCT()
struct POSESEARCH_API FPoseSearchCost
{
	GENERATED_BODY()
public:
	FPoseSearchCost() = default;
	FPoseSearchCost(float InDissimilarityCost, float InNotifyCostAddend, float InMirrorMismatchCostAddend, float InContinuingPoseCostAddend)
	: TotalCost(InDissimilarityCost + InNotifyCostAddend + InMirrorMismatchCostAddend + InContinuingPoseCostAddend)
	{
#if WITH_EDITORONLY_DATA
		NotifyCostAddend = InNotifyCostAddend;
		MirrorMismatchCostAddend = InMirrorMismatchCostAddend;
		ContinuingPoseCostAddend = InContinuingPoseCostAddend;
#endif // WITH_EDITORONLY_DATA
	}

	bool IsValid() const { return TotalCost != MAX_flt; }
	float GetTotalCost() const { return TotalCost; }
	bool operator<(const FPoseSearchCost& Other) const { return TotalCost < Other.TotalCost; }

protected:
	UPROPERTY()
	float TotalCost = MAX_flt;

#if WITH_EDITORONLY_DATA
public:

	float GetCostAddend() const { return NotifyCostAddend + MirrorMismatchCostAddend + ContinuingPoseCostAddend; }

	// Contribution from ModifyCost anim notify
	UPROPERTY()
	float NotifyCostAddend = 0.f;

	// Contribution from mirroring cost
	UPROPERTY()
	float MirrorMismatchCostAddend = 0.f;

	UPROPERTY()
	float ContinuingPoseCostAddend = 0.f;

#endif // WITH_EDITORONLY_DATA
};