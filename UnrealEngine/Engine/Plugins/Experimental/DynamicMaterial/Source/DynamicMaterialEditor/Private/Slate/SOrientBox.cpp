// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOrientBox.h"
#include "Layout/LayoutUtils.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "SOrientBox"

SOrientBox::FSlot::FSlotArguments SOrientBox::Slot()
{
	return FSlot::FSlotArguments(MakeUnique<FSlot>());
}

EOrientation SOrientBox::InvertOrientation(const EOrientation InOrietation)
{
	return InOrietation == Orient_Horizontal ? Orient_Vertical : Orient_Horizontal;
}

EHorizontalAlignment SOrientBox::InvertHAlign(const EHorizontalAlignment InAlignment)
{
	switch (InAlignment)
	{
		case HAlign_Left: return HAlign_Right;
		case HAlign_Right: return HAlign_Left;
	}
	return InAlignment;
}

EVerticalAlignment SOrientBox::InvertVAlign(const EVerticalAlignment InAlignment)
{
	switch (InAlignment)
	{
		case VAlign_Top: return VAlign_Bottom;
		case VAlign_Bottom: return VAlign_Top;
	}
	return InAlignment;
}

EVerticalAlignment SOrientBox::HAlignToVAlign(const EHorizontalAlignment InAlignment)
{
	switch (InAlignment)
	{
		case HAlign_Fill: return VAlign_Fill;
		case HAlign_Left: return VAlign_Top;
		case HAlign_Center: return VAlign_Center;
		case HAlign_Right: return VAlign_Bottom;
	}
	return VAlign_Center;
}

EHorizontalAlignment SOrientBox::VAlignToHAlign(const EVerticalAlignment InAlignment)
{
	switch (InAlignment)
	{
		case VAlign_Fill: return HAlign_Fill;
		case VAlign_Top: return HAlign_Left;
		case VAlign_Center: return HAlign_Center;
		case VAlign_Bottom: return HAlign_Right;
	}
	return HAlign_Center;
}

FMargin SOrientBox::InvertPadding(const FMargin& InPadding)
{
	return FMargin(InPadding.Right, InPadding.Bottom, InPadding.Left, InPadding.Top);
}

FMargin SOrientBox::InvertHorizontalPadding(const FMargin& InPadding)
{
	return FMargin(InPadding.Right, InPadding.Top, InPadding.Left, InPadding.Bottom);
}

FMargin SOrientBox::InvertVerticalPadding(const FMargin& InPadding)
{
	return FMargin(InPadding.Left, InPadding.Bottom, InPadding.Right, InPadding.Top);
}

SOrientBox::SOrientBox()
	: Children(this)
{
	SetCanTick(true);
	bCanSupportFocus = false;
}

void SOrientBox::Construct(const FArguments& InArgs)
{
	Orientation = InArgs._Orientation;
	Reverse = InArgs._Reverse;

	Children.AddSlots(MoveTemp(const_cast<TArray<FSlot::FSlotArguments>&>(InArgs._Slots)));
	
	SetOrientation(Orientation.Get());
	SetReversed(Reverse.Get());
}

void SOrientBox::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SPanel::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (Orientation.Get() != CachedOrientation)
	{
		SetOrientation(Orientation.Get());
	}
}

FVector2D SOrientBox::ComputeDesiredSize(float) const
{
	const int32 ChildCount = Children.Num();
	const bool bIsReversed = Reverse.Get();

	FVector2D ThisDesiredSize = FVector2D::ZeroVector;
	for (int32 SlotIndex = 0; SlotIndex < ChildCount; ++SlotIndex)
	{
		const int32 ThisSlotIndex = bIsReversed
			? (ChildCount - 1) - SlotIndex
			: SlotIndex;

		const SOrientBox::FSlot& ThisSlot = Children[ThisSlotIndex];

		if (ThisSlot.GetWidget()->GetVisibility() != EVisibility::Collapsed)
		{
			const FVector2D ChildDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();
			FMargin ThisPadding = Orientation.Get() == EOrientation::Orient_Horizontal ? ThisSlot.GetPadding() : InvertPadding(ThisSlot.GetPadding());

			if (Orientation.Get() == Orient_Horizontal)
			{
				ThisPadding = bIsReversed ? InvertHorizontalPadding(ThisPadding) : ThisPadding;
				ThisDesiredSize.X += ChildDesiredSize.X + ThisPadding.GetTotalSpaceAlong<Orient_Horizontal>();
				ThisDesiredSize.Y = FMath::Max(ChildDesiredSize.Y, ThisDesiredSize.Y);
			}
			else
			{
				ThisPadding = bIsReversed ? InvertVerticalPadding(ThisPadding) : ThisPadding;
				ThisDesiredSize.X = FMath::Max(ChildDesiredSize.X, ThisDesiredSize.X);
				ThisDesiredSize.Y += ChildDesiredSize.Y + ThisPadding.GetTotalSpaceAlong<Orient_Vertical>();
			}
		}
	}

	return ThisDesiredSize;
}

void SOrientBox::OnArrangeChildren(const FGeometry& InAllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	const int32 ChildCount = Children.Num();
	const bool bIsReversed = Reverse.Get();

	float CurrentChildOffset = 0.0f;

	for (int32 SlotIndex = 0; SlotIndex < Children.Num(); ++SlotIndex)
	{
		const int32 ThisSlotIndex = bIsReversed
			? (ChildCount - 1) - SlotIndex
			: SlotIndex;

		const SOrientBox::FSlot& ThisSlot = Children[ThisSlotIndex];
		const EVisibility ChildVisibility = ThisSlot.GetWidget()->GetVisibility();

		if (ChildVisibility != EVisibility::Collapsed)
		{
			const EFlowDirection FlowDirection = EFlowDirection::LeftToRight;
			const FMargin& ThisPadding = Orientation.Get() == EOrientation::Orient_Horizontal ? ThisSlot.GetPadding() : InvertPadding(ThisSlot.GetPadding());
			const FVector2D& WidgetDesiredSize = ThisSlot.GetWidget()->GetDesiredSize();

			if (Orientation.Get() == EOrientation::Orient_Horizontal)
			{
				const AlignmentArrangeResult YAlignmentResult = AlignChild<Orient_Vertical>(FlowDirection, InAllottedGeometry.GetLocalSize().Y, ThisSlot, ThisPadding);
				ArrangedChildren.AddWidget(InAllottedGeometry.MakeChild(ThisSlot.GetWidget(), FVector2D(CurrentChildOffset + ThisPadding.Left, YAlignmentResult.Offset), FVector2D(WidgetDesiredSize.X, YAlignmentResult.Size)));
				
				CurrentChildOffset += WidgetDesiredSize.X + ThisPadding.GetTotalSpaceAlong<Orient_Horizontal>();
			}
			else
			{
				const AlignmentArrangeResult XAlignmentResult = AlignChild<Orient_Horizontal>(FlowDirection, InAllottedGeometry.GetLocalSize().X, ThisSlot, ThisPadding);
				ArrangedChildren.AddWidget(InAllottedGeometry.MakeChild(ThisSlot.GetWidget(), FVector2D(XAlignmentResult.Offset, CurrentChildOffset + ThisPadding.Top), FVector2D(XAlignmentResult.Size, WidgetDesiredSize.Y)));

				CurrentChildOffset += WidgetDesiredSize.Y + ThisPadding.GetTotalSpaceAlong<Orient_Vertical>();
			}
		}
	}
}

AlignmentArrangeResult SOrientBox::AlignChildSlot(const EOrientation InOrientation, const EFlowDirection InLayoutFlow, const FVector2D InAllottedSize, const FSlot& InChildToArrange, const FMargin& InSlotPadding, const float& InContentScale, bool bInClampToParent)
{
	const int32 Alignment = GetChildAlignment(InOrientation, InLayoutFlow, InChildToArrange);
	float TotalMargin = 0.0f;
	float MarginPre = 0.0f;
	float MarginPost = 0.0f;
	float AllottedSize = 0.0f;

	if (InOrientation == Orient_Vertical)
	{
		TotalMargin = InSlotPadding.GetTotalSpaceAlong<Orient_Horizontal>();
		MarginPre = InSlotPadding.Left;
		MarginPost = InSlotPadding.Right;
		AllottedSize = InAllottedSize.Y;
	}
	else
	{
		TotalMargin = InSlotPadding.GetTotalSpaceAlong<Orient_Vertical>();
		MarginPre = InSlotPadding.Top;
		MarginPost = InSlotPadding.Bottom;
		AllottedSize = InAllottedSize.X;
	}

	switch (Alignment)
	{
	case HAlign_Fill:
		return AlignmentArrangeResult(MarginPre, FMath::Max((AllottedSize - TotalMargin) * InContentScale, 0.f));
	}

	const float ChildDesiredSize = (InOrientation == Orient_Horizontal)
		? (InChildToArrange.GetWidget()->GetDesiredSize().X * InContentScale)
		: (InChildToArrange.GetWidget()->GetDesiredSize().Y * InContentScale);

	const float ChildSize = FMath::Max((bInClampToParent ? FMath::Min(ChildDesiredSize, AllottedSize - TotalMargin) : ChildDesiredSize), 0.f);

	switch (Alignment)
	{
	case HAlign_Left: // same as Align_Top
		return AlignmentArrangeResult(MarginPre, ChildSize);
	case HAlign_Center:
		return AlignmentArrangeResult((AllottedSize - ChildSize) / 2.0f + MarginPre - MarginPost, ChildSize);
	case HAlign_Right: // same as Align_Bottom		
		return AlignmentArrangeResult(AllottedSize - ChildSize - MarginPost, ChildSize);
	}

	// Same as Fill
	return AlignmentArrangeResult(MarginPre, FMath::Max((AllottedSize - TotalMargin) * InContentScale, 0.f));
}

int32 SOrientBox::GetChildAlignment(const EOrientation InOrientation, const EFlowDirection InFlowDirection, const FSlot& InChildToArrange)
{
	EHorizontalAlignment HAlign = InChildToArrange.GetHorizontalAlignment();
	EVerticalAlignment VAlign = InChildToArrange.GetVerticalAlignment();

	// We flip the horizontal and vertical alignments when orientation is not default (horizontal).
	if (InOrientation == Orient_Vertical)
	{
		HAlign = VAlignToHAlign(VAlign);
		VAlign = HAlignToVAlign(HAlign);
	}

	switch (InOrientation)
	{
	case Orient_Horizontal:
	{
		switch (InFlowDirection)
		{
		default:
		case EFlowDirection::LeftToRight:
			return StaticCast<int32>(HAlign);
		case EFlowDirection::RightToLeft:
			switch (HAlign)
			{
			case HAlign_Left: return StaticCast<int32>(HAlign_Right);
			case HAlign_Right: return StaticCast<int32>(HAlign_Left);
			default: return StaticCast<int32>(HAlign);
			}
		}
		break;
	}
	case Orient_Vertical:
	{
		switch (InFlowDirection)
		{
		default:
		case EFlowDirection::LeftToRight:
			return StaticCast<int32>(VAlign);
		case EFlowDirection::RightToLeft:
			switch (VAlign)
			{
			case VAlign_Top: return StaticCast<int32>(VAlign_Bottom);
			case VAlign_Bottom: return StaticCast<int32>(VAlign_Top);
			default: return StaticCast<int32>(VAlign);
			}
		}
		break;
	}
	}
	return 0;
}

SOrientBox::FScopedWidgetSlotArguments SOrientBox::AddSlot(const int32 SlotIndex)
{
	if (!Children.IsValidIndex(SlotIndex))
	{
		// Insert at the end.
		return FScopedWidgetSlotArguments { MakeUnique<FSlot>(), Children, INDEX_NONE };
	}
	else
	{
		TWeakPtr<SOrientBox> OrientBoxWeak = SharedThis(this);
		return FScopedWidgetSlotArguments{ MakeUnique<FSlot>(), Children, SlotIndex, [OrientBoxWeak](const FSlot*, int32 SlotIndex) {} };
	}
}

void SOrientBox::RemoveSlot(const TSharedRef<SWidget>& WidgetToRemove)
{
	Children.Remove(WidgetToRemove);
}

void SOrientBox::ClearChildren()
{
	Children.Empty();
}

void SOrientBox::SetOrientation(const EOrientation InOrientation)
{
	CachedOrientation = InOrientation;

	Invalidate(EInvalidateWidget::Layout);
}

void SOrientBox::SetReversed(const bool bInReversed)
{
	CachedReverse = bInReversed;

	Invalidate(EInvalidateWidget::Layout);
}

bool SOrientBox::IsContentVertical() const
{
	return CachedOrientation == Orient_Vertical;
}

bool SOrientBox::IsContentHorizontal() const
{
	return CachedOrientation == Orient_Horizontal;
}

bool SOrientBox::IsContentReversed() const
{
	return Reverse.Get();
}

#undef LOCTEXT_NAMESPACE
