// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"
#include "HAL/CriticalSection.h"
#include "Core/MediaTypes.h"
#include "Core/MediaNoncopyable.h"


class FMediaCriticalSection : private TMediaNoncopyable<FMediaCriticalSection>
{
public:
	FMediaCriticalSection()
	{
	}

	~FMediaCriticalSection()
	{
		CriticalSection.Lock();
		CriticalSection.Unlock();
	}

	void Lock() const
	{
		CriticalSection.Lock();
	}

	void Unlock() const
	{
		CriticalSection.Unlock();
	}

	bool TryLock() const
	{
		return CriticalSection.TryLock();
	}

	/**
	 * Scoped lock helper class.
	*/
	class ScopedLock : private TMediaNoncopyable<ScopedLock>
	{
	public:
		explicit ScopedLock(const FMediaCriticalSection& lock)
			: CriticalSection(lock)
		{
			CriticalSection.Lock();
		}
		~ScopedLock()
		{
			CriticalSection.Unlock();
		}
	private:
		ScopedLock();
		const FMediaCriticalSection& CriticalSection;
	};

private:
	mutable FCriticalSection CriticalSection;

};
