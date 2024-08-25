// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationSystem/NetRefHandle.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "Containers/Map.h"
#include "ObjectReferenceCacheFwd.h"
#include "UObject/ObjectPtr.h"

namespace UE::Net
{
	class FStringTokenStore;
	class FNetTokenStore;
	class FNetSerializationContext;
	namespace Private
	{
		class FNetRefHandleManager;
		class FNetExportContext;
		struct FPendingBatches;
		typedef uint32 FInternalNetRefIndex;
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
	FNetRefHandle CreateObjectReferenceHandle(const UObject* Object);

	// Get existing handle for object
	FNetRefHandle GetObjectReferenceHandleFromObject(const UObject* Object, EGetRefHandleFlags GetRefHandleFlags = EGetRefHandleFlags::None) const;

	// Get object from handle, only if the object is in the cache.
	UObject* GetObjectFromReferenceHandle(FNetRefHandle RefHandle);

	// Try to resolve the object reference and try to load it if the object cannot be found
	UObject* ResolveObjectReferenceHandle(FNetRefHandle RefHandle, const FNetObjectResolveContext& ResolveContext);

	UObject* ResolveObjectReference(const FNetObjectReference& ObjectRef, const FNetObjectResolveContext& ResolveContext);
	ENetObjectReferenceResolveResult ResolveObjectReference(const FNetObjectReference& ObjectRef, const FNetObjectResolveContext& ResolveContext, UObject*& OutResolvedObject);

	// Returns true of this NetRefHandle is marked as broken
	bool IsNetRefHandleBroken(FNetRefHandle Handle, bool bMustBeRegistered) const;

	// Returns true of the provided NetRefHandle or one of its outers is pending async loading.
	bool IsNetRefHandlePending(FNetRefHandle NetRefHandle, const FPendingBatches& PendingBatches) const;

	// Find replicated outer
	FNetObjectReference GetReplicatedOuter(const FNetObjectReference& Reference) const;

	// Get or create a NetObjectReference from the object
	FNetObjectReference GetOrCreateObjectReference(const UObject* Instance);

	// Get or create a NetObjectReference from the object identifed by path relative to outer
	FNetObjectReference GetOrCreateObjectReference(const FString& ObjectPath, const UObject* Outer);

	// Add reference for dynamically spawned object
	void AddRemoteReference(FNetRefHandle RefHandle, const UObject* Object);

	// Remove references to dynamic objects
	void RemoveReference(FNetRefHandle RefHandle, const UObject* Object);

	// Write full chain of object references for RefHandle
	void WriteFullReference(FNetSerializationContext& Context, FNetObjectReference Ref) const;

	// Read/load full reference data, this will populate the cache on the receiving end, but will not try to resolve the actual objects
	void ReadFullReference(FNetSerializationContext& Context, FNetObjectReference& OutRef);

	// Write reference, the reference must already be exported
	void WriteReference(FNetSerializationContext& Context, FNetObjectReference Ref) const;
	
	// Read reference, as written by WriteReference
	void ReadReference(FNetSerializationContext& Context, FNetObjectReference& OutRef);

	// Add exports to the set of pending exports for the current batch being written
	void AddPendingExports(FNetSerializationContext& Context, TArrayView<const FNetObjectReference> ExportsView) const;

	// Add export to the set of pending exports for the current batch being written
	void AddPendingExport(FNetExportContext& ExportContext, const FNetObjectReference& Reference) const;

	enum class EWriteExportsResult : unsigned
	{
		// We did write exports
		WroteExports,

		// BitStream overflow.
		BitStreamOverflow,

		// Some error occurred while serializing the object.
		NoExports,
	};

	// Exports are expected to be part of the written state, so if the result is a BitStreamOverflow
	// it is up to the caller to roll back written data and pending exports
	EWriteExportsResult WritePendingExports(FNetSerializationContext& Context, FInternalNetRefIndex ObjectIndex);

	bool ReadExports(FNetSerializationContext& Context, TArray<FNetRefHandle>* MustBeMappedExports);

	static FNetObjectReference MakeNetObjectReference(FNetRefHandle Handle);

	// Async interface, kept as close to possible to FNetGuidCache/PackageMapClient
	enum class EAsyncLoadMode : uint8
	{
		UseCVar			= 0,		// Use CVar (net.AllowAsyncLoading) to determine if we should async load
		ForceDisable	= 1,		// Disable async loading
		ForceEnable		= 2,		// Force enable async loading
	};

