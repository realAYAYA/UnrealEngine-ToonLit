// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassCommonFragments.h"
#include "MassRepresentationFragments.h"
#include "MassProcessor.h"
#include "MassLODCalculator.h"

#include "MassVisualizationLODProcessor.generated.h"

UCLASS()
class MASSREPRESENTATION_API UMassVisualizationLODProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UMassVisualizationLODProcessor();

protected:

	/** Configure the owned FMassEntityQuery instances to express processor's requirements */
	virtual void ConfigureQueries() override;

	/** 
	 * Execution method for this processor 
	 * @param EntitySubsystem is the system to execute the lambdas on each entity chunk
	 * @param Context is the execution context to be passed when executing the lambdas
	 */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	
	/**
	 * Forces Off LOD on all calculation
	 * @param bForce to whether force or not Off LOD
	 */
	void ForceOffLOD(bool bForce) { bForceOFFLOD = bForce; }

protected:
	FMassEntityQuery CloseEntityQuery;
	FMassEntityQuery CloseEntityAdjustDistanceQuery;
	FMassEntityQuery FarEntityQuery;
	FMassEntityQuery DebugEntityQuery;

	bool bForceOFFLOD = false;

	UPROPERTY(Transient)
	TObjectPtr<const UScriptStruct> FilterTag = nullptr;
};