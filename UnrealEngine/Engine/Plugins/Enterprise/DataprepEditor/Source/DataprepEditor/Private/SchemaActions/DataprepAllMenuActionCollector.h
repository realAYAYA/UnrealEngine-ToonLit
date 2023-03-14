// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SchemaActions/IDataprepMenuActionCollector.h"

#include "CoreMinimal.h"

struct FDataprepSchemaAction;

/**
 * Help collecting all the dataprep specific menu action (operations and filters)
 */
class FDataprepAllMenuActionCollector : public IDataprepMenuActionCollector
{
public:
	virtual TArray<TSharedPtr<FDataprepSchemaAction>> CollectActions() override;
	virtual bool ShouldAutoExpand() override;
};