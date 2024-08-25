// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaTimer.h"
#include "UbaPlatform.h"

#include <wchar.h>
#if !PLATFORM_WINDOWS
#include <sys/time.h>
#endif

namespace uba
{
	u64 GetTime()
	{
		#if PLATFORM_WINDOWS
		LARGE_INTEGER li;
		QueryPerformanceCounter(&li);
		return li.QuadPart;
		//#elif PLATFORM_LINUX
		//struct timespec tp;
		//clock_gettime(CLOCK_REALTIME, &tp);
		//u64 result = u64(tp.tv_sec * 10'000'000LL + tp.tv_nsec/100); // Stored as a 10th of a microsecond
		//return result;
		#else
		timeval tv;
		gettimeofday(&tv, NULL); // Returns time in microseconds since 1 Jan 1970
		return u64(tv.tv_sec) * 1'000'000ull + u64(tv.tv_usec);
		#endif
	}

	u64 GetFrequency()
	{
		#if PLATFORM_WINDOWS
		static u64 frequency = []() { LARGE_INTEGER li; QueryPerformanceFrequency(&li); return li.QuadPart; }();
		return frequency;
		#else
		return 1000000LL;
		#endif
	}

	u64 GetSystemTimeUs()
	{
		#if PLATFORM_WINDOWS
		constexpr u64 EPOCH_DIFF = 11'644'473'600ull; // Seconds from 1 Jan. 1601 to 1970 (windows to linux)
		FILETIME st;
		GetSystemTimeAsFileTime(&st);
		return *(u64*)&st / 10 - (EPOCH_DIFF*1'000'000ull);
		#else
		return GetTime();
		#endif
	}

	TimeToText::TimeToText(u64 time, bool allowMinutes) : TimeToText(time, allowMinutes, GetFrequency()) {}
	TimeToText::TimeToText(u64 time, bool allowMinutes, u64 frequency)
	{
		u64 ms = TimeToMs(time, frequency);
		if (ms == 0 && time != 0)
			TStrcpy_s(str, 32, TC("<1ms"));
		else if (ms < 1000)
			TSprintf_s(str, 32, TC("%llums"), ms);
		else if (ms < 60 * 1000 || !allowMinutes)
			TSprintf_s(str, 32, TC("%.1fs"), float(ms) / 1000);
		else
		{
			u32 totalSec = u32(float(ms) / 1000);
			u32 totalMin = totalSec / 60;
			u32 min = totalMin % 60;
			u32 sec = totalSec % 60;
			if (u32 hour = totalMin / 60)
				TSprintf_s(str, 32, TC("%uh%um%us"), (unsigned int)hour, (unsigned int)min, (unsigned int)sec);
			else
				TSprintf_s(str, 32, TC("%um%us"), (unsigned int)min, (unsigned int)sec);
		}
	}
}