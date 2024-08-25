// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/Layers/SDMSlotLayerItem.h"

#include "ContentBrowserDataDragDropOp.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/MaterialStageInputs/DMMSITextureUV.h"
#include "DMPrivate.h"
#include "DragDrop/DMSlotLayerDragDropOperation.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Menus/DMMaterialSlotLayerAddEffectMenus.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "ScopedTransaction.h"
#include "Slate/Layers/SDMLayerEffectsView.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "SlateOptMacros.h"
#include "Components/DMMaterialEffectFunction.h"
#include "Components/DMMaterialEffectStack.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMSlotLayer"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMSlotLayerItem::Construct(const FArguments& InArgs, const TSharedPtr<SDMSlot>& InSlotWidget, const TSharedRef<STableViewBase>& InLayerView, 
	const TSharedPtr<FDMMaterialLayerReference>& InLayerItem)
{
	SlotWidgetWeak = InSlotWidget;
	LayerViewWeak = InLayerView;
	LayerItem = InLayerItem;

	StageBaseEnabled = InArgs._StageBaseEnabled;
	StageBaseSelected = InArgs._StageBaseSelected;
	StageMaskEnabled = InArgs._StageMaskEnabled;
	StageMaskSelected = InArgs._StageMaskSelected;
	PreviewSize = InArgs._PreviewSize;
	OnStageSelected = InArgs._OnStageSelected;
	OnLayerLinkToggled = InArgs._OnLayerLinkToggled;

	STableRow<TSharedPtr<FDMMaterialLayerReference>>::Construct(
		STableRow<TSharedPtr<FDMMaterialLayerReference>>::FArguments()
		.Padding(2.0f)
		.ShowSelection(true)
		.ToolTipText(this, &SDMSlotLayerItem::GetToolTipText)
		.Style(FDynamicMaterialEditorStyle::Get(), "LayerView.Row")
		.OnPaintDropIndicator(this, &SDMSlotLayerItem::OnLayerItemPaintDropIndicator)
		.OnCanAcceptDrop(this, &SDMSlotLayerItem::OnLayerItemCanAcceptDrop)
		.OnDragDetected(this, &SDMSlotLayerItem::OnLayerItemDragDetected)
		.OnDragEnter(this, &SDMSlotLayerItem::OnLayerItemDragEnter)
		.OnDragLeave(this, &SDMSlotLayerItem::OnLayerItemDragLeave)
		.OnAcceptDrop(this, &SDMSlotLayerItem::OnLayerItemAcceptDrop)
		, InLayerView
	);

	SetContent(CreateMainContent());
}

