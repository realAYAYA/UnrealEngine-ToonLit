// Copyright Epic Games, Inc. All Rights Reserved.
#include "PhysicsControlNameRecords.h"

//======================================================================================================================
void FPhysicsControlNameRecords::AddControl(FName Name, FName SetName)
{
	ControlSets.FindOrAdd(SetName).Add(Name);
	ControlSets.FindOrAdd("All").Add(Name);
}

//======================================================================================================================
void FPhysicsControlNameRecords::RemoveControl(FName Name)
{
	for (TPair<FName, TArray<FName>>& Set : ControlSets)
	{
		Set.Value.Remove(Name);
	}
}

//======================================================================================================================
const TArray<FName>& FPhysicsControlNameRecords::GetControlNamesInSet(FName SetName) const
{
	const TArray<FName>* Names = ControlSets.Find(SetName);
	if (Names)
	{
		return *Names;
	}
	static TArray<FName> FailureResult;
	return FailureResult;
}

//======================================================================================================================
void FPhysicsControlNameRecords::AddBodyModifier(FName Name, FName Set)
{
	BodyModifierSets.FindOrAdd(Set).Add(Name);	
	BodyModifierSets.FindOrAdd("All").Add(Name);
}

//======================================================================================================================
void FPhysicsControlNameRecords::RemoveBodyModifier(FName Name)
{
	for (TPair<FName, TArray<FName>>& Set : BodyModifierSets)
	{
		Set.Value.Remove(Name);
	}
}

//======================================================================================================================
const TArray<FName>& FPhysicsControlNameRecords::GetBodyModifierNamesInSet(FName Set) const
{
	const TArray<FName>* Names = BodyModifierSets.Find(Set);
	if (Names)
	{
		return *Names;
	}
	static TArray<FName> FailureResult;
	return FailureResult;
}
