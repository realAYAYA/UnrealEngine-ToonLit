// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "NNEAttributeDataType.generated.h"

/**
 * Attribute data types
 * 
 * Note: also extend NNEAttributeValueTraits.h for more type support
 */
UENUM()
enum class ENNEAttributeDataType : uint8
{
	None,
	Float,								//!< 32-bit floating number
	FloatArray,							//!< TArray of 32-bit floating numbers
	Int32,								//!< 32-bit signed integer
	Int32Array,							//!< TArray of 32-bit signed integers
	String,								//!< built-in FString
	StringArray							//!< TArray of built-in FString
};