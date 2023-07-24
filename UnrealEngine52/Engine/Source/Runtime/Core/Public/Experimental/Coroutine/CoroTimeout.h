// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Coroutine.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Timespan.h"

enum class ECoroTimeoutFlags : uint8
{
	Suspend_Named	= (1U << 1U),
	Suspend_Worker	= (1U << 2U),
	Suspend_All		= Suspend_Named | Suspend_Worker,
};
ENUM_CLASS_FLAGS(ECoroTimeoutFlags)

#if WITH_CPP_COROUTINES
/*
 * FCoroTimeoutAwaitable allows to suspend a Coroutine after it's timelimit has passed
 */
class FCoroTimeoutAwaitable
{
	mutable FTimespan SuspendTime;
	FTimespan Timedelta;
	ECoroTimeoutFlags Flags;

public:

	/*
	 * FCoroTimeoutAwaitable Constructor
	 * @param InTimedelta: Timelimit between suspensions
	 * @param: InFlags Flags to configure if only workers and or named threads should trigger the suspensions after the timelimit was reached
	 */
	inline FCoroTimeoutAwaitable(FTimespan InTimedelta = FTimespan::FromMilliseconds(33), ECoroTimeoutFlags InFlags = ECoroTimeoutFlags::Suspend_Worker) : Timedelta(InTimedelta), Flags(InFlags)
	{
		SuspendTime = FTimespan::FromSeconds(FPlatformTime::Seconds()) + Timedelta;
	}

	inline bool await_ready() const noexcept
	{
		bool Ready = false;
		if (LowLevelTasks::FTask::GetActiveTask() != nullptr)
		{
			Ready = !EnumHasAnyFlags(Flags, ECoroTimeoutFlags::Suspend_Worker);
		}
		else
		{
			Ready = !EnumHasAnyFlags(Flags, ECoroTimeoutFlags::Suspend_Named);
		}
		
		return Ready || FTimespan::FromSeconds(FPlatformTime::Seconds()) <= SuspendTime;
	}

	inline void await_suspend(coroutine_handle_base) noexcept
	{
	}

	inline void await_resume() noexcept
	{
		SuspendTime = FTimespan::FromSeconds(FPlatformTime::Seconds()) + Timedelta; 
	}
};

#else

class FCoroTimeoutAwaitable
{
public:
	inline FCoroTimeoutAwaitable(FTimespan InTimedelta = FTimespan::FromMilliseconds(33), ECoroTimeoutFlags InFlags = ECoroTimeoutFlags::Suspend_Worker)
	{
	}
};

#endif