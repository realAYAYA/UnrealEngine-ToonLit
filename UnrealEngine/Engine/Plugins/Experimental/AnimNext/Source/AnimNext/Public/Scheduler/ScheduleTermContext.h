// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::AnimNext
{
	struct FParamStackLayerHandle;
	struct FScheduleContext;
}

namespace UE::AnimNext
{

struct FScheduleTermContext
{
	FScheduleTermContext() = delete;

	FScheduleTermContext(const FScheduleContext& InScheduleContext, const FParamStackLayerHandle& InResultLayerHandle)
		: ScheduleContext(InScheduleContext)
		, ResultLayerHandle(InResultLayerHandle)
	{}

	// Get the schedule context we wrap
	const FScheduleContext& GetScheduleContext() const { return ScheduleContext; }
	
	// Get the layer handle for the terms that the port operates on
	const FParamStackLayerHandle& GetLayerHandle() const { return ResultLayerHandle; }

private:
	// The schedule context we wrap
	const FScheduleContext& ScheduleContext;
	
	// The layer handle for the terms that the port operates on
	const FParamStackLayerHandle& ResultLayerHandle;
};

}
