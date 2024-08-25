// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Layers/SDMLayerEffectsItem.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialStage.h"
#include "DragDrop/DMLayerEffectsDragDropOperation.h"
#include "DynamicMaterialEditorStyle.h"
#include "ScopedTransaction.h"
#include "Slate/SDMStage.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"

#define LOCTEXT_NAMESPACE "SDMLayerEffectsItem"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

FDMEffectsLayerItem::FDMEffectsLayerItem(UDMMaterialEffect* InMaterialEffect)
{
	MaterialEffectWeak = InMaterialEffect;
}

void SDMLayerEffectsItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InEffectsView, 
	const TSharedPtr<FDMEffectsLayerItem>& InLayerItem)
{
	EffectsViewWeak = InEffectsView;
	LayerItem = InLayerItem;
	bEffectSelected = false;

	OnEffectSelected = InArgs._OnEffectSelected;

	STableRow<TSharedPtr<FDMEffectsLayerItem>>::Construct(
		STableRow<TSharedPtr<FDMEffectsLayerItem>>::FArguments()
		.Padding(2.0f)
		.ShowSelection(true)
		.ToolTipText(this, &SDMLayerEffectsItem::GetToolTipText)
		.Style(FDynamicMaterialEditorStyle::Get(), "EffectsView.Row")
		.OnPaintDropIndicator(this, &SDMLayerEffectsItem::OnLayerItemPaintDropIndicator)
		.OnCanAcceptDrop(this, &SDMLayerEffectsItem::OnLayerItemCanAcceptDrop)
		.OnDragDetected(this, &SDMLayerEffectsItem::OnLayerItemDragDetected)
		.OnDragEnter(this, &SDMLayerEffectsItem::OnLayerItemDragEnter)
		.OnDragLeave(this, &SDMLayerEffectsItem::OnLayerItemDragLeave)
		.OnAcceptDrop(this, &SDMLayerEffectsItem::OnLayerItemAcceptDrop)
		, InEffectsView
	);

	SetContent(CreateMainContent());
}

TSharedRef<SWidget> SDMLayerEffectsItem::CreateMainContent()
{
	return
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)
			.ToolTipText(this, &SDMLayerEffectsItem::GetToolTipText)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				CreateLayerBypassButton()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SDMLayerEffectsItem::GetLayerHeaderText)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 10.0f, 0.0f)
			[
				CreateLayerRemoveButton()
			]
		];
}

TSharedRef<SWidget> SDMLayerEffectsItem::CreateLayerBypassButton()
{
	return 
		SNew(SButton)
		.ContentPadding(4.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("LayerBypassTooltip", "Toggle the bypassing of this layer."))
		.OnClicked(this, &SDMLayerEffectsItem::OnLayerBypassButtonClick)
		[
			SNew(SImage)
			.DesiredSizeOverride(FVector2D(16.0f))
			.Image(this, &SDMLayerEffectsItem::GetLayerBypassButtonImage)
		];
}

TSharedRef<SWidget> SDMLayerEffectsItem::CreateLayerRemoveButton()
{
	return 
		SNew(SButton)
		.ContentPadding(2.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("RemoveEffectTooltip", "Remove Effect"))
		.OnClicked(this, &SDMLayerEffectsItem::OnLayerRemoveButtonClick)
		[
			SNew(SImage)
			.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
			.DesiredSizeOverride(FVector2D(12.0f))
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FCursorReply SDMLayerEffectsItem::OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const
{
	if (InCursorEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}
	return STableRow<TSharedPtr<FDMEffectsLayerItem>>::OnCursorQuery(InMyGeometry, InCursorEvent);
}

int32 SDMLayerEffectsItem::OnLayerItemPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FSlateBrush* DropIndicatorBrush = GetDropIndicatorBrush(InItemDropZone);
	static float OffsetX = 10.0f;
	FVector2D Offset(OffsetX * GetIndentLevel(), 0.0f);

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(FVector2D(AllottedGeometry.GetLocalSize() - Offset), FSlateLayoutTransform(Offset)),
		DropIndicatorBrush,
		ESlateDrawEffect::None,
		DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	return LayerId;
}

TOptional<EItemDropZone> SDMLayerEffectsItem::OnLayerItemCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, 
	TSharedPtr<FDMEffectsLayerItem> InSlotLayer) const
{
	if (InSlotLayer.IsValid())
	{
		TSharedPtr<FDMLayerEffectsDragDropOperation> DragDropOperation = InDragDropEvent.GetOperationAs<FDMLayerEffectsDragDropOperation>();
		if (DragDropOperation.IsValid())
		{
			return InDropZone;
		}
	}

	return TOptional<EItemDropZone>();
}

void SDMLayerEffectsItem::OnLayerItemDragEnter(const FDragDropEvent& InDragDropEvent)
{

}

void SDMLayerEffectsItem::OnLayerItemDragLeave(const FDragDropEvent& InDragDropEvent)
{

}

FReply SDMLayerEffectsItem::OnLayerItemDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bShouldDuplicate = InMouseEvent.IsAltDown();

	TSharedRef<FDMLayerEffectsDragDropOperation> DragDropOperation = MakeShared<FDMLayerEffectsDragDropOperation>(SharedThis(this), bShouldDuplicate);

	return FReply::Handled().BeginDragDrop(DragDropOperation);
}

FReply SDMLayerEffectsItem::OnLayerItemAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone,
	TSharedPtr<FDMEffectsLayerItem> InSlotLayer)
{
	if (!LayerItem.IsValid())
	{
		return FReply::Handled();
	}

	UDMMaterialEffect* MaterialEffect = LayerItem->MaterialEffectWeak.Get();

	if (!MaterialEffect)
	{
		return FReply::Handled();
	}

	UDMMaterialEffectStack* MaterialEffectStack = MaterialEffect->GetEffectStack();

	if (!MaterialEffectStack)
	{
		return FReply::Handled();
	}

	TSharedPtr<FDMLayerEffectsDragDropOperation> DragDropOperation = InDragDropEvent.GetOperationAs<FDMLayerEffectsDragDropOperation>();

	if (!DragDropOperation.IsValid())
	{
		return FReply::Handled();
	}

	TSharedPtr<SDMLayerEffectsItem> DraggedWidget = DragDropOperation->GetLayerItemWidget();

	if (!DraggedWidget.IsValid())
	{
		return FReply::Handled();
	}

	TSharedPtr<FDMEffectsLayerItem> DraggedItem = DraggedWidget->GetLayerItem();

	if (!DraggedItem.IsValid())
	{
		return FReply::Handled();
	}

	UDMMaterialEffect* DraggedMaterialEffect = DraggedItem->MaterialEffectWeak.Get();

	if (!IsValid(DraggedMaterialEffect) || DraggedMaterialEffect == MaterialEffect)
	{
		return FReply::Handled();
	}

	UDMMaterialEffectStack* DraggedMaterialEffectStack = DraggedMaterialEffect->GetEffectStack();

	if (!IsValid(DraggedMaterialEffectStack) || DraggedMaterialEffectStack != MaterialEffectStack)
	{
		return FReply::Handled();
	}

	const int32 MaterialEffectIndex = MaterialEffect->FindIndex();

	if (MaterialEffectIndex == INDEX_NONE)
	{
		return FReply::Handled();
	}

	const int32 DraggedMaterialEffectIndex = DraggedMaterialEffect->FindIndex();

	if (DraggedMaterialEffectIndex != INDEX_NONE)
	{
		return FReply::Handled();
	}

	if (InDropZone == EItemDropZone::AboveItem)
	{
		MaterialEffectStack->MoveEffect(DraggedMaterialEffect, MaterialEffectIndex);
	}
	else
	{
		MaterialEffectStack->MoveEffect(DraggedMaterialEffect, MaterialEffectIndex + 1);
	}

	return FReply::Handled();
}

