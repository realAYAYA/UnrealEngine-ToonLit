// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"
#include "VVMLog.h"

namespace Verse
{

template <typename T>
struct alignas(alignof(T)) TNeverDestroyed
{
	TNeverDestroyed()
	{
		UE_LOG(LogVerseVM, Display, TEXT("Running TNeverDestroyed constructor at %p"), this);
		new (&Get()) T();
	}

	T& Get()
	{
		return *reinterpret_cast<T*>(Data);
	}

	T& operator*()
	{
		return Get();
	}

	T* operator->()
	{
		return &Get();
	}

	const T& Get() const
	{
		return *reinterpret_cast<T*>(Data);
	}

	const T& operator*() const
	{
		return Get();
	}

	const T* operator->() const
	{
		return &Get();
	}

private:
	std::byte Data[sizeof(T)];
};

} // namespace Verse
#endif // WITH_VERSE_VM
