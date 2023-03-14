// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Framework/MultiBufferResource.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "Containers/Queue.h"

#ifndef WITH_TODO_COLLISION_DISABLE
#define WITH_TODO_COLLISION_DISABLE 0
#endif

namespace Chaos
{
	/**
	 * @brief Flags for user-control over per-particle collision behaviour
	*/
	enum class ECollisionConstraintFlags : uint32
	{
		CCF_None                       = 0x0,
		CCF_BroadPhaseIgnoreCollisions = 0x1,
		CCF_SmoothEdgeCollisions       = 0x2,
		CCF_DummyFlag
	};

	class CHAOS_API FIgnoreCollisionManager
	{
	public:
		using FHandleID = FUniqueIdx;
		using FDeactivationSet = TSet<FUniqueIdx>;
		using FActiveMap UE_DEPRECATED(5.1, "This type is no longer used") = TMap<FHandleID, TArray<FHandleID>>;
		using FPendingMap = TMap<FHandleID, TArray<FHandleID>>;

		struct FStorageData
		{
			FPendingMap PendingActivations;
			FDeactivationSet PendingDeactivations;
			int32 ExternalTimestamp = INDEX_NONE;

			void Reset()
			{
				PendingActivations.Reset();
				PendingDeactivations.Reset();
				ExternalTimestamp = INDEX_NONE;
			}
		};

		FIgnoreCollisionManager()
			: StorageDataProducer(nullptr)
		{
			StorageDataProducer = GetNewStorageData();
		}

		bool ContainsHandle(FHandleID Body0) const;

		bool IgnoresCollision(FHandleID Body0, FHandleID Body1) const;

		int32 NumIgnoredCollision(FHandleID Body0) const;

		void AddIgnoreCollisionsFor(FHandleID Body0, FHandleID Body1);

		/**
		 * Remove an ignore entry for collisions of Body0->Body1
		 * Note, a reversed ignore entry could exist
		 * @param Body0 First body in the collision
		 * @param Body1 Second body in the collision
		 * @return Number of collisions Body0 ignores after the removal
		 */
		int32 RemoveIgnoreCollisionsFor(FHandleID Body0, FHandleID Body1);

		FPendingMap& GetPendingActivationsForGameThread(int32 ExternalTimestamp) 
		{
			if (StorageDataProducer->ExternalTimestamp == INDEX_NONE)
			{
				StorageDataProducer->ExternalTimestamp = ExternalTimestamp;
			}
			else
			{
				ensure(StorageDataProducer->ExternalTimestamp == ExternalTimestamp);
			}

			return StorageDataProducer->PendingActivations;
		}

		FDeactivationSet& GetPendingDeactivationsForGameThread(int32 ExternalTimestamp)
		{
			if (StorageDataProducer->ExternalTimestamp == INDEX_NONE)
			{
				StorageDataProducer->ExternalTimestamp = ExternalTimestamp;
			}
			else
			{
				ensure(StorageDataProducer->ExternalTimestamp == ExternalTimestamp);
			}

			return StorageDataProducer->PendingDeactivations;
		}

		void PushProducerStorageData_External(int32 ExternalTimestamp)
		{
			if (StorageDataProducer->ExternalTimestamp != INDEX_NONE)
			{
				ensure(ExternalTimestamp == StorageDataProducer->ExternalTimestamp);
				StorageDataQueue.Enqueue(StorageDataProducer);
				StorageDataProducer = GetNewStorageData();
			}
		}

		/*
		*
		*/
		void ProcessPendingQueues();

		/*
		*
		*/
		void PopStorageData_Internal(int32 ExternalTimestamp);

	private:

		// Multiple sources can request an ignore pair, so we handle a simple count of
		// ignore requests to ensure we don't prematurely remove an ignore pair
		// #CHAOSTODO replace bi-directional map to pair hash to reduce required storage and dependency
		struct FIgnoreEntry
		{
			FIgnoreEntry() = delete;
			explicit FIgnoreEntry(FHandleID InId)
				: Id(InId)
				, Count(1) // Begin at 1 for convenience when adding entries
			{}

			FHandleID Id;
			int32 Count;
		};

		FStorageData* GetNewStorageData()
		{
			FStorageData* StorageData;
			if (StorageDataFreePool.Dequeue(StorageData))
			{
				return StorageData;
			}

			StorageDataBackingBuffer.Emplace(MakeUnique<FStorageData>());
			return StorageDataBackingBuffer.Last().Get();
		}

		void ReleaseStorageData(FStorageData *InStorageData)
		{
			InStorageData->Reset();
			StorageDataFreePool.Enqueue(InStorageData);
		}

		// Maps collision body0 to a list of other bodies and a count for how many
		// sources have requested for this pair to be ignored
		TMap<FHandleID, TArray<FIgnoreEntry>> IgnoreCollisionsList;

		FPendingMap PendingActivations;
		FDeactivationSet PendingDeactivations;

		// Producer storage data, pending changes written here until pushed into queue.
		FStorageData* StorageDataProducer;

		TQueue<FStorageData*, EQueueMode::Spsc> StorageDataQueue; // Queue of storage data being passed to physics thread
		TQueue<FStorageData*,EQueueMode::Spsc> StorageDataFreePool;	//free pool of storage data
		TArray<TUniquePtr<FStorageData>> StorageDataBackingBuffer;	// Holds unique ptrs for storage data allocation
	};

} // Chaos
