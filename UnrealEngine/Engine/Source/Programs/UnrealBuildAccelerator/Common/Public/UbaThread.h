// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaEvent.h"
#include "UbaMemory.h"

namespace uba
{
	class Event;
	struct GroupAffinity;


	class Thread
	{
	public:
		Thread();
		Thread(Function<u32()>&& func);
		~Thread();
		void Start(Function<u32()>&& func);
		bool Wait(u32 milliseconds = ~0u, Event* wakeupEvent = nullptr);
		bool GetGroupAffinity(GroupAffinity& out);

	private:
		Function<u32()>	m_func;
		void* m_handle = nullptr;

		#if !PLATFORM_WINDOWS
		Event m_finished;
		#endif

		Thread(const Thread&) = delete;
		void operator=(const Thread&) = delete;
	};


	struct GroupAffinity
	{
		u64 mask = 0;
		u16 group = 0;
	};
	bool SetThreadGroupAffinity(void* nativeThreadHandle, const GroupAffinity& affinity);
	bool AlternateThreadGroupAffinity(void* nativeThreadHandle);
}
