// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ConcertMessageData.h"
#include "HAL/Platform.h"
#include "Misc/Optional.h"
#include "Templates/Function.h"

struct FConcertReplicatedObjectId;

namespace UE::ConcertSyncCore
{
	/**
	 * Responsible for obtaining replication data.
	 * 
	 * Implementation examples:
	 * - serialize the object path (client),
	 * - cache received object data and return it (client & server)
	 */
	class IReplicationDataSource
	{
	public:
		
		/** Iterates the objects that must be processed for replication (the result of ExtractReplicationDataForObject MAY return something new). */
		virtual void ForEachPendingObject(TFunctionRef<void(const FConcertReplicatedObjectId&)> ProcessItemFunc) const = 0;
		/** @return The number of objects ForEachPendingObject would iterate. Can be used, e.g. for Reserve()-ing a container. */
		virtual int32 NumObjects() const = 0;

		/**
		 * Extracts data for Object. Object must have been previously returned by ForEachPendingObject.
		 * This "dequeues" Object so it will not be mentioned by ForEachPendingObject again until it is again marked as "dirty".
		 *
		 * The call to ProcessCopyable / ProcessMoveable may be skipped if there is no new data to send.
		 * Either ProcessCopyable or ProcessMoveable will be called, never both, and it will be called at most once.
		 * @param Object The object for which to obtain data
		 * @param ProcessCopyable Callback if the event was retrieved and not owned by this IReplicationDataSource (hence not being moveable).
		 * @param ProcessMoveable Callback if the event was just constructed (and hence can be moved)
		 * 
		 * @return Whether successful. False indicates the call was invalid to make (ForEachPendingObject did not return Object). True indicates success, even if ProcessCopyable was not called.		
		 */
		virtual bool ExtractReplicationDataForObject(
			const FConcertReplicatedObjectId& Object,
			TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable,
			TFunctionRef<void(FConcertSessionSerializedPayload&& Payload)> ProcessMoveable
			) = 0;
		/** Util version for callers that only want to read and do not want to store the payload. */
		bool ExtractReplicationDataForObject(const FConcertReplicatedObjectId& Object, TFunctionRef<void(const FConcertSessionSerializedPayload& Payload)> ProcessCopyable)
		{
			return ExtractReplicationDataForObject(Object, ProcessCopyable, [&ProcessCopyable](FConcertSessionSerializedPayload&& Payload){ ProcessCopyable(Payload); });
		}

		/** Util function for add the contained objects to a container (TArray / TSet). Container must have Reserve and Add functions.  */
		template<typename TContainerType>
		void AddEachObjectToContainer(TContainerType& OutContainer) const;
		
		virtual ~IReplicationDataSource() = default;
	};

	template <typename TContainerType>
	void IReplicationDataSource::AddEachObjectToContainer(TContainerType& OutContainer) const
	{
		OutContainer.Reserve(OutContainer.Num() + NumObjects());
		ForEachPendingObject([&OutContainer](const FSoftObjectPath& Item) mutable
		{
			OutContainer.Add(Item);
		});
	}
}
