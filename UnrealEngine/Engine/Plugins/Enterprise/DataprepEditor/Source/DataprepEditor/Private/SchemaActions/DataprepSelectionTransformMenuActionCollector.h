// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SchemaActions/IDataprepMenuActionCollector.h"

#include "CoreMinimal.h"

struct FDataprepSchemaAction;

/**
 * Help collecting all the dataprep selection transform menu action
 */
class FDataprepSelectionTransformMenuActionCollector : public IDataprepMenuActionCollector
{
public:
	virtual TArray<TSharedPtr<FDataprepSchemaAction>> CollectActions() override;
	virtual bool ShouldAutoExpand() override;

	int32 GroupingPriority = 0;

private:
	TSharedPtr<FDataprepSchemaAction> CreateMenuActionFromClass(UClass& Class);
};