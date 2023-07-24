// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

// BEGIN EPIC MOD
//#include PCH_INCLUDE
// END EPIC MOD
#include "LC_Atomic_Windows.h"
#include "LC_Foundation_Windows.h"


void Atomic::IncrementConsistent(volatile int32_t* value)
{
	LC_COMPILER_FENCE;
	LC_MEMORY_FENCE;
	LC_COMPILER_FENCE;
	::_InterlockedIncrement(reinterpret_cast<volatile long*>(value));
	LC_COMPILER_FENCE;
	LC_MEMORY_FENCE;
	LC_COMPILER_FENCE;
}


void Atomic::DecrementConsistent(volatile int32_t* value)
{
	LC_COMPILER_FENCE;
	LC_MEMORY_FENCE;
	LC_COMPILER_FENCE;
	::_InterlockedDecrement(reinterpret_cast<volatile long*>(value));
	LC_COMPILER_FENCE;
	LC_MEMORY_FENCE;
	LC_COMPILER_FENCE;
}
