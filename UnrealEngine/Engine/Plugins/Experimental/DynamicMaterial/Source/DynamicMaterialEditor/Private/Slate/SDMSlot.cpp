// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMSlot.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageExpression.h"
#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialStageGradient.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageBlends/DMMSBNormal.h"
#include "Components/MaterialStageExpressions/DMMSESceneTexture.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIFunction.h"
#include "Components/MaterialStageInputs/DMMSIGradient.h"
#include "Components/MaterialStageInputs/DMMSISlot.h"
#include "Components/MaterialStageInputs/DMMSITextureUV.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "DetailLayoutBuilder.h"
#include "DMBlueprintFunctionLibrary.h"
#include "DMPrivate.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Menus/DMMaterialSlotLayerAddEffectMenus.h"
#include "Menus/DMMaterialSlotMenus.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Properties/Editors/SDMPropertyEditOpacity.h"
#include "ScopedTransaction.h"
#include "Slate/Layers/SDMSlotLayerItem.h"
#include "Slate/Layers/SDMSlotLayerView.h"
#include "Slate/Properties/SDMMaterialProperty.h"
#include "Slate/Properties/SDMSourceBlendType.h"
#include "Slate/SDMComponentEdit.h"
#include "Slate/SDMEditor.h"
#include "Slate/SDMStage.h"
#include "SlateOptMacros.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMSlot"

SDMSlot::~SDMSlot()
{
	if (OnEndFrameDelegateHandle.IsValid())
	{
		FCoreDelegates::OnEndFrame.Remove(OnEndFrameDelegateHandle);
	}

	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialSlot* Slot = SlotWeak.Get())
	{
		Slot->GetOnPropertiesUpdateDelegate().RemoveAll(this);
		Slot->GetOnLayersUpdateDelegate().RemoveAll(this);
	}

	SDMEditor::ClearPropertyHandles(this);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMSlot::Construct(const FArguments& InArgs, const TSharedRef<SDMEditor>& InEditor, UDMMaterialSlot* InSlot)
{
	EditorWidgetWeak = InEditor;
	SlotWeak = InSlot;

	SlotPreviewSize = InArgs._SlotPreviewSize;
	LayerPreviewSize = InArgs._LayerPreviewSize;

	if (ensure(IsValid(InSlot)))
	{
		InSlot->GetOnPropertiesUpdateDelegate().AddSP(this, &SDMSlot::OnSlotPropertiesUpdated);
		InSlot->GetOnLayersUpdateDelegate().AddSP(this, &SDMSlot::OnSlotLayersUpdated);

		for (const UDMMaterialLayerObject* Layer : InSlot->GetLayers())
		{
			Layer->ForEachValidStage(
				EDMMaterialLayerStage::All,
				[](UDMMaterialStage* InStage)
				{
					InStage->SetBeingEdited(false);
				});
		}

		RefreshMainWidget();

		bInvalidateMainWidget = false;
		bInvalidateHeaderWidget = false;
		bInvalidateSettingsWidget = false;
		bInvalidateComponentEditWidget = false;
		OnEndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddSP(this, &SDMSlot::HandleEndFrameRefresh);
	}
}

void SDMSlot::OnLayerSelected(TSharedPtr<FDMMaterialLayerReference> InLayerItem, const int32 InLayerIndex)
{
	InvalidateSlotSettingsRowWidget();
	InvalidateComponentEditWidget();
}

void SDMSlot::OnLayerStageSelected(const bool bInSelected, const TSharedRef<SDMStage>& InStageWidget)
{
	InvalidateSlotSettingsRowWidget();
	SetEditedComponent(InStageWidget->GetStage());
}

void SDMSlot::HandleEndFrameRefresh()
{
	if (bInvalidateMainWidget)
	{
		RefreshMainWidget();
	}
	else
	{
		if (bInvalidateHeaderWidget)
		{
			RefreshHeaderPropertyListWidget();
		}

		if (bInvalidateSettingsWidget)
		{
			RefreshSlotSettingsRowWidget();
		}

		if (bInvalidateComponentEditWidget)
		{
			RefreshComponentEditWidget();
		}
	}
}

FText SDMSlot::GetLayerButtonsDescription() const
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!Slot)
	{
		return FText::GetEmpty();
	}

	const int32 SlotLayerCount = Slot->GetLayers().Num();
	return SlotLayerCount == 1
		? LOCTEXT("SlotLayerInfo_OneLayer", "1 Layer")
		: FText::Format(LOCTEXT("SlotLayerInfo_MultipleLayers", "{0} Layers"), SlotLayerCount);
}

TSharedRef<SWidget> SDMSlot::GetLayerButtonsMenuContent()
{
	return FDMMaterialSlotMenus::MakeAddLayerButtonMenu(SharedThis(this));
}

bool SDMSlot::GetLayerCanAddEffect() const
{
	return !!GetSelectedLayer();
}

TSharedRef<SWidget> SDMSlot::GetLayerEffectsMenuContent()
{
	if (UDMMaterialLayerObject* LayerObject = GetSelectedLayer())
	{
		return FDMMaterialSlotLayerAddEffectMenus::OpenAddEffectMenu(LayerObject);
	}

	return SNullWidget::NullWidget;
}

