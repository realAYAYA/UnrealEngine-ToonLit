// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/AssertionMacros.h"

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

namespace Verse
{

// This struct is just a pointer wrapper to be used when you want to allocate `Aux` memory.
// Aux memory is marked but not put on the MarkStack like VCells and we wrap Aux pointers in this
// so TWriteBarrier/TWeakBarrier can know to mark its `Ptr` as Aux.
template <typename T>
class TAux
{
	T* Ptr;

public:
	TAux()
		: Ptr(nullptr) {}
	TAux(void* InPtr)
		: Ptr(static_cast<T*>(InPtr)) {}

	T* GetPtr() { return Ptr; }

	explicit operator bool() const { return !!(Ptr); }

	T& operator*()
	{
		check(Ptr);
		return *Ptr;
	};
	T* operator->()
	{
		check(Ptr);
		return Ptr;
	};
	T& operator[](size_t Index)
	{
		return Ptr[Index];
	}
};

template <typename T>
static constexpr inline bool IsTAux = false;

template <typename T>
static constexpr inline bool IsTAux<TAux<T>> = true;

} // namespace Verse
#endif // WITH_VERSE_VM
