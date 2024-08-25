// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class SSlider;
class FReply;
struct FSlateBrush;

enum class EChaosVDPlaybackButtonsID : uint8
{
	Play,
	Pause,
	Stop,
	Next,
	Prev
};

DECLARE_DELEGATE_OneParam(FChaosControlButtonClicked, EChaosVDPlaybackButtonsID)
DECLARE_DELEGATE_OneParam(FChaosVDFrameChangedDelegate, int32)
DECLARE_DELEGATE_OneParam(FChaosVDFrameLockStateDelegate, bool)

enum class EChaosVDSetTimelineFrameFlags
{
	None = 0,
	BroadcastChange = 1 << 0,
	Silent = 1 << 1,
};
ENUM_CLASS_FLAGS(EChaosVDSetTimelineFrameFlags)

enum class EChaosVDTimelineElementIDFlags : uint16
{
	None = 0,
	Play = 1 << 0,
	Stop = 1 << 1,
	Next = 1 << 2,
	Prev = 1 << 3,
	Lock = 1 << 4,
	Timeline = 1 << 5,
	
	ManualSteppingButtons = Next | Prev,
	AllManualStepping = Next | Prev | Timeline,
	AllPlaybackButtons = Play | Stop | Next | Prev,
	AllPlayback = Play | Stop | Next | Prev | Timeline,
	All = Play | Stop | Next | Prev | Timeline | Lock,
};
ENUM_CLASS_FLAGS(EChaosVDTimelineElementIDFlags)

/** Simple timeline control widget */
class SChaosVDTimelineWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SChaosVDTimelineWidget ){}
		SLATE_ARGUMENT(int32, MaxFrames)
		SLATE_ARGUMENT(uint16, ButtonVisibilityFlags)
		SLATE_ARGUMENT(bool, AutoStopEnabled)
		SLATE_EVENT(FChaosVDFrameChangedDelegate, OnFrameChanged)
		SLATE_EVENT(FChaosVDFrameLockStateDelegate, OnFrameLockStateChanged)
		SLATE_EVENT(FChaosControlButtonClicked, OnButtonClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void UpdateMinMaxValue(float NewMin, float NewMax);

	void SetCurrentTimelineFrame(float FrameNumber, EChaosVDSetTimelineFrameFlags Options = EChaosVDSetTimelineFrameFlags::BroadcastChange);

	void SetIsLocked(bool NewIsLocked);

	bool IsUnlocked() const { return !bIsLocked; }

	void SetTargetFrameTime(float TargetFrameTimeSeconds);

	/** Brings back the state of the timeline to its original state*/
	void ResetTimeline();

	/** Called when a new frame is manually selected or auto-updated during playback */
	FChaosVDFrameChangedDelegate& OnFrameChanged() { return FrameChangedDelegate; }
	
	/** Called when this timeline is locked or unlocked */
	FChaosVDFrameLockStateDelegate& OnFrameLockStateChanged() { return FrameLockedDelegate; }

	uint16& GetCurrentElementVisibilityFlags() { return ElementVisibilityFlags; }
	void SetCurrentElementVisibilityFlags(uint16 NewVisibilityFlags) { ElementVisibilityFlags = NewVisibilityFlags; }

	uint16& GetMutableElementEnabledFlagsRef() { return ElementEnabledFlags; }

	void SetAutoStopEnabled(bool bNewEnabled) { bAutoStopEnabled = bNewEnabled; }

	int32 GetCurrentFrame() const { return CurrentFrame; }

	void Play();
	FReply  Stop();

protected:

	void Pause();
	FReply  TogglePlay();
	FReply  Next();
	FReply  Prev();

	FReply ToggleLockState();

	const FSlateBrush* GetPlayOrPauseIcon() const;
	const FSlateBrush* GetLockStateIcon() const;

	EVisibility GetElementVisibility(EChaosVDTimelineElementIDFlags ElementID) const;
	bool GetElementEnabled(EChaosVDTimelineElementIDFlags ElementID) const;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	TSharedPtr<SSlider> TimelineSlider;

	int32 CurrentFrame = 0;
	int32 MinFrames = 0;
	int32 MaxFrames = 1000;

	bool bIsPlaying = false;
	float CurrentPlaybackTime = 0.0f;
	float CurrentPlaybackRate = 1.0f / 60.0f;

	FChaosVDFrameChangedDelegate FrameChangedDelegate;
	FChaosVDFrameLockStateDelegate FrameLockedDelegate;
	FChaosControlButtonClicked ButtonClickedDelegate;

	bool bIsLocked = false;
	bool bAutoStopEnabled = false;

	uint16 ElementVisibilityFlags = 0;
	uint16 ElementEnabledFlags = static_cast<uint16>(EChaosVDTimelineElementIDFlags::All);
	uint16 DefaultEnabledElementsFlags = static_cast<uint16>(EChaosVDTimelineElementIDFlags::All);
	
};
