// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScalabilityState.generated.h"

USTRUCT()
struct FNiagaraScalabilityState
{
	GENERATED_BODY()

	FNiagaraScalabilityState()
		: Significance(1.0f)
		, LastVisibleTime(0.0f)
		, SystemDataIndex(INDEX_NONE)
		, bNewlyRegistered(1)
		, bNewlyRegisteredDirty(0)
		, bCulled(0)
		, bPreviousCulled(0)
		, bCulledByDistance(0)
		, bCulledByInstanceCount(0)
		, bCulledByVisibility(0)
		, bCulledByGlobalBudget(0)
	{
	}

	FNiagaraScalabilityState(float InSignificance, bool InCulled, bool InPreviousCulled)
		: Significance(InSignificance)
		, LastVisibleTime(0.0f)
		, SystemDataIndex(INDEX_NONE)
		, bNewlyRegistered(1)
		, bNewlyRegisteredDirty(0)
		, bCulled(InCulled)
		, bPreviousCulled(InPreviousCulled)
		, bCulledByDistance(0)
		, bCulledByInstanceCount(0)
		, bCulledByVisibility(0)
		, bCulledByGlobalBudget(0)
	{
	}

	bool IsDirty() const { return bCulled != bPreviousCulled; }
	void Apply() { bPreviousCulled = bCulled; }

	UPROPERTY(VisibleAnywhere, Category="Scalability")
	float Significance;

	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	float LastVisibleTime;

	int16 SystemDataIndex;

	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	uint8 bNewlyRegistered : 1;

	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	uint8 bNewlyRegisteredDirty : 1;

	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	uint8 bCulled : 1;

	UPROPERTY(VisibleAnywhere, Category="Scalability")
	uint8 bPreviousCulled : 1;

	UPROPERTY(VisibleAnywhere, Category="Scalability")
	uint8 bCulledByDistance : 1;

	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	uint8 bCulledByInstanceCount : 1;

	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	uint8 bCulledByVisibility : 1;
	
	UPROPERTY(VisibleAnywhere, Category = "Scalability")
	uint8 bCulledByGlobalBudget : 1;
};
