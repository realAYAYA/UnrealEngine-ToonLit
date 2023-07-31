// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/StaticAssertCompleteType.h"

/**
 * Traits class which tests if a type is a UEnum class.
 */
template <typename T>
struct TIsUEnumClass
{
	UE_STATIC_ASSERT_COMPLETE_TYPE(T, "TIsUEnumClass instantiated with an incomplete type");
	enum { Value = false };
};
