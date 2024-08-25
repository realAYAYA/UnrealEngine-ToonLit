// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateTypes.h"
#include "UI/BaseLogicUI/SRCLogicPanelListBase.h"

struct FRCPanelStyle;
class URCBehaviour;
class FRCControllerModel;
class FRCBehaviourModel;
class ITableRow;
class ITableBase;
class SRCBehaviourPanel;
class SRemoteControlPanel;
class STableViewBase;
class URemoteControlPreset;
template <typename ItemType> class SListView;

/*
* ~ SRCBehaviourPanelList ~
*
* UI Widget for Behaviours List
* Used as part of the RC Logic Behaviour Panel.
*/
class REMOTECONTROLUI_API SRCBehaviourPanelList : public SRCLogicPanelListBase
{
public:
	SLATE_BEGIN_ARGS(SRCBehaviourPanelList)
	{
	}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, const TSharedRef<SRCBehaviourPanel> InBehaviourPanel, TSharedPtr<FRCControllerModel> InControllerItem, const TSharedRef<SRemoteControlPanel> InRemoteControlPanel);
	
	/** Returns true if the underlying list is valid and empty. */
	virtual bool IsEmpty() const override;
	
	/** Returns number of items in the list. */
	virtual int32 Num() const override;

	/** The number of Controllers currently selected */
	virtual int32 NumSelectedLogicItems() const override;

	/** Whether the Behaviours List View currently has focus. */
	virtual bool IsListFocused() const override;

	/** Deletes currently selected items from the list view */
	virtual void DeleteSelectedPanelItems() override;

	/** Returns the UI items currently selected by the user (if any). */
	virtual TArray<TSharedPtr<FRCLogicModeBase>> GetSelectedLogicItems() override;

	TSharedPtr<FRCBehaviourModel> GetSelectedBehaviourItem()
	{
		return SelectedBehaviourItemWeakPtr.Pin();
	}

	void RequestRefresh();

	virtual void AddNewLogicItem(UObject* InLogicItem) override;

	/** Allows Logic panels to add special functionality to the Context Menu based on context */
	virtual void AddSpecialContextMenuOptions(FMenuBuilder& MenuBuilder) override;

private:

	/** Enables or Disables the currently selected behaviour */
	void SetIsBehaviourEnabled(const bool bIsEnabled);

	/** Enables or Disables the passed behaviour */
	void SetIsBehaviourEnabled(const TSharedPtr<FRCBehaviourModel>& InBehaviourModel, const bool bIsEnabled);

	void AddBehaviourToList(URCBehaviour* InBehaviour);

	/** OnGenerateRow delegate for the Behaviours List View */
	TSharedRef<ITableRow> OnGenerateWidgetForList( TSharedPtr<FRCBehaviourModel> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	/** Return whether or not the given behaviour is checked */
	ECheckBoxState IsBehaviourChecked(const TSharedPtr<FRCBehaviourModel> InBehaviourModel) const;

	/** Executed when toggling the behaviour state */
	void OnToggleEnableBehaviour(ECheckBoxState State, TSharedPtr<FRCBehaviourModel> InBehaviourModel);

	/** OnSelectionChanged delegate for Behaviours List View */
	void OnTreeSelectionChanged(TSharedPtr<FRCBehaviourModel> InItem , ESelectInfo::Type);

	/** Responds to the selection of a newly created Behaviour. Resets UI state */
	void OnBehaviourAdded(const URCBehaviour* InBehaviour);

	/** Responds to the removal of all Behaviours. Rests UI state */
	void OnEmptyBehaviours();

	/** Refreshes the list from the latest state of the model */
	virtual void Reset() override;

	/** Handles broadcasting of a successful remove item operation. */
	virtual void BroadcastOnItemRemoved() override;

	/** Fetches the Remote Control preset associated with the parent panel */
	virtual URemoteControlPreset* GetPreset() override;

	/** Removes the given Behaviour UI model item from the list of UI models */
	virtual int32 RemoveModel(const TSharedPtr<FRCLogicModeBase> InModel) override;

	void OnBehaviourListModified();

private:

	/** The parent Behaviour Panel widget */
	TWeakPtr<SRCBehaviourPanel> BehaviourPanelWeakPtr;

	/** The Controller (UI model) associated with us */
	TWeakPtr<FRCControllerModel> ControllerItemWeakPtr;

	/** The currently selected Behaviour item (UI model) */
	TWeakPtr<FRCBehaviourModel> SelectedBehaviourItemWeakPtr;

	/** List of Behaviours (UI models) active in this widget */
	TArray<TSharedPtr<FRCBehaviourModel>> BehaviourItems;

	/** List View widget for representing our Behaviours List */
	TSharedPtr<SListView<TSharedPtr<FRCBehaviourModel>>> ListView;

	/** Panel Style reference. */
	const FRCPanelStyle* RCPanelStyle;
};

