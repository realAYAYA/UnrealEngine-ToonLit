// Copyright Epic Games, Inc. All Rights Reserved.

#include "Framework/Text/SlateTextLayout.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/ILayoutBlock.h"
#include "Framework/Text/ISlateRun.h"
#include "Framework/Text/ISlateRunRenderer.h"
#include "Framework/Text/ISlateLineHighlighter.h"
#include "Framework/Text/SlateTextRun.h"
#include "Framework/Text/SlatePasswordRun.h"
#include "Trace/SlateMemoryTags.h"

TSharedRef< FSlateTextLayout > FSlateTextLayout::Create(SWidget* InOwner, FTextBlockStyle InDefaultTextStyle)
{
	LLM_SCOPE_BYTAG(UI_Text);
	TSharedRef< FSlateTextLayout > Layout = MakeShareable( new FSlateTextLayout(InOwner, MoveTemp(InDefaultTextStyle)) );
	Layout->AggregateChildren();

	return Layout;
}

FSlateTextLayout::FSlateTextLayout(SWidget* InOwner, FTextBlockStyle InDefaultTextStyle)
	: DefaultTextStyle(MoveTemp(InDefaultTextStyle))
	, Children(InOwner, false)
	, bIsPassword(false)
{

}

FChildren* FSlateTextLayout::GetChildren()
{
	return &Children;
}

void FSlateTextLayout::ArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	for (int32 LineIndex = 0; LineIndex < LineViews.Num(); LineIndex++)
	{
		const FTextLayout::FLineView& LineView = LineViews[ LineIndex ];

		for (int32 BlockIndex = 0; BlockIndex < LineView.Blocks.Num(); BlockIndex++)
		{
			const TSharedRef< ILayoutBlock > Block = LineView.Blocks[ BlockIndex ];
			const TSharedRef< ISlateRun > Run = StaticCastSharedRef< ISlateRun >( Block->GetRun() );
			Run->ArrangeChildren( Block, AllottedGeometry, ArrangedChildren );
		}
	}
}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

int32 GShowTextDebugging = 0;
static FAutoConsoleVariableRef CVarSlateShowTextDebugging(TEXT("Slate.ShowTextDebugging"), GShowTextDebugging, TEXT("Show debugging painting for text rendering."), ECVF_Default);

#endif

int32 FSlateTextLayout::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const ESlateDrawEffect DrawEffects = bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	FLinearColor BlockDebugHue( 0, 1.0f, 1.0f, 0.5 );
#endif

	// The block size and offset values are pre-scaled, so we need to account for that when converting the block offsets into paint geometry
	const float InverseScale = Inverse(AllottedGeometry.Scale);

	int32 HighestLayerId = LayerId;

	auto IsLineViewVisible = [&AllottedGeometry, &InverseScale, &MyCullingRect](const FTextLayout::FLineView& LineView, bool bTestForVerticallyClippedLine = false)
	{
		// Is this line visible?  This checks if the culling rect, which represents the AABB around the last clipping rect, intersects the 
		// line of text, this requires that we get the text line into render space.
		// TODO perhaps save off this line view rect during text layout?
		const FVector2D LocalLineOffset = LineView.Offset * InverseScale;
		FSlateRect LineViewRect(AllottedGeometry.GetRenderBoundingRect(FSlateRect(LocalLineOffset, LocalLineOffset + (LineView.Size * InverseScale))));

		if (!bTestForVerticallyClippedLine)
		{
			return FSlateRect::DoRectanglesIntersect(LineViewRect, MyCullingRect);
		}
		else
		{
			// We only care about vertically clipped lines. Horizontal clipping will be replaced with an ellipsis
			// So make the left and right side the same as the clip rect to avoid the test failing due to being larger horizontally

			FSlateRect GeometryRect = AllottedGeometry.GetRenderBoundingRect();
			LineViewRect.Left = GeometryRect.Left;
			LineViewRect.Right = GeometryRect.Right;

			return FSlateRect::IsRectangleContained(GeometryRect, LineViewRect);
		}

	};

	for (int32 LineIndex = 0; LineIndex < LineViews.Num(); ++LineIndex)
	{
		const FTextLayout::FLineView& LineView = LineViews[LineIndex];

		ETextOverflowPolicy OverflowPolicy = TextOverflowPolicyOverride.Get(DefaultTextStyle.OverflowPolicy);

		if (!IsLineViewVisible(LineView))
		{
			continue;
		}

		// Render any underlays for this line
		const int32 HighestUnderlayLayerId = OnPaintHighlights( Args, LineView, LineView.UnderlayHighlights, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled );

		const int32 BlockDebugLayer = HighestUnderlayLayerId;
		const int32 TextLayer = BlockDebugLayer + 1;
		int32 HighestBlockLayerId = TextLayer;

		const ETextJustify::Type VisualJustification = CalculateLineViewVisualJustification(LineView);
		ETextOverflowDirection OverflowDirection = VisualJustification == ETextJustify::Left ? ETextOverflowDirection::LeftToRight : (VisualJustification == ETextJustify::Right ? ETextOverflowDirection::RightToLeft : ETextOverflowDirection::NoOverflow);

		bool bForceEllipsisDueToClippedLine = false;
	
		if (OverflowPolicy == ETextOverflowPolicy::Ellipsis && LineViews.Num() > 1)
		{
			// Force the ellipsis to be on when the next line in a multi line text layout is clipped. This forces an ellipsis on this line even when its not clipped to indicate that an entire line or more is clipped
			bForceEllipsisDueToClippedLine = LineViews.IsValidIndex(LineIndex + 1) ? !IsLineViewVisible(LineViews[LineIndex + 1], true) : false;
		}

		// Render every block for this line
		for (const TSharedRef< ILayoutBlock >& Block : LineView.Blocks)
		{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if ( GShowTextDebugging )
			{
				BlockDebugHue.R += 50.0f;

				FSlateDrawElement::MakeBox(
					OutDrawElements, 
					BlockDebugLayer,
					AllottedGeometry.ToPaintGeometry(TransformVector(InverseScale, Block->GetSize()), FSlateLayoutTransform(TransformPoint(InverseScale, Block->GetLocationOffset()))),
					&DefaultTextStyle.HighlightShape,
					DrawEffects,
					InWidgetStyle.GetColorAndOpacityTint() * BlockDebugHue.HSVToLinearRGB()
					);
			}
#endif

			FTextArgs TextArgs(LineView, Block, DefaultTextStyle, OverflowPolicy, OverflowDirection);
			TextArgs.bForceEllipsisDueToClippedLine = bForceEllipsisDueToClippedLine;

			const TSharedRef< ISlateRun > Run = StaticCastSharedRef< ISlateRun >( Block->GetRun() );

			int32 HighestRunLayerId = TextLayer;
			const TSharedPtr< ISlateRunRenderer > RunRenderer = StaticCastSharedPtr< ISlateRunRenderer >( Block->GetRenderer() );
			if ( RunRenderer.IsValid() )
			{
				HighestRunLayerId = RunRenderer->OnPaint( Args, LineView, Run, Block, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, TextLayer, InWidgetStyle, bParentEnabled );
			}
			else
			{
				HighestRunLayerId = Run->OnPaint( Args, TextArgs, AllottedGeometry, MyCullingRect, OutDrawElements, TextLayer, InWidgetStyle, bParentEnabled );
			}

			HighestBlockLayerId = FMath::Max( HighestBlockLayerId, HighestRunLayerId );
		}

		// Render any overlays for this line
		const int32 HighestOverlayLayerId = OnPaintHighlights(Args, LineView, LineView.OverlayHighlights, DefaultTextStyle, AllottedGeometry, MyCullingRect, OutDrawElements, HighestBlockLayerId, InWidgetStyle, bParentEnabled);
		HighestLayerId = FMath::Max(HighestLayerId, HighestOverlayLayerId);
	}

	return HighestLayerId;
}

int32 FSlateTextLayout::OnPaintHighlights( const FPaintArgs& Args, const FTextLayout::FLineView& LineView, const TArray<FLineViewHighlight>& Highlights, const FTextBlockStyle& InDefaultTextStyle, const FGeometry& AllottedGeometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements, const int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	int32 CurrentLayerId = LayerId;

	for (const FLineViewHighlight& Highlight : Highlights)
	{
		const TSharedPtr< ISlateLineHighlighter > LineHighlighter = StaticCastSharedPtr< ISlateLineHighlighter >( Highlight.Highlighter );
		if (LineHighlighter.IsValid())
		{
			CurrentLayerId = LineHighlighter->OnPaint( Args, LineView, Highlight.OffsetX, Highlight.Width, InDefaultTextStyle, AllottedGeometry, CullingRect, OutDrawElements, CurrentLayerId, InWidgetStyle, bParentEnabled );
		}
	}

	return CurrentLayerId;
}

void FSlateTextLayout::EndLayout()
{
	FTextLayout::EndLayout();
	AggregateChildren();
}

void FSlateTextLayout::SetDefaultTextStyle(FTextBlockStyle InDefaultTextStyle)
{
	DefaultTextStyle = MoveTemp(InDefaultTextStyle);
}

const FTextBlockStyle& FSlateTextLayout::GetDefaultTextStyle() const
{
	return DefaultTextStyle;
}

void FSlateTextLayout::SetIsPassword(const TAttribute<bool>& InIsPassword)
{
	bIsPassword = InIsPassword;
}

void FSlateTextLayout::AggregateChildren()
{
	Children.Empty();
	const TArray< FLineModel >& LayoutLineModels = GetLineModels();
	for (int32 LineModelIndex = 0; LineModelIndex < LayoutLineModels.Num(); LineModelIndex++)
	{
		const FLineModel& LineModel = LayoutLineModels[ LineModelIndex ];
		for (int32 RunIndex = 0; RunIndex < LineModel.Runs.Num(); RunIndex++)
		{
			const FRunModel& LineRun = LineModel.Runs[ RunIndex ];
			const TSharedRef< ISlateRun > SlateRun = StaticCastSharedRef< ISlateRun >( LineRun.GetRun() );

			const TArray< TSharedRef<SWidget> >& RunChildren = SlateRun->GetChildren();
			for (int32 ChildIndex = 0; ChildIndex < RunChildren.Num(); ChildIndex++)
			{
				const TSharedRef< SWidget >& Child = RunChildren[ ChildIndex ];
				Children.Add( Child );
			}
		}
	}
}

TSharedRef<IRun> FSlateTextLayout::CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const
{
	if (bIsPassword.Get(false))
	{
		return FSlatePasswordRun::Create(FRunInfo(), NewText, DefaultTextStyle, NewRange);
	}
	return FSlateTextRun::Create(FRunInfo(), NewText, DefaultTextStyle, NewRange);
}
