// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMediaPlayerSlider.h"
#include "MediaPlayer.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SSlider.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMediaPlayerSlider::Construct(const FArguments& InArgs, UMediaPlayer* InMediaPlayer)
{
	MediaPlayerWeak = InMediaPlayer;

	ChildSlot
	[
		SAssignNew(ScrubberSlider, SSlider)
		.IsEnabled_Raw(this, &SMediaPlayerSlider::DoesMediaPlayerSupportSeeking)
		.OnMouseCaptureBegin_Raw(this, &SMediaPlayerSlider::OnScrubBegin)
		.OnMouseCaptureEnd_Raw(this, &SMediaPlayerSlider::OnScrubEnd)
		.OnValueChanged_Raw(this, &SMediaPlayerSlider::Seek)
		.Value_Raw(this, &SMediaPlayerSlider::GetPlaybackPosition)
		.Visibility_Raw(this, &SMediaPlayerSlider::GetScrubberVisibility)
		.Orientation(Orient_Horizontal)
		.SliderBarColor(FLinearColor::Transparent)
		.Style(InArgs._Style)
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

bool SMediaPlayerSlider::DoesMediaPlayerSupportSeeking() const
{
	if (const UMediaPlayer* MediaPlayer = MediaPlayerWeak.Get())
	{
		return MediaPlayer->SupportsSeeking();
	}

	return false;
}

void SMediaPlayerSlider::OnScrubBegin()
{
	if (UMediaPlayer* MediaPlayer = MediaPlayerWeak.Get())
	{
		ScrubValue = static_cast<float>(FTimespan::Ratio(MediaPlayer->GetDisplayTime(), MediaPlayer->GetDuration()));

		if (MediaPlayer->SupportsScrubbing())
		{
			PreScrubRate = MediaPlayer->GetRate();
			MediaPlayer->SetRate(0.0f);
		}
	}
}

void SMediaPlayerSlider::OnScrubEnd()
{
	if (UMediaPlayer* MediaPlayer = MediaPlayerWeak.Get())
	{
		if (MediaPlayer->SupportsScrubbing())
		{
			MediaPlayer->SetRate(PreScrubRate);
		}

		// Set playback position to scrub value when drag ends
		MediaPlayer->Seek(MediaPlayer->GetDuration() * ScrubValue);
	}
}

void SMediaPlayerSlider::Seek(float InPlaybackPosition)
{
	if (UMediaPlayer* MediaPlayer = MediaPlayerWeak.Get())
	{
		ScrubValue = InPlaybackPosition;

		if (!ScrubberSlider->HasMouseCapture() || MediaPlayer->SupportsScrubbing())
		{
			MediaPlayer->Seek(MediaPlayer->GetDuration() * InPlaybackPosition);
		}
	}
}

float SMediaPlayerSlider::GetPlaybackPosition() const
{
	if (const UMediaPlayer* MediaPlayer = MediaPlayerWeak.Get())
	{
		if (ScrubberSlider->HasMouseCapture())
		{
			return ScrubValue;
		}

		return (MediaPlayer->GetDuration() > FTimespan::Zero())
			? static_cast<float>(FTimespan::Ratio(MediaPlayer->GetDisplayTime(), MediaPlayer->GetDuration()))
			: 0.0f;
	}

	return 0.0f;
}

EVisibility SMediaPlayerSlider::GetScrubberVisibility() const
{
	if (const UMediaPlayer* MediaPlayer = MediaPlayerWeak.Get())
	{
		return (MediaPlayer->SupportsScrubbing() || MediaPlayer->SupportsSeeking())
															? EVisibility::Visible
															: VisibilityWhenInactive;
	}

	return VisibilityWhenInactive;
}

void SMediaPlayerSlider::SetSliderHandleColor(const FSlateColor& InSliderColor)
{
	if (ScrubberSlider)
	{
		ScrubberSlider->SetSliderHandleColor(InSliderColor);
	}
}

void SMediaPlayerSlider::SetSliderBarColor(const FSlateColor& InSliderColor)
{
	if (ScrubberSlider)
	{
		ScrubberSlider->SetSliderBarColor(InSliderColor);
	}
}

void SMediaPlayerSlider::SetVisibleWhenInactive(EVisibility InVisibility)
{
	VisibilityWhenInactive = InVisibility;
}
