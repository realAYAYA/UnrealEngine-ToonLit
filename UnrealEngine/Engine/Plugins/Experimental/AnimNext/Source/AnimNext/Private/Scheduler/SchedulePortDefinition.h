// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/ParamTypeHandle.h"

struct FAnimNextSchedulePortTask;

namespace UE::AnimNext
{
	struct FScheduler;
	struct FSchedulePortAdapterContext;
}

namespace UE::AnimNext
{

// Port definition registered with the scheduler
struct FSchedulePortDefinition
{
	// Function signature for port adapters
	using FAdapterFunction = TUniqueFunction<void(const FSchedulePortAdapterContext& InContext)>;

	FSchedulePortDefinition(FName InName, const FParamTypeHandle& InInputType, const FParamTypeHandle& InOutputType, FAdapterFunction&& InAdapter)
		: Name(InName)
		, InputType(InInputType)
		, OutputType(InOutputType)
		, Adapter(MoveTemp(InAdapter))
	{}

private:
	friend struct FScheduler;
	friend struct ::FAnimNextSchedulePortTask;

	FName Name;

	FParamTypeHandle InputType;

	FParamTypeHandle OutputType;

	FAdapterFunction Adapter;
};

}