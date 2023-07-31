// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CurveEditor.h"

/**
 * Custom Curve editor child class
 */
class FCameraCalibrationCurveEditor : public FCurveEditor
{
public:
	FCameraCalibrationCurveEditor();
	
public:
	/** Delegate instance when new Data Point added */
	FSimpleDelegate OnAddDataPointDelegate;
};

