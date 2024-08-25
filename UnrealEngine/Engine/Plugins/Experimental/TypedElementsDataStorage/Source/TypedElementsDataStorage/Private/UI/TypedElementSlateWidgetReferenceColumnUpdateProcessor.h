// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementSlateWidgetReferenceColumnUpdateProcessor.generated.h"

/**
 * Queries that check whether or not a widget still exists. If it has been deleted
 * then it will remove the column from the Data Storage or deletes the entire row if
 * the FTypedElementSlateWidgetReferenceDeletesRowTag was found.
 */
UCLASS()
class UTypedElementSlateWidgetReferenceColumnUpdateFactory : public UTypedElementDataStorageFactory
{
	GENERATED_BODY()

public:
	~UTypedElementSlateWidgetReferenceColumnUpdateFactory() override = default;

	void RegisterQueries(ITypedElementDataStorageInterface& DataStorage) override;

private:
	void RegisterDeleteRowOnWidgetDeleteQuery(ITypedElementDataStorageInterface& DataStorage) const;
	void RegisterDeleteColumnOnWidgetDeleteQuery(ITypedElementDataStorageInterface& DataStorage) const;
};