	void SetAsyncLoadMode(const EAsyncLoadMode NewMode);
	bool ShouldAsyncLoad() const;
	void AsyncPackageCallback(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result);

	// While async loading of pending must be mapped references we need to maintain references to already resolved objects as there will be no instance referencing them
	void AddReferencedObjects(FReferenceCollector& ReferenceCollector);
	void AddTrackedQueuedBatchObjectReference(const FNetRefHandle InHandle, const UObject* InObject);
	void UpdateTrackedQueuedBatchObjectReference(const FNetRefHandle InHandle, const UObject* NewObject);
	void RemoveTrackedQueuedBatchObjectReference(const FNetRefHandle InHandle);

	FString DescribeObjectReference(const FNetObjectReference Ref, const FNetObjectResolveContext& ResolveContext);

private:

	struct FCachedNetObjectReference
	{
		TWeakObjectPtr<UObject> Object;
		const UObject* ObjectKey = nullptr;

		// NetRefHandle
		FNetRefHandle NetRefHandle;

		// RelativePath to outer
		FNetToken RelativePath;

		// Ref to outer
		FNetRefHandle OuterNetRefHandle;

		// Flags
		uint8 bNoLoad : 1;				// Don't load this, only do a find
		uint8 bIgnoreWhenMissing : 1;
		uint8 bIsPackage : 1;
		uint8 bIsBroken : 1;
		uint8 bIsPending : 1;
	};

	struct FQueuedBatchObjectReference
	{
		TObjectPtr<const UObject> Object = nullptr;
		uint32 RefCount = 0U;
	};

	struct FPendingAsyncLoadRequest
	{
		FPendingAsyncLoadRequest(FNetRefHandle InNetRefHandle, double InRequestStartTime);
		void Merge(const FPendingAsyncLoadRequest& Other);
		void Merge(FNetRefHandle InNetRefHandle);

		// NetRefHandles that requested loading for the same UPackage
		TArray<FNetRefHandle, TInlineAllocator<4>> NetRefHandles;
		double RequestStartTime;
	};

	bool CreateObjectReferenceInternal(const UObject* Object, FNetObjectReference& OutReference);

	void ConditionalWriteNetTokenData(FNetSerializationContext& Context, Private::FNetExportContext* ExportContext, const FNetToken& NetToken) const;
	void ConditionalReadNetTokenData(FNetSerializationContext& Context, const FNetToken& NetToken) const;

	void ReadFullReferenceInternal(FNetSerializationContext& Context, FNetObjectReference& OutRef, uint32 RecursionCount);
	void WriteFullReferenceInternal(FNetSerializationContext& Context, const FNetObjectReference& Ref) const;

	UObject* ResolveObjectReferenceHandleInternal(FNetRefHandle RefHandle, const FNetObjectResolveContext& ResolveContext, bool& bOutMustBeMapped);
	bool IsDynamicInternal(const UObject* Object) const;
	bool SupportsObjectInternal(const UObject* Object) const;
	bool CanClientLoadObjectInternal(const UObject* Object, bool bIsDynamic) const;
	bool ShouldIgnoreWhenMissing(FNetRefHandle RefHandle) const;
	bool RenamePathForPie(uint32 ConnectionId, FString& Str, bool bReading);

	// Get the string path of RefHandle
	FString FullPath(FNetRefHandle RefHandle, const FNetObjectResolveContext& ResolveContext) const;
	void GenerateFullPath_r(FNetRefHandle RefHandle, const FNetObjectResolveContext& ResolveContext, FString& OutFullPath) const;

	// Find dynamic root
	FNetRefHandle GetDynamicRoot(const FNetRefHandle Handle) const;

	static FNetObjectReference MakeNetObjectReference(FNetRefHandle RefHandle, FNetToken RelativePath);
	static FNetObjectReference MakeNetObjectReference(const FCachedNetObjectReference& CachedReference);

	// Must be mapped exports are written for each batch that serializes object references, if async loading is enabled the client
	// will defer application of data contained in the batch until the must be mapped exports are resolvable.
	bool WriteMustBeMappedExports(FNetSerializationContext& Context, FInternalNetRefIndex ObjectIndex, TArrayView<const FNetObjectReference> ExportsView) const;
	void ReadMustBeMappedExports(FNetSerializationContext& Context, TArray<FNetRefHandle>* MustBeMappedExports);

