// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImportTestFunctionsBase.h"
#include "InterchangeTestFunction.h"
#include "ActorImportTestFunctions.generated.h"

class AActor;
struct FInterchangeTestFunctionResult;


UCLASS()
class INTERCHANGETESTS_API UActorImportTestFunctions : public UImportTestFunctionsBase
{
	GENERATED_BODY()

public:

	// UImportTestFunctionsBase interface
	virtual UClass* GetAssociatedAssetType() const override;

	/** Check whether the expected number of actors are imported */
	UFUNCTION(Exec)
	static FInterchangeTestFunctionResult CheckImportedActorCount(const TArray<AActor*>& Actors, int32 ExpectedNumberOfImportedActors);
};
