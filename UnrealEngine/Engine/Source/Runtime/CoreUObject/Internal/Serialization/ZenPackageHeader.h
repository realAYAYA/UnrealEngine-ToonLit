// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "IO/PackageId.h"
#include "Memory/MemoryFwd.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/MappedName.h"
#include "UObject/NameTypes.h"

struct FZenPackageHeader
{
	uint32 CookedHeaderSize = 0;
	uint32 ExportCount = 0; // Need to keep this count around after ExportMap is cleared
	TOptional<FZenPackageVersioningInfo> VersioningInfo;
	FNameMap NameMap;
	FName PackageName;

	// Backed by IoBuffer
	const FZenPackageSummary* PackageSummary = nullptr;
	TArrayView<const uint64> ImportedPublicExportHashes;
	TArrayView<const FPackageObjectIndex> ImportMap;
	TArrayView<const FExportMapEntry> ExportMap;
	TArrayView<const FBulkDataMapEntry> BulkDataMap;
	TArrayView<const FExportBundleEntry> ExportBundleEntries;
	TArrayView<const FDependencyBundleHeader> DependencyBundleHeaders;
	TArrayView<const FDependencyBundleEntry> DependencyBundleEntries;

	TArray<FName> ImportedPackageNames;

	COREUOBJECT_API static FZenPackageHeader MakeView(FMemoryView Memory);
	COREUOBJECT_API static FZenPackageHeader MakeView(FMemoryView Memory, FString& OutError);
	COREUOBJECT_API void Reset();
};
