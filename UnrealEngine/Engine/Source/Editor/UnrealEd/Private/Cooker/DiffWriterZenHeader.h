// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiffWriterArchive.h"
#include "Serialization/ZenPackageHeader.h"

namespace UE::DiffWriter
{

/**
 * Helper object for FDiffWriterZenHeader: Stores the map from PublicExportHash to package-relative
 * objectpath for all of the objects in a package, used to convert a ZenPackageHeader's import's
 * FPackageObjectIndex into the full path to the imported object
 */
class FZenPackageExportsForDiff
{
public:
	void Initialize(FAccumulatorGlobals& Globals, FName InPackageName, bool bUseZenStore);
	FStringView GetExportPackageRelativePath(uint64 PublicExportHash);

private:
	void InitializeFromZenStore(FAccumulatorGlobals& Globals);
	void InitializeFromMemory(FAccumulatorGlobals& Globals);

private:
	FName PackageName = NAME_None;
	TMap<uint64, FString> ExportPaths;
};

/** Adapter type for FDiffWriterZenHeader to represent its exports in DumpTableDifferences. */
struct FZenHeaderIndexIntoExportMap
{
	/** Index of the given export in FZenPackageHeader::ExportMap. */
	int32 Index;
};

/**
 * Context object for DumpTableDifferences when we are using EPackageHeaderFormat::ZenPackageSummary and so the
 * header information comes from ZenStore's optimized package format parsed into FZenPackageHeader.
*/
class FDiffWriterZenHeader
{
public:
	// Constructor and simple data readers
	FDiffWriterZenHeader(FAccumulatorGlobals& InGlobals, const FMessageCallback& InMessageCallback,
		bool bInUseZenStoreForImports, const FPackageData& Package, const FString& AssetFilename,
		const TCHAR* WhichComparisonPackage);
	bool IsValid() const;
	const FZenPackageHeader& GetPackageHeader() const;

	// Optional data loaded on demand
	FStringView GetObjectIndexPathName(FPackageObjectIndex PackageObjectIndex);

	// API for DumpTableDifferences
	bool IsNameMapIdentical(FDiffWriterZenHeader& DestContext,
		const TArray<FString>& SourceNames, const TArray<FString>& DestNames);
	FString GetTableKey(const FString& Id);
	bool CompareTableItem(FDiffWriterZenHeader& DestContext, const FString& SourceText, const FString& DestText);
	FString ConvertItemToText(const FString& Id);

	bool IsImportMapIdentical(FDiffWriterZenHeader& DestContext);
	FString GetTableKey(FPackageObjectIndex Id);
	bool CompareTableItem(FDiffWriterZenHeader& DestContext, const FPackageObjectIndex& SourceIndex,
		const FPackageObjectIndex& DestIndex);
	FString ConvertItemToText(FPackageObjectIndex Id);

	bool IsExportMapIdentical(FDiffWriterZenHeader& DestContext);
	FString GetTableKey(const FZenHeaderIndexIntoExportMap& Id);
	bool CompareTableItem(FDiffWriterZenHeader& DestContext, const FZenHeaderIndexIntoExportMap& SourceExport,
		const FZenHeaderIndexIntoExportMap& DestExport);
	FString ConvertItemToText(const FZenHeaderIndexIntoExportMap& Id);

	void LogMessage(ELogVerbosity::Type Verbosity, FStringView Message);

private:
	FZenPackageHeader PackageHeader;
	TMap<FName, FZenPackageExportsForDiff> ImportedPackageExports;
	TMap<FPackageObjectIndex, FString> ObjectIndexPathNames;
	FAccumulatorGlobals& Globals;
	const FMessageCallback& MessageCallback;
	bool bUseZenStoreForImports = false;
	bool bValid = true;
};

}