// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats.h"
#include "Utils.h"

namespace AutoRTFM
{
	class FWriteLogBumpAllocator final
	{
	public:
		constexpr static size_t MaxSize = 128;

		FWriteLogBumpAllocator()
		{
			// The max we can support in our tagged pointers is 16 bits of data.
			static_assert(MaxSize <= UINT16_MAX);
			Reset();
		}

		FWriteLogBumpAllocator(const FWriteLogBumpAllocator&) = delete;

		~FWriteLogBumpAllocator()
		{
			Reset();
		}

		void* Allocate(size_t Bytes)
		{
			ASSERT(Bytes <= MaxSize);
			StatTotalSize += Bytes;

			if (nullptr == Start)
			{
				Start = new FPage();
				Current = Start;
			}

			if (Bytes <= (MaxSize - Current->Size))
			{
				void* Result = &Current->Bytes[Current->Size];
				Current->Size += Bytes;
				return Result;
			}

			Current->Next = new FPage();
			Current = Current->Next;

			return Allocate(Bytes);
		}

		void Reset()
		{
			FPage* Page = Start;

			while (nullptr != Page)
			{
				FPage* const Next = Page->Next;
				delete Page;
				Page = Next;
			}

			Start = nullptr;
			Current = nullptr;
		}

		void Merge(FWriteLogBumpAllocator&& Other)
		{
			if (nullptr != Start)
			{
				Current->Next = Other.Start;
			}
			else
			{
				Start = Other.Start;
				Current = Start;
			}

			Other.Start = nullptr;
			Other.Current = nullptr;
		}
		
		TStatStorage<size_t> StatTotalSize = 0;
	private:

		struct FPage final
		{
			// Specify a constructor so that `Bytes` below will be left uninitialized.
			explicit FPage() {}

			uint8_t Bytes[FWriteLogBumpAllocator::MaxSize];
			FPage* Next = nullptr;
			size_t Size = 0;
		};

		FPage* Start = nullptr;
		FPage* Current = nullptr;
	};
}
