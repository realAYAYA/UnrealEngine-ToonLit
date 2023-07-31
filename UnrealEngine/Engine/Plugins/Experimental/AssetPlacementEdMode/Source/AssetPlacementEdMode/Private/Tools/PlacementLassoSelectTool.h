// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "Tools/PlacementBrushToolBase.h"

#include "PlacementLassoSelectTool.generated.h"

UCLASS(Transient, MinimalAPI)
class UPlacementModeLassoSelectToolBuilder : public UPlacementToolBuilderBase
{
	GENERATED_BODY()

public:
	virtual UPlacementBrushToolBase* FactoryToolInstance(UObject* Outer) const override;
};

UCLASS(MinimalAPI)
class UPlacementModeLassoSelectTool : public UPlacementBrushToolBase
{
	GENERATED_BODY()

public:
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual void OnTick(float DeltaTime) override;

	constexpr static TCHAR ToolName[] = TEXT("LassoSelectTool");
};
