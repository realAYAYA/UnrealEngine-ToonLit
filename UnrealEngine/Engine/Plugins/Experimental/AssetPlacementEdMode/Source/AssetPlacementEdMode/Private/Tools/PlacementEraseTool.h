// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "Tools/PlacementBrushToolBase.h"

#include "PlacementEraseTool.generated.h"

UCLASS(Transient, MinimalAPI)
class UPlacementModeEraseToolBuilder : public UPlacementToolBuilderBase
{
	GENERATED_BODY()

protected:
	virtual UPlacementBrushToolBase* FactoryToolInstance(UObject* Outer) const override;
};

UCLASS(MinimalAPI)
class UPlacementModeEraseTool : public UPlacementBrushToolBase
{
	GENERATED_BODY()
public:
	constexpr static TCHAR ToolName[] = TEXT("EraseTool");

	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;

protected:
	virtual void OnTick(float DeltaTime) override;
};