bool SDMSlot::GetLayerRowsButtonsCanDuplicate() const
{
	return LayerView->GetSelectedItems().IsEmpty() == false;
}

FReply SDMSlot::OnLayerRowButtonsDuplicateClicked()
{
	if (TSharedPtr<SDMEditor> EditorWidget = EditorWidgetWeak.Pin())
	{
		if (EditorWidget->CanDuplicateSelectedLayer())
		{
			EditorWidget->DuplicateSelectedLayer();
		}
	}

	return FReply::Handled();
}

bool SDMSlot::GetLayerRowsButtonsCanRemove() const
{
	const UDMMaterialLayerObject* Layer = GetSelectedLayer();

	if (!Layer)
	{
		return false;
	}

	return CanRemoveLayerByIndex(Layer->FindIndex());
}

FReply SDMSlot::OnLayerRowButtonsRemoveClicked()
{
	const UDMMaterialLayerObject* Layer = GetSelectedLayer();

	if (!Layer)
	{
		return FReply::Unhandled();
	}

	RemoveLayerByIndex(Layer->FindIndex());

	return FReply::Handled();
}

UDMMaterialLayerObject* SDMSlot::GetSelectedLayer() const
{
	if (!LayerView.IsValid())
	{
		return nullptr;
	}

	const int32 SelectedLayerIndex = LayerView->GetSelectedLayerIndex();

	if (SelectedLayerIndex == INDEX_NONE)
	{
		return nullptr;
	}

	if (!LayerView->GetLayerItems().IsValidIndex(SelectedLayerIndex))
	{
		return nullptr;
	}

	return LayerView->GetLayerItems()[SelectedLayerIndex]->GetLayer();
}

void SDMSlot::SetSelectedLayer(UDMMaterialLayerObject* InLayer) const
{
	if (!LayerView.IsValid())
	{
		return;
	}

	LayerView->ClearSelection();
	LayerView->SelectLayerItem(InLayer, /* Mask */ false, /* Selected */ true, ESelectInfo::Direct);
}

UDMMaterialComponent* SDMSlot::GetEditedComponent() const
{
	return EditedComponent.Get();
}

void SDMSlot::SetEditedComponent(UDMMaterialComponent* InComponent)
{
	if (EditedComponent.IsValid())
	{
		EditedComponent->GetOnUpdate().RemoveAll(this);
	}

	EditedComponent = InComponent;

	if (EditedComponent.IsValid())
	{
		EditedComponent->GetOnUpdate().AddSP(this, &SDMSlot::OnComponentUpdated);
	}

	InvalidateComponentEditWidget();
}

int32 SDMSlot::GetSourceBlendTypeSwitcherIndex() const
{
	if (LayerView->GetLayerItems().IsValidIndex(LayerView->GetSelectedLayerIndex()))
	{
		if (TSharedPtr<SDMSlotLayerItem> LayerWidget = LayerView->WidgetFromLayerItem(LayerView->GetLayerItems()[LayerView->GetSelectedLayerIndex()]))
		{
			if (TSharedPtr<SDMStage> BaseStageWidget = LayerWidget->GetBaseStageWidget())
			{
				if (UDMMaterialStage* BaseStage = BaseStageWidget->GetStage())
				{
					return BaseStage->CanChangeSource() ? 2 : 1;
				}
			}
		}
	}

	return 0;
}

TSubclassOf<UDMMaterialStageBlend> SDMSlot::GetSelectedSourceBlendType() const
{
	if (LayerView->GetLayerItems().IsValidIndex(LayerView->GetSelectedLayerIndex()))
	{
		if (TSharedPtr<SDMSlotLayerItem> LayerWidget = LayerView->WidgetFromLayerItem(LayerView->GetLayerItems()[LayerView->GetSelectedLayerIndex()]))
		{
			if (TSharedPtr<SDMStage> BaseStageWidget = LayerWidget->GetBaseStageWidget())
			{
				if (UDMMaterialStage* BaseStage = BaseStageWidget->GetStage())
				{
					if (UDMMaterialStageSource* BaseStageSource = BaseStage->GetSource())
					{
						return BaseStageSource->GetClass();
					}
				}
			}
		}
	}

	return UDMMaterialStageBlendNormal::StaticClass();
}

void SDMSlot::OnSourceBlendTypedSelected(const TSubclassOf<UDMMaterialStageBlend> InNewItem)
{
	if (LayerView->GetLayerItems().IsValidIndex(LayerView->GetSelectedLayerIndex()))
	{
		if (TSharedPtr<SDMSlotLayerItem> LayerWidget = LayerView->WidgetFromLayerItem(LayerView->GetLayerItems()[LayerView->GetSelectedLayerIndex()]))
		{
			if (TSharedPtr<SDMStage> BaseStageWidget = LayerWidget->GetBaseStageWidget())
			{
				if (UDMMaterialStage* BaseStage = BaseStageWidget->GetStage())
				{
					FScopedTransaction Transaction(LOCTEXT("SetStageBlendMode", "Material Designer Set Blend Mode"));
					BaseStage->Modify();
					BaseStage->ChangeSource<UDMMaterialStageBlend>(InNewItem);

					RefreshSlotSettingsRowWidget();
				}
			}
		}
	}
}