UDMMaterialLayerObject* SDMSlotLayerItem::GetLayer() const
{
	if (LayerItem.IsValid())
	{
		return LayerItem->GetLayer();
	}

	return nullptr;
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateMainContent()
{
	UDMMaterialEffectStack* EffectStack = nullptr;

	if (LayerItem.IsValid())
	{
		if (UDMMaterialLayerObject* Layer = LayerItem->GetLayer())
		{
			EffectStack = Layer->GetEffectStack();
		}
	}

	return 
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			CreateHeaderRowContent()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(10.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(this, &SDMSlotLayerItem::GetEffectsListVisibility)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(3.0f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.2f))
					.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Right"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(1.0f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.2f))
					.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Left"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(EffectsList, SDMLayerEffectsView, SlotWidgetWeak.Pin(), EffectStack)
			]
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateHeaderRowContent()
{
	return 
		SNew(SHorizontalBox)
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
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				CreateLayerBaseToggleButton()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 3.0f)
			[
				CreateStageBaseWidget(true, false,
					TAttribute<FVector2D>::CreateSP(this, &SDMSlotLayerItem::GetStagePreviewSize),
					FDMOnStageUniformSizeChanged::CreateSP(this, &SDMSlotLayerItem::SaveLayerPreviewSize))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				CreateLayerLinkToggleButton()
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				CreateLayerMaskToggleButton()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 3.0f)
			[
				CreateStageMaskWidget(true, false,
					TAttribute<FVector2D>::CreateSP(this, &SDMSlotLayerItem::GetStagePreviewSize),
					FDMOnStageUniformSizeChanged::CreateSP(this, &SDMSlotLayerItem::SaveLayerPreviewSize))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Fill)
		.Padding(2.0f, 0.0f, 10.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SAssignNew(LayerHeaderTextContainer, SBox)
				[
					CreateLayerHeaderText()
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.Text(this, &SDMSlotLayerItem::GetBlendModeText)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
				.Text(this, &SDMSlotLayerItem::GetStageDescription)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			CreateEffectsToggleButton()
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateStageBaseWidget(const bool bInteractable, const bool bInShowTextOverlays, TAttribute<FVector2D> InDesiredSize, FDMOnStageUniformSizeChanged InOnUniformSizeChanged)
{
	FProperty* PreviewSizeProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, LayerPreviewSize));
	ensure(PreviewSizeProperty);
	const int32 ClampMin = PreviewSizeProperty ? PreviewSizeProperty->GetIntMetaData("ClampMin") : 0;
	const int32 ClampMax = PreviewSizeProperty ? PreviewSizeProperty->GetIntMetaData("ClampMax") : 1;

	const UDMMaterialLayerObject* Layer = GetLayer();

	if (!Layer)
	{
		return SNullWidget::NullWidget;
	}

	return 
		SAssignNew(BaseStageWidget, SDMStage, Layer->GetStage(EDMMaterialLayerStage::Base))
		.Interactable(bInteractable)
		.StageEnabled(StageBaseEnabled)
		.StageSelected(StageBaseSelected)
		.StageIsMask(false)
		.ShowTextOverlays(bInShowTextOverlays)
		.DesiredSize(InDesiredSize)
		.OnClick(this, &SDMSlotLayerItem::OnStageClick)
		.OnCanAcceptDrop(this, &SDMSlotLayerItem::OnStageCanAcceptDrop)
		.OnAcceptDrop(this, &SDMSlotLayerItem::OnStageAcceptDrop)
		.MinUniformSize(ClampMin)
		.MaxUniformSize(ClampMax)
		.OnUniformSizeChanged(InOnUniformSizeChanged);
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateStageMaskWidget(const bool bInteractable, const bool bInShowTextOverlays, TAttribute<FVector2D> InDesiredSize, FDMOnStageUniformSizeChanged InOnUniformSizeChanged)
{
	FProperty* PreviewSizeProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, LayerPreviewSize));
	ensure(PreviewSizeProperty);
	const int32 ClampMin = PreviewSizeProperty ? PreviewSizeProperty->GetIntMetaData("ClampMin") : 0;
	const int32 ClampMax = PreviewSizeProperty ? PreviewSizeProperty->GetIntMetaData("ClampMax") : 1;

	const UDMMaterialLayerObject* Layer = GetLayer();

	if (!Layer)
	{
		return SNullWidget::NullWidget;
	}

	return
		SAssignNew(MaskStageWidget, SDMStage, Layer->GetStage(EDMMaterialLayerStage::Mask))
		.Interactable(bInteractable)
		.StageEnabled(StageMaskEnabled)
		.StageSelected(StageMaskSelected)
		.StageIsMask(true)
		.ShowTextOverlays(bInShowTextOverlays)
		.DesiredSize(InDesiredSize)
		.OnClick(this, &SDMSlotLayerItem::OnStageClick)
		.OnCanAcceptDrop(this, &SDMSlotLayerItem::OnStageCanAcceptDrop)
		.OnAcceptDrop(this, &SDMSlotLayerItem::OnStageAcceptDrop)
		.MinUniformSize(ClampMin)
		.MaxUniformSize(ClampMax)
		.OnUniformSizeChanged(InOnUniformSizeChanged);
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateHandleWidget()
{
	TSharedRef<SBox> LayerIndexTextHandleWidget = 
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Cursor(EMouseCursor::GrabHand)
		.ToolTipText(this, &SDMSlotLayerItem::GetToolTipText)
		[
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.BorderImage(this, &SDMSlotLayerItem::GetRowHandleBrush)
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.TextStyle(&FDynamicMaterialEditorStyle::Get(), "LayerView.Row.HeaderText.Small")
					.Justification(ETextJustify::Center)
					.Text(this, &SDMSlotLayerItem::GetLayerIndexText)
				]
			]
		];

	// Make sure all the index numbers align between single and double digit values.
	const float HandleThickness = 20.0f;

	LayerIndexTextHandleWidget->SetHeightOverride(HandleThickness);

	return LayerIndexTextHandleWidget;
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateLayerBypassButton()
{
	return 
		SNew(SButton)
		.ContentPadding(4.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("LayerBypassTooltip", "Toggle the bypassing of this layer."))
		.OnClicked(this, &SDMSlotLayerItem::OnCreateLayerBypassButtonClicked)
		[
			SNew(SImage)
			.DesiredSizeOverride(FVector2D(16.0f))
			.Image(this, &SDMSlotLayerItem::GetCreateLayerBypassButtonImage)
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateTogglesWidget()
{
	return
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				CreateLayerBaseToggleButton()
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Fill)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				CreateLayerMaskToggleButton()
			]
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateLayerBaseToggleButton()
{
	return
		SNew(SButton)
		.ContentPadding(0.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("MaterialLayerBaseToggleTooltip", "Toggle the layer base on and off.\n\n"
			"Warning: Toggling a layer base off may result in inputs being reset where incompatibilities are found.\n\n"
			"The base of the first layer cannot be toggled off."))
		.OnClicked(this, &SDMSlotLayerItem::OnBaseToggleButtonClicked)
		[
			SNew(SImage)
			.Image(this, &SDMSlotLayerItem::GetBaseToggleButtonImage)
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateLayerMaskToggleButton()
{
	return
		SNew(SButton)
		.ContentPadding(0.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("MaterialLayerMaskToggleTooltip", "Toggle the layer mask on and off."))
		.OnClicked(this, &SDMSlotLayerItem::OnLayerMaskToggleButtonClicked)
		[
			SNew(SImage)
			.Image(this, &SDMSlotLayerItem::GetLayerMaskToggleButtonImage)
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateLayerLinkToggleButton()
{
	return 
		SNew(SButton)
		.ContentPadding(0.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("MaterialStageLinkTooltip", "Click to toggle UV Link on and off."))
		.OnClicked(this, &SDMSlotLayerItem::OnLayerLinkToggleButton)
		[
			SNew(SImage)
			.Image(this, &SDMSlotLayerItem::GetLayerLinkToggleButtonImage)
			.Visibility(this, &SDMSlotLayerItem::GetLayerLinkToggleButtonVisibility)
		];
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateEffectsToggleButton()
{
	return 
		SNew(SButton)
		.ContentPadding(0.0f)
		.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly")
		.ToolTipText(LOCTEXT("MaterialLayerFxTooltip", "Toggle the visibility of this slot layer's effects.\n\nNot implemented."))
		.OnClicked(this, &SDMSlotLayerItem::OnEffectsToggleButtonClicked)
		[
			SNew(SImage)
			.Image(this, &SDMSlotLayerItem::GetEffectsToggleButtonImage)
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

UDMMaterialStage* SDMSlotLayerItem::GetBaseStage() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->GetStage(EDMMaterialLayerStage::Base);
	}

	return nullptr;
}

UDMMaterialStage* SDMSlotLayerItem::GetMaskStage() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->GetStage(EDMMaterialLayerStage::Mask);
	}

	return nullptr;
}

UDMMaterialSlot* SDMSlotLayerItem::GetSlot() const
{
	if (UDMMaterialStage* const Stage = GetBaseStage())
	{
		if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			return Layer->GetSlot();
		}
	}

	return nullptr;
}

bool SDMSlotLayerItem::AreStagesLinked() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsTextureUVLinkEnabled();
	}

	return false;
}

bool SDMSlotLayerItem::IsBaseStageEnabled() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsStageEnabled(EDMMaterialLayerStage::Base);
	}

	return false;
}

