// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SlateGlobals.h"
#include "Widgets/SWidget.h"
#include "Framework/Text/IRun.h"
#include "Framework/Text/TextLayout.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/ISlateRun.h"
#include "Brushes/SlateDynamicImageBrush.h"

class FArrangedChildren;
class FPaintArgs;
class FSlateWindowElementList;
struct FTextBlockStyle;
enum class ETextHitPoint : uint8;

#if WITH_FANCY_TEXT

class FSlateImageRun : public ISlateRun, public TSharedFromThis< FSlateImageRun >
{
public:

	static SLATE_API TSharedRef< FSlateImageRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FSlateBrush* InImage, const TAttribute<int16>& InBaseline );
	static SLATE_API TSharedRef< FSlateImageRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FSlateBrush* InImage, const TAttribute<int16>& InBaseline , const FTextRange& InRange );
	static SLATE_API TSharedRef< FSlateImageRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, FName InDynamicBrushName, const TAttribute<int16>& InBaseline );
	static SLATE_API TSharedRef< FSlateImageRun > Create( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, FName InDynamicBrushName, const TAttribute<int16>& InBaseline, const FTextRange& InRange );

public:

	SLATE_API virtual ~FSlateImageRun();

	SLATE_API virtual FTextRange GetTextRange() const override;
	SLATE_API virtual void SetTextRange( const FTextRange& Value ) override;

	SLATE_API virtual int16 GetBaseLine( float Scale ) const override;
	SLATE_API virtual int16 GetMaxHeight( float Scale ) const override;

	SLATE_API virtual FVector2D Measure( int32 StartIndex, int32 EndIndex, float Scale, const FRunTextContext& TextContext ) const override;

	SLATE_API virtual int8 GetKerning(int32 CurrentIndex, float Scale, const FRunTextContext& TextContext) const override;

	SLATE_API virtual TSharedRef< ILayoutBlock > CreateBlock( int32 StartIndex, int32 EndIndex, FVector2D Size, const FLayoutBlockTextContext& TextContext, const TSharedPtr< IRunRenderer >& Renderer ) override;

	SLATE_API virtual int32 OnPaint(const FPaintArgs& PaintArgs, const FTextArgs& TextArgs, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	SLATE_API virtual const TArray< TSharedRef<SWidget> >& GetChildren() override;

	SLATE_API virtual void ArrangeChildren( const TSharedRef< ILayoutBlock >& Block, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const override;

	SLATE_API virtual int32 GetTextIndexAt( const TSharedRef< ILayoutBlock >& Block, const FVector2D& Location, float Scale, ETextHitPoint* const OutHitPoint = nullptr ) const override;

	SLATE_API virtual FVector2D GetLocationAt( const TSharedRef< ILayoutBlock >& Block, int32 Offset, float Scale ) const override;

	virtual void BeginLayout() override {}
	virtual void EndLayout() override {}

	SLATE_API virtual void Move(const TSharedRef<FString>& NewText, const FTextRange& NewRange) override;
	SLATE_API virtual TSharedRef<IRun> Clone() const override;

	SLATE_API virtual void AppendTextTo(FString& AppendToText) const override;
	SLATE_API virtual void AppendTextTo(FString& AppendToText, const FTextRange& PartialRange) const override;

	SLATE_API virtual const FRunInfo& GetRunInfo() const override;

	SLATE_API virtual ERunAttributes GetRunAttributes() const override;

protected:

	SLATE_API FSlateImageRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FSlateBrush* InImage, const TAttribute<int16>& InBaseline );

	SLATE_API FSlateImageRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, const FSlateBrush* InImage, const TAttribute<int16>& InBaseline, const FTextRange& InRange );

	SLATE_API FSlateImageRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, FName InDynamicBrushName, const TAttribute<int16>& InBaseline );

	SLATE_API FSlateImageRun( const FRunInfo& InRunInfo, const TSharedRef< const FString >& InText, FName InDynamicBrushName, const TAttribute<int16>& InBaseline, const FTextRange& InRange );

private:

	FRunInfo RunInfo;
	TSharedRef< const FString > Text;
	FTextRange Range;
	const FSlateBrush* Image;
	TAttribute< int16 > Baseline;
	TSharedPtr< FSlateDynamicImageBrush > DynamicBrush;
};

#endif //WITH_FANCY_TEXT
