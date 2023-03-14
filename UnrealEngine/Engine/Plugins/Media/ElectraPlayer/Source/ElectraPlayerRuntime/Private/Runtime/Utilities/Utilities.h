// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PlayerCore.h"



namespace Electra
{

	namespace Utils
	{


		template <typename T>
		inline T AbsoluteValue(T Value)
		{
			return Value >= T(0) ? Value : -Value;
		}

		template <typename T>
		inline T Min(T a, T b)
		{
			return a < b ? a : b;
		}

		template <typename T>
		inline T Max(T a, T b)
		{
			return a > b ? a : b;
		}

		inline uint32 BitReverse32(uint32 InValue)
		{
			uint32 rev = 0;
			for(int32 i=0; i<32; ++i)
			{
				rev = (rev << 1) | (InValue & 1);
				InValue >>= 1;
			}
			return rev;
		}


	} // namespace Utils

} // namespace Electra

