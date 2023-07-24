// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "MassProcessor.h"

#include "TypedElementSlateWidgetReferenceColumnUpdateProcessor.generated.h"

/**
 * Processor that checks whether or not a widget still exists. If it has been deleted
 * then it will remove the column from the Data Storage or deletes the entire row if
 * the FTypedElementSlateWidgetReferenceDeletesRowTag was found.
 */
UCLASS()
class UTypedElementSlateWidgetReferenceColumnUpdateProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementSlateWidgetReferenceColumnUpdateProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	template<bool DeleteRow>
	static void CheckStatus(FMassExecutionContext& Context);

	FMassEntityQuery ColumnRemovalQuery;
	FMassEntityQuery RowDeletionQuery;
};