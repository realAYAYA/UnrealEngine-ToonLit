// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/PlacementBrushToolBase.h"

#include "PlacementClickDragToolBase.generated.h"

UCLASS(Abstract, MinimalAPI)
class UPlacementClickDragToolBase : public UPlacementBrushToolBase
{
	GENERATED_BODY()
public:
	virtual void Setup() override;

protected:
	virtual double EstimateMaximumTargetDimension() override;
	virtual void SetupBrushStampIndicator() override;
};
