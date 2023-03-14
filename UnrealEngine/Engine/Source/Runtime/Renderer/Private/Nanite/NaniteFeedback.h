// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GPUMessaging.h"


namespace Nanite
{
#if !UE_BUILD_SHIPPING
class FFeedbackManager
{
	GPUMessage::FSocket StatusFeedbackSocket;
	FDelegateHandle		ScreenMessageDelegate;

	struct FBufferState
	{
		double LatestOverflowTime	= -MAX_dbl;
		uint32 LatestOverflowPeak	= 0;
		uint32 HighWaterMark		= 0;

		bool Update(const uint32 Peak, const uint32 Capacity);
	};

	FBufferState	NodeState;
	FBufferState	CandidateClusterState;
	FBufferState	VisibleClusterState;
public:
	FFeedbackManager();
	~FFeedbackManager();
	
	void Update(class FRDGBuilder& GraphBuilder, const struct FSharedContext& SharedContext, struct FCullingContext& CullingContext);
};
#endif

} // namespace Nanite
