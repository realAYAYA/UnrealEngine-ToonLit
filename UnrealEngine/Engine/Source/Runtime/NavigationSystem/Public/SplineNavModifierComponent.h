// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NavModifierComponent.h"

#include "SplineNavModifierComponent.generated.h"

struct FNavigationRelevantData;

/**
 *	Assign a chosen NavArea in the vicinity of a spline
 *	This component must only be attached to an actor with a USplineComponent
 */
UCLASS(Blueprintable, Meta = (BlueprintSpawnableComponent), hidecategories = (Variable, Tags, Cooking, Collision), MInimalAPI)
class USplineNavModifierComponent : public UNavModifierComponent
{
	GENERATED_BODY()

	/** Radius of the tube surrounding the spline which will be used to generate the Nav Modifiers */
	UPROPERTY(EditAnywhere, Category = Navigation, Meta=(UIMin="1", ClampMin="1"))
	double SplineExtent = 1000.0f;

	/** How many sections the spline will be divided into for generating overlap volumes. More samples result in finer detail */
	UPROPERTY(EditAnywhere, Category = Navigation, Meta=(UIMin="2", ClampMin="2"))
	int32 NumSplineSamples = 100;
	
	virtual void CalculateBounds() const override;
	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;
};
