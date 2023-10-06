// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/ISlateStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UMediaPlayer;
class UMediaSoundComponent;
class UMediaTexture;


class SMediaPlayerEditorViewport
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SMediaPlayerEditorViewport) { }
	SLATE_END_ARGS()

public:

	/** Default constructor. */
	SMediaPlayerEditorViewport();

public:

	/**
	 * Construct this widget
	 *
	 * @param InArgs The declaration data for this widget.
	 * @param InMediaPlayer The UMediaPlayer asset to show the details for.
	 * @param InMediaTexture The UMediaTexture asset to output video to. If nullptr then use our own.
	 * @param InStyleSet The style set to use.
	 * @param bInIsSoundEnabled If true then produce sound.
	 */
	void Construct(const FArguments& InArgs, UMediaPlayer& InMediaPlayer,
		UMediaTexture* InMediaTexture, const TSharedRef<ISlateStyle>& InStyle,
		bool bInIsSoundEnabled);

	/**
	 * Enables/disables using the mouse to control the viewport.
	 */
	void EnableMouseControl(bool bIsEnabled) { bIsMouseControlEnabled = bIsEnabled; }

public:

	//~ SWidget interface

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:

	/** Callback for getting the text of the player name overlay. */
	FText HandleMediaPlayerNameText() const;

	/** Callback for getting the text of the playback state overlay. */
	FText HandleMediaPlayerStateText() const;

	/** Callback for getting the text of the media source name overlay. */
	FText HandleMediaSourceNameText() const;

	/** Callback for getting the text of the notification overlay. */
	FText HandleNotificationText() const;

	/** Callback for getting the text of the view settings overlay. */
	FText HandleViewSettingsText() const;

private:

	/** Pointer to the media player that is being viewed. */
	UMediaPlayer* MediaPlayer;

	/** The style set to use for this widget. */
	TSharedPtr<ISlateStyle> Style;
	
	/** True if the mouse can control things. */
	bool bIsMouseControlEnabled;
};