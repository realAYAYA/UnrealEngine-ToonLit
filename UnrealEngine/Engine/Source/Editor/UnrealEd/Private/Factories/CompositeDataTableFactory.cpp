// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/CompositeDataTableFactory.h"

#include "Engine/CompositeDataTable.h"
#include "Templates/SubclassOf.h"

class UDataTable;
class UObject;

UCompositeDataTableFactory::UCompositeDataTableFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UCompositeDataTable::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UDataTable* UCompositeDataTableFactory::MakeNewDataTable(UObject* InParent, FName Name, EObjectFlags Flags)
{
	return NewObject<UCompositeDataTable>(InParent, Name, Flags);
}