// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/PolymorphicNetSerializer.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "Iris/Core/IrisProfiler.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "UObject/UObjectIterator.h"

namespace UE::Net
{

const FName NetError_PolymorphicStructNetSerializer_InvalidStructType(TEXT("Invalid struct type"));

void FPolymorphicNetSerializerScriptStructCache::InitForType(const UScriptStruct* InScriptStruct)
{
	IRIS_PROFILER_SCOPE(FPolymorphicNetSerializerScriptStructCache_InitForType);

	TArray<FTypeInfo> UpdatedRegisteredTypes;
	UpdatedRegisteredTypes.Reserve(RegisteredTypes.Num());

	CommonTraits = EReplicationStateTraits::None;

	bool bFoundNewScriptStructs = false;

	// Find all script structs of this type and add them to the list and build descriptor
	// (not sure of a better way to do this but it should only happen at startup)
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		if (It->IsChildOf(InScriptStruct))
		{
			FTypeInfo Entry;
			Entry.ScriptStruct = *It;

			// See if we already had a descriptor for the struct
			const uint32 ExistingTypeIndex = GetTypeIndex(Entry.ScriptStruct);
			if (ExistingTypeIndex != InvalidTypeIndex)
			{
				Entry.Descriptor = GetTypeInfo(ExistingTypeIndex)->Descriptor;
			}
			else
			{
				// Get or create descriptor
				FReplicationStateDescriptorBuilder::FParameters Params;
				Entry.Descriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(*It, Params);
				bFoundNewScriptStructs = true;
			}
			
			if (Entry.Descriptor.IsValid())
			{
				UpdatedRegisteredTypes.Add(Entry);

				CommonTraits |= (Entry.Descriptor->Traits & EReplicationStateTraits::HasObjectReference);
			}
			else
			{
				UE_LOG(LogIris, Error, TEXT("FPolymorphicNetSerializerScriptStructCache::InitForType Failed to create descriptor for type %s when building cache for base %s"), ToCStr((*It)->GetName()), ToCStr(InScriptStruct->GetName()));
			}
		}
	}

	UpdatedRegisteredTypes.Sort([](const FTypeInfo& A, const FTypeInfo& B) { return A.ScriptStruct->GetName().ToLower() > B.ScriptStruct->GetName().ToLower(); });

	if ((uint32)UpdatedRegisteredTypes.Num() > MaxRegisteredTypeCount)
	{
		UE_LOG(LogIris, Error, TEXT("FPolymorphicNetSerializerScriptStructCache::InitForType Too many types (%u of %u) of base %s"), UpdatedRegisteredTypes.Num(), MaxRegisteredTypeCount,  ToCStr(InScriptStruct->GetName()));
		check((uint32)RegisteredTypes.Num() <= MaxRegisteredTypeCount);
	}

	// Log if we updated the types
	const bool bWasUpdated = (UpdatedRegisteredTypes.Num() != RegisteredTypes.Num()) || bFoundNewScriptStructs;
	UE_CLOG(bWasUpdated, LogIris, Log,TEXT("FPolymorphicNetSerializerScriptStructCache::InitForType Updated CachedTypedIds for base %s, NumCachedTypeIDs: %d"), ToCStr(InScriptStruct->GetName()), UpdatedRegisteredTypes.Num());

	RegisteredTypes = MoveTemp(UpdatedRegisteredTypes);
}

}

namespace UE::Net::Private
{

void* FPolymorphicStructNetSerializerInternal::Alloc(FNetSerializationContext& Context, SIZE_T Size, SIZE_T Alignment)
{
	return Context.GetInternalContext()->Alloc(Size, Alignment);
}

void FPolymorphicStructNetSerializerInternal::Free(FNetSerializationContext& Context, void* Ptr)
{
	return Context.GetInternalContext()->Free(Ptr);
}

void FPolymorphicStructNetSerializerInternal::CollectReferences(FNetSerializationContext& Context, FNetReferenceCollector& Collector, const FNetSerializerChangeMaskParam& OuterChangeMaskInfo, const uint8* RESTRICT SrcInternalBuffer,  const FReplicationStateDescriptor* Descriptor)
{
	return FReplicationStateOperationsInternal::CollectReferences(Context, Collector, OuterChangeMaskInfo, SrcInternalBuffer, Descriptor);
}

void FPolymorphicStructNetSerializerInternal::CloneQuantizedState(FNetSerializationContext& Context, uint8* RESTRICT DstInternalBuffer, const uint8* RESTRICT SrcInternalBuffer, const FReplicationStateDescriptor* Descriptor)
{
	return FReplicationStateOperationsInternal::CloneQuantizedState(Context, DstInternalBuffer, SrcInternalBuffer, Descriptor);
}

}
