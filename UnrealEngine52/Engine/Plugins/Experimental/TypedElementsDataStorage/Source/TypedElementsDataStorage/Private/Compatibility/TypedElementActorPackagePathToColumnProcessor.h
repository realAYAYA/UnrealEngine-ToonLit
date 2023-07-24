// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementActorPackagePathToColumnProcessor.generated.h"

UCLASS()
class UTypedElementActorPackagePathToColumnProcessor : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UTypedElementActorPackagePathToColumnProcessor();

protected:
	void ConfigureQueries() override;
	void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};
