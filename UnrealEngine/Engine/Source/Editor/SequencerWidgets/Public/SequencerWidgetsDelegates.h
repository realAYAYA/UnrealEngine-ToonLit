// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Math/Range.h"
#include "ViewRangeInterpolation.h"

struct FFrameTime;
struct FFrameNumber;
struct FMovieSceneSequenceID;
enum class ENearestKeyOption : uint8;

DECLARE_DELEGATE_ThreeParams(FOnScrubPositionChanged, FFrameTime, bool, bool)
DECLARE_DELEGATE_TwoParams(FOnViewRangeChanged, TRange<double>, EViewRangeInterpolation)
DECLARE_DELEGATE_OneParam(FOnTimeRangeChanged, TRange<double>)
DECLARE_DELEGATE_OneParam(FOnFrameRangeChanged, TRange<FFrameNumber>)
DECLARE_DELEGATE_TwoParams(FOnSetMarkedFrame, int32, FFrameNumber)
DECLARE_DELEGATE_OneParam(FOnAddMarkedFrame, FFrameNumber)
DECLARE_DELEGATE_OneParam(FOnDeleteMarkedFrame, int32)
DECLARE_DELEGATE_RetVal_TwoParams(FFrameNumber, FOnGetNearestKey, FFrameTime, ENearestKeyOption)
DECLARE_DELEGATE_OneParam(FOnScrubPositionParentChanged, FMovieSceneSequenceID)
