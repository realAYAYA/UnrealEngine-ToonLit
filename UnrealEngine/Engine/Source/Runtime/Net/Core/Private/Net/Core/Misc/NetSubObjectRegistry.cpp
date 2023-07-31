// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Core/Misc/NetSubObjectRegistry.h"

namespace UE::Net 
{

// ----------------------------------------------------------------
//	FSubObjectRegistry
// ----------------------------------------------------------------
FSubObjectRegistry::EResult FSubObjectRegistry::AddSubObjectUnique(UObject* InSubObject, ELifetimeCondition InNetCondition)
{
	check(InSubObject);

	const FEntry SubObjectEntry(InSubObject, InNetCondition);

	const int32 CurrentIndex = Registry.Find(SubObjectEntry);
	if (CurrentIndex == INDEX_NONE)
	{
		Registry.Add(SubObjectEntry);
		return EResult::NewEntry;
	}
	else
	{
		// Check if the net condition is different.
		const FEntry& ActiveEntry = Registry[CurrentIndex];
		return (ActiveEntry.NetCondition == InNetCondition) ? EResult::AlreadyRegistered : EResult::NetConditionConflict;
	}
}

ELifetimeCondition FSubObjectRegistry::GetNetCondition(UObject* InSubObject) const
{
	const int32 CurrentIndex = Registry.IndexOfByKey(InSubObject);
	return CurrentIndex == INDEX_NONE ? COND_Max : Registry[CurrentIndex].NetCondition;
}

bool FSubObjectRegistry::RemoveSubObject(UObject* InSubObject)
{
	int32 Index = Registry.IndexOfByKey(InSubObject);
	if (Index != INDEX_NONE)
	{
		Registry.RemoveAt(Index);
		return true;
	}
	return false;
}

bool FSubObjectRegistry::IsSubObjectInRegistry(const UObject* SubObject) const
{
	return Registry.FindByKey(SubObject) != nullptr;
}

} //namespace UE::Net