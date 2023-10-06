// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Styling/SlateTypes.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "Widgets/SWidget.h"
#include "Layout/Children.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;

class FSlateTextLayout : public FTextLayout
{
public:

	static SLATE_API TSharedRef< FSlateTextLayout > Create(SWidget* InOwner, FTextBlockStyle InDefaultTextStyle);

	SLATE_API FChildren* GetChildren();

	SLATE_API virtual void ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const;

	SLATE_API virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const;

	SLATE_API virtual void EndLayout() override;

	SLATE_API void SetDefaultTextStyle(FTextBlockStyle InDefaultTextStyle);
	SLATE_API const FTextBlockStyle& GetDefaultTextStyle() const;

	SLATE_API void SetIsPassword(const TAttribute<bool>& InIsPassword);

protected:

	SLATE_API FSlateTextLayout(SWidget* InOwner, FTextBlockStyle InDefaultTextStyle);

	SLATE_API virtual int32 OnPaintHighlights(const FPaintArgs& Args, const FTextLayout::FLineView& LineView, const TArray<FLineViewHighlight>& Highlights, const FTextBlockStyle& DefaultTextStyle, const FGeometry& AllottedGeometry, const FSlateRect& ClippingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const;

	SLATE_API virtual void AggregateChildren();

	SLATE_API virtual TSharedRef<IRun> CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const override;

protected:
	/** Default style used by the TextLayout */
	FTextBlockStyle DefaultTextStyle;

private:

	TSlotlessChildren< SWidget > Children;

	/** This this layout displaying a password? */
	TAttribute<bool> bIsPassword;

	friend class FSlateTextLayoutFactory;
};
