// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "HAL/Platform.h"
#include "Templates/SharedPointer.h"

class FChaosVDPlaybackController;
class IChaosVDPlaybackControllerInstigator;
enum class EChaosVDPlaybackButtonsID : uint8;

namespace Chaos::VisualDebugger
{
	/* Helper that allows execute the same behavior from different playback control objects for common Playback UI actions **/
	void HandleUserPlaybackInputControl(EChaosVDPlaybackButtonsID ButtonClicked, const IChaosVDPlaybackControllerInstigator& Instigator, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController);
}

