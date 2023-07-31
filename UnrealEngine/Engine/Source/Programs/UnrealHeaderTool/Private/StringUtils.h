// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Timespan.h"

/** 
 * Attempts to strip the given class name of its affixed prefix. If no prefix exists, will return a blank string.
 *
 * @param InClassName Class Name with a prefix to be stripped
 */
FString GetClassNameWithPrefixRemoved(const FString& InClassName);

/** 
 * Attempts to strip the given class name of its affixed prefix. If no prefix exists, will return unchanged string.
 *
 * @param InClassName Class Name with a prefix to be stripped
 */
FString GetClassNameWithoutPrefix(FString InClassNameOrFilename);

/** 
 * Attempts to get class prefix. If the given class name does not start with a valid Unreal Prefix, it will return an empty string.
 *
 * @param InClassName Name w/ potential prefix to check
 */
FString GetClassPrefix(const FString& InClassName);

/** 
 * Attempts to get class prefix. If the given class name does not start with a valid Unreal Prefix, it will return an empty string.
 *
 * @param InClassName Name w/ potential prefix to check
 * @param out bIsLabeledDeprecated Reference param set to True if the class name is marked as deprecated
 */
FString GetClassPrefix(const FString& InClassName, bool& bIsLabeledDeprecated);

/**
* Generates hash for text, ignoring characters such as \r and the terminating \0
*
* @param InText Text to digest
* @return Hash generated for the specified text buffer
*/
uint32 GenerateTextHash(const TCHAR* InText);
