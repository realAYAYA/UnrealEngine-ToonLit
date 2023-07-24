// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * When we have an optional value IsSet() returns true, and GetValue() is meaningful.
 * Otherwise GetValue() is not meaningful.
 */
template<typename OptionalType>
struct TOptional;

struct FNullOpt
{
	explicit constexpr FNullOpt(int) {}
};
