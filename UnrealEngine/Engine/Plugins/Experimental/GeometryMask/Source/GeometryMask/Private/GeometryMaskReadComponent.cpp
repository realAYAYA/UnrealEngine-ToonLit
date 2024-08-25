// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryMaskReadComponent.h"

#include "Engine/CanvasRenderTarget2D.h"
#include "GeometryMaskCanvas.h"

void UGeometryMaskReadComponent::SetParameters(FGeometryMaskReadParameters& InParameters)
{
	Parameters = InParameters;
	TryResolveCanvas();
}

bool UGeometryMaskReadComponent::TryResolveCanvas()
{
	const bool bResult = TryResolveNamedCanvas(Parameters.CanvasName);
	if (bResult)
	{
		Parameters.ColorChannel = CanvasWeak.Get()->GetColorChannel();
	}
	
	return bResult;
}
