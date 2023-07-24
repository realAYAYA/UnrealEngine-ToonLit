// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassStateTreeTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "MassExternalSubsystemTraits.h"
#include "MassStateTreeSubsystem.generated.h"

class UStateTree;

USTRUCT()
struct MASSAIBEHAVIOR_API FMassStateTreeInstanceDataItem
{
	GENERATED_BODY()

	UPROPERTY()
	FStateTreeInstanceData InstanceData;

	UPROPERTY()
	int32 Generation = 0;
};

/**
* A subsystem managing StateTree assets in Mass
*/
UCLASS()
class MASSAIBEHAVIOR_API UMassStateTreeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
	
public:

	// USubsystem BEGIN
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// USubsystem END

	/**
	 * Allocates new instance data for specified StateTree.
	 * @param StateTree StateTree to allocated the data for.
	 * @return Handle to the data.
	 */
	FMassStateTreeInstanceHandle AllocateInstanceData(const UStateTree* StateTree);

	/**
	 * Frees instance data.
	 * @param Handle Instance data handle to free.
	 */
	void FreeInstanceData(const FMassStateTreeInstanceHandle Handle);

	/** @return Pointer to instance data held by the handle, or nullptr if handle is not valid. */
	FStateTreeInstanceData* GetInstanceData(const FMassStateTreeInstanceHandle Handle)
	{
		return IsValidHandle(Handle) ? &InstanceDataArray[Handle.GetIndex()].InstanceData : nullptr;
	}

	/** @return True if the handle points to active instance data. */
	bool IsValidHandle(const FMassStateTreeInstanceHandle Handle) const
	{
		return InstanceDataArray.IsValidIndex(Handle.GetIndex()) && InstanceDataArray[Handle.GetIndex()].Generation == Handle.GetGeneration();
	}
	
protected:

	TArray<int32> InstanceDataFreelist;

	UPROPERTY(Transient)
	TArray<FMassStateTreeInstanceDataItem> InstanceDataArray;
};

template<>
struct TMassExternalSubsystemTraits<UMassStateTreeSubsystem> final
{
	enum
	{
		GameThreadOnly = true
	};
};
