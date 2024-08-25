// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	/**
	 * FDecoratorRegistryHandle
	 *
	 * Encapsulates a value used as a handle in the decorator registry.
	 * When valid, it can be used to retrieve a pointer to the corresponding decorator.
	 */
	struct FDecoratorRegistryHandle final
	{
		// Default constructed handles are invalid
		FDecoratorRegistryHandle() noexcept = default;

		// Returns whether or not this handle points to a valid decorator
		bool IsValid() const noexcept { return HandleValue != 0; }

		// Returns whether or not this handle is valid and points to a static decorator
		bool IsStatic() const noexcept { return IsValid() && HandleValue > 0; }

		// Returns whether or not this handle is valid and points to a dynamic decorator
		bool IsDynamic() const noexcept { return IsValid() && HandleValue < 0; }

		// Returns the static buffer offset for this handle when valid, otherwise INDEX_NONE
		int32 GetStaticOffset() const noexcept { return IsStatic() ? (HandleValue - 1) : INDEX_NONE; }

		// Returns the dynamic array index for this handle when valid, otherwise INDEX_NONE
		int32 GetDynamicIndex() const noexcept { return IsDynamic() ? (-HandleValue - 1) : INDEX_NONE; }

		// Compares for equality and inequality
		bool operator==(FDecoratorRegistryHandle RHS) const noexcept { return HandleValue == RHS.HandleValue; }
		bool operator!=(FDecoratorRegistryHandle RHS) const noexcept { return HandleValue != RHS.HandleValue; }

	private:
		explicit FDecoratorRegistryHandle(int16 HandleValue_) noexcept
			: HandleValue(HandleValue_)
		{}

		// Creates a static handle based on a decorator offset in the static buffer
		static FDecoratorRegistryHandle MakeStatic(int32 DecoratorOffset)
		{
			check(DecoratorOffset >= 0 && DecoratorOffset < (1 << 15));
			return FDecoratorRegistryHandle(DecoratorOffset + 1);
		}

		// Creates a dynamic handle based on a decorator index in the dynamic array
		static FDecoratorRegistryHandle MakeDynamic(int32 DecoratorIndex)
		{
			// Convert the index from 0-based to 1-based since we reserve 0 for our invalid handle
			check(DecoratorIndex >= 0 && DecoratorIndex < (1 << 15));
			return FDecoratorRegistryHandle(-DecoratorIndex - 1);
		}

		// When 0, the handle is invalid
		// When positive, it is a 1-based offset in the registry's static buffer
		// When negative, it is a 1-based index in the registry's dynamic array
		int16 HandleValue = 0;

		friend struct FDecoratorRegistry;
	};
}
