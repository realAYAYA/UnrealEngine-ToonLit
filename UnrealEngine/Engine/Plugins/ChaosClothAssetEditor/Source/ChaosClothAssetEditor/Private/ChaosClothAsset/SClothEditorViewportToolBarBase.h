// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCommonEditorViewportToolbarBase.h"

/// Common code for toolbars in the Cloth Editor
class CHAOSCLOTHASSETEDITOR_API SChaosClothAssetEditorViewportToolBarBase : public SCommonEditorViewportToolbarBase
{
protected:
	TSharedRef<SWidget> GenerateClothViewportOptionsMenu() const;
	TSharedRef<SWidget> GenerateFOVMenu() const;
	TSharedRef<SWidget> GenerateCameraSpeedSettingsMenu() const;
	float OnGetFOVValue() const;
	FText GetCameraSpeedLabel() const;
	float GetCamSpeedSliderPosition() const;
	void OnSetCamSpeed(float NewValue) const;
	FText GetCameraSpeedScalarLabel() const;
	float GetCamSpeedScalarBoxValue() const;
	void OnSetCamSpeedScalarBoxValue(float NewValue) const;
};

