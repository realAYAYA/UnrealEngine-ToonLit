// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NiagaraActions.h"

/** Represents a single action for adding an item to a group in the stack. */
class INiagaraStackItemGroupAddAction
{
public:
	/** Gets the category for this action. */
	virtual TArray<FString> GetCategories() const = 0;

	/** Gets the short display name for this action. */
	virtual FText GetDisplayName() const = 0;

	/** Gets a long description of what will happen if this add action is executed. */
	virtual FText GetDescription() const = 0;

	/** Gets a space separated string of keywords which expose additional search terms for this action. */
	virtual FText GetKeywords() const = 0;

	/** Gets a bool that indicates whether this action is suggested or not */
	virtual bool GetSuggested() const
	{
		return false;
	}

	/** Indicates if this action represents a library action */
	virtual bool IsInLibrary() const
	{
		return true;
	}

	/** Gets a source data struct that is useful to display additional information about an action */
	virtual FNiagaraActionSourceData GetSourceData() const
	{
		return FNiagaraActionSourceData();
	}

	virtual ~INiagaraStackItemGroupAddAction() { }
};

/** Defines options for adding items to groups in the stack. */
struct FNiagaraStackItemGroupAddOptions
{
	/** Whether or not to include deprecated items. */
	bool bIncludeDeprecated		= false;

	/** Whether or not to include non-library items. */
	bool bIncludeNonLibrary		= false;
};

/** Defines utilities for generically handling adding items to groups in the stack. */
class INiagaraStackItemGroupAddUtilities
{
public:
	/** Defines different modes for adding to a stack group. */
	enum EAddMode
	{
		/** The group adds a new item directly. */
		AddDirectly,
		/** The group provides a list of actions to choose from for adding. */
		AddFromAction
	};

public:
	/** Gets the generic name for the type of item to add e.g. "Module" */
	virtual FText GetAddItemName() const = 0;

	virtual bool GetShowLabel() const = 0;

	/** Gets whether or not the add actions should be automatically expanded in the UI. */
	virtual bool GetAutoExpandAddActions() const = 0;

	/** Gets the add mode supported by these add group utilities. */
	virtual EAddMode GetAddMode() const = 0;

	/** Adds a new item directly. */
	virtual void AddItemDirectly() = 0;

	/** Populates an array with the valid add actions. */
	virtual void GenerateAddActions(TArray<TSharedRef<INiagaraStackItemGroupAddAction>>& OutAddActions, const FNiagaraStackItemGroupAddOptions& AddProperties = FNiagaraStackItemGroupAddOptions()) const = 0;

	/** Executes the specified add action. */
	virtual void ExecuteAddAction(TSharedRef<INiagaraStackItemGroupAddAction> AddAction, int32 TargetIndex) = 0;

	/** Should we add a library filter to the add menu? */
	virtual bool SupportsLibraryFilter() const = 0;

	/** Should we add a source filter to the add menu? */
	virtual bool SupportsSourceFilter() const = 0;

	virtual ~INiagaraStackItemGroupAddUtilities() { }
};

class FNiagaraStackItemGroupAddUtilities : public INiagaraStackItemGroupAddUtilities
{
public:
	FNiagaraStackItemGroupAddUtilities(FText InAddItemName, EAddMode InAddMode, bool bInAutoExpandAddActions, bool bInShowLabel)
		: AddItemName(InAddItemName)
		, bShowLabel(bInShowLabel)
		, bAutoExpandAddActions(bInAutoExpandAddActions)
		, AddMode(InAddMode)
	{
	}

	virtual FText GetAddItemName() const override { return AddItemName; }
	virtual bool GetShowLabel() const override { return bShowLabel; }
	virtual bool GetAutoExpandAddActions() const override { return bAutoExpandAddActions; }
	virtual EAddMode GetAddMode() const override { return AddMode; }

	virtual bool SupportsLibraryFilter() const override { return false; }
	virtual bool SupportsSourceFilter() const override { return false; }

protected:
	FText AddItemName;
	bool bShowLabel;
	bool bAutoExpandAddActions;
	EAddMode AddMode;
};

template<typename AddedItemType>
class TNiagaraStackItemGroupAddUtilities : public FNiagaraStackItemGroupAddUtilities
{
public:
	DECLARE_DELEGATE_OneParam(FOnItemAdded, AddedItemType);

public:
	TNiagaraStackItemGroupAddUtilities(FText InAddItemName, EAddMode InAddMode, bool bInAutoExpandAddActions, bool bInShowLabel, FOnItemAdded InOnItemAdded)
		: FNiagaraStackItemGroupAddUtilities(InAddItemName, InAddMode, bInAutoExpandAddActions, bInShowLabel)
		, OnItemAdded(InOnItemAdded)
	{
	}

protected:
	FOnItemAdded OnItemAdded;
};