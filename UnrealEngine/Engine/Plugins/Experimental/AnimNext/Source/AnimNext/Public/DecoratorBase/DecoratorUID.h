// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	/**
	 * FDecoratorUID
	 *
	 * Encapsulates an decorator global UID.
	 * The string is exposed in non-shipping builds for logging and debugging purposes.
	 * The UID should be generated from the provided string using FNV1a with 32 bits.
	 *
	 * The whole struct is meant to be 'constexpr' to allow inlining.
	 */
	struct FDecoratorUID final
	{
		// Constructs an invalid UID
		constexpr FDecoratorUID()
			: UID(0)
#if !UE_BUILD_SHIPPING
			, DecoratorName(TEXT("<Invalid decorator UID>"))
#endif
		{}

		// Constructs a decorator UID
		constexpr FDecoratorUID(const TCHAR* DecoratorName_, uint32 UID_)
			: UID(UID_)
#if !UE_BUILD_SHIPPING
			, DecoratorName(DecoratorName_)
#endif
		{
		}

#if !UE_BUILD_SHIPPING
		// Returns a literal string to the interface name
		constexpr const TCHAR* GetDecoratorName() const { return DecoratorName; }
#endif

		// Returns the decorator global UID
		constexpr uint32 GetUID() const { return UID; }

		// Returns whether this UID is valid or not
		constexpr bool IsValid() const { return UID != 0; }

	private:
		uint32	UID;

#if !UE_BUILD_SHIPPING
		const TCHAR* DecoratorName;
#endif
	};

	// Compares for equality and inequality
	constexpr bool operator==(FDecoratorUID LHS, FDecoratorUID RHS) { return LHS.GetUID() == RHS.GetUID(); }
	constexpr bool operator!=(FDecoratorUID LHS, FDecoratorUID RHS) { return LHS.GetUID() != RHS.GetUID(); }
	constexpr bool operator==(FDecoratorUID LHS, uint32 RHS) { return LHS.GetUID() == RHS; }
	constexpr bool operator!=(FDecoratorUID LHS, uint32 RHS) { return LHS.GetUID() != RHS; }
	constexpr bool operator==(uint32 LHS, FDecoratorUID RHS) { return LHS == RHS.GetUID(); }
	constexpr bool operator!=(uint32 LHS, FDecoratorUID RHS) { return LHS != RHS.GetUID(); }
}