bool SDMSlotLayerItem::IsMaskStageEnabled() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		return Layer->IsStageEnabled(EDMMaterialLayerStage::Mask);
	}

	return false;
}

FCursorReply SDMSlotLayerItem::OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const
{
	if (InCursorEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHandClosed);
	}
	else
	{
		return FCursorReply::Cursor(EMouseCursor::GrabHand);
	}
}

FReply SDMSlotLayerItem::OnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey MouseButton = InMouseEvent.GetEffectingButton();

	if (MouseButton == EKeys::LeftMouseButton)
	{

	}

	return STableRow<TSharedPtr<FDMMaterialLayerReference>>::OnMouseButtonDown(InMyGeometry, InMouseEvent);
}

int32 SDMSlotLayerItem::OnLayerItemPaintDropIndicator(EItemDropZone InItemDropZone, const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
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

TOptional<EItemDropZone> SDMSlotLayerItem::OnLayerItemCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, 
	TSharedPtr<FDMMaterialLayerReference> InSlotLayer) const
{
	if (!InSlotLayer.IsValid())
	{
		return TOptional<EItemDropZone>();
	}

	const UDMMaterialLayerObject* Layer = GetLayer();

	if (!Layer)
	{
		return TOptional<EItemDropZone>();
	}

	if (TSharedPtr<FDMSlotLayerDragDropOperation> SlotLayerDragDropOp = InDragDropEvent.GetOperationAs<FDMSlotLayerDragDropOperation>())
	{
		UDMMaterialLayerObject* DraggedSlotLayer = SlotLayerDragDropOp->GetLayer();
		if (!DraggedSlotLayer)
		{
			return TOptional<EItemDropZone>();
		}

		SlotLayerDragDropOp->SetToInvalidDropLocation();

		switch (InDropZone)
		{
			case EItemDropZone::AboveItem:
				if (Layer->CanMoveLayerAbove(DraggedSlotLayer))
				{
					SlotLayerDragDropOp->SetToValidDropLocation();
					return InDropZone;
				}
				break;

			case EItemDropZone::OntoItem:
			case EItemDropZone::BelowItem:
				if (Layer->CanMoveLayerBelow(DraggedSlotLayer))
				{
					SlotLayerDragDropOp->SetToValidDropLocation();
					return EItemDropZone::BelowItem;
				}
				break;
		}
	}
	else if (TSharedPtr<FContentBrowserDataDragDropOp> ContentBrowserDragDropOp = InDragDropEvent.GetOperationAs<FContentBrowserDataDragDropOp>())
	{
		bool bHasValidItem = false;

		for (const FAssetData& DraggedAsset : ContentBrowserDragDropOp->GetAssets())
		{
			UClass* AssetClass = DraggedAsset.GetClass(EResolveClass::Yes);

			if (AssetClass && AssetClass->IsChildOf(UMaterialFunctionInterface::StaticClass()))
			{
				return EItemDropZone::OntoItem;
			}
		}
	}

	return TOptional<EItemDropZone>();
}

