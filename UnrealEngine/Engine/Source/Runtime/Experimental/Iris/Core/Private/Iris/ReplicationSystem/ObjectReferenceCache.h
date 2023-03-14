// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationSystem/NetHandle.h"
#include "UObject/WeakObjectPtr.h"
#include "Containers/Map.h"
#include "ObjectReferenceCacheFwd.h"

namespace UE::Net
{
	class FStringTokenStore;
	class FNetTokenStore;
	class FNetSerializationContext;
	namespace Private
	{
		class FNetHandleManager;
		class FNetExportContext;
	}
}

namespace UE::Net::Private
{

// A lot of code in this class is extracted from the re-factored GUIDCache/ClientPackageMap and must be kept up to sync once they are submitted
// Hopefully we can merge parts of this back together later on

class FObjectReferenceCache
{
public:
	FObjectReferenceCache();

	void Init(UReplicationSystem* ReplicationSystem);

	// Determine if the object is dynamic
	bool IsDynamicObject(const UObject* Object) const;

	// Are we allowed to create new NetHandles to reference objects?
	bool IsAuthority() const;

	// Create and assign a new NetHandle to the object
	FNetHandle CreateObjectReferenceHandle(const UObject* Object);

	// Get existing handle for object
	FNetHandle GetObjectReferenceHandleFromObject(const UObject* Object) const;

	// Get object from handle, only if the object is in the cache.
	UObject* GetObjectFromReferenceHandle(FNetHandle RefHandle);

	// Try to resolve the object reference and try to load it if the object cannot be found
	UObject* ResolveObjectReferenceHandle(FNetHandle RefHandle, const FNetObjectResolveContext& ResolveContext);

	UObject* ResolveObjectReference(const FNetObjectReference& ObjectRef, const FNetObjectResolveContext& ResolveContext);
	ENetObjectReferenceResolveResult ResolveObjectReference(const FNetObjectReference& ObjectRef, const FNetObjectResolveContext& ResolveContext, UObject*& OutResolvedObject);

	// Find replicated outer
	FNetObjectReference GetReplicatedOuter(const FNetObjectReference& Reference) const;

	// Get or create a NetObjectReference from the object
	FNetObjectReference GetOrCreateObjectReference(const UObject* Instance);

	// Get or create a NetObjectReference from the object identifed by path relative to outer
	FNetObjectReference GetOrCreateObjectReference(const FString& ObjectPath, const UObject* Outer);

	// Add reference for dynamically spawned object
	void AddRemoteReference(FNetHandle RefHandle, const UObject* Object);

	// Remove references to dynamic objects
	void RemoveReference(FNetHandle RefHandle, const UObject* Object);

	// Write full chain of object references for RefHandle
	void WriteFullReference(FNetSerializationContext& Context, FNetObjectReference Ref) const;

	// Read/load full reference data, this will populate the cache on the receiving end, but will not try to resolve the actual objects
	void ReadFullReference(FNetSerializationContext& Context, FNetObjectReference& OutRef);

	// Write reference, the reference must already be exported
	void WriteReference(FNetSerializationContext& Context, FNetObjectReference Ref) const;
	
	// Read reference, as written by WriteReference
	void ReadReference(FNetSerializationContext& Context, FNetObjectReference& OutRef);
 
	// Exports are expected to be part of the written state, so if WriteExports returns false or the BitStream is overflown
	// it us up to the caller to roll back written data and exports
	bool WriteExports(FNetSerializationContext& Context, TArrayView<const FNetObjectReference> ExportsView) const;

	bool ReadExports(FNetSerializationContext& Context);

	static FNetObjectReference MakeNetObjectReference(FNetHandle Handle) { return FNetObjectReference(Handle); }

private:

	struct FCachedNetObjectReference
	{
		TWeakObjectPtr<UObject> Object;
		const UObject* ObjectKey;

		// NetHandle
		FNetHandle NetHandle;

		// RelativePath to outer
		FNetToken RelativePath;

