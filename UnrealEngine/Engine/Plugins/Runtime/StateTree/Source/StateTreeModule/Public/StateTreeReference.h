// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeTypes.h"
#include "StateTreeReference.generated.h"

class UStateTree;

/**
 * Struct to hold reference to a StateTree asset along with values to parameterized it.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeReference
{
	GENERATED_BODY()

	/** @return true if the reference is set. */
	bool IsValid() const
	{
		return StateTree != nullptr;
	}
	
	/** Sets the StateTree asset and referenced parameters. */
	void SetStateTree(UStateTree* NewStateTree)
	{
		StateTree = NewStateTree;
		SyncParameters();
	}

	/** @return const pointer to the referenced StateTree asset. */
	const UStateTree* GetStateTree() const
	{
		return StateTree;
	}

	/** @return pointer to the referenced StateTree asset. */
	UStateTree* GetMutableStateTree()
	{
		return StateTree;
	}

	/** @return reference to the parameters for the referenced StateTree asset. */
	const FInstancedPropertyBag& GetParameters() const
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/** @return reference to the parameters for the referenced StateTree asset. */
	FInstancedPropertyBag& GetMutableParameters()
	{
		ConditionallySyncParameters();
		return Parameters;
	}

	/**
	 * Enforce self parameters to be compatible with those exposed by the selected StateTree asset.
	 */
	void SyncParameters() { SyncParametersToMatchStateTree(Parameters); }

	/**
	 * Sync provided parameters to be compatible with those exposed by the selected StateTree asset.
	 */
	void SyncParametersToMatchStateTree(FInstancedPropertyBag& ParametersToSync) const;

	/**
	 * Indicates if current parameters are compatible with those available in the selected StateTree asset.
	 * @return true when parameters requires to be synced to be compatible with those available in the selected StateTree asset, false otherwise.
	 */
	bool RequiresParametersSync() const;

	/** Sync parameters to match the asset if required. */
	void ConditionallySyncParameters() const;
	
protected:
	UPROPERTY(EditAnywhere, Category = "")
	TObjectPtr<UStateTree> StateTree = nullptr;

	UPROPERTY(EditAnywhere, Category = "", meta = (FixedLayout))
	FInstancedPropertyBag Parameters;
};