bool SDMSlot::IsOpacityPropertyEnabled() const
{
	return LayerView->GetLayerItems().IsValidIndex(LayerView->GetSelectedLayerIndex());
}

FReply SDMSlot::OnRemoveSlotClicked()
{
	if (CanRemoveSlot())
	{
		RemoveSlot();
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool SDMSlot::CanRemoveSlot() const
{
	if (UDMMaterialSlot* Slot = SlotWeak.Get())
	{
		// Cannot remove RGB slot.
		const TArray<TObjectPtr<UDMMaterialLayerObject>>& Layers = Slot->GetLayers();

		for (const TObjectPtr<UDMMaterialLayerObject>& Layer : Layers)
		{
			if (Layer->GetMaterialProperty() == EDMMaterialPropertyType::BaseColor
				|| Layer->GetMaterialProperty() == EDMMaterialPropertyType::EmissiveColor)
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void SDMSlot::RefreshMainWidget()
{
	bInvalidateMainWidget = false;
	bInvalidateHeaderWidget = false;
	bInvalidateSettingsWidget = false;
	bInvalidateComponentEditWidget = false;

	const float SplitterLocation = FMath::Clamp(UDynamicMaterialEditorSettings::Get()->SplitterLocation, 0.3f, 0.7f);
	const float LayerViewSplitterLocation = SplitterLocation;
	const float ExtraSpaceSplitterLocation = 1.0f - SplitterLocation;

	LayerView = SNew(SDMSlotLayerView, SharedThis(this))
		.OnLayerSelected(this, &SDMSlot::OnLayerSelected)
		.OnLayerStageSelected(this, &SDMSlot::OnLayerStageSelected)
		.PreviewSize(LayerPreviewSize);

	SplitterContainer =
		SNew(SSplitter)
		.Style(FAppStyle::Get(), "DetailsView.Splitter")
		.Orientation(Orient_Vertical)
		.ResizeMode(ESplitterResizeMode::Fill)
		.PhysicalSplitterHandleSize(3.0f)
		.HitDetectionSplitterHandleSize(4.0f)
		.OnSplitterFinishedResizing(this, &SDMSlot::OnSplitterResized)

		+ SSplitter::Slot()
		.Resizable(false)
		.SizeRule(SSplitter::ESizeRule::SizeToContent)
		[
			SAssignNew(SlotSettingsRowContainer, SBox)
			.Padding(3.0f, 4.0f, 3.0f, 4.0f)
			[
				CreateSlotSettingsRow()
			]
		]

	+ SSplitter::Slot()
		.Expose(LayerViewSplitterSlot)
		.Resizable(true)
		.SizeRule(SSplitter::ESizeRule::FractionOfParent)
		.MinSize(50)
		.Value(LayerViewSplitterLocation)
		[
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FDynamicMaterialEditorStyle::GetBrush("LayerView.Background"))
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				LayerView.ToSharedRef()
			]
		]

	+ SSplitter::Slot()
		.Resizable(false)
		.SizeRule(SSplitter::ESizeRule::SizeToContent)
		[
			CreateLayerButtonsRowWidget()
		]

	+ SSplitter::Slot()
		.Expose(ExtraSpaceSplitterSlot)
		.Resizable(true)
		.SizeRule(SSplitter::ESizeRule::FractionOfParent)
		.MinSize(50)
		.Value(ExtraSpaceSplitterLocation)
		[
			SAssignNew(ComponentEditContainer, SScrollBox)
			+ SScrollBox::Slot()
			.AutoSize()
			[
				CreateComponentEditWidget()
			]
		];

	TSharedRef<SVerticalBox> Child = SNew(SVerticalBox);

	if constexpr (UE::DynamicMaterialEditor::bMultipleSlotPropertiesEnabled)
	{
		Child->AddSlot()
			.AutoHeight()
			[
				SAssignNew(HeaderPropertyListWidget, SBox)
					.Padding(2.0f, 2.0f, 2.0f, 2.0f)
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					[
						CreateHeaderPropertyListWidget()
					]
			];
	}

	Child->AddSlot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SplitterContainer.ToSharedRef()
		];

	ChildSlot
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			Child
		];

	if (UDMMaterialSlot* Slot = SlotWeak.Get())
	{
		Slot->SetEditingLayers(true);

		const TArray<TObjectPtr<UDMMaterialLayerObject>>& SlotLayers = Slot->GetLayers();

		for (const TObjectPtr<UDMMaterialLayerObject>& Layer : SlotLayers)
		{
			if (UDMMaterialStage* Stage = Layer->GetFirstStageBeingEdited(EDMMaterialLayerStage::All))
			{
				LayerView->SelectLayerItem(Stage, true, ESelectInfo::OnMouseClick);
				break;
			}
		}
	}
}

TSharedRef<SWidget> SDMSlot::CreateHeaderPropertyListWidget()
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<SDMEditor> MaterialEditor = EditorWidgetWeak.Pin();
	if (!ensure(MaterialEditor.IsValid()))
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModel* MaterialModel = MaterialEditor->GetMaterialModel();
	if (!ensure(IsValid(MaterialModel)))
	{
		return SNullWidget::NullWidget;
	}

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
	if (!ensure(IsValid(ModelEditorOnlyData)))
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SWrapBox> PropertyContainer = SNew(SWrapBox)
		.InnerSlotPadding(FVector2D(2.f, 2.f))
		.Orientation(EOrientation::Orient_Horizontal)
		.UseAllottedSize(true)
		.UseAllottedWidth(true);

	const TArray<EDMMaterialPropertyType> Properties = ModelEditorOnlyData->GetMaterialPropertiesForSlot(Slot);

	for (EDMMaterialPropertyType Property : Properties)
	{
		PropertyContainer->AddSlot()
			.FillEmptySpace(false)
			[
				SNew(SDMMaterialProperty, SharedThis(this), Property)
			];
	}

	const TMap<TWeakObjectPtr<UDMMaterialSlot>, int32>& SlotsReferencedBy = Slot->GetSlotsReferencedBy();

	for (const TPair<TWeakObjectPtr<UDMMaterialSlot>, int32>& Pair : SlotsReferencedBy)
	{
		PropertyContainer->AddSlot()
			.FillEmptySpace(false)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				[
					SNew(SColorBlock)
					.Color(FStyleColors::AccentGreen.GetSpecifiedColor())
					.CornerRadius(FVector4(5.f, 5.f, 5.f, 5.f))
				]
				+ SOverlay::Slot()
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				[
					SNew(SCanvas)
					+ SCanvas::Slot()
					.Position(FVector2D(8.f, 8.f))
					.Size(FVector2D(4.f, 4.f))
					[
						SNew(SColorBlock)
						.Color(FLinearColor::White)
						.CornerRadius(FVector4(2.f, 2.f, 2.f, 2.f))
					]
				]
				+ SOverlay::Slot()
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				[
					SNew(STextBlock)
					.Text(Pair.Key->GetDescription())
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FLinearColor::White)
					.Margin(FMargin(16.f, 4.f, 4.f, 4.f))
				]
			];
	}

	return PropertyContainer;
}

