// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD

namespace memory
{
	template <typename T>
	inline void ReleaseAndNull(T*& instance)
	{
		if (instance)
		{
			instance->Release();
			instance = nullptr;
		}
	}


	template <typename T>
	inline void DeleteAndNull(T*& instance)
	{
		delete instance;
		instance = nullptr;
	}


	template <typename T>
	inline void DeleteAndNullArray(T*& instance)
	{
		delete[] instance;
		instance = nullptr;
	}


	template <typename T, size_t N>
	bool Matches(const T(&mem1)[N], const T(&mem2)[N])
	{
		for (size_t i = 0u; i < N; ++i)
		{
			if (mem1[i] != mem2[i])
			{
				return false;
			}
		}

		return true;
	}
}
