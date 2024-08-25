// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMMenuContext.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageBlend.h"
#include "Components/DMMaterialStageSource.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "ToolMenu.h"
#include "ToolMenus.h"

UDMMenuContext* UDMMenuContext::Create(const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject)
{
	UDMMenuContext* Context = NewObject<UDMMenuContext>();
	Context->SlotWidgetWeak = InSlotWidget;
	Context->StageWidgetWeak = InStageWidget;
	Context->LayerObjectWeak = InLayerObject;
	return Context;
}

UToolMenu* UDMMenuContext::GenerateContextMenu(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget, UDMMaterialLayerObject* InLayerObject)
{
	UToolMenu* NewMenu = UToolMenus::Get()->RegisterMenu(InMenuName, NAME_None, EMultiBoxType::Menu, false);
	if (!NewMenu)
	{
		return nullptr;
	}

	if (!IsValid(InLayerObject))
	{
		if (TSharedPtr<SDMStage> StageWidget = InStageWidget.Pin())
		{
			if (UDMMaterialStage* Stage = StageWidget->GetStage())
			{
				InLayerObject = Stage->GetLayer();
			}
		}
	}

	NewMenu->bToolBarForceSmallIcons = true;
	NewMenu->bShouldCloseWindowAfterMenuSelection = true;
	NewMenu->bCloseSelfOnly = true;

	TSharedPtr<FUICommandList> CommandList = nullptr;

	if (TSharedPtr<SDMSlot> Slot = InSlotWidget.Pin())
	{
		if (TSharedPtr<SDMEditor> Editor = Slot->GetEditorWidget())
		{
			CommandList = Editor->GetCommandList();
		}
	}

	NewMenu->Context = FToolMenuContext(CommandList, TSharedPtr<FExtender>(), Create(InSlotWidget, InStageWidget, InLayerObject));

	return NewMenu;
}

UDMMenuContext* UDMMenuContext::CreateEmpty()
{
	return Create(nullptr, nullptr, nullptr);
}

UDMMenuContext* UDMMenuContext::CreateSlot(const TWeakPtr<SDMSlot>& InSlotWidget)
{
	return Create(InSlotWidget, nullptr, nullptr);
}

UDMMenuContext* UDMMenuContext::CreateLayer(const TWeakPtr<SDMSlot>& InSlotWidget, UDMMaterialLayerObject* InLayerObject)
{
	return Create(InSlotWidget, nullptr, InLayerObject);
}

UDMMenuContext* UDMMenuContext::CreateStage(const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget)
{
	return Create(InSlotWidget, InStageWidget, nullptr);
}

UToolMenu* UDMMenuContext::GenerateContextMenuDefault(const FName InMenuName)
{
	return GenerateContextMenu(InMenuName, nullptr, nullptr, nullptr);
}

UToolMenu* UDMMenuContext::GenerateContextMenuSlot(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget)
{
	return GenerateContextMenu(InMenuName, InSlotWidget, nullptr, nullptr);
}

UToolMenu* UDMMenuContext::GenerateContextMenuLayer(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget, UDMMaterialLayerObject* InLayerObject)
{
	return GenerateContextMenu(InMenuName, InSlotWidget, nullptr, InLayerObject);
}

UToolMenu* UDMMenuContext::GenerateContextMenuStage(const FName InMenuName, const TWeakPtr<SDMSlot>& InSlotWidget, const TWeakPtr<SDMStage>& InStageWidget)
{
	return GenerateContextMenu(InMenuName, InSlotWidget, InStageWidget, nullptr);
}

UDMMaterialSlot* UDMMenuContext::GetSlot() const
{
	if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
	{
		return SlotWidget->GetSlot();
	}

	return nullptr;
}

UDynamicMaterialModel* UDMMenuContext::GetModel() const
{
	if (const UDMMaterialSlot* const Slot = GetSlot())
	{
		if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData())
		{
			return ModelEditorOnlyData->GetMaterialModel();
		}
	}

	return nullptr;
}

UDMMaterialStage* UDMMenuContext::GetStage() const
{
	if (TSharedPtr<SDMStage> StageWidget = StageWidgetWeak.Pin())
	{
		return StageWidget->GetStage();
	}

	return nullptr;
}

UDMMaterialStageSource* UDMMenuContext::GetStageSource() const
{
	if (UDMMaterialStage* Stage = GetStage())
	{
		return Stage->GetSource();
	}

	return nullptr;
}

UDMMaterialStageBlend* UDMMenuContext::GetStageSourceAsBlend() const
{
	return Cast<UDMMaterialStageBlend>(GetStageSource());
}

const UDMMaterialLayerObject* UDMMenuContext::GetLayer() const
{
	return LayerObjectWeak.Get();
}
