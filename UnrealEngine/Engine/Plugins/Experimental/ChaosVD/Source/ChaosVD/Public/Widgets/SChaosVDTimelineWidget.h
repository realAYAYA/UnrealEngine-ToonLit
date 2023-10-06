// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class SSlider;
class FReply;
struct FSlateBrush;

DECLARE_DELEGATE_OneParam(FChaosVDFrameChangedDelegate, int32)
DECLARE_DELEGATE_OneParam(FChaosVDFrameLockStateDelegate, bool)

enum class EChaosVDSetTimelineFrameFlags
{
	None = 0,
	BroadcastChange = 1 << 0,
	Silent = 1 << 1,
};
ENUM_CLASS_FLAGS(EChaosVDSetTimelineFrameFlags)


/** Simple timeline control widget */
class SChaosVDTimelineWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SChaosVDTimelineWidget ){}
		SLATE_ARGUMENT(int32, MaxFrames)
		SLATE_EVENT(FChaosVDFrameChangedDelegate, OnFrameChanged)
		SLATE_EVENT(FChaosVDFrameLockStateDelegate, OnFrameLockStateChanged)
		SLATE_ATTRIBUTE( bool, HidePlayStopButtons)
		SLATE_ATTRIBUTE( bool, HideNextPrevButtons)
		SLATE_ATTRIBUTE( bool, HideLockButton)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void UpdateMinMaxValue(float NewMin, float NewMax);

	void SetCurrentTimelineFrame(float FrameNumber, EChaosVDSetTimelineFrameFlags Options = EChaosVDSetTimelineFrameFlags::BroadcastChange);

	void SetIsLocked(bool NewIsLocked) { bIsLocked = NewIsLocked; }

	bool IsUnlocked() const { return !bIsLocked; }

	/** Brings back the state of the timeline to its original state*/
	void ResetTimeline();

	/** Called when a new frame is manually selected or auto-updated during playback */
	FChaosVDFrameChangedDelegate& OnFrameChanged() { return FrameChangedDelegate; }
	
	/** Called when this timeline is locked or unlocked */
	FChaosVDFrameLockStateDelegate& OnFrameLockStateChanged() { return FrameLockedDelegate; }

protected:

	FReply  Play();
	FReply  Stop();
	FReply  Next();
	FReply  Prev();

	FReply ToggleLockState();

	const FSlateBrush* GetPlayOrPauseIcon() const;
	const FSlateBrush* GetLockStateIcon() const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TSharedPtr<SSlider> TimelineSlider;

	int32 CurrentFrame = 0;
	int32 MinFrames = 0;
	int32 MaxFrames = 1000;

	bool bIsPlaying = false;
	float CurrentPlaybackTime = 0.0f;

	FChaosVDFrameChangedDelegate FrameChangedDelegate;
	FChaosVDFrameLockStateDelegate FrameLockedDelegate;

	bool bIsLocked = false;
};
