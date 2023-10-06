// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Action/RCFunctionAction.h"
#include "Action/RCPropertyAction.h"
#include "UI/BaseLogicUI/RCLogicModeBase.h"

#include "UObject/WeakFieldPtr.h"

class FMenuBuilder;
class FRCBehaviourModel;
struct FRCPanelStyle;
class IDetailTreeNode;
class ITableRow;
class IPropertyRowGenerator;
class SHeaderRow;
class SHorizontalBox;
class SRCVirtualPropertyWidget;
class STableViewBase;
class SWidget;
class URCAction;
class URCPropertyAction;
class URCVirtualProperty;
class URCFunctionAction;

/* 
* ~ FRCActionModel ~
*
* UI model for representing an Action.
* Contains a row widget with Action related metadata and a generic value widget
*/
class FRCActionModel : public FRCLogicModeBase
{
public:
	FRCActionModel(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel);
	
	/** Fetch the Action (data model) associated with us*/
	URCAction* GetAction() const;

	/** Widget representing Action Name field */
	virtual TSharedRef<SWidget> GetNameWidget() const;

	/** Widget representing the Value field */
	virtual TSharedRef<SWidget> GetWidget() const override;

	/** Widget representing Color Coding for the Action Type*/
	virtual TSharedRef<SWidget> GetTypeColorTagWidget() const;

	/** Color code for this Action. Customized per action type*/
	virtual FLinearColor GetActionTypeColor() const = 0;

	/** Allows Logic panels to add special functionality to the Context Menu based on context */
	virtual void AddSpecialContextMenuOptions(FMenuBuilder& MenuBuilder);

	/** OnGenerateRow delegate for the Actions List View*/
	TSharedRef<ITableRow> OnGenerateWidgetForList(TSharedPtr<FRCActionModel> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get the Header Row appropriate for this particular Action model */
	static TSharedPtr<SHeaderRow> GetHeaderRow();

	/** Chooses the appropriate Action model for the current class and field type*/
	static TSharedPtr<FRCActionModel> GetModelByActionType(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel);

	/** Invoked when this Action item no longer has focus*/
	virtual void OnSelectionExit();

	/** Returns the Behaviour Item associated with us*/
	TSharedPtr<FRCBehaviourModel> GetParentBehaviour()
	{
		return BehaviourItemWeakPtr.Pin();
	}

protected:
	/** The Action (data model) associated with us*/
	TWeakObjectPtr<URCAction> ActionWeakPtr;

	/** Panel Style reference. */
	const FRCPanelStyle* RCPanelStyle;

	/** The Behaviour (UI model) associated with us*/
	TWeakPtr<FRCBehaviourModel> BehaviourItemWeakPtr;

	/** Optional: An editable virtual property widget associated with this row
	* Child action models can assign this variable and receive the Edit context menu option automatically*/
	TSharedPtr<SRCVirtualPropertyWidget> EditableVirtualPropertyWidget;
};

/*
* ~ FRCPropertyActionType ~
*
* Reusable class containing Property action related data and functions
*/
class FRCPropertyActionType
{
public:
	FRCPropertyActionType(URCPropertyAction* InPropertyAction);

	/** Property Name associated with this Action */
	const FName& GetPropertyName() const;

	/** The widget to be rendered for this Property
	* Represents the input field which the user will use to set a value for this Action
	*/
	TSharedRef<SWidget> GetPropertyWidget() const;

	FLinearColor GetPropertyTypeColor() const;

protected:
	/** The Property Action (data model) associated with us*/
	TWeakObjectPtr<URCPropertyAction> PropertyActionWeakPtr;

	/** The row generator used to represent this widget as a row, when used with SListView */
	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;

	/** Used to create a generic Value Widget for the property row widget*/
	TWeakPtr<IDetailTreeNode> DetailTreeNodeWeakPtr;
};

/*
* ~ FRCFunctionActionType ~
*
* Reusable class containing Function action related data and functions
*/
class FRCFunctionActionType
{
public:
	FRCFunctionActionType(URCFunctionAction* InFunctionAction) : FunctionActionWeakPtr(InFunctionAction) {}

	/** Color code for function actions in the Actions table */
	FLinearColor GetFunctionTypeColor() const
	{
		// @todo: Confirm color to be used for this with VP team.
		return FLinearColor::Blue; 
	}

protected:
	/** The Function Action (data model) associated with us*/
	TWeakObjectPtr<URCFunctionAction> FunctionActionWeakPtr;
};

/*
* ~ FRCPropertyActionModel ~
*
* UI model for Property based Actions
*/
class FRCPropertyActionModel : public FRCActionModel, public FRCPropertyActionType
{
public:
	FRCPropertyActionModel(URCPropertyAction* InPropertyAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
		: FRCActionModel(InPropertyAction, InBehaviourItem, InRemoteControlPanel),
		FRCPropertyActionType(InPropertyAction)	{ }

	virtual TSharedRef<SWidget> GetWidget() const override
	{
		return GetPropertyWidget();
	}

	/** Color code for this Action*/
	virtual FLinearColor GetActionTypeColor() const override
	{
		return GetPropertyTypeColor();
	}
};

/*
* ~ FRCFunctionActionModel ~
*
* UI model for Function based Actions
*/
class FRCFunctionActionModel : public FRCActionModel, public FRCFunctionActionType
{
public:
	FRCFunctionActionModel(URCFunctionAction* InFunctionAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
		: FRCActionModel(InFunctionAction, InBehaviourItem, InRemoteControlPanel)
		, FRCFunctionActionType(InFunctionAction) { }

	/** Color code for this Action*/
	virtual FLinearColor GetActionTypeColor() const override
	{
		return GetFunctionTypeColor();
	}
};
