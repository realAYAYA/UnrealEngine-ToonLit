// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassProcessor.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementMiscProcessors.generated.h"

/**
 * Removes all FTypedElementSyncBackToWorldTags at the end of an update cycle.
 */
UCLASS()
class UTypedElementRemoveSyncToWorldTagProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementRemoveSyncToWorldTagProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};