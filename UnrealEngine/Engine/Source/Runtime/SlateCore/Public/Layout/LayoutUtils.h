// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layout/Children.h"
#include "Types/SlateStructs.h"
#include "CoreMinimal.h"
#include "Margin.h"
#include "Visibility.h"
#include "SlateRect.h"
#include "ArrangedChildren.h"
#include "FlowDirection.h"

struct AlignmentArrangeResult
{
	AlignmentArrangeResult( float InOffset, float InSize )
	: Offset(InOffset)
	, Size(InSize)
	{
	}
	
	float Offset;
	float Size;
};

namespace ArrangeUtils
{
	/** Gets the alignment of an axis-agnostic int32 so that we can do alignment on an axis without caring about its orientation */
	template<EOrientation Orientation>
	struct GetChildAlignment
	{
		template<typename SlotType>
		static int32 AsInt(EFlowDirection InFlowDirection, const SlotType& InSlot );
	};

	template<>
	struct GetChildAlignment<Orient_Horizontal>
	{
		template<typename SlotType>
		static int32 AsInt(EFlowDirection InFlowDirection, const SlotType& InSlot )
		{
			switch (InFlowDirection)
			{
			default:
			case EFlowDirection::LeftToRight:
				return static_cast<int32>(InSlot.GetHorizontalAlignment());
			case EFlowDirection::RightToLeft:
				switch (InSlot.GetHorizontalAlignment())
				{
				case HAlign_Left:
					return static_cast<int32>(HAlign_Right);
				case HAlign_Right:
					return static_cast<int32>(HAlign_Left);
				default:
					return static_cast<int32>(InSlot.GetHorizontalAlignment());
				}
			}
		}
	};

	template<>
	struct GetChildAlignment<Orient_Vertical>
	{
		template<typename SlotType>
		static int32 AsInt(EFlowDirection InFlowDirection, const SlotType& InSlot )
		{
			// InFlowDirection has no effect in vertical orientations.
			return static_cast<int32>(InSlot.GetVerticalAlignment());
		}
	};

	/**
	 * Same as AlignChild but force the alignment to be fill.
	 * @return  Offset and Size of widget
	 */
	template<EOrientation Orientation>
	static AlignmentArrangeResult AlignFill(float AllottedSize, const FMargin& SlotPadding, const float ContentScale = 1.0f)
	{
		const FMargin& Margin = SlotPadding;
		const float TotalMargin = Margin.GetTotalSpaceAlong<Orientation>();
		const float MarginPre = (Orientation == Orient_Horizontal) ? Margin.Left : Margin.Top;
		return AlignmentArrangeResult(MarginPre, FMath::Max((AllottedSize - TotalMargin) * ContentScale, 0.f));
	}

	/**
	 * Same as AlignChild but force the alignment to be center.
	 * @return  Offset and Size of widget
	 */
	template<EOrientation Orientation>
	static AlignmentArrangeResult AlignCenter(float AllottedSize, float ChildDesiredSize, const FMargin& SlotPadding, const float ContentScale = 1.0f, bool bClampToParent = true)
	{
		const FMargin& Margin = SlotPadding;
		const float TotalMargin = Margin.GetTotalSpaceAlong<Orientation>();
		const float MarginPre = (Orientation == Orient_Horizontal) ? Margin.Left : Margin.Top;
		const float MarginPost = (Orientation == Orient_Horizontal) ? Margin.Right : Margin.Bottom;
		const float ChildSize = FMath::Max((bClampToParent ? FMath::Min(ChildDesiredSize, AllottedSize - TotalMargin) : ChildDesiredSize), 0.f);
		return AlignmentArrangeResult((AllottedSize - ChildSize) / 2.0f + MarginPre - MarginPost, ChildSize);
	}
}



/**
 * Helper method to BoxPanel::ArrangeChildren.
 * 
 * @param AllottedSize         The size available to arrange the widget along the given orientation
 * @param ChildToArrange       The widget and associated layout information
 * @param SlotPadding          The padding to when aligning the child
 * @param ContentScale         The scale to apply to the child before aligning it.
 * @param bClampToParent       If true the child's size is clamped to the allotted size before alignment occurs, if false, the child's desired size is used, even if larger than the allotted size.
 * 
 * @return  Offset and Size of widget
 */
