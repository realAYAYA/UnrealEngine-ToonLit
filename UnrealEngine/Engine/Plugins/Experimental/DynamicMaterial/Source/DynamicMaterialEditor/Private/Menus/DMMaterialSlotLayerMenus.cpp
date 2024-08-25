// Copyright Epic Games, Inc. All Rights Reserved.

#include "Menus/DMMaterialSlotLayerMenus.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageExpression.h"
#include "Components/DMMaterialStageGradient.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageExpressions/DMMSETextureSampleEdgeColor.h"
#include "DMDefs.h"
#include "DMMaterialSlotLayerAddEffectMenus.h"
#include "DMValueDefinition.h"
#include "DynamicMaterialEditorCommands.h"
#include "DynamicMaterialEditorModule.h"
#include "Framework/Commands/GenericCommands.h"
#include "Menus/DMMenuContext.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Slate/SDMSlot.h"
#include "Styling/SlateIconFinder.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "FDMMaterialSlotLayerMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static const FName SlotLayerMenuName = TEXT("MaterialDesigner.MaterialSlot.Layer");
	static const FName SlotLayerAddSectionName = TEXT("AddLayer");
	static const FName SlotLayerModifySectionName = TEXT("ModifyLayer");
	static const FName GlobalValuesSectionName = TEXT("GlobalValues");

	void AddLayerModifySection(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(SlotLayerModifySectionName))
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(SlotLayerModifySectionName, LOCTEXT("LayerActions", "LayerActions"));

		NewSection.AddMenuEntry(
			FDynamicMaterialEditorCommands::Get().InsertDefaultLayerAbove,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIconFinder::FindIcon("EditableComboBox.Add")
		);

		NewSection.AddMenuEntry(FGenericCommands::Get().Copy);
		NewSection.AddMenuEntry(FGenericCommands::Get().Cut);
		NewSection.AddMenuEntry(FGenericCommands::Get().Paste);
		NewSection.AddMenuEntry(FGenericCommands::Get().Duplicate);
		NewSection.AddMenuEntry(FGenericCommands::Get().Delete);
	}

	void AddLayerAddEffectsSection(UToolMenu* InMenu, UDMMaterialLayerObject* InLayerObject)
	{
		FDMMaterialSlotLayerAddEffectMenus::AddEffectSubMenu(InMenu, InLayerObject);
	}

	void AddGlobalValueSection(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(GlobalValuesSectionName))
		{
			return;
		}

		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		UDynamicMaterialModel* const MaterialModel = MenuContext->GetModel();

		if (!MaterialModel)
		{
			return;
		}

		const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();

		if (Values.IsEmpty())
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(GlobalValuesSectionName, LOCTEXT("GlobalValues", "Add Global Value"));

		NewSection.AddSubMenu(
			NAME_None,
			LOCTEXT("AddValueStage", "Global Value"),
			LOCTEXT("AddValueStageTooltip", "Add a Material Stage based on a Material Value defined above."),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
				{
					const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

					if (!MenuContext)
					{
						return;
					}

					UDynamicMaterialModel* const MaterialModel = MenuContext->GetModel();

					if (!MaterialModel)
					{
						return;
					}

					const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();

					for (UDMMaterialValue* Value : Values)
					{
						InMenu->AddMenuEntry(NAME_None,
							FToolMenuEntry::InitMenuEntry(NAME_None,
								Value->GetDescription(),
								LOCTEXT("AddValueStageSpecificTooltip", "Add a Material Stage based on this Material Value."),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateSP(MenuContext->GetSlotWidget().Pin().Get(), &SDMSlot::AddNewLayer_GlobalValue, Value)))
						);
					}
				})
		);

		NewSection.AddSubMenu(
			NAME_None,
			LOCTEXT("AddNewValueStage", "New Global Value"),
			LOCTEXT("AddNewValueStageTooltip", "Add a new global Material Value as use it as a Material Stage."),
			FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
				{
					const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

					if (!MenuContext)
					{
						return;
					}

					for (EDMValueType ValueType : UDMValueDefinitionLibrary::GetValueTypes())
					{
						FText Name = UDMValueDefinitionLibrary::GetValueDefinition(ValueType).GetDisplayName();
						FText FormattedTooltip = FText::Format(LOCTEXT("AddTypeTooltipTemplate", "Add a new {0} Value and use it as a Material Stage."), Name);

						InMenu->AddMenuEntry(NAME_None,
							FToolMenuEntry::InitMenuEntry(NAME_None,
								Name,
								FormattedTooltip,
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateSP(MenuContext->GetSlotWidget().Pin().Get(), &SDMSlot::AddNewLayer_NewGlobalValue, ValueType)))
						);
					}
				})
		);
	}

	void AddSlotMenuEntry(SDMSlot* SlotWidget, UToolMenu* InMenu, const FText& Name, UDMMaterialSlot* Slot, EDMMaterialPropertyType MaterialProperty)
	{
		InMenu->AddMenuEntry(NAME_None,
			FToolMenuEntry::InitMenuEntry(NAME_None,
				Name,
				LOCTEXT("AddValueStageSpecificTooltip", "Add a Material Stage based on this Material Value."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(SlotWidget, &SDMSlot::AddNewLayer_Slot, Slot, MaterialProperty))
			));
	}

	void AddLayerInputsMenu_Slot_Properties(UToolMenu* InMenu, UDMMaterialSlot* InSlot)
	{
		if (!IsValid(InMenu))
		{
			return;
		}

		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		const TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin();

		if (!SlotWidget.IsValid())
		{
			return;
		}

		UDynamicMaterialModel* const MaterialModel = MenuContext->GetModel();

		if (!MaterialModel)
		{
			return;
		}

		UDynamicMaterialModelEditorOnlyData* const ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel);

		if (!ensure(ModelEditorOnlyData) || !ensure(InSlot->GetMaterialModelEditorOnlyData() == ModelEditorOnlyData))
		{
			return;
		}

		const TArray<EDMMaterialPropertyType> SlotProperties = ModelEditorOnlyData->GetMaterialPropertiesForSlot(InSlot);

		for (EDMMaterialPropertyType SlotProperty : SlotProperties)
		{
			UDMMaterialProperty* MaterialProperty = ModelEditorOnlyData->GetMaterialProperty(SlotProperty);

			if (ensure(MaterialProperty))
			{
				UE::DynamicMaterialEditor::Private::AddSlotMenuEntry(
					SlotWidget.Get(),
					InMenu,
					MaterialProperty->GetDescription(),
					InSlot,
					SlotProperty
				);
			}
		}
	}

	void AddLayerInputsMenu_Slots(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(SlotLayerAddSectionName))
		{
			return;
		}

		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		const TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin();

		if (!SlotWidget.IsValid())
		{
			return;
		}

		const UDMMaterialSlot* const Slot = SlotWidget->GetSlot();

		if (!Slot)
		{
			return;
		}

		const UDynamicMaterialModelEditorOnlyData* const ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

		if (!ModelEditorOnlyData)
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
			if (Slot == SlotIter)
			{
				continue;
			}

			if (SlotIter->GetLayers().IsEmpty())
			{
				continue;
			}

			const TArray<EDMMaterialPropertyType> SlotProperties = ModelEditorOnlyData->GetMaterialPropertiesForSlot(SlotIter);

			if (SlotProperties.Num() == 1)
			{
				static const FText SlotNameFormatTemplate = LOCTEXT("SlotAndProperty", "{0} [{1}]");

				UDMMaterialProperty* MaterialProperty = ModelEditorOnlyData->GetMaterialProperty(SlotProperties[0]);

				if (ensure(MaterialProperty))
				{
					UE::DynamicMaterialEditor::Private::AddSlotMenuEntry(
						SlotWidget.Get(),
						InMenu,
						FText::Format(SlotNameFormatTemplate, SlotIter->GetDescription(), MaterialProperty->GetDescription()),
						SlotIter,
						SlotProperties[0]
					);
				}
			}
			else
			{
				InMenu->AddMenuEntry(NAME_None,
					FToolMenuEntry::InitSubMenu(NAME_None,
						LOCTEXT("AddSlotStage2", "Add Slot Output"),
						LOCTEXT("AddSlotStageTooltip2", "Add a Material Stage based on the output of another Material Slot."),
						FNewToolMenuDelegate::CreateStatic(&AddLayerInputsMenu_Slot_Properties, SlotIter))
				);
			}
		}
	}

	void AddLayerMenu_Gradients(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu))
		{
			return;
		}

		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		const TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin();

		if (!SlotWidget.IsValid())
		{
			return;
		}

		const TArray<TStrongObjectPtr<UClass>>& Gradients = UDMMaterialStageGradient::GetAvailableGradients();

		for (const TStrongObjectPtr<UClass>& Gradient : Gradients)
		{
			UDMMaterialStageGradient* GradientCDO = Cast<UDMMaterialStageGradient>(Gradient->GetDefaultObject());

			if (!ensure(GradientCDO))
			{
				continue;
			}

			const FText MenuName = GradientCDO->GetDescription();

			InMenu->AddMenuEntry(NAME_None,
				FToolMenuEntry::InitMenuEntry(NAME_None,
					MenuName,
					LOCTEXT("ChangeGradientSourceTooltip", "Change the source of this stage to a Material Gradient."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(
						SlotWidget.Get(),
						&SDMSlot::AddNewLayer_Gradient,
						TSubclassOf<UDMMaterialStageGradient>(Gradient.Get()))
					)
				)
			);
		}
	}
}

UToolMenu* FDMMaterialSlotLayerMenus::GenerateSlotLayerMenu(const TSharedPtr<SDMSlot>& InSlotWidget, UDMMaterialLayerObject* InLayerObject)
{
	using namespace UE::DynamicMaterialEditor::Private;

	UToolMenu* NewToolMenu = UDMMenuContext::GenerateContextMenuLayer(SlotLayerMenuName, InSlotWidget, InLayerObject);

	if (!NewToolMenu)
	{
		return nullptr;
	}

	AddAddLayerSection(NewToolMenu);

	if constexpr (UE::DynamicMaterialEditor::bGlobalValuesEnabled)
	{
		AddGlobalValueSection(NewToolMenu);
	}

	AddLayerAddEffectsSection(NewToolMenu, InLayerObject);

	AddLayerModifySection(NewToolMenu);

	return NewToolMenu;
}

void FDMMaterialSlotLayerMenus::AddAddLayerSection(UToolMenu* InMenu)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (!IsValid(InMenu) || InMenu->ContainsSection(SlotLayerAddSectionName))
	{
		return;
	}

	const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

	if (!MenuContext)
	{
		return;
	}

	const TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin();

	if (!SlotWidget.IsValid())
	{
		return;
	}

	const UDMMaterialSlot* const Slot = SlotWidget->GetSlot();

	if (!Slot)
	{
		return;
	}

	UDynamicMaterialModelEditorOnlyData* const ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

	if (!ModelEditorOnlyData)
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = ModelEditorOnlyData->GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	bool bHasValidSlot = false;

	if constexpr (UE::DynamicMaterialEditor::bAdvancedSlotsEnabled)
	{
		const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

		for (UDMMaterialSlot* SlotIter : Slots)
		{
			if (Slot == SlotIter)
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

			bHasValidSlot = true;
			break;
		}
	}

	FToolMenuSection& NewSection = InMenu->AddSection(SlotLayerAddSectionName, LOCTEXT("AddLayer", "Add Layer"));

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddTextureSample", "Texture"),
		LOCTEXT("AddTextureSampleTooltip", "Add a Material Stage based on a Texture."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(
			SlotWidget.Get(),
			&SDMSlot::AddNewLayer_Expression,
			TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
			EDMMaterialLayerStage::All
		))
	);

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddTextureSampleBaseOnly", "Texture (No alpha)"),
		LOCTEXT("AddTextureSampleBaseOnlyTooltip", "Add a Material Stage based on a Texture with the Alpha disabled."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(
			SlotWidget.Get(),
			&SDMSlot::AddNewLayer_Expression,
			TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
			EDMMaterialLayerStage::Base
		))
	);

	if (Slot->GetLayers().IsEmpty() == false)
	{
		NewSection.AddMenuEntry(
			NAME_None,
			LOCTEXT("AddAlphaOnly", "Alpha Only"),
			LOCTEXT("AddAlphaOnlyTooltip", "Add an Alpha-Only Material Layer.\n\nThe base layer will be disabled by default. It can still be re-enabled later."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(
				SlotWidget.Get(),
				&SDMSlot::AddNewLayer_Expression,
				TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
				EDMMaterialLayerStage::Mask
			))
		);
	}

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddColor", "Solid Color"),
		LOCTEXT("AddColorTooltip", "Add a new Material Layer with a solid RGB color."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(SlotWidget.Get(), &SDMSlot::AddNewLayer_NewLocalValue, EDMValueType::VT_Float3_RGB))
	);

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddColorAtlas", "Color Atlas"),
		LOCTEXT("AddColorAtlasTooltip", "Add a new Material Layer with a Color Atlas."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(SlotWidget.Get(), &SDMSlot::AddNewLayer_NewLocalValue, EDMValueType::VT_ColorAtlas))
	);

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddEdgeColor", "Texture Edge Color"),
		LOCTEXT("AddEdgeColorTooltip", "Add a new Material Layer with a solid color based on the edge color on a texture."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(
			SlotWidget.Get(),
			&SDMSlot::AddNewLayer_Expression,
			TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSampleEdgeColor::StaticClass()),
			EDMMaterialLayerStage::All
		))
	);

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddSceneTexture", "Scene Texture"),
		LOCTEXT("AddSceneTextureTooltip", "Add a new Material Layer that represents the Scene Texture for a post process material."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(
			SlotWidget.Get(),
			&SDMSlot::AddNewLayer_SceneTexture
		))
	);

	if constexpr (UE::DynamicMaterialEditor::bAdvancedSlotsEnabled)
	{
		if (bHasValidSlot)
		{
			NewSection.AddSubMenu(
				NAME_None,
				LOCTEXT("AddSlotStage", "Slot Output"),
				LOCTEXT("AddSlotStageTooltip", "Add a Material Stage based on the output of another Material Slot."),
				FNewToolMenuDelegate::CreateStatic(&AddLayerInputsMenu_Slots)
			);
		}
	}

	const TArray<TStrongObjectPtr<UClass>>& Gradients = UDMMaterialStageGradient::GetAvailableGradients();

	if (!Gradients.IsEmpty())
	{
		NewSection.AddSubMenu(
			NAME_None,
			LOCTEXT("AddGradientStage", "Gradient"),
			LOCTEXT("AddGradientStageTooltip", "Add a Material Stage based on a Material Gradient."),
			FNewToolMenuDelegate::CreateStatic(&AddLayerMenu_Gradients)
		);
	}

	NewSection.AddMenuEntry(
		NAME_None,
		LOCTEXT("AddMaterialFunction", "Material Function"),
		LOCTEXT("AddMaterialFunctionTooltip", "Add a new Material Layer based on a Material Function."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(SlotWidget.Get(), &SDMSlot::AddNewLayer_MaterialFunction))
	);

	if constexpr (UE::DynamicMaterialEditor::bGlobalValuesEnabled)
	{
		AddGlobalValueSection(InMenu);
	}
}

#undef LOCTEXT_NAMESPACE
