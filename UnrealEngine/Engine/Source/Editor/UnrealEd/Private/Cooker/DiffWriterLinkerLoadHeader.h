// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DiffWriterArchive.h"

namespace UE::DiffWriter
{

/**
	* Context object for DumpTableDifferences when we are using EPackageHeaderFormat::PackageFileSummary and so the
	* header information comes from FLinkerLoad.
*/
struct FDiffWriterLinkerLoadHeader
{
public:
	FLinkerLoad* Linker = nullptr;
	const FMessageCallback& MessageCallback;

public:
	// Constructor
	FDiffWriterLinkerLoadHeader(FLinkerLoad* InLinker, const FMessageCallback& InMessageCallback);

	// API for DumpTableDifferences
	FString GetTableKey(const FObjectExport& Export);
	FString GetTableKey(const FObjectImport& Import);
	FString GetTableKey(const FName& Name);
	FString GetTableKey(FNameEntryId Id);
	FString GetTableKeyForIndex(FPackageIndex Index);
	FString ConvertItemToText(const FObjectExport& Export);
	FString ConvertItemToText(const FName& Name);
	FString ConvertItemToText(FNameEntryId Id);
	FString ConvertItemToText(const FObjectImport& Import);
	bool CompareTableItem(FDiffWriterLinkerLoadHeader& DestContext, const FName& SourceName, const FName& DestName);
	bool CompareTableItem(FDiffWriterLinkerLoadHeader& DestContext, FNameEntryId SourceName, FNameEntryId DestName);
	bool CompareTableItem(FDiffWriterLinkerLoadHeader& DestContext, const FObjectImport& SourceImport,
		const FObjectImport& DestImport);
	bool CompareTableItem(FDiffWriterLinkerLoadHeader& DestContext, const FObjectExport& SourceExport,
		const FObjectExport& DestExport);
	bool ComparePackageIndices(FDiffWriterLinkerLoadHeader& DestContext, const FPackageIndex& SourceIndex,
		const FPackageIndex& DestIndex);
	bool IsImportMapIdentical(FDiffWriterLinkerLoadHeader& DestContext);
	bool IsExportMapIdentical(FDiffWriterLinkerLoadHeader& DestContext);
	void LogMessage(ELogVerbosity::Type Verbosity, FStringView Message);
};

}