FText SDMLayerEffectsItem::GetToolTipText() const
{
	if (LayerItem.IsValid())
	{
		if (UDMMaterialEffect* MaterialEffect = LayerItem->MaterialEffectWeak.Get())
		{
			return MaterialEffect->GetEffectDescription();
		}
	}

	return FText::GetEmpty();
}

FText SDMLayerEffectsItem::GetLayerHeaderText() const
{
	if (LayerItem.IsValid())
	{
		if (UDMMaterialEffect* MaterialEffect = LayerItem->MaterialEffectWeak.Get())
		{
			return MaterialEffect->GetEffectName();
		}
	}

	return FText::GetEmpty();
}

void SDMLayerEffectsItem::OnEffectClick(const FPointerEvent& InMouseEvent, const TSharedRef<SDMStage>& InStageWidget)
{
	UDMMaterialStage* const ClickedStage = InStageWidget->GetStage();

	if (!ClickedStage)
	{
		return;
	}

	const FKey MouseButton = InMouseEvent.GetEffectingButton();

	if (MouseButton == EKeys::LeftMouseButton)
	{
		if (TSharedPtr<STableViewBase> TableViewBase = EffectsViewWeak.Pin())
		{
			TSharedPtr<SListView<TSharedPtr<FDMEffectsLayerItem>>> LayerView = StaticCastSharedPtr<SListView<TSharedPtr<FDMEffectsLayerItem>>>(TableViewBase);
			LayerView->SetSelection(LayerItem);

			// Trigger the unselected event
			for (TSharedPtr<FDMEffectsLayerItem> LayerItemIter : LayerView->GetItems())
			{
				if (LayerItemIter.IsValid() && LayerItemIter != LayerItem)
				{
					if (TSharedPtr<ITableRow> Widget = LayerView->WidgetFromItem(LayerItemIter))
					{
						TSharedPtr<SDMLayerEffectsItem> ItemWidget = StaticCastSharedPtr<SDMLayerEffectsItem>(Widget);

						if (ItemWidget->IsEffectSelected())
						{
							OnEffectSelected.ExecuteIfBound(false, ItemWidget.ToSharedRef());
						}
					}
				}
			}
		}

		OnEffectSelected.ExecuteIfBound(true, SharedThis(this));
	}
}

bool SDMLayerEffectsItem::OnEffectCanAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget) const
{
	return true;
}

FReply SDMLayerEffectsItem::OnEffectAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget)
{
	return FReply::Handled();
}

FReply SDMLayerEffectsItem::OnLayerBypassButtonClick()
{
	if (LayerItem.IsValid())
	{
		if (UDMMaterialEffect* Effect = LayerItem->MaterialEffectWeak.Get())
		{
			FScopedTransaction Transaction(LOCTEXT("ToggleEffectEnabled", "Material Designer Toggle Effect"));
			Effect->Modify();
			Effect->SetEnabled(!Effect->IsEnabled());
		}
	}

	return FReply::Handled();
}

const FSlateBrush* SDMLayerEffectsItem::GetLayerBypassButtonImage() const
{
	static const FSlateIcon VisibleIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visible");
	static const FSlateIcon InvisibleIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Hidden");

	if (LayerItem.IsValid())
	{
		if (UDMMaterialEffect* Effect = LayerItem->MaterialEffectWeak.Get())
		{
			if (Effect->IsEnabled())
			{
				return VisibleIcon.GetIcon();
			}
		}
	}

	return InvisibleIcon.GetIcon();
}

FReply SDMLayerEffectsItem::OnLayerRemoveButtonClick()
{
	if (LayerItem.IsValid())
	{
		if (UDMMaterialEffect* Effect = LayerItem->MaterialEffectWeak.Get())
		{
			if (UDMMaterialEffectStack* EffectStack = Effect->GetEffectStack())
			{
				FScopedTransaction Transaction(LOCTEXT("RemoveEffect", "Material Designer Remove Effect"));
				EffectStack->Modify();
				EffectStack->RemoveEffect(Effect);
			}
		}
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
