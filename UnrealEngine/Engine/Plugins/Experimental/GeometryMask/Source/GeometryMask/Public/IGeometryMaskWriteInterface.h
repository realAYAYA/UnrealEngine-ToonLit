// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryMaskTypes.h"
#include "UObject/Interface.h"
#include "IGeometryMaskWriteInterface.generated.h"

class FCanvas;
struct FGeometryMaskWriteParameters;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UGeometryMaskWriteInterface : public UInterface
{
	GENERATED_BODY()
};

/** Implement to write to a host canvas. */
class IGeometryMaskWriteInterface
{
	GENERATED_BODY()

public:
	virtual const FGeometryMaskWriteParameters& GetParameters() const = 0;
	virtual void SetParameters(FGeometryMaskWriteParameters& InParameters) = 0;
	virtual void DrawToCanvas(FCanvas* InCanvas) = 0;
	virtual FOnGeometryMaskSetCanvasNativeDelegate& OnSetCanvas() = 0;
};
