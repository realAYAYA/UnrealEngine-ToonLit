// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "Tools/PlacementBrushToolBase.h"

#include "PlacementPlaceTool.generated.h"

UCLASS(Transient, MinimalAPI)
class UPlacementModePlacementToolBuilder : public UPlacementToolBuilderBase
{
	GENERATED_BODY()

public:
	virtual UPlacementBrushToolBase* FactoryToolInstance(UObject* Outer) const override;
};

UCLASS(MinimalAPI)
class UPlacementModePlacementTool : public UPlacementBrushToolBase
{
	GENERATED_BODY()
public:
	constexpr static TCHAR ToolName[] = TEXT("PlaceTool");


protected:
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual void OnTick(float DeltaTime) override;

	void GetRandomVectorInBrush(FVector& OutStart, FVector& OutEnd);
};
