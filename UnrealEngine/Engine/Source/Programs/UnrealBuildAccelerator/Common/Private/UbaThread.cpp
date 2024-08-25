// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaThread.h"
#include "UbaPlatform.h"

namespace uba
{
	bool AlternateThreadGroupAffinity(void* nativeThreadHandle)
	{
#if PLATFORM_WINDOWS
		int processorGroupCount = GetProcessorGroupCount();
		if (processorGroupCount <= 1)
			return true;
		static Atomic<int> processorGroupCounter;
		u16 processorGroup = u16((processorGroupCounter++) % processorGroupCount);

		u32 groupProcessorCount = ::GetActiveProcessorCount(processorGroup);

		GROUP_AFFINITY groupAffinity = {};
		groupAffinity.Mask = ~0ull >> (int)(64 - groupProcessorCount);
		groupAffinity.Group = processorGroup;
		return ::SetThreadGroupAffinity(nativeThreadHandle, &groupAffinity, NULL);
#else
		return true;
#endif
	}

	bool SetThreadGroupAffinity(void* nativeThreadHandle, const GroupAffinity& affinity)
	{
#if PLATFORM_WINDOWS
		if (GetProcessorGroupCount() <= 1)
			return true;
		GROUP_AFFINITY groupAffinity = {};
		groupAffinity.Mask = affinity.mask;
		groupAffinity.Group = affinity.group;
		return ::SetThreadGroupAffinity(nativeThreadHandle, &groupAffinity, NULL);
#else
		return false;
#endif
	}


	Thread::Thread()
	{
	}

	Thread::Thread(Function<u32()>&& func)
	{
		Start(std::move(func));
	}

	Thread::~Thread()
	{
		Wait();
	}

	void Thread::Start(Function<u32()>&& f)
	{
		m_func = std::move(f);
#if PLATFORM_WINDOWS
		m_handle = CreateThread(NULL, 0, [](LPVOID p) -> DWORD { return ((Thread*)p)->m_func(); }, this, 0, NULL);
		AlternateThreadGroupAffinity(m_handle);
#else
		int err = 0;

		m_finished.Create(true);
		static_assert(sizeof(pthread_t) <= sizeof(m_handle), "");
		auto& pth = *(pthread_t*)&m_handle;

		pthread_attr_t tattr;
		// initialized with default attributes
		err = pthread_attr_init(&tattr);

		// TODO: Need to figure out a better value, or decrease stack usage
		// without this though we get a bus error on Intel Macs
		#if !defined(__arm__) && !defined(__arm64__)
		size_t size = PTHREAD_STACK_MIN * 500;
		err = pthread_attr_setstacksize(&tattr, size);
		#endif

		err = pthread_create(&pth, &tattr, [](void* p) -> void*
			{
				auto& t = *(Thread*)p;
				int res = t.m_func();
				t.m_finished.Set();
				return (void*)(uintptr_t)res;
			}, this);
		UBA_ASSERT(err == 0); (void)err;

		err = pthread_attr_destroy(&tattr);
		UBA_ASSERT(err == 0); (void)err;
#endif
	}

	bool Thread::Wait(u32 milliseconds, Event* wakeupEvent)
	{
		if (!m_handle)
			return true;

#if PLATFORM_WINDOWS // Optimization, not needed in initial implementation
		if (wakeupEvent)
		{
			HANDLE h[] = { m_handle, wakeupEvent->GetHandle() };
			DWORD res = WaitForMultipleObjects(2, h, false, milliseconds);
			if (res == WAIT_OBJECT_0 + 1 || res == WAIT_TIMEOUT)
				return false;
		}
		else
		{
			if (WaitForSingleObject(m_handle, milliseconds) == WAIT_TIMEOUT)
				return false;
		}
		CloseHandle(m_handle);
#else
		if (!m_finished.IsSet(milliseconds))
			return false;
		int* ptr = 0;
		int res = pthread_join(*(pthread_t*)&m_handle, (void**)&ptr);
		UBA_ASSERT(res == 0);
#endif
		m_func = {};
		m_handle = nullptr;
		return true;
	}

	bool Thread::GetGroupAffinity(GroupAffinity& out)
	{
#if PLATFORM_WINDOWS
		if (GetProcessorGroupCount() <= 1)
			return true;
		GROUP_AFFINITY aff;
		if (!::GetThreadGroupAffinity(m_handle, &aff))
			return false;
		out.mask = aff.Mask;
		out.group = aff.Group;
		return true;
#else
		return false;
#endif
	}

}