TSharedRef<SWidget> SDMSlot::CreateLayerButtonsRowWidget()
{
	return 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(5.0f, 2.0f, 10.0f, 2.0f)
		[
			SNew(SButton)
			.ContentPadding(4.0f)
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
			.ToolTipText(LOCTEXT("RemoveSlotTooltip", "Remove Slot\n\nRGB Slots cannot be removed."))
			.IsEnabled(this, &SDMSlot::CanRemoveSlot)
			.OnClicked(this, &SDMSlot::OnRemoveSlotClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("GenericCommands.Delete"))
				.DesiredSizeOverride(FVector2D(16.0f))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 0.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "SlotLayerInfo")
			.Text(this, &SDMSlot::GetLayerButtonsDescription)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.IsFocusable(true)
			.ContentPadding(4.0f)
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
			.ToolTipText(LOCTEXT("AddLayerEffecTooltip", "Add Layer Effect"))
			.IsEnabled(this, &SDMSlot::GetLayerCanAddEffect)
			.OnGetMenuContent(this, &SDMSlot::GetLayerEffectsMenuContent)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FDynamicMaterialEditorStyle::GetBrush("EffectsView.Row.Fx"))
				.DesiredSizeOverride(FVector2D(16.0f))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(SComboButton)
			.HasDownArrow(false)
			.IsFocusable(true)
			.ContentPadding(4.0f)
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
			.ToolTipText(LOCTEXT("AddLayerTooltip", "Add New Layer"))
			.OnGetMenuContent(this, &SDMSlot::GetLayerButtonsMenuContent)
			.ButtonContent()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
				.ColorAndOpacity(FDynamicMaterialEditorStyle::Get().GetColor("Color.Stage.Enabled"))
				.DesiredSizeOverride(FVector2D(16.0f))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 2.0f, 0.0f, 2.0f)
		[
			SNew(SButton)
			.ContentPadding(4.0f)
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
			.ToolTipText(LOCTEXT("DuplicateLayer", "Duplicate Selected Layer"))
			.IsEnabled(this, &SDMSlot::GetLayerRowsButtonsCanDuplicate)
			.OnClicked(this, &SDMSlot::OnLayerRowButtonsDuplicateClicked)
			[
				SNew(SImage)
				.Image(FDynamicMaterialEditorStyle::GetBrush("LayerView.DuplicateIcon"))
				.DesiredSizeOverride(FVector2D(16.0f))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 2.0f, 0.0f, 2.0f)
		[
			SNew(SButton)
			.ContentPadding(4.0f)
			.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
			.ToolTipText(LOCTEXT("RemoveLayerTooltip", "Remove Selected Layer\n\nThe last layer cannot be removed."))
			.IsEnabled(this, &SDMSlot::GetLayerRowsButtonsCanRemove)
			.OnClicked(this, &SDMSlot::OnLayerRowButtonsRemoveClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Delete"))
				.DesiredSizeOverride(FVector2D(16.0f))
			]
		];
}

