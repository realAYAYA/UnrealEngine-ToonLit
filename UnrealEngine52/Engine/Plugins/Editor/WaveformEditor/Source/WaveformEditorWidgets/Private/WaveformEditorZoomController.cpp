// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorZoomController.h"

void FWaveformEditorZoomController::ZoomIn()
{
	if (CanZoomIn())
	{
		ZoomLevel = FMath::Clamp(ZoomLevel += ZoomLevelStep, 0, LogRatioBase);
		ApplyZoom();
	}

}

bool FWaveformEditorZoomController::CanZoomIn() const
{
	return ZoomLevel + ZoomLevelStep <= LogRatioBase + ZoomLevelInitValue;
}

void FWaveformEditorZoomController::ZoomOut()
{
	if (CanZoomOut())
	{
		ZoomLevel = FMath::Clamp(ZoomLevel -= ZoomLevelStep, 0, LogRatioBase);
		ApplyZoom();
	}
}

bool FWaveformEditorZoomController::CanZoomOut() const
{
	return ZoomLevel - ZoomLevelStep >= 0;
}


void FWaveformEditorZoomController::ZoomByDelta(const float Delta)
{
	if (Delta >= 0.f)
	{
		ZoomIn();
	}
	else
	{
		ZoomOut();
	}
}

float FWaveformEditorZoomController::GetZoomRatio() const
{
	return 1 - ConvertZoomLevelToLogRatio();
}

void FWaveformEditorZoomController::ApplyZoom()
{
	OnZoomRatioChanged.Broadcast(1 - ConvertZoomLevelToLogRatio());
}

float FWaveformEditorZoomController::ConvertZoomLevelToLogRatio() const
{
	return FMath::Clamp(FMath::LogX(LogRatioBase, ZoomLevel), 0.f, 1.f);
}
