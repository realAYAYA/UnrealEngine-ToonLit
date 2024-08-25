// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaPlayerEditorModule.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SCompoundWidget.h"

class SSlider;
class UMediaPlayer;

/**
 * Implements a scrubber to visualize the current playback position of a Media Player
 * and interact with it.
 */
class SMediaPlayerSlider : public IMediaPlayerSlider
{
public:
	SLATE_BEGIN_ARGS(SMediaPlayerSlider)
			: _Style(&FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider"))
		{
		}

	/** The Slider style used to draw the scrubber. */
	SLATE_STYLE_ARGUMENT(FSliderStyle, Style)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, UMediaPlayer* InMediaPlayer);

	//~Begin IMediaPlayerScrubber
	virtual void SetSliderHandleColor(const FSlateColor& InSliderColor) override;
	virtual void SetSliderBarColor(const FSlateColor& InSliderColor) override;
	virtual void SetVisibleWhenInactive(EVisibility InVisibility) override;
	//~End IMediaPlayerScrubber

private:
	bool DoesMediaPlayerSupportSeeking() const;
	void OnScrubBegin();
	void OnScrubEnd();
	void Seek(float InPlaybackPosition);
	float GetPlaybackPosition() const;
	EVisibility GetScrubberVisibility() const;

	/** The scrubber visibility when inactive. */
	EVisibility VisibilityWhenInactive = EVisibility::Hidden;

	/** Pointer to the media player that is being viewed. */
	TWeakObjectPtr<UMediaPlayer> MediaPlayerWeak;

	/** The playback rate prior to scrubbing. */
	float PreScrubRate = 0.0f;

	/** Holds the scrubber slider. */
	TSharedPtr<SSlider> ScrubberSlider;

	/** The value currently being scrubbed to. */
	float ScrubValue = 0.0f;
};
