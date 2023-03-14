// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FRCLogicModeBase;
class SRemoteControlPanel;
class URemoteControlPreset;

/*
* 
* ~ SRCLogicPanelBase ~
*
* Base UI Widget for Logic Panels (Controllers / Behaviours / Actions).
* Provides access to parent Remote Control Preset object and common Delete Panel Item functionality
*/
class REMOTECONTROLUI_API SRCLogicPanelBase : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRCLogicPanelBase)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel);

	/** Returns the Remote Control Preset object associated with us*/
	virtual URemoteControlPreset* GetPreset() const;

	/** Returns the parent Remote Control Panel widget,
	* This is the main container holding all the Remote Control panels including Logic and Exposed Properties*/
	virtual TSharedPtr<SRemoteControlPanel> GetRemoteControlPanel() const;

	/** "Delete Item" UI command implementation for panels*/
	virtual void DeleteSelectedPanelItem() = 0;

	/** "Duplicate Item" UI command implementation for panels*/
	virtual void DuplicateSelectedPanelItem() {}

	/** "Copy Item" UI command implementation for panels*/
	virtual void CopySelectedPanelItem() {}

	/** "Paste Item" UI command implementation for panels*/
	virtual void PasteItemFromClipboard() {}

	/** Whether a given clipboard item can be successfully pasted into this panel */
	virtual bool CanPasteClipboardItem(UObject* InLogicClipboardItem) { return true; }

	/** Returns the UI item currently selected by the user (if any). To be implemented per child panel*/
	virtual TSharedPtr<FRCLogicModeBase> GetSelectedLogicItem() = 0;

	/** Provides an item suffix for the Paste context menu to provide users with useful context on the nature of the item being pasted */
	virtual FText GetPasteItemMenuEntrySuffix() { return FText::GetEmpty(); }

	/** Warns user before deleting a selected panel item. */
	virtual FReply RequestDeleteSelectedItem() = 0;

	/** Warns user before deleting all items in a panel. */
	virtual FReply RequestDeleteAllItems() = 0;

protected:
	/** The parent Remote Control Panel widget*/
	TWeakPtr<SRemoteControlPanel> PanelWeakPtr;
};
