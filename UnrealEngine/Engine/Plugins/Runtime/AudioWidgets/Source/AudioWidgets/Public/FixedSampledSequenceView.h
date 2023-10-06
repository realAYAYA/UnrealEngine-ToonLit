// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"

struct FFixedSampledSequenceView
{
	TArrayView<const float> SampleData;
	uint32 NumDimensions = 0;
	uint32 SampleRate = 0;
};