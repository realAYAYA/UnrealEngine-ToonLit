// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/BaseLogicUI/SRCLogicPanelBase.h"

enum class ECheckBoxState : uint8;

struct FRCPanelStyle;
class FRCActionModel;
struct FRemoteControlField;
class FRCBehaviourModel;
class SBox;
class SCheckBox;
class SRCBehaviourDetails;
class SRCLogicPanelListBase;
class SRemoteControlPanel;
class URCAction;
class URCBehaviour;

/*
* ~ SRCActionPanel ~
*
* UI Widget for Action Panel.
* Contains a header, footer, list of actions and a behaviour specific panel
*/
class REMOTECONTROLUI_API SRCActionPanel : public SRCLogicPanelBase
{
public:
	SLATE_BEGIN_ARGS(SRCActionPanel)
		{
		}

	SLATE_END_ARGS()
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<SRemoteControlPanel>& InPanel);

	/** Shutdown panel */
	static void Shutdown();

	/** Whether the Actions list widget currently has focus.*/
	bool IsListFocused() const;

	/** Delete Item UI command implementation for this panel */
	virtual void DeleteSelectedPanelItem() override;

	/** Returns the UI item currently selected by the user (if any)*/
	virtual TSharedPtr<FRCLogicModeBase> GetSelectedLogicItem() override;

	/** "Duplicate Item" UI command implementation for Action panel*/
	virtual void DuplicateSelectedPanelItem() override;

	/** "Copy Item" UI command implementation for Action panel*/
	virtual void CopySelectedPanelItem() override;

	/** "Paste Item" UI command implementation for Action panel*/
	virtual void PasteItemFromClipboard() override;

	/** Whether a given clipboard item can be successfully pasted into this panel */
	virtual bool CanPasteClipboardItem(UObject* InLogicClipboardItem) override;

	/** Provides an item suffix for the Paste context menu to provide users with useful context on the nature of the item being pasted */
	virtual FText GetPasteItemMenuEntrySuffix() override;

	/** Adds an Action for the currently active Behaviour and broadcasts to parent panels */
	URCAction* AddAction(const TSharedRef<const FRemoteControlField> InRemoteControlField);

	/** Whether this Actions panel can create an action for the given remote control field */
	bool CanHaveActionForField(const FGuid& InRemoteControlFieldId);

	void RequestRefreshForAddActionsMenu()
	{
		bAddActionMenuNeedsRefresh = true;
	}

	/** Set the Enabled state of our parent Behaviour */
	void SetIsBehaviourEnabled(const bool bIsEnabled);

	/** Fetches the currently selected behaviour item*/
	TSharedPtr<FRCBehaviourModel> GetSelectedBehaviourItem()
	{
		return SelectedBehaviourItemWeakPtr.Pin();
	}

	/** Refreshes the UI widgets enabled state depending on whether the parent behaviour is currently enabled */
	void RefreshIsBehaviourEnabled(const bool bIsEnabled);

protected:
	/** Warns user before deleting a selected panel item. */
	virtual FReply RequestDeleteSelectedItem() override;

	/** Warns user before deleting all items in a panel. */
	virtual FReply RequestDeleteAllItems() override;

private:
	/** Determines the visibility Add All Button. */
	EVisibility HandleAddAllButtonVisibility() const;

	/** 
	* Behaviour selection change listener.
	* Updates the list of actions from newly selected Behaviour
	*/
	void OnBehaviourSelectionChanged(TSharedPtr<FRCBehaviourModel> InBehaviourItem);

	/* Rebuilds the Action Panel for a newly selected Behaviour*/
	void UpdateWrappedWidget(TSharedPtr<FRCBehaviourModel> InBehaviourItem = nullptr);

	/** Handles click event for Open Behaviour Blueprint button*/
	FReply OnClickOverrideBlueprintButton();

	/**
	 * Builds a menu containing the list of all possible Actions
	 * These are derived from the list of Exposed entities of the Remote Control Preset associated with us.
	 */
	TSharedRef<SWidget> GetActionMenuContentWidget();

	/** Handles click event for Add Action button*/
	void OnAddActionClicked(TSharedPtr<FRemoteControlField> InRemoteControlField);

	/** Handles click event for Empty button; clear all Actions from the panel*/
	FReply OnClickEmptyButton();

	/** Handles click event for Add All button; Adds all possible actions for the active Remote Control Preset*/
	FReply OnAddAllFields();

	/** Event invoked when a new remote control field has been added to the Remote Control Preset associated with this Action panel */
	void OnRemoteControlFieldAdded(const FGuid& GroupId, const FGuid& FieldId, int32 FieldPosition);

	/** Event invoked when a remote control field is removed from the Remote Control Preset associated with this Action panel */
	void OnRemoteControlFieldDeleted(const FGuid& GroupId, const FGuid& FieldId, int32 FieldPosition);

private:

	void DuplicateAction(URCAction* InAction);

	void AddNewActionToList(URCAction* NewAction);

	/** The parent Behaviour that this Action panel is associated with */
	TWeakPtr<FRCBehaviourModel> SelectedBehaviourItemWeakPtr;
	
	/** Parent Box for the entire widget */
	TSharedPtr<SBox> WrappedBoxWidget;

	/** Widget representing List of Actions */
	TSharedPtr<SRCLogicPanelListBase> ActionPanelList;

	/** Behaviour specific details widget*/
	TSharedPtr<SRCBehaviourDetails> BehaviourDetailsWidget;

	/** Helper widget for behavior details. */
	static TSharedPtr<SBox> NoneSelectedWidget;

	/** Panel Style reference. */
	const FRCPanelStyle* RCPanelStyle;

	/** Cached menu widget for Add New Action */
	TSharedPtr<SWidget> AddNewActionMenuWidget;

	/** Whether the Add Actions menu list is outdated and needs to be refreshed */
	bool bAddActionMenuNeedsRefresh = false;
};