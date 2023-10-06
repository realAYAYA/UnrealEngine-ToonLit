// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassVisualizationTrait.h"
#include "MassMovableVisualizationTrait.generated.h"


UCLASS()
class MASSREPRESENTATION_API UMassMovableVisualizationTrait : public UMassVisualizationTrait
{
	GENERATED_BODY()
public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
	