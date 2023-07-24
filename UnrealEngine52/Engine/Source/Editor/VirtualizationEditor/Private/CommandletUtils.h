// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "IO/IoHash.h"

namespace UE::Virtualization
{

/** Parse all of the active mount points and find all .uasset/.umaps */
TArray<FString> FindAllPackages();

/**
 * Finds all of the packages under a the directory given by the provided command line.
 * If no commandline switch can be found then the function will return all avaliable 
 * packages.
 * Valid commandline switches:
 * '-PackageDir=...'
 * '-PackageFolder=...'
 * 
 * @param CmdlineParams A string containing the command line
 * @return An array with the file path for each package found
 */
TArray<FString> DiscoverPackages(const FString& CmdlineParams);

/** Returns a combined list of unique virtualized payload ids from the given list of packages */
TArray<FIoHash> FindVirtualizedPayloads(const TArray<FString>& PackageNames);

} //namespace UE::Virtualization
