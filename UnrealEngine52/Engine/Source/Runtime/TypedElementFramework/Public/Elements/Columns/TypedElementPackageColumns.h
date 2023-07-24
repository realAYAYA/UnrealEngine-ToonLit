// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "IO/PackageId.h"
#include "Misc/PackagePath.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementPackageColumns.generated.h"


/**
 * Column that stores the path of a package.
 */
USTRUCT()
struct FTypedElementPackagePathColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	FString Path;
};

inline uint32 GetTypeHash(const FTypedElementPackagePathColumn& InStruct)
{
	return GetTypeHash(InStruct.Path);
}

/**
 * Column that stores the full loading path to a package.
 */
USTRUCT()
struct FTypedElementPackageLoadedPathColumn : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	FPackagePath LoadedPath;
};
