// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"

namespace UE::Net::AllocationPolicies
{

void* FElementAllocationPolicy::Realloc(FNetSerializationContext& Context, void* Original, SIZE_T Size, uint32 Alignment)
{
	return Context.GetInternalContext()->Realloc(Original, Size, Alignment);
}

void FElementAllocationPolicy::Free(FNetSerializationContext& Context, void* Ptr)
{
	return Context.GetInternalContext()->Free(Ptr);
}

}

