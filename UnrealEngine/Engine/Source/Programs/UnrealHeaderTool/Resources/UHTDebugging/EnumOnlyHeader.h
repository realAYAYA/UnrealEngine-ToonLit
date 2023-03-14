// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.generated.h"

UENUM()
enum EOldEnum
{
	One,
	Two,
	Three
};

UENUM()
namespace ENamespacedEnum
{
	enum Type
	{
		Four,
		Five,
		Six
	};
}

UENUM()
enum class ECppEnum : uint8
{
	Seven,
	Eight,
	Nine
};

UENUM()
enum struct ECppEnumStruct : uint8
{
	Ten,
	Eleven,
	Twelve
};

UENUM()
enum alignas(8) EAlignedOldEnum
{
	Thirteen,
	Fourteen,
	Fifteen
};

UENUM()
namespace EAlignedNamespacedEnum
{
	enum alignas(8) Type
	{
		Sixteen,
		Seventeen,
		Eighteen
	};
}

UENUM()
enum class alignas(8) EAlignedCppEnum : uint8
{
	Nineteen,
	Twenty,
	TwentyOne
};

UENUM()
enum struct alignas(8) EAlignedCppEnumStruct : uint8
{
	TwentyTwo,
	TwentyThree,
	TwentyFour
};

UENUM()
enum UE_DEPRECATED(1.23, "Deprecated") EDeprecatedEnum
{
	TwentyFive,
	TwentySix,
	TwentySeven
};

UENUM()
enum class UE_DEPRECATED(1.23, "Deprecated") EDeprecatedEnumClass
{
	TwentyEight,
	TwnetyNine,
	Thirty
};

UENUM()
namespace UE_DEPRECATED(1.23, "Deprecated") EDeprecatedNamespacedEnum
{
	enum Type
	{
		ThirtyOne,
		ThirtyTwo,
		ThirtyThree
	};
}
