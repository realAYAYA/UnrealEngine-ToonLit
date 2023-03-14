// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Model/PointTimeline.h"
#include "Runtime/Private/Trace/PoseSearchTraceLogger.h"
#include "TraceServices/Model/AnalysisSession.h"


namespace UE::PoseSearch
{

/** Motion matching state message container */
struct FTraceMotionMatchingStateMessage : FTraceMotionMatchingState, FTraceMessage
{
};
FArchive& operator<<(FArchive& Ar, FTraceMotionMatchingStateMessage& State);

/**
 * Provider to the widgets for pose search functionality, largely mimicking FAnimationProvider
 */
class FTraceProvider : public TraceServices::IProvider
{
public:
	explicit FTraceProvider(TraceServices::IAnalysisSession& InSession);

	/** Get all node ids of motion matching nodes appended to the object's timeline thus far */
	TSet<int32> GetMotionMatchingNodeIds(uint64 InAnimInstanceId) const;

	using FMotionMatchingStateTimeline = TraceServices::ITimeline<FTraceMotionMatchingStateMessage>;
	
	/** Read the node-relative timeline info of the AnimInstance and provide it via callback */
	bool ReadMotionMatchingStateTimeline(uint64 InAnimInstanceId, int32 InNodeId, TFunctionRef<void(const FMotionMatchingStateTimeline&)> Callback) const;
	
	/** Enumerate all node timelines on the AnimInstance and provide them via callback */
	bool EnumerateMotionMatchingStateTimelines(uint64 InAnimInstanceId, TFunctionRef<void(const FMotionMatchingStateTimeline&)> Callback) const;

	/** Append to the timeline on our object */
	void AppendMotionMatchingState(const FTraceMotionMatchingStateMessage& InMessage, double InTime);

	
	static const FName ProviderName;

private:
	/**
	 * Convenience struct for creating timelines per message type.
	 * We store an array of timelines for every possible AnimInstance Id that gets appended.
	 */
	template <class MessageType, class TimelineType = TraceServices::TPointTimeline<MessageType>>
	struct TTimelineStorage
	{
		// Map the timelines as AnimInstanceId -> NodeId -> Timeline
		using FNodeToTimelineMap = TMap<int32, uint32>;
		using FAnimInstanceToTimelineMap = TMap<uint64, FNodeToTimelineMap>;

		/** Retrieves the timeline for internal use, creating it if it does not exist */
		TSharedRef<TimelineType> GetTimeline(TraceServices::IAnalysisSession& InSession, uint64 InAnimInstanceId, int32 InNodeId)
		{
			const uint32* TimelineIndex = nullptr;
			
			FNodeToTimelineMap* NodeToTimelineMap = AnimInstanceIdToTimelines.Find(InAnimInstanceId);
			if (NodeToTimelineMap == nullptr)
			{
				// Create the timeline map if this is the first time accessing with this anim instance
				NodeToTimelineMap = &AnimInstanceIdToTimelines.Add(InAnimInstanceId, {});
			}
			else
			{
				// Anim instance already used, attempt to find the NodeId's timeline
				TimelineIndex = NodeToTimelineMap->Find(InNodeId);
			}

			if (TimelineIndex == nullptr)
			{
				// Append our timeline to the storage and our object + the storage index to the map
				TSharedRef<TimelineType> Timeline = MakeShared<TimelineType>(InSession.GetLinearAllocator());
				const uint32 NewIndex = Timelines.Add(Timeline);
				NodeToTimelineMap->Add(InNodeId, NewIndex);
				return Timeline;
			}

			return Timelines[*TimelineIndex];
		}

		bool ReadNodeTimeline(const FNodeToTimelineMap* NodeToTimelineMap, int32 InNodeId, TFunctionRef<void(const TraceServices::ITimeline<MessageType>&)> Callback) const
		{
			const uint32* TimelineIndex = NodeToTimelineMap->Find(InNodeId);
			if (TimelineIndex == nullptr || !Timelines.IsValidIndex(*TimelineIndex))
			{
				return false;
			}

			Callback(*Timelines[*TimelineIndex]);
			return true;
		}

		/** Retrieve a timeline from an anim instance + node and execute the callback */
		bool ReadTimeline(uint64 InAnimInstanceId, int32 InNodeId, TFunctionRef<void(const TraceServices::ITimeline<MessageType>&)> Callback) const
		{
			const FNodeToTimelineMap* NodeToTimelineMap = AnimInstanceIdToTimelines.Find(InAnimInstanceId);
			if (NodeToTimelineMap == nullptr)
			{
				return false;
			}

			return ReadNodeTimeline(NodeToTimelineMap, InNodeId, Callback);
		}

		/** Enumerates timelines of all nodes given an AnimInstance */
		bool EnumerateNodeTimelines(uint64 InAnimInstanceId, TFunctionRef<void(const TraceServices::ITimeline<MessageType>&)> Callback) const
		{
			const FNodeToTimelineMap* NodeToTimelineMap = AnimInstanceIdToTimelines.Find(InAnimInstanceId);
			if (NodeToTimelineMap == nullptr)
			{
				return false;
			}

			bool bSuccess = true;
			for (const TPair<int32, uint32>& Pair : *NodeToTimelineMap)
			{
				bSuccess &= ReadNodeTimeline(NodeToTimelineMap, Pair.Key, Callback);
				if (!bSuccess)
				{
					break;
				}
			}

			return bSuccess;
		}

		TSet<int32> GetNodeIds(uint64 InAnimInstanceId) const
		{
			TSet<int32> NodeIds;
			if (!AnimInstanceIdToTimelines.Find(InAnimInstanceId))
			{
				return NodeIds;
			}
			const FNodeToTimelineMap& NodeToTimelineMap = AnimInstanceIdToTimelines[InAnimInstanceId];

			// For each node that has a timeline, add the node to the set
			for(const TTuple<int32, uint32>& Pair : NodeToTimelineMap)
			{
				NodeIds.Add(Pair.Key);
			}
			
			return NodeIds;
		}

		/** Maps AnimInstanceIds to a map of NodeIds to timelines. AnimInstanceId -> NodeId -> Timeline */
		FAnimInstanceToTimelineMap AnimInstanceIdToTimelines;

		/** Timelines per node */
		TArray<TSharedRef<TimelineType>> Timelines;
	};

	// Storage for each message type
	struct FMotionMatchingStateTimelineStorage : TTimelineStorage<FTraceMotionMatchingStateMessage>
	{
	} MotionMatchingStateTimelineStorage;


	TraceServices::IAnalysisSession& Session;
};
} // namespace UE::PoseSearch
