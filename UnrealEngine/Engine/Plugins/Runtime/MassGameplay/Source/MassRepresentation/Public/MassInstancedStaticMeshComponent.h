// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Components/InstancedStaticMeshComponent.h"
#include "MassInstancedStaticMeshComponent.generated.h"


struct FMassISMCSharedData;

UCLASS()
class MASSREPRESENTATION_API UMassInstancedStaticMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:
	UMassInstancedStaticMeshComponent();

	void ApplyVisualChanges(const FMassISMCSharedData& SharedData);
		
protected:
	/**
	 * Add multiple instances to this component. The newly added instances are associated with Ids as indicated by InstanceIds.
	 * @param bWorldSpace determines whether input transforms are in world-space (true) or local-space (false)
	 * @param OutAddedIndices if provided will be appended with indices of newly created instances
	 */
	void AddInstancesWithIds(TConstArrayView<int32> InstanceIds, TConstArrayView<FTransform> InstanceTransforms, int32 InNumCustomDataFloats
		, TConstArrayView<float> CustomFloatData, bool bWorldSpace = false, TArray<int32>* OutAddedIndices = nullptr);

	/** Remove the instances given as instance Ids. Returns True on success. */
	bool RemoveInstanceWithIds(TConstArrayView<int32> InstanceIds);

	bool RemoveInstanceInternal(const int32 InstanceIndex, const bool bInstanceAlreadyRemoved);
};
