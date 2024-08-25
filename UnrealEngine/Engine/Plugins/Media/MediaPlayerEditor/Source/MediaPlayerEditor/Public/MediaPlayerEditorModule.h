// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FImgMediaPlayer;
class ISlateStyle;
class UMediaPlayer;

/**
 * Interface for MediaPlayer Playback Scrubber Widget
 */
class MEDIAPLAYEREDITOR_API IMediaPlayerSlider : public SCompoundWidget
{
public:
	/** Set the Scrubber Slider Handle Color */
	virtual void SetSliderHandleColor(const FSlateColor& InSliderColor) = 0;

	/** Set the Scrubber Slider Color */
	virtual void SetSliderBarColor(const FSlateColor& InSliderColor) = 0;

	/** Set the Scrubber Slider Visibility, when the player is inactive */
	virtual void SetVisibleWhenInactive(EVisibility InVisibility) = 0;
};

/**
* Interface for the MediaPlayerEditor module.
*/
class MEDIAPLAYEREDITOR_API IMediaPlayerEditorModule
	: public IModuleInterface
{
public:

	/** Get the style used by this module. */
	virtual TSharedPtr<ISlateStyle> GetStyle() = 0;

	/**
	 * Creates a Widget to visualize playback time and scrub the content played by a Media Player
	 * @param InMediaPlayer: the player affected by the widget
	 * @param InStyle: the style chosen for this slider widget
	 * @return Scrubber Widget
	 */
	virtual TSharedRef<IMediaPlayerSlider> CreateMediaPlayerSliderWidget(UMediaPlayer* InMediaPlayer, const FSliderStyle& InStyle = FCoreStyle::Get().GetWidgetStyle<FSliderStyle>("Slider")) = 0;
};
