// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Fonts/SlateFontInfo.h"

class FText;
class SWidget;
class UTexture2D;

/**
 * Helpers to build UI used by the Camera Calibration modules.
 */
class FCameraCalibrationWidgetHelpers
{
public:
	/** Builds a UI with a horizontal box with a lable on the left and the provided widget on the right */
	static TSharedRef<SWidget> BuildLabelWidgetPair(FText&& Text, TSharedRef<SWidget> Widget);

	/** Stores the default row height used throughout the Camera Calibration UI */
	static const int32 DefaultRowHeight;

	/** Displays a window with the given texture, preserving aspect ratio and almost full screen */
	static void DisplayTextureInWindowAlmostFullScreen(UTexture2D* Texture, FText&& Title, float ScreenMarginFactor = 0.85f);
};