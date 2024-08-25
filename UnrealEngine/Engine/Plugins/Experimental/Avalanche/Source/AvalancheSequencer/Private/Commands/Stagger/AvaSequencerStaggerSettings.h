// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameTime.h"

struct FAvaSequencerStaggerSettings
{
	enum EOperationPoint : uint8
	{
		Start = 0,
		End = 1
	};
	EOperationPoint OperationPoint = EOperationPoint::End;

	enum EStartPosition : uint8
	{
		FirstSelected = 0,
		FirstInTimeline = 1
	};
	EStartPosition StartPosition = EStartPosition::FirstSelected;

	FFrameTime Shift;
};
