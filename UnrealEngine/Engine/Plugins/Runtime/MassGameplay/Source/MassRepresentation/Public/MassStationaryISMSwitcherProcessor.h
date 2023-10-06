// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "MassProcessor.h"
#include "MassStationaryISMSwitcherProcessor.generated.h"

namespace UE::Mass::Signals
{
	const FName SwitchedToActor = FName(TEXT("SwitchedToActor"));
	const FName SwitchedToISM = FName(TEXT("SwitchedToISM"));
}

/** 
 * This processor's sole responsibility is to process all entities tagged with FMassStaticRepresentationTag
 * and check if they've switched to or away from EMassRepresentationType::StaticMeshInstance; and acordingly add or remove 
 * the entity from the appropriate FMassInstancedStaticMeshInfoArrayView.
 */
UCLASS()
class MASSREPRESENTATION_API UMassStationaryISMSwitcherProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassStationaryISMSwitcherProcessor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// UMassProcessor overrides begin
	virtual void ConfigureQueries() override;
	// UMassProcessor overrides end
	// 
	// UMassRepresentationProcessor overrides begin
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	// UMassRepresentationProcessor overrides end

	FMassEntityQuery EntityQuery;
};
