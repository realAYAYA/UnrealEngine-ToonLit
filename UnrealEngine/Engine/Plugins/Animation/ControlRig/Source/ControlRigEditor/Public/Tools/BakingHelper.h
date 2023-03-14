// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameNumber.h"
#include "Templates/SharedPointer.h"
#include "Math/Transform.h"


class ISequencer;
class UControlRig;
struct FFrameRate;
enum class EMovieSceneTransformChannel : uint32;

/**
 * FBakingHelper
 */

struct FBakingHelper
{
	/** Returns the current sequencer. */
	static TWeakPtr<ISequencer> GetSequencer();
};
