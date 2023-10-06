// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "UObject/CoreNet.h"
#include "Delegates/Delegate.h"

class FArchive;
class UObject;

/**
 * This class is used to store meta data about properties that is shared between connections,
 * including whether or not a given property is Conditional, Active, and any external data
 * that may be needed for Replays.
 *
 * TODO: This class (and arguably IRepChangedPropertyTracker) should be renamed to reflect
 *			what they actually do now.
 */
class FRepChangedPropertyTracker : public IRepChangedPropertyTracker
{
public:
	FRepChangedPropertyTracker() = delete;
	FRepChangedPropertyTracker(FCustomPropertyConditionState&& InActiveState);

	UE_DEPRECATED(5.1, "Replay arguments no longer used.")
	FRepChangedPropertyTracker(const bool InbIsReplay, const bool InbIsClientReplayRecording);

	virtual ~FRepChangedPropertyTracker() = default;

	//~ Begin IRepChangedPropertyTracker Interface.
	/**
		* Manually set whether or not Property should be marked inactive.
		* This will change the Active status for all connections.
		*
		* @see DOREPLIFETIME_ACTIVE_OVERRIDE
		*
		* @param OwningObject	The object that we're tracking.
		* @param RepIndex		Replication index for the Property.
		* @param bIsActive		The new Active state.
		*/
	UE_DEPRECATED(5.2, "Please use FNetPropertyConditionManager::SetPropertyActiveOverride instead.")
	virtual void SetCustomIsActiveOverride(UObject* OwningObject, const uint16 RepIndex, const bool bIsActive) override;

	virtual void CountBytes(FArchive& Ar) const override;
	//~ End IRepChangedPropertyTracker Interface

	UE_DEPRECATED(5.1, "No longer used, ActiveState must be constructed with the correct number of properties.")
	void InitActiveParents(int32 ParentCount) {}

	bool IsParentActive(uint16 ParentIndex) const
	{
		return ActiveState.GetActiveState(ParentIndex);
	}

	int32 GetParentCount() const
	{
		return ActiveState.GetNumProperties();
	}

	ELifetimeCondition GetDynamicCondition(uint16 ParentIndex) const
	{
		return ActiveState.GetDynamicCondition(ParentIndex);
	}

	uint32 GetDynamicConditionChangeCounter() const
	{
		return ActiveState.GetDynamicConditionChangeCounter();
	}

private:
	friend UE::Net::Private::FNetPropertyConditionManager;

	// Called from FNetPropertyConditionManager 
	void CallSetDynamicCondition(const UObject* OwningObject, const uint16 RepIndex, const ELifetimeCondition Condition);

	/** Activation data for top level Properties on the given Actor / Object. */
	FCustomPropertyConditionState ActiveState;
};
