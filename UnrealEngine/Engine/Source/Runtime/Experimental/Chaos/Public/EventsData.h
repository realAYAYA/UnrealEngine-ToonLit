// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/ExternalCollisionData.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Framework/PhysicsProxy.h"

class UPrimitiveComponent;

namespace Chaos
{
	// base class for data that requires time of creation to be recorded
	struct FTimeResource
	{
		FTimeResource() : TimeCreated(-TNumericLimits<FReal>::Max()) {}
		FReal TimeCreated;
	};

	typedef TArray<FCollidingData> FCollisionDataArray;
	typedef TArray<FBreakingData> FBreakingDataArray;
	typedef TArray<FTrailingData> FTrailingDataArray;
	typedef TArray<FRemovalData> FRemovalDataArray;
	typedef TArray<FSleepingData> FSleepingDataArray;
	typedef TArray<FCrumblingData> FCrumblingDataArray;

	/* Common */

	/* Maps PhysicsProxy to list of indices in events arrays 
	 * - for looking up say all collisions a particular physics object had this frame
	 */
	struct FIndicesByPhysicsProxy : public FTimeResource
	{
		FIndicesByPhysicsProxy()
			: PhysicsProxyToIndicesMap(TMap<IPhysicsProxyBase*, TArray<int32>>())
		{}

		void Reset()
		{
			PhysicsProxyToIndicesMap.Reset();
		}

		TMap<IPhysicsProxyBase*, TArray<int32>> PhysicsProxyToIndicesMap; // PhysicsProxy -> Indices in Events arrays
	};

	/* Collision */

	/*   
	 * All the collision events for one frame time stamped with the time for that frame
	 */
	struct FAllCollisionData : public FTimeResource
	{
		FAllCollisionData() : AllCollisionsArray(FCollisionDataArray()) {}

		void Reset()
		{
			AllCollisionsArray.Reset();
		}

		FCollisionDataArray AllCollisionsArray;
	};

	struct FCollisionEventData
	{
		FCollisionEventData() {}

		void Reset()
		{
			PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap.Reset();
			CollisionData.Reset();
		}

		FAllCollisionData CollisionData;
		FIndicesByPhysicsProxy PhysicsProxyToCollisionIndices;
	};

	/* Breaking */

	/*
	 * All the breaking events for one frame time stamped with the time for that frame
	 */
	struct FAllBreakingData : public FTimeResource
	{
		FAllBreakingData() : AllBreakingsArray(FBreakingDataArray()), bHasGlobalEvent(false) {}

		void Reset()
		{
			AllBreakingsArray.Reset();
			bHasGlobalEvent = false;
		}

		FBreakingDataArray AllBreakingsArray;
		bool bHasGlobalEvent;
	};

	struct FBreakingEventData
	{
		FBreakingEventData() {}

		void Reset()
		{
			BreakingData.Reset();
			PhysicsProxyToBreakingIndices.PhysicsProxyToIndicesMap.Reset();
		}

		FAllBreakingData BreakingData;
		FIndicesByPhysicsProxy PhysicsProxyToBreakingIndices;
	};

	/* Trailing */

	/*
	 * All the trailing events for one frame time stamped with the time for that frame  
	 */
	struct FAllTrailingData : FTimeResource
	{
		FAllTrailingData() : AllTrailingsArray(FTrailingDataArray()) {}

		void Reset()
		{
			AllTrailingsArray.Reset();
		}

		FTrailingDataArray AllTrailingsArray;
	};


	struct FTrailingEventData
	{
		FTrailingEventData() {}

		void Reset()
		{
			TrailingData.Reset();
			PhysicsProxyToTrailingIndices.Reset();
		}

		FAllTrailingData TrailingData;
		FIndicesByPhysicsProxy PhysicsProxyToTrailingIndices;
	};

	/* Removal */

	/*
	 * All the removal events for one frame time stamped with the time for that frame
	 */
	struct FAllRemovalData : FTimeResource
	{
		FAllRemovalData() : AllRemovalArray(FRemovalDataArray()) {}

		void Reset()
		{
			AllRemovalArray.Reset();
		}

		FRemovalDataArray AllRemovalArray;
	};


	struct FRemovalEventData
	{
		FRemovalEventData() {}

		void Reset()
		{
			RemovalData.Reset();
			PhysicsProxyToRemovalIndices.PhysicsProxyToIndicesMap.Reset();
		}

		FAllRemovalData RemovalData;
		FIndicesByPhysicsProxy PhysicsProxyToRemovalIndices;
	};

	struct FSleepingEventData
	{
		FSleepingEventData() {}

		void Reset()
		{
			SleepingData.Reset();
		}

		FSleepingDataArray SleepingData;
	};

	/*
	* All the crumbling events for one frame time stamped with the time for that frame
	*/
	struct FAllCrumblingData : public FTimeResource
	{
		FAllCrumblingData() : AllCrumblingsArray(FCrumblingDataArray()), bHasGlobalEvent(false) {}

		void Reset()
		{
			AllCrumblingsArray.Reset();
			bHasGlobalEvent = false;
		}

		FCrumblingDataArray AllCrumblingsArray;
		bool bHasGlobalEvent;
	};

	struct FCrumblingEventData
	{
		FCrumblingEventData() {}

		void Reset()
		{
			CrumblingData.Reset();
			PhysicsProxyToCrumblingIndices.Reset();
		}

		FORCEINLINE_DEBUGGABLE void Reserve(int32 Num)
		{
			CrumblingData.AllCrumblingsArray.Reserve(Num);
		}
		
		FORCEINLINE_DEBUGGABLE void SetTimeCreated(FReal TimeCreatedIn)
		{
			CrumblingData.TimeCreated = TimeCreatedIn;
		}

		FORCEINLINE_DEBUGGABLE void AddCrumbling(const FCrumblingData& CrumblingToAdd)
		{
			const int32 NewIndex = CrumblingData.AllCrumblingsArray.Emplace(CrumblingToAdd);
			TArray<int32>& Indices = PhysicsProxyToCrumblingIndices.PhysicsProxyToIndicesMap.FindOrAdd(CrumblingToAdd.Proxy);
			Indices.Add(NewIndex);
		}
		
		FAllCrumblingData CrumblingData;
		FIndicesByPhysicsProxy PhysicsProxyToCrumblingIndices;
	};

	template<typename PayloadType>
	bool IsEventDataEmpty(const PayloadType* Buffer)
	{
		if (!Buffer)
		{
			return false;
		}

		if constexpr (std::is_same_v<PayloadType, FCollisionEventData>)
		{
			return Buffer->CollisionData.AllCollisionsArray.IsEmpty();
		}
		else if constexpr (std::is_same_v<PayloadType, FBreakingEventData>)
		{
			return Buffer->BreakingData.AllBreakingsArray.IsEmpty();
		}
		else if constexpr (std::is_same_v<PayloadType, FTrailingEventData>)
		{
			return Buffer->TrailingData.AllTrailingsArray.IsEmpty();
		}
		else if constexpr (std::is_same_v<PayloadType, FRemovalEventData>)
		{
			return Buffer->RemovalData.AllRemovalArray.IsEmpty();
		}
		else if constexpr (std::is_same_v<PayloadType, FSleepingEventData>)
		{
			return Buffer->SleepingData.IsEmpty();
		}
		else if constexpr (std::is_same_v<PayloadType, FCrumblingEventData>)
		{
			return Buffer->CrumblingData.AllCrumblingsArray.IsEmpty();
		}
		else
		{
			return false;
		}
	}
	
	template<typename PayloadType>
	const TMap<IPhysicsProxyBase*, TArray<int32>>* GetProxyToIndexMap(const PayloadType* Buffer)
	{
		if (!Buffer)
		{
			return nullptr;
		}

		if constexpr (std::is_same_v<PayloadType, FCollisionEventData>)
		{
			return &Buffer->PhysicsProxyToCollisionIndices.PhysicsProxyToIndicesMap;
		}
		else if constexpr (std::is_same_v<PayloadType, FBreakingEventData>)
		{
			return &Buffer->PhysicsProxyToBreakingIndices.PhysicsProxyToIndicesMap;
		}
		else if constexpr (std::is_same_v<PayloadType, FTrailingEventData>)
		{
			return nullptr; //&Buffer->PhysicsProxyToTrailingIndices.PhysicsProxyToIndicesMap;
		}
		else if constexpr (std::is_same_v<PayloadType, FRemovalEventData>)
		{
			return &Buffer->PhysicsProxyToRemovalIndices.PhysicsProxyToIndicesMap;
		}
		else if constexpr (std::is_same_v<PayloadType, FSleepingEventData>)
		{
			return nullptr;
		}
		else if constexpr (std::is_same_v<PayloadType, FCrumblingEventData>)
		{
			return &Buffer->PhysicsProxyToCrumblingIndices.PhysicsProxyToIndicesMap;
		}
		else
		{
			return nullptr;
		}
	}
}