TSharedRef<SWidget> SDMSlot::CreateComponentEditWidget()
{
	if (UDMMaterialComponent* Component = EditedComponent.Get())
	{
		return
			SNew(SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(3.0f)
			.BorderImage(FDynamicMaterialEditorStyle::GetBrush("LayerView.Details.Background"))
			[
				SNew(SDMComponentEdit, Component, SharedThis(this))
			];
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDMSlot::CreateSlotSettingsRow()
{
	TSharedPtr<SDMPropertyEdit> OpacityPropertyEditWidget = nullptr;
	TSharedRef<SWidget> OpacityExtensionWidget = SNullWidget::NullWidget;

	if (const UDMMaterialLayerObject* SelectedLayer = GetSelectedLayer())
	{
		if (UDMMaterialStage* ValidStage = SelectedLayer->GetFirstValidStage(EDMMaterialLayerStage::All))
		{
			SelectedOpacityStageInputValue = UDMBlueprintFunctionLibrary::FindDefaultStageOpacityInputValue(ValidStage);

			if (SelectedOpacityStageInputValue.IsValid())
			{
				UDMMaterialValueFloat1* OpacityValue = Cast<UDMMaterialValueFloat1>(SelectedOpacityStageInputValue->GetValue());
				SelectedOpacityInputValue = OpacityValue;

				if (SelectedOpacityInputValue.IsValid())
				{
					UDMMaterialStageSource* StageSource = ValidStage->GetSource();

					if (IsValid(StageSource))
					{
						OpacityPropertyEditWidget = SNew(SDMPropertyEditOpacity, SharedThis(this), OpacityValue);
						OpacityPropertyEditWidget->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMSlot::IsOpacityPropertyEnabled));

						OpacityExtensionWidget = SDMComponentEdit::CreateExtensionButtons(
							SharedThis(this),
							OpacityValue,
							UDMMaterialValue::ValueName,
							true,
							FSimpleDelegate::CreateWeakLambda(OpacityValue, [OpacityValue]()
								{
									OpacityValue->ApplyDefaultValue();
								})
						);
					}
				}
			}
		}
	}

	TSharedPtr<SWidget> OpacityLabel;

	TSharedRef<SWidget> Row =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("MaterialDesignerInstanceBlendModeTooltip", "Change the Blend Mode for this Material Designer Instance."))
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialDesignerInstanceBlendMode", "Blend"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(22.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex(GetSourceBlendTypeSwitcherIndex())
					+ SWidgetSwitcher::Slot()
					[
						SNew(STextBlock)
						.Text(INVTEXT("-"))
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(STextBlock)
						.Text(INVTEXT("-"))
					]
					+ SWidgetSwitcher::Slot()
					[
						SNew(SDMSourceBlendType)
						.SelectedItem(this, &SDMSlot::GetSelectedSourceBlendType)
						.OnSelectedItemChanged(this, &SDMSlot::OnSourceBlendTypedSelected)
					]
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("StageOpacityTooltip", "Change the opacity for the selected stage."))
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SAssignNew(OpacityLabel, SBox)
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("StageOpacity", "Opacity"))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.HeightOverride(22.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					OpacityPropertyEditWidget.IsValid()
						? StaticCastSharedRef<SWidget>(OpacityPropertyEditWidget.ToSharedRef())
						: StaticCastSharedRef<SWidget>(SNew(STextBlock).Text(INVTEXT("-")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				OpacityExtensionWidget
			]
		];

	if (OpacityPropertyEditWidget.IsValid())
	{
		OpacityLabel->SetOnMouseButtonDown(FPointerEventHandler::CreateStatic(&SDMPropertyEdit::CreateRightClickDetailsMenu, OpacityPropertyEditWidget.ToWeakPtr()));
	}

	return Row;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMSlot::InvalidateMainWidget()
{
	bInvalidateMainWidget = true;
}

void SDMSlot::InvalidateHeaderPropertyListWidget()
{
	bInvalidateHeaderWidget = true;
}

void SDMSlot::InvalidateSlotSettingsRowWidget()
{
	bInvalidateSettingsWidget = true;
}

void SDMSlot::InvalidateComponentEditWidget()
{
	bInvalidateComponentEditWidget = true;
}

void SDMSlot::RefreshHeaderPropertyListWidget()
{
	bInvalidateHeaderWidget = false;

	if (HeaderPropertyListWidget.IsValid())
	{
		HeaderPropertyListWidget->SetContent(CreateHeaderPropertyListWidget());
	}
}

void SDMSlot::RefreshSlotSettingsRowWidget()
{
	bInvalidateSettingsWidget = false;

	if (SlotSettingsRowContainer.IsValid())
	{
		SlotSettingsRowContainer->SetContent(CreateSlotSettingsRow());
	}
}

void SDMSlot::RefreshComponentEditWidget()
{
	bInvalidateComponentEditWidget = false;

	if (ComponentEditContainer.IsValid())
	{
		ComponentEditContainer->ClearChildren();
		ComponentEditContainer->AddSlot()
			.AutoSize()
			[
				CreateComponentEditWidget()
			];
	}
}

void SDMSlot::OnSplitterResized() const
{
	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	if (LayerViewSplitterSlot)
	{
		Settings->SplitterLocation = LayerViewSplitterSlot->GetSizeValue();
		Settings->SaveConfig();
	}
}

void SDMSlot::StartStageOpacityTransaction()
{
	StartTransaction(LOCTEXT("StageOpacityTransDescription", "Material Designer Value Scrubbing (Stage Opacity)"));

	if (UDMMaterialValueFloat1* FloatValue = SelectedOpacityInputValue.Get())
	{
		FloatValue->Modify();
	}
}

void SDMSlot::SetStageOpacity(const float InNewValue)
{
	if (UDMMaterialValueFloat1* FloatValue = SelectedOpacityInputValue.Get())
	{
		FloatValue->SetValue(InNewValue);
	}
}

void SDMSlot::OnStageOpacityChanged(const float InNewValue)
{
	if (ScrubbingTransaction.IsValid())
	{
		SetStageOpacity(InNewValue);
	}
}

void SDMSlot::OnStageOpacityCommitted(const float InNewValue, ETextCommit::Type InCommitType)
{
	if (!ScrubbingTransaction.IsValid())
	{
		StartStageOpacityTransaction();
		SetStageOpacity(InNewValue);
		EndTransaction();
	}
}

void SDMSlot::StartTransaction(const FText InDescription)
{
	if (ScrubbingTransaction.IsValid())
	{
		return;
	}

	ScrubbingTransaction = MakeShared<FScopedTransaction>(InDescription);
}

void SDMSlot::EndTransaction()
{
	ScrubbingTransaction.Reset();
}

bool SDMSlot::CheckValidity()
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!Slot || !Slot->IsComponentValid())
	{
		return false;
	}

	bool bHasInvalidStage = false;

	for (const TSharedPtr<FDMMaterialLayerReference>& LayerItem : LayerView->GetLayerItems())
	{
		if (!LayerItem.IsValid())
		{
			bHasInvalidStage = true;
			break;
		}

		const UDMMaterialLayerObject* Layer = LayerItem->GetLayer();

		if (!Layer)
		{
			bHasInvalidStage = true;
			break;
		}

		if (!IsValid(Layer->GetStage(EDMMaterialLayerStage::Base)) || !Layer->GetStage(EDMMaterialLayerStage::Base)->IsComponentValid() 
			|| Layer->FindIndex() == INDEX_NONE)
		{
			bHasInvalidStage = true;
			break;
		}
	}

	if (bHasInvalidStage)
	{
		InvalidateSlotSettingsRowWidget();
		InvalidateComponentEditWidget();
	}

	return true;
}

