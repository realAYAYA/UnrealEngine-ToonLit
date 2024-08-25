// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IReplicationDataSource.h"
#include "Replication/Data/ObjectIds.h"

#include "Templates/SharedPointer.h"
#include "UObject/SoftObjectPath.h"

struct FConcertObjectReplicationSettings;
struct FConcertObjectInStreamID;

namespace UE::ConcertSyncCore
{
	class IReplicationDataSource;
		
	/**
	 * Params for FObjectReplicationProcessor::ProcessObjects.
	 * Exists as struct so we can add params (e.g. UE-190714) without affecting the signature and can avoid deprecation.
	 */
	struct FProcessObjectsParams
	{
		/** The current frame's delta time */
		float DeltaTime;
		// TODO UE-190714: We should add a time budget.
	};
	
	
	class CONCERTSYNCCORE_API IObjectReplicationProcessor
	{
	public:
		
		/**
		 * Processes all changed objects under the given time budget.
		 * TODO UE-190714: A time budget should be added.
		 */
		virtual void ProcessObjects(const FProcessObjectsParams& Params) = 0;

		virtual ~IObjectReplicationProcessor() = default;
	};


	/**
	 * Responsible for prioritizing a list of objects and processing them.
	 * 
	 * Processing is defined by the subclass. Some example implementations:
	 *  - serialize the object and send (client),
	 *  - find loaded object and apply replication data (client),
	 *  - send object data to interested clients (server)
	 *  
	 * FObjectReplicationProcessor is backed by an IReplicationDataSource, which is a source of replicated object data.
	 */
	class CONCERTSYNCCORE_API FObjectReplicationProcessor
	{
	public:
		
		/**
		 * @param DataSource Source of the data that is to be sent
		 */
		FObjectReplicationProcessor(TSharedRef<IReplicationDataSource> DataSource);
		virtual ~FObjectReplicationProcessor() = default;
		
		virtual void ProcessObjects(const FProcessObjectsParams& Params);
		
	protected:

		struct FObjectProcessArgs
		{
			/** Info about the object to process */
			FConcertReplicatedObjectId ObjectInfo;
		};

		FORCEINLINE IReplicationDataSource& GetDataSource() const { return DataSource.Get(); }
		
		/** Processes the object. */
		virtual void ProcessObject(const FObjectProcessArgs& Args) = 0;

	private:
		
		/** Abstracts where replication data comes from: could be generated (clients) or received (server or client) */
		const TSharedRef<IReplicationDataSource> DataSource;
	};
}
