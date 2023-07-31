// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"

struct FIoHash;

namespace UE::Virtualization::Utils
{

/** 
 * Converts a FIoHash into a file path.
 *
 * This utility will take an FIoHash and return a file path that is
 * 3 directories deep. The first six characters of the id will be used to
 * create the directory names, with each directory using two characters.
 * The remaining thirty four characters will be used as the file name.
 * Lastly the extension '.payload' will be applied to complete the path/
 * Example: FIoHash 0139d6d5d477e32dfd2abd3c5bc8ea8507e8eef8 becomes
 *			01/39/d6/d5d477e32dfd2abd3c5bc8ea8507e8eef8.payload'
 * 
 * @param	Id The payload identifier used to create the file path.
 * @param	OutPath Will be reset and then assigned the resulting file path.
 *			The string builder must have a capacity of at least 52 characters 
			to avoid reallocation.
 */
void PayloadIdToPath(const FIoHash& Id, FStringBuilderBase& OutPath);

/** 
 * Converts a FIoHash into a file path.
 * 
 * See above for further details
 * 
 * @param	Id The payload identifier used to create the file path.
 * @return	The resulting file path.
 */
FString PayloadIdToPath(const FIoHash& Id);

/**
 * Fill in the given string builder with the human readable message of the current system
 * code, followed by the code value itself.
 * In the system value is currently 0, then we assume that it was cleared before this was
 * able to be called and write that the error is unknown instead of assuming that the
 * operation was a success.
 */
void GetFormattedSystemError(FStringBuilderBase& SystemErrorMessage);

} // namespace UE::Virtualization::Utils
