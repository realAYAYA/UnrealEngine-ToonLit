// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataTableFactory.h"
#include "CompositeDataTableFactory.generated.h"

UCLASS(hidecategories = Object, MinimalAPI)
class UCompositeDataTableFactory : public UDataTableFactory
{
	GENERATED_UCLASS_BODY()

protected:
	UNREALED_API virtual UDataTable* MakeNewDataTable(UObject* InParent, FName Name, EObjectFlags Flags) override;
};

