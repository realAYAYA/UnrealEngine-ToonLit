// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SLeafWidget.h"
#include "Framework/SlateDelegates.h"

class FPaintArgs;
class FSlateWindowElementList;
struct FSlateBrush;

/**
 * Implements the color spectrum widget.
 */
class SLATE_API SColorSpectrum
	: public SLeafWidget
{
public:

	SLATE_BEGIN_ARGS(SColorSpectrum)
		: _SelectedColor()
		, _OnMouseCaptureBegin()
		, _OnMouseCaptureEnd()
		, _OnValueChanged()
	{ }
	
		/** The current color selected by the user */
		SLATE_ATTRIBUTE(FLinearColor, SelectedColor)
		
		/** Invoked when the mouse is pressed and a capture begins */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureBegin)

		/** Invoked when the mouse is released and a capture ends */
		SLATE_EVENT(FSimpleDelegate, OnMouseCaptureEnd)

		/** Invoked when a new value is selected on the color wheel */
		SLATE_EVENT(FOnLinearColorValueChanged, OnValueChanged)

	SLATE_END_ARGS()
	
public:
	SColorSpectrum();

	/**
	 * Construct this widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct( const FArguments& InArgs );

public:

	// SWidget overrides

	virtual FVector2D ComputeDesiredSize(float) const override;
	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override;
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual FReply OnMouseMove( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	
protected:

	/**
	 * Calculates the position of the color selection indicator.
	 *
	 * @return The position relative to the widget.
	 */
	FVector2D CalcRelativeSelectedPosition( ) const;

	/**
	 * Performs actions according to mouse click / move
	 */
	void ProcessMouseAction(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

private:

	// The color wheel image to show.
	const FSlateBrush* Image;
	
	// The current color selected by the user.
	TSlateAttribute<FLinearColor, EInvalidateWidgetReason::Paint> SelectedColor;

	// The color selector image to show.
	const FSlateBrush* SelectorImage;

private:

	// Holds a delegate that is executed when the mouse is pressed and a capture begins.
	FSimpleDelegate OnMouseCaptureBegin;

	// Holds a delegate that is executed when the mouse is let up and a capture ends.
	FSimpleDelegate OnMouseCaptureEnd;

	// Holds a delegate that is executed when a new value is selected on the color wheel.
	FOnLinearColorValueChanged OnValueChanged;
};
