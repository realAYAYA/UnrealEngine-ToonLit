// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cooker/DiffWriterZenHeader.h"

#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace UE::DiffWriter
{

FDiffWriterZenHeader::FDiffWriterZenHeader(FAccumulatorGlobals& InGlobals,
	const FMessageCallback& InMessageCallback, bool bInUseZenStoreForImports,
	const FPackageData& Package, const FString& AssetFilename, const TCHAR* WhichComparisonPackage)
	: Globals(InGlobals)
	, MessageCallback(InMessageCallback)
	, bUseZenStoreForImports(bInUseZenStoreForImports)
{
	FString ErrorMessage;
	PackageHeader = FZenPackageHeader::MakeView(FMemoryView(Package.Data, Package.Size), ErrorMessage);
	if (!ErrorMessage.IsEmpty())
	{
		InMessageCallback(ELogVerbosity::Warning, FString::Printf(
			TEXT("%s: header is different, but cannot display differences. Package data for %s is invalid: %s"),
			*AssetFilename, WhichComparisonPackage, *ErrorMessage));
		PackageHeader = FZenPackageHeader();
		bValid = false;
	}
}

bool FDiffWriterZenHeader::IsValid() const
{
	return bValid;
}

const FZenPackageHeader& FDiffWriterZenHeader::GetPackageHeader() const
{
	return PackageHeader;
}

FStringView FDiffWriterZenHeader::GetObjectIndexPathName(FPackageObjectIndex PackageObjectIndex)
{
	FString* PathName = &ObjectIndexPathNames.FindOrAdd(PackageObjectIndex);
	if (!PathName->IsEmpty())
	{
		return *PathName;
	}

	if (PackageObjectIndex.IsExport())
	{
		uint32 ExportIndex = PackageObjectIndex.ToExport();
		if (!PackageHeader.ExportMap.IsValidIndex(ExportIndex))
		{
			*PathName = TEXT("InvalidExport");
			return *PathName;
		}
		const FExportMapEntry& Export = PackageHeader.ExportMap[ExportIndex];
		if (Export.OuterIndex.IsNull())
		{
			PackageHeader.NameMap.GetName(Export.ObjectName).ToString(*PathName);
		}
		else
		{
			*PathName = TEXT("<Cycle>"); // Set to non-empty to prevent cycles
			FStringView ParentText = GetObjectIndexPathName(Export.OuterIndex);
			// PathName is no longer usable because we potentially modified ObjectIndexPathNames when we called GetObjectIndexPathName
			PathName = ObjectIndexPathNames.Find(PackageObjectIndex);
			check(PathName);
			*PathName = ParentText;
			PathName->AppendChar('/');
			PackageHeader.NameMap.GetName(Export.ObjectName).AppendString(*PathName);
		}
	}
	else if (PackageObjectIndex.IsScriptImport())
	{
		FPackageStoreOptimizer::FScriptObjectData* ObjectData = Globals.ScriptObjectsMap.Find(PackageObjectIndex);
		if (ObjectData)
		{
			*PathName = ObjectData->FullName;
		}
		else
		{
			*PathName = TEXT("<UnknownScriptImport>");
		}
	}
	else if (PackageObjectIndex.IsPackageImport())
	{
		FPackageImportReference Ref = PackageObjectIndex.ToPackageImportRef();
		if (PackageHeader.ImportedPackageNames.IsValidIndex(Ref.GetImportedPackageIndex()))
		{
			FName PackageName = PackageHeader.ImportedPackageNames[Ref.GetImportedPackageIndex()];
			FZenPackageExportsForDiff& PackageExports = ImportedPackageExports.FindOrAdd(PackageName);
			PackageExports.Initialize(Globals, PackageName, bUseZenStoreForImports);
			uint64 ExportHash = PackageHeader.ImportedPublicExportHashes[Ref.GetImportedPublicExportHashIndex()];
			FStringView ExportName = PackageExports.GetExportPackageRelativePath(ExportHash);
			if (ExportName.StartsWith('/'))
			{
				ExportName.RightChopInline(1);
			}
			*PathName = FString::Printf(TEXT("%s.%.*s"),
				*WriteToString<256>(PackageName), ExportName.Len(), ExportName.GetData());
		}
		else
		{
			*PathName = TEXT("<UnknownPackageImport>");
		}
	}
	else
	{
		check(PackageObjectIndex.IsNull());
		*PathName = TEXT("NULL");
	}
	return *PathName;
}

bool FDiffWriterZenHeader::IsNameMapIdentical(FDiffWriterZenHeader& DestContext,
	const TArray<FString>& SourceNames, const TArray<FString>& DestNames)
{
	int32 NumNames = SourceNames.Num();
	if (NumNames != DestNames.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < NumNames; ++Index)
	{
		if (SourceNames[Index].Compare(DestNames[Index], ESearchCase::CaseSensitive) != 0)
		{
			return false;
		}
	}
	return true;
}

FString FDiffWriterZenHeader::GetTableKey(const FString& Id)
{
	return Id;
}

bool FDiffWriterZenHeader::CompareTableItem(FDiffWriterZenHeader& DestContext,
	const FString& SourceName, const FString& DestName)
{
	return SourceName.Compare(DestName, ESearchCase::CaseSensitive) == 0;
}

FString FDiffWriterZenHeader::ConvertItemToText(const FString& Id)
{
	return Id;
}

bool FDiffWriterZenHeader::IsImportMapIdentical(FDiffWriterZenHeader& DestContext)
{
	int32 NumImports = PackageHeader.ImportMap.Num();
	if (NumImports != DestContext.PackageHeader.ImportMap.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < NumImports; ++Index)
	{
		if (!CompareTableItem(DestContext, PackageHeader.ImportMap[Index],
			DestContext.PackageHeader.ImportMap[Index]))
		{
			return false;
		}
	}
	return true;
}

FString FDiffWriterZenHeader::GetTableKey(FPackageObjectIndex Id)
{
	// TODO: When Id is an import, we could calculate the key more efficiently by changing it to the PackageId + ExportHash instead
	// of looking up the name of the ExportHash
	return FString(GetObjectIndexPathName(Id));
}

bool FDiffWriterZenHeader::CompareTableItem(FDiffWriterZenHeader& DestContext, const FPackageObjectIndex& SourceIndex,
	const FPackageObjectIndex& DestIndex)
{
	if (SourceIndex.IsImport())
	{
		if (!DestIndex.IsImport())
		{
			return false;
		}
		// TODO: we could compare more efficiently by comparing PackageId + ExportHash instead
		// of looking up the name of the ExportHash
		return GetObjectIndexPathName(SourceIndex) == DestContext.GetObjectIndexPathName(DestIndex);
	}
	else if (SourceIndex.IsExport())
	{
		if (!DestIndex.IsExport())
		{
			return false;
		}
		return GetObjectIndexPathName(SourceIndex) == DestContext.GetObjectIndexPathName(DestIndex);
	}
	else
	{
		check(SourceIndex.IsNull());
		return DestIndex.IsNull();
	}
}

FString FDiffWriterZenHeader::ConvertItemToText(FPackageObjectIndex Id)
{
	return FString(GetObjectIndexPathName(Id));
}

bool FDiffWriterZenHeader::IsExportMapIdentical(FDiffWriterZenHeader& DestContext)
{
	int32 NumExports = PackageHeader.ExportMap.Num();
	if (NumExports != DestContext.PackageHeader.ExportMap.Num())
	{
		return false;
	}

	for (int32 ExportIndex = 0; ExportIndex < NumExports; ++ExportIndex)
	{
		FZenHeaderIndexIntoExportMap ExportIndexStruct{ ExportIndex };
		if (!CompareTableItem(DestContext, ExportIndexStruct, ExportIndexStruct))
		{
			return false;
		}
	}
	return true;
}

FString FDiffWriterZenHeader::GetTableKey(const FZenHeaderIndexIntoExportMap& Index)
{
	return FString(GetObjectIndexPathName(FPackageObjectIndex::FromExportIndex(Index.Index)));
}

bool FDiffWriterZenHeader::CompareTableItem(FDiffWriterZenHeader& DestContext,
	const FZenHeaderIndexIntoExportMap& SourceExportIndex, const FZenHeaderIndexIntoExportMap& DestExportIndex)
{
	if (!PackageHeader.ExportMap.IsValidIndex(SourceExportIndex.Index) ||
		!DestContext.PackageHeader.ExportMap.IsValidIndex(DestExportIndex.Index))
	{
		return false;
	}
	const FExportMapEntry& SourceExport = PackageHeader.ExportMap[SourceExportIndex.Index];
	const FExportMapEntry& DestExport = DestContext.PackageHeader.ExportMap[DestExportIndex.Index];

	// Ignore CookedSerialOffset; it is irrelevant to the comparison of the export
	if (SourceExport.CookedSerialSize != DestExport.CookedSerialSize) return false;
	if (PackageHeader.NameMap.GetName(SourceExport.ObjectName) !=
		DestContext.PackageHeader.NameMap.GetName(DestExport.ObjectName)) return false;
	if (!CompareTableItem(DestContext, SourceExport.OuterIndex, DestExport.OuterIndex)) return false;
	if (!CompareTableItem(DestContext, SourceExport.ClassIndex, DestExport.ClassIndex)) return false;
	if (!CompareTableItem(DestContext, SourceExport.SuperIndex, DestExport.SuperIndex)) return false;
	if (!CompareTableItem(DestContext, SourceExport.TemplateIndex, DestExport.TemplateIndex)) return false;
	if (SourceExport.PublicExportHash != DestExport.PublicExportHash) return false;
	if (SourceExport.ObjectFlags != DestExport.ObjectFlags) return false;
	if (SourceExport.FilterFlags != DestExport.FilterFlags) return false;

	return true;
}

FString FDiffWriterZenHeader::ConvertItemToText(const FZenHeaderIndexIntoExportMap& Index)
{
	if (!PackageHeader.ExportMap.IsValidIndex(Index.Index))
	{
		return TEXT("<InvalidExport>");
	}
	const FExportMapEntry& Export = PackageHeader.ExportMap[Index.Index];
	FString ClassName = FString(GetObjectIndexPathName(Export.ClassIndex));
	FString PathName = FString(GetObjectIndexPathName(FPackageObjectIndex::FromExportIndex(Index.Index)));
	FString SuperName = FString(GetObjectIndexPathName(Export.SuperIndex));
	FString TemplateName = FString(GetObjectIndexPathName(Export.TemplateIndex));

	return FString::Printf(TEXT("%s %s Super: %s, Template: %s, Flags: %d, Size: %" INT64_FMT ", FilterFlags: %d"),
		*ClassName,
		*PathName,
		*SuperName,
		*TemplateName,
		(int32)Export.ObjectFlags,
		Export.CookedSerialSize,
		(int32)Export.FilterFlags);
}

void FDiffWriterZenHeader::LogMessage(ELogVerbosity::Type Verbosity, FStringView Message)
{
	MessageCallback(Verbosity, Message);
}

void FZenPackageExportsForDiff::Initialize(FAccumulatorGlobals& Globals, FName InPackageName, bool bUseZenStore)
{
	if (!PackageName.IsNone())
	{
		return;
	}
	PackageName = InPackageName;
	ExportPaths.Reset();
	if (bUseZenStore)
	{
		InitializeFromZenStore(Globals);
	}
	else
	{
		InitializeFromMemory(Globals);
	}
}

void FZenPackageExportsForDiff::InitializeFromZenStore(FAccumulatorGlobals& Globals)
{
	// Silently ignore on errors; GetExportName will report <UnknownExport.%u>
	if (!Globals.PackageWriter)
	{
		return;
	}

	FPackageId PackageId = FPackageId::FromName(PackageName);
	IPackageWriter::FPackageInfo PackageInfo;
	uint16 MultiOutputIndex = 0; // We're reading exports from the non-optional version of the package
	PackageInfo.ChunkId = CreateIoChunkId(PackageId.Value(), MultiOutputIndex, EIoChunkType::ExportBundleData);
	ICookedPackageWriter::FPreviousCookedBytesData PackageBytes;
	if (!Globals.PackageWriter->GetPreviousCookedBytes(PackageInfo, PackageBytes))
	{
		return;
	}
	FString Error;
	FZenPackageHeader PackageHeader = FZenPackageHeader::MakeView(
		FMemoryView(PackageBytes.Data.Get(), PackageBytes.Size), Error);
	if (!Error.IsEmpty())
	{
		return;
	}

	TArray<FString> LocalPackageRelativePaths;
	FString EmptyPath;
	FString UnknownImportPath(TEXT("<UnknownImportInOtherPackage>"));
	FString InvalidExportIndex(TEXT("<InvalidExportIndex>"));
	FString InvalidCycleText(TEXT("<Cycle>"));
	TFunction<const FString& (FPackageObjectIndex)> FindOrConstructPackageRelativePath;
	FindOrConstructPackageRelativePath =
		[&FindOrConstructPackageRelativePath /** Recursive calls */, &LocalPackageRelativePaths, &PackageHeader, &EmptyPath,
		&UnknownImportPath, &InvalidExportIndex, &InvalidCycleText]
	(FPackageObjectIndex Index) -> const FString&
	{
		if (Index.IsNull())
		{
			return EmptyPath;
		}
		if (Index.IsExport())
		{
			uint32 ExportIndex = Index.ToExport();
			if (!LocalPackageRelativePaths.IsValidIndex(ExportIndex))
			{
				return InvalidExportIndex;
			}
			FString& PackageRelativePath = LocalPackageRelativePaths[ExportIndex];
			if (!PackageRelativePath.IsEmpty())
			{
				return PackageRelativePath;
			}
			// Avoid cycles by setting ObjectName before the recursive call to FindOrConstructPackageRelativePath
			PackageRelativePath = InvalidCycleText;

			check(PackageHeader.ExportMap.IsValidIndex(ExportIndex)); // LocalExportNames is the same length and was checked above
			const FExportMapEntry& Export = PackageHeader.ExportMap[ExportIndex];
			FName LeafName;
			if (!PackageHeader.NameMap.TryGetName(Export.ObjectName, LeafName) || LeafName.IsNone())
			{
				PackageRelativePath = InvalidExportIndex;
				return PackageRelativePath;
			}
			const FString& ParentPackageRelativePath = FindOrConstructPackageRelativePath(Export.OuterIndex);

			// Every path name is ParentPath (even if empty) + / + LeafName. PackageRelativePaths when parent is empty start with / 
			PackageRelativePath = ParentPackageRelativePath;
			PackageRelativePath.Append(TEXT("/"));
			LeafName.AppendString(PackageRelativePath);

			// Down-case the path names to match the behavior for the path names constructed from UObjects in memory
			// by FPackageStoreOptimizer::AppendPathForPublicExportHash
			PackageRelativePath.ToLowerInline();

			return PackageRelativePath;
		}
		else
		{
			return UnknownImportPath;
		}
	};

	uint32 NumExports = static_cast<uint32>(PackageHeader.ExportMap.Num());
	LocalPackageRelativePaths.SetNum(NumExports);
	for (uint32 ExportIndex = 0; ExportIndex < NumExports; ++ExportIndex)
	{
		FindOrConstructPackageRelativePath(FPackageObjectIndex::FromExportIndex(ExportIndex));
	}

	ExportPaths.Reserve(NumExports);
	for (uint32 ExportIndex = 0; ExportIndex < NumExports; ++ExportIndex)
	{
		const FExportMapEntry& Export = PackageHeader.ExportMap[ExportIndex];
		ExportPaths.Add(Export.PublicExportHash, LocalPackageRelativePaths[ExportIndex]);
	}
}

void FZenPackageExportsForDiff::InitializeFromMemory(FAccumulatorGlobals& Globals)
{
	// Silently ignore on errors; GetExportName will report <UnknownExport.%u>
	UPackage* Package = FindPackage(nullptr, *WriteToString<256>(PackageName));
	if (!Package)
	{
		return;
	}

	// We do not add the UPackage object itself, because UPackage object is not stored in the list
	// of exports in the disk version of the package
	ForEachObjectWithOuter(Package, [this](UObject* Object)
		{
			TStringBuilder<256> PackageRelativePath;
			FPackageStoreOptimizer::AppendPathForPublicExportHash(Object, PackageRelativePath);
			uint64 PublicExportHash;
			if (FPackageStoreOptimizer::TryGetPublicExportHash(PackageRelativePath, PublicExportHash))
			{
				ExportPaths.Add(PublicExportHash, FString(PackageRelativePath));
			}
		}, true /* bIncludeNestedObjects */);
}

FStringView FZenPackageExportsForDiff::GetExportPackageRelativePath(uint64 PublicExportHash)
{
	FString& ExportPath = ExportPaths.FindOrAdd(PublicExportHash);
	if (ExportPath.IsEmpty())
	{
		ExportPath = FString::Printf(TEXT("<UnknownExport.%u>"), PublicExportHash);
	}
	return ExportPath;
}

}