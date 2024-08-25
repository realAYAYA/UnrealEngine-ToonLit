// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include <atomic>

//////////////////////////////////////////////////////////////////////////
struct AccessInfo
{
	std::atomic<double>				Timestamp = 0;		/// The last time was accessed. We use timestamp to determine modification 
														/// between two time intervals. Hence atomic.

	/// If any of these need to be accessed in a thread-safe manner
	/// then these can be turned into atomics as well
	uint64							FrameId = 0;		/// When as the last frame that we were bound
	uint64							BatchId = 0;		/// The batch ID when this was last bound
	uint64							Count = 0;			/// The number of times this was bound and accessed

	AccessInfo()
	{
	}

	AccessInfo(const AccessInfo& RHS)
	{
		*this						= RHS;
	}

	FORCEINLINE void				operator = (const AccessInfo& RHS)
	{
		Timestamp					= RHS.Timestamp.load();
		FrameId						= RHS.FrameId;
		BatchId						= RHS.BatchId;
		Count						= RHS.Count;
	}
};

//////////////////////////////////////////////////////////////////////////
/// Garbage collectible data structure
class TEXTUREGRAPHENGINE_API G_Collectible
{
protected:
	AccessInfo						AccessDetails;				/// Information about the last time this DeviceBuffer was accessed

public:
	virtual void					UpdateAccessInfo(uint64 BatchId = 0);
	virtual							~G_Collectible() {}

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE const AccessInfo&	GetAccessInfo() const { return AccessDetails; }
	FORCEINLINE bool				IsSame(const std::atomic<double>& Timestamp) const { return AccessDetails.Timestamp == Timestamp; }
};