	void StartAsyncLoadingPackage(FCachedNetObjectReference& Object, FName PackagePath, const FNetRefHandle RefHandle, const bool bWasAlreadyAsyncLoading);
	void ValidateAsyncLoadingPackage(FCachedNetObjectReference& Object, FName PackagePath, const FNetRefHandle RefHandle);

private:

	// Map raw UObject pointer -> Handle
	// To verify that the reference is valid we need to check the weakpointer stored in the cache
	TMap<const UObject*, FNetRefHandle> ObjectToNetReferenceHandle;

	// Map ReferenceHandle -> CachedReference
	TMap<FNetRefHandle, FCachedNetObjectReference> ReferenceHandleToCachedReference;

	// To properly clean up stale references referencing dynamic objects we need to track them
	TMultiMap<FNetRefHandle, FNetRefHandle> HandleToDynamicOuter;
	
	UReplicationSystem* ReplicationSystem;
	UObjectReplicationBridge* ReplicationBridge;
	FNetTokenStore* NetTokenStore;
	FStringTokenStore* StringTokenStore;
	FNetRefHandleManager* NetRefHandleManager;
	
	// Do we have authority to create references?
	uint32 bIsAuthority : 1;

	/**
	 * Set of all current Objects that we've been requested to be referenced while we are doing async loading.
	 * This is used to prevent objects (especially async load objects,
	 * which may have no other references) from being GC'd while a the object is waiting for more
	 * pending references
	 */
	TMap<FNetRefHandle, FQueuedBatchObjectReference> QueuedBatchObjectReferences;

	EAsyncLoadMode AsyncLoadMode;
	bool bCachedCVarAllowAsyncLoading;

	/** Set of packages that are currently pending async loads, referenced by package name. */
	TMap<FName, FPendingAsyncLoadRequest> PendingAsyncLoadRequests;

	// $TODO: $IRIS: Stats support
#if 0
	/** Store all GUIDs that caused the sync loading of a package, for debugging & logging with LogNetSyncLoads */
	//TArray<FNetRefHandle> SyncLoadedGUIDs;
	//FNetAsyncLoadDelinquencyAnalytics DelinquentAsyncLoads;
	//void ConsumeAsyncLoadDelinquencyAnalytics(FNetAsyncLoadDelinquencyAnalytics& Out);
	//const FNetAsyncLoadDelinquencyAnalytics& GetAsyncLoadDelinquencyAnalytics() const;
	//void ResetAsyncLoadDelinquencyAnalytics();	
	//bool WasGUIDSyncLoaded(FNetworkGUID NetGUID) const { return SyncLoadedGUIDs.Contains(NetGUID); }
	//void ClearSyncLoadedGUID(FNetworkGUID NetGUID) { SyncLoadedGUIDs.Remove(NetGUID); }
	/**
	 * If LogNetSyncLoads is enabled, log all objects that caused a sync load that haven't been otherwise reported
	 * by the package map yet, and clear that list.
	 */
	//void ReportSyncLoadedGUIDs();
#endif

};

inline UObject* FObjectReferenceCache::ResolveObjectReferenceHandle(FNetRefHandle RefHandle, const FNetObjectResolveContext& ResolveContext)
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

inline FNetObjectReference FObjectReferenceCache::MakeNetObjectReference(FNetRefHandle Handle)
{
	return FNetObjectReference(Handle);
}

inline FNetObjectReference FObjectReferenceCache::MakeNetObjectReference(FNetRefHandle RefHandle, FNetToken RelativePath)
{
	const ENetObjectReferenceTraits Traits = RelativePath.IsValid() ? ENetObjectReferenceTraits::CanBeExported : ENetObjectReferenceTraits::None;
	return FNetObjectReference(RefHandle, RelativePath, Traits);
}

inline FNetObjectReference FObjectReferenceCache::MakeNetObjectReference(const FCachedNetObjectReference& CachedReference)
{
	const ENetObjectReferenceTraits Traits = CachedReference.RelativePath.IsValid() ? ENetObjectReferenceTraits::CanBeExported : ENetObjectReferenceTraits::None;
	return FNetObjectReference(CachedReference.NetRefHandle, FNetToken(), Traits);
}

}
