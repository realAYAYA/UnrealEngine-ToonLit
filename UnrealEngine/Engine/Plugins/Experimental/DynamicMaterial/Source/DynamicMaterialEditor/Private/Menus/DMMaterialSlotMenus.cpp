// Copyright Epic Games, Inc. All Rights Reserved.

#include "Menus/DMMaterialSlotMenus.h"
#include "Menus/DMMaterialSlotLayerMenus.h"
#include "Menus/DMMenuContext.h"
#include "Slate/SDMSlot.h"
#include "ToolMenus.h"
#include "Widgets/SWidget.h"

#define LOCTEXT_NAMESPACE "FDMMaterialSlotMenus"

namespace UE::DynamicMaterialEditor::Private
{
	static const FName SlotAddLayerMenuName = TEXT("MaterialDesigner.MaterialSlot.AddLayer");
	static const FName SlotSectionName = TEXT("Slot");

	void AddSlotMenu(UToolMenu* InMenu)
	{
		if (!IsValid(InMenu) || InMenu->ContainsSection(SlotSectionName))
		{
			return;
		}

		const UDMMenuContext* const MenuContext = InMenu->FindContext<UDMMenuContext>();

		if (!MenuContext)
		{
			return;
		}

		FToolMenuSection& NewSection = InMenu->AddSection(SlotSectionName, LOCTEXT("MaterialSlotMenu", "Material Slot"));
		{
			NewSection.AddMenuEntry(
				NAME_None,
				LOCTEXT("RemoveSlot", "Remove"),
				LOCTEXT("RemoveSlotTooltip", "Remove this stage from its Material Slot.\n\n"
					"Warning: Removing a stage off may result in inputs being reset where incompatibilities are found."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(
					MenuContext->GetSlotWidget().Pin().Get(),
					&SDMSlot::RemoveSlot
				))
			);
		}
	}
}

TSharedRef<SWidget> FDMMaterialSlotMenus::MakeAddLayerButtonMenu(const TSharedPtr<SDMSlot>& InSlotWidget)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (!UToolMenus::Get()->IsMenuRegistered(SlotAddLayerMenuName))
	{
		UToolMenu* NewToolMenu = UDMMenuContext::GenerateContextMenuDefault(SlotAddLayerMenuName);

		if (!NewToolMenu)
		{
			return SNullWidget::NullWidget;
		}

		NewToolMenu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateStatic(&FDMMaterialSlotLayerMenus::AddAddLayerSection));
	}

	FToolMenuContext MenuContext(UDMMenuContext::CreateSlot(InSlotWidget));

	return UToolMenus::Get()->GenerateWidget(SlotAddLayerMenuName, MenuContext);
}

#undef LOCTEXT_NAMESPACE
