// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Vector4.h"
#include "RendererInterface.h"

class FManyLightsViewState
{
public:
	TRefCountPtr<IPooledRenderTarget> DiffuseLightingAndSecondMomentHistory;
	TRefCountPtr<IPooledRenderTarget> SpecularLightingAndSecondMomentHistory;
	TRefCountPtr<IPooledRenderTarget> SceneDepthHistory;
	TRefCountPtr<IPooledRenderTarget> NumFramesAccumulatedHistory;

	FVector4f HistoryScreenPositionScaleBias = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
	FVector4f HistoryUVMinMax = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);

	void SafeRelease()
	{
		DiffuseLightingAndSecondMomentHistory.SafeRelease();
		SpecularLightingAndSecondMomentHistory.SafeRelease();
		SceneDepthHistory.SafeRelease();
		NumFramesAccumulatedHistory.SafeRelease();
	}
};