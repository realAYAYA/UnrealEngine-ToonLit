// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include <type_traits>

/**
 * TRemoveCV<type> will remove any const/volatile qualifiers from a type.
 * (based on std::remove_cv<>
 * note: won't remove the const from "const int*", as the pointer is not const
 */
template <typename T>
struct UE_DEPRECATED(5.3, "TRemoveCV has been deprecated, please use std::remove_cv_t instead.") TRemoveCV
{
	using Type = std::remove_cv_t<T>;
};
