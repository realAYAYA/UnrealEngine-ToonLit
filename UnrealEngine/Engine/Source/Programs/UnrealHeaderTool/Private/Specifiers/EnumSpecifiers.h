// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EEnumSpecifier
{
	None = -1

	#define ENUM_SPECIFIER(SpecifierName) , SpecifierName
		#include "EnumSpecifiers.def"
	#undef ENUM_SPECIFIER

	, Max
};

extern const TCHAR* GEnumSpecifierStrings[(int32)EEnumSpecifier::Max];
