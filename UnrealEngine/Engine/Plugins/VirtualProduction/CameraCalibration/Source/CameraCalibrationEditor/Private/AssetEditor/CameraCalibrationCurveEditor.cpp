// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraCalibrationCurveEditor.h"


FCameraCalibrationCurveEditor::FCameraCalibrationCurveEditor()
	: FCurveEditor()
{
	//Disable zoom axis snapping behavior. 
	InputSnapEnabledAttribute = false;
}