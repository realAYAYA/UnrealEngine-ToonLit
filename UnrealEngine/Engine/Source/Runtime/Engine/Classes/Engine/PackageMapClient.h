// Copyright Epic Games, Inc. All Rights Reserved.


/**
 * PackageMap implementation that is client/connection specific. This subclass implements all NetGUID Acking and interactions with a UConnection.
 *	On the server, each client will have their own instance of UPackageMapClient.
 *
 *	UObjects are first serialized as <NetGUID, Name/Path> pairs. UPackageMapClient tracks each NetGUID's usage and knows when a NetGUID has been ACKd.
 *	Once ACK'd, objects are just serialized as <NetGUID>. The result is higher bandwidth usage upfront for new clients, but minimal bandwidth once
 *	things gets going.
 *
 *	A further optimization is enabled by default. References will actually be serialized via:
 *	<NetGUID, <(Outer *), Object Name> >. Where Outer * is a reference to the UObject's Outer.
 *
 *	The main advantages from this are:
 *		-Flexibility. No precomputed net indices or large package lists need to be exchanged for UObject serialization.
 *		-Cross version communication. The name is all that is needed to exchange references.
 *		-Efficiency in that a very small % of UObjects will ever be serialized. Only Objects that serialized are assigned NetGUIDs.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/NetworkGuid.h"
#include "Misc/NetworkVersion.h"
#include "UObject/CoreNet.h"
#include "Net/DataBunch.h"
#include "Net/NetAnalyticsTypes.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "HAL/LowLevelMemTracker.h"
#include "PackageMapClient.generated.h"

LLM_DECLARE_TAG_API(GuidCache, ENGINE_API);

class UNetConnection;
class UNetDriver;

class ENGINE_API FNetFieldExport
{
public:
	FNetFieldExport() : Handle( 0 ), CompatibleChecksum( 0 ), bExported( false ), bDirtyForReplay( true ), bIncompatible( false )
	{
	}

	FNetFieldExport( const uint32 InHandle, const uint32 InCompatibleChecksum, const FName& InName ) :
		Handle( InHandle ),
		CompatibleChecksum( InCompatibleChecksum ),
		ExportName( InName ),
		bExported( false ),
		bDirtyForReplay( true ),
		bIncompatible( false )
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FNetFieldExport& C);

	void CountBytes(FArchive& Ar) const;

	uint32			Handle;
	uint32			CompatibleChecksum;
	FName			ExportName;
	bool			bExported;
	bool			bDirtyForReplay;

	// Transient properties
	mutable bool	bIncompatible;		// If true, we've already determined that this property isn't compatible. We use this to curb warning spam.
};

class ENGINE_API FNetFieldExportGroup
{
public:
	FNetFieldExportGroup() : PathNameIndex( 0 ), bDirtyForReplay( true ) { }

	FString						PathName;
	uint32						PathNameIndex;
	TArray< FNetFieldExport >	NetFieldExports;
	bool						bDirtyForReplay;

	friend FArchive& operator<<(FArchive& Ar, FNetFieldExportGroup& C);

	int32 FindNetFieldExportHandleByChecksum(const uint32 Checksum) const
	{
		for (int32 i = 0; i < NetFieldExports.Num(); i++)
		{
			if (NetFieldExports[i].CompatibleChecksum == Checksum)
			{
				return i;
			}
		}

		return INDEX_NONE;
	}

	void CountBytes(FArchive& Ar) const;
};

/** Stores an object with path associated with FNetworkGUID */
class FNetGuidCacheObject
{
public:
	FNetGuidCacheObject() : NetworkChecksum( 0 ), ReadOnlyTimestamp( 0 ), bNoLoad( 0 ), bIgnoreWhenMissing( 0 ), bIsPending( 0 ), bIsBroken( 0 ), bDirtyForReplay( 1 )
	{
	}

	TWeakObjectPtr< UObject >	Object;

	// These fields are set when this guid is static
	FNetworkGUID				OuterGUID;
	FName						PathName;
	uint32						NetworkChecksum;			// Network checksum saved, used to determine backwards compatible

	double						ReadOnlyTimestamp;			// Time in second when we should start timing out after going read only

	uint8						bNoLoad				: 1;	// Don't load this, only do a find
	uint8						bIgnoreWhenMissing	: 1;	// Don't warn when this asset can't be found or loaded
	uint8						bIsPending			: 1;	// This object is waiting to be fully loaded
	uint8						bIsBroken			: 1;	// If this object failed to load, then we set this to signify that we should stop trying
	uint8						bDirtyForReplay		: 1;	// If this object has been modified, used by replay checkpoints
};

enum class EAppendNetExportFlags : uint32
{
	None = 0,
	ForceExportDirtyGroups = (1 << 0),
};

ENUM_CLASS_FLAGS(EAppendNetExportFlags);


/** Convenience type for holding onto references to objects while we have queued bunches referring to those objects. */
struct FQueuedBunchObjectReference
{
private:

	friend class FNetGUIDCache;

	FQueuedBunchObjectReference(const FNetworkGUID InNetGUID, UObject* InObject) :
		NetGUID(InNetGUID),
		Object(InObject)
	{
	}

	FNetworkGUID NetGUID;
	UObject* Object;
};

class ENGINE_API FNetGUIDCache
{
public:
	FNetGUIDCache( UNetDriver * InDriver );

	enum class ENetworkChecksumMode : uint8
	{
		None			= 0,		// Don't use checksums
		SaveAndUse		= 1,		// Save checksums in stream, and use to validate while loading packages
		SaveButIgnore	= 2,		// Save checksums in stream, but ignore when loading packages
	};

	enum class EAsyncLoadMode : uint8
	{
		UseCVar			= 0,		// Use CVar (net.AllowAsyncLoading) to determine if we should async load
		ForceDisable	= 1,		// Disable async loading
		ForceEnable		= 2,		// Force enable async loading
	};

	void			CleanReferences();
	bool			SupportsObject( const UObject* Object, const TWeakObjectPtr<UObject>* WeakObjectPtr=nullptr /** Optional: pass in existing weakptr to prevent this function from constructing one internally */ ) const;
	bool			IsDynamicObject( const UObject* Object );
	bool			IsNetGUIDAuthority() const;
	FNetworkGUID	GetOrAssignNetGUID( UObject* Object, const TWeakObjectPtr<UObject>* WeakObjectPtr=nullptr /** Optional: pass in existing weakptr to prevent this function from constructing one internally */ );
	FNetworkGUID	GetNetGUID( const UObject* Object ) const;
	FNetworkGUID	GetOuterNetGUID( const FNetworkGUID& NetGUID ) const;
	FNetworkGUID	AssignNewNetGUID_Server( UObject* Object );
	FNetworkGUID	AssignNewNetGUIDFromPath_Server( const FString& PathName, UObject* ObjOuter, UClass* ObjClass );
	void			RegisterNetGUID_Internal( const FNetworkGUID& NetGUID, const FNetGuidCacheObject& CacheObject );
	void			RegisterNetGUID_Server( const FNetworkGUID& NetGUID, UObject* Object );
	void			RegisterNetGUID_Client( const FNetworkGUID& NetGUID, const UObject* Object );
	void			RegisterNetGUIDFromPath_Client( const FNetworkGUID& NetGUID, const FString& PathName, const FNetworkGUID& OuterGUID, const uint32 NetworkChecksum, const bool bNoLoad, const bool bIgnoreWhenMissing );
	void			RegisterNetGUIDFromPath_Server( const FNetworkGUID& NetGUID, const FString& PathName, const FNetworkGUID& OuterGUID, const uint32 NetworkChecksum, const bool bNoLoad, const bool bIgnoreWhenMissing );
	UObject *		GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped );
	bool			ShouldIgnoreWhenMissing( const FNetworkGUID& NetGUID ) const;
	FNetGuidCacheObject const * const GetCacheObject(const FNetworkGUID& NetGUID) const;
	bool			IsGUIDRegistered( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDLoaded( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const;
	bool			IsGUIDNoLoad( const FNetworkGUID& NetGUID ) const;
	bool			IsGUIDPending( const FNetworkGUID& NetGUID ) const;
	FString			FullNetGUIDPath( const FNetworkGUID& NetGUID ) const;
	void			GenerateFullNetGUIDPath_r( const FNetworkGUID& NetGUID, FString& FullPath ) const;
	uint32			GetClassNetworkChecksum( UClass* Class );
	uint32			GetNetworkChecksum( UObject* Obj );
	void			SetNetworkChecksumMode( const ENetworkChecksumMode NewMode );
	void			SetAsyncLoadMode( const EAsyncLoadMode NewMode );
	bool			ShouldAsyncLoad() const;
	bool			CanClientLoadObject( const UObject* Object, const FNetworkGUID& NetGUID ) const;
	FString			Describe(const FNetworkGUID& NetGUID) const;

	void			AsyncPackageCallback(const FName& PackageName, UPackage * Package, EAsyncLoadingResult::Type Result);

	void			ResetCacheForDemo();

	void			CountBytes(FArchive& Ar) const;

	void ConsumeAsyncLoadDelinquencyAnalytics(FNetAsyncLoadDelinquencyAnalytics& Out);
	const FNetAsyncLoadDelinquencyAnalytics& GetAsyncLoadDelinquencyAnalytics() const;
	void ResetAsyncLoadDelinquencyAnalytics();

	void CollectReferences(class FReferenceCollector& ReferenceCollector);
	TSharedRef<FQueuedBunchObjectReference> TrackQueuedBunchObjectReference(const FNetworkGUID InNetGUID, UObject* InObject);

	bool WasGUIDSyncLoaded(FNetworkGUID NetGUID) const { return SyncLoadedGUIDs.Contains(NetGUID); }
	void ClearSyncLoadedGUID(FNetworkGUID NetGUID) { SyncLoadedGUIDs.Remove(NetGUID); }

	/**
	 * If LogNetSyncLoads is enabled, log all objects that caused a sync load that haven't been otherwise reported
	 * by the package map yet, and clear that list.
	 */
	void ReportSyncLoadedGUIDs();

	TMap< FNetworkGUID, FNetGuidCacheObject >		ObjectLookup;
	TMap< TWeakObjectPtr< UObject >, FNetworkGUID >	NetGUIDLookup;
	int32											UniqueNetIDs[2];

	TSet< FNetworkGUID >							ImportedNetGuids;
	TMap< FNetworkGUID, TSet< FNetworkGUID > >		PendingOuterNetGuids;

	UNetDriver *									Driver;

	ENetworkChecksumMode							NetworkChecksumMode;
	EAsyncLoadMode									AsyncLoadMode;

	bool											IsExportingNetGUIDBunch;

private:

	friend class UPackageMapClient;

	TMap<FName, FNetworkGUID> PendingAsyncPackages;

	/** Maps net field export group name to the respective FNetFieldExportGroup */
	TMap < FString, TSharedPtr< FNetFieldExportGroup > >	NetFieldExportGroupMap;

	/** Maps field export group path to assigned index */
	TMap < FString, uint32 >								NetFieldExportGroupPathToIndex;

	/** Maps assigned net field export group index to pointer to group, lifetime of the referenced FNetFieldExportGroups are managed by NetFieldExportGroupMap **/
	TMap < uint32, FNetFieldExportGroup* >					NetFieldExportGroupIndexToGroup;

	/** Current index used when filling in NetFieldExportGroupPathToIndex/NetFieldExportGroupIndexToPath */
	int32													UniqueNetFieldExportGroupPathIndex;

	/** Store all GUIDs that caused the sync loading of a package, for debugging & logging with LogNetSyncLoads */
	TArray<FNetworkGUID> SyncLoadedGUIDs;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
public:

	static const bool IsHistoryEnabled();

	// History for debugging entries in the guid cache
	TMap<FNetworkGUID, FString>						History;
private:
#endif

	struct FPendingAsyncLoadRequest
	{
		FPendingAsyncLoadRequest(const FNetworkGUID InNetGUID, const double InRequestStartTime):
			NetGUID(InNetGUID),
			RequestStartTime(InRequestStartTime)
		{
		}

		FNetworkGUID NetGUID;
		double RequestStartTime;

#if CSV_PROFILER
		bool bWasRequestedByOwnerOrPawn = false;
#endif
	};

	/** Set of packages that are currently pending Async loads, referenced by package name. */
	TMap<FName, FPendingAsyncLoadRequest> PendingAsyncLoadRequests;

	FNetAsyncLoadDelinquencyAnalytics DelinquentAsyncLoads;

	void StartAsyncLoadingPackage(FNetGuidCacheObject& Object, const FNetworkGUID ObjectGUID, const bool bWasAlreadyAsyncLoading);
	void ValidateAsyncLoadingPackage(FNetGuidCacheObject& Object, const FNetworkGUID ObjectGUID);

	void UpdateQueuedBunchObjectReference(const FNetworkGUID NetGUID, UObject* NewObject);

	/**
	 * Set of all current Objects that we've been requested to be referenced while channels
	 * resolve their queued bunches. This is used to prevent objects (especially async load objects,
	 * which may have no other references) from being GC'd while a channel is waiting for more
	 * pending guids. 
	 */
	TMap<FNetworkGUID, TWeakPtr<FQueuedBunchObjectReference>> QueuedBunchObjectReferences;

#if CSV_PROFILER
public:

	bool IsTrackingOwnerOrPawn() const;

	struct FIsOwnerOrPawnHelper
	{
	private:
		friend class UActorChannel;

		FIsOwnerOrPawnHelper(
			class FNetGUIDCache* const InGuidCache,
			const class AActor* InConnectionActor,
			const class AActor* ChannelActor);

	public:

		~FIsOwnerOrPawnHelper();

		bool IsOwnerOrPawn() const;

	private:

		class FNetGUIDCache* const GuidCache;
		const class AActor* ConnectionActor;
		const class AActor* ChannelActor;
		mutable int8 CachedResult = INDEX_NONE;

		// No copying, moving, or constructing off stack.
		// References or pointers to this class should never be kept alive.
		FIsOwnerOrPawnHelper(const FIsOwnerOrPawnHelper&) = delete;
		FIsOwnerOrPawnHelper(FIsOwnerOrPawnHelper&&) = delete;

		FIsOwnerOrPawnHelper& operator=(const FIsOwnerOrPawnHelper&) = delete;
		FIsOwnerOrPawnHelper& operator=(FIsOwnerOrPawnHelper&&) = delete;

		void* operator new (size_t) = delete;
		void* operator new[](size_t) = delete;
		void operator delete (void *) = delete;
		void operator delete[](void*) = delete;
	};

private:

	FIsOwnerOrPawnHelper* TrackingOwnerOrPawnHelper = nullptr;
#endif
};

class ENGINE_API FPackageMapAckState
{
public:
	TMap< FNetworkGUID, int32 >	NetGUIDAckStatus;				// Map that represents the ack state of each net guid for this connection
	TSet< uint32 >				NetFieldExportGroupPathAcked;	// Map that represents whether or not a net field export group has been ack'd by the client
	TSet< uint64 >				NetFieldExportAcked;			// Map that represents whether or not a net field export has been ack'd by the client

	void Reset()
	{
		NetGUIDAckStatus.Empty();
		NetFieldExportGroupPathAcked.Empty();
		NetFieldExportAcked.Empty();
	}

	void CountBytes(FArchive& Ar) const;
};

UCLASS(transient)
class ENGINE_API UPackageMapClient : public UPackageMap
{
public:
	GENERATED_BODY()

	UPackageMapClient(const FObjectInitializer & ObjectInitializer = FObjectInitializer::Get());

	void Initialize(UNetConnection * InConnection, TSharedPtr<FNetGUIDCache> InNetGUIDCache)
	{
		Connection = InConnection;
		GuidCache = InNetGUIDCache;
		ExportNetGUIDCount = 0;
		OverrideAckState = &AckState;
	}

	virtual ~UPackageMapClient()
	{
		if (CurrentExportBunch)
		{
			delete CurrentExportBunch;
			CurrentExportBunch = NULL;
		}
	}

	
	// UPackageMap Interface
	virtual bool SerializeObject( FArchive& Ar, UClass* InClass, UObject*& Obj, FNetworkGUID *OutNetGUID = NULL ) override;
	virtual bool SerializeNewActor( FArchive& Ar, class UActorChannel *Channel, class AActor*& Actor) override;
	
	virtual bool WriteObject( FArchive& Ar, UObject* InOuter, FNetworkGUID NetGUID, FString ObjName ) override;

	// UPackageMapClient Connection specific methods

	bool NetGUIDHasBeenAckd(FNetworkGUID NetGUID);

	virtual void ReceivedNak( const int32 NakPacketId ) override;
	virtual void ReceivedAck( const int32 AckPacketId ) override;
	virtual void NotifyBunchCommit( const int32 OutPacketId, const FOutBunch* OutBunch ) override;
	virtual void GetNetGUIDStats(int32 &AckCount, int32 &UnAckCount, int32 &PendingCount) override;

	void ReceiveNetGUIDBunch( FInBunch &InBunch );
	void AppendExportBunches(TArray<FOutBunch *>& OutgoingBunches);

	void AppendExportData(FArchive& Archive);
	void ReceiveExportData(FArchive& Archive);

	TMap<FNetworkGUID, int32>	NetGUIDExportCountMap;	// How many times we've exported each NetGUID on this connection. Public for ListNetGUIDExports 

	void HandleUnAssignedObject( UObject* Obj );

	static void	AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	virtual void NotifyStreamingLevelUnload(UObject* UnloadedLevel) override;

	virtual bool PrintExportBatch() override;

	virtual void ResetTrackedSyncLoadedGuids() override { TrackedSyncLoadedGUIDs.Reset(); }
	virtual void ReportSyncLoadsForProperty(const FProperty* Property, const UObject* Object) override;

	virtual void			LogDebugInfo( FOutputDevice & Ar) override;
	virtual UObject *		GetObjectFromNetGUID( const FNetworkGUID& NetGUID, const bool bIgnoreMustBeMapped ) override;
	virtual FNetworkGUID	GetNetGUIDFromObject( const UObject* InObject) const override;
	virtual bool			IsGUIDBroken( const FNetworkGUID& NetGUID, const bool bMustBeRegistered ) const override { return GuidCache->IsGUIDBroken( NetGUID, bMustBeRegistered ); }

	/** Returns true if this guid is directly pending, or depends on another guid that is pending */
	virtual bool			IsGUIDPending( const FNetworkGUID& NetGUID ) const;

	/** Set rather this actor is associated with a channel with queued bunches */
	virtual void			SetHasQueuedBunches( const FNetworkGUID& NetGUID, bool bHasQueuedBunches );

	TArray< FNetworkGUID > & GetMustBeMappedGuidsInLastBunch() { return MustBeMappedGuidsInLastBunch; }

	class UNetConnection* GetConnection() { return Connection; }

	void SyncPackageMapExportAckStatus( const UPackageMapClient* Source );

	void SavePackageMapExportAckStatus( FPackageMapAckState& OutState );
	void RestorePackageMapExportAckStatus( const FPackageMapAckState& InState );
	void OverridePackageMapExportAckStatus( FPackageMapAckState* NewState );

	/** Resets the AckState and empties the PendingAckGuids, not meant to reset the OverrideAckState. */
	void ResetAckState();

	/** Functions to help with exporting/importing net field info */
	TSharedPtr< FNetFieldExportGroup >	GetNetFieldExportGroup( const FString& PathName );
	void								AddNetFieldExportGroup( const FString& PathName, TSharedPtr< FNetFieldExportGroup > NewNetFieldExportGroup );
	void								TrackNetFieldExport( FNetFieldExportGroup* NetFieldExportGroup, const int32 NetFieldExportHandle );
	TSharedPtr< FNetFieldExportGroup >	GetNetFieldExportGroupChecked( const FString& PathName ) const;
	void								SerializeNetFieldExportGroupMap( FArchive& Ar, bool bClearPendingExports=true );
	void								SerializeNetFieldExportDelta(FArchive& Ar);

	TUniquePtr<TGuardValue<bool>> ScopedIgnoreReceivedExportGUIDs()
	{
		return MakeUnique<TGuardValue<bool>>(bIgnoreReceivedExportGUIDs, true);
	}

	virtual void Serialize(FArchive& Ar) override;

	FString GetFullNetGUIDPath(const FNetworkGUID& NetGUID) const
	{
		FString FullGuidCachePath;

		if (const FNetGUIDCache * const GuidCacheLocal = GuidCache.Get())
		{
			FullGuidCachePath = GuidCacheLocal->FullNetGUIDPath(NetGUID);
		}

		return FullGuidCachePath;
	}

protected:

	/** Functions to help with exporting/importing net field export info */
	void AppendNetFieldExports( FArchive& Archive );
	void ReceiveNetFieldExports( FArchive& Archive );

	void AppendNetFieldExportsInternal( FArchive& Archive, const TSet<uint64>& InNetFieldExports, EAppendNetExportFlags Flags );

	void AppendNetExportGUIDs( FArchive& Archive );
	void ReceiveNetExportGUIDs( FArchive& Archive );

	bool ExportNetGUIDForReplay( FNetworkGUID&, UObject* Object, FString& PathName, UObject* ObjOuter );
	bool ExportNetGUID( FNetworkGUID NetGUID, UObject* Object, FString PathName, UObject* ObjOuter );
	void ExportNetGUIDHeader();

	void			InternalWriteObject( FArchive& Ar, FNetworkGUID NetGUID, UObject* Object, FString ObjectPathName, UObject* ObjectOuter );	
	FNetworkGUID	InternalLoadObject( FArchive & Ar, UObject *& Object, const int32 InternalLoadObjectRecursionCount );

	virtual UObject* ResolvePathAndAssignNetGUID( const FNetworkGUID& NetGUID, const FString& PathName ) override;

	bool	ShouldSendFullPath(const UObject* Object, const FNetworkGUID &NetGUID);
	
	bool IsNetGUIDAuthority() const;

	class UNetConnection* Connection;

	bool ObjectLevelHasFinishedLoading(UObject* Obj) const;

	TArray<TArray<uint8>>				ExportGUIDArchives;
	TSet< FNetworkGUID >				CurrentExportNetGUIDs;				// Current list of NetGUIDs being written to the Export Bunch.

	/** Set of Actor NetGUIDs with currently queued bunches and the time they were first queued. */
	TMap<FNetworkGUID, double> CurrentQueuedBunchNetGUIDs;

	TArray< FNetworkGUID >				PendingAckGUIDs;					// Quick access to all GUID's that haven't been acked

	FPackageMapAckState					AckState;							// Current ack state of exported data
	FPackageMapAckState*				OverrideAckState;					// This is a pointer that allows us to override the current ack state, it's never NULL (it will point to AckState by default)

	// Bunches of NetGUID/path tables to send with the current content bunch
	TArray<FOutBunch* >					ExportBunches;
	FOutBunch *							CurrentExportBunch;

	int32								ExportNetGUIDCount;

	TSharedPtr< FNetGUIDCache >			GuidCache;

	TArray< FNetworkGUID >				MustBeMappedGuidsInLastBunch;

	/** List of net field exports that need to go out on next bunch */
	TSet< uint64 >						NetFieldExports;

private:
	void ReceiveNetFieldExportsCompat(FInBunch& InBunch);

	/** Used by SerializeNewActor to report sync loads with LogNetSyncLoads */
	void ReportSyncLoadsForActorSpawn(const AActor* Actor);

	bool bIgnoreReceivedExportGUIDs;

public:

	int32 GetNumQueuedBunchNetGUIDs() const;
	void ConsumeQueuedActorDelinquencyAnalytics(FNetQueuedActorDelinquencyAnalytics& Out);
	const FNetQueuedActorDelinquencyAnalytics& GetQueuedActorDelinquencyAnalytics() const;
	void ResetQueuedActorDelinquencyAnalytics();

private:

	FNetQueuedActorDelinquencyAnalytics DelinquentQueuedActors;

	/**
	 * Tracks GUIDs contained in FNetGUIDCache::SyncLoadedGUIDs that are referenced during deserialization
	 * of a particular property or actor spawn, to help correlate those sync loads with that property or actor.
	 */
	TArray<FNetworkGUID> TrackedSyncLoadedGUIDs;
};