// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeInstanceData.h"
#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTreeInstanceData)

//----------------------------------------------------------------//
//  FStateTreeInstanceData
//----------------------------------------------------------------//

int32 FStateTreeInstanceData::GetEstimatedMemoryUsage() const
{
	int32 Size = sizeof(FStateTreeInstanceData);

	Size += InstanceStructs.GetAllocatedMemory();

	for (const UObject* InstanceObject : InstanceObjects)
	{
		if (InstanceObject)
		{
			Size += InstanceObject->GetClass()->GetStructureSize();
		}
	}

	return Size;
}

int32 FStateTreeInstanceData::GetNumItems() const
{
	return InstanceStructs.Num() + InstanceObjects.Num();
}

bool FStateTreeInstanceData::Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const
{
	if (Other == nullptr)
	{
		return false;
	}

	// Identical if both are uninitialized.
	if (!IsValid() && !Other->IsValid())
	{
		return true;
	}

	// Not identical if one is valid and other is not.
	if (IsValid() != Other->IsValid())
	{
		return false;
	}

	// Not identical if different amount of instanced objects.
	if (InstanceObjects.Num() != Other->InstanceObjects.Num())
	{
		return false;
	}

	// Not identical if structs are different.
	if (InstanceStructs.Identical(&Other->InstanceStructs, PortFlags) == false)
	{
		return false;
	}
	
	// Check that the instance object contents are identical.
	// Copied from object property.
	auto AreObjectsIndentical = [](UObject* A, UObject* B, uint32 PortFlags) -> bool
	{
		if ((PortFlags & PPF_DuplicateForPIE) != 0)
		{
			return false;
		}

		if (A == B)
		{
			return true;
		}

		// Resolve the object handles and run the deep comparison logic 
		if ((PortFlags & (PPF_DeepCompareInstances | PPF_DeepComparison)) != 0)
		{
			return FObjectPropertyBase::StaticIdentical(A, B, PortFlags);
		}

		return true;
	};

	bool bResult = true;
	for (int32 Index = 0; Index < InstanceObjects.Num(); Index++)
	{
		if (InstanceObjects[Index] != nullptr && Other->InstanceObjects[Index] != nullptr)
		{
			if (!AreObjectsIndentical(InstanceObjects[Index], Other->InstanceObjects[Index], PortFlags))
			{
				bResult = false;
				break;
			}
		}
		else
		{
			bResult = false;
			break;
		}
	}
	
	return bResult;
}

void FStateTreeInstanceData::CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther)
{
	if (&InOther == this)
	{
		return;
	}

	// Copy structs
	InstanceStructs = InOther.InstanceStructs;

	// Copy instance objects.
	InstanceObjects.Reset();
	for (const UObject* Instance : InOther.InstanceObjects)
	{
		if (ensure(Instance != nullptr))
		{
			ensure(Instance->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) == false);
			InstanceObjects.Add(DuplicateObject(Instance, &InOwner));
		}
	}
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<UObject*> InObjects)
{
	Reset();
	Append(InOwner, InStructs, InObjects);
}

void FStateTreeInstanceData::Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<UObject*> InObjects)
{
	Reset();
	Append(InOwner, InStructs, InObjects);
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<UObject*> InObjects)
{
	InstanceStructs.Append(InStructs);
	
	InstanceObjects.Reserve(InstanceObjects.Num() + InObjects.Num());
	for (const UObject* Instance : InObjects)
	{
		if (ensure(Instance != nullptr))
		{
			ensure(Instance->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) == false);
			InstanceObjects.Add(DuplicateObject(Instance, &InOwner));
		}
	}
}

void FStateTreeInstanceData::Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<UObject*> InObjects)
{
	InstanceStructs.Append(InStructs);
	
	InstanceObjects.Reserve(InstanceObjects.Num() + InObjects.Num());
	for (const UObject* Instance : InObjects)
	{
		if (ensure(Instance != nullptr))
		{
			ensure(Instance->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists) == false);
			InstanceObjects.Add(DuplicateObject(Instance, &InOwner));
		}
	}
}

void FStateTreeInstanceData::Prune(const int32 NumStructs, const int32 NumObjects)
{
	check(NumStructs <= InstanceStructs.Num() && NumObjects <= InstanceObjects.Num());  
	InstanceStructs.SetNum(NumStructs);
	InstanceObjects.SetNum(NumObjects);
}

void FStateTreeInstanceData::Reset()
{
	InstanceStructs.Reset();
	InstanceObjects.Reset();
}

