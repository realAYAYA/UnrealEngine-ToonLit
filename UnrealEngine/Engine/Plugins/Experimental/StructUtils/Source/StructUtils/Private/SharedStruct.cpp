// Copyright Epic Games, Inc. All Rights Reserved.
#include "SharedStruct.h"
#include "InstancedStruct.h"
#include "StructView.h"
#include "StructUtilsTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SharedStruct)

///////////////////////////////////////////////////////////////// FConstSharedStruct /////////////////////////////////////////////////////////////////

bool FConstSharedStruct::Identical(const FConstSharedStruct* Other, uint32 PortFlags) const
{
	// Only empty is considered equal
	return Other != nullptr && GetMemory() == nullptr && Other->GetMemory() == nullptr && GetScriptStruct() == nullptr && Other->GetScriptStruct() == nullptr;
}

void FConstSharedStruct::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	if (const UScriptStruct* Struct = GetScriptStruct())
	{
		Collector.AddReferencedObjects(Struct, const_cast<uint8*>(GetMemory()));
	}
}

///////////////////////////////////////////////////////////////// FSharedStruct /////////////////////////////////////////////////////////////////

FSharedStruct::FSharedStruct(const FConstStructView InOther)
{
	InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
}

FSharedStruct& FSharedStruct::operator=(const FConstStructView InOther)
{
	if (*this != InOther)
	{
		InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
	}
	return *this;
}
