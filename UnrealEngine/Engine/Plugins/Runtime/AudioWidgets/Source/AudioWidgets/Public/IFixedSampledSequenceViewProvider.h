// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/Range.h"
#include "FixedSampledSequenceView.h"

class IFixedSampledSequenceViewProvider
{
public:
	virtual ~IFixedSampledSequenceViewProvider() = default;
	virtual FFixedSampledSequenceView RequestSequenceView(const TRange<double> DataRatioRange) = 0;
};