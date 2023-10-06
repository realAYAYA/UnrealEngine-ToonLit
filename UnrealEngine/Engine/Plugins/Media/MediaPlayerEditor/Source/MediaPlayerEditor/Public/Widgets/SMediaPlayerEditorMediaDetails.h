// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class STextBlock;
class UMediaPlayer;
class UMediaTexture;;

/**
 * Implements the media details panel of the MediaPlayer asset editor.
 */
class SMediaPlayerEditorMediaDetails
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorMediaDetails) { }
	SLATE_END_ARGS()

public:

	/**
	 * Construct this widget.
	 *
	 * @param InArgs			The declaration data for this widget.
	 * @param InMediaPlayer		The MediaPlayer to show the details for.
	 * @param InMediaTexture	The MediaTexture to show the details for.
	 * 
	 */
	void Construct(const FArguments& InArgs, UMediaPlayer* InMediaPlate, UMediaTexture* InMediaTexture);

	//~ SWidget interface
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:

	/**
	 * Updates our widgets to reflect the current state.
	 */
	void UpdateDetails();

	/** Pointer to the MediaPlayer that is being viewed. */
	TWeakObjectPtr<UMediaPlayer> MediaPlayer;
	/** Pointer to the MediaTexture that is being viewed. */
	TWeakObjectPtr<UMediaTexture> MediaTexture;

	/** Our widgets. */
	TSharedPtr<STextBlock> FormatText;
	TSharedPtr<STextBlock> FrameRateText;
	TSharedPtr<STextBlock> LODBiasText;
	TSharedPtr<STextBlock> MethodText;
	TSharedPtr<STextBlock> NumMipsText;
	TSharedPtr<STextBlock> NumTilesText;
	TSharedPtr<STextBlock> ResolutionText;
	TSharedPtr<STextBlock> ResourceSizeText;
};
