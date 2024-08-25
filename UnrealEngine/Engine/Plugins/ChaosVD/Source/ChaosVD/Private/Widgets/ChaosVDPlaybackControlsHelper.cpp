// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/ChaosVDPlaybackControlsHelper.h"

#include "ChaosVDModule.h"
#include "ChaosVDPlaybackController.h"
#include "ChaosVDPlaybackControllerObserver.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SChaosVDTimelineWidget.h"

void Chaos::VisualDebugger::HandleUserPlaybackInputControl(EChaosVDPlaybackButtonsID ButtonClicked, const IChaosVDPlaybackControllerInstigator& Instigator, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController)
{
	if (const TSharedPtr<FChaosVDPlaybackController> PlaybackControllerPtr = InPlaybackController.Pin())
	{
		switch (ButtonClicked)
		{
			case EChaosVDPlaybackButtonsID::Play:
				{
					if (!ensure(PlaybackControllerPtr->AcquireExclusivePlaybackControls(Instigator)))
					{
						UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to aquire exclusive playback controls..."), ANSI_TO_TCHAR(__FUNCTION__));
						return;
					}
					PlaybackControllerPtr->RequestUnpause();
					break;
				}
			case EChaosVDPlaybackButtonsID::Pause:
				{
					if (!ensure(PlaybackControllerPtr->ReleaseExclusivePlaybackControls(Instigator)))
					{
						UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to release exclusive playback controls..."), ANSI_TO_TCHAR(__FUNCTION__));
						return;
					}
					PlaybackControllerPtr->RequestPause();
					break;
				}
			case EChaosVDPlaybackButtonsID::Stop:
				{
					if (!ensure(PlaybackControllerPtr->ReleaseExclusivePlaybackControls(Instigator)))
					{
						UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to release exclusive playback controls..."), ANSI_TO_TCHAR(__FUNCTION__));
						return;
					}
					PlaybackControllerPtr->RequestStop(Instigator);

					break;
				}
			default:
				break;
		}
	}
}
