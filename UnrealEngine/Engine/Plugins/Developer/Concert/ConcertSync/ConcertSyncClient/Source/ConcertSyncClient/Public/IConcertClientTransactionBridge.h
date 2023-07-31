// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "UObject/StructOnScope.h"
#include "ConcertTransactionEvents.h"
#include "IdentifierTable/ConcertIdentifierTable.h"

struct FConcertSessionVersionInfo;
class UObject;
class UPackage;

/**
 * Common data for a transaction.
 */
struct FConcertClientLocalTransactionCommonData
{
	FConcertClientLocalTransactionCommonData(const FText InTransactionTitle, const FGuid& InTransactionId, const FGuid& InOperationId, UObject* InPrimaryObject)
		: TransactionTitle(InTransactionTitle)
		, TransactionId(InTransactionId)
		, OperationId(InOperationId)
		, PrimaryObject(InPrimaryObject)
	{
	}

	FText TransactionTitle;
	FGuid TransactionId;
	FGuid OperationId;
	FWeakObjectPtr PrimaryObject;
	TArray<FName> ModifiedPackages;
	TArray<FConcertObjectId> ExcludedObjectUpdates;
	bool bIsExcluded = false;
};

/**
 * Snapshot data for a transaction.
 */
struct FConcertClientLocalTransactionSnapshotData
{
	TArray<FConcertExportedObject> SnapshotObjectUpdates;
};

/**
 * Finalized data for a transaction.
 */
struct FConcertClientLocalTransactionFinalizedData
{
	FConcertLocalIdentifierTable FinalizedLocalIdentifierTable;
	TArray<FConcertExportedObject> FinalizedObjectUpdates;
	bool bWasCanceled = false;
};

enum class ETransactionNotification
{
	Begin,
	End,
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnApplyTransaction, ETransactionNotification, const bool bIsSnapshot);
DECLARE_DELEGATE_RetVal_TwoParams(ETransactionFilterResult, FTransactionFilterDelegate, UObject*, UPackage*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertClientLocalTransactionSnapshot, const FConcertClientLocalTransactionCommonData&, const FConcertClientLocalTransactionSnapshotData&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnConcertClientLocalTransactionFinalized, const FConcertClientLocalTransactionCommonData&, const FConcertClientLocalTransactionFinalizedData&);

/**
 * Bridge between the editor transaction system and Concert.
 * Deals with converting local ongoing transactions to Concert transaction data, 
 * and applying remote Concert transaction data onto this local instance.
 */
class IConcertClientTransactionBridge
{
public:
	/**
	 * Create a new instance of the concrete implementation of this class. 
	 */
	CONCERTSYNCCLIENT_API static TUniquePtr<IConcertClientTransactionBridge> NewInstance();

	/** Scoped struct to ignore a local transaction */
	struct FScopedIgnoreLocalTransaction : private TGuardValue<bool>
	{
		FScopedIgnoreLocalTransaction(IConcertClientTransactionBridge& InTransactionBridge)
			: TGuardValue(InTransactionBridge.GetIgnoreLocalTransactionsRef(), true)
		{
		}
	};

	virtual ~IConcertClientTransactionBridge() = default;

	/**
	 * Set whether or not to include editor-only properties when serializing object and property changes.
	 * @note This is set to true by default.
	 */
	virtual void SetIncludeEditorOnlyProperties(const bool InIncludeEditorOnlyProperties) = 0;

	/**
	 * Set whether to include non-property object data in updates, or whether to only include property changes.
	 * @note This is set to true by default.
	 */
	virtual void SetIncludeNonPropertyObjectData(const bool InIncludeNonPropertyObjectData) = 0;

	/**
	 * Set whether to include object changes that have been generated via a transaction annotation 
	 * (where possible), or whether to send the entire transaction annotation blob instead.
	 * @note This is set to UConcertSyncConfig::bIncludeAnnotationObjectChanges by default.
	 */
	virtual void SetIncludeAnnotationObjectChanges(const bool InIncludeAnnotationObjectChanges) = 0;

	/**
	 * Called when an ongoing transaction is updated via a snapshot.
	 * @note This is called during end-frame processing.
	 */
	virtual FOnConcertClientLocalTransactionSnapshot& OnLocalTransactionSnapshot() = 0;

	/**
	 * Called when an transaction is finalized.
	 * @note This is called during end-frame processing.
	 */
	virtual FOnConcertClientLocalTransactionFinalized& OnLocalTransactionFinalized() = 0;

	/**
	 * Can we currently apply a remote transaction event to this local instance?
	 * @return True if we can apply a remote transaction, false otherwise.
	 */
	virtual bool CanApplyRemoteTransaction() const = 0;

	/**
	 * Notification of an application of a transaction. This will tell the user if the transaction
	 * originates as a snapshot or is a finalized snapshot message.
	 */
	virtual FOnApplyTransaction& OnApplyTransaction() = 0;

	/**
	 * Apply a remote transaction event to this local instance.
	 * @param InEvent					The event to apply.
	 * @param InVersionInfo				The version information for the serialized data in the event, or null if the event should be serialized using the compiled in version info.
	 * @param InPackagesToProcess		The list of packages to apply changes for, or an empty array to apply all changes.
	 * @param InLocalIdentifierTablePtr The local identifier table for the event data (if any).
	 * @param bIsSnapshot				True if this transaction event was a snapshot rather than a finalized transaction.
	 */
	virtual void ApplyRemoteTransaction(const FConcertTransactionEventBase& InEvent, const FConcertSessionVersionInfo* InVersionInfo, const TArray<FName>& InPackagesToProcess, const FConcertLocalIdentifierTable* InLocalIdentifierTablePtr, const bool bIsSnapshot) = 0;

	/**
	 * Apply a remote transaction event to this local instance.
	 * @param InEvent					The event to apply.
	 * @param InVersionInfo				The version information for the serialized data in the event, or null if the event should be serialized using the compiled in version info.
	 * @param InPackagesToProcess		The list of packages to apply changes for, or an empty array to apply all changes.
	 * @param InLocalIdentifierTablePtr The local identifier table for the event data (if any).
	 * @param bIsSnapshot				True if this transaction event was a snapshot rather than a finalized transaction.
	 * @param ConcertSyncWorldRemapper	Remapper to use in the case the current world is different from the one sent in the transation.
	 */
	virtual void ApplyRemoteTransaction(const FConcertTransactionEventBase& InEvent, const FConcertSessionVersionInfo* InVersionInfo, const TArray<FName>& InPackagesToProcess, const FConcertLocalIdentifierTable* InLocalIdentifierTablePtr, const bool bIsSnapshot, const class FConcertSyncWorldRemapper& ConcertSyncWorldRemapper) = 0;

	/** Callback to register delegate for handling transaction events */
	virtual void RegisterTransactionFilter(FName FilterName, FTransactionFilterDelegate FilterHandle) = 0;

	/** Callback to register delegate for handling transaction events */
	virtual void UnregisterTransactionFilter(FName FilterName) = 0;
protected:
	/**
	 * Function to access the internal bool controlling whether local transactions are currently being tracked.
	 * @note Exists to implement FScopedIgnoreLocalTransaction.
	 */
	virtual bool& GetIgnoreLocalTransactionsRef() = 0;
};
