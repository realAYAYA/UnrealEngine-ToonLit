// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "IO/PackageId.h"
#include "Misc/PackagePath.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementPackageColumns.generated.h"

/**
 * A package reference column that has not yet been resolved to reference a package.
 */
USTRUCT(meta = (DisplayName = "Unresolved package path reference"))
struct FTypedElementPackageUnresolvedReference final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()

	FString PathOnDisk;
	
	TypedElementDataStorage::IndexHash Index;
};

/**
 * Column that references a row in the table that provides package and source control information.
 */
USTRUCT(meta = (DisplayName = "Package path reference"))
struct FTypedElementPackageReference final : public FTypedElementDataStorageColumn
{
	GENERATED_BODY()
	
	TypedElementRowHandle Row;
};

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
