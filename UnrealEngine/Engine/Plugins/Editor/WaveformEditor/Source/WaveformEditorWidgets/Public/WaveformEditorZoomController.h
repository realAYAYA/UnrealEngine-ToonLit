// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnZoomRatioChanged, const float /* New Zoom Ratio */);

class WAVEFORMEDITORWIDGETS_API FWaveformEditorZoomController
{
public:
	FWaveformEditorZoomController() = default;

	void ZoomIn();
	bool CanZoomIn() const;
	void ZoomOut();
	bool CanZoomOut() const;
	void ZoomByDelta(const float Delta);

	float GetZoomRatio() const;

	FOnZoomRatioChanged OnZoomRatioChanged;

private:
	void ApplyZoom();
	float ConvertZoomLevelToLogRatio() const;
	
	float ZoomLevelInitValue = 1.f;
	float ZoomLevel = ZoomLevelInitValue;
	float ZoomLevelStep = 2.f;
	const uint32 LogRatioBase = 100;
};