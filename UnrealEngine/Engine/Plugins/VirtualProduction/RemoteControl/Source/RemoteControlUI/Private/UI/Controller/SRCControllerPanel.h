// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBag.h"
#include "UI/BaseLogicUI/SRCLogicPanelBase.h"

enum class EPropertyBagPropertyType : uint8;
class FRCControllerModel;
struct FRCPanelStyle;
class SRCControllerPanel;
class URCController;

/*
* ~ SRCControllerPanel ~
*
* UI Widget for Controller Panel.
* Contains a header (Add/Remove/Empty) and List of Controllers
*/
class REMOTECONTROLUI_API SRCControllerPanel : public SRCLogicPanelBase
{
public:
	SLATE_BEGIN_ARGS(SRCControllerPanel)
	{}

		SLATE_ATTRIBUTE(bool, LiveMode)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel);

	/** Whether the Controller list widget currently has focus. Used for Delete Item UI command */
	bool IsListFocused() const;

	/** Delete Item UI command implementation for this panel */
	virtual void DeleteSelectedPanelItem() override;

	/** "Duplicate Item" UI command implementation for Controller panel*/
	virtual void DuplicateSelectedPanelItem() override;

	/** "Copy Item" UI command implementation for Controller panel*/
	virtual void CopySelectedPanelItem() override;

	/** "Paste Item" UI command implementation for Controller panel*/
	virtual void PasteItemFromClipboard() override;

	/** Provides an item suffix for the Paste context menu to provide users with useful context on the nature of the item being pasted */
	virtual FText GetPasteItemMenuEntrySuffix() override;

	/** Returns the UI item currently selected by the user (if any)*/
	virtual TSharedPtr<FRCLogicModeBase> GetSelectedLogicItem() override;

	void EnterRenameMode();

protected:
	/** Warns user before deleting a selected panel item. */
	virtual FReply RequestDeleteSelectedItem() override;

	/** Warns user before deleting all items in a panel. */
	virtual FReply RequestDeleteAllItems() override;

private:
	/** Duplicates a given Controller object*/
	void DuplicateController(URCController* InController);

	/** Builds a menu containing the list of all possible Controllers*/
	TSharedRef<SWidget> GetControllerMenuContentWidget() const;

	/** Handles click event for Add Controller button*/
	void OnAddControllerClicked(const EPropertyBagPropertyType InValueType, UObject* InValueTypeObject = nullptr) const;

	/** Handles click event for Empty button; clears all controllers from the panel*/
	FReply OnClickEmptyButton();

private:

	/** Widget representing List of Controllers */
	TSharedPtr<class SRCControllerPanelList> ControllerPanelList;

	/** Whether the panel is in live mode. */
	TAttribute<bool> bIsInLiveMode;

	/** Panel Style reference. */
	const FRCPanelStyle* RCPanelStyle;
};