template<EOrientation Orientation, typename SlotType>
static AlignmentArrangeResult AlignChild(EFlowDirection InLayoutFlow, float AllottedSize, float ChildDesiredSize, const SlotType& ChildToArrange, const FMargin& SlotPadding, const float& ContentScale = 1.0f, bool bClampToParent = true)
{
	const FMargin& Margin = SlotPadding;
	const float TotalMargin = Margin.GetTotalSpaceAlong<Orientation>();
	const float MarginPre = ( Orientation == Orient_Horizontal ) ? Margin.Left : Margin.Top;
	const float MarginPost = ( Orientation == Orient_Horizontal ) ? Margin.Right : Margin.Bottom;

	const int32 Alignment = ArrangeUtils::GetChildAlignment<Orientation>::AsInt(InLayoutFlow, ChildToArrange);

	switch (Alignment)
	{
	case HAlign_Fill:
		return AlignmentArrangeResult(MarginPre, FMath::Max((AllottedSize - TotalMargin) * ContentScale, 0.f));
	}
	
	const float ChildSize = FMath::Max((bClampToParent ? FMath::Min(ChildDesiredSize, AllottedSize - TotalMargin) : ChildDesiredSize), 0.f);

	switch( Alignment )
	{
	case HAlign_Left: // same as Align_Top
		return AlignmentArrangeResult(MarginPre, ChildSize);
	case HAlign_Center:
		return AlignmentArrangeResult(( AllottedSize - ChildSize ) / 2.0f + MarginPre - MarginPost, ChildSize);
	case HAlign_Right: // same as Align_Bottom		
		return AlignmentArrangeResult(AllottedSize - ChildSize - MarginPost, ChildSize);
	}

	// Same as Fill
	return AlignmentArrangeResult(MarginPre, FMath::Max(( AllottedSize - TotalMargin ) * ContentScale, 0.f));
}

template<EOrientation Orientation, typename SlotType>
static AlignmentArrangeResult AlignChild(float AllottedSize, float ChildDesiredSize, const SlotType& ChildToArrange, const FMargin& SlotPadding, const float& ContentScale = 1.0f, bool bClampToParent = true)
{
	return AlignChild<Orientation, SlotType>(EFlowDirection::LeftToRight, AllottedSize, ChildDesiredSize, ChildToArrange, SlotPadding, ContentScale, bClampToParent);
}

template<EOrientation Orientation, typename SlotType>
static AlignmentArrangeResult AlignChild(EFlowDirection InLayoutFlow, float AllottedSize, const SlotType& ChildToArrange, const FMargin& SlotPadding, const float& ContentScale = 1.0f, bool bClampToParent = true)
{
	const FMargin& Margin = SlotPadding;
	const float TotalMargin = Margin.GetTotalSpaceAlong<Orientation>();
	const float MarginPre = ( Orientation == Orient_Horizontal ) ? Margin.Left : Margin.Top;
	const float MarginPost = ( Orientation == Orient_Horizontal ) ? Margin.Right : Margin.Bottom;

	const int32 Alignment = ArrangeUtils::GetChildAlignment<Orientation>::AsInt(InLayoutFlow, ChildToArrange);

	switch (Alignment)
	{
	case HAlign_Fill:
		return AlignmentArrangeResult(MarginPre, FMath::Max((AllottedSize - TotalMargin) * ContentScale, 0.f));
	}

	const float ChildDesiredSize = ( Orientation == Orient_Horizontal )
		? ( ChildToArrange.GetWidget()->GetDesiredSize().X * ContentScale )
		: ( ChildToArrange.GetWidget()->GetDesiredSize().Y * ContentScale );

	const float ChildSize = FMath::Max((bClampToParent ? FMath::Min(ChildDesiredSize, AllottedSize - TotalMargin) : ChildDesiredSize), 0.f);

	switch ( Alignment )
	{
	case HAlign_Left: // same as Align_Top
		return AlignmentArrangeResult(MarginPre, ChildSize);
	case HAlign_Center:
		return AlignmentArrangeResult(( AllottedSize - ChildSize ) / 2.0f + MarginPre - MarginPost, ChildSize);
	case HAlign_Right: // same as Align_Bottom		
		return AlignmentArrangeResult(AllottedSize - ChildSize - MarginPost, ChildSize);
	}

	// Same as Fill
	return AlignmentArrangeResult(MarginPre, FMath::Max((AllottedSize - TotalMargin) * ContentScale, 0.f));
}

template<EOrientation Orientation, typename SlotType>
static AlignmentArrangeResult AlignChild(float AllottedSize, const SlotType& ChildToArrange, const FMargin& SlotPadding, const float& ContentScale = 1.0f, bool bClampToParent = true)
{
	return AlignChild<Orientation, SlotType>(EFlowDirection::LeftToRight, AllottedSize, ChildToArrange, SlotPadding, ContentScale, bClampToParent);
}


/**
 * Arrange a ChildSlot within the AllottedGeometry and populate ArrangedChildren with the arranged result.
 * The code makes certain assumptions about the type of ChildSlot.
 */
template<typename SlotType>
static void ArrangeSingleChild(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SlotType& ChildSlot, const TAttribute<FVector2D>& ContentScale)
{
	ArrangeSingleChild<SlotType>(EFlowDirection::LeftToRight, AllottedGeometry, ArrangedChildren, ChildSlot, ContentScale);
}

