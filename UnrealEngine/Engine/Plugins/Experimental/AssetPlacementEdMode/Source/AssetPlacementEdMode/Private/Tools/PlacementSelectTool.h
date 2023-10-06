// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/PlacementBrushToolBase.h"
#include "Tools/PlacementClickDragToolBase.h"

#include "PlacementSelectTool.generated.h"

UCLASS(Transient, MinimalAPI)
class UPlacementModeSelectToolBuilder : public UPlacementToolBuilderBase
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual UPlacementBrushToolBase* FactoryToolInstance(UObject* Outer) const override;
};

UCLASS(MinimalAPI)
class UPlacementModeSelectTool : public UPlacementClickDragToolBase
{
	GENERATED_BODY()

public:
	virtual void Setup() override;
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;

	constexpr static TCHAR ToolName[] = TEXT("SelectTool");
};
