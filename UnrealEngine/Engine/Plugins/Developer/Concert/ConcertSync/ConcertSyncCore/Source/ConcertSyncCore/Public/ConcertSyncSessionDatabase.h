// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertSyncSessionTypes.h"
#include "Templates/SharedPointerInternals.h"

class FConcertFileCache;
class FConcertSyncSessionDatabaseStatements;

class FSQLiteDatabase;
enum class ESQLiteDatabaseOpenMode : uint8;

enum class EBreakBehavior
{
	Break,
	Continue
};

using FConsumePackageActivityFunc = TFunctionRef<void(FConcertSyncActivity&&/*BasePart*/, FConcertSyncPackageEventData& /*EventPart*/)>;
using FIteratePackageActivityFunc = TFunctionRef<EBreakBehavior(FConcertSyncActivity&&/*BasePart*/, FConcertSyncPackageEventData& /*EventPart*/)>;
using FIterateActivityFunc = TFunctionRef<EBreakBehavior(FConcertSyncActivity&&)>;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnActivityProduced, const FConcertSyncActivity&);

/**
 * Database of activities that have happened in a Concert Sync Session.
 * Stores the activity index and their associated data.
 */
class CONCERTSYNCCORE_API FConcertSyncSessionDatabase
{
public:
	
	FConcertSyncSessionDatabase();
	~FConcertSyncSessionDatabase();

	FConcertSyncSessionDatabase(const FConcertSyncSessionDatabase&) = delete;
	FConcertSyncSessionDatabase& operator=(const FConcertSyncSessionDatabase&) = delete;

	FConcertSyncSessionDatabase(FConcertSyncSessionDatabase&&);
	FConcertSyncSessionDatabase& operator=(FConcertSyncSessionDatabase&&);

	/**
	 * Is this a valid database? (ie, has been successfully opened).
	 */
	bool IsValid() const;

	/**
	 * Open (or create) a database file.
	 *
	 * @param InSessionPath				The root path to store all the data for this session under.
	 *
	 * @return True if the database was opened, false otherwise.
	 */
	bool Open(const FString& InSessionPath);

	/**
	 * Open (or create) a database file.
	 *
	 * @param InSessionPath				The root path to store all the data for this session under.
	 * @param InOpenMode				How should the database be opened?
	 *
	 * @return True if the database was opened, false otherwise.
	 */
	bool Open(const FString& InSessionPath, const ESQLiteDatabaseOpenMode InOpenMode);

	/**
	 * Close an open database file.
	 *
	 * @param InDeleteDatabase			True if the session database and its associated data should be deleted after closing the database.
	 */
	bool Close(const bool InDeleteDatabase = false);

	/**
	 * Get the filename of the currently open database, or an empty string.
	 * @note The returned filename will be an absolute pathname.
	 */
	FString GetFilename() const;

	/**
	 * Get the last error reported by this database.
	 */
	FString GetLastError() const;

	/**
	 * Add a new connection activity to this database, assigning it both an activity and connection event ID.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the server to populate its version of the session database.
	 *
	 * @param InConnectionActivity		The connection activity to add (the ActivityId, EventTime, EventType, and EventId members are ignored).
	 * @param OutActivityId				Populated with the ID of this activity in the database.
	 * @param OutConnectionEventId		Populated with the ID of the connection event in the database (@see GetConnectionEvent).
	 *
	 * @return True if the connection activity was added, false otherwise.
	 */
	bool AddConnectionActivity(const FConcertSyncConnectionActivity& InConnectionActivity, int64& OutActivityId, int64& OutConnectionEventId);

	/**
	 * Add a new lock activity to this database, assigning it both an activity and lock event ID.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the server to populate its version of the session database.
	 *
	 * @param InLockActivity			The lock activity to add (the ActivityId, EventTime, EventType, and EventId members are ignored).
	 * @param OutActivityId				Populated with the ID of this activity in the database.
	 * @param OutLockEventId			Populated with the ID of the lock event in the database (@see GetLockEvent).
	 *
	 * @return True if the lock activity was added, false otherwise.
	 */
	bool AddLockActivity(const FConcertSyncLockActivity& InLockActivity, int64& OutActivityId, int64& OutLockEventId);

	/**
	 * Add a new transaction activity to this database, assigning it both an activity and transaction event ID.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the server to populate its version of the session database.
	 *
	 * @param InTransactionActivity		The transaction activity to add (the ActivityId, EventTime, EventType, and EventId members are ignored).
	 * @param OutActivityId				Populated with the ID of this activity in the database.
	 * @param OutTransactionEventId		Populated with the ID of the transaction event in the database (@see GetTransactionEvent).
	 *
	 * @return True if the transaction activity was added, false otherwise.
	 */
	bool AddTransactionActivity(const FConcertSyncTransactionActivity& InTransactionActivity, int64& OutActivityId, int64& OutTransactionEventId);

	/**
	 * Add a new package activity to this database, assigning it both an activity and package event ID.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the server to populate its version of the session database.
	 *
	 * @param InPackageActivity			The base part of the package activity to add (the ActivityId, EventTime, EventType, EventId, and PackageRevision members are ignored).
	 * @param InPackageInfo				Information about the package affected.
	 * @param InPackageDataStream		A stream to the package data if the activty is about adding or modifying a package. Can be empty when the activity is about deleting or discarding changes.
	 * @param OutActivityId				Populated with the ID of this activity in the database.
	 * @param OutPackageEventId			Populated with the ID of the package event in the database (@see GetPackageEvent).
	 *
	 * @return True if the package activity was added, false otherwise.
	 */
	bool AddPackageActivity(const FConcertSyncActivity& InPackageActivity, const FConcertPackageInfo& InPackageInfo, FConcertPackageDataStream& InPackageDataStream, int64& OutActivityId, int64& OutPackageEventId);

	/**
	 * Iterates the given range, calls UpdateCallback on each element, and commits the update.
	 */
	bool SetActivities(const TSet<int64>& ActivityIds, TFunctionRef<void(FConcertSyncActivity&)> UpdateCallback);
	
	/**
	 * Set a connection activity in this database, creating or replacing it.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the client to populate its version of the session database from data synced from the server.
	 *
	 * @param InConnectionActivity		The connection activity to set.
	 *
	 * @return True if the connection activity was set, false otherwise.
	 */
	bool SetConnectionActivity(const FConcertSyncConnectionActivity& InConnectionActivity);

	/**
	 * Set a lock activity in this database, creating or replacing it.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the client to populate its version of the session database from data synced from the server.
	 *
	 * @param InLockActivity			The lock activity to set.
	 *
	 * @return True if the lock activity was set, false otherwise.
	 */
	bool SetLockActivity(const FConcertSyncLockActivity& InLockActivity);

	/**
	 * Set a transaction activity in this database, creating or replacing it.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the client to populate its version of the session database from data synced from the server.
	 *
	 * @param InTransactionActivity		The transaction activity to set.
	 * @param bMetaDataOnly				True to store the meta data only, omitting the transaction data required to replay the transaction.
	 *
	 * @return True if the transaction activity was set, false otherwise.
	 */
	bool SetTransactionActivity(const FConcertSyncTransactionActivity& InTransactionActivity, const bool bMetaDataOnly = false);

	/**
	 * Set a package activity in this database, creating or replacing it.
	 * @note The endpoint ID referenced by the activity must exist in the database (@see SetEndpoint).
	 * @note This function is expected to be called on the client to populate its version of the session database from data synced from the server.
	 *
	 * @param InPackageActivity			The base part of the package activity to set.
	 * @param InPackageActivityEvent	The event meta data (package info + revision) and possibly the corresponding package data.
	 * @param bMetaDataOnly				True to store the meta data only, omitting the package data itself.
	 *
	 * @return True if the package activity was set, false otherwise.
	 */
	bool SetPackageActivity(const FConcertSyncActivity& InPackageActivity, FConcertSyncPackageEventData& InPackageActivityEvent, const bool bMetaDataOnly = false);

	/**
	 * Get the generic part of an activity from this database.
	 *
	 * @param InActivityId				The ID of the activity to find.
	 * @param OutActivity				The generic part of an activity to populate with the result.
	 *
	 * @return True if the activity was found, false otherwise.
	 */
	bool GetActivity(const int64 InActivityId, FConcertSyncActivity& OutActivity) const;

	/**
	 * Get a connection activity from this database.
	 *
	 * @param InActivityId				The ID of the activity to find.
	 * @param OutConnectionActivity		The connection activity to populate with the result.
	 *
	 * @return True if the connection activity was found, false otherwise.
	 */
	bool GetConnectionActivity(const int64 InActivityId, FConcertSyncConnectionActivity& OutConnectionActivity) const;

	/**
	 * Get a lock activity from this database.
	 *
	 * @param InActivityId				The ID of the activity to find.
	 * @param OutLockActivity			The lock activity to populate with the result.
	 *
	 * @return True if the lock activity was found, false otherwise.
	 */
	bool GetLockActivity(const int64 InActivityId, FConcertSyncLockActivity& OutLockActivity) const;

	/**
	 * Get a transaction activity from this database.
	 *
	 * @param InActivityId				The ID of the activity to find.
	 * @param OutTransactionActivity	The transaction activity to populate with the result.
	 *
	 * @return True if the transaction activity was found, false otherwise.
	 */
	bool GetTransactionActivity(const int64 InActivityId, FConcertSyncTransactionActivity& OutTransactionActivity) const;

	/**
	 * Get a package activity from this database.
	 *
	 * @param InActivityId			The ID of the activity to find.
	 * @param PackageActivityFn		A callback invoked with the package activity components (the base activity part and the event part).
	 * @note The package data stream passed to the callback through the FConcertSycnPackageEventData is only valid during the callback.
	 *
	 * @return True if the package activity was found, false otherwise.
	 */
	bool GetPackageActivity(const int64 InActivityId, FConsumePackageActivityFunc PackageActivityFn) const;

	/**
	 * Get the type of an activity in this database.
	 *
	 * @param InActivityId				The ID of the activity to query.
	 * @param OutEventType				The event type to populate with the result.
	 *
	 * @return True if the activity was found, false otherwise.
	 */
	bool GetActivityEventType(const int64 InActivityId, EConcertSyncActivityEventType& OutEventType) const;
	
	/**
	 * Get the generic part of an activity for an event in this database.
	 *
	 * @param InEventId					The ID of the event to find the activity for.
	 * @param InEventType				The type of the event to find the activity for.
	 * @param OutActivity				The generic part of an activity to populate with the result.
	 *
	 * @return True if the activity was found, false otherwise.
	 */
	bool GetActivityForEvent(const int64 InEventId, const EConcertSyncActivityEventType InEventType, FConcertSyncActivity& OutActivity) const;

	/**
	 * Get a connection activity for an event in this database.
	 *
	 * @param InConnectionEventId		The ID of the connection event to find the activity for.
	 * @param OutConnectionActivity		The connection activity to populate with the result.
	 *
	 * @return True if the connection activity was found, false otherwise.
	 */
	bool GetConnectionActivityForEvent(const int64 InConnectionEventId, FConcertSyncConnectionActivity& OutConnectionActivity) const;

	/**
	 * Get a lock activity for an event in this database.
	 *
	 * @param InLockEventId				The ID of the lock event to find the activity for.
	 * @param OutLockActivity			The lock activity to populate with the result.
	 *
	 * @return True if the lock activity was found, false otherwise.
	 */
	bool GetLockActivityForEvent(const int64 InLockEventId, FConcertSyncLockActivity& OutLockActivity) const;

	/**
	 * Get a transaction activity for an event in this database.
	 *
	 * @param InTransactionEventId		The ID of the transaction event to find the activity for.
	 * @param OutTransactionActivity	The transaction activity to populate with the result.
	 *
	 * @return True if the transaction activity was found, false otherwise.
	 */
	bool GetTransactionActivityForEvent(const int64 InTransactionEventId, FConcertSyncTransactionActivity& OutTransactionActivity) const;

	/**
	 * Get a package activity for an event in this database.
	 *
	 * @param InPackageEventId		The ID of the package event to find the activity for.
	 * @param PackageActivityFn		A callback invoked with the package activity components (the base activity part and the event part).
	 * @note The package data stream passed to the callback through the FConcertSycnPackageEventData is only valid during the callback.
	 *
	 * @return True if the package activity was found, false otherwise.
	 */
	bool GetPackageActivityForEvent(const int64 InPackageEventId, FIteratePackageActivityFunc PackageActivityFn) const;

	/**
	 * Enumerate the generic part of the activities in this database.
	 *
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the activities were enumerated without error, false otherwise.
	 */
	bool EnumerateActivities(FIterateActivityFunc InCallback) const;

	/**
	 * Enumerate all the connection activities in this database.
	 *
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the connection activities were enumerated without error, false otherwise.
	 */
	bool EnumerateConnectionActivities(TFunctionRef<bool(FConcertSyncConnectionActivity&&)> InCallback) const;

	/**
	 * Enumerate all the lock activities in this database.
	 *
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the lock activities were enumerated without error, false otherwise.
	 */
	bool EnumerateLockActivities(TFunctionRef<bool(FConcertSyncLockActivity&&)> InCallback) const;

	/**
	 * Enumerate all the transaction activities in this database.
	 *
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the transaction activities were enumerated without error, false otherwise.
	 */
	bool EnumerateTransactionActivities(TFunctionRef<bool(FConcertSyncTransactionActivity&&)> InCallback) const;

	/**
	 * Enumerate all the package activities in this database.
	 *
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the package activities were enumerated without error, false otherwise.
	 */
	bool EnumeratePackageActivities(FIteratePackageActivityFunc InCallback) const;

	/**
	 * Enumerate all the activities in this database of the given type.
	 *
	 * @param InEventType				The type of the event to enumerate the activities for.
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the activities were enumerated without error, false otherwise.
	 */
	bool EnumerateActivitiesForEventType(const EConcertSyncActivityEventType InEventType, FIterateActivityFunc InCallback) const;

	/**
	 * Enumerate all the activities in this database in the given range.
	 *
	 * @param InFirstActivityId			The first activity ID to include in the results.
	 * @param InMaxNumActivities		The maximum number of activities to include in the results.
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the activities were enumerated without error, false otherwise.
	 */
	bool EnumerateActivitiesInRange(const int64 InFirstActivityId, const int64 InMaxNumActivities, FIterateActivityFunc InCallback) const;

	/**
	 * Enumerate the IDs and event types of all the activities in this database.
	 *
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the activities were enumerated without error, false otherwise.
	 */
	bool EnumerateActivityIdsAndEventTypes(TFunctionRef<bool(int64, EConcertSyncActivityEventType)> InCallback) const;

	/**
	 * Enumerate the IDs and event types of the activities in this database in the given range.
	 *
	 * @param InFirstActivityId			The first activity ID to include in the results.
	 * @param InMaxNumActivities		The maximum number of activities to include in the results.
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the activities were enumerated without error, false otherwise.
	 */
	bool EnumerateActivityIdsAndEventTypesInRange(const int64 InFirstActivityId, const int64 InMaxNumActivities, TFunctionRef<bool(int64, EConcertSyncActivityEventType)> InCallback) const;

	/**
	 * Enumerate the IDs, event types and flags of the activities in this database in the given range.
	 *
	 * @param InFirstActivityId			The first activity ID to include in the results.
	 * @param InMaxNumActivities		The maximum number of activities to include in the results.
	 * @param InCallback				Callback invoked for each activity; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the activities were enumerated without error, false otherwise.
	 */
	bool EnumerateActivityIdsWithEventTypesAndFlagsInRange(const int64 InFirstActivityId, const int64 InMaxNumActivities, TFunctionRef<bool(int64, EConcertSyncActivityEventType, EConcertSyncActivityFlags)> InCallback) const;
	
	/**
	 * Get the maximum ID of the activities in this database.
	 *
	 * @param OutActivityId				The activity ID to populate with the result.
	 *
	 * @return True if the activity ID was resolved, false otherwise.
	 */
	bool GetActivityMaxId(int64& OutActivityId) const;

	/**
	 * Set an endpoint in this database, creating or replacing it.
	 *
	 * @param InEndpointId				The ID of the endpoint to set.
	 * @param InEndpointData			The endpoint data to set.
	 *
	 * @return True if the endpoint was set, false otherwise.
	 */
	bool SetEndpoint(const FGuid& InEndpointId, const FConcertSyncEndpointData& InEndpointData);

	/**
	 * Get an endpoint from this database.
	 *
	 * @param InEndpointId				The ID of the endpoint to find.
	 * @param OutEndpointData			The endpoint data to populate with the result.
	 *
	 * @return True if the endpoint was found, false otherwise.
	 */
	bool GetEndpoint(const FGuid& InEndpointId, FConcertSyncEndpointData& OutEndpointData) const;

	/**
	 * Enumerate all the endpoints in this database.
	 *
	 * @param InCallback				Callback invoked for each endpoint; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the endpoints were enumerated without error, false otherwise.
	 */
	bool EnumerateEndpoints(TFunctionRef<bool(FConcertSyncEndpointIdAndData&&)> InCallback) const;

	/**
	 * Enumerate all the endpoint IDs in this database.
	 *
	 * @param InCallback				Callback invoked for each endpoint ID; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the endpoint IDs were enumerated without error, false otherwise.
	 */
	bool EnumerateEndpointIds(TFunctionRef<bool(FGuid)> InCallback) const;

	/**
	 * Get a connection event from this database.
	 *
	 * @param InConnectionEventId		The ID of the connection event to find.
	 * @param OutConnectionEvent		The connection event to populate with the result.
	 *
	 * @return True if the connection event was found, false otherwise.
	 */
	bool GetConnectionEvent(const int64 InConnectionEventId, FConcertSyncConnectionEvent& OutConnectionEvent) const;

	/**
	 * Get a lock event from this database.
	 *
	 * @param InLockEventId				The ID of the lock event to find.
	 * @param OutLockEvent				The lock event to populate with the result.
	 *
	 * @return True if the lock event was found, false otherwise.
	 */
	bool GetLockEvent(const int64 InLockEventId, FConcertSyncLockEvent& OutLockEvent) const;

	/**
	 * Get a transaction event from this database.
	 *
	 * @param InTransactionEventId		The ID of the transaction event to find.
	 * @param OutTransactionEvent		The transaction event to populate with the result.
	 * @param InMetaDataOnly			True to only get the meta-data for the transaction, and skip the bulk of the data.
	 *
	 * @return True if the transaction event was found, false otherwise.
	 */
	bool GetTransactionEvent(const int64 InTransactionEventId, FConcertSyncTransactionEvent& OutTransactionEvent, const bool InMetaDataOnly = false) const;

	/**
	 * Get the maximum ID of the transaction events in this database.
	 *
	 * @param OutTransactionEventId		The transaction event ID to populate with the result.
	 *
	 * @return True if the transaction event ID was resolved, false otherwise.
	 */
	bool GetTransactionMaxEventId(int64& OutTransactionEventId) const;

	/**
	 * Check whether the given transaction event ID is currently for a live transaction event.
	 *
	 * @param InTransactionEventId		The ID of the transaction event to check.
	 * @param OutIsLive					Bool to populate with the result.
	 *
	 * @return True if the transaction event ID was queried successfully, false otherwise.
	 */
	bool IsLiveTransactionEvent(const int64 InTransactionEventId, bool& OutIsLive) const;

	/**
	 * Get the IDs of any live transaction events.
	 *
	 * @param OutTransactionEventIds	The array of transaction event IDs to populate with the result (will be sorted in ascending order).
	 *
	 * @return True if the live transaction event IDs were resolved, false otherwise.
	 */
	bool GetLiveTransactionEventIds(TArray<int64>& OutTransactionEventIds) const;

	/**
	 * Get the IDs of any live transaction events for the given package name.
	 *
	 * @param InPackageName				The name of the package to get the live transaction event IDs for.
	 * @param OutTransactionEventIds	The array of transaction event IDs to populate with the result (will be sorted in ascending order).
	 *
	 * @return True if the live transaction event IDs were resolved, false otherwise.
	 */
	bool GetLiveTransactionEventIdsForPackage(const FName InPackageName, TArray<int64>& OutTransactionEventIds) const;

	/**
	 * Get if a Package has any live transactions
	 *
	 * @param InPackageName				The name of the package to check if it has live transactions.
	 * @param OutHasLiveTransaction		Bool to populate with the result.
	 *
	 * @return True if query was resolved correctly, false otherwise.
	 */
	bool PackageHasLiveTransactions(const FName InPackageName, bool& OutHasLiveTransaction) const;

	/**
	 * Enumerate the IDs of any live transaction events for the given package name.
	 *
	 * @param InPackageName				The name of the package to get the live transaction event IDs for.
	 * @param InCallback				Callback invoked for each transaction event ID; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the live transaction event IDs were enumerated without error, false otherwise.
	 */
	bool EnumerateLiveTransactionEventIdsForPackage(const FName InPackageName, TFunctionRef<bool(int64)> InCallback) const;

	/**
	 * Enumerate the names of of any packages that have live transaction events.
	 *
	 * @param InCallback				Callback invoked for each package name; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the package names were enumerated without error, false otherwise.
	 */
	bool EnumeratePackageNamesWithLiveTransactions(TFunctionRef<bool(FName)> InCallback) const;

	/**
	 * Add a new dummy package event to this database, assigning it a package event ID.
	 * @note These are use to fence live transactions without generating a full activity entry (eg, when saving packages locally on the client during a persist operation).
	 *
	 * @param InPackageName				The name of the package to add the event for.
	 * @param OutPackageEventId			Populated with the ID of the package event in the database.
	 *
	 * @return True if the package event was added, false otherwise.
	 */
	bool AddDummyPackageEvent(const FName InPackageName, int64& OutPackageEventId);

	/**
	 * Get a package event meta data (omitting the package data itself) from this database.
	 *
	 * @param InPackageEventId		The ID of the package event to find.
	 * @param OuptPackageRevision	The package revision number.
	 * @param OutPackageInfo		The Package info.
	 *
	 * @return True if the package event was found, false otherwise.
	 */
	bool GetPackageEventMetaData(const int64 InPackageEventId, int64& OuptPackageRevision, FConcertPackageInfo& OutPackageInfo) const;
	
	/**
	 * Get a package event from this database.
	 *
	 * @param InPackageEventId			The ID of the package event to find.
	 * @param PackageEventFn			The callback invoked with the package event containing the event meta data and the package data if available.
	 *
	 * @return True if the package event was found, false otherwise.
	 */
	bool GetPackageEvent(const int64 InPackageEventId, const TFunctionRef<void(FConcertSyncPackageEventData&)>& PackageEventFn) const;

	/**
	 * Enumerate package names for packages with a head revision (at least one package event)
	 *
	 * @param InCallback				Callback invoked for each package name; return true to continue enumeration, or false to stop.
	 * @param IgnorePersisted			Will skip enumeration of packages which head revision have been persisted.
	 *
	 * @return True if the package data was enumerated without error, false otherwise.
	 */
	bool EnumeratePackageNamesWithHeadRevision(TFunctionRef<bool(FName)> InCallback, bool IgnorePersisted) const;

	/**
	 * Enumerate the head revision package data for all packages in this database.
	 *
	 * @param InCallback				Callback invoked for each package; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the package data was enumerated without error, false otherwise.
	 */
	bool EnumerateHeadRevisionPackageData(TFunctionRef<bool(const FConcertPackageInfo&, FConcertPackageDataStream&)> InCallback) const;

	/**
	 * Get the data from this database for the given package name for the given revision.
	 *
	 * @param InPackageName				The name of the package to get the head revision for.
	 * @param OutPackageInfo			The package info to populate with the result.
	 * @param InPackageRevision			The revision of the package to get the data for, or null to get the head revision.
	 *
	 * @return True if package data could be found for the given revision, false otherwise.
	 */
	bool GetPackageInfoForRevision(const FName InPackageName, FConcertPackageInfo& OutPackageInfo, const int64* InPackageRevision = nullptr) const;
	
	/**
	 * Get the data from this database for the given package name for the given revision.
	 *
	 * @param InPackageName				The name of the package to get the head revision for.
	 * @param InCallback				A callback invoked with the package info and package data.
	 * @param InPackageRevision			The revision of the package to get the data for, or null to get the head revision.
	 *
	 * @return True if package data could be found for the given revision, false otherwise.
	 */
	bool GetPackageDataForRevision(const FName InPackageName, const TFunctionRef<void(const FConcertPackageInfo&, FConcertPackageDataStream&)>& InCallback, const int64* InPackageRevision = nullptr) const;

	/**
	 * Gets the package size in bytes of a package
	 *
	 * @param InPackageName				The name of the package to get the head revision for.
	 * @param InPackageRevision			The revision of the package to get the data for, or null to get the head revision.
	 *
	 * @return True if package data could be found for the given revision, false otherwise.
	 */
	TOptional<int64> GetPackageSizeForRevision(const FName InPackageName, const int64* InPackageRevision = nullptr) const;
	
	/**
	 * Get the head revision in this database for the given package name.
	 *
	 * @param InPackageName				The name of the package to get the head revision for.
	 * @param OutRevision				The package revision to populate with the result.
	 *
	 * @return True if the head revision was resolved, false otherwise.
	 */
	bool GetPackageHeadRevision(const FName InPackageName, int64& OutRevision) const;

	/**
	 * Check whether the given package event ID is currently for the head revision of the package in the event.
	 *
	 * @param InPackageEventId			The ID of the package event to check.
	 * @param OutIsHeadRevision			Bool to populate with the result.
	 *
	 * @return True if the package event ID was queried successfully, false otherwise.
	 */
	bool IsHeadRevisionPackageEvent(const int64 InPackageEventId, bool& OutIsHeadRevision) const;

	/**
	 * Add a package event ID for the head revision to the persist events in this database, if not already existing.
	 *
	 * @param PackageName				The package name to add an event for.
	 * @param OutPersistEventId			Populated with the ID of the persist event in the database.
	 *
	 * @return True if the persist event was added, false otherwise.
	 */
	bool AddPersistEventForHeadRevision(FName InPackageName, int64& OutPersistEventId);

	/**
	 * Update the specified transaction event.
	 * @note The function is meant to update a transaction event that was partially synced to store the corresponding transaction data.
	 * @param InTransactionEventId		The ID of the event to update.
	 * @param InTransactionEvent		The event to store.
	 * @return True if the transaction was updated.
	 */
	bool UpdateTransactionEvent(const int64 InTransactionEventId, const FConcertSyncTransactionEvent& InTransactionEvent);

	/**
	 * Update the specified package event.
	 * @note The function is meant to be update a partially synced package event to store the corresponding package data.
	 * @param InPackageEventId			The ID of the event to update.
	 * @param PackageEvent				The event to store.
	 * @return True if the package was updated.
	 */
	bool UpdatePackageEvent(const int64 InPackageEventId, FConcertSyncPackageEventData& PackageEvent);

	/**
	 * Check Asynchronous Tasks Status
	 */
	void UpdateAsynchronousTasks();

	/**
	 * Flush any ongoing asynchronous tasks.
	 */
	void FlushAsynchronousTasks();

	FOnActivityProduced& OnActivityProduced() { return ActivityProducedEvent; }
	
private:

	using FProcessPackageRequest = TFunctionRef<bool(const FConcertPackageInfo& PackageInfo, const FString& DataFilename)>;
	/** Helper function which obtains package information and passes it to HandleFunc. */
	bool HandleRequestPackageRequest(const FName InPackageName, const int64* InPackageRevision, FProcessPackageRequest HandleFunc) const;

	/** Helper functions that for getting a package revision for an optional package revision argument */
	TOptional<int64> GetSpecifiedOrHeadPackageRevision(FName InPackageName, const int64* InPackageRevision) const;
	
	/**
	 * Schedule an asynchronous write for the given Package Stream.  The stream must be in-memory. File sharing
	 * asynchronous write is not supported.
	 *
	 * @param InDstPackageBlobPathName  Full path of the destination package.
	 * @param InPackageDataStream       The package data stream.
	 **/
	void ScheduleAsyncWrite(const FString& InDstPackageBlobPathname, FConcertPackageDataStream& InPackageDataStream);

	/**
	 * Set the active ignored state for the given activity.
	 *
	 * @param InActivityId				The ID of the activity to update.
	 * @param InIsIgnored				True if this activity should be ignored, false otherwise.
	 *
	 * @return True if the ignored state was set, false otherwise.
	 */
	bool SetActivityIgnoredState(const int64 InActivityId, const bool InIsIgnored);

	/**
	 * Add a new connection event to this database, assigning it a connection event ID.
	 *
	 * @param InConnectionEvent			The connection event to add.
	 * @param OutConnectionEventId		Populated with the ID of the connection event in the database.
	 *
	 * @return True if the connection event was added, false otherwise.
	 */
	bool AddConnectionEvent(const FConcertSyncConnectionEvent& InConnectionEvent, int64& OutConnectionEventId);

	/**
	 * Set a connection event in this database, creating or replacing it.
	 *
	 * @param InConnectionEventId		The ID of the connection event to set.
	 * @param InConnectionEvent			The connection event to set.
	 *
	 * @return True if the connection event was set, false otherwise.
	 */
	bool SetConnectionEvent(const int64 InConnectionEventId, const FConcertSyncConnectionEvent& InConnectionEvent);

	/**
	 * Add a new lock event to this database, assigning it a lock event ID.
	 *
	 * @param InLockEvent				The lock event to add.
	 * @param OutLockEventId			Populated with the ID of the lock event in the database.
	 *
	 * @return True if the lock event was added, false otherwise.
	 */
	bool AddLockEvent(const FConcertSyncLockEvent& InLockEvent, int64& OutLockEventId);

	/**
	 * Set a lock event in this database, creating or replacing it.
	 *
	 * @param InLockEventId				The ID of the lock event to set.
	 * @param InLockEvent				The lock event to set.
	 *
	 * @return True if the lock event was set, false otherwise.
	 */
	bool SetLockEvent(const int64 InLockEventId, const FConcertSyncLockEvent& InLockEvent);

	/**
	 * Add a new transaction event to this database, assigning it a transaction event ID.
	 *
	 * @param InTransactionEvent		The transaction event to add.
	 * @param OutTransactionEventId		Populated with the ID of the transaction event in the database.
	 *
	 * @return True if the transaction event was added, false otherwise.
	 */
	bool AddTransactionEvent(const FConcertSyncTransactionEvent& InTransactionEvent, int64& OutTransactionEventId);

	/**
	 * Set a transaction event in this database, creating or replacing it.
	 *
	 * @param InTransactionEventId		The ID of the transaction event to set.
	 * @param InTransactionEvent		The transaction event to set.
	 * @param bMetaDataOnly				True to store the meta data only, omitting the transaction data.
	 *
	 * @return True if the transaction event was set, false otherwise.
	 */
	bool SetTransactionEvent(const int64 InTransactionEventId, const FConcertSyncTransactionEvent& InTransactionEvent, const bool bMetaDataOnly = false);

	/**
	 * Enumerate the IDs of any live transaction events for the given package name ID.
	 *
	 * @param InPackageNameId			The package name ID to get the live transaction event IDs for.
	 * @param InCallback				Callback invoked for each transaction event ID; return true to continue enumeration, or false to stop.
	 *
	 * @return True if the live transaction event IDs were enumerated without error, false otherwise.
	 */
	bool EnumerateLiveTransactionEventIdsForPackage(const int64 InPackageNameId, TFunctionRef<bool(int64)> InCallback) const;

	/**
	 * Add a new package event to this database, assigning it a package event ID.
	 *
	 * @param InPackageInfo				The package info to add for this event.
	 * @param InPackageDataStream		The package data to add for this event (may be empty if the event doesn't carry package data).
	 * @param OutPackageEventId			Populated with the ID of the package event in the database.
	 *
	 * @return True if the package event was added, false otherwise.
	 */
	bool AddPackageEvent(const FConcertPackageInfo& InPackageInfo, FConcertPackageDataStream& InPackageDataStream, int64& OutPackageEventId);

	/**
	 * Set a package event in this database, creating or replacing it.
	 *
	 * @param InPackageEventId			The ID of the package event to set.
	 * @param PackageRevision			The package revision number.
	 * @param PackageInfo				The package info.
	 * @param InPackageDataStream		The package data stream or null to set only the package meta data (revision and package info).
	 *
	 * @return True if the package event was set, false otherwise.
	 */
	bool SetPackageEvent(const int64 InPackageEventId, int64 PackageRevision, const FConcertPackageInfo& PackageInfo, FConcertPackageDataStream* InPackageDataStream);

	/**
	 * Get the maximum ID of the package events in this database.
	 *
	 * @param OutPackageId				The package event ID to populate with the result.
	 *
	 * @return True if the package event ID was resolved, false otherwise.
	 */
	bool GetPackageMaxEventId(int64& OutPackageEventId) const;

	/**
	 * Get the object path name from this database that matches the given object name ID.
	 *
	 * @param InObjectNameId			The ID of the object path name to get the result for.
	 * @param OutObjectPathName			The object path name to populate with the result.
	 *
	 * @return True if the object path name was resolved, false otherwise.
	 */
	bool GetObjectPathName(const int64 InObjectNameId, FName& OutObjectPathName) const;

	/**
	 * Get the object name ID from this database that matches the given object path name.
	 *
	 * @param InObjectPathName			The object path name to get the result for.
	 * @param OutObjectNameId			The object name ID to populate with the result.
	 *
	 * @return True if the object name ID name was resolved, false otherwise.
	 */
	bool GetObjectNameId(const FName InObjectPathName, int64& OutObjectNameId) const;

	/**
	 * Ensure that an object name ID in this database matches the given object path name, creating it if required.
	 *
	 * @param InObjectPathName			The object path name to get the result for.
	 * @param OutObjectNameId			The object name ID to populate with the result.
	 *
	 * @return True if the object name ID name was resolved, false otherwise.
	 */
	bool EnsureObjectNameId(const FName InObjectPathName, int64& OutObjectNameId);

	/**
	 * Get the package name from this database that matches the given package name ID.
	 *
	 * @param InPackageNameId			The ID of the package name to get the result for.
	 * @param OutPackageName			The package name to populate with the result.
	 *
	 * @return True if the package name was resolved, false otherwise.
	 */
	bool GetPackageName(const int64 InPackageNameId, FName& OutPackageName) const;

	/**
	 * Get the package name ID from this database that matches the given package name.
	 *
	 * @param InPackageName				The package name to get the result for.
	 * @param OutPackageNameId			The package name ID to populate with the result.
	 *
	 * @return True if the package name ID name was resolved, false otherwise.
	 */
	bool GetPackageNameId(const FName InPackageName, int64& OutPackageNameId) const;

	/**
	 * Ensure that a package name ID in this database matches the given package name, creating it if required.
	 *
	 * @param InPackageName				The package name to get the result for.
	 * @param OutPackageNameId			The package name ID to populate with the result.
	 *
	 * @return True if the package name ID name was resolved, false otherwise.
	 */
	bool EnsurePackageNameId(const FName InPackageName, int64& OutPackageNameId);

	/**
	 * Map the resource names so that they have an association with the given lock event ID in this database.
	 *
	 * @param InLockEventId				The lock event ID to map resource references to.
	 * @param InResourceNames			The array of resource names to map.
	 *
	 * @return True if the resource name references were mapped, false otherwise.
	 */
	bool MapResourceNamesForLock(const int64 InLockEventId, const TArray<FName>& InResourceNames);

	/**
	 * Map the package names in the given transaction so that they have an association with the given transaction event ID in this database.
	 *
	 * @param InTransactionEventId		The transaction event ID to map package references to.
	 * @param InTransactionEvent		The transaction containing potential package references.
	 *
	 * @return True if the package name references were mapped, false otherwise.
	 */
	bool MapPackageNamesForTransaction(const int64 InTransactionEventId, const FConcertTransactionFinalizedEvent& InTransactionEvent);

	/**
	 * Map the object path names in the given transaction so that they have an association with the given transaction event ID in this database.
	 *
	 * @param InTransactionEventId		The transaction event ID to map object references to.
	 * @param InTransactionEvent		The transaction containing potential object references.
	 *
	 * @return True if the object path name references were mapped, false otherwise.
	 */
	bool MapObjectNamesForTransaction(const int64 InTransactionEventId, const FConcertTransactionFinalizedEvent& InTransactionEvent);

	/**
	 * Save and cache the given transaction data with the given filename.
	 *
	 * @param InTransactionFilename		The filename to save the transaction data as.
	 * @param InTransaction				The transaction data to save.
	 *
	 * @return True if the transaction data was saved, false otherwise.
	 */
	bool SaveTransaction(const FString& InTransactionFilename, const FStructOnScope& InTransaction) const;

	/**
	 * Load and cache the transaction data for the given filename.
	 *
	 * @param InTransactionFilename		The filename to load the transaction data from.
	 * @param OutTransaction			The transaction data to populate with the result.
	 *
	 * @return True if the transaction data was loaded, false otherwise.
	 */
	bool LoadTransaction(const FString& InTransactionFilename, FStructOnScope& OutTransaction) const;

	/**
	 * Save and cache the given package data with the given filename.
	 *
	 * @param InDstPackageBlobPathname	The pathname to save the package data as. The package data may or may not be compressed.
	 * @param InPackageInfo				The package info to save.
	 * @param InPackageDataStream		The package data to save.
	 *
	 * @return True if the package data was saved, false otherwise.
	 */
	bool SavePackage(const FString& InDstPackageBlobPathname, const FConcertPackageInfo& InPackageInfo, FConcertPackageDataStream& InPackageDataStream);

	/**
	 * Load the package data for the given filename.
	 *
	 * @param InPackageBlobFilename			The blob filename containing the package data to extract.
	 * @param PackageDataStreamFn			Callback invoked to let the caller stream the package data.
	 * @note The archive passed as output is only valid during the callback, its position is set at the beginning of the data (not necessarily zero) and may contain data passed the supplied size.
	 * @return True if the package data was loaded, false otherwise.
	 */
	bool LoadPackage(const FString& InPackageBlobFilename, const TFunctionRef<void(FConcertPackageDataStream&)>& PackageDataStreamFn) const;
	
	/** Called when an activity is produced */
	FOnActivityProduced ActivityProducedEvent;
	
	/** Root path to store all session data under */
	FString SessionPath;

	/** In-memory cache of on-disk transaction files */
	TUniquePtr<FConcertFileCache> TransactionFileCache;

	/** In-memory cache of on-disk package files */
	TUniquePtr<FConcertFileCache> PackageFileCache;

	/** Prepared statements for the currently open database */
	TUniquePtr<FConcertSyncSessionDatabaseStatements> Statements;

	/** Internal SQLite database */
	TUniquePtr<FSQLiteDatabase> Database;

	TPimplPtr<struct FDeferredLargePackageIOImpl> DeferredLargePackageIOPtr;
};

namespace ConcertSyncSessionDatabaseFilterUtil
{

	/**
	 * Check to see whether the transaction event with the given ID passes the given filter.
	 *
	 * @param InTransactionEventId		The ID of the transaction event to query for.
	 * @param InSessionFilter			The session filter used to query the event against.
	 * @param InDatabase				The database to query data from.
	 *
	 * @return True if the transaction event passes the filter, false otherwise.
	 */
	CONCERTSYNCCORE_API bool TransactionEventPassesFilter(const int64 InTransactionEventId, const FConcertSessionFilter& InSessionFilter, const FConcertSyncSessionDatabase& InDatabase);

	/**
	 * Check to see whether the package event with the given ID passes the given filter.
	 *
	 * @param InPackageEventId			The ID of the package event to query for.
	 * @param InSessionFilter			The session filter used to query the event against.
	 * @param InDatabase				The database to query data from.
	 *
	 * @return True if the package event passes the filter, false otherwise.
	 */
	CONCERTSYNCCORE_API bool PackageEventPassesFilter(const int64 InPackageEventId, const FConcertSessionFilter& InSessionFilter, const FConcertSyncSessionDatabase& InDatabase);

} // namespace ConcertSyncSessionDatabaseFilterUtil