void SDMSlotLayerItem::OnLayerItemDragEnter(const FDragDropEvent& InDragDropEvent)
{

}

void SDMSlotLayerItem::OnLayerItemDragLeave(const FDragDropEvent& InDragDropEvent)
{

}

FReply SDMSlotLayerItem::OnLayerItemDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	const bool bShouldDuplicate = InMouseEvent.IsAltDown();

	TSharedRef<FDMSlotLayerDragDropOperation> SlotLayerDragDropOp = MakeShared<FDMSlotLayerDragDropOperation>(SharedThis(this), bShouldDuplicate);

	return FReply::Handled().BeginDragDrop(SlotLayerDragDropOp);
}

FReply SDMSlotLayerItem::OnLayerItemAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, 
	TSharedPtr<FDMMaterialLayerReference> InSlotLayer)
{
	if (!InSlotLayer.IsValid())
	{
		return FReply::Handled();
	}

	const UDMMaterialLayerObject* DraggedOverLayer = GetLayer();

	if (!DraggedOverLayer)
	{
		return FReply::Handled();
	}

	if (TSharedPtr<FDMSlotLayerDragDropOperation> SlotLayerDragDropOp = InDragDropEvent.GetOperationAs<FDMSlotLayerDragDropOperation>())
	{
		if (!DraggedOverLayer->GetStage(EDMMaterialLayerStage::Base))
		{
			return FReply::Handled();
		}

		UDMMaterialLayerObject* DraggedLayer = SlotLayerDragDropOp->GetLayer();

		if (!DraggedLayer || !DraggedLayer->GetStage(EDMMaterialLayerStage::Base))
		{
			return FReply::Handled();
		}

		const int32 ThisLayerIndex = GetLayerItemIndex();

		if (ThisLayerIndex == INDEX_NONE)
		{
			return FReply::Handled();
		}
		
		UDMMaterialSlot* const Slot = DraggedLayer->GetSlot();

		if (!Slot)
		{
			return FReply::Handled();
		}

		{
			FScopedTransaction Transaction(LOCTEXT("MoveLayer", "Move Material Designer Layer"));
			Slot->Modify();
			Slot->MoveLayer(DraggedLayer, ThisLayerIndex);

			if (TSharedPtr<STableViewBase> LayerView = LayerViewWeak.Pin())
			{
				LayerView->RequestListRefresh();

				if (TSharedPtr<SDMSlot> SlotWidget = StaticCastSharedPtr<SDMSlotLayerView>(LayerView)->GetSlotWidget())
				{
					SlotWidget->InvalidateSlotSettingsRowWidget();
				}
			}
		}

		return FReply::Handled();
	}

	if (TSharedPtr<FContentBrowserDataDragDropOp> ContentBrowserDragDropOp = InDragDropEvent.GetOperationAs<FContentBrowserDataDragDropOp>())
	{
		UDMMaterialEffectStack* EffectStack = DraggedOverLayer->GetEffectStack();

		if (!EffectStack)
		{
			return FReply::Handled();
		}

		for (const FAssetData& DraggedAsset : ContentBrowserDragDropOp->GetAssets())
		{
			UClass* AssetClass = DraggedAsset.GetClass(EResolveClass::Yes);

			if (AssetClass && AssetClass->IsChildOf(UMaterialFunctionInterface::StaticClass()))
			{
				if (UMaterialFunctionInterface* MaterialFunction = Cast<UMaterialFunctionInterface>(DraggedAsset.GetAsset()))
				{
					UDMMaterialEffectFunction* EffectFunction = UDMMaterialEffect::CreateEffect<UDMMaterialEffectFunction>(EffectStack);
					EffectFunction->SetMaterialFunction(MaterialFunction);

					// Will successfully set the function if it's valid
					if (EffectFunction->GetMaterialFunction() == MaterialFunction)
					{
						EffectStack->AddEffect(EffectFunction);
					}
				}
			}
		}

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FText SDMSlotLayerItem::GetToolTipText() const
{
	const UDMMaterialLayerObject* Layer = GetLayer();
	if (!Layer)
	{
		return FText();
	}

	return Layer->GetComponentDescription();
}

FText SDMSlotLayerItem::GetLayerHeaderText() const
{
	const UDMMaterialLayerObject* Layer = GetLayer();
	if (!Layer)
	{
		return FText();
	}

	return Layer->GetComponentDescription();
}

FText SDMSlotLayerItem::GetLayerIndexText() const
{
	return FText::Format(LOCTEXT("LayerIndexText", "{0}"), GetLayerItemIndex());
}

FText SDMSlotLayerItem::GetBlendModeText() const
{
	if (UDMMaterialLayerObject* Layer = GetLayer())
	{
		if (UDMMaterialSlot* Slot = Layer->GetSlot())
		{
			if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
			{
				if (UDMMaterialProperty* Property = ModelEditorOnlyData->GetMaterialProperty(Layer->GetMaterialProperty()))
				{
					return Property->GetDescription();
				}
			}
		}
	}

	return LOCTEXT("Error", "Error");
}

FText SDMSlotLayerItem::GetStageDescription() const
{
	if (UDMMaterialLayerObject* Layer = GetLayer())
	{
		if (UDMMaterialStage* Stage = Layer->GetFirstEnabledStage(EDMMaterialLayerStage::All))
		{
			return Stage->GetComponentDescription();
		}
	}

	return GetDefault<UDMMaterialStage>()->GetComponentDescription();
}

int32 SDMSlotLayerItem::GetLayerItemIndex() const
{
	UDMMaterialLayerObject* Layer = GetLayer();

	if (!Layer)
	{
		return INDEX_NONE;
	}

	return Layer->FindIndex();
}

bool SDMSlotLayerItem::IsRootLayer() const
{
	return GetLayerItemIndex() == 0;
}

const FSlateBrush* SDMSlotLayerItem::GetRowHandleBrush() const
{
	static const FSlateBrush* const Default = FDynamicMaterialEditorStyle::GetBrush("LayerView.Row.Handle.Left");
	static const FSlateBrush* const Selected = FDynamicMaterialEditorStyle::GetBrush("LayerView.Row.Handle.Left.Select");
	static const FSlateBrush* const Hovered = FDynamicMaterialEditorStyle::GetBrush("LayerView.Row.Handle.Left.Hover");
	static const FSlateBrush* const SelectedHovered = FDynamicMaterialEditorStyle::GetBrush("LayerView.Row.Handle.Left.Select.Hover");

	const bool bSelected = IsSelected();
	const bool bHovered = IsHovered();

	if (bSelected && bHovered)
	{
		return SelectedHovered;
	}
	else if (bSelected)
	{
		return Selected;
	}
	else if (bHovered)
	{
		return Hovered;
	}
	else
	{
		return Default;
	}
}

void SDMSlotLayerItem::OnStageClick(const FPointerEvent& InMouseEvent, const TSharedRef<SDMStage>& InStageWidget)
{
	UDMMaterialStage* const ClickedStage = InStageWidget->GetStage();

	if (!ClickedStage)
	{
		return;
	}

	const FKey MouseButton = InMouseEvent.GetEffectingButton();

	if (MouseButton == EKeys::LeftMouseButton)
	{
		if (TSharedPtr<STableViewBase> TableViewBase = LayerViewWeak.Pin())
		{
			TSharedPtr<SListView<TSharedPtr<FDMMaterialLayerReference>>> LayerView = StaticCastSharedPtr<SListView<TSharedPtr<FDMMaterialLayerReference>>>(TableViewBase);
			LayerView->SetSelection(LayerItem);
		}

		if (SelectedStageWidget.IsValid())
		{
			OnStageSelected.ExecuteIfBound(false, SelectedStageWidget.ToSharedRef());
		}

		SelectedStageWidget = InStageWidget;
		OnStageSelected.ExecuteIfBound(true, InStageWidget);
	}
}

bool SDMSlotLayerItem::OnStageCanAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget) const
{
	return true;
}

FReply SDMSlotLayerItem::OnStageAcceptDrop(const FDragDropEvent& InDragDropEvent, const TSharedRef<SDMStage>& InStageWidget)
{
	const TSharedPtr<FDMSlotLayerDragDropOperation> SlotLayerDragDropOp = InDragDropEvent.GetOperationAs<FDMSlotLayerDragDropOperation>();
	if (!SlotLayerDragDropOp.IsValid())
	{
		return FReply::Unhandled();
	}

	auto ReturnInvalidHandled = [&SlotLayerDragDropOp]()
	{
		SlotLayerDragDropOp->SetToInvalidDropLocation();
		return FReply::Handled();
	};

	const UDMMaterialLayerObject* Layer = GetLayer();

	if (!Layer)
	{
		return ReturnInvalidHandled();
	}

	UDMMaterialLayerObject* DraggedLayerItem = SlotLayerDragDropOp->GetLayer();
	if (!DraggedLayerItem || !Layer->GetStage(EDMMaterialLayerStage::Base))
	{
		return ReturnInvalidHandled();
	}

	if (Layer == DraggedLayerItem)
	{
		SlotLayerDragDropOp->SetToValidDropLocation();
		return FReply::Handled();
	}

	UDMMaterialStage* ThisBaseStage = Layer->GetStage(EDMMaterialLayerStage::Base);
	UDMMaterialStage* DraggedBaseStage = DraggedLayerItem->GetStage(EDMMaterialLayerStage::Base);
	if (!ThisBaseStage || !DraggedBaseStage)
	{
		return ReturnInvalidHandled();
	}

	const int32 ThisBaseStageIndex = Layer->FindIndex();
	const int32 DraggedBaseStageIndex = DraggedLayerItem->FindIndex();
	if (ThisBaseStageIndex == INDEX_NONE || DraggedBaseStageIndex == INDEX_NONE)
	{
		return ReturnInvalidHandled();
	}

	const UDMMaterialSlot* const ThisBaseSlot = Layer->GetSlot();
	const UDMMaterialSlot* const DraggedBaseSlot = DraggedLayerItem->GetSlot();
	if (!ThisBaseSlot || !DraggedBaseSlot)
	{
		return ReturnInvalidHandled();
	}

	if (ThisBaseSlot != DraggedBaseSlot)
	{
		return ReturnInvalidHandled();
	}

	const UDMMaterialLayerObject* PreviousLayer = Layer->GetPreviousLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);
	const UDMMaterialLayerObject* NextLayer = DraggedLayerItem->GetNextLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);

	if (PreviousLayer || NextLayer)
	{
		if (ThisBaseStageIndex < DraggedBaseStageIndex)
		{
			if (PreviousLayer)
			{
				const bool bIsCompatibleWithBase = DraggedBaseStage->IsCompatibleWithPreviousStage(PreviousLayer->GetStage(EDMMaterialLayerStage::Base));
				const bool bIsCompatibleWithNextStage = DraggedBaseStage->IsCompatibleWithNextStage(ThisBaseStage);
				if (!bIsCompatibleWithBase || !bIsCompatibleWithNextStage)
				{
					return ReturnInvalidHandled();
				}
			}
		}
		else
		{
			if (NextLayer)
			{
				const bool bIsCompatibleWithPreviousStage = DraggedBaseStage->IsCompatibleWithPreviousStage(ThisBaseStage);
				const bool bIsCompatibleWithNextStage = DraggedBaseStage->IsCompatibleWithNextStage(NextLayer->GetStage(EDMMaterialLayerStage::Base));
				if (!bIsCompatibleWithPreviousStage || !bIsCompatibleWithNextStage)
				{
					return ReturnInvalidHandled();
				}
			}
		}

		PreviousLayer = DraggedLayerItem->GetPreviousLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);
		NextLayer = DraggedLayerItem->GetNextLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);
		if (PreviousLayer && NextLayer)
		{
			if (!PreviousLayer->GetStage(EDMMaterialLayerStage::Base)->IsCompatibleWithNextStage(NextLayer->GetStage(EDMMaterialLayerStage::Base)))
			{
				return ReturnInvalidHandled();
			}
			if (!NextLayer->GetStage(EDMMaterialLayerStage::Base)->IsCompatibleWithPreviousStage(PreviousLayer->GetStage(EDMMaterialLayerStage::Base)))
			{
				return ReturnInvalidHandled();
			}
		}
	}

	UDMMaterialSlot* Slot = DraggedLayerItem->GetSlot();
	if (!Slot)
	{
		return FReply::Unhandled();
	}

	{
		FScopedTransaction Transaction(LOCTEXT("MoveLayer", "Move Material Designer Layer"));
		Slot->Modify();
		Slot->MoveLayer(DraggedLayerItem, ThisBaseStageIndex);
	}

	return FReply::Handled();
}

