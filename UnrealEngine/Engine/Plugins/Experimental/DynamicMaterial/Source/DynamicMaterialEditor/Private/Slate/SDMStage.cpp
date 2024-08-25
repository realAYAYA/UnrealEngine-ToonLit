// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMStage.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragDrop/DMStageDragDropOperation.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "Engine/Texture.h"
#include "ScopedTransaction.h"
#include "SDMEditor.h"
#include "Slate/Previews/SDMStagePreview.h"
#include "Slate/SMaterialToolTip.h"
#include "SlateOptMacros.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScissorRectBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableViewBase.h"

#define LOCTEXT_NAMESPACE "SDMStage"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

SDMStage::~SDMStage()
{
	SDMEditor::ClearPropertyHandles(this);
}

void SDMStage::Construct(const FArguments& InArgs, UDMMaterialStage* InStage)
{
	ensure(IsValid(InStage));
	StageWeak = InStage;

	bStageEnabled = InArgs._StageEnabled;
	bStageSelected = InArgs._StageSelected;
	bIsMask = InArgs._StageIsMask;
	bInteractable = InArgs._Interactable;
	bShowTextOverlays = InArgs._ShowTextOverlays;
	DesiredSize = InArgs._DesiredSize;
	MinUniformSize = InArgs._MinUniformSize;
	MaxUniformSize = InArgs._MaxUniformSize;
	OnUniformSizeChanged = InArgs._OnUniformSizeChanged;
	OnClick = InArgs._OnClick;

	OnCanAcceptDropEvent = InArgs._OnCanAcceptDrop;
	OnAcceptDropEvent = InArgs._OnAcceptDrop;
	OnPaintDropIndicatorEvent = InArgs._OnPaintDropIndicator;
	
	FText ToolTipText;
	if (bInteractable.Get())
	{
		SetCursor(EMouseCursor::Hand);
		ToolTipText = LOCTEXT("MaterialStagePreviewTooltip", "Material Stage\n\nRight click for options.\nControl+LMB to remove.\nAlt+LMB to toggle.");
	}
	else
	{
		ToolTipText = LOCTEXT("MaterialStagePreviewTooltipNoInteraction", "Material Stage Preview");
	}

	SetToolTip(
		SNew(SToolTip)
		.IsInteractive(false)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolTip.Background"))
		[
			SNew(SMaterialToolTip)
			.Material(StageWeak->GetPreviewMaterial())
			.Text(ToolTipText)
			.MaterialSize_Lambda([]() { return FVector2D(UDynamicMaterialEditorSettings::Get()->TooltipTextureSize); })
			.ShowMaterial(this, &SDMStage::IsToolTipInteractable)
		]
	);

	const float BorderWidth = 1.0f;

	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.5f))
		.Padding(2.0f)
		.BorderImage(FAppStyle::Get().GetBrush("ContentBrowser.AssetTileItem.DropShadow"))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(BorderWidth)
			[
				SNew(SBorder)
				.Clipping(EWidgetClipping::ClipToBounds)
				.BorderBackgroundColor(FLinearColor::Transparent)
				[
					SNew(SBox)
					.Clipping(EWidgetClipping::ClipToBounds)
					.WidthOverride(this, &SDMStage::GetStagePreviewDesiredWidth)
					.HeightOverride(this, &SDMStage::GetStagePreviewDesiredHeight)
					[
						SNew(SDMStagePreview, StageWeak.Get())
						.PreviewSize(DesiredSize)
					]
				]
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				CreateTextBlockBackground()
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(1.0f)
			[
				SNew(SBorder)
				.BorderBackgroundColor(this, &SDMStage::GetBorderBrushBackgroundColor)
				.BorderImage(this, &SDMStage::GetBorderBrush)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBorder)
				.Visibility(this, &SDMStage::GetOverlayBorderBrushVisibility)
				.BorderImage(this, &SDMStage::GetOverlayBorderBrush)
			]
		]
	];
}

