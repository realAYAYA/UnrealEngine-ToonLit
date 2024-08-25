// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimNextSchedulerWorldSubsystem;

namespace UE::AnimNext
{
	struct FScheduler;
	struct FSchedulerImpl;
}

namespace UE::AnimNext
{

// Opaque handle representing a binding between a schedule and parameters
struct FScheduleHandle
{
	FScheduleHandle() = default;

	bool IsValid() const
	{
		return Index != MAX_uint32 && SerialNumber != 0;
	}

	void Invalidate()
	{
		Index = MAX_uint32;
		SerialNumber = 0;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(%d, %d)"), Index, SerialNumber);
	}

private:
	friend struct FScheduler;
	friend struct FSchedulerImpl;
	friend struct FSchedulerEntry;
	friend struct FScheduleContext;
	friend class ::UAnimNextSchedulerWorldSubsystem;

	uint32 Index = MAX_uint32;
	uint32 SerialNumber = 0;
};

}