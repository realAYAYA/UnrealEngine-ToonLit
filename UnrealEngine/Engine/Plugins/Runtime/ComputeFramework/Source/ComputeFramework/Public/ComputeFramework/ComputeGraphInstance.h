// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComputeGraphInstance.generated.h"

class FSceneInterface;
class UComputeDataProvider;
class UComputeGraph;

/** 
 * Class to store a set of data provider bindings for UComputeGraph and to
 * enqueue work to the ComputeFramework's compute system.
 */
USTRUCT()
struct COMPUTEFRAMEWORK_API FComputeGraphInstance
{
	GENERATED_USTRUCT_BODY();

public:
	/** 
	 * Create the Data Provider objects for a single binding of a ComputeGraph. 
	 * The type of binding object is expected to match the associated Binding on the UComputeGraph.
	 */
	void CreateDataProviders(UComputeGraph* InComputeGraph, int32 InBindingIndex, TObjectPtr<UObject> InBindingObject);

	/** Create the Data Provider objects. */
	void DestroyDataProviders();

	/** Returns true if the Data Provider objects are all created and valid. */
	bool ValidateDataProviders(UComputeGraph* InComputeGraph) const;

	/** Get the Data Provider objects. */
	TArray< TObjectPtr<UComputeDataProvider> >& GetDataProviders() { return DataProviders; }

	/** Enqueue the ComputeGraph work. */
	bool EnqueueWork(UComputeGraph* InComputeGraph, FSceneInterface const* Scene, FName InOwnerName);

private:
	/** The currently bound Data Provider objects. */
	UPROPERTY(Transient)
	TArray< TObjectPtr<UComputeDataProvider> > DataProviders;
};