TSharedRef<SWidget> SDMStage::CreateTextBlockBackground()
{
	const float DesiredHeight = FMath::Min(DesiredSize.Get().Y / 4.0f, 20.0f);
	const int32 DefaultLabelFontSize = 12;

	return 
		SNew(SBox)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.HeightOverride(DesiredHeight)
		.Visibility(this, &SDMStage::GetTextBlockBackgroundVisibility)
		[
			SNew(SBorder)
			.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Stage.TextBlock"))
			[
				SNew(SScaleBox)
				.StretchDirection(EStretchDirection::Both)
				.Stretch(EStretch::ScaleToFitX)
				[
					SNew(STextBlock)
					.Margin(3.0f)
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.Text(this, &SDMStage::GetStageName)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", DefaultLabelFontSize))
				]
			]
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

EVisibility SDMStage::GetBorderVisibility() const
{
	if (bInteractable.Get())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Hidden;
}

const FSlateBrush* SDMStage::GetBorderBrush() const
{
	FString BrushName = "Stage.Inactive";

	if (!bInteractable.Get() || bStageSelected.Get())
	{
		return FDynamicMaterialEditorStyle::GetBrush(*BrushName);
	}

	if (!bStageEnabled.Get())
	{
		BrushName = "Stage.Disabled";
	}

	if (IsHovered())
	{
		BrushName.Append(".Hover");
	}

	return FDynamicMaterialEditorStyle::GetBrush(*BrushName);
}

const FSlateBrush* SDMStage::GetOverlayBorderBrush() const
{
	FString BrushName = "Stage.Inactive";

	if (!bInteractable.Get())
	{
		return FDynamicMaterialEditorStyle::GetBrush(*BrushName);
	}

	if (!bStageEnabled.Get())
	{
		BrushName = "Stage.Disabled";
	}

	if (bStageSelected.Get())
	{
		BrushName.Append(".Select");
	}

	if (IsHovered())
	{
		BrushName.Append(".Hover");
	}

	return FDynamicMaterialEditorStyle::GetBrush(*BrushName);
}

FText SDMStage::GetStageName() const
{
	return StageWeak.IsValid() ? StageWeak->GetComponentDescription() : FText::GetEmpty();
}

FCursorReply SDMStage::OnCursorQuery(const FGeometry& InMyGeometry, const FPointerEvent& InCursorEvent) const
{
	if (HasMouseCapture())
	{
		return FCursorReply::Cursor(EMouseCursor::None);
	}

	return SCompoundWidget::OnCursorQuery(InMyGeometry, InCursorEvent);
}

FReply SDMStage::OnMouseMove(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (OnUniformSizeChanged.IsBound() && InMouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
	{
		FReply Reply = FReply::Handled();

		if (!HasMouseCapture())
		{
			CachedCursorLocation = InMouseEvent.GetScreenSpacePosition();
			CachedPreviewSize = UDynamicMaterialEditorSettings::Get()->LayerPreviewSize;

			Reply.CaptureMouse(AsShared()).UseHighPrecisionMouseMovement(AsShared());
		}

		FTableViewDimensions CursorDeltaDimensions(Orient_Vertical, InMouseEvent.GetCursorDelta());
		const float SizeChange = CursorDeltaDimensions.ScrollAxis / InMyGeometry.Scale;

		CachedPreviewSize += SizeChange;
		CachedPreviewSize = FMath::Clamp(CachedPreviewSize, MinUniformSize, MaxUniformSize);

		OnUniformSizeChanged.Execute(CachedPreviewSize, SharedThis(this));

		return Reply;
	}

	return SCompoundWidget::OnMouseMove(InMyGeometry, InMouseEvent);
}

FReply SDMStage::OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	if (HasMouseCapture())
	{
		FReply Reply = FReply::Handled().ReleaseMouseCapture();

		if (OnUniformSizeChanged.IsBound() && InMouseEvent.IsMouseButtonDown(EKeys::MiddleMouseButton))
		{
			Reply.SetMousePos(FIntPoint(CachedCursorLocation.X, CachedCursorLocation.Y));
		}

		return Reply;
	}
	
	return SCompoundWidget::OnMouseButtonUp(InMyGeometry, InMouseEvent);
}

FReply SDMStage::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bInteractable.Get())
	{
		return FReply::Unhandled();
	}

	const FKey MouseButton = MouseEvent.GetEffectingButton();

	OnClick.ExecuteIfBound(MouseEvent, SharedThis(this));
	
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SDMStage::OnDragDetected(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	TSharedRef<FDMStageDragDropOperation> Operation = MakeShared<FDMStageDragDropOperation>(SharedThis(this));
	return FReply::Handled().BeginDragDrop(Operation);
}

void SDMStage::OnDragEnter(FGeometry const& InMyGeometry, FDragDropEvent const& InDragDropEvent)
{
	SCompoundWidget::OnDragEnter(InMyGeometry, InDragDropEvent);

	if (!bInteractable.Get())
	{
		return;
	}

	if (!StageWeak.IsValid())
	{
		return;
	}

	if (const TSharedPtr<FDMStageDragDropOperation> StageDragDropOp = InDragDropEvent.GetOperationAs<FDMStageDragDropOperation>())
	{
		HandleStageDragDropOperation(*StageDragDropOp);
	}
	else if (const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		if (UDMMaterialStage* DraggedOverStage = StageWeak.Get())
		{
			if (UDMMaterialLayerObject* DraggedOverStageLayer = DraggedOverStage->GetLayer())
			{
				UDMMaterialStage* LayerBase = DraggedOverStageLayer->GetStage(EDMMaterialLayerStage::Base);
				UDMMaterialStage* LayerMask = DraggedOverStageLayer->GetStage(EDMMaterialLayerStage::Mask);

				if ((LayerBase && LayerBase == DraggedOverStage && LayerBase->IsEnabled())
					|| (LayerMask && LayerMask == DraggedOverStage && LayerMask->IsEnabled()))
				{
					// @TODO: some visual cue for drag over
					//HighlightColor = FStyleColors::AccentBlue.GetSpecifiedColor();
					//HighlightColor.A = 0.3;
				}
			}
		}
	}
}

void SDMStage::OnDragLeave(FDragDropEvent const& InDragDropEvent)
{
	SCompoundWidget::OnDragLeave(InDragDropEvent);

	if (!bInteractable.Get())
	{
		return;
	}

	const TSharedPtr<FDMStageDragDropOperation> StageDragDropOp = InDragDropEvent.GetOperationAs<FDMStageDragDropOperation>();

	if (StageDragDropOp.IsValid())
	{
		StageDragDropOp->SetToValidDropLocation();
	}
}

FReply SDMStage::OnDragOver(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent)
{
	if (OnCanAcceptDropEvent.IsBound())
	{
		OnCanAcceptDropEvent.Execute(InDragDropEvent, SharedThis(this));
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SDMStage::OnDrop(const FGeometry& InMyGeometry, const FDragDropEvent& InDragDropEvent)
{
	if (!bInteractable.Get())
	{
		return FReply::Unhandled();
	}

	SCompoundWidget::OnDrop(InMyGeometry, InDragDropEvent);

	if (!StageWeak.IsValid())
	{
		return FReply::Unhandled();
	}

	FReply Reply = FReply::Unhandled();

	if (const TSharedPtr<FAssetDragDropOp> AssetDragDropOp = InDragDropEvent.GetOperationAs<FAssetDragDropOp>())
	{
		HandleAssetDragDropOperation(*AssetDragDropOp);
		Reply = FReply::Handled();
	}

	if (OnAcceptDropEvent.IsBound())
	{
		return OnAcceptDropEvent.Execute(InDragDropEvent, SharedThis(this));
	}

	return Reply;
}

void SDMStage::HandleStageDragDropOperation(FDMStageDragDropOperation& StageDragDropOperation)
{
	UDMMaterialStage* DraggedOverStage = StageWeak.Get();
	
	UDMMaterialStage* DraggedStage = StageDragDropOperation.GetStage();
	if (!IsValid(DraggedStage))
	{
		return;
	}

	if (DraggedStage == DraggedOverStage)
	{
		StageDragDropOperation.SetToValidDropLocation();
		return;
	}

	UDMMaterialLayerObject* DraggedStageLayer = DraggedStage->GetLayer();
	UDMMaterialLayerObject* DraggedOverStageLayer = DraggedOverStage->GetLayer();

	if (!DraggedStageLayer || !DraggedOverStageLayer)
	{
		StageDragDropOperation.SetToInvalidDropLocation();
		return;
	}

	if (DraggedStageLayer == DraggedOverStageLayer || DraggedStageLayer->GetSlot() != DraggedOverStageLayer->GetSlot())
	{
		StageDragDropOperation.SetToInvalidDropLocation();
		return;
	}

	const int32 DraggedIndex = DraggedStageLayer->FindIndex();
	const int32 DraggedOverIndex = DraggedOverStageLayer->FindIndex();
	if (DraggedIndex == INDEX_NONE || DraggedOverIndex == INDEX_NONE)
	{
		return;
	}

	bool bReject = false;

	if (DraggedOverIndex == 0 && !DraggedStageLayer->GetStage(EDMMaterialLayerStage::Base))
	{
		return;
	}

	if (!bReject)
	{
		const UDMMaterialLayerObject* PreviousLayer = DraggedOverStageLayer->GetPreviousLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);
		const UDMMaterialLayerObject* NextLayer = DraggedOverStageLayer->GetNextLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);

		if (PreviousLayer || NextLayer)
		{
			if (DraggedOverIndex < DraggedIndex)
			{
				if (PreviousLayer &&
					(!DraggedStage->IsCompatibleWithPreviousStage(PreviousLayer->GetStage(EDMMaterialLayerStage::Base))
						|| !DraggedStage->IsCompatibleWithNextStage(DraggedOverStage)))
				{
					bReject = true;
				}
			}
			else
			{
				if (NextLayer &&
					(!DraggedStage->IsCompatibleWithPreviousStage(DraggedOverStage)
						|| !DraggedStage->IsCompatibleWithNextStage(NextLayer->GetStage(EDMMaterialLayerStage::Base))))
				{
					bReject = true;
				}
			}

			if (!bReject)
			{
				PreviousLayer = DraggedStageLayer->GetPreviousLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);
				NextLayer = DraggedStageLayer->GetNextLayer(EDMMaterialPropertyType::Any, EDMMaterialLayerStage::None);

				if (PreviousLayer && NextLayer)
				{
					if (!PreviousLayer->GetStage(EDMMaterialLayerStage::Base)->IsCompatibleWithNextStage(NextLayer->GetStage(EDMMaterialLayerStage::Base)))
					{
						bReject = true;
					}

					if (!NextLayer->GetStage(EDMMaterialLayerStage::Base)->IsCompatibleWithPreviousStage(PreviousLayer->GetStage(EDMMaterialLayerStage::Base)))
					{
						bReject = true;
					}
				}
			}
		}
	}

	if (bReject)
	{
		StageDragDropOperation.SetToInvalidDropLocation();
		return;
	}

	StageDragDropOperation.SetToValidDropLocation();
}

void SDMStage::HandleAssetDragDropOperation(FAssetDragDropOp& AssetDragDropOperation)
{
	UDMMaterialStage* DraggedOverStage = StageWeak.Get();
	if (!IsValid(DraggedOverStage))
	{
		return;
	}

	const UDMMaterialLayerObject* StageLayer = DraggedOverStage->GetLayer();
	if (!StageLayer)
	{
		return;
	}

	if ((StageLayer->GetStage(EDMMaterialLayerStage::Base) == DraggedOverStage && !StageLayer->IsStageEnabled(EDMMaterialLayerStage::Base))
		|| (StageLayer->GetStage(EDMMaterialLayerStage::Mask) == DraggedOverStage && !StageLayer->IsStageEnabled(EDMMaterialLayerStage::Mask)))
	{
		return;
	}

	if (!DraggedOverStage->GetSource())
	{
		return;
	}

	UTexture* Texture = nullptr;

	for (const FAssetData& AssetData : AssetDragDropOperation.GetAssets())
	{
		UObject* Asset = AssetData.GetAsset();
		if (IsValid(Asset))
		{
			if (UTexture* TextureIter = Cast<UTexture>(Asset))
			{
				Texture = TextureIter;
				break;
			}
		}
	}

	if (!Texture)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("DragTextureOntoStage", "Material Designer Drag Texture onto Stage"), !FDMInitializationGuard::IsInitializing());
	UDMMaterialValueTexture* TextureValue = nullptr;

	DraggedOverStage->Modify();

	UDMMaterialStageSource* DraggedOverStageSource = DraggedOverStage->GetSource();

	if (DraggedOverStageSource->IsA<UDMMaterialStageBlend>())
	{
		UDMMaterialStageInputExpression* NewInput = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
			DraggedOverStage,
			UDMMaterialStageExpressionTextureSample::StaticClass(), 
			UDMMaterialStageBlend::InputB,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			0,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		UDMMaterialSubStage* SubStage = NewInput->GetSubStage();

		UDMMaterialStageInputValue* NewInputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			SubStage,
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			EDMValueType::VT_Texture,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		TextureValue = Cast<UDMMaterialValueTexture>(NewInputValue->GetValue());
	}
	else if (DraggedOverStageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
	{
		bool bHasAlpha = UE::DynamicMaterial::Private::HasAlpha(Texture);

		UDMMaterialStageInputExpression* NewInput = UDMMaterialStageInputExpression::ChangeStageInput_Expression(
			DraggedOverStage,
			UDMMaterialStageExpressionTextureSample::StaticClass(), 
			2,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			bHasAlpha ? 1 : 0,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		UDMMaterialSubStage* SubStage = NewInput->GetSubStage();

		UDMMaterialStageInputValue* NewInputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			SubStage,
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			EDMValueType::VT_Texture,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		TextureValue = Cast<UDMMaterialValueTexture>(NewInputValue->GetValue());
	}
	else
	{
		UDMMaterialStageExpression* NewExpression = DraggedOverStage->ChangeSource<UDMMaterialStageExpressionTextureSample>();

		UDMMaterialStageInputValue* NewInputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
			DraggedOverStage,
			0, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
			EDMValueType::VT_Texture,
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
		);

		TextureValue = Cast<UDMMaterialValueTexture>(NewInputValue->GetValue());
	}

	if (TextureValue)
	{
		TextureValue->SetValue(Texture);
	}
}

bool SDMStage::IsToolTipInteractable() const
{
	return bInteractable.Get() && UDynamicMaterialEditorSettings::Get()->bShowTooltipPreview;
}

FOptionalSize SDMStage::GetStagePreviewDesiredWidth() const
{
	return DesiredSize.Get().X;
}

FOptionalSize SDMStage::GetStagePreviewDesiredHeight() const
{
	return DesiredSize.Get().Y;
}

FSlateColor SDMStage::GetBorderBrushBackgroundColor() const
{
	return bInteractable.Get() ? FLinearColor::White : FLinearColor::Transparent;
}

EVisibility SDMStage::GetOverlayBorderBrushVisibility() const
{
	return bStageSelected.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Hidden;
}

EVisibility SDMStage::GetTextBlockBackgroundVisibility() const
{
	return bShowTextOverlays.Get() ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
