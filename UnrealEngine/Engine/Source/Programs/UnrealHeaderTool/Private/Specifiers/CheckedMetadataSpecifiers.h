// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ECheckedMetadataSpecifier : int32
{
	None = -1

	#define CHECKED_METADATA_SPECIFIER(SpecifierName) , SpecifierName
		#include "CheckedMetadataSpecifiers.def"
	#undef CHECKED_METADATA_SPECIFIER

	, Max
};

extern const TMap<FName, ECheckedMetadataSpecifier> GCheckedMetadataSpecifiers;

ECheckedMetadataSpecifier GetCheckedMetadataSpecifier(FName Key);