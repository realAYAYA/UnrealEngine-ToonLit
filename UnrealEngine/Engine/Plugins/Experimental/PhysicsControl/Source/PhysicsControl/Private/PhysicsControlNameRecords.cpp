// Copyright Epic Games, Inc. All Rights Reserved.
#include "PhysicsControlNameRecords.h"

//======================================================================================================================
void AddUniqueNamesToSet(const TArray<FName>& Names, const FName SetName, TMap<FName, TArray<FName>>& SetCollection)
{
	TArray<FName>& SetElementNames = SetCollection.FindOrAdd(SetName);

	for (const FName Name : Names)
	{
		SetElementNames.AddUnique(Name);
	}
}

//======================================================================================================================
void FPhysicsControlNameRecords::AddControl(FName Name, FName SetName)
{
	ControlSets.FindOrAdd(SetName).AddUnique(Name);
	ControlSets.FindOrAdd("All").AddUnique(Name);
}

//======================================================================================================================
void FPhysicsControlNameRecords::AddControl(FName Name, const TArray<FName>& SetNames)
{
	for (const FName SetName : SetNames)
	{
		ControlSets.FindOrAdd(SetName).AddUnique(Name);
	}
	ControlSets.FindOrAdd("All").AddUnique(Name);
}


//======================================================================================================================
void FPhysicsControlNameRecords::AddControls(const TArray<FName>& ControlNames, const FName SetName)
{
	AddUniqueNamesToSet(ControlNames, SetName, ControlSets);
	AddUniqueNamesToSet(ControlNames, "All", ControlSets);
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
void FPhysicsControlNameRecords::AddBodyModifier(FName Name, FName SetName)
{
	BodyModifierSets.FindOrAdd(SetName).AddUnique(Name);
	BodyModifierSets.FindOrAdd("All").AddUnique(Name);
}

//======================================================================================================================
void FPhysicsControlNameRecords::AddBodyModifier(FName Name, const TArray<FName>& SetNames)
{
	for (FName SetName : SetNames)
	{
		BodyModifierSets.FindOrAdd(SetName).AddUnique(Name);
	}
	BodyModifierSets.FindOrAdd("All").AddUnique(Name);
}

//======================================================================================================================
void FPhysicsControlNameRecords::AddBodyModifiers(const TArray<FName>& BodyModifierNames, const FName SetName)
{
	AddUniqueNamesToSet(BodyModifierNames, SetName, BodyModifierSets);
	AddUniqueNamesToSet(BodyModifierNames, "All", BodyModifierSets);
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
const TArray<FName>& FPhysicsControlNameRecords::GetBodyModifierNamesInSet(FName SetName) const
{
	const TArray<FName>* Names = BodyModifierSets.Find(SetName);
	if (Names)
	{
		return *Names;
	}
	static TArray<FName> FailureResult;
	return FailureResult;
}

//======================================================================================================================
void FPhysicsControlNameRecords::Reset()
{
	ControlSets.Reset();
	BodyModifierSets.Reset();
}

//======================================================================================================================
TArray<FName> ExpandName(const FName InName, const TMap<FName, TArray<FName>>& SetNames)
{
	TArray<FName> OutputNames;
	if (const TArray<FName>* const FoundSet = SetNames.Find(InName))
	{
		OutputNames.Append(*FoundSet);
	}
	else
	{
		OutputNames.Add(InName);
	}
	return OutputNames;
}

//======================================================================================================================
TArray<FName> ExpandName(const TArray<FName>& InNames, const TMap<FName, TArray<FName>>& SetNames)
{
	TArray<FName> OutputNames;

	for (const FName Name : InNames)
	{
		OutputNames.Append(ExpandName(Name, SetNames));
	}

	return OutputNames;
}
