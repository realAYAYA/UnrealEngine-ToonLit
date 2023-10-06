// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StridedView.h"
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
	UE_DEPRECATED(5.2, "Implement any validation in FComputeDataProviderRenderProxy::IsValid().")
	virtual bool IsValid() const { return true; }
	
	/**
	 * Get an associated render thread proxy object.
	 * Currently these are created and destroyed per frame by the FComputeGraphInstance.
	 */
	virtual FComputeDataProviderRenderProxy* GetRenderProxy() { return nullptr; }
};

/**
 * Compute Framework Data Provider Proxy. 
 * A concrete instance of this is created by the UComputeDataProvider gathering of data for a Compute Kernel on the render thread. 
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

	/** Data needed for validation. */
	struct FValidationData
	{
		int32 NumInvocations;
		int32 ParameterStructSize;
	};

	/**
	 * Validates that we are OK to dispatch work.
	 * Default implementation returns false.
	 */
	virtual bool IsValid(FValidationData const& InValidationData) const { return false; }

	/** Data needed for setting permuations. */
	struct FPermutationData
	{
		const int32 NumInvocations;
		const FComputeKernelPermutationVector& PermutationVector;
		TArray<int32> PermutationIds;
	};

	/** 
	 * Gathers permutation bits for each invocation.
	 * This is called before any calls to AllocateResources() because we validate all requested shaders before doing any further work.
	 */
	virtual void GatherPermutations(FPermutationData& InOutPermutationData) const {}

	/** Setup needed to allocate resources. */
	struct FAllocationData
	{
		int32 NumGraphKernels;
	};

	// todo: Fix up derived classes to implement new AllocateResources().
	UE_DEPRECATED(5.3, "Convert to using the new AllocateResources() that takes FAllocationData.")
	virtual void AllocateResources(FRDGBuilder& GraphBuilder) {}

	/* Called once before any calls to GatherDispatchData() to allow any RDG resource allocation. */
	virtual void AllocateResources(FRDGBuilder& GraphBuilder, FAllocationData const& InAllocationData)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AllocateResources(GraphBuilder);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	/** Setup needed to gather dispatch data. */
	struct FDispatchData
	{
		int32 GraphKernelIndex;
		int32 NumInvocations;
		bool bUnifiedDispatch;
		int32 ParameterStructSize;
		int32 ParameterBufferOffset;
		int32 ParameterBufferStride;
		uint8* ParameterBuffer;
	};

	/** Collect parameter data required to dispatch work. */
	virtual void GatherDispatchData(FDispatchData const& InDispatchData) {}

protected:
	/** Helper for making an FStridedView over the FDispatchData. */
	template <typename ElementType>
	TStridedView<ElementType> MakeStridedParameterView(FDispatchData const& InDispatchData)
	{
		return TStridedView<ElementType>(InDispatchData.ParameterBufferStride, (ElementType*)(InDispatchData.ParameterBuffer + InDispatchData.ParameterBufferOffset), InDispatchData.NumInvocations);
	}

protected:
	//~ Start deprecation of old GatherDispatchData() interface.
	struct FDispatchSetup
	{
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		int32 NumInvocations;
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		int32 ParameterBufferOffset;
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		int32 ParameterBufferStride;
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		int32 ParameterStructSizeForValidation;
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		FComputeKernelPermutationVector const& PermutationVector;
	};

	struct FCollectedDispatchData
	{
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		uint8* ParameterBuffer;
		UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
		TArray<int32> PermutationId;
	};

	UE_DEPRECATED(5.2, "Convert to using the new GatherDispatchData() that takes FDispatchData.")
	virtual void GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData) {}
	//~ End deprecation of old GatherDispatchData() interface.
};
