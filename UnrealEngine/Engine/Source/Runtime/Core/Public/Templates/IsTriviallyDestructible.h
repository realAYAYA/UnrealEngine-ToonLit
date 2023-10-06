// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include <type_traits>

/**
 * Traits class which tests if a type has a trivial destructor.
 */
template <typename T>
struct TIsTriviallyDestructible
{
	enum { Value = std::is_trivially_destructible_v<T> };
};
