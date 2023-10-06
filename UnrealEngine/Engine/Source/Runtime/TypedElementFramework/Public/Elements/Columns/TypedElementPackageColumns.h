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
USTRUCT(meta = (DisplayName = "Package path"))
struct FTypedElementPackagePathColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	UPROPERTY()
	FString Path;
};

inline uint32 GetTypeHash(const FTypedElementPackagePathColumn& InStruct)
{
	return GetTypeHash(InStruct.Path);
}

/**
 * Column that stores the full loading path to a package.
 */
USTRUCT(meta = (DisplayName = "Package loaded path"))
struct FTypedElementPackageLoadedPathColumn final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	FPackagePath LoadedPath;
};
