// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDTimelineWidget.h"

#include "ChaosVDStyle.h"
#include "Input/Reply.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

void SChaosVDTimelineWidget::Construct(const FArguments& InArgs)
{
	MaxFrames = InArgs._MaxFrames;
	FrameChangedDelegate = InArgs._OnFrameChanged;
	FrameLockedDelegate = InArgs._OnFrameLockStateChanged;
	ButtonClickedDelegate = InArgs._OnButtonClicked;
	bAutoStopEnabled = InArgs._AutoStopEnabled;
	ElementVisibilityFlags = InArgs._ButtonVisibilityFlags;

	SetCanTick(false);

	ChildSlot
	[
		SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(4.0f,0.0f)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Raw(this, &SChaosVDTimelineWidget::GetElementVisibility, EChaosVDTimelineElementIDFlags::Play)
					.IsEnabled_Raw(this, &SChaosVDTimelineWidget::GetElementEnabled, EChaosVDTimelineElementIDFlags::Play)
					.OnClicked( FOnClicked::CreateRaw(this, &SChaosVDTimelineWidget::TogglePlay))
					.ContentPadding( 2.0f )
					.ForegroundColor( FSlateColor::UseForeground() )
					.IsFocusable( false )
					[
						SNew( SImage )
						.Image_Raw(this, &SChaosVDTimelineWidget::GetPlayOrPauseIcon)
						.DesiredSizeOverride(FVector2D(16.0f,16.0f))
						.ColorAndOpacity( FSlateColor::UseForeground() )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Raw(this, &SChaosVDTimelineWidget::GetElementVisibility, EChaosVDTimelineElementIDFlags::Stop)
					.IsEnabled_Raw(this, &SChaosVDTimelineWidget::GetElementEnabled, EChaosVDTimelineElementIDFlags::Stop)
					.OnClicked( FOnClicked::CreateRaw(this, &SChaosVDTimelineWidget::Stop))
					.ContentPadding( 2.0f )
					.ForegroundColor( FSlateColor::UseForeground() )
					.IsFocusable( false )
					[
						SNew( SImage )
						.Image(FChaosVDStyle::Get().GetBrush("StopIcon"))
						.DesiredSizeOverride(FVector2D(16.0f,16.0f))
						.ColorAndOpacity( FSlateColor::UseForeground() )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Raw(this, &SChaosVDTimelineWidget::GetElementVisibility, EChaosVDTimelineElementIDFlags::Prev)
					.IsEnabled_Raw(this, &SChaosVDTimelineWidget::GetElementEnabled, EChaosVDTimelineElementIDFlags::Prev)
					.OnClicked( FOnClicked::CreateRaw(this, &SChaosVDTimelineWidget::Prev))
					.ContentPadding( 2.0f )
					.ForegroundColor( FSlateColor::UseForeground() )
					.IsFocusable( false )
					[
						SNew( SImage )
						.DesiredSizeOverride(FVector2D(16.0f,16.0f))
						.Image(FChaosVDStyle::Get().GetBrush("PrevIcon"))
						.ColorAndOpacity( FSlateColor::UseForeground() )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Raw(this, &SChaosVDTimelineWidget::GetElementVisibility, EChaosVDTimelineElementIDFlags::Next)
					.IsEnabled_Raw(this, &SChaosVDTimelineWidget::GetElementEnabled, EChaosVDTimelineElementIDFlags::Next)
					.OnClicked( FOnClicked::CreateRaw(this, &SChaosVDTimelineWidget::Next))
					.ContentPadding( 2.0f )
					.ForegroundColor( FSlateColor::UseForeground() )
					.IsFocusable( false )
					[
						SNew( SImage )
						.DesiredSizeOverride(FVector2D(16.0f,16.0f))
						.Image(FChaosVDStyle::Get().GetBrush("NextIcon"))
						.ColorAndOpacity( FSlateColor::UseForeground() )
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Visibility_Raw(this, &SChaosVDTimelineWidget::GetElementVisibility, EChaosVDTimelineElementIDFlags::Lock)
					.IsEnabled_Raw(this, &SChaosVDTimelineWidget::GetElementEnabled, EChaosVDTimelineElementIDFlags::Lock)
					.OnClicked( FOnClicked::CreateRaw(this, &SChaosVDTimelineWidget::ToggleLockState))
					.ContentPadding( 2.0f )
					.ForegroundColor( FSlateColor::UseForeground())
					.IsFocusable( false )
					[
						SNew( SImage )
						.Image_Raw(this, &SChaosVDTimelineWidget::GetLockStateIcon)
						.DesiredSizeOverride(FVector2D(16.0f,16.0f))
						.ColorAndOpacity( FSlateColor::UseForeground() )
					]
				]
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(4.0f,0.0f)
			.FillWidth(1.0f)
			[
			  SAssignNew(TimelineSlider, SSlider)
			  .Visibility_Raw(this, &SChaosVDTimelineWidget::GetElementVisibility, EChaosVDTimelineElementIDFlags::Timeline)
			  //TODO: Enable locking when we have custom images for the slider elements. Currently locking it messes up with the transparency/images
			  //.Locked_Raw(this, &SChaosVDTimelineWidget::GetElementEnabled, EChaosVDTimelineElementIDFlags::Timeline)
			  .ToolTipText_Lambda([this]()-> FText{ return FText::AsNumber(CurrentFrame); })
			  .Value(CurrentFrame)
			  .OnValueChanged_Raw(this, &SChaosVDTimelineWidget::SetCurrentTimelineFrame, EChaosVDSetTimelineFrameFlags::BroadcastChange)
			  .StepSize(1)
			  .MaxValue(MaxFrames)
			  .MinValue(0)
			]
			+SHorizontalBox::Slot()
			.Padding(4.0f,0.0f)
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::White)
				.Text_Lambda([this]()->FText{ return FText::Format(LOCTEXT("FramesCounter","{0} / {1}"), CurrentFrame, MaxFrames);})
			]
	];
}

void SChaosVDTimelineWidget::UpdateMinMaxValue(float NewMin, float NewMax)
{
	if (!TimelineSlider.IsValid())
	{
		return;
	}

	TimelineSlider->SetMinAndMaxValues(NewMin, NewMax);

	MinFrames = NewMin;
	MaxFrames = NewMax;
	
	if (CurrentFrame < NewMin || CurrentFrame > NewMax)
	{
		CurrentFrame = NewMin;
	}
}

void SChaosVDTimelineWidget::SetTargetFrameTime(float TargetFrameTimeSeconds)
{
	if (TargetFrameTimeSeconds > 0)
	{
		CurrentPlaybackRate = TargetFrameTimeSeconds; 
	}
	else
	{
		// Default to 60 FPS
		CurrentPlaybackRate = 1 / 60.0f;
	}
}

void SChaosVDTimelineWidget::ResetTimeline()
{
	TimelineSlider->SetValue(MinFrames);
	CurrentFrame = MinFrames;
}

void SChaosVDTimelineWidget::SetCurrentTimelineFrame(float FrameNumber, EChaosVDSetTimelineFrameFlags Options)
{
	CurrentFrame = static_cast<int32>(FrameNumber);

	if (TimelineSlider.IsValid())
	{
		TimelineSlider->SetValue(CurrentFrame);
		if (EnumHasAnyFlags(Options, EChaosVDSetTimelineFrameFlags::BroadcastChange))
		{
			FrameChangedDelegate.ExecuteIfBound(CurrentFrame);
		}
	}
}

void SChaosVDTimelineWidget::SetIsLocked(bool NewIsLocked)
{
	bIsLocked = NewIsLocked;

	if (bIsLocked)
	{
		ElementEnabledFlags = static_cast<uint16>(EChaosVDTimelineElementIDFlags::Lock);	
	}
	else
	{
		ElementEnabledFlags = DefaultEnabledElementsFlags;
	}
}

FReply SChaosVDTimelineWidget::TogglePlay()
{
	if (bIsPlaying)
	{
		Pause();
		ButtonClickedDelegate.ExecuteIfBound(EChaosVDPlaybackButtonsID::Pause);
	}
	else
	{
		Play();
		ButtonClickedDelegate.ExecuteIfBound(EChaosVDPlaybackButtonsID::Play);
	}

	return FReply::Handled();
}

void SChaosVDTimelineWidget::Play()
{
	bIsPlaying = true;
	SetCanTick(true);
}

FReply SChaosVDTimelineWidget::Stop()
{
	CurrentFrame = 0;
	CurrentPlaybackTime = 0.0f;
	bIsPlaying = false;

	SetCurrentTimelineFrame(CurrentFrame);

	SetCanTick(false);

	ButtonClickedDelegate.ExecuteIfBound(EChaosVDPlaybackButtonsID::Stop);

	return FReply::Handled();
}

void SChaosVDTimelineWidget::Pause()
{
	bIsPlaying = false;
	SetCanTick(false);
}

FReply SChaosVDTimelineWidget::Next()
{
	if (CurrentFrame >= MaxFrames)
	{
		CurrentFrame = MaxFrames;
		return FReply::Handled();
	}

	CurrentFrame++;
	
	SetCurrentTimelineFrame(CurrentFrame);

	ButtonClickedDelegate.ExecuteIfBound(EChaosVDPlaybackButtonsID::Next);

	return FReply::Handled();
}

FReply SChaosVDTimelineWidget::Prev()
{
	if (CurrentFrame == 0)
	{
		return FReply::Handled();
	}

	CurrentFrame--;

	SetCurrentTimelineFrame(CurrentFrame);
	
	ButtonClickedDelegate.ExecuteIfBound(EChaosVDPlaybackButtonsID::Prev);

	return FReply::Handled();
}

FReply SChaosVDTimelineWidget::ToggleLockState()
{
	SetIsLocked(!bIsLocked);

	FrameLockedDelegate.ExecuteIfBound(bIsLocked);

	return FReply::Handled();
}

const FSlateBrush* SChaosVDTimelineWidget::GetPlayOrPauseIcon() const
{
	return bIsPlaying ? FChaosVDStyle::Get().GetBrush("PauseIcon") : FChaosVDStyle::Get().GetBrush("PlayIcon");
}

const FSlateBrush* SChaosVDTimelineWidget::GetLockStateIcon() const
{
	return bIsLocked ? FChaosVDStyle::Get().GetBrush("LockIcon") : FChaosVDStyle::Get().GetBrush("UnlockedIcon");
}

EVisibility SChaosVDTimelineWidget::GetElementVisibility(EChaosVDTimelineElementIDFlags ElementID) const
{
	 return ((static_cast<uint16>(ElementID) & ElementVisibilityFlags) == static_cast<uint16>(ElementID)) ? EVisibility::Visible : EVisibility::Collapsed;
}

bool SChaosVDTimelineWidget::GetElementEnabled(EChaosVDTimelineElementIDFlags ElementID) const
{
	return ((static_cast<uint16>(ElementID) & ElementEnabledFlags) == static_cast<uint16>(ElementID));
}

void SChaosVDTimelineWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	//TODO: We should move the Ticking logic to advance frames outside of this widget.
	// The logic to update the visual state is already controlled externally
	if (bIsPlaying)
	{
		if (CurrentFrame == MaxFrames)
		{
			if (bAutoStopEnabled)
			{
				Stop();
			}
		}

		CurrentPlaybackTime += InDeltaTime;

		while (CurrentPlaybackTime > CurrentPlaybackRate)
		{
			CurrentPlaybackTime -= CurrentPlaybackRate;
			Next();
		}
	}
}

#undef LOCTEXT_NAMESPACE
