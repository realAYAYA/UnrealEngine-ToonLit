// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraStackPropertyRowUtilities.h"

#include "HAL/PlatformApplicationMisc.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"

SNiagaraStackTableRow::FOnFillRowContextMenu FNiagaraStackPropertyRowUtilities::CreateOnFillRowContextMenu(TSharedPtr<IPropertyHandle> PropertyHandle, const FNodeWidgetActions& GeneratedPropertyNodeWidgetActions)
{
	return SNiagaraStackTableRow::FOnFillRowContextMenu::CreateStatic(&OnFillPropertyRowContextMenu, PropertyHandle, GeneratedPropertyNodeWidgetActions);
}

void FNiagaraStackPropertyRowUtilities::OnFillPropertyRowContextMenu(FMenuBuilder& MenuBuilder, TSharedPtr<IPropertyHandle> PropertyHandle, FNodeWidgetActions PropertyNodeWidgetActions)
{
	FUIAction CopyAction;
	FUIAction PasteAction;

	if (PropertyNodeWidgetActions.CopyMenuAction.ExecuteAction.IsBound() && PropertyNodeWidgetActions.PasteMenuAction.ExecuteAction.IsBound())
	{
		CopyAction = PropertyNodeWidgetActions.CopyMenuAction;
		PasteAction = PropertyNodeWidgetActions.PasteMenuAction;
	}
	else
	{
		if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
		{
			PropertyHandle->CreateDefaultPropertyCopyPasteActions(CopyAction, PasteAction);
		}
	}

	bool bEditSectionAdded = false;
	if (CopyAction.ExecuteAction.IsBound() && PasteAction.ExecuteAction.IsBound())
	{
		MenuBuilder.BeginSection(NAME_None, NSLOCTEXT("NiagaraStackPropertyRowUtilities", "PropertyRowEditMenuLabel", "Edit"));
		{
			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("NiagaraStackPropertyRowUtilities", "CopyProperty", "Copy"),
				NSLOCTEXT("NiagaraStackPropertyRowUtilities", "CopyPropertyToolTip", "Copy this property value"),
				FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Copy"),
				CopyAction);

			MenuBuilder.AddMenuEntry(
				NSLOCTEXT("NiagaraStackPropertyRowUtilities", "PasteProperty", "Paste"),
				NSLOCTEXT("NiagaraStackPropertyRowUtilities", "PastePropertyToolTip", "Paste the copied value here"),
				FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Paste"),
				PasteAction);
		}
		MenuBuilder.EndSection();
		bEditSectionAdded = true;
	}

	if (bEditSectionAdded && PropertyNodeWidgetActions.CustomMenuItems.Num() > 0)
	{
		MenuBuilder.AddSeparator();
	}

	for (const FNodeWidgetActionsCustomMenuData& CustomMenuData : PropertyNodeWidgetActions.CustomMenuItems)
	{
		MenuBuilder.AddMenuEntry(CustomMenuData.Name, CustomMenuData.Tooltip, CustomMenuData.SlateIcon, CustomMenuData.Action);
	}
}