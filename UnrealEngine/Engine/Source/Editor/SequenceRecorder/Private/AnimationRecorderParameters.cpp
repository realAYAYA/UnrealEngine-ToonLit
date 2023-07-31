// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationRecorderParameters.h"

float UAnimationRecordingParameters::GetRecordingDurationSeconds()
{
	return bEndAfterDuration ? MaximumDurationSeconds : FAnimationRecordingSettings::UnboundedMaximumLength;
}

const FFrameRate& UAnimationRecordingParameters::GetRecordingFrameRate()
{
	return SampleFrameRate;
}
