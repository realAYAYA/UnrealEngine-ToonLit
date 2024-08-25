// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownRCControllerItem.h"

#include "Controller/RCController.h"
#include "IRemoteControlUIModule.h"
#include "RCVirtualProperty.h"
#include "SAvaRundownRCControllerItemRow.h"

FAvaRundownRCControllerItem::FAvaRundownRCControllerItem(int32 InInstanceIndex, FName InAssetName, URCController* InController, const TSharedRef<IDetailTreeNode>& InTreeNode)
{
	AssetName = InAssetName;
	InstanceIndex = InInstanceIndex;
	
	if (InController)
	{
		DisplayIndex = InController->DisplayIndex;
		
		const FName DisplayName = InController->DisplayName.IsNone()
			? InController->PropertyName
			: InController->DisplayName;
		
		DisplayNameText = FText::FromName(DisplayName);
		NodeWidgets = InTreeNode->CreateNodeWidgets();

		// Check if the Controller has a custom widget. In that case, overwrite the value widget with it
		if (const TSharedPtr<SWidget>& CustomControllerWidget = IRemoteControlUIModule::Get().CreateCustomControllerWidget(InController, InTreeNode->CreatePropertyHandle()))
		{
			NodeWidgets.ValueWidget = CustomControllerWidget;
		}
	}
}

TSharedRef<ITableRow> FAvaRundownRCControllerItem::CreateWidget(TSharedRef<SAvaRundownRCControllerPanel> InControllerPanel, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SAvaRundownRCControllerItemRow, InControllerPanel, InOwnerTable, SharedThis(this));
}
