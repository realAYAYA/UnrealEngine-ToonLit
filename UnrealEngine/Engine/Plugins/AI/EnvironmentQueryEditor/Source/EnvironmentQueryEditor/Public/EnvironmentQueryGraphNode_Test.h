// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQueryGraphNode.h"
#include "EnvironmentQueryGraphNode_Test.generated.h"

UCLASS()
class UEnvironmentQueryGraphNode_Test : public UEnvironmentQueryGraphNode
{
	GENERATED_UCLASS_BODY()

	/** weight percent for display */
	UPROPERTY()
	float TestWeightPct;

	/** weight is passed as named param */
	UPROPERTY()
	uint32 bHasNamedWeight : 1;

	/** toggles test */
	UPROPERTY()
	uint32 bTestEnabled : 1;

	uint32 bStatShowOverlay : 1;
	FEnvionmentQueryNodeStats Stats;

	virtual void InitializeInstance() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetDescription() const override;

	void SetDisplayedWeight(float Pct, bool bNamed);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
