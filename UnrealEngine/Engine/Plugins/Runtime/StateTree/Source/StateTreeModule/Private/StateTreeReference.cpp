// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeReference.h"
#include "StateTree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeReference)

void FStateTreeReference::SyncParametersToMatchStateTree(FInstancedPropertyBag& ParametersToSync) const
{
	if (StateTree == nullptr)
	{
		ParametersToSync.Reset();
	}
	else
	{
		ParametersToSync.MigrateToNewBagInstance(StateTree->GetDefaultParameters());
	}
}

bool FStateTreeReference::RequiresParametersSync() const
{
	return (StateTree == nullptr && Parameters.IsValid())
		|| (StateTree != nullptr && StateTree->GetDefaultParameters().GetPropertyBagStruct() != Parameters.GetPropertyBagStruct());
}

void FStateTreeReference::ConditionallySyncParameters() const
{
	if (RequiresParametersSync())
	{
		FStateTreeReference* NonConstThis = const_cast<FStateTreeReference*>(this);
		NonConstThis->SyncParameters();
		UE_LOG(LogStateTree, Warning, TEXT("Parameters for '%s' stored in StateTreeReference were auto-fixed to be usable at runtime."), *GetNameSafe(StateTree));	
	}
}

