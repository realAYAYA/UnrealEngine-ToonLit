// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PoseSearchCost.generated.h"

USTRUCT()
struct POSESEARCH_API FPoseSearchCost
{
	GENERATED_BODY()
public:
	FPoseSearchCost() = default;
	FPoseSearchCost(float DissimilarityCost, float NotifyCostAddend, float ContinuingPoseCostAddend)
	: TotalCost(DissimilarityCost + NotifyCostAddend + ContinuingPoseCostAddend)
	{
#if WITH_EDITORONLY_DATA
		CostAddend = NotifyCostAddend + ContinuingPoseCostAddend;
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

	float GetCostAddend() const { return CostAddend; }

	// Contribution from ModifyCost anim notify, and ContinuingPoseCostAddend
	UPROPERTY()
	float CostAddend = 0.f;

#endif // WITH_EDITORONLY_DATA
};