// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SRigHierarchyTagWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "DetailLayoutBuilder.h"
#include "ControlRigDragOps.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "SRigHierarchyTagWidget"

//////////////////////////////////////////////////////////////
/// SRigHierarchyTagWidget
///////////////////////////////////////////////////////////

void SRigHierarchyTagWidget::Construct(const FArguments& InArgs)
{
	Text = InArgs._Text;
	Icon = InArgs._Icon;
	IconColor = InArgs._IconColor;
	Color = InArgs._Color;
	Radius = InArgs._Radius;
	Padding = InArgs._Padding;
	ContentPadding = InArgs._ContentPadding;
	Identifier = InArgs._Identifier;
	bAllowDragDrop = InArgs._AllowDragDrop;
	OnClicked = InArgs._OnClicked;
	OnRenamed = InArgs._OnRenamed;
	OnVerifyRename = InArgs._OnVerifyRename;

	const FMargin CombinedPadding = ContentPadding + Padding;

	SetVisibility(InArgs._Visibility);

	ChildSlot
	[
		SNew(SHorizontalBox)
		.ToolTipText(InArgs._TooltipText)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(CombinedPadding.Left, CombinedPadding.Top, 0, CombinedPadding.Bottom)
		[
			SNew(SImage)
			.Visibility_Lambda([this]()
			{
				return Icon.Get() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.Image(Icon)
			.ColorAndOpacity(IconColor)
			.DesiredSizeOverride(InArgs._IconSize)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(TAttribute<FMargin>::CreateSP(this, &SRigHierarchyTagWidget::GetTextPadding))
		[
			SNew(SInlineEditableTextBlock)
			.Text(Text)
			.ColorAndOpacity(InArgs._TextColor)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.IsReadOnly_Lambda([this](){ return !OnRenamed.IsBound(); })
			.OnTextCommitted(this, &SRigHierarchyTagWidget::HandleElementRenamed)
			.OnVerifyTextChanged(this, &SRigHierarchyTagWidget::HandleVerifyRename)
		]
	];
}

int32 SRigHierarchyTagWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateRoundedBoxBrush RoundedBoxBrush(FLinearColor::White, Radius);
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			AllottedGeometry.GetLocalSize() - Padding.GetDesiredSize(),
			FSlateLayoutTransform(FVector2d(Padding.Left, Padding.Top))
		),
		&RoundedBoxBrush,
		ESlateDrawEffect::NoPixelSnapping,
		Color.Get()
	);
	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FReply SRigHierarchyTagWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if(OnClicked.IsBound())
		{
			OnClicked.Execute();
		}
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}
	return FReply::Unhandled();
}

FReply SRigHierarchyTagWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if(bAllowDragDrop && (Identifier.IsSet() || Identifier.IsBound())) 
	{
		if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && !Identifier.Get().IsEmpty())
		{
			TSharedRef<FRigHierarchyTagDragDropOp> DragDropOp = FRigHierarchyTagDragDropOp::New(SharedThis(this));

			FRigElementKey DraggedKey;
			FRigElementKey::StaticStruct()->ImportText(*Identifier.Get(), &DraggedKey, nullptr, EPropertyPortFlags::PPF_None, nullptr, FRigElementKey::StaticStruct()->GetName(), true);

			if(DraggedKey.IsValid())
			{
				(void)OnElementKeyDragDetectedDelegate.ExecuteIfBound(DraggedKey);
			}

			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}
	return FReply::Unhandled();
}

FMargin SRigHierarchyTagWidget::GetTextPadding() const
{
	const FMargin CombinedPadding = ContentPadding + Padding;
	if(Icon.IsBound() || Icon.IsSet())
	{
		if(Icon.Get())
		{
			return FMargin(ContentPadding.Left * 2, ContentPadding.Top, ContentPadding.Right, ContentPadding.Bottom);
		}
	}
	return CombinedPadding;
}

void SRigHierarchyTagWidget::HandleElementRenamed(const FText& InNewName, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		if (OnRenamed.IsBound())
		{
			OnRenamed.Execute(InNewName, InCommitType);
		}
	}
}

bool SRigHierarchyTagWidget::HandleVerifyRename(const FText& InText, FText& OutError)
{
	if (OnVerifyRename.IsBound())
	{
		return OnVerifyRename.Execute(InText, OutError);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
