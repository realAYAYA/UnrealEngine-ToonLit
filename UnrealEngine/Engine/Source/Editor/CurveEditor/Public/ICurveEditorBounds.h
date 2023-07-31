// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

struct ICurveEditorBounds
{
	virtual ~ICurveEditorBounds(){}

	virtual void GetInputBounds(double& OutMin, double& OutMax) const = 0;
	virtual void SetInputBounds(double InMin, double InMax) = 0;
};


struct FStaticCurveEditorBounds : ICurveEditorBounds
{
	double InputMin = 0.0;
	double InputMax = 1.0;

	virtual void GetInputBounds(double& OutMin, double& OutMax) const override final
	{
		OutMin = InputMin;
		OutMax = InputMax;
	}

	virtual void SetInputBounds(double InMin, double InMax) override final
	{
		InputMin = InMin;
		InputMax = InMax;
	}
};