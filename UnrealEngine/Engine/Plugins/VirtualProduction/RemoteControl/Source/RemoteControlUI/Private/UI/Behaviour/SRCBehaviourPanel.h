// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Behaviour/RCBehaviourNode.h"
#include "UI/BaseLogicUI/SRCLogicPanelBase.h"

struct FRCPanelStyle;
class FRCBehaviourModel;
class FRCControllerModel;
class SBox;
class SRCBehaviourPanel;
class URCController;

/*
* ~ SRCBehaviourPanel ~
*
* UI Widget for Behaviour Panel.
* Contains a header (Add/Remove/Empty) and List of Behaviours
*/
class REMOTECONTROLUI_API SRCBehaviourPanel : public SRCLogicPanelBase
{
public:
	SLATE_BEGIN_ARGS(SRCBehaviourPanel)
		{
		}

	SLATE_END_ARGS()
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel);

	/** Shutdown panel */
	static void Shutdown();

	/** Whether the Behaviour list widget currently has focus. Used for Delete Item UI command */
	bool IsListFocused() const;

	/** Delete Item UI command implementation for this panel */
	virtual void DeleteSelectedPanelItem() override;

	/** "Duplicate Item" UI command implementation for Behaviour panel*/
	virtual void DuplicateSelectedPanelItem() override;

	/** "Copy Item" UI command implementation for Behaviour panel*/
	virtual void CopySelectedPanelItem() override;

	/** "Paste Item" UI command implementation for Behaviour panel*/
	virtual void PasteItemFromClipboard() override;

	/** Whether a given clipboard item can be successfully pasted into this panel */
	virtual bool CanPasteClipboardItem(UObject* InLogicClipboardItem) override;

	/** Provides an item suffix for the Paste context menu to provide users with useful context on the nature of the item being pasted */
	virtual FText GetPasteItemMenuEntrySuffix() override;

	/** Returns the UI item currently selected by the user (if any)*/
	virtual TSharedPtr<FRCLogicModeBase> GetSelectedLogicItem() override;

	/** Returns the parent Controller associated with this behaviour*/
	URCController* GetParentController();

protected:

	/** Warns user before deleting a selected panel item. */
	virtual FReply RequestDeleteSelectedItem() override;

	/** Warns user before deleting all items in a panel. */
	virtual FReply RequestDeleteAllItems() override;

private:
	/** Duplicates a given Behaviour object*/
	void DuplicateBehaviour(URCBehaviour* InBehaviour);
	
	/**
	* Controller list selection change listener.
	* Updates the list of behaviours from newly selected Controller
	*/
	void OnControllerSelectionChanged(TSharedPtr<FRCControllerModel> InControllerItem);

	/* Rebuilds the Behaviour Panel for a newly selected Controller*/
	void UpdateWrappedWidget(TSharedPtr<FRCControllerModel> InControllerItem = nullptr);

	/** Builds a menu containing the list of all possible Behaviours */
	TSharedRef<SWidget> GetBehaviourMenuContentWidget();

	/** Handles click event for Add Behaviour button*/
	void OnAddBehaviourClicked(UClass* InClass);

	/** Handles click event for "Empty" button; clears all Behaviours from the panel*/
	FReply OnClickEmptyButton();

private:

	/** The parent Controller that this Behaviour panel is associated with */
	TWeakPtr<FRCControllerModel> SelectedControllerItemWeakPtr = nullptr;
	
	/** Parent Box for the entire widget */
	TSharedPtr<SBox> WrappedBoxWidget;

	/** Widget representing List of Behaviours */
	TSharedPtr<class SRCBehaviourPanelList> BehaviourPanelList;

	/** Helper widget for behavior details. */
	static TSharedPtr<SBox> NoneSelectedWidget;

	/** Panel Style reference. */
	const FRCPanelStyle* RCPanelStyle;
};
