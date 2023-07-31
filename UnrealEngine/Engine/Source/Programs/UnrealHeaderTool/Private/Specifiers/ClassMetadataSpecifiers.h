// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EClassMetadataSpecifier
{
	None = -1

	#define CLASS_METADATA_SPECIFIER(SpecifierName) , SpecifierName
		#include "ClassMetadataSpecifiers.def"
	#undef CLASS_METADATA_SPECIFIER

	, Max
};

extern const TCHAR* GClassMetadataSpecifierStrings[(int32)EClassMetadataSpecifier::Max];
