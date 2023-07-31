// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Action/RCFunctionAction.h"
#include "Action/RCPropertyAction.h"
#include "UI/Action/RCActionModel.h"

class SActionItemListRow;
class SBox;
class SRCVirtualPropertyWidget;

/* 
* ~ FRCActionConditionalModel ~
*
* UI model for representing an Action of the Conditional Behaviour.
* Contains a Condition widget for representing the comparand/comparator and target property/value widgets
* 
*  <Actions Table>       <Condition>                <Description>         <Value>
* 
*  Color coding | Comparator+Comparand | Target Property | Value To Set
*/
class FRCActionConditionalModel : public FRCActionModel
{
public:
	FRCActionConditionalModel(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel);	

	/** Widget representing the Condition (Comparator|Comparand) used
	* to trigger the corresponding action
	*/
	TSharedRef<SWidget> GetConditionWidget();

	/** OnGenerateRow delegate for the Actions List View*/
	TSharedRef<ITableRow> OnGenerateWidgetForList(TSharedPtr<FRCActionConditionalModel> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Get the Header Row appropriate for this particular Action model */
	static TSharedPtr<SHeaderRow> GetHeaderRow();

	/** Chooses the appropriate Action model for the current class and field type*/
	static TSharedPtr<FRCActionConditionalModel> GetModelByActionType(URCAction* InAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel);

private:
	TSharedRef<SWidget> OnGenerateConditionWidget(class URCVirtualPropertySelfContainer* InComparand);
};

/*
* ~ FRCPropertyActionConditionalModel ~
*
* UI model for Property based Conditional Actions
*/
class FRCPropertyActionConditionalModel : public FRCActionConditionalModel, public FRCPropertyActionType
{
public:
	FRCPropertyActionConditionalModel(URCPropertyAction* InPropertyAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
	: FRCActionConditionalModel(InPropertyAction, InBehaviourItem, InRemoteControlPanel),
		FRCPropertyActionType(InPropertyAction) { }

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
* ~ FRCFunctionActionConditionalModel ~
*
* UI model for Function based Conditional Actions
*/
class FRCFunctionActionConditionalModel : public FRCActionConditionalModel, public FRCFunctionActionType
{
public:
	FRCFunctionActionConditionalModel(URCFunctionAction* InFunctionAction, const TSharedPtr<class FRCBehaviourModel> InBehaviourItem, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
		: FRCActionConditionalModel(InFunctionAction, InBehaviourItem, InRemoteControlPanel)
		, FRCFunctionActionType(InFunctionAction) { }

	/** Color code for this Action*/
	virtual FLinearColor GetActionTypeColor() const override
	{
		return GetFunctionTypeColor();
	}
};