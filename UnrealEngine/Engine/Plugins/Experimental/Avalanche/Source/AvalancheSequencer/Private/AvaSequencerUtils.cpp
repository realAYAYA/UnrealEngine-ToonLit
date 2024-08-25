// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequencerUtils.h"
#include "Playback/AvaSequencerController.h"
#include "Templates/SharedPointer.h"

TSharedRef<IAvaSequencerController> FAvaSequencerUtils::CreateSequencerController()
{
	return MakeShared<FAvaSequencerController>();
}
