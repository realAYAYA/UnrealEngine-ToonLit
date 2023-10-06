// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/CopyQualifiersFromTo.h"
#include <type_traits>

/**
 * Tests if qualifiers are lost between one type and another, e.g.:
 *
 * TLosesQualifiersFromTo<const    T1,                T2>::Value == true
 * TLosesQualifiersFromTo<volatile T1, const volatile T2>::Value == false
 */
template <typename From, typename To>
struct TLosesQualifiersFromTo
{
	enum { Value = !std::is_same_v<typename TCopyQualifiersFromTo<From, To>::Type, To> };
};
