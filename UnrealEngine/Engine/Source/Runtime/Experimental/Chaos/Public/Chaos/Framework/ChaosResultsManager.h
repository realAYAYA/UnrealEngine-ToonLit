// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PullPhysicsDataImp.h"

namespace Chaos
{
	class FChaosMarshallingManager;
	
	extern FRealSingle SecondChannelDelay;
	extern int32 DefaultNumActiveChannels;

	struct CHAOS_API FChaosRigidInterpolationData
	{
		FDirtyRigidParticleData Prev;
		FDirtyRigidParticleData Next;
	};

	struct CHAOS_API FChaosGeometryCollectionInterpolationData
	{
		FDirtyGeometryCollectionData Prev;
		FDirtyGeometryCollectionData Next;
	};
	
	struct CHAOS_API FChaosInterpolationResults
	{
		FChaosInterpolationResults()
			: Prev(nullptr)
			, Next(nullptr)
		{
		}

		FChaosInterpolationResults(const FChaosInterpolationResults& Other) = delete;
		FChaosInterpolationResults(FChaosInterpolationResults&& Other)
			: RigidInterpolations(MoveTemp(Other.RigidInterpolations))
			, GeometryCollectionInterpolations(MoveTemp(Other.GeometryCollectionInterpolations))
			, Prev(Other.Prev)
			, Next(Other.Next)
			, Alpha(Other.Alpha)
		{
			Other.Prev = nullptr;
			Other.Next = nullptr;
		}

		void Reset();
		
		TArray<FChaosRigidInterpolationData> RigidInterpolations;
		TArray<FChaosGeometryCollectionInterpolationData> GeometryCollectionInterpolations;
		FPullPhysicsData* Prev;
		FPullPhysicsData* Next;
		FRealSingle Alpha;
	};

	struct FChaosResultsChannel;

	class CHAOS_API FChaosResultsManager
	{
	public:
		FChaosResultsManager(FChaosMarshallingManager& InMarshallingManager);
		FChaosResultsManager(const FChaosResultsManager& Other) = delete;
		FChaosResultsManager(FChaosResultsManager&& Other) = default;
		
		~FChaosResultsManager();

		const FChaosInterpolationResults& PullSyncPhysicsResults_External();
		TArray<const FChaosInterpolationResults*> PullAsyncPhysicsResults_External(const FReal ResultsTime);

		void RemoveProxy_External(FSingleParticlePhysicsProxy* Proxy);

	private:

		friend FChaosResultsChannel;

		FPullPhysicsData* PopPullData_External(int32 ChannelIdx);
		void FreePullData_External(FPullPhysicsData* PullData, int32 ChannelIdx);

		TArray<FChaosResultsChannel*> Channels;
		FChaosMarshallingManager& MarshallingManager;

		static constexpr int32 MaxNumChannels = 2;

		struct FPullDataQueueInfo
		{
			FPullDataQueueInfo(FPullPhysicsData* Data, int32 NumActiveChannels)
			: PullData(Data)
			{
				for(int32 Idx = 0; Idx < NumActiveChannels; ++Idx)
				{
					bHasPopped[Idx] = false;
					bPendingFree[Idx] = false;
				}

				//inactive channels are treated like they've already consumed the data
				for (int32 Idx = NumActiveChannels; Idx < MaxNumChannels; ++Idx)
				{
					bHasPopped[Idx] = true;
					bPendingFree[Idx] = true;
				}
			}

			FPullDataQueueInfo(FPullDataQueueInfo& Other) = delete;
			FPullDataQueueInfo(FPullDataQueueInfo&& Other) = default;

			FPullPhysicsData* PullData;
			bool bHasPopped[MaxNumChannels];
			bool bPendingFree[MaxNumChannels];
		};

		TArray<FPullDataQueueInfo> InternalQueue;
		int32 NumActiveChannels;
		TArray<FRealSingle> PerChannelTimeDelay;
	};
}
