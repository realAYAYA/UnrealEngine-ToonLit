// Copyright Epic Games, Inc. All Rights Reserved.

#include "Menus/DMMaterialStageMenus.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialValue.h"
#include "Menus/DMMenuContext.h"
#include "ScopedTransaction.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "ToolMenu.h"

#define LOCTEXT_NAMESPACE "FDMMaterialStageMenus"

FName FDMMaterialStageMenus::GetStageMenuName()
{
	return "MaterialDesigner.MaterialStage";
}

FName FDMMaterialStageMenus::GetStageToggleSectionName()
{
	return "StageToggle";
}

UToolMenu* FDMMaterialStageMenus::GenerateStageMenu(const TSharedPtr<SDMSlot>& InSlotWidget, const TSharedPtr<SDMStage>& InStageWidget)
{
	UToolMenu* NewToolMenu = UDMMenuContext::GenerateContextMenuStage(GetStageMenuName(), InSlotWidget, InStageWidget);
	if (!NewToolMenu)
	{
		return nullptr;
	}

	AddStageSection(NewToolMenu);

	return NewToolMenu;
}

void FDMMaterialStageMenus::AddStageSection(UToolMenu* InMenu)
{
	if (!IsValid(InMenu) || InMenu->ContainsSection(GetStageToggleSectionName()))
	{
		return;
	}

	const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();
	if (!MenuContext)
	{
		return;
	}

	UDMMaterialStage* Stage = MenuContext->GetStage();
	if (!Stage)
	{
		return;
	}

	const UDMMaterialLayerObject* Layer = MenuContext->GetLayer();
	if (!Layer)
	{
		return;
	}

	const int32 LayerIndex = Layer->FindIndex();

	const bool bAllowRemoveLayer = LayerIndex >= 1;
	const EDMMaterialLayerStage StageType = Layer->GetStageType(Stage);

	if (bAllowRemoveLayer || StageType == EDMMaterialLayerStage::Mask)
	{
		FToolMenuSection& NewSection = InMenu->AddSection(GetStageToggleSectionName(), LOCTEXT("MaterialStageMenu", "Material Stage"));

		if (bAllowRemoveLayer)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ToggleLayer", "Toggle"),
				LOCTEXT("ToggleLayerTooltip", "Toggle the entire layer on and off.\n\n"
					"Warning: Toggling a layer off may result in inputs being reset where incompatibilities are found.\n\nAlt+Left Click"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateWeakLambda(
					Layer,
					[Layer]()
					{
						FScopedTransaction Transaction(LOCTEXT("ToggleAllStageEnabled", "Material Designer Toggle All Stage Enabled"));

						for (UDMMaterialStage* Stage : Layer->GetStages(EDMMaterialLayerStage::All))
						{
							Stage->Modify();
							Stage->SetEnabled(!Stage->IsEnabled());
						}
					}
				))
			);

			if (StageType == EDMMaterialLayerStage::Base)
			{
				NewSection.AddMenuEntry(NAME_None,
					LOCTEXT("ToggleLayerBase", "Toggle Base"),
					LOCTEXT("ToggleLayerBaseTooltip", "Toggle the layer base on and off.\n\n"
						"Warning: Toggling a layer base off may result in inputs being reset where incompatibilities are found.\n\nAlt+Shift+Left Click"),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateWeakLambda(
						Layer,
						[Layer]()
						{
							FScopedTransaction Transaction(LOCTEXT("ToggleBaseStageEnabled", "Material Designer Toggle Base Stage Enabled"));

							if (UDMMaterialStage* Stage = Layer->GetStage(EDMMaterialLayerStage::Base))
							{
								Stage->Modify();
								Stage->SetEnabled(!Stage->IsEnabled());
							}
						}
					))
				);
			}
		}

		if (StageType == EDMMaterialLayerStage::Mask)
		{
			NewSection.AddMenuEntry(NAME_None,
				LOCTEXT("ToggleLayerMask", "Toggle Mask"),
				LOCTEXT("ToggleLayerMaskTooltip", "Toggle the layer mask on and off.\n\nAlt+Shift+Left Click"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateWeakLambda(
					Layer,
					[Layer]()
					{
						FScopedTransaction Transaction(LOCTEXT("ToggleBaseStageEnabled", "Material Designer Toggle Base Stage Enabled"));

						if (UDMMaterialStage* Stage = Layer->GetStage(EDMMaterialLayerStage::Mask))
						{
							Stage->Modify();
							Stage->SetEnabled(!Stage->IsEnabled());
						}
					}
				))
			);
		}

		if (bAllowRemoveLayer)
		{
			if (TSharedPtr<SDMSlot> SlotWidget = MenuContext->GetSlotWidget().Pin())
			{
				//FDMSlotLayerMenus::AddLayerSection
				NewSection.AddMenuEntry(NAME_None,
					LOCTEXT("RemoveLayer", "Remove"),
					LOCTEXT("RemoveLayerTooltip", "Remove this layer from its Material Slot.\n\n"
						"Warning: Removing a stage off may result in inputs being reset where incompatibilities are found."),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateSP(
						SlotWidget.Get(),
						&SDMSlot::RemoveLayerByStage,
						Stage, 
						false
					))
				);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