		// Ref to outer
		FNetHandle OuterNetHandle;

		// Flags
		uint8 bNoLoad : 1;				// Don't load this, only do a find
		uint8 bIgnoreWhenMissing : 1;
		uint8 bIsPackage : 1;
		uint8 bIsBroken : 1;
		uint8 bIsPending : 1;
	};

	bool CreateObjectReferenceInternal(const UObject* Object, FNetObjectReference& OutReference);

	void ConditionalWriteNetTokenData(FNetSerializationContext& Context, Private::FNetExportContext* ExportContext, const FNetToken& NetToken) const;
	void ConditionalReadNetTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) const;

	void ReadFullReferenceInternal(FNetSerializationContext& Context, FNetObjectReference& OutRef, uint32 RecursionCount);
	void WriteFullReferenceInternal(FNetSerializationContext& Context, const FNetObjectReference& Ref) const;
	UObject* ResolveObjectReferenceHandleInternal(FNetHandle RefHandle, const FNetObjectResolveContext& ResolveContext, bool& bOutMustBeMapped);
	bool IsDynamicInternal(const UObject* Object) const;
	bool SupportsObjectInternal(const UObject* Object) const;
	bool CanClientLoadObjectInternal(const UObject* Object, bool bIsDynamic) const;
	bool ShouldIgnoreWhenMissing(FNetHandle RefHandle) const;
	bool RenamePathForPie(uint32 ConnectionId, FString& Str, bool bReading);

	// Get the string path of RefHandle
	FString FullPath(FNetHandle RefHandle, const FNetObjectResolveContext& ResolveContext) const;
	void GenerateFullPath_r(FNetHandle RefHandle, const FNetObjectResolveContext& ResolveContext, FString& OutFullPath) const;

	// Find dynamic root
	FNetHandle GetDynamicRoot(const FNetHandle Handle) const;

	static FNetObjectReference MakeNetObjectReference(FNetHandle NetHandle, FNetToken RelativePath) { return FNetObjectReference(NetHandle, RelativePath, RelativePath.IsValid() ? ENetObjectReferenceTraits::CanBeExported : ENetObjectReferenceTraits::None); }
	static FNetObjectReference MakeNetObjectReference(const FCachedNetObjectReference& CachedReference) { return FNetObjectReference(CachedReference.NetHandle, FNetToken(), CachedReference.RelativePath.IsValid() ? ENetObjectReferenceTraits::CanBeExported : ENetObjectReferenceTraits::None); }

private:

	// Map raw UObject pointer -> Handle
	// To verify that the reference is valid we need to check the weakpointer stored in the cache
	TMap<const UObject*, FNetHandle> ObjectToNetReferenceHandle;

	// Map ReferenceHandle -> CachedReference
	TMap<FNetHandle, FCachedNetObjectReference> ReferenceHandleToCachedReference;

	// To properly clean up stale references referencing dynamic objects we need to track them
	TMultiMap<FNetHandle, FNetHandle> HandleToDynamicOuter;
	
	UReplicationSystem* ReplicationSystem;
	UObjectReplicationBridge* ReplicationBridge;
	FNetTokenStore* NetTokenStore;
	FStringTokenStore* StringTokenStore;
	FNetHandleManager* NetHandleManager;
	
	// Do we have authority to create references?
	uint32 bIsAuthority : 1;
};

inline UObject* FObjectReferenceCache::ResolveObjectReferenceHandle(FNetHandle RefHandle, const FNetObjectResolveContext& ResolveContext)
{
	bool bMustBeMapped;
	return ResolveObjectReferenceHandleInternal(RefHandle, ResolveContext, bMustBeMapped);
}

inline UObject* FObjectReferenceCache::ResolveObjectReference(const FNetObjectReference& ObjectRef, const FNetObjectResolveContext& ResolveContext)
{
	UObject* ResolvedObject = nullptr;
	ResolveObjectReference(ObjectRef, ResolveContext, ResolvedObject);
	return ResolvedObject;
}

}
