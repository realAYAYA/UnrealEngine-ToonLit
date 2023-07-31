// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SBox.h"
#include "Types/PaintArgs.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/LayoutUtils.h"

SLATE_IMPLEMENT_WIDGET(SBox)
void SBox::PrivateRegisterAttributes(FSlateAttributeInitializer& AttributeInitializer)
{
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION_WITH_NAME(AttributeInitializer, "SlotPadding", ChildSlot.SlotPaddingAttribute, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, WidthOverride, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, HeightOverride, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, MinDesiredWidth, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, MinDesiredHeight, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, MaxDesiredWidth, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, MaxDesiredHeight, EInvalidateWidgetReason::Layout);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, MinAspectRatio, EInvalidateWidgetReason::Paint);
	SLATE_ADD_MEMBER_ATTRIBUTE_DEFINITION(AttributeInitializer, MaxAspectRatio, EInvalidateWidgetReason::Paint);
}

SBox::SBox()
	: ChildSlot(this)
	, WidthOverride(*this)
	, HeightOverride(*this)
	, MinDesiredWidth(*this)
	, MinDesiredHeight(*this)
	, MaxDesiredWidth(*this)
	, MaxDesiredHeight(*this)
	, MinAspectRatio(*this)
	, MaxAspectRatio(*this)
{
	SetCanTick(false);
	bCanSupportFocus = false;
}

void SBox::Construct( const FArguments& InArgs )
{
	SetWidthOverride(InArgs._WidthOverride);
	SetHeightOverride(InArgs._HeightOverride);

	SetMinDesiredWidth(InArgs._MinDesiredWidth);
	SetMinDesiredHeight(InArgs._MinDesiredHeight);
	SetMaxDesiredWidth(InArgs._MaxDesiredWidth);
	SetMaxDesiredHeight(InArgs._MaxDesiredHeight);

	SetMinAspectRatio(InArgs._MinAspectRatio);
	SetMaxAspectRatio( InArgs._MaxAspectRatio);

	ChildSlot
		.HAlign( InArgs._HAlign )
		.VAlign( InArgs._VAlign )
		.Padding( InArgs._Padding )
		[
			InArgs._Content.Widget
		];
}

void SBox::SetContent(const TSharedRef< SWidget >& InContent)
{
	ChildSlot
	[
		InContent
	];
}

void SBox::SetHAlign(EHorizontalAlignment HAlign)
{
	ChildSlot.SetHorizontalAlignment(HAlign);
}

void SBox::SetVAlign(EVerticalAlignment VAlign)
{
	ChildSlot.SetVerticalAlignment(VAlign);
}

void SBox::SetPadding(TAttribute<FMargin> InPadding)
{
	ChildSlot.SetPadding(InPadding);
}

void SBox::SetWidthOverride(TAttribute<FOptionalSize> InWidthOverride)
{
	WidthOverride.Assign(*this, InWidthOverride);
}

void SBox::SetHeightOverride(TAttribute<FOptionalSize> InHeightOverride)
{
	HeightOverride.Assign(*this, InHeightOverride);
}

void SBox::SetMinDesiredWidth(TAttribute<FOptionalSize> InMinDesiredWidth)
{
	MinDesiredWidth.Assign(*this, InMinDesiredWidth);
}

void SBox::SetMinDesiredHeight(TAttribute<FOptionalSize> InMinDesiredHeight)
{
	MinDesiredHeight.Assign(*this, InMinDesiredHeight);
}

void SBox::SetMaxDesiredWidth(TAttribute<FOptionalSize> InMaxDesiredWidth)
{
	MaxDesiredWidth.Assign(*this, InMaxDesiredWidth);
}

void SBox::SetMaxDesiredHeight(TAttribute<FOptionalSize> InMaxDesiredHeight)
{
	MaxDesiredHeight.Assign(*this, InMaxDesiredHeight);
}

void SBox::SetMinAspectRatio(TAttribute<FOptionalSize> InMinAspectRatio)
{
	MinAspectRatio.Assign(*this, InMinAspectRatio);
}

void SBox::SetMaxAspectRatio(TAttribute<FOptionalSize> InMaxAspectRatio)
{
	MaxAspectRatio.Assign(*this, InMaxAspectRatio);
}

FVector2D SBox::ComputeDesiredSize( float ) const
{
	EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();

	if ( ChildVisibility != EVisibility::Collapsed )
	{
		const FOptionalSize CurrentWidthOverride = WidthOverride.Get();
		const FOptionalSize CurrentHeightOverride = HeightOverride.Get();

		return FVector2D(
			( CurrentWidthOverride.IsSet() ) ? CurrentWidthOverride.Get() : ComputeDesiredWidth(),
			( CurrentHeightOverride.IsSet() ) ? CurrentHeightOverride.Get() : ComputeDesiredHeight()
		);
	}
	
	return FVector2D::ZeroVector;
}

float SBox::ComputeDesiredWidth() const
{
	// If the user specified a fixed width or height, those values override the Box's content.
	const FVector2D& UnmodifiedChildDesiredSize = ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.GetPadding().GetDesiredSize();
	const FOptionalSize CurrentMinDesiredWidth = MinDesiredWidth.Get();
	const FOptionalSize CurrentMaxDesiredWidth = MaxDesiredWidth.Get();

	float CurrentWidth = UnmodifiedChildDesiredSize.X;

	if (CurrentMinDesiredWidth.IsSet())
	{
		CurrentWidth = FMath::Max(CurrentWidth, CurrentMinDesiredWidth.Get());
	}

	if (CurrentMaxDesiredWidth.IsSet())
	{
		CurrentWidth = FMath::Min(CurrentWidth, CurrentMaxDesiredWidth.Get());
	}

	return CurrentWidth;
}

