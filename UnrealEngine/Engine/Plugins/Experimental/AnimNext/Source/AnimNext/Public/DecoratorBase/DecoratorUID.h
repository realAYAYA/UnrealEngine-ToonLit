// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	// Type alias for a raw decorator UID, not typesafe
	using FDecoratorUIDRaw = uint32;

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
		constexpr FDecoratorUID() noexcept
			: UID(INVALID_UID)
#if !UE_BUILD_SHIPPING
			, DecoratorName(TEXT("<Invalid decorator UID>"))
#endif
		{
		}

		// Constructs a decorator UID
		explicit constexpr FDecoratorUID(FDecoratorUIDRaw InUID, const TCHAR* InDecoratorName = TEXT("<Unknown Decorator Name>")) noexcept
			: UID(InUID)
#if !UE_BUILD_SHIPPING
			, DecoratorName(InDecoratorName)
#endif
		{
		}

#if !UE_BUILD_SHIPPING
		// Returns a literal string to the interface name
		constexpr const TCHAR* GetDecoratorName() const noexcept { return DecoratorName; }
#endif

		// Returns the decorator global UID
		constexpr FDecoratorUIDRaw GetUID() const noexcept { return UID; }

		// Returns whether this UID is valid or not
		constexpr bool IsValid() const noexcept { return UID != INVALID_UID; }

	private:
		static constexpr FDecoratorUIDRaw INVALID_UID = 0;

		FDecoratorUIDRaw	UID;

#if !UE_BUILD_SHIPPING
		const TCHAR*		DecoratorName;
#endif
	};

	// Compares for equality and inequality
	constexpr bool operator==(FDecoratorUID LHS, FDecoratorUID RHS) noexcept { return LHS.GetUID() == RHS.GetUID(); }
	constexpr bool operator!=(FDecoratorUID LHS, FDecoratorUID RHS) noexcept { return LHS.GetUID() != RHS.GetUID(); }
	constexpr bool operator==(FDecoratorUID LHS, FDecoratorUIDRaw RHS) noexcept { return LHS.GetUID() == RHS; }
	constexpr bool operator!=(FDecoratorUID LHS, FDecoratorUIDRaw RHS) noexcept { return LHS.GetUID() != RHS; }
	constexpr bool operator==(FDecoratorUIDRaw LHS, FDecoratorUID RHS) noexcept { return LHS == RHS.GetUID(); }
	constexpr bool operator!=(FDecoratorUIDRaw LHS, FDecoratorUID RHS) noexcept { return LHS != RHS.GetUID(); }
}
