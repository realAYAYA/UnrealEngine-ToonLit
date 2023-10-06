// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassVisualizationTrait.h"
#include "MassStationaryVisualizationTrait.generated.h"


UCLASS()
class MASSREPRESENTATION_API UMassStationaryVisualizationTrait : public UMassVisualizationTrait
{
	GENERATED_BODY()
public:
	UMassStationaryVisualizationTrait(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

protected:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
};
