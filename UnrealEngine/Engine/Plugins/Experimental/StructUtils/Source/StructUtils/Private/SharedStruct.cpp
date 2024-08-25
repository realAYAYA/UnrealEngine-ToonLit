// Copyright Epic Games, Inc. All Rights Reserved.
#include "SharedStruct.h"
#include "StructView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SharedStruct)

///////////////////////////////////////////////////////////////// FConstSharedStruct /////////////////////////////////////////////////////////////////

bool FConstSharedStruct::Identical(const FConstSharedStruct* Other, uint32 PortFlags) const
{
	// Only empty or strictly identical is considered equal
	return Other != nullptr
		&& GetMemory() == Other->GetMemory()
		&& GetScriptStruct() == Other->GetScriptStruct();
}

void FConstSharedStruct::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	if (auto* Struct = GetScriptStructPtr(); Struct && *Struct)
	{
		Collector.AddReferencedObjects(*Struct, const_cast<uint8*>(GetMemory()));
	}
}

///////////////////////////////////////////////////////////////// FSharedStruct /////////////////////////////////////////////////////////////////

FSharedStruct::FSharedStruct(const FConstStructView& InOther)
{
	InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
}

FSharedStruct& FSharedStruct::operator=(const FConstStructView& InOther)
{
	if (*this != InOther)
	{
		InitializeAs(InOther.GetScriptStruct(), InOther.GetMemory());
	}
	return *this;
}

bool FSharedStruct::Identical(const FSharedStruct* Other, uint32 PortFlags) const
{
	// Only empty or strictly identical is considered equal
	return Other != nullptr 
		&& GetMemory() == Other->GetMemory() 
		&& GetScriptStruct() == Other->GetScriptStruct();
}

void FSharedStruct::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	if (auto* Struct = GetScriptStructPtr(); Struct && *Struct)
	{
		Collector.AddReferencedObjects(*Struct, const_cast<uint8*>(GetMemory()));
	}
}
