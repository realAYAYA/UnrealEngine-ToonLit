// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/UnrealTypeTraits.h"

namespace UE::StructUtils
{
	template <typename T>
	void CheckStructType()
	{
		static_assert(!TIsDerivedFrom<T, struct FInstancedStruct>::IsDerived &&
					  !TIsDerivedFrom<T, struct FConstStructView>::IsDerived &&
					  !TIsDerivedFrom<T, struct FConstSharedStruct>::IsDerived, "It does not make sense to create a instanced struct over an other struct wrapper type");
	}
}
