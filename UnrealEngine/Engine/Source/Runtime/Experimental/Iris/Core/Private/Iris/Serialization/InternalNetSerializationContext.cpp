// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "HAL/MemoryBase.h"

namespace UE::Net::Private
{

FInternalNetSerializationContext::FInternalNetSerializationContext(UReplicationSystem* InReplicationSystem)
: ReplicationSystem(InReplicationSystem)
, ObjectReferenceCache(&InReplicationSystem->GetReplicationSystemInternal()->GetObjectReferenceCache())
, PackageMap(InReplicationSystem->GetReplicationSystemInternal()->GetIrisObjectReferencePackageMap())
, bDowngradeAutonomousProxyRole(0)
, bInlineObjectReferenceExports(0)
{
}

void* FInternalNetSerializationContext::Alloc(SIZE_T Size, SIZE_T Alignment)
{
	return GMalloc->Malloc(Size, static_cast<uint32>(Alignment));
}

void FInternalNetSerializationContext::Free(void* Ptr)
{
	return GMalloc->Free(Ptr);
}

void* FInternalNetSerializationContext::Realloc(void* PrevAddress, SIZE_T NewSize, uint32 Alignment)
{
	return GMalloc->Realloc(PrevAddress, NewSize, Alignment);
}

void FInternalNetSerializationContext::Init(const FInitParameters& InitParams)
{
	ReplicationSystem = InitParams.ReplicationSystem;
	ObjectReferenceCache = &InitParams.ReplicationSystem->GetReplicationSystemInternal()->GetObjectReferenceCache();
	PackageMap = InitParams.PackageMap;
	ResolveContext = InitParams.ObjectResolveContext;
}

}
