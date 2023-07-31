// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassSimulationLOD.h"

#include "MassLODTrait.generated.h"

UCLASS(meta = (DisplayName = "LODCollector"))
class MASSLOD_API UMassLODCollectorTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(meta = (DisplayName = "SimulationLOD"))
class MASSLOD_API UMassSimulationLODTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

	UPROPERTY(Category = "Config", EditAnywhere)
	FMassSimulationLODParameters Params;

	UPROPERTY(Category = "Config", EditAnywhere)
	bool bEnableVariableTicking = false;

	UPROPERTY(Category = "Config", EditAnywhere, meta = (EditCondition = "bEnableVariableTicking", EditConditionHides))
	FMassSimulationVariableTickParameters VariableTickParams;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
