// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComputeDataProvider.generated.h"

class FComputeDataProviderRenderProxy;
struct FComputeKernelPermutationVector;
class FRDGBuilder;

/**
 * Compute Framework Data Provider.
 * A concrete instance of this is responsible for supplying data declared by a UComputeDataInterface.
 * One of these must be created for each UComputeDataInterface object in an instance of a Compute Graph.
 */
UCLASS(Abstract)
class COMPUTEFRAMEWORK_API UComputeDataProvider : public UObject
{
	GENERATED_BODY()

public:
	/** Return false if the provider has not been fully initialized. */
	virtual bool IsValid() const { return true; }
	
	/**
	 * Get an associated render thread proxy object.
	 * Currently these are created and destroyed per frame by the FComputeGraphInstance.
	 * todo[CF]: Don't destroy FComputeDataProviderRenderProxy objects every frame?
	 */
	virtual FComputeDataProviderRenderProxy* GetRenderProxy() { return nullptr; }
};

/**
 * Compute Framework Data Provider Proxy. 
 * A concrete instance of this is created by the UComputeDataProvider and used for the render thread gathering of data for a Compute Kernel. 
 */
class COMPUTEFRAMEWORK_API FComputeDataProviderRenderProxy
{
public:
	virtual ~FComputeDataProviderRenderProxy() {}

	/** 
	 * Called on render thread to determine invocation count and dispatch thread counts per invocation. 
	 * This will only be called if the associated UComputeDataInterface returned true for IsExecutionInterface().
	*/
	virtual int32 GetDispatchThreadCount(TArray<FIntVector>& ThreadCounts) const { return 0; };

	/* Called once before any calls to GatherDispatchData() to allow any RDG resource allocation. */
	virtual void AllocateResources(FRDGBuilder& GraphBuilder) {}

	/** */
	struct FDispatchSetup
	{
		int32 NumInvocations;
		int32 ParameterBufferOffset;
		int32 ParameterBufferStride;
		int32 ParameterStructSizeForValidation;
		FComputeKernelPermutationVector const& PermutationVector;
	};

	/** Collected data during kernel dispatch. */
	struct FCollectedDispatchData
	{
		uint8* ParameterBuffer;
		TArray<int32> PermutationId;
	};

	virtual void GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData) {}
};
