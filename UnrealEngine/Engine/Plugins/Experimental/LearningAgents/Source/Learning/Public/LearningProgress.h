// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/CriticalSection.h"
#include "Internationalization/Text.h"

namespace UE::Learning
{
	/**
	* Simple thread-safe structure to record progress of some long-running computation.
	*/
	struct LEARNING_API FProgress
	{
		int32 GetProgress() const;
		void SetProgress(const int32 InProgress);

		void GetMessage(FText& OutMessage);
		void SetMessage(const FText& InMessage);

		void Decrement();
		void Decrement(const int32 Num);
		void Done();

	private:
		FRWLock Lock;
		FText Message;
		TAtomic<int32> Progress = 0;
	};

	/**
	* Scoped read lock, which can optionally be null
	*/
	struct LEARNING_API FScopeNullableReadLock
	{
	public:

		UE_NODISCARD_CTOR FScopeNullableReadLock(FRWLock* InLock);
		~FScopeNullableReadLock();

		FScopeNullableReadLock() = delete;
		FScopeNullableReadLock(const FScopeNullableReadLock& InScopeLock) = delete;
		FScopeNullableReadLock& operator=(FScopeNullableReadLock& InScopeLock) = delete;

	private:
		FRWLock* Lock = nullptr;
	};

	/**
	* Scoped write lock, which can optionally be null
	*/
	struct LEARNING_API FScopeNullableWriteLock
	{
	public:

		UE_NODISCARD_CTOR FScopeNullableWriteLock(FRWLock* InLock);
		~FScopeNullableWriteLock();

		FScopeNullableWriteLock() = delete;
		FScopeNullableWriteLock(const FScopeNullableWriteLock& InScopeLock) = delete;
		FScopeNullableWriteLock& operator=(FScopeNullableWriteLock& InScopeLock) = delete;

	private:
		FRWLock* Lock = nullptr;
	};

}

