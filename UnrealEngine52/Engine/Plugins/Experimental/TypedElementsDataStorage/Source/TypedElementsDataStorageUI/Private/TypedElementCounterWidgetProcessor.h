// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementCounterWidgetProcessor.generated.h"

/** Runs the count queries and updates the Slate widgets. */
UCLASS()
class TYPEDELEMENTSDATASTORAGEUI_API UTypedElementCounterWidgetProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTypedElementCounterWidgetProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery UiQuery;
};