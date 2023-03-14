// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FDataprepSchemaAction;
struct FGraphActionListBuilderBase;

/**
 * The DataprepActionCollector is an interface for the SDataprepActionMenu
 */
class IDataprepMenuActionCollector
{
public:

	virtual ~IDataprepMenuActionCollector() {}

	/** The function that will be called to collect all the actions */
	virtual TArray<TSharedPtr<FDataprepSchemaAction>> CollectActions() = 0;

	/** Should the categories be auto expended */
	virtual bool ShouldAutoExpand() = 0;
};
