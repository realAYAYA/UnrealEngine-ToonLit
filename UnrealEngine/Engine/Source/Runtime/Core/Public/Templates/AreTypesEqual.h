// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// HEADER_UNIT_SKIP - Deprecated

#include <type_traits>

/** Tests whether two typenames refer to the same type. */
template<typename A,typename B>
struct UE_DEPRECATED(5.2, "TAreTypesEqual has been deprecated, please use std::is_same instead.") TAreTypesEqual
{
	enum { Value = std::is_same_v<A, B> };
};

#define ARE_TYPES_EQUAL(A,B) std::is_same_v<A, B>
