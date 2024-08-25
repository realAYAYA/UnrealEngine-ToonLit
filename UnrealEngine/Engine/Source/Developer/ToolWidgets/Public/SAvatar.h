// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/** Widget for displaying a user avatar icon.
 * The avatar's background color is computed based on the Identifier.
 * The avatar's displayed letter is the initial letter of the Description.
 */
class TOOLWIDGETS_API SAvatar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvatar) :
		_HeightOverride(17.f),
		_WidthOverride(17.f),
		_ShowInitial(true)
	{}
		SLATE_ARGUMENT(FString, Identifier)
		SLATE_ARGUMENT(FString, Description)
		SLATE_ARGUMENT(float, HeightOverride)
		SLATE_ARGUMENT(float, WidthOverride)
		SLATE_ARGUMENT(bool, ShowInitial)
	SLATE_END_ARGS()

public:
	/** Construct this widget */
	void Construct(const FArguments& InArgs);

	/** The custom OnPaint handler necessary because we want to draw a circle */
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** The Identifier of the avatar */
	FString Identifier;

	/** The Description of the avatar */
	FString Description;

	/** The BackgroundColor used */
	FColor BackgroundColor;

	/** The ForegroundColor used (text) */
	FColor ForegroundColor;

	/** The setting that determines if we're showing the initial */
	bool bShowInitial;
};