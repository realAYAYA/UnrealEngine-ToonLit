// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"

namespace UE::Net::Private
{

/** Construct the external state described by the descriptor in the given Buffer */
IRISCORE_API void ConstructPropertyReplicationState(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor);

/** Destruct the external state described by the descriptor in the given Buffer */
IRISCORE_API void DestructPropertyReplicationState(uint8* StateBuffer, const FReplicationStateDescriptor* Descriptor);

/** Copy external state in SrcStateBuffer to external state DstStateBuffer including changemask */
IRISCORE_API void CopyPropertyReplicationState(uint8* RESTRICT DstStateBuffer, uint8* RESTRICT SrcStateBuffer, const FReplicationStateDescriptor* Descriptor);


/** Copy property value, if the property is fully replicated we use the properties copy function, otherwise we do a per member copy of all replicated data */
IRISCORE_API void InternalCopyPropertyValue(const FReplicationStateDescriptor* Descriptor, uint32 MemberIndex, void* RESTRICT Dst, const void* RESTRICT Src);

/** Copy struct members using InternalCopyPropertyValue */
IRISCORE_API void InternalCopyStructProperty(const FReplicationStateDescriptor* StructDescriptor, void* RESTRICT Dst, const void* RESTRICT Src);

/** Compare struct members using InternalCompareMember, this function will only compare replicated members of the struct */
IRISCORE_API bool InternalCompareStructProperty(const FReplicationStateDescriptor* StructDescriptor, void* RESTRICT Dst, const void* RESTRICT Src);

/** CompareMembers, if data is not fully replicated we will use per member compare for structs and arrays */
IRISCORE_API bool InternalCompareMember(const FReplicationStateDescriptor* Descriptor, uint32 MemberIndex, const void* RESTRICT ValueA, const void* RESTRICT ValueB);

}
