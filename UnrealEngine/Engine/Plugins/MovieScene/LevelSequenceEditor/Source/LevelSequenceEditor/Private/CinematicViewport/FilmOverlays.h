// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/UICommandList.h"
#include "Widgets/SCompoundWidget.h"
#include "IFilmOverlay.h"

class FPaintArgs;
class FSlateWindowElementList;
struct FSlateBrush;


/** A widget that sits on top of a viewport, and draws custom content */
class SFilmOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFilmOverlay){}

		/** User provided array of overlays to draw */
		SLATE_ATTRIBUTE(TArray<IFilmOverlay*>, FilmOverlays)

	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs);

	/** Paint this widget */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Attribute used once per frame to retrieve the film overlays to paint */
	TAttribute<TArray<IFilmOverlay*>> FilmOverlays;
};

/** A custom widget that comprises a combo box displaying all available overlay options */
class SFilmOverlayOptions : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFilmOverlayOptions){}
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs);

	/** Retrieve the actual overlay widget that this widget controls. Can be positioned in any other widget hierarchy. */
	TSharedRef<SFilmOverlay> GetFilmOverlayWidget() const;

	/** Bind commands for the overlays */
	void BindCommands(TSharedRef<FUICommandList>);

private:

	/** Generate menu content for the combo button */
	TSharedRef<SWidget> GetMenuContent();

	/** Construct the part of the menu that defines the set of film overlays */
	TSharedRef<SWidget> ConstructPrimaryOverlaysMenu();

	/** Construct the part of the menu that defines the set of toggleable overlays (currently just safe-frames) */
	TSharedRef<SWidget> ConstructToggleableOverlaysMenu();

private:

	/** Get the thumbnail to be displayed on the combo box button */
	const FSlateBrush* GetCurrentThumbnail() const;

	/** Get the primary film overlay ptr (may be nullptr) */
	IFilmOverlay* GetPrimaryFilmOverlay() const;

	/** Get an array of all enabled film overlays */
	TArray<IFilmOverlay*> GetActiveFilmOverlays() const;

	/** Set the current primary overlay to the specified name */
	FReply SetPrimaryFilmOverlay(FName InName);

	/** Get/Set the color tint override for the current primary overlay */
	FLinearColor GetPrimaryColorTint() const;
	void OnPrimaryColorTintChanged(const FLinearColor& Tint);
	
	/** Toggle the film overlay enabled or disabled */
	FReply ToggleFilmOverlay(FName InName);

private:

	/** The name of the current primary overlay */
	FName CurrentPrimaryOverlay;

	/** Color tint to apply to primary overlays */
	FLinearColor PrimaryColorTint;

	/** Primary overlays (only one can be active at a time) */
	TArray<TSharedPtr<IFilmOverlay>> PrimaryOverlays;

	/** Toggleable overlays (any number can be active at a time) */
	TArray<TSharedPtr<IFilmOverlay>> ToggleableOverlays;

	/** The overlay widget we control */
	TSharedPtr<SFilmOverlay> OverlayWidget;
};
