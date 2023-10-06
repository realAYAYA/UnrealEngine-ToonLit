// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/StringBuilder.h"

#include "HAL/PlatformMath.h"
#include "HAL/UnrealMemory.h"
#include "Misc/CString.h"
#include "Misc/ScopeExit.h"

static inline uint64_t NextPowerOfTwo(uint64_t x)
{
	x -= 1;
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);

	return x + 1;
}

template <typename C>
TStringBuilderBase<C>::~TStringBuilderBase()
{
	if (bIsDynamic)
	{
		FreeBuffer(Base, static_cast<SIZE_T>(End - Base));
	}
}

template <typename C>
void TStringBuilderBase<C>::Extend(SIZE_T ExtraCapacity)
{
	const SIZE_T OldCapacity = End - Base;
	const SIZE_T NewCapacity = FPlatformMath::Max<SIZE_T>(NextPowerOfTwo(OldCapacity + ExtraCapacity), 32);

	C* NewBase = (C*)AllocBuffer(NewCapacity);

	SIZE_T Pos = CurPos - Base;
	FMemory::Memcpy(NewBase, Base, Pos * sizeof(C));

	if (bIsDynamic)
	{
		FreeBuffer(Base, OldCapacity);
	}

	Base		= NewBase;
	CurPos		= NewBase + Pos;
	End			= NewBase + NewCapacity;
	bIsDynamic	= true;
}

template <typename C>
void* TStringBuilderBase<C>::AllocBuffer(SIZE_T CharCount)
{
	return FMemory::Malloc(CharCount * sizeof(C));
}

template <typename C>
void TStringBuilderBase<C>::FreeBuffer(void* Buffer, SIZE_T CharCount)
{
	FMemory::Free(Buffer);
}

template <typename C>
TStringBuilderBase<C>& TStringBuilderBase<C>::AppendV(const C* Fmt, va_list Args)
{
	for (;;)
	{
		va_list ArgPack;
		va_copy(ArgPack, Args);
		const int32 RemainingSize = (int32)(End - CurPos);
		const int32 Result = TCString<C>::GetVarArgs(CurPos, RemainingSize, Fmt, ArgPack);
		va_end(ArgPack);

		if (Result >= 0 && Result < RemainingSize)
		{
			CurPos += Result;
			return *this;
		}
		else
		{
			// Total size will be rounded up to the next power of two. Start with at least 64.
			Extend(64);
		}
	}
}

template <typename C>
TStringBuilderBase<C>& TStringBuilderBase<C>::AppendfImpl(BuilderType& Self, const C* Fmt, ...)
{
	va_list ArgPack;
	va_start(ArgPack, Fmt);
	ON_SCOPE_EXIT { va_end(ArgPack); };
	return Self.AppendV(Fmt, ArgPack);
}

// Instantiate templates once

template class TStringBuilderBase<ANSICHAR>;
template class TStringBuilderBase<UTF8CHAR>;
template class TStringBuilderBase<WIDECHAR>;