void SDMSlotLayerItem::SaveLayerPreviewSize(const float InNewSize, const TSharedRef<SDMStage>& InStageWidget)
{
	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();
	Settings->LayerPreviewSize = InNewSize;
	Settings->SaveConfig();
}

EVisibility SDMSlotLayerItem::GetEffectsListVisibility() const
{
	return EffectsList->GetLayerItemCount() > 0 && bDisplayEffectsList ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateNewEffectMenu()
{
	if (LayerItem.IsValid())
	{
		return FDMMaterialSlotLayerAddEffectMenus::OpenAddEffectMenu(LayerItem->GetLayer());
	}

	return SNullWidget::NullWidget;
}

FReply SDMSlotLayerItem::OnCreateLayerBypassButtonClicked()
{
	if (UDMMaterialLayerObject* Layer = GetLayer())
	{
		FScopedTransaction Transaction(LOCTEXT("ToggledLayerVisibility", "Toggle Material Designer Layer Visibility"));
		Layer->Modify();
		Layer->SetEnabled(!Layer->IsEnabled());

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

const FSlateBrush* SDMSlotLayerItem::GetCreateLayerBypassButtonImage() const
{
	static const FSlateBrush* const ExposeBrush = FCoreStyle::Get().GetBrush("Kismet.VariableList.ExposeForInstance");
	static const FSlateBrush* const HideBrush = FCoreStyle::Get().GetBrush("Kismet.VariableList.HideForInstance");

	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		if (Layer->IsEnabled())
		{
			return ExposeBrush;
		}
	}

	return HideBrush;
}

FReply SDMSlotLayerItem::OnBaseToggleButtonClicked()
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		constexpr EDMMaterialLayerStage StageType = EDMMaterialLayerStage::Base;

		if (UDMMaterialStage* Stage = Layer->GetStage(StageType))
		{
			FScopedTransaction Transaction(LOCTEXT("ToggleBaseStageEnabled", "Toggle Material Designer Base Stage Enabled"));
			Stage->Modify();

			if (!Stage->SetEnabled(!Stage->IsEnabled()))
			{
				Transaction.Cancel();
			}
		}
	}

	return FReply::Handled();
}

const FSlateBrush* SDMSlotLayerItem::GetBaseToggleButtonImage() const
{
	static const FSlateBrush* Disabled = FDynamicMaterialEditorStyle::GetBrush("Icons.Stage.Disabled");
	static const FSlateBrush* Enabled = FDynamicMaterialEditorStyle::GetBrush("Icons.Stage.Enabled");

	if (!IsBaseStageEnabled())
	{
		return Disabled;
	}

	return Enabled;
}

FReply SDMSlotLayerItem::OnLayerMaskToggleButtonClicked()
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		constexpr EDMMaterialLayerStage StageType = EDMMaterialLayerStage::Mask;

		if (UDMMaterialStage* Stage = Layer->GetStage(StageType))
		{
			FScopedTransaction Transaction(LOCTEXT("ToggleMaskStageEnabled", "Toggle Material Designer Mask Stage Enabled"));
			Stage->Modify();

			if (!Stage->SetEnabled(!Stage->IsEnabled()))
			{
				Transaction.Cancel();
			}
		}
	}

	return FReply::Handled();
}

const FSlateBrush* SDMSlotLayerItem::GetLayerMaskToggleButtonImage() const
{
	static const FSlateBrush* Disabled = FDynamicMaterialEditorStyle::GetBrush("Icons.Stage.Disabled");
	static const FSlateBrush* Enabled = FDynamicMaterialEditorStyle::GetBrush("Icons.Stage.Enabled");

	if (!IsMaskStageEnabled())
	{
		return Disabled;
	}

	return Enabled;
}

FReply SDMSlotLayerItem::OnLayerLinkToggleButton()
{
	if (UDMMaterialLayerObject* Layer = GetLayer())
	{
		FScopedTransaction Transaction(LOCTEXT("UVLayerLinkToggle", "Toggle Material Designer Layer UV Link"));
		Layer->Modify();
		Layer->ToggleTextureUVLinkEnabled();

		if (SelectedStageWidget.IsValid())
		{
			OnLayerLinkToggled.ExecuteIfBound(Layer->IsTextureUVLinkEnabled(), SelectedStageWidget.ToSharedRef());
		}
	}

	return FReply::Handled();
}

const FSlateBrush* SDMSlotLayerItem::GetLayerLinkToggleButtonImage() const
{
	static const FSlateBrush* const Unlinked = FDynamicMaterialEditorStyle::GetBrush("Icons.Stage.ChainUnlinked.Vertical");
	static const FSlateBrush* const Linked = FDynamicMaterialEditorStyle::GetBrush("Icons.Stage.ChainLinked.Vertical");

	return AreStagesLinked() ? Linked : Unlinked;
}

EVisibility SDMSlotLayerItem::GetLayerLinkToggleButtonVisibility() const
{
	if (const UDMMaterialLayerObject* Layer = GetLayer())
	{
		if (UDMMaterialStage* BaseStage = Layer->GetStage(EDMMaterialLayerStage::Base))
		{
			if (UDMMaterialStageSource* const StageSource = BaseStage->GetSource())
			{
				if (StageSource->IsA<UDMMaterialStageInputTextureUV>())
				{
					return EVisibility::Visible;
				}

				if (const UDMMaterialStageThroughput* const Throughput = Cast<UDMMaterialStageThroughput>(StageSource))
				{
					if (Throughput->SupportsLayerMaskTextureUVLink())
					{
						return EVisibility::Visible;
					}
				}
			}
		}
	}

	return EVisibility::Hidden;
}

