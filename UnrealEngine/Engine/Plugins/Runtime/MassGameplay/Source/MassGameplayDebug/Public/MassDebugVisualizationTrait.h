// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassEntityTraitBase.h"
#include "MassGameplayDebugTypes.h"
#include "MassDebugVisualizationTrait.generated.h"


UCLASS(meta = (DisplayName = "Debug Visualization"))
class MASSGAMEPLAYDEBUG_API UMassDebugVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

protected:

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Mass|Spawn")
	FAgentDebugVisualization DebugShape;
#endif // WITH_EDITORONLY_DATA
};
