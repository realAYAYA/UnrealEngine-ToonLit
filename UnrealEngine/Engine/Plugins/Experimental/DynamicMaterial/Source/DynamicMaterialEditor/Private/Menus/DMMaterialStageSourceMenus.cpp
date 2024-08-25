// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMMaterialStageSourceMenus.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageExpression.h"
#include "Components/DMMaterialStageFunction.h"
#include "Components/DMMaterialStageGradient.h"
#include "Components/DMMaterialStageSource.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialStageThroughputLayerBlend.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageExpressions/DMMSESceneTexture.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageExpressions/DMMSETextureSampleEdgeColor.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIFunction.h"
#include "Components/MaterialStageInputs/DMMSIGradient.h"
#include "Components/MaterialStageInputs/DMMSISlot.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "DMDefs.h"
#include "DMValueDefinition.h"
#include "DynamicMaterialEditorModule.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Menus/DMMenuContext.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "ScopedTransaction.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "Templates/SharedPointer.h"
#include "ToolMenus.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "FDMMaterialStageSourceMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static const FName ChangeStageSourceMenuName = TEXT("MaterialDesigner.MaterialStage.ChangeSource");

	void GenerateChangeSourceMenu_NewLocalValues(UToolMenu* InMenu)
	{
		if (!ensure(IsValid(InMenu)))
		{
			return;
		}

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!ensure(MenuContext))
		{
			return;
		}

		UDMMaterialStage* Stage = MenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		for (EDMValueType ValueType : UDMValueDefinitionLibrary::GetValueTypes())
		{
			static const FText NameTooltipFormat = LOCTEXT("ChangeSourceNewValueSourceTooltipTemplate", "Add a new {0} Value and use it as the source of this stage.");

			const FText Name = UDMValueDefinitionLibrary::GetValueDefinition(ValueType).GetDisplayName();
			const FText FormattedTooltip = FText::Format(NameTooltipFormat, Name);

			InMenu->AddMenuEntry(NAME_None, FToolMenuEntry::InitMenuEntry(NAME_None,
				Name,
				FormattedTooltip,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateWeakLambda(
						MenuContext,
						[ValueType, MenuContext]()
						{
							UDMMaterialStage* const Stage = MenuContext->GetStage();
							if (!Stage)
							{
								return;
							}

							UDMMaterialStageSource* const StageSource = Stage->GetSource();
							if (!StageSource)
							{
								return;
							}

							if (StageSource->IsA<UDMMaterialStageBlend>())
							{
								const int32 OutputChannel = ValueType == EDMValueType::VT_ColorAtlas
									? FDMMaterialStageConnectorChannel::THREE_CHANNELS
									: FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

								FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
								Stage->Modify();

								UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
									Stage, 
									UDMMaterialStageBlend::InputB,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
									ValueType, 
									OutputChannel
								);
							}
							else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
							{
								const int32 OutputChannel = ValueType == EDMValueType::VT_ColorAtlas
									? FDMMaterialStageConnectorChannel::FOURTH_CHANNEL
									: FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

								FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
								Stage->Modify();

								UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
									Stage, 
									UDMMaterialStageThroughputLayerBlend::InputMaskSource,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
									ValueType,
									OutputChannel
								);
							}
							else
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
								Stage->Modify();
								UDMMaterialStageInputValue::ChangeStageSource_NewLocalValue(Stage, ValueType);
							}

							if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
							{
								SlotWidget->InvalidateComponentEditWidget();
							}
						})
				)
			));
		}
	}

	void GenerateChangeSourceMenu_GlobalValues(UToolMenu* InMenu)
	{
		if (!ensure(IsValid(InMenu)))
		{
			return;
		}

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();
		if (!ensure(MenuContext))
		{
			return;
		}

		UDynamicMaterialModel* MaterialModel = MenuContext->GetModel();
		if (!ensure(MaterialModel))
		{
			return;
		}

		const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();

		if (Values.IsEmpty())
		{
			return;
		}

		for (UDMMaterialValue* Value : Values)
		{
			if (!IsValid(Value))
			{
				continue;
			}

			InMenu->AddMenuEntry(NAME_None, FToolMenuEntry::InitMenuEntry(NAME_None,
				Value->GetDescription(),
				LOCTEXT("ChangeSourceValueSourceTooltip2", "Change the source of this stage to this Material Value."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateWeakLambda(
					MenuContext,
					[MenuContext, Value]()
					{
						UDMMaterialStage* const Stage = MenuContext->GetStage();
						if (!Stage)
						{
							return;
						}

						UDMMaterialStageSource* const StageSource = Stage->GetSource();
						if (!StageSource)
						{
							return;
						}

						if (StageSource->IsA<UDMMaterialStageBlend>())
						{
							const int32 OutputChannel = Value->GetType() == EDMValueType::VT_ColorAtlas
								? FDMMaterialStageConnectorChannel::THREE_CHANNELS
								: FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

							FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
							Stage->Modify();

							UDMMaterialStageInputValue::ChangeStageInput_Value(
								Stage, 
								UDMMaterialStageBlend::InputB,
								FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
								Value, 
								OutputChannel
							);
						}
						else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
						{
							const int32 OutputChannel = Value->GetType() == EDMValueType::VT_ColorAtlas
								? FDMMaterialStageConnectorChannel::FOURTH_CHANNEL
								: FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

							FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
							Stage->Modify();

							UDMMaterialStageInputValue::ChangeStageInput_Value(
								Stage, 
								UDMMaterialStageThroughputLayerBlend::InputMaskSource, 
								FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
								Value, 
								OutputChannel
							);
						}
						else
						{
							FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
							Stage->Modify();
							UDMMaterialStageInputValue::ChangeStageSource_Value(Stage, Value);
						}

						if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
						{
							SlotWidget->InvalidateComponentEditWidget();
						}
					})
				)
			));
		}
	}

	void GenerateChangeSourceMenu_NewGlobalValues(UToolMenu* InMenu)
	{
		if (!ensure(IsValid(InMenu)))
		{
			return;
		}

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();
		if (!ensure(MenuContext))
		{
			return;
		}

		UDMMaterialStage* Stage = MenuContext->GetStage();
		if (!Stage)
		{
			return;
		}

		for (EDMValueType ValueType : UDMValueDefinitionLibrary::GetValueTypes())
		{
			static const FText NameTooltipFormat = LOCTEXT("ChangeSourceNewValueSourceTooltipTemplate", "Add a new {0} Value and use it as the source of this stage.");

			const FText Name = UDMValueDefinitionLibrary::GetValueDefinition(ValueType).GetDisplayName();
			const FText FormattedTooltip = FText::Format(NameTooltipFormat, Name);

			InMenu->AddMenuEntry(NAME_None, FToolMenuEntry::InitMenuEntry(NAME_None,
				Name,
				FormattedTooltip,
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateWeakLambda(
						MenuContext,
						[ValueType, MenuContext]()
						{
							UDMMaterialStage* const Stage = MenuContext->GetStage();
							if (!Stage)
							{
								return;
							}

							UDMMaterialStageSource* const StageSource = Stage->GetSource();
							if (!StageSource)
							{
								return;
							}

							if (StageSource->IsA<UDMMaterialStageBlend>())
							{
								const int32 OutputChannel = ValueType == EDMValueType::VT_ColorAtlas
									? FDMMaterialStageConnectorChannel::THREE_CHANNELS
									: FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

								FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
								Stage->Modify();

								UDMMaterialStageInputValue::ChangeStageInput_NewValue(
									Stage, 
									UDMMaterialStageBlend::InputB, 
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
									ValueType, 
									OutputChannel
								);
							}
							else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
							{
								const int32 OutputChannel = ValueType == EDMValueType::VT_ColorAtlas
									? FDMMaterialStageConnectorChannel::FOURTH_CHANNEL
									: FDMMaterialStageConnectorChannel::WHOLE_CHANNEL;

								FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
								Stage->Modify();

								UDMMaterialStageInputValue::ChangeStageInput_NewValue(
									Stage, 
									UDMMaterialStageThroughputLayerBlend::InputMaskSource,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
									ValueType,
									 OutputChannel
									);
							}
							else
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
								Stage->Modify();
								UDMMaterialStageInputValue::ChangeStageSource_NewValue(Stage, ValueType);
							}

							if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
							{
								SlotWidget->InvalidateComponentEditWidget();
							}
						})
				)
			));
		}
	}

	void GenerateChangeSourceMenu_Slot_Properties(UToolMenu* InMenu, UDMMaterialSlot* InSlot)
	{
		if (!ensure(IsValid(InMenu)))
		{
			return;
		}

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();
		if (!ensure(MenuContext))
		{
			return;
		}

		UDMMaterialStage* Stage = MenuContext->GetStage();
		if (!Stage)
		{
			return;
		}

		UDMMaterialLayerObject* Layer = Stage->GetLayer();
		if (!Layer)
		{
			return;
		}

		if (!IsValid(InSlot) || Layer->GetSlot() != InSlot)
		{
			return;
		}

		UDynamicMaterialModel* MaterialModel = MenuContext->GetModel();
		if (!MaterialModel)
		{
			return;
		}

		UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
		if (!ModelEditorOnlyData)
		{
			return;
		}

		for (EDMMaterialPropertyType Property : ModelEditorOnlyData->GetMaterialPropertiesForSlot(InSlot))
		{
			UDMMaterialProperty* PropertyObj = ModelEditorOnlyData->GetMaterialProperty(Property);

			if (ensure(PropertyObj))
			{
				InMenu->AddMenuEntry(NAME_None, FToolMenuEntry::InitMenuEntry(NAME_None,
					PropertyObj->GetDescription(),
					LOCTEXT("ChangeSourceSlotSourceTooltip3", "Change the source of this stage to the output from this Material Slot's Property."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MenuContext,
						[MenuContext, Property]()
						{
							UDMMaterialStage* const Stage = MenuContext->GetStage();
							if (!Stage)
							{
								return;
							}

							UDMMaterialStageSource* const StageSource = Stage->GetSource();
							if (!StageSource)
							{
								return;
							}

							UDMMaterialLayerObject* Layer = Stage->GetLayer();
							if (!Layer)
							{
								return;
							}

							UDMMaterialSlot* Slot = Layer->GetSlot();
							if (!Slot)
							{
								return;
							}

							if (StageSource->IsA<UDMMaterialStageBlend>())
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
								Stage->Modify();

								UDMMaterialStageInputSlot::ChangeStageInput_Slot(
									Stage, 
									UDMMaterialStageBlend::InputB,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
									Slot, 
									Property,
									0, 
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
								);
							}
							else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
								Stage->Modify();

								UDMMaterialStageInputSlot::ChangeStageInput_Slot(
									Stage, 
									UDMMaterialStageThroughputLayerBlend::InputMaskSource,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
									Slot, 
									Property, 
									0,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
								);
							}
							else
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
								Stage->Modify();
								UDMMaterialStageInputSlot::ChangeStageSource_Slot(Stage, Slot, Property);
							}

							if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
							{
								SlotWidget->InvalidateComponentEditWidget();
							}
						})
					)
				));
			}
		}
	}

	void GenerateChangeSourceMenu_Slots(UToolMenu* InMenu)
	{
		if (!ensure(IsValid(InMenu)))
		{
			return;
		}

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();
		if (!ensure(MenuContext))
		{
			return;
		}

		UDMMaterialStage* Stage = MenuContext->GetStage();
		if (!Stage)
		{
			return;
		}

		UDMMaterialLayerObject* Layer = Stage->GetLayer();
		if (!Layer)
		{
			return;
		}

		UDMMaterialSlot* Slot = Layer->GetSlot();
		if (!Slot)
		{
			return;
		}

		UDynamicMaterialModel* MaterialModel = MenuContext->GetModel();
		if (!MaterialModel)
		{
			return;
		}

		UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);
		if (!MaterialModel)
		{
			return;
		}

		const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();
		if (Slots.Num() <= 1)
		{
			return;
		}

		for (UDMMaterialSlot* SlotIter : Slots)
		{
			if (SlotIter == Slot)
			{
				continue;
			}

			if (SlotIter->GetLayers().IsEmpty())
			{
				continue;
			}

			TArray<EDMMaterialPropertyType> SlotProperties = ModelEditorOnlyData->GetMaterialPropertiesForSlot(SlotIter);
			if (SlotProperties.IsEmpty())
			{
				continue;
			}

			if (SlotProperties.Num() == 1)
			{
				static const FText SlotNameFormatTemplate = LOCTEXT("SlotAndProperty", "{0} [{1}]");

				UDMMaterialProperty* PropertyObj = ModelEditorOnlyData->GetMaterialProperty(SlotProperties[0]);

				if (ensure(PropertyObj))
				{
					InMenu->AddMenuEntry(NAME_None, FToolMenuEntry::InitMenuEntry(NAME_None,
						FText::Format(SlotNameFormatTemplate, SlotIter->GetDescription(), PropertyObj->GetDescription()),
						LOCTEXT("ChangeSourceSlotSourceTooltip3", "Change the source of this stage to the output from this Material Slot's Property."),
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateWeakLambda(
							SlotIter,
							[SlotIter, SlotProperty = SlotProperties[0], MenuContext]()
							{
								if (!IsValid(MenuContext))
								{
									return;
								}

								UDMMaterialStage* const Stage = MenuContext->GetStage();
								if (!Stage)
								{
									return;
								}

								UDMMaterialStageSource* const StageSource = Stage->GetSource();
								if (!StageSource)
								{
									return;
								}

								if (StageSource->IsA<UDMMaterialStageBlend>())
								{
									FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
									Stage->Modify();

									UDMMaterialStageInputSlot::ChangeStageInput_Slot(
										Stage, 
										UDMMaterialStageBlend::InputB,
										FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
										SlotIter, 
										SlotProperty, 
										0, 
										FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
									);
								}
								else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
								{
									FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
									Stage->Modify();

									UDMMaterialStageInputSlot::ChangeStageInput_Slot(
										Stage, 
										UDMMaterialStageThroughputLayerBlend::InputMaskSource,
										FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
										SlotIter, 
										SlotProperty, 
										0,
										FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
									);
								}
								else
								{
									FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
									Stage->Modify();
									UDMMaterialStageInputSlot::ChangeStageSource_Slot(Stage, SlotIter, SlotProperty);
								}

								if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
								{
									SlotWidget->InvalidateComponentEditWidget();
								}
							})
						)
					));
				}
			}
			else
			{
				FToolMenuSection& NewSection = InMenu->AddSection("ChangeSourceSlotTooltip", LOCTEXT("ChangeSourceSlot", "Change Source Slot"));
				NewSection.AddDynamicEntry(NAME_None,
					FNewToolMenuSectionDelegate::CreateWeakLambda(
						SlotIter,
						[SlotIter](FToolMenuSection& InSection)
						{
							InSection.AddSubMenu(NAME_None,
							SlotIter->GetDescription(),
							LOCTEXT("ChangeSourceSlotTooltip", "Change the source of this stage to the output from another Material Slot."),
							FNewToolMenuDelegate::CreateLambda([SlotIter](UToolMenu* InMenu) { GenerateChangeSourceMenu_Slot_Properties(InMenu, SlotIter); })
							);
						}));
			}
		}
	}

	void GenerateChangeSourceMenu_Gradients(UToolMenu* const InMenu)
	{
		if (!ensure(IsValid(InMenu)))
		{
			return;
		}

		UDMMenuContext* MenuContext = InMenu->FindContext<UDMMenuContext>();
		if (!ensure(MenuContext))
		{
			return;
		}

		const TArray<TStrongObjectPtr<UClass>>& Gradients = UDMMaterialStageGradient::GetAvailableGradients();
		if (Gradients.IsEmpty())
		{
			return;
		}

		for (TStrongObjectPtr<UClass> GradientClass : Gradients)
		{
			UDMMaterialStageGradient* GradientCDO = Cast<UDMMaterialStageGradient>(GradientClass->GetDefaultObject());

			if (ensure(GradientCDO))
			{
				const FText MenuName = GradientCDO->GetDescription();

				InMenu->AddMenuEntry(NAME_None, FToolMenuEntry::InitMenuEntry(NAME_None,
					MenuName,
					LOCTEXT("ChangeSourceGradientTooltip", "Change the source of this stage to a Material Gradient."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						MenuContext,
						[MenuContext, GradientClass]()
						{
							UDMMaterialStage* const Stage = MenuContext->GetStage();
							if (!Stage)
							{
								return;
							}

							UDMMaterialStageSource* const StageSource = Stage->GetSource();
							if (!StageSource)
							{
								return;
							}

							if (StageSource->IsA<UDMMaterialStageBlend>())
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
								Stage->Modify();

								UDMMaterialStageInputGradient::ChangeStageInput_Gradient(
									Stage,
									GradientClass.Get(), UDMMaterialStageBlend::InputB,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
								);
							}
							else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
								Stage->Modify();
								UDMMaterialStageInputGradient::ChangeStageInput_Gradient(
									Stage,
									GradientClass.Get(), 
									UDMMaterialStageThroughputLayerBlend::InputMaskSource,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
									FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
								);
							}
							else
							{
								FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
								Stage->Modify();
								UDMMaterialStageInputGradient::ChangeStageSource_Gradient(Stage, GradientClass.Get());
							}

							if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
							{
								SlotWidget->InvalidateComponentEditWidget();
							}
						})
					)
				));
			}
		}
	}

	void ChangeSourceToTextureSampleFromContext(UDMMenuContext* InMenuContext)
	{
		if (!IsValid(InMenuContext))
		{
			return;
		}

		UDMMaterialStage* const Stage = InMenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		UDMMaterialStageSource* const StageSource = Stage->GetSource();

		if (!StageSource)
		{
			return;
		}

		if (StageSource->IsA<UDMMaterialStageBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage,
				UDMMaterialStageExpressionTextureSample::StaticClass(), 
				UDMMaterialStageBlend::InputB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::THREE_CHANNELS
			);
		}
		else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage, 
				UDMMaterialStageExpressionTextureSample::StaticClass(), 
				UDMMaterialStageThroughputLayerBlend::InputMaskSource,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageSource_Expression(
				Stage, 
				UDMMaterialStageExpressionTextureSample::StaticClass()
			);
		}

		if (TSharedPtr<SDMSlot> SlotWidget = InMenuContext->GetSlotWidget().Pin())
		{
			SlotWidget->InvalidateComponentEditWidget();
		}
	}

	void ChangeSourceToSolidColorRGBFromContext(UDMMenuContext* InMenuContext)
	{
		if (!IsValid(InMenuContext))
		{
			return;
		}

		UDMMaterialStage* const Stage = InMenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		UDMMaterialStageSource* const StageSource = Stage->GetSource();

		if (!StageSource)
		{
			return;
		}

		if (StageSource->IsA<UDMMaterialStageBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
			Stage->Modify();

			UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				Stage, 
				UDMMaterialStageBlend::InputB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				EDMValueType::VT_Float3_RGB,
				FDMMaterialStageConnectorChannel::THREE_CHANNELS
			);
		}
		else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
			Stage->Modify();

			UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				Stage, 
				UDMMaterialStageThroughputLayerBlend::InputMaskSource,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				EDMValueType::VT_Float3_RGB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
			Stage->Modify();
			UDMMaterialStageInputValue::ChangeStageSource_NewLocalValue(Stage, EDMValueType::VT_Float3_RGB);
		}

		if (TSharedPtr<SDMSlot> SlotWidget = InMenuContext->GetSlotWidget().Pin())
		{
			SlotWidget->InvalidateComponentEditWidget();
		}
	}

	void ChangeSourceToColorAtlasFromContext(UDMMenuContext* InMenuContext)
	{
		if (!IsValid(InMenuContext))
		{
			return;
		}

		UDMMaterialStage* const Stage = InMenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		UDMMaterialStageSource* const StageSource = Stage->GetSource();

		if (!StageSource)
		{
			return;
		}

		if (StageSource->IsA<UDMMaterialStageBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
			Stage->Modify();

			UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				Stage, 
				UDMMaterialStageBlend::InputB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, EDMValueType::VT_ColorAtlas,
				FDMMaterialStageConnectorChannel::THREE_CHANNELS
			);

		}
		else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
			Stage->Modify();

			UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(
				Stage, 
				UDMMaterialStageThroughputLayerBlend::InputMaskSource,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				EDMValueType::VT_ColorAtlas,
				FDMMaterialStageConnectorChannel::FOURTH_CHANNEL
			);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
			Stage->Modify();
			UDMMaterialStageInputValue::ChangeStageSource_NewLocalValue(Stage, EDMValueType::VT_ColorAtlas);
		}

		if (TSharedPtr<SDMSlot> SlotWidget = InMenuContext->GetSlotWidget().Pin())
		{
			SlotWidget->InvalidateComponentEditWidget();
		}
	}

	void ChangeSourceToTextureSampleEdgeColorFromContext(UDMMenuContext* InMenuContext)
	{
		if (!IsValid(InMenuContext))
		{
			return;
		}

		UDMMaterialStageSource* const StageSource = InMenuContext->GetStageSource();

		if (!StageSource)
		{
			return;
		}

		UDMMaterialStage* const Stage = InMenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		UDMMaterialLayerObject* const Layer = Stage->GetLayer();

		if (!Layer)
		{
			return;
		}

		if (StageSource->IsA<UDMMaterialStageBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage,
				UDMMaterialStageExpressionTextureSampleEdgeColor::StaticClass(), 
				UDMMaterialStageBlend::InputB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::THREE_CHANNELS
			);
		}
		else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage, 
				UDMMaterialStageExpressionTextureSampleEdgeColor::StaticClass(),
				UDMMaterialStageThroughputLayerBlend::InputMaskSource,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInput", "Set Material Designer Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageSource_Expression(
				Stage,
				UDMMaterialStageExpressionTextureSampleEdgeColor::StaticClass()
			);
		}

		if (UDMMaterialStage* MaskStage = Layer->GetStage(EDMMaterialLayerStage::Mask))
		{
			MaskStage->SetEnabled(false);
		}

		if (TSharedPtr<SDMSlot> SlotWidget = InMenuContext->GetSlotWidget().Pin())
		{
			SlotWidget->InvalidateComponentEditWidget();
		}
	}

	void ChangeSourceToSceneTextureFromContext(UDMMenuContext* InMenuContext)
	{
		if (!IsValid(InMenuContext))
		{
			return;
		}

		UDMMaterialStageSource* const StageSource = InMenuContext->GetStageSource();

		if (!StageSource)
		{
			return;
		}

		UDMMaterialStage* const Stage = InMenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		UDMMaterialLayerObject* const Layer = Stage->GetLayer();

		if (!Layer)
		{
			return;
		}

		if (StageSource->IsA<UDMMaterialStageBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage, 
				UDMMaterialStageExpressionSceneTexture::StaticClass(), 
				UDMMaterialStageBlend::InputB,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::THREE_CHANNELS
			);

			if (Layer->GetStageType(Stage) == EDMMaterialLayerStage::Base)
			{
				if (UDMMaterialStage* MaskStage = Layer->GetStage(EDMMaterialLayerStage::Mask, true))
				{
					MaskStage->Modify();

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
		}
		else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageInput_Expression(
				Stage, 
				UDMMaterialStageExpressionSceneTexture::StaticClass(), 
				UDMMaterialStageThroughputLayerBlend::InputMaskSource,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::FOURTH_CHANNEL
			);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInput", "Set Material Designer Source"));
			Stage->Modify();

			UDMMaterialStageInputExpression::ChangeStageSource_Expression(
				Stage,
				UDMMaterialStageExpressionSceneTexture::StaticClass()
			);
		}

		if (TSharedPtr<SDMSlot> SlotWidget = InMenuContext->GetSlotWidget().Pin())
		{
			SlotWidget->InvalidateComponentEditWidget();
		}
	}

	bool CanChangeSourceToSceneTextureFromContext(UDMMenuContext* InMenuContext)
	{
		if (IsValid(InMenuContext))
		{
			if (UDMMaterialStageSource* const StageSource = InMenuContext->GetStageSource())
			{
				if (UDMMaterialStage* const Stage = InMenuContext->GetStage())
				{
					if (UDMMaterialLayerObject* const Layer = Stage->GetLayer())
					{
						if (UDMMaterialSlot* Slot = Layer->GetSlot())
						{
							if (UDynamicMaterialModelEditorOnlyData* EditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
							{
								return EditorOnlyData->GetDomain() == EMaterialDomain::MD_PostProcess;
							}
						}
					}
				}
			}
		}

		return false;
	}

	void ChangeSourceToMaterialFunctionFromContext(UDMMenuContext* InMenuContext)
	{
		if (!IsValid(InMenuContext))
		{
			return;
		}

		UDMMaterialStage* const Stage = InMenuContext->GetStage();

		if (!Stage)
		{
			return;
		}

		UDMMaterialStageSource* const StageSource = Stage->GetSource();

		if (!StageSource)
		{
			return;
		}

		if (StageSource->IsA<UDMMaterialStageBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputBase", "Set Material Designer Base Source"));
			Stage->Modify();

			UDMMaterialStageInputFunction::ChangeStageInput_Function(
				Stage, 
				UDMMaterialStageFunction::GetNoOpFunction(),
				UDMMaterialStageBlend::InputB, 	
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
				0, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);
		}
		else if (StageSource->IsA<UDMMaterialStageThroughputLayerBlend>())
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageInputMask", "Set Material Designer Mask Source"));
			Stage->Modify();

			UDMMaterialStageInputFunction::ChangeStageInput_Function(
				Stage, 
				UDMMaterialStageFunction::GetNoOpFunction(),
				UDMMaterialStageThroughputLayerBlend::InputMaskSource, 
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 
				0,
				FDMMaterialStageConnectorChannel::WHOLE_CHANNEL
			);
		}
		else
		{
			FScopedTransaction Transaction(LOCTEXT("SetStageSource", "Set Material Designer Stage Source"));
			Stage->Modify();
			UDMMaterialStageInputFunction::ChangeStageSource_Function(Stage, UDMMaterialStageFunction::GetNoOpFunction());
		}

		if (TSharedPtr<SDMSlot> SlotWidget = InMenuContext->GetSlotWidget().Pin())
		{
			SlotWidget->InvalidateComponentEditWidget();
		}
	}

	void CreateChangeMaterialStageSource(FToolMenuSection& InSection)
	{
		UDMMenuContext* MenuContext = InSection.FindContext<UDMMenuContext>();

		if (!ensure(MenuContext))
		{
			return;
		}

		UDMMaterialSlot* Slot = MenuContext->GetSlot();

		UDynamicMaterialModel* MaterialModel = MenuContext->GetModel();

		if (!ensure(MaterialModel))
		{
			return;
		}

		UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

		if (!ensure(ModelEditorOnlyData))
		{
			return;
		}

		const TArray<TStrongObjectPtr<UClass>>& Gradients = UDMMaterialStageGradient::GetAvailableGradients();

		bool bHasValidSlot = false;

		if constexpr (bAdvancedSlotsEnabled)
		{
			const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

			for (UDMMaterialSlot* SlotIter : Slots)
			{
				if (SlotIter == Slot)
				{
					continue;
				}

				if (SlotIter->GetLayers().IsEmpty())
				{
					continue;
				}

				bHasValidSlot = true;
				break;
			}
		}

		InSection.AddMenuEntry("TextureSample",
			LOCTEXT("TextureSample", "Texture"),
			LOCTEXT("ChangeSourceTextureSampleTooltip", "Change the source of this stage to a texture."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(
					&ChangeSourceToTextureSampleFromContext,
					MenuContext
				)
			)
		);

		InSection.AddMenuEntry("SolidColor",
			LOCTEXT("ChangeSourceColorRGB", "Solid Color"),
			LOCTEXT("ChangeSourceColorRGBTooltip", "Change the source of this stage to a Solid Color."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(
					&ChangeSourceToSolidColorRGBFromContext,
					MenuContext
				)
			)
		);

		InSection.AddMenuEntry("ColorAtlas",
			LOCTEXT("ChangeSourceColorAtlas", "Color Atlas"),
			LOCTEXT("ChangeSourceColorAtlasTooltip", "Change the source of this stage to a Color Atlas."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(
					&ChangeSourceToColorAtlasFromContext,
					MenuContext
				)
			)
		);

		InSection.AddMenuEntry("TextureSample_EdgeColor",
			LOCTEXT("AddTextureSampleEgdeColor", "Texture Edge Color"),
			LOCTEXT("ChangeSourceTextureSampleEdgeColorTooltip", "Change the source of this stage to the edge color of a texture."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(
					&ChangeSourceToTextureSampleEdgeColorFromContext,
					MenuContext
				)
			)
		);

		InSection.AddMenuEntry("SceneTexture",
			LOCTEXT("AddSceneTexture", "Scene Texture"),
			LOCTEXT("ChangeSourceSceneTextureTooltip", "Change the source of this stage to Scene Texture in post process materials."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(
					&ChangeSourceToSolidColorRGBFromContext,
					MenuContext
				),
				FCanExecuteAction::CreateStatic(
					&CanChangeSourceToSceneTextureFromContext,
					MenuContext
				)
			)
		);

		if constexpr (UE::DynamicMaterialEditor::bGlobalValuesEnabled)
		{
			const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();

			InSection.AddSubMenu("NewLocalValue",
				LOCTEXT("ChangeSourceNewLocalValue", "New Local Value"),
				LOCTEXT("ChangeSourceNewLocalValueTooltip", "Add a new local Material Value and use it as the source of this stage."),
				FNewToolMenuDelegate::CreateStatic(&GenerateChangeSourceMenu_NewLocalValues)
			);

			if (!Values.IsEmpty())
			{
				InSection.AddSubMenu("GlobalValue",
					LOCTEXT("ChangeSourceValue", "Global Value"),
					LOCTEXT("ChangeSourceValueTooltip", "Change the source of this stage to a global Material Value."),
					FNewToolMenuDelegate::CreateStatic(&GenerateChangeSourceMenu_GlobalValues)
				);
			}

			InSection.AddSubMenu("NewLocalValue",
				LOCTEXT("ChangeSourceNewValue", "New Global Value"),
				LOCTEXT("ChangeSourceNewValueTooltip", "Add a new global Material Value and use it as the source of this stage."),
				FNewToolMenuDelegate::CreateStatic(&GenerateChangeSourceMenu_NewGlobalValues)
			);
		}

		if (!Gradients.IsEmpty())
		{
			InSection.AddSubMenu("Gradient",
				LOCTEXT("ChangeSourceGradient", "Gradient"),
				LOCTEXT("ChangeSourceGradientTooltip", "Change the source of this stage to a Material Gradient."),
				FNewToolMenuDelegate::CreateStatic(&GenerateChangeSourceMenu_Gradients)
			);
		}

		InSection.AddMenuEntry("MaterialFunction",
			LOCTEXT("ChangeSourceMaterialFunction", "Material Function"),
			LOCTEXT("ChangeSourceMaterialFunctionTooltip", "Change the source of this stage to a Material Function."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateStatic(
					&ChangeSourceToMaterialFunctionFromContext,
					MenuContext
				)
			)
		);

		if constexpr (UE::DynamicMaterialEditor::bAdvancedSlotsEnabled)
		{
			if (bHasValidSlot)
			{
				InSection.AddDynamicEntry(
					NAME_None,
					FNewToolMenuSectionDelegate::CreateLambda(
						[](FToolMenuSection& InSection)
						{
							InSection.AddSubMenu("SlotOutput",
							LOCTEXT("ChangeSourceSlotOuptut", "Slot Output"),
							LOCTEXT("ChangeSourceSlotOutputTooltip", "Change the source of this stage to the output from another Material Slot."),
							FNewToolMenuDelegate::CreateStatic(&GenerateChangeSourceMenu_Slots)
							);
						}));
			}
		}
	}
}

TSharedRef<SWidget> FDMMaterialStageSourceMenus::MakeChangeSourceMenu(const TSharedPtr<SDMSlot>& InSlotWidget, const TSharedPtr<SDMStage>& InStageWidget)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (!UToolMenus::Get()->IsMenuRegistered(ChangeStageSourceMenuName))
	{
		UToolMenu* const NewToolMenu = UDMMenuContext::GenerateContextMenuDefault(ChangeStageSourceMenuName);
		if (!NewToolMenu)
		{
			return SNullWidget::NullWidget;
		}

		FToolMenuSection& NewSection = NewToolMenu->AddSection("ChangeMaterialStage", LOCTEXT("ChangeStageSource", "Change Stage Source"));
		NewSection.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateStatic(&CreateChangeMaterialStageSource));
	}

	FToolMenuContext MenuContext(UDMMenuContext::CreateStage(InSlotWidget, InStageWidget));

	return UToolMenus::Get()->GenerateWidget(ChangeStageSourceMenuName, MenuContext);
}

void FDMMaterialStageSourceMenus::CreateSourceMenuTree(TFunction<void(EDMExpressionMenu Menu, TArray<UDMMaterialStageExpression*>& SubmenuExpressionList)> Callback, const TArray<TStrongObjectPtr<UClass>>& AllExpressions)
{
	TMap<EDMExpressionMenu, TArray<UDMMaterialStageExpression*>> MenuMap;

	for (const TStrongObjectPtr<UClass>& Class : AllExpressions)
	{
		TSubclassOf<UDMMaterialStageExpression> ExpressionClass = Class.Get();
		if (!ExpressionClass.Get())
		{
			continue;
		}

		UDMMaterialStageExpression* ExpressionCDO = Cast<UDMMaterialStageExpression>(ExpressionClass->GetDefaultObject(true));
		if (!ExpressionCDO)
		{
			continue;
		}

		const TArray<EDMExpressionMenu>& Menus = ExpressionCDO->GetMenus();
		for (EDMExpressionMenu Menu : Menus)
		{
			MenuMap.FindOrAdd(Menu).Add(ExpressionCDO);
		}
	}

	auto CreateMenu = [&Callback, &MenuMap](EDMExpressionMenu Menu)
	{
		TArray<UDMMaterialStageExpression*>* ExpressionListPtr = MenuMap.Find(Menu);
		if (!ExpressionListPtr || ExpressionListPtr->Num() == 0)
		{
			return;
		}

		Callback(Menu, *ExpressionListPtr);
	};

	CreateMenu(EDMExpressionMenu::Texture);
	CreateMenu(EDMExpressionMenu::Math);
	CreateMenu(EDMExpressionMenu::Geometry);
	CreateMenu(EDMExpressionMenu::Object);
	CreateMenu(EDMExpressionMenu::WorldSpace);
	CreateMenu(EDMExpressionMenu::Time);
	CreateMenu(EDMExpressionMenu::Camera);
	CreateMenu(EDMExpressionMenu::Particle);
	CreateMenu(EDMExpressionMenu::Decal);
	CreateMenu(EDMExpressionMenu::Landscape);
	CreateMenu(EDMExpressionMenu::Other);
}

#undef LOCTEXT_NAMESPACE