void SDMSlot::OnSlotLayersUpdated(UDMMaterialSlot* InSlot)
{
	if (InSlot != SlotWeak.Get())
	{
		return;
	}

	InvalidateComponentEditWidget();
}

void SDMSlot::OnSlotPropertiesUpdated(UDMMaterialSlot* InSlot)
{
	if (InSlot != SlotWeak.Get())
	{
		return;
	}

	InvalidateHeaderPropertyListWidget();
}

void SDMSlot::OnComponentUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (InUpdateType == EDMUpdateType::Structure)
	{
		InvalidateComponentEditWidget();
	}
}

TSharedPtr<SDMStage> SDMSlot::FindStageWidget(UDMMaterialStage* const InStage) const
{
	if (!LayerView.IsValid())
	{
		return nullptr;
	}

	const TArray<TWeakPtr<SDMStage>>& StageWidgets = LayerView->GetSelectedStageWidgets();

	for (const TWeakPtr<SDMStage>& StageWidgetWeak : StageWidgets)
	{
		if (TSharedPtr<SDMStage> StageWidget = StageWidgetWeak.Pin())
		{
			if (StageWidget->GetStage() == InStage)
			{
				return StageWidget;
			}
		}
	}

	return nullptr;
}

void SDMSlot::RemoveLayerByStage(UDMMaterialStage* const InStage, const bool bInSelectNextMaskIfMask)
{
	if (!IsValid(InStage))
	{
		return;
	}

	UDMMaterialLayerObject* Layer = InStage->GetLayer();

	if (!Layer)
	{
		return;
	}

	RemoveLayerByIndex(Layer->FindIndex(), bInSelectNextMaskIfMask);
}

bool SDMSlot::CanRemoveLayerByIndex(const int32 InLayerIndex) const
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!Slot)
	{
		return false;
	}

	if (!Slot->GetLayers().IsValidIndex(InLayerIndex))
	{
		return false;
	}

	if (Slot->GetLayers().Num() > 1)
	{
		return true;
	}

	const UDMMaterialLayerObject* LayerToRemove = Slot->GetLayer(InLayerIndex);

	if (!LayerToRemove)
	{
		return false;
	}

	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
	{
		const TArray<EDMMaterialPropertyType> Properties = ModelEditorOnlyData->GetMaterialPropertiesForSlot(Slot);

		for (EDMMaterialPropertyType Property : Properties)
		{
			switch (Property)
			{
				case EDMMaterialPropertyType::BaseColor:
				case EDMMaterialPropertyType::EmissiveColor:
					return Slot->GetLayers().Num() > 1;

				default:
					// Nothing
					break;
			}
		}
	}

	return true;
}

void SDMSlot::RemoveLayerByIndex(const int32 InLayerIndex, const bool bInSelectNextMaskIfMask)
{
	if (!CanRemoveLayerByIndex(InLayerIndex))
	{
		return;
	}

	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!Slot)
	{
		return;
	}

	UDMMaterialLayerObject* LayerToRemove = Slot->GetLayer(InLayerIndex);

	if (!LayerToRemove)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("RemoveLayer", "Material Designer Remove Layer"));
	Slot->Modify();
	Slot->RemoveLayer(LayerToRemove);

	InvalidateMainWidget();
}

int32 SDMSlot::GetLayerCount() const
{
	if (UDMMaterialSlot* Slot = SlotWeak.Get())
	{
		return Slot->GetLayers().Num();
	}

	return 0;
}

TArray<int32> SDMSlot::GetSelectedLayerIndices() const
{
	TArray<int32> OutIndices;
	
	TArray<TSharedPtr<FDMMaterialLayerReference>> SelectedLayerItems = LayerView->GetSelectedItems();
	OutIndices.Reserve(SelectedLayerItems.Num());

	for (const TSharedPtr<FDMMaterialLayerReference>& SelectedLayerItem : SelectedLayerItems)
	{
		const int32 LayerIndex = LayerView->GetLayerItemIndex(SelectedLayerItem);
		OutIndices.Add(LayerIndex);
	}
	
	return OutIndices;
}

void SDMSlot::ClearSelection()
{
	if (LayerView.IsValid())
	{
		LayerView->ClearSelection();
	}
}

void SDMSlot::RemoveSlot()
{
	if (UDMMaterialSlot* Slot = SlotWeak.Get())
	{
		if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
		{
			FScopedTransaction Transaction(LOCTEXT("RemoveSlot", "Material Designer Remove Slot"));
			ModelEditorOnlyData->Modify();
			ModelEditorOnlyData->RemoveSlot(Slot->GetIndex());

			if (TSharedPtr<SDMEditor> Editor = EditorWidgetWeak.Pin())
			{
				Editor->SetActiveSlotIndex(0);
				Editor->RefreshSlotPickerList();
			}
		}
	}
}

void SDMSlot::AddPropertyToSlot(EDMMaterialPropertyType Property)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	if (!ensure(IsValid(ModelEditorOnlyData)))
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("AssignPropertyToSlot", "Assign Property To Slot"));
	ModelEditorOnlyData->Modify();
	ModelEditorOnlyData->AssignMaterialPropertyToSlot(Property, Slot);
}

UDMMaterialLayerObject* SDMSlot::AddNewLayer(UDMMaterialStage* InNewBaseStage, UDMMaterialStage* InNewMaskStage)
{
	if (!IsValid(InNewBaseStage))
	{
		return nullptr;
	}

	UDMMaterialSlot* Slot = SlotWeak.Get();
	
	if (!ensure(Slot))
	{
		return nullptr;
	}

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

	if (!ensure(IsValid(ModelEditorOnlyData)))
	{
		return nullptr;
	}

	{
		const FDMUpdateGuard Guard;

		FScopedTransaction Transaction(LOCTEXT("AddLayer", "Material Designer Add Layer"));
		Slot->Modify();

		EDMMaterialPropertyType MaterialProperty = EDMMaterialPropertyType::None;

		if (Slot->GetLayers().IsEmpty())
		{
			switch (ModelEditorOnlyData->GetShadingModel())
			{
				case EDMMaterialShadingModel::Unlit:
					MaterialProperty = EDMMaterialPropertyType::EmissiveColor;
					break;

				case EDMMaterialShadingModel::DefaultLit:
					MaterialProperty = EDMMaterialPropertyType::BaseColor;
					break;

				default:
					checkNoEntry();
					break;
			}
		}
		else
		{
			MaterialProperty = Slot->GetLayers().Last()->GetMaterialProperty();
		}

		if (!InNewMaskStage)
		{
			Slot->AddLayer(MaterialProperty, InNewBaseStage);
		}
		else
		{
			Slot->AddLayerWithMask(MaterialProperty, InNewBaseStage, InNewMaskStage);
		}
	}

	UDMMaterialStageSource* const Source = InNewBaseStage->GetSource();
	if (IsValid(Source))
	{
		Source->Update(EDMUpdateType::Structure);
	}

	UDMMaterialLayerObject* Layer = Slot->FindLayer(InNewBaseStage);
	if (!Layer)
	{
		return nullptr;
	}

	if (ensure(LayerView.IsValid()))
	{
		LayerView->AddLayerItem(MakeShared<FDMMaterialLayerReference>(Layer));
	}

	return Layer;
}

