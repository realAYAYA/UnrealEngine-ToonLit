// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveCommand.h"
#include "Selection/GeometrySelector.h"    // for FGeometrySelectionHandle
#include "SelectionEditInteractiveCommand.generated.h"

/**
 * Arguments for a UGeometrySelectionEditCommand
 */
UCLASS()
class MODELINGCOMPONENTS_API UGeometrySelectionEditCommandArguments : public UInteractiveCommandArguments
{
	GENERATED_BODY()
public:
	FGeometrySelectionHandle SelectionHandle;

	bool IsSelectionEmpty() const
	{
		return SelectionHandle.Selection == nullptr || SelectionHandle.Selection->IsEmpty();
	}

	bool IsMatchingType(FGeometryIdentifier::ETargetType TargetType, FGeometryIdentifier::EObjectType EngineType) const
	{
		return SelectionHandle.Identifier.TargetType == TargetType
			|| SelectionHandle.Identifier.ObjectType == EngineType;
	}
};



/**
 * UGeometrySelectionEditCommand is a command that edits geometry based on a selection.
 * Requires a UGeometrySelectionEditCommandArguments
 */
UCLASS(Abstract)
class MODELINGCOMPONENTS_API UGeometrySelectionEditCommand : public UInteractiveCommand
{
	GENERATED_BODY()
public:
	virtual bool AllowEmptySelection() const { return false; }

	virtual bool CanExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* SelectionArgs)
	{
		return false;
	}
	
	virtual void ExecuteCommandForSelection(UGeometrySelectionEditCommandArguments* Arguments)
	{
	}


public:

	// UInteractiveCommand API

	virtual bool CanExecuteCommand(UInteractiveCommandArguments* Arguments) override
	{
		UGeometrySelectionEditCommandArguments* SelectionArgs = Cast<UGeometrySelectionEditCommandArguments>(Arguments);
		if  (SelectionArgs && (SelectionArgs->IsSelectionEmpty() == false || AllowEmptySelection()) )
		{
			return CanExecuteCommandForSelection(SelectionArgs);
		}
		return false;
	}

	virtual void ExecuteCommand(UInteractiveCommandArguments* Arguments) override
	{
		UGeometrySelectionEditCommandArguments* SelectionArgs = Cast<UGeometrySelectionEditCommandArguments>(Arguments);
		if ( SelectionArgs && (SelectionArgs->IsSelectionEmpty() == false || AllowEmptySelection()) )
		{
			ExecuteCommandForSelection(SelectionArgs);
		}
	}


};
