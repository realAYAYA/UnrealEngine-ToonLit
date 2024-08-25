// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaBase.h"

namespace uba
{
	class Event
	{
	public:
		Event();
		Event(bool manualReset);
		~Event();
		bool Create(bool manualReset, bool shared = false);
		void Destroy();
		void Set();
		void Reset();
		bool IsSet(u32 timeOutMs = ~0u);
		void* GetHandle();
	private:
		#if PLATFORM_WINDOWS
		void* m_ev;
		#else
		u64 m_data[16];
		#endif

		Event(const Event&) = delete;
		void operator=(const Event&) = delete;
	};
}