template<typename SlotType>
static void ArrangeSingleChild(EFlowDirection InFlowDirection, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SlotType& ChildSlot, const TAttribute<FVector2D>& ContentScale)
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if ( ArrangedChildren.Accepts(ChildVisibility) )
	{
		const FVector2D ThisContentScale = ContentScale.Get();
		const FMargin SlotPadding(LayoutPaddingWithFlow(InFlowDirection, ChildSlot.GetPadding()));
		const AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(InFlowDirection, AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding, ThisContentScale.X);
		const AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding, ThisContentScale.Y);

		ArrangedChildren.AddWidget( ChildVisibility, AllottedGeometry.MakeChild(
				ChildSlot.GetWidget(),
				FVector2D(XResult.Offset, YResult.Offset),
				FVector2D(XResult.Size, YResult.Size)
		) );
	}
}

template<typename SlotType>
static void ArrangeSingleChild(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SlotType& ChildSlot, const FVector2D& ContentScale)
{
	ArrangeSingleChild<SlotType>(EFlowDirection::LeftToRight, AllottedGeometry, ArrangedChildren, ChildSlot, ContentScale);
}

template<typename SlotType>
static void ArrangeSingleChild(EFlowDirection InFlowDirection, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, const SlotType& ChildSlot, const FVector2D& ContentScale)
{
	const EVisibility ChildVisibility = ChildSlot.GetWidget()->GetVisibility();
	if (ArrangedChildren.Accepts(ChildVisibility))
	{
		const FVector2D ThisContentScale = ContentScale;
		const FMargin SlotPadding(LayoutPaddingWithFlow(InFlowDirection, ChildSlot.GetPadding()));
		const AlignmentArrangeResult XResult = AlignChild<Orient_Horizontal>(InFlowDirection, AllottedGeometry.GetLocalSize().X, ChildSlot, SlotPadding, ThisContentScale.X);
		const AlignmentArrangeResult YResult = AlignChild<Orient_Vertical>(AllottedGeometry.GetLocalSize().Y, ChildSlot, SlotPadding, ThisContentScale.Y);

		ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
			ChildSlot.GetWidget(),
			FVector2f(XResult.Size, YResult.Size),
			FSlateLayoutTransform(FVector2f(XResult.Offset, YResult.Offset))
		));
	}
}

