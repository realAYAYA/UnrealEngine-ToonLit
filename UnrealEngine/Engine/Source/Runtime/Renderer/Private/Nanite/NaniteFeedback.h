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

	// Must guard the (complex) data as the on-screen feedback delegate is called from the game thread.
	FCriticalSection DelgateCallbackCS;

	// Map to track non-nanite page area items that are shown on screen
	struct FMaterialWarningItem
	{
		double LastTimeSeen = -MAX_dbl;
		double LastTimeLogged = -MAX_dbl;
	};
	TMap<FString, FMaterialWarningItem> MaterialWarningItems;
public:
	FFeedbackManager();
	~FFeedbackManager();

	uint32 GetStatusMessageId() const { return StatusFeedbackSocket.GetMessageId().GetIndex(); }

	void ReportMaterialPerformanceWarning(const FString& MaterialName);

};

extern bool ShouldReportFeedbackMaterialPerformanceWarning();

#endif

} // namespace Nanite
