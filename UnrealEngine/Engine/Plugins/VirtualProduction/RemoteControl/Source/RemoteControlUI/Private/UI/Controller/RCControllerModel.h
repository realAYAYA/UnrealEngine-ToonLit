// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/BaseLogicUI/RCLogicModeBase.h"
#include "UObject/WeakFieldPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"

class FRCBehaviourModel;
class IDetailTreeNode;
class SInlineEditableTextBlock;
class SRemoteControlPanel;
class URCBehaviour;
class URCControllerContainer;
class URCUserDefinedStruct;
class URCVirtualPropertyContainerBase;
class URCVirtualPropertyBase;

/*
* ~ FRCControllerModel ~
*
* UI model for representing a Controller
* Contains a row widget with Controller Name and a Value Widget
*/
class FRCControllerModel : public FRCLogicModeBase
{
public:
	FRCControllerModel(URCVirtualPropertyBase* InVirtualProperty, const TSharedRef<IDetailTreeNode>& InTreeNode, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel);
	
	/** The widget to be rendered for this Controller
	* Used to represent a single row when added to the Controllers Panel List
	*/
	virtual TSharedRef<SWidget> GetWidget() const override;

	TSharedRef<SWidget> GetNameWidget() const;

	/** Returns the Controller (Data Model) associated with us*/
	virtual URCVirtualPropertyBase* GetVirtualProperty() const;
	
	/** Returns the Controller Name (from Virtual Property) represented by us*/
	virtual const FName GetPropertyName() const;

	/** Returns the currently selected Behaviour (UI model) */
	TSharedPtr<FRCBehaviourModel> GetSelectedBehaviourModel() const;

	/** Updates our internal record of the currently selected Behaviour (UI Model) */
	void UpdateSelectedBehaviourModel(TSharedPtr<FRCBehaviourModel> InModel);

	/** Allows users to enter text into the Controller Name Box */
	void EnterRenameMode();

	/** User-friendly Name of the underlying Controller */
	FName GetControllerDisplayName();

	/**Fetches the unique Id for this UI item */
	FGuid GetId() const { return Id; }

private:
	/* Text commit event for Controller Name text box*/
	void OnControllerNameCommitted(const FText& InNewControllerName, ETextCommit::Type InCommitInfo);

	/** Virtual Property (Data Model) represented by us*/
	TWeakObjectPtr<URCVirtualPropertyBase> VirtualPropertyWeakPtr;
	
	/** The row generator used to build a generic Value Widget*/
	TWeakPtr<IDetailTreeNode> DetailTreeNodeWeakPtr;

	/** The currently selected Behaviour (UI model) */
	TWeakPtr<FRCBehaviourModel>  SelectedBehaviourModelWeakPtr;

	/** Controller name - editable text box */
	TSharedPtr<SInlineEditableTextBlock> ControllerNameTextBox;

	/**Unique Id for this UI item*/
	FGuid Id;
};