void SDMSlot::AddNewLayer_NewLocalValue(EDMValueType InType)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	UDMMaterialStage* NewBase = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	AddNewLayer(NewBase);

	UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
		NewBase, 
		UDMMaterialStageBlend::InputB,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		InType,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_GlobalValue(UDMMaterialValue* InValue)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();
	if (!ensure(ModelEditorOnlyData))
	{
		return;
	}

	if (!ensure(IsValid(InValue)) || !ensure(InValue->GetMaterialModel() == ModelEditorOnlyData->GetMaterialModel()))
	{
		return;
	}

	UDMMaterialStage* NewBase = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	AddNewLayer(NewBase);

	UDMMaterialStageInputValue::ChangeStageInput_Value(
		NewBase, 
		UDMMaterialStageBlend::InputB,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		InValue,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_NewGlobalValue(EDMValueType InType)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	AddNewLayer(NewStage);

	UDMMaterialStageInputValue::ChangeStageInput_NewValue(
		NewStage, 
		UDMMaterialStageBlend::InputB,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
		InType, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_Slot(UDMMaterialSlot* InSlot, EDMMaterialPropertyType InMaterialProperty)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	if (!ensure(IsValid(InSlot)) || !ensure(Slot != InSlot)
		|| !ensure(InSlot->GetMaterialModelEditorOnlyData() == Slot->GetMaterialModelEditorOnlyData()))
	{
		return;
	}

	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	AddNewLayer(NewStage);

	UDMMaterialStageInputSlot::ChangeStageInput_Slot(
		NewStage, 
		UDMMaterialStageBlend::InputB,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		InSlot, 
		InMaterialProperty, 
		0,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_Expression(TSubclassOf<UDMMaterialStageExpression> InExpressionClass, EDMMaterialLayerStage LayerEnabledMask)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	if (!ensure(IsValid(InExpressionClass)) || !ensure(InExpressionClass != UDMMaterialStageExpression::StaticClass()))
	{
		return;
	}

	// Extra transaction here because there are changes to the slot afterwards.
	FScopedTransaction Transaction(LOCTEXT("AddLayerExpression", "Material Designer Add Layer (Expression)"));
	Slot->Modify();

	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	UDMMaterialLayerObject* NewLayer = AddNewLayer(NewStage);

	UDMMaterialStageInputExpression::ChangeStageInput_Expression(
		NewStage, 
		InExpressionClass,
		UDMMaterialStageBlend::InputB, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
		0, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);

	switch (LayerEnabledMask)
	{
		case EDMMaterialLayerStage::Base:
			if (UDMMaterialStage* MaskStage = NewLayer->GetStage(EDMMaterialLayerStage::Mask))
			{
				MaskStage->SetEnabled(false);
			}
			break;

		case EDMMaterialLayerStage::Mask:
			if (UDMMaterialStage* BaseStage = NewLayer->GetStage(EDMMaterialLayerStage::Base))
			{
				BaseStage->SetEnabled(false);
			}
			break;

		default:
			// No nothing
			break;
	}
}

void SDMSlot::AddNewLayer_Blend(TSubclassOf<UDMMaterialStageBlend> InBlendClass)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	if (!ensure(IsValid(InBlendClass)) || !ensure(InBlendClass != UDMMaterialStageBlend::StaticClass()))
	{
		return;
	}

	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(InBlendClass);
	AddNewLayer(NewStage);

	UDMMaterialStageInputExpression::ChangeStageInput_Expression(
		NewStage, 
		UDMMaterialStageExpressionTextureSample::StaticClass(),
		UDMMaterialStageBlend::InputB, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		0, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_Gradient(TSubclassOf<UDMMaterialStageGradient> InGradientClass)
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	if (!ensure(IsValid(InGradientClass)) || !ensure(InGradientClass != UDMMaterialStageGradient::StaticClass()))
	{
		return;
	}

	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	AddNewLayer(NewStage);

	UDMMaterialStageInputGradient::ChangeStageInput_Gradient(
		NewStage, 
		InGradientClass, 
		UDMMaterialStageBlend::InputB, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_UV()
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* EditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

	if (!ensure(EditorOnlyData))
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = EditorOnlyData->GetMaterialModel();

	if (!ensure(MaterialModel))
	{
		return;
	}

	UDMMaterialStage* NewStage = UDMMaterialStageInputTextureUV::CreateStage(MaterialModel);
	AddNewLayer(NewStage);
}

void SDMSlot::AddNewLayer_MaterialFunction()
{
	UDMMaterialSlot* Slot = SlotWeak.Get();

	if (!ensure(Slot))
	{
		return;
	}

	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	AddNewLayer(NewStage);

	UDMMaterialStageInputFunction::ChangeStageInput_Function(
		NewStage, 
		UDMMaterialStageFunction::GetNoOpFunction(),
		UDMMaterialStageBlend::InputB, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
		0,
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
	);
}

void SDMSlot::AddNewLayer_SceneTexture()
{
	UDMMaterialStage* NewStage = UDMMaterialStageBlend::CreateStage(UDMMaterialStageBlendNormal::StaticClass());
	UDMMaterialLayerObject* NewLayer = AddNewLayer(NewStage);

	UDMMaterialStageInputExpression::ChangeStageInput_Expression(
		NewStage, 
		UDMMaterialStageExpressionSceneTexture::StaticClass(),
		UDMMaterialStageBlend::InputB, 
		FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
		0, 
		FDMMaterialStageConnectorChannel::THREE_CHANNELS
	);

	if (UDMMaterialStage* MaskStage = NewLayer->GetStage(EDMMaterialLayerStage::Mask))
	{
		UDMMaterialStageInputExpression::ChangeStageInput_Expression(
			MaskStage, 
			UDMMaterialStageExpressionSceneTexture::StaticClass(),
			UDMMaterialStageThroughputLayerBlend::InputMaskSource, 
			FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			0, 
			FDMMaterialStageConnectorChannel::FOURTH_CHANNEL
		);
	}
}

#undef LOCTEXT_NAMESPACE