template<EOrientation Orientation, typename SlotType>
static void ArrangeChildrenInStack(EFlowDirection InLayoutFlow, const TPanelChildren<SlotType>& Children, const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren, float InOffset, bool InAllowShrink)
{
	// Allotted space will be given to fixed-size children first.
	// Remaining space will be proportionately divided between stretch children (SizeRule_Stretch)
	// based on their stretch coefficient

	if (Children.Num() > 0)
	{
		float StretchCoefficientTotal = 0.0f;
		float FixedTotal = 0.0f;
		float StretchSizeTotal = 0.0f;

		bool bAnyChildVisible = false;
		// Compute the sum of stretch coefficients (SizeRule_Stretch) and space required by fixed-size widgets (SizeRule_Auto),
		// as well as the total desired size.
		for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
		{
			const SlotType& CurChild = Children[ChildIndex];

			if (CurChild.GetWidget()->GetVisibility() != EVisibility::Collapsed)
			{
				bAnyChildVisible = true;
				// All widgets contribute their margin to the fixed space requirement
				FixedTotal += CurChild.GetPadding().template GetTotalSpaceAlong<Orientation>();

				FVector2f ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();

				// Auto-sized children contribute their desired size to the fixed space requirement
				const float ChildSize = (Orientation == Orient_Vertical)
					? ChildDesiredSize.Y
					: ChildDesiredSize.X;

				if (CurChild.GetSizeRule() == FSizeParam::SizeRule_Stretch)
				{
					// for stretch children we save sum up the stretch coefficients
					StretchCoefficientTotal += CurChild.GetSizeValue();
					StretchSizeTotal += ChildSize;
				}
				else
				{

					// Clamp to the max size if it was specified
					float MaxSize = CurChild.GetMaxSize();
					FixedTotal += MaxSize > 0.0f ? FMath::Min(MaxSize, ChildSize) : ChildSize;
				}
			}
		}

		if (!bAnyChildVisible)
		{
			return;
		}

		//When shrink is not allowed, we'll ensure to use all the space desired by the stretchable widgets.
		const float MinSize = InAllowShrink ? 0.0f : StretchSizeTotal;

		// The space available for SizeRule_Stretch widgets is any space that wasn't taken up by fixed-sized widgets.
		const float NonFixedSpace = FMath::Max(MinSize, (Orientation == Orient_Vertical)
			? AllottedGeometry.GetLocalSize().Y - FixedTotal
			: AllottedGeometry.GetLocalSize().X - FixedTotal);

		float PositionSoFar = 0.0f;

		// Now that we have the total fixed-space requirement and the total stretch coefficients we can
		// arrange widgets top-to-bottom or left-to-right (depending on the orientation).
		for (TPanelChildrenConstIterator<SlotType> It(Children, Orientation, InLayoutFlow); It; ++It)
		{
			const SlotType& CurChild = *It;
			const EVisibility ChildVisibility = CurChild.GetWidget()->GetVisibility();

			// Figure out the area allocated to the child in the direction of BoxPanel
			// The area allocated to the slot is ChildSize + the associated margin.
			float ChildSize = 0.0f;
			if (ChildVisibility != EVisibility::Collapsed)
			{
				// The size of the widget depends on its size type
				if (CurChild.GetSizeRule() == FSizeParam::SizeRule_Stretch)
				{
					if (StretchCoefficientTotal > 0.0f)
					{
						// Stretch widgets get a fraction of the space remaining after all the fixed-space requirements are met
						ChildSize = NonFixedSpace * CurChild.GetSizeValue() / StretchCoefficientTotal;
					}
				}
				else
				{
					const FVector2f ChildDesiredSize = CurChild.GetWidget()->GetDesiredSize();

					// Auto-sized widgets get their desired-size value
					ChildSize = (Orientation == Orient_Vertical)
						? ChildDesiredSize.Y
						: ChildDesiredSize.X;
				}

				// Clamp to the max size if it was specified
				float MaxSize = CurChild.GetMaxSize();
				if (MaxSize > 0.0f)
				{
					ChildSize = FMath::Min(MaxSize, ChildSize);
				}
			}

			const FMargin SlotPadding(LayoutPaddingWithFlow(InLayoutFlow, CurChild.GetPadding()));

			FVector2f SlotSize = (Orientation == Orient_Vertical)
				? FVector2f(AllottedGeometry.GetLocalSize().X, ChildSize + SlotPadding.template GetTotalSpaceAlong<Orient_Vertical>())
				: FVector2f(ChildSize + SlotPadding.template GetTotalSpaceAlong<Orient_Horizontal>(), AllottedGeometry.GetLocalSize().Y);

			// Figure out the size and local position of the child within the slot			
			AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>(InLayoutFlow, SlotSize.X, CurChild, SlotPadding);
			AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>(SlotSize.Y, CurChild, SlotPadding);

			const FVector2f LocalPosition = (Orientation == Orient_Vertical)
				? FVector2f(XAlignmentResult.Offset, PositionSoFar + YAlignmentResult.Offset + InOffset)
				: FVector2f(PositionSoFar + XAlignmentResult.Offset + InOffset, YAlignmentResult.Offset);

			const FVector2f LocalSize = FVector2f(XAlignmentResult.Size, YAlignmentResult.Size);

			// Add the information about this child to the output list (ArrangedChildren)
			ArrangedChildren.AddWidget(ChildVisibility, AllottedGeometry.MakeChild(
				// The child widget being arranged
				CurChild.GetWidget(),
				// Child's local position (i.e. position within parent)
				LocalPosition,
				// Child's size
				LocalSize
			));

			if (ChildVisibility != EVisibility::Collapsed)
			{
				// Offset the next child by the size of the current child and any post-child (bottom/right) margin
				PositionSoFar += (Orientation == Orient_Vertical) ? SlotSize.Y : SlotSize.X;
			}
		}
	}
}

static FMargin LayoutPaddingWithFlow(EFlowDirection InLayoutFlow, const FMargin& InPadding)
{
	FMargin ReturnPadding(InPadding);
	if (InLayoutFlow == EFlowDirection::RightToLeft)
	{
		float Temp = ReturnPadding.Left;
		ReturnPadding.Left = ReturnPadding.Right;
		ReturnPadding.Right = Temp;
	}
	return ReturnPadding;
}

/**
* Given information about a popup and the space available for displaying that popup, compute best placement for it.
*
* @param InAnchor          Area relative to which popup is being created (e.g. the button part of a combo box)
* @param PopupRect         Proposed placement of popup; position may require adjustment.
* @param Orientation       Are we trying to show the popup above/below or left/right relative to the anchor?
* @param RectToFit         The space available for showing this popup; we want to fit entirely within it without clipping.
*
* @return A best position within the RectToFit such that none of the popup clips outside of the RectToFit.
*/
SLATECORE_API UE::Slate::FDeprecateVector2DResult ComputePopupFitInRect(const FSlateRect& InAnchor, const FSlateRect& PopupRect, const EOrientation& Orientation, const FSlateRect& RectToFit);
