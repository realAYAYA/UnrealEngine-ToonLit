// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNECoreAttributeDataType.h"

void LexFromString(ENNEAttributeDataType& OutValue, const TCHAR* StringVal)
{
	int64 EnumVal = StaticEnum<ENNEAttributeDataType>()->GetValueByName(StringVal);
	if (EnumVal == INDEX_NONE)
	{
		OutValue = ENNEAttributeDataType::None;
		ensureMsgf(false, TEXT("ENNEAttributeDataType LexFromString didn't have a match for '%s'"), StringVal);
	}

	OutValue = (ENNEAttributeDataType)EnumVal;
};