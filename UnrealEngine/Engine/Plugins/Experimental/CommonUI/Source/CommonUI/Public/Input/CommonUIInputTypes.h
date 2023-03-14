// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIActionBindingHandle.h"
#include "UITag.h"
#include "Misc/EnumRange.h"
#include "CommonInputModeTypes.h"
#include "InputCoreTypes.h"

struct COMMONUI_API FBindUIActionArgs
{
	FBindUIActionArgs(FUIActionTag InActionTag, const FSimpleDelegate& InOnExecuteAction)
		: ActionTag(InActionTag)
		, OnExecuteAction(InOnExecuteAction)
	{}

	FBindUIActionArgs(FUIActionTag InActionTag, bool bShouldDisplayInActionBar, const FSimpleDelegate& InOnExecuteAction)
		: ActionTag(InActionTag)
		, bDisplayInActionBar(bShouldDisplayInActionBar)
		, OnExecuteAction(InOnExecuteAction)
	{}

	// @TODO: DarenC - Remove legacy.
	FBindUIActionArgs(const FDataTableRowHandle& InLegacyActionTableRow, const FSimpleDelegate& InOnExecuteAction)
		: LegacyActionTableRow(InLegacyActionTableRow)
		, OnExecuteAction(InOnExecuteAction)
	{}

	// @TODO: DarenC - Remove legacy.
	FBindUIActionArgs(const FDataTableRowHandle& InLegacyActionTableRow, bool bShouldDisplayInActionBar, const FSimpleDelegate& InOnExecuteAction)
		: LegacyActionTableRow(InLegacyActionTableRow)
		, bDisplayInActionBar(bShouldDisplayInActionBar)
		, OnExecuteAction(InOnExecuteAction)
	{}

	FName GetActionName() const;

	bool ActionHasHoldMappings() const;

	FUIActionTag ActionTag;
	FDataTableRowHandle LegacyActionTableRow;

	ECommonInputMode InputMode = ECommonInputMode::Menu;
	EInputEvent KeyEvent = IE_Pressed;

	/**
	 * A persistent binding is always registered and will be executed regardless of the activation status of the binding widget's parentage.
	 * Persistent bindings also never stomp one another - if two are bound to the same action, both will execute. Use should be kept to a minimum.
	 */
	bool bIsPersistent = false;

	/**
	 * True to have this binding consume the triggering key input.
	 * Persistent bindings that consume will prevent the key reaching non-persistent bindings and game agents.
	 * Non-persistent bindings that consume will prevent the key reaching game agents.
	 */
	bool bConsumeInput = true;

	/** Whether this binding can/should be displayed in a CommonActionBar (if one exists) */
	bool bDisplayInActionBar = true;

	/** Optional display name to associate with this binding instead of the default */
	FText OverrideDisplayName;

	FSimpleDelegate OnExecuteAction;

	/** If the bound action has any hold mappings, this will fire each frame while held. Has no bearing on actual execution and wholly irrelevant for non-hold actions */
	DECLARE_DELEGATE_OneParam(FOnHoldActionProgressed, float);
	FOnHoldActionProgressed OnHoldActionProgressed;
};
