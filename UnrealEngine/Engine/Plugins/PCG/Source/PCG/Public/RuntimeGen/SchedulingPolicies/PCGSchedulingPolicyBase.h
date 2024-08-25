// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSchedulingPolicyBase.generated.h"

class IPCGGenSourceBase;

/** 
 * Scheduling Policies provide custom logic to efficiently schedule work for the Runtime Generation Scheduling system.
 * A higher priority value means the work will be scheduled sooner, and larger grid sizes will always have a higher
 * priority than lower grid sizes.
 *
 * If multiple Generation Sources overlap a component, the highest priority value will be used for scheduling.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class PCG_API UPCGSchedulingPolicyBase : public UObject
{
	GENERATED_BODY()

public:
	/** Calculate the runtime scheduling priority with respect to a Generation Source. Should return a value in the range [0, 1], where higher values will be scheduled sooner. */
	virtual double CalculatePriority(const IPCGGenSourceBase* InGenSource, const FBox& GenerationBounds, bool bUse2DGrid) const 
		PURE_VIRTUAL(UPCGSchedulingPolicyBase::CalculatePriority, return 0.0;);

	/** A SchedulingPolicy is equivalent to another SchedulingPolicy if they are the same (same ptr), or if they have the same type and parameter values. */
	virtual bool IsEquivalent(const UPCGSchedulingPolicyBase* OtherSchedulingPolicy) const PURE_VIRTUAL(UPCGSchedulingPolicyBase::IsEquivalent, return false;);

#if WITH_EDITOR
	/** Sets whether or not properties should be displayed in the editor.Used to hide instanced SchedulingPolicy properties when runtime generation is not enabled. */
	void SetShouldDisplayProperties(bool bInShouldDisplayProperties) { bShouldDisplayProperties = bInShouldDisplayProperties; }
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
private:
	/** Hidden property to control display of SchedulingPolicy properties. */
	UPROPERTY(Transient, meta = (EditCondition = false, EditConditionHides))
	bool bShouldDisplayProperties = true;
#endif // WITH_EDITORONLY_DATA 
};
