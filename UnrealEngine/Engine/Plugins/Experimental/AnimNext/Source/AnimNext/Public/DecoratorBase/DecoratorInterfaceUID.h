// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	/**
	 * FDecoratorInterfaceUID
	 *
	 * Encapsulates an interface global UID.
	 * The string is exposed in non-shipping builds for logging and debugging purposes.
	 * The UID should be generated from the provided string using FNV1a with 32 bits.
	 *
	 * The whole struct is meant to be 'constexpr' to allow inlining in interface queries.
	 */
	struct FDecoratorInterfaceUID final
	{
		// Constructs an invalid UID
		constexpr FDecoratorInterfaceUID()
			: UID(0)
#if !UE_BUILD_SHIPPING
			, InterfaceName(TEXT("<Invalid Interface UID>"))
#endif
		{}

		// Constructs an interface UID
		constexpr FDecoratorInterfaceUID(const TCHAR* InterfaceName_, uint32 UID_)
			: UID(UID_)
#if !UE_BUILD_SHIPPING
			, InterfaceName(InterfaceName_)
#endif
		{
		}

#if !UE_BUILD_SHIPPING
		// Returns a literal string to the interface name
		constexpr const TCHAR* GetInterfaceName() const { return InterfaceName; }
#endif

		// Returns the interface global UID
		constexpr uint32 GetUID() const { return UID; }

		// Returns whether this UID is valid or not
		constexpr bool IsValid() const { return UID != 0; }

		// Compares for equality and inequality
		constexpr bool operator==(FDecoratorInterfaceUID RHS) const { return UID == RHS.UID; }
		constexpr bool operator!=(FDecoratorInterfaceUID RHS) const { return UID != RHS.UID; }

	private:
		uint32	UID;

#if !UE_BUILD_SHIPPING
		const TCHAR* InterfaceName;
#endif
	};
}
