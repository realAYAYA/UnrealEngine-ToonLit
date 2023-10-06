// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Timespan.h"
#include "HAL/PlatformTime.h"
#include "Math/UnrealMathUtility.h"
#include "CoreTypes.h"

namespace UE
{
	// utility class to handle timeouts
	// usage:
	// ------------------------------------
	// FTimeout Timeout(FTimespan::FromMilliseconds(2));
	// while (!Timeout) { ... }
	// ------------------------------------
	class FTimeout
	{
	public:
		explicit FTimeout(FTimespan Value)
			: Timeout(Value)
		{
		}

		explicit operator bool() const
		{
			return GetRemainingTime() <= FTimespan::Zero();
		}

		FTimespan GetElapsedTime() const
		{
			return FTimespan::FromSeconds(FPlatformTime::Seconds()) - Start;
		}

		FTimespan GetRemainingTime() const
		{
			return Timeout == FTimespan::MaxValue() ? FTimespan::MaxValue() : Timeout - GetElapsedTime();
		}

		static FTimeout Never()
		{
			return FTimeout{ FTimespan::MaxValue() };
		}

		FTimespan GetTimeoutValue() const
		{
			return Timeout;
		}

		// intended for use in waiting functions, e.g. `FEvent::Wait()`
		// returns the whole number (rounded up) of remaining milliseconds, clamped into [0, MAX_uint32] range
		uint32 GetRemainingRoundedUpMilliseconds() const
		{
			if (*this == Never())
			{
				return MAX_uint32;
			}

			int64 RemainingTicks = GetRemainingTime().GetTicks();
			int64 RemainingMsecs = FMath::DivideAndRoundUp(RemainingTicks, ETimespan::TicksPerMillisecond);
			int64 RemainingMsecsClamped = FMath::Clamp<int64>(RemainingMsecs, 0, MAX_uint32);
			return (uint32)RemainingMsecsClamped;
		}

		friend bool operator==(FTimeout Left, FTimeout Right)
		{
			return Left.Timeout == Right.Timeout && (Left.Timeout == FTimespan::MaxValue() || Left.Start == Right.Start);
		}

		friend bool operator!=(FTimeout Left, FTimeout Right)
		{
			return !operator==(Left, Right);
		}

	private:
		FTimespan Start{ FTimespan::FromSeconds(FPlatformTime::Seconds()) };
		FTimespan Timeout;
	};
}