float SBox::ComputeDesiredHeight() const
{
	// If the user specified a fixed width or height, those values override the Box's content.
	const FVector2D& UnmodifiedChildDesiredSize = ChildSlot.GetWidget()->GetDesiredSize() + ChildSlot.GetPadding().GetDesiredSize();

	const FOptionalSize CurrentMinDesiredHeight = MinDesiredHeight.Get();
	const FOptionalSize CurrentMaxDesiredHeight = MaxDesiredHeight.Get();

	float CurrentHeight = UnmodifiedChildDesiredSize.Y;

	if (CurrentMinDesiredHeight.IsSet())
	{
		CurrentHeight = FMath::Max(CurrentHeight, CurrentMinDesiredHeight.Get());
	}

	if (CurrentMaxDesiredHeight.IsSet())
	{
		CurrentHeight = FMath::Min(CurrentHeight, CurrentMaxDesiredHeight.Get());
	}

	return CurrentHeight;
}

void SBox::OnArrangeChildren( const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren ) const
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ArrangedChildren.Accepts(ChildVisibility) )
	{
		const FOptionalSize CurrentMinAspectRatio = MinAspectRatio.Get();
		const FOptionalSize CurrentMaxAspectRatio = MaxAspectRatio.Get();
		const FMargin SlotPadding(ChildSlot.GetPadding());
		bool bAlignChildren = true;

		AlignmentArrangeResult XAlignmentResult(0, 0);
		AlignmentArrangeResult YAlignmentResult(0, 0);

		if (CurrentMaxAspectRatio.IsSet() || CurrentMinAspectRatio.IsSet())
		{
			float CurrentWidth = FMath::Min(AllottedGeometry.Size.X, ChildSlot.GetWidget()->GetDesiredSize().X);
			float CurrentHeight = FMath::Min(AllottedGeometry.Size.Y, ChildSlot.GetWidget()->GetDesiredSize().Y);

			float MinAspectRatioWidth = CurrentMinAspectRatio.IsSet() ? CurrentMinAspectRatio.Get() : 0;
			float MaxAspectRatioWidth = CurrentMaxAspectRatio.IsSet() ? CurrentMaxAspectRatio.Get() : 0;
			if (CurrentHeight > 0 && CurrentWidth > 0)
			{
				const float CurrentRatioWidth = (AllottedGeometry.GetLocalSize().X / AllottedGeometry.GetLocalSize().Y);

				bool bFitMaxRatio = (CurrentRatioWidth > MaxAspectRatioWidth && MaxAspectRatioWidth != 0);
				bool bFitMinRatio = (CurrentRatioWidth < MinAspectRatioWidth && MinAspectRatioWidth != 0);
				if (bFitMaxRatio || bFitMinRatio)
				{
					XAlignmentResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding);
					YAlignmentResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding);

					float NewWidth;
					float NewHeight;

					if (bFitMaxRatio)
					{
						const float MaxAspectRatioHeight = 1.0f / MaxAspectRatioWidth;
						NewWidth = MaxAspectRatioWidth * XAlignmentResult.Size;
						NewHeight = MaxAspectRatioHeight * NewWidth;
					}
					else
					{
						const float MinAspectRatioHeight = 1.0f / MinAspectRatioWidth;
						NewWidth = MinAspectRatioWidth * XAlignmentResult.Size;
						NewHeight = MinAspectRatioHeight * NewWidth;
					}

					const float MaxWidth = AllottedGeometry.Size.X - SlotPadding.GetTotalSpaceAlong<Orient_Horizontal>();
					const float MaxHeight = AllottedGeometry.Size.Y - SlotPadding.GetTotalSpaceAlong<Orient_Vertical>();

					if ( NewWidth > MaxWidth )
					{
						float Scale = MaxWidth / NewWidth;
						NewWidth *= Scale;
						NewHeight *= Scale;
					}

					if ( NewHeight > MaxHeight )
					{
						float Scale = MaxHeight / NewHeight;
						NewWidth *= Scale;
						NewHeight *= Scale;
					}

					XAlignmentResult.Size = NewWidth;
					YAlignmentResult.Size = NewHeight;

					bAlignChildren = false;
				}
			}
		}

		if ( bAlignChildren )
		{
			XAlignmentResult = AlignChild<Orient_Horizontal>(AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding);
			YAlignmentResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding);
		}

		const float AlignedSizeX = XAlignmentResult.Size;
		const float AlignedSizeY = YAlignmentResult.Size;

		ArrangedChildren.AddWidget(
			AllottedGeometry.MakeChild(
				ChildSlot.GetWidget(),
				FVector2D(XAlignmentResult.Offset, YAlignmentResult.Offset),
				FVector2D(AlignedSizeX, AlignedSizeY)
			)
		);
	}
}

FChildren* SBox::GetChildren()
{
	return &ChildSlot;
}

int32 SBox::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	// An SBox just draws its only child
	FArrangedChildren ArrangedChildren(EVisibility::Visible);
	this->ArrangeChildren(AllottedGeometry, ArrangedChildren);

	// Maybe none of our children are visible
	if( ArrangedChildren.Num() > 0 )
	{
		check( ArrangedChildren.Num() == 1 );
		FArrangedWidget& TheChild = ArrangedChildren[0];

		return TheChild.Widget->Paint( Args.WithNewParent(this), TheChild.Geometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, ShouldBeEnabled( bParentEnabled ) );
	}
	return LayerId;
}