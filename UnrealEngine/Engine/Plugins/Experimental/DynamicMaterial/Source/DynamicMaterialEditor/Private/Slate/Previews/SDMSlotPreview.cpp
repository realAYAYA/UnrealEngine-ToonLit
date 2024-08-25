// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMSlotPreview.h"
#include "Components/DMMaterialSlot.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#include "Slate/SMaterialToolTip.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Views/STableViewBase.h"

SDMSlotPreview::SDMSlotPreview()
	: Brush(FSlateMaterialBrush(FVector2D(1.f, 1.f)))
{
}

SDMSlotPreview::~SDMSlotPreview()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialSlot* Slot = SlotWeak.Get())
	{
		Slot->GetOnUpdate().RemoveAll(this);
	}
}

void SDMSlotPreview::Construct(const FArguments& InArgs, UDMMaterialSlot* InSlot, EDMMaterialLayerStage InLayerStage)
{
	SlotWeak = InSlot;
	LayerStage = InLayerStage;

	PreviewSize = InArgs._PreviewSize;

	SetCanTick(true);

	Brush.SetImageSize(PreviewSize.Get());

	if (ensure(IsValid(InSlot)))
	{
		InSlot->GetOnUpdate().AddSP(this, &SDMSlotPreview::OnSlotUpdated);
		OnSlotUpdated(InSlot, EDMUpdateType::Structure);

		SetToolTipText(FText());
		SetToolTip(
			SNew(SToolTip)
			.IsInteractive(false)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.Background"))
			[
				SNew(SMaterialToolTip)
				.Material(InSlot->GetPreviewMaterial(InLayerStage))
				.Text(InArgs._ToolTipText)
				.MaterialSize_Lambda([]() { return FVector2D(UDynamicMaterialEditorSettings::Get()->TooltipTextureSize); })
				.ShowMaterial_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->bShowTooltipPreview; })
			]
		);
	}
	
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(2.f)
		[
			SNew(SImage)
			.Image(&Brush)
			.DesiredSizeOverride(this, &SDMSlotPreview::GetPreviewSizeInner)
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		[
			SNew(SImage)
			.DesiredSizeOverride(this, &SDMSlotPreview::GetPreviewSizeInner)
			.Image(FDynamicMaterialEditorStyle::GetBrush("ImageBorder"))
			.Cursor(EMouseCursor::Hand)
			.OnMouseButtonDown(InArgs._OnMouseButtonDown)
		]
	];
}

void SDMSlotPreview::OnSlotUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (UDMMaterialSlot* Slot = Cast<UDMMaterialSlot>(InComponent))
	{
		if (Slot == SlotWeak.Get() && IsValid(Slot) && Slot->IsComponentValid())
		{
			UMaterial* PreviewMaterial = Slot->GetPreviewMaterial(LayerStage);

			if (Brush.GetResourceObject() != PreviewMaterial)
			{
				Brush.SetMaterial(PreviewMaterial);
				PreviewMaterialWeak = PreviewMaterial;
			}
		}
	}
}

TOptional<FVector2D> SDMSlotPreview::GetPreviewSizeInner() const
{
	return PreviewSize.Get();
}

TOptional<FVector2D> SDMSlotPreview::GetPreviewSizeOuter() const
{
	return PreviewSize.Get() + FVector2D(2.0f);
}

void SDMSlotPreview::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (!PreviewMaterialWeak.IsValid())
	{
		Brush.SetMaterial(nullptr);
	}

	if (UDMMaterialComponent::CanClean())
	{
		if (UDMMaterialSlot* Slot = SlotWeak.Get())
		{
			if (Slot->IsComponentValid() && Slot->NeedsClean())
			{
				Slot->DoClean();
				OnSlotUpdated(Slot, EDMUpdateType::Structure);
			}
		}
	}
}

FCursorReply SDMSlotPreview::OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const
{
	if (InCursorEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}
	return SCompoundWidget::OnCursorQuery(InMyGeometry, InCursorEvent);
}

FReply SDMSlotPreview::OnMouseMove(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton) && !InMouseEvent.IsTouchEvent())
	{
		FReply Reply = FReply::Handled();

		if (!HasMouseCapture())
		{
			CachedCursorLocation = InMouseEvent.GetScreenSpacePosition();
			CachedPreviewSize = UDynamicMaterialEditorSettings::Get()->SlotPreviewSize;

			Reply.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
		}

		FTableViewDimensions CursorDeltaDimensions(Orient_Vertical, InMouseEvent.GetCursorDelta());
		CursorDeltaDimensions.LineAxis = 0.0f;
		const float SizeChange = CursorDeltaDimensions.ScrollAxis / InMyGeometry.Scale;

		FProperty* PreviewSizeProperty = UDynamicMaterialEditorSettings::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UDynamicMaterialEditorSettings, SlotPreviewSize));
		ensure(PreviewSizeProperty);
		const int32 ClampMin = PreviewSizeProperty ? PreviewSizeProperty->GetIntMetaData("ClampMin") : 0;
		const int32 ClampMax = PreviewSizeProperty ? PreviewSizeProperty->GetIntMetaData("ClampMax") : 1;

		CachedPreviewSize += SizeChange;
		CachedPreviewSize = FMath::Clamp(CachedPreviewSize, ClampMin, ClampMax);

		UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();
		Settings->SlotPreviewSize = CachedPreviewSize;
		Settings->SaveConfig();

		return Reply;
	}

	return SCompoundWidget::OnMouseMove(InMyGeometry, InMouseEvent);
}

FReply SDMSlotPreview::OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		FReply Reply = FReply::Handled().ReleaseMouseCapture();
		if (HasMouseCapture())
		{
			Reply.SetMousePos(FIntPoint(CachedCursorLocation.X, CachedCursorLocation.Y));
		}
		return Reply;
	}
	return SCompoundWidget::OnMouseButtonUp(InMyGeometry, InMouseEvent);
}