FReply SDMSlotLayerItem::OnEffectsToggleButtonClicked()
{
	bDisplayEffectsList = !bDisplayEffectsList;

	return FReply::Handled();
}

const FSlateBrush* SDMSlotLayerItem::GetEffectsToggleButtonImage() const
{
	static const FSlateBrush* Displayed = FDynamicMaterialEditorStyle::GetBrush("EffectsView.Row.Fx.Opened");
	static const FSlateBrush* Hidden = FDynamicMaterialEditorStyle::GetBrush("EffectsView.Row.Fx.Closed");

	if (bDisplayEffectsList)
	{
		return Displayed;
	}

	return Hidden;
}

FVector2D SDMSlotLayerItem::GetStagePreviewSize() const
{
	return FVector2D(PreviewSize.Get());
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateLayerHeaderText() const
{
	TSharedRef<SWidget> TextBlock = SNew(STextBlock)
		.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		.Text(this, &SDMSlotLayerItem::GetLayerHeaderText)
		.ColorAndOpacity(FSlateColor(EStyleColor::PrimaryHover));

	TextBlock->SetOnMouseButtonDown(FPointerEventHandler::CreateSPLambda(
		this,
		[this](const FGeometry& InGeometry, const FPointerEvent& InPointerEvent)
		{
			if (LayerHeaderTextContainer.IsValid())
			{
				TSharedRef<SWidget> EditableContent = CreateLayerHeaderEditableText();
				LayerHeaderTextContainer->SetContent(EditableContent);
				FSlateApplication::Get().SetKeyboardFocus(EditableContent);
			}

			return FReply::Handled();
		}
	));

	return TextBlock;
}

TSharedRef<SWidget> SDMSlotLayerItem::CreateLayerHeaderEditableText() const
{
	FText LayerName = FText::GetEmpty();

	if (const UDMMaterialStage* Stage = GetBaseStage())
	{
		if (const UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (!Layer->GetLayerName().IsEmpty())
			{
				LayerName = Layer->GetLayerName();
			}
		}
	}

	return SNew(SEditableTextBox)
		.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
		.HintText(LOCTEXT("LayerName", "Layer Name"))
		.IsEnabled(true)
		.Text(LayerName)
		.Style(FDynamicMaterialEditorStyle::Get(), "InlineEditableTextBoxStyle")
		.OnTextCommitted(FOnTextCommitted::CreateSPLambda(
			this,
			[this](const FText& InText, ETextCommit::Type InCommitType)
			{
				if (LayerHeaderTextContainer.IsValid())
				{
					if (InCommitType != ETextCommit::OnUserMovedFocus && InCommitType != ETextCommit::OnCleared)
					{
						if (const UDMMaterialStage* Stage = GetBaseStage())
						{
							if (UDMMaterialLayerObject* Layer = Stage->GetLayer())
							{
								FScopedTransaction Transaction(LOCTEXT("ChangeLayerName", "Material Designer Change Layer Name"));
								Layer->Modify();
								Layer->SetLayerName(InText);
							}
						}
					}

					LayerHeaderTextContainer->SetContent(CreateLayerHeaderText());
				}
			}));
}

void SDMSlotLayerItem::DeselectAllEffects()
{
	EffectsList->ClearSelection();
}

#undef LOCTEXT_NAMESPACE
