// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetRegistry/PackageReader.h"

#include "AssetRegistry.h"
#include "AssetRegistryPrivate.h"
#include "AssetRegistry/AssetData.h"
#include "HAL/FileManager.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/GatherableTextData.h"
#include "Logging/MessageLog.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/Linker.h"
#include "UObject/LinkerLoad.h"
#include "UObject/PackageRelocation.h"
#include "UObject/PackageTrailer.h"

const TCHAR* LexToString(FPackageReader::EOpenPackageResult Result)
{
	switch(Result)
	{
		case FPackageReader::EOpenPackageResult::Success: return TEXT("Success");
		case FPackageReader::EOpenPackageResult::NoLoader: return TEXT("NoLoader");
		case FPackageReader::EOpenPackageResult::MalformedTag: return TEXT("MalformedTag");
		case FPackageReader::EOpenPackageResult::VersionTooOld: return TEXT("VersionTooOld");
		case FPackageReader::EOpenPackageResult::VersionTooNew: return TEXT("VersionTooNew");
		case FPackageReader::EOpenPackageResult::CustomVersionMissing: return TEXT("CustomVersionMissing");
		case FPackageReader::EOpenPackageResult::CustomVersionInvalid: return TEXT("CustomVersionInvalid");
		case FPackageReader::EOpenPackageResult::Unversioned: return TEXT("Unversioned");
	}
	checkf(false, TEXT("LexToString unimplemented for EOpenPackageResult %d"), (int32)Result);
	return TEXT("UNKNOWN");
}

FPackageReader::FPackageReader()
	: Loader(nullptr)
	, PackageFileSize(0)
	, AssetRegistryDependencyDataOffset(INDEX_NONE)
	, bLoaderOwner(false)
{
	this->SetIsLoading(true);
	this->SetIsPersistent(true);
}

FPackageReader::~FPackageReader()
{
	if (Loader && bLoaderOwner)
	{
		delete Loader;
	}
}

bool FPackageReader::OpenPackageFile(FStringView InPackageFilename, EOpenPackageResult* OutErrorCode)
{
	return OpenPackageFile(FStringView(), InPackageFilename, OutErrorCode);
}

bool FPackageReader::OpenPackageFile(FStringView InLongPackageName, FStringView InPackageFilename, EOpenPackageResult* OutErrorCode)
{
	check(!Loader);
	LongPackageName = InLongPackageName;
	PackageFilename = InPackageFilename;
	Loader = IFileManager::Get().CreateFileReader(*PackageFilename);
	bLoaderOwner = true;
	EOpenPackageResult Tmp = EOpenPackageResult::Success;
	return OpenPackageFile(OutErrorCode ? *OutErrorCode : Tmp); 
}

bool FPackageReader::OpenPackageFile(FArchive* InLoader, EOpenPackageResult* OutErrorCode)
{
	check(!Loader);
	Loader = InLoader;
	bLoaderOwner = false;
	LongPackageName.Empty();
	PackageFilename = Loader->GetArchiveName();
	EOpenPackageResult Tmp = EOpenPackageResult::Success;
	return OpenPackageFile(OutErrorCode ? *OutErrorCode : Tmp); 
}

bool FPackageReader::OpenPackageFile(TUniquePtr<FArchive> InLoader, EOpenPackageResult* OutErrorCode)
{
	check(!Loader);
	Loader = InLoader.Release();
	bLoaderOwner = true;
	LongPackageName.Empty();
	PackageFilename = Loader->GetArchiveName();
	EOpenPackageResult Tmp = EOpenPackageResult::Success;
	return OpenPackageFile(OutErrorCode ? *OutErrorCode : Tmp); 
}

bool FPackageReader::OpenPackageFile(EOpenPackageResult& OutErrorCode)
{
	OutErrorCode = EOpenPackageResult::Success;
	if( Loader == nullptr )
	{
		// Couldn't open the file
		OutErrorCode = EOpenPackageResult::NoLoader;
		return false;
	}

	// Read package file summary from the file
	*this << PackageFileSummary;

	// Validate the summary.

	// Make sure this is indeed a package
	if( PackageFileSummary.Tag != PACKAGE_FILE_TAG || IsError())
	{
		// Unrecognized or malformed package file
		UE_LOG(LogAssetRegistry, Error, TEXT("Package %s has malformed tag"), *PackageFilename);
		OutErrorCode = EOpenPackageResult::MalformedTag;
		return false;
	}

	// IsEnforcePackageCompatibleVersionCheck(): If LinkerLoad is not validating, PackageReader should not either.
	// Optimize the IsEnforcePackageCompatibleVersionCheck==true but no errors case; only test
	// IsEnforcePackageCompatibleVersionCheck after finding a version mismatch.
	if (!PackageFileSummary.IsFileVersionValid() &&
		IsEnforcePackageCompatibleVersionCheck())
	{
		// Log a warning rather than an error. Linkerload gracefully handles this case.
		UE_LOG(LogAssetRegistry, Warning,
			TEXT("Package %s is unversioned which cannot be opened by the current process"), *PackageFilename);
		OutErrorCode = EOpenPackageResult::Unversioned;
		return false;
	}

	// Don't read packages that are too old
	if (PackageFileSummary.IsFileVersionTooOld() &&
		IsEnforcePackageCompatibleVersionCheck())
	{
		// Log a warning rather than an error. Linkerload gracefully handles this case.
		UE_LOG(LogAssetRegistry, Warning, TEXT("Package %s is too old. Min Version: %i  Package Version: %i"),
			*PackageFilename, (int32)VER_UE4_OLDEST_LOADABLE_PACKAGE,
			PackageFileSummary.GetFileVersionUE().FileVersionUE4);

		OutErrorCode = EOpenPackageResult::VersionTooOld;
		return false;
	}

	// Don't read packages that were saved with a package version newer than the current one.
	if (PackageFileSummary.IsFileVersionTooNew() &&
		IsEnforcePackageCompatibleVersionCheck())
	{
		// Log a warning rather than an error. Linkerload gracefully handles this case.
		UE_LOG(LogAssetRegistry, Warning, TEXT("Package %s is too new. Engine Version: %i  Package Version: %i"),
			*PackageFilename, GPackageFileUEVersion.ToValue(), PackageFileSummary.GetFileVersionUE().ToValue());

		OutErrorCode = EOpenPackageResult::VersionTooNew;
		return false;
	}

	if (PackageFileSummary.GetFileVersionLicenseeUE() > GPackageFileLicenseeUEVersion &&
		IsEnforcePackageCompatibleVersionCheck())
	{
		// Log a warning rather than an error. Linkerload gracefully handles this case.
		UE_LOG(LogAssetRegistry, Warning, TEXT("Package %s is too new. Licensee Version: %i Package Licensee Version: %i"),
			*PackageFilename, GPackageFileLicenseeUEVersion, PackageFileSummary.GetFileVersionLicenseeUE());

		OutErrorCode = EOpenPackageResult::VersionTooNew;
		return false;
	}

	// Check serialized custom versions against latest custom versions.
	TArray<FCustomVersionDifference> Diffs = FCurrentCustomVersions::Compare(
		PackageFileSummary.GetCustomVersionContainer().GetAllVersions(), *PackageFilename);
	for (FCustomVersionDifference Diff : Diffs)
	{
		if (Diff.Type == ECustomVersionDifference::Missing)
		{
			if (IsEnforcePackageCompatibleVersionCheck())
			{
				OutErrorCode = EOpenPackageResult::CustomVersionMissing;
			}
		}
		else if (Diff.Type == ECustomVersionDifference::Invalid)
		{
			if (IsEnforcePackageCompatibleVersionCheck())
			{
				OutErrorCode = EOpenPackageResult::CustomVersionInvalid;
			}
		}
		else if (Diff.Type == ECustomVersionDifference::Newer)
		{
			if (IsEnforcePackageCompatibleVersionCheck())
			{
				UE_LOG(LogAssetRegistry, Error, TEXT("Package %s has newer custom version of %s"),
					*PackageFilename, *Diff.Version->GetFriendlyName().ToString());
				OutErrorCode = EOpenPackageResult::VersionTooNew;
			}
		}
		// else ECustomVersionDifference::Older, which is not a problem
	}

	//make sure the filereader gets the correct version number (it defaults to latest version)
	SetUEVer(PackageFileSummary.GetFileVersionUE());
	SetLicenseeUEVer(PackageFileSummary.GetFileVersionLicenseeUE());
	SetEngineVer(PackageFileSummary.SavedByEngineVersion);

	const FCustomVersionContainer& PackageFileSummaryVersions = PackageFileSummary.GetCustomVersionContainer();
	SetCustomVersions(PackageFileSummaryVersions);
	
	SetUseUnversionedPropertySerialization((PackageFileSummary.GetPackageFlags() & EPackageFlags::PKG_UnversionedProperties) != 0);

	PackageFileSize = Loader->TotalSize();
	
	if (LongPackageName.IsEmpty())
	{
		LongPackageName = PackageFileSummary.PackageName;
	}

	return OutErrorCode == EOpenPackageResult::Success;
}

bool FPackageReader::TryGetLongPackageName(FString& OutLongPackageName) const
{
	if (!LongPackageName.IsEmpty())
	{
		OutLongPackageName = LongPackageName;
		return true;
	}
	else
	{
		return FPackageName::TryConvertFilenameToLongPackageName(PackageFilename, OutLongPackageName);
	}
}

FString FPackageReader::GetLongPackageName() const
{
	FString Result;
	verify(TryGetLongPackageName(Result));
	return Result;
}

bool FPackageReader::StartSerializeSection(int64 Offset)
{
	check(Loader);
	if (Offset <= 0 || Offset > PackageFileSize)
	{
		return false;
	}
	ClearError();
	Loader->ClearError();
	Seek(Offset);
	return !IsError();
}

#define UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING(MessageKey, PackageFileName) \
	do \
	{\
		FFormatNamedArguments CorruptPackageWarningArguments; \
		CorruptPackageWarningArguments.Add(TEXT("FileName"), FText::FromString(PackageFileName)); \
		UE_LOG(LogAssetRegistry, Warning, TEXT("%s"), \
			*FText::Format(NSLOCTEXT("AssetRegistry", MessageKey, \
			"Cannot read AssetRegistry Data in {FileName}, skipping it. Error: " MessageKey "."), \
			CorruptPackageWarningArguments).ToString()); \
	} while (false)


const FPackageFileSummary& FPackageReader::GetPackageFileSummary() const
{
	return PackageFileSummary;
}

bool FPackageReader::GetNames(TArray<FName>& OutNames)
{
	if (!SerializeNameMap())
	{
		return false;
	}	
	OutNames = NameMap;
	return true;
}

bool FPackageReader::GetImports(TArray<FObjectImport>& OutImportMap)
{
	if (!SerializeNameMap() || !SerializeImportMap())
	{
		return false;
	}
	OutImportMap = ImportMap;
	return true;
}

bool FPackageReader::GetExports(TArray<FObjectExport>& OutExportMap)
{
	if (!SerializeNameMap() || !SerializeExportMap())
	{
		return false;
	}
	OutExportMap = ExportMap;
	return true;
}

bool FPackageReader::GetDependsMap(TArray<TArray<FPackageIndex>>& OutDependsMap)
{
	if (!SerializeDependsMap())
	{
		return false;
	}
	OutDependsMap = DependsMap;
	return true;
}

bool FPackageReader::GetSoftPackageReferenceList(TArray<FName>& OutSoftPackageReferenceList)
{
	if (!SerializeNameMap() || !SerializeSoftPackageReferenceList())
	{
		return false;
	}
	OutSoftPackageReferenceList = SoftPackageReferenceList;
	return true;
}

bool FPackageReader::GetSoftObjectPaths(TArray<FSoftObjectPath>& OutSoftObjectPaths)
{
	if (!SerializeNameMap() || !SerializeSoftObjectPathMap())
	{
		return false;
	}
	OutSoftObjectPaths = SoftObjectPathMap;
	return true;
}

bool FPackageReader::GetGatherableTextData(TArray<FGatherableTextData>& OutText)
{
	if (!SerializeGatherableTextDataMap())
	{
		return false;
	}
	OutText = GatherableTextDataMap;
	return true;
}

bool FPackageReader::GetThumbnails(TArray<FObjectFullNameAndThumbnail>& OutThumbnails)
{
	if (!SerializeThumbnailMap())
	{
		return false;
	}	
	
	OutThumbnails = ThumbnailMap;
	return true;
}

bool FPackageReader::ReadEditorOnlyFlags(TBitArray<>& OutImportUsedInGame, TBitArray<>& OutSoftPackageUsedInGame)
{
	if (!SerializeNameMap() || !SerializeImportMap() || !SerializeSoftPackageReferenceList())
	{
		return false;
	}
	if (!SerializeEditorOnlyFlags(OutImportUsedInGame, OutSoftPackageUsedInGame))
	{
		return false;
	}
	return true;
}

bool FPackageReader::ReadImportedClasses(TArray<FName>& OutClassNames)
{
	if (!SerializeNameMap() || !SerializeImportMap())
	{
		return false;
	}
	if (!SerializeImportedClasses(ImportMap, OutClassNames))
	{
		return false;
	}
	return true;
}

bool FPackageReader::ReadAssetRegistryData(TArray<FAssetData*>& AssetDataList, bool& bOutIsCookedWithoutAssetData)
{
	bOutIsCookedWithoutAssetData = false;
	if (!!(GetPackageFlags() & PKG_FilterEditorOnly))
	{
		return ReadAssetRegistryDataFromCookedPackage(AssetDataList, bOutIsCookedWithoutAssetData);
	}

	if (!SerializeNameMap() || !SerializeImportMap() || !SerializeExportMap())
	{
		return false;
	}

	if (!StartSerializeSection(PackageFileSummary.AssetRegistryDataOffset))
	{
		if (!ReadAssetDataFromThumbnailCache(AssetDataList))
		{
			// Legacy files without AR data and without a thumbnail cache are treated as having no assets
			AssetDataList.Reset();
		}
		return true;
	}

	// Determine the package name and path
	FString PackageName;
	if (!TryGetLongPackageName(PackageName))
	{
		// Path was possibly unmounted
		return false;
	}

	using namespace UE::AssetRegistry;

	EReadPackageDataMainErrorCode ErrorCode;
	if (!ReadPackageDataMain(*this, PackageName, PackageFileSummary, AssetRegistryDependencyDataOffset, AssetDataList, ErrorCode,
		&ImportMap, &ExportMap))
	{
		switch (ErrorCode)
		{
		case EReadPackageDataMainErrorCode::InvalidObjectCount:
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("EReadPackageDataMainErrorCode::InvalidObjectCount", PackageFilename);
			break;
		case EReadPackageDataMainErrorCode::InvalidTagCount:
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("EReadPackageDataMainErrorCode::InvalidTagCount", PackageFilename);
			break;
		case EReadPackageDataMainErrorCode::InvalidTag:
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("EReadPackageDataMainErrorCode::InvalidTag", PackageFilename);
			break;
		default:
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("EReadPackageDataMainErrorCode::Unknown", PackageFilename);
			break;
		}
		return false;
	}

	return true;
}

bool FPackageReader::ReadLinkerObjects(TMap<FSoftObjectPath, FObjectData>& OutExports,
	TMap<FSoftObjectPath, FObjectData>& OutImports, TMap<FName, bool>& OutSoftPackageReferences)
{
	if (!SerializeNameMap() || !SerializeImportMap() || !SerializeExportMap() || !SerializeSoftPackageReferenceList())
	{
		return false;
	}
	TBitArray<> ImportUsedInGame;
	TBitArray<> SoftPackageUsedInGame;
	if (!SerializeEditorOnlyFlags(ImportUsedInGame, SoftPackageUsedInGame))
	{
		return false;
	}

	TArray<FSoftObjectPath> ExportsPaths;
	TArray<FSoftObjectPath> ImportsPaths;
	int32 NumExports = ExportMap.Num();
	int32 NumImports = ImportMap.Num();
	int32 NumSoftPackageReferences = SoftPackageReferenceList.Num();
	ConvertLinkerTableToPaths(FName(*GetLongPackageName()), ExportMap, ImportMap, ExportsPaths, ImportsPaths);
	auto GetClassPath = [&ExportsPaths, &ImportsPaths](FPackageIndex ClassIndex)
	{
		if (ClassIndex.IsNull())
		{
			return FSoftObjectPath(TEXT("/Script/CoreUObject.Class"));
		}
		else if (ClassIndex.IsExport())
		{
			int32 ExportIndex = ClassIndex.ToExport();
			return ExportIndex < ExportsPaths.Num() ? ExportsPaths[ExportIndex] :
				FSoftObjectPath(TEXT("/Script/Unknown.Unknown"));
		}
		else
		{
			int32 ImportIndex = ClassIndex.ToImport();
			return ImportIndex < ImportsPaths.Num() ? ImportsPaths[ImportIndex] :
				FSoftObjectPath(TEXT("/Script/Unknown.Unknown"));
		}
	};
	OutExports.Reserve(NumExports);
	for (int32 Index = 0; Index < NumExports; ++Index)
	{
		FObjectData Data;
		Data.ClassPath = GetClassPath(ExportMap[Index].ClassIndex);
		Data.bUsedInGame = true;
		OutExports.Add(ExportsPaths[Index], MoveTemp(Data));
	}
	OutImports.Reserve(NumImports);
	for (int32 Index = 0; Index < NumImports; ++Index)
	{
		FObjectData Data;
		const FObjectImport& Import = ImportMap[Index];
		Data.ClassPath = FSoftObjectPath(Import.ClassPackage, Import.ClassName, FString());
		Data.bUsedInGame = ImportUsedInGame[Index];
		OutImports.Add(ImportsPaths[Index], MoveTemp(Data));
	}
	OutSoftPackageReferences.Reserve(NumSoftPackageReferences);
	for (int32 Index = 0; Index < NumSoftPackageReferences; ++Index)
	{
		OutSoftPackageReferences.Add(SoftPackageReferenceList[Index], SoftPackageUsedInGame[Index]);
	}

	auto SoftObjectPathLexicalLess = [](const FSoftObjectPath& A, const FSoftObjectPath& B) { return A.LexicalLess(B); };
	OutExports.KeySort(SoftObjectPathLexicalLess);
	OutImports.KeySort(SoftObjectPathLexicalLess);
	OutSoftPackageReferences.KeySort(FNameLexicalLess());
	return true;
}

bool FPackageReader::SerializeAssetRegistryDependencyData(TBitArray<>& OutImportUsedInGame, TBitArray<>& OutSoftPackageUsedInGame,
	const TArray<FObjectImport>& InImportMap, const TArray<FName>& InSoftPackageReferenceList)
{
	if (AssetRegistryDependencyDataOffset == INDEX_NONE)
	{
		// For old package versions that did not write out the dependency flags, set default values of the flags
		OutImportUsedInGame.Init(true, InImportMap.Num());
		OutSoftPackageUsedInGame.Init(true, InSoftPackageReferenceList.Num());
		return true;
	}

	if (!StartSerializeSection(AssetRegistryDependencyDataOffset))
	{
		return false;
	}

	if (!UE::AssetRegistry::ReadPackageDataDependencies(*this, OutImportUsedInGame, OutSoftPackageUsedInGame) ||
		OutImportUsedInGame.Num() != InImportMap.Num() ||
		OutSoftPackageUsedInGame.Num() != InSoftPackageReferenceList.Num())
	{
		UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeAssetRegistryDependencyData", PackageFilename);
		return false;
	}
	return true;
}

bool FPackageReader::SerializePackageTrailer(FAssetPackageData& PackageData)
{
	if (!StartSerializeSection(PackageFileSummary.PayloadTocOffset))
	{
		PackageData.SetHasVirtualizedPayloads(false);
		return true;
	}

	UE::FPackageTrailer Trailer;
	if (!Trailer.TryLoad(*this))
	{
		// This is not necessarily corrupt; TryLoad will return false if the trailer is empty
		PackageData.SetHasVirtualizedPayloads(false);
		return true;
	}

	PackageData.SetHasVirtualizedPayloads(Trailer.GetNumPayloads(UE::EPayloadStorageType::Virtualized) > 0);
	return true;
}

void FPackageReader::ApplyRelocationToImportMapAndSoftPackageReferenceList(FStringView LoadedPackageName, TArray<FName>& OutSoftPackageReferenceList)
{
#if WITH_EDITOR
	UE::Package::Relocation::Private::FPackageRelocationContext RelocationArgs;
	if (UE::Package::Relocation::Private::ShouldApplyRelocation(PackageFileSummary, LoadedPackageName, RelocationArgs))
	{
		UE_LOG(LogPackageRelocation, Verbose, TEXT("Detected relocated package (%.*s). The package was saved as (%s)."), LoadedPackageName.Len(), LoadedPackageName.GetData(), *PackageFileSummary.PackageName);
		UE::Package::Relocation::Private::ApplyRelocationToObjectImportMap(RelocationArgs, ImportMap);
		UE::Package::Relocation::Private::ApplyRelocationToNameArray(RelocationArgs, OutSoftPackageReferenceList);
	}
#endif
}

bool FPackageReader::ReadAssetDataFromThumbnailCache(TArray<FAssetData*>& AssetDataList)
{
	if (!SerializeThumbnailMap())
	{
		return false;
	}

	// Iterate over every thumbnail entry and harvest the objects classnames
	for(const FObjectFullNameAndThumbnail& Thumbnail : ThumbnailMap)
	{
		FString ClassName;
		FString PackageName;
		FString ObjectName;
		FString SubobjectName;
		FPackageName::SplitFullObjectPath(Thumbnail.ObjectFullName.ToString(), ClassName, PackageName, ObjectName, SubobjectName);
		FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		// Create a new FAssetData for this asset and update it with the gathered data
		AssetDataList.Add(new FAssetData(
			FName(PackageName),
			FName(PackagePath), 
			FName(ObjectName), // AssetName
			FTopLevelAssetPath(ClassName), 
			FAssetDataTagMap(), 
			PackageFileSummary.ChunkIDs, 
			PackageFileSummary.GetPackageFlags())
		);
	}

	return true;
}

bool FPackageReader::ReadAssetRegistryDataFromCookedPackage(TArray<FAssetData*>& AssetDataList, bool& bOutIsCookedWithoutAssetData)
{
	FString PackageName;
	if (!TryGetLongPackageName(PackageName))
	{
		return false;
	}

	bool bFoundAtLeastOneAsset = false;

	// If the packaged is saved with the right version we have the information
	// which of the objects in the export map as the asset.
	// Otherwise we need to store a temp minimal data and then force load the asset
	// to re-generate its registry data
	if (UEVer() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		if (!SerializeNameMap() || !SerializeImportMap() || !SerializeExportMap())
		{
			return false;
		}
		for (const FObjectExport& Export : ExportMap)
		{
			if (Export.bIsAsset)
			{
				// We need to get the class name from the import/export maps
				FString ObjectClassName;
				if (Export.ClassIndex.IsNull())
				{
					ObjectClassName = UClass::StaticClass()->GetPathName();
				}
				else if (Export.ClassIndex.IsExport())
				{
					const FObjectExport& ClassExport = ExportMap[Export.ClassIndex.ToExport()];
					ObjectClassName = PackageName;
					ObjectClassName += '.'; 
					ClassExport.ObjectName.AppendString(ObjectClassName);
				}
				else if (Export.ClassIndex.IsImport())
				{
					const FObjectImport& ClassImport = ImportMap[Export.ClassIndex.ToImport()];
					const FObjectImport& ClassPackageImport = ImportMap[ClassImport.OuterIndex.ToImport()];
					ClassPackageImport.ObjectName.AppendString(ObjectClassName);
					ObjectClassName += '.';
					ClassImport.ObjectName.AppendString(ObjectClassName);
				}

				AssetDataList.Add(new FAssetData(FName(*PackageName), FName(*PackagePath), Export.ObjectName, FTopLevelAssetPath(ObjectClassName), FAssetDataTagMap(), TArray<int32>(), GetPackageFlags()));
				bFoundAtLeastOneAsset = true;
			}
		}
	}
	bOutIsCookedWithoutAssetData = !bFoundAtLeastOneAsset;
	return true;
}

bool FPackageReader::ReadDependencyData(FPackageDependencyData& OutDependencyData, EReadOptions Options)
{
	FString PackageNameString;
	if (!TryGetLongPackageName(PackageNameString))
	{
		// Path was possibly unmounted
		return false;
	}

	OutDependencyData.PackageName = FName(*PackageNameString);
	if (!EnumHasAnyFlags(Options, EReadOptions::PackageData | EReadOptions::Dependencies))
	{
		return true;
	}

	if (!SerializeNameMap() || !SerializeImportMap())
	{
		return false;
	}

	if (EnumHasAnyFlags(Options, EReadOptions::PackageData))
	{
		OutDependencyData.bHasPackageData = true;
		FAssetPackageData& PackageData = OutDependencyData.PackageData;
		PackageData.DiskSize = PackageFileSize;
#if WITH_EDITORONLY_DATA
		PackageData.SetPackageSavedHash(PackageFileSummary.GetSavedHash());
#endif
		PackageData.SetCustomVersions(PackageFileSummary.GetCustomVersionContainer().GetAllVersions());
		PackageData.FileVersionUE = PackageFileSummary.GetFileVersionUE();
		PackageData.FileVersionLicenseeUE = PackageFileSummary.GetFileVersionLicenseeUE();
		PackageData.SetIsLicenseeVersion(PackageFileSummary.SavedByEngineVersion.IsLicenseeVersion());
		PackageData.Extension = FPackagePath::ParseExtension(PackageFilename);

		if (!SerializeImportedClasses(ImportMap, PackageData.ImportedClasses))
		{
			return false;
		}
		if (!SerializePackageTrailer(PackageData))
		{
			return false;
		}
	}

	if (EnumHasAnyFlags(Options, EReadOptions::Dependencies))
	{
		OutDependencyData.bHasDependencyData = true;
		if (!SerializeSoftPackageReferenceList())
		{
			return false;
		}
		FLinkerTables SearchableNames;
		if (!SerializeSearchableNamesMap(SearchableNames))
		{
			return false;
		}

		TBitArray<> ImportUsedInGame;
		TBitArray<> SoftPackageUsedInGame;
		if (!SerializeAssetRegistryDependencyData(ImportUsedInGame, SoftPackageUsedInGame, ImportMap,
			SoftPackageReferenceList))
		{
			return false;
		}

		ApplyRelocationToImportMapAndSoftPackageReferenceList(PackageNameString, SoftPackageReferenceList);

		OutDependencyData.LoadDependenciesFromPackageHeader(OutDependencyData.PackageName, ImportMap, SoftPackageReferenceList,
			SearchableNames.SearchableNamesMap, ImportUsedInGame, SoftPackageUsedInGame);
	}

	return true;
}

bool FPackageReader::SerializeNameMap()
{
	if (NameMap.Num() > 0)
	{
		return true;
	}
	if( PackageFileSummary.NameCount > 0 )
	{
		if (!StartSerializeSection(PackageFileSummary.NameOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeNameMapInvalidNameOffset", PackageFilename);
			return false;
		}

		const int MinSizePerNameEntry = 1;
		if (PackageFileSize < Tell() + PackageFileSummary.NameCount * MinSizePerNameEntry)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeNameMapInvalidNameCount", PackageFilename);
			return false;
		}

		for ( int32 NameMapIdx = 0; NameMapIdx < PackageFileSummary.NameCount; ++NameMapIdx )
		{
			// Read the name entry from the file.
			FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
			*this << NameEntry;
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeNameMapInvalidName", PackageFilename);
				NameMap.Reset();
				return false;
			}
			NameMap.Add(FName(NameEntry));
		}
	}

	return true;
}

bool FPackageReader::SerializeImportMap()
{
	if (ImportMap.Num() > 0)
	{
		return true;
	}

	if( PackageFileSummary.ImportCount > 0 )
	{
		if (!StartSerializeSection(PackageFileSummary.ImportOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeImportMapInvalidImportOffset", PackageFilename);
			return false;
		}

		const int MinSizePerImport = 1;
		if (PackageFileSize < Tell() + PackageFileSummary.ImportCount * MinSizePerImport)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeImportMapInvalidImportCount", PackageFilename);
			return false;
		}
		ImportMap.Reserve(PackageFileSummary.ImportCount);
		for ( int32 ImportMapIdx = 0; ImportMapIdx < PackageFileSummary.ImportCount; ++ImportMapIdx )
		{
			*this << ImportMap.Emplace_GetRef();
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeImportMapInvalidImport", PackageFilename);
				ImportMap.Reset();
				return false;
			}
		}
	}

	return true;
}

static FName CoreUObjectPackageName(TEXT("/Script/CoreUObject"));
static FName ScriptStructName(TEXT("ScriptStruct"));

bool FPackageReader::SerializeImportedClasses(const TArray<FObjectImport>& InImportMap, TArray<FName>& OutClassNames)
{
	OutClassNames.Reset();

	TSet<int32> ClassImportIndices;
	// Any import that is specified as the class of an export is an imported class
	if (PackageFileSummary.ExportCount > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.ExportOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeExportMapInvalidExportOffset", PackageFilename);
			return false;
		}

		const int MinSizePerExport = 1;
		if (PackageFileSize < Tell() + PackageFileSummary.ExportCount * MinSizePerExport)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeExportMapInvalidExportCount", PackageFilename);
			return false;
		}
		FObjectExport ExportBuffer;
		for (int32 ExportMapIdx = 0; ExportMapIdx < PackageFileSummary.ExportCount; ++ExportMapIdx)
		{
			*this << ExportBuffer;
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeExportMapInvalidExport", PackageFilename);
				return false;
			}
			if (ExportBuffer.ClassIndex.IsImport())
			{
				ClassImportIndices.Add(ExportBuffer.ClassIndex.ToImport());
			}
		}
	}
	// Any imports of types UScriptStruct are an imported struct and need to be added to ImportedClasses
	// This covers e.g. DataTable, which has a RowStruct pointer that it uses in its native serialization to
	// serialize data into its rows
	// TODO: Projects may create their own ScriptStruct subclass, and if they use one of these subclasses
	// as a serialized-external-struct-pointer then we will miss it. In a future implementation we will 
	// change the PackageReader to report all imports, and allow the AssetRegistry to decide which ones
	// are classes based on its class database.
	for (int32 ImportIndex = 0; ImportIndex < InImportMap.Num(); ++ImportIndex)
	{
		const FObjectImport& ObjectImport = InImportMap[ImportIndex];
		if (ObjectImport.ClassPackage == CoreUObjectPackageName && ObjectImport.ClassName == ScriptStructName)
		{
			ClassImportIndices.Add(ImportIndex);
		}
	}

	TArray<FName, TInlineAllocator<5>>  ParentChain;
	FNameBuilder ClassObjectPath;
	for (int32 ClassImportIndex : ClassImportIndices)
	{
		ParentChain.Reset();
		ClassObjectPath.Reset();
		if (!InImportMap.IsValidIndex(ClassImportIndex))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeImportedClassesInvalidClassIndex", PackageFilename);
			return false;
		}
		bool bParentChainComplete = false;
		int32 CurrentParentIndex = ClassImportIndex;
		for (;;)
		{
			const FObjectImport& ObjectImport = InImportMap[CurrentParentIndex];
			ParentChain.Add(ObjectImport.ObjectName);
			if (ObjectImport.OuterIndex.IsImport())
			{
				CurrentParentIndex = ObjectImport.OuterIndex.ToImport();
				if (!InImportMap.IsValidIndex(CurrentParentIndex))
				{
					UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeImportedClassesInvalidImportInParentChain",
						PackageFilename);
					return false;
				}
			}
			else if (ObjectImport.OuterIndex.IsNull())
			{
				bParentChainComplete = true;
				break;
			}
			else
			{
				check(ObjectImport.OuterIndex.IsExport());
				// Ignore classes in an external package but with an object in this package as one of their outers;
				// We do not need to handle that case yet for Import Classes, and we would have to make this
				// loop more complex (searching in both ExportMap and ImportMap) to do so
				break;
			}
		}

		if (bParentChainComplete)
		{
			int32 NumTokens = ParentChain.Num();
			check(NumTokens >= 1);
			const TCHAR Delimiters[] = { TEXT('.'), SUBOBJECT_DELIMITER_CHAR, TEXT('.') };
			int32 DelimiterIndex = 0;
			ParentChain[NumTokens - 1].AppendString(ClassObjectPath);
			for (int32 TokenIndex = NumTokens - 2; TokenIndex >= 0; --TokenIndex)
			{
				ClassObjectPath << Delimiters[DelimiterIndex];
				DelimiterIndex = FMath::Min(DelimiterIndex+1, static_cast<int32>(UE_ARRAY_COUNT(Delimiters))-1);
				ParentChain[TokenIndex].AppendString(ClassObjectPath);
			}
			OutClassNames.Emplace(ClassObjectPath);
		}
	}

	OutClassNames.Sort(FNameLexicalLess());
	return true;
}

bool FPackageReader::SerializeExportMap()
{
	if (ExportMap.Num() > 0)
	{
		return true;
	}

	if (PackageFileSummary.ExportCount > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.ExportOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeExportMapInvalidExportOffset", PackageFilename);
			return false;
		}

		const int MinSizePerExport = 1;
		if (PackageFileSize < Tell() + PackageFileSummary.ExportCount * MinSizePerExport)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeExportMapInvalidExportCount", PackageFilename);
			return false;
		}
		ExportMap.Reserve(PackageFileSummary.ExportCount);
		for (int32 ExportMapIdx = 0; ExportMapIdx < PackageFileSummary.ExportCount; ++ExportMapIdx)
		{
			*this << ExportMap.Emplace_GetRef();
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeExportMapInvalidExport", PackageFilename);
				ExportMap.Reset();
				return false;
			}
		}
	}

	return true;
}

bool FPackageReader::SerializeDependsMap()
{
	if (DependsMap.Num() > 0)
	{
		return true;
	}

	if (PackageFileSummary.DependsOffset > 0 && PackageFileSummary.ExportCount > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.DependsOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeDependsMapInvalidOffset", PackageFilename);
			return false;
		}

		const int MinSizePerExport = 1;
		if (PackageFileSize < Tell() + PackageFileSummary.ExportCount * MinSizePerExport)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeDependsMapInvalidExportCount", PackageFilename);
			return false;
		}
		DependsMap.Reserve(PackageFileSummary.ExportCount);
		for (int32 DependsMapIdx = 0; DependsMapIdx < PackageFileSummary.ExportCount; ++DependsMapIdx)
		{
			*this << DependsMap.Emplace_GetRef();
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeDependsMapInvalidEntry", PackageFilename);
				DependsMap.Reset();
				return false;
			}
		}
	}

	return true;
}

bool FPackageReader::SerializeSoftPackageReferenceList()
{
	if (SoftPackageReferenceList.Num() > 0)
	{
		return true;
	}

	if (UEVer() >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP && PackageFileSummary.SoftPackageReferencesOffset > 0 && PackageFileSummary.SoftPackageReferencesCount > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.SoftPackageReferencesOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftPackageReferenceListInvalidReferencesOffset", PackageFilename);
			return false;
		}
		
		const int MinSizePerSoftPackageReference = 1;
		if (PackageFileSize < Tell() + PackageFileSummary.SoftPackageReferencesCount * MinSizePerSoftPackageReference)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftPackageReferenceListInvalidReferencesCount", PackageFilename);
			return false;
		}

		SoftPackageReferenceList.Reserve(PackageFileSummary.SoftPackageReferencesCount);
		if (UEVer() < VER_UE4_ADDED_SOFT_OBJECT_PATH)
		{
			for (int32 ReferenceIdx = 0; ReferenceIdx < PackageFileSummary.SoftPackageReferencesCount; ++ReferenceIdx)
			{
				FString PackageName;
				*this << PackageName;
				if (IsError())
				{
					UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftPackageReferenceListInvalidReferencePreSoftObjectPath", PackageFilename);
					SoftPackageReferenceList.Reset();
					return false;
				}

				if (UEVer() < VER_UE4_KEEP_ONLY_PACKAGE_NAMES_IN_STRING_ASSET_REFERENCES_MAP)
				{
					PackageName = FPackageName::GetNormalizedObjectPath(PackageName);
					if (!PackageName.IsEmpty())
					{
						PackageName = FPackageName::ObjectPathToPackageName(PackageName);
					}
				}

				SoftPackageReferenceList.Add(FName(*PackageName));
			}
		}
		else
		{
			for (int32 ReferenceIdx = 0; ReferenceIdx < PackageFileSummary.SoftPackageReferencesCount; ++ReferenceIdx)
			{
				FName PackageName;
				*this << PackageName;
				if (IsError())
				{
					UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftPackageReferenceListInvalidReference", PackageFilename);
					SoftPackageReferenceList.Reset();
					return false;
				}

				SoftPackageReferenceList.Add(PackageName);
			}
		}
	}

	return true;
}

bool FPackageReader::SerializeSoftObjectPathMap()
{
	if (SoftObjectPathMap.Num() > 0)
	{
		return true;
	}

	if (PackageFileSummary.SoftObjectPathsOffset > 0 && PackageFileSummary.SoftObjectPathsCount > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.SoftObjectPathsOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftObjectPathMapListInvalidOffset", PackageFilename);
			return false;
		}
		
		int32 MinSizePerSoftObjectPath = 0; 
		if (UEVer() < VER_UE4_ADDED_SOFT_OBJECT_PATH)
		{
			MinSizePerSoftObjectPath = 8; // FString
		}
		else if (UEVer() < EUnrealEngineObjectUE5Version::FSOFTOBJECTPATH_REMOVE_ASSET_PATH_FNAMES)
		{
			MinSizePerSoftObjectPath = 8 + 8; // FName + FString 
		}
		else
		{
			MinSizePerSoftObjectPath = 8 + 8 + 8; // 2xFName + FString
		}

		if (PackageFileSize < Tell() + PackageFileSummary.SoftObjectPathsCount * MinSizePerSoftObjectPath)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftObjectPathMapInvalidCount", PackageFilename);
			return false;
		}

		SoftObjectPathMap.Reserve(PackageFileSummary.SoftObjectPathsCount);
		for (int32 PathIdx = 0; PathIdx < PackageFileSummary.SoftObjectPathsCount; ++PathIdx)
		{
			FSoftObjectPath Path;
			Path.SerializePath(*this);
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSoftObjectPathMapInvalidPath", PackageFilename);
				SoftObjectPathMap.Reset();
				return false;
			}

			SoftObjectPathMap.Add(Path);
		}
	}

	return true;
}

bool FPackageReader::SerializeGatherableTextDataMap()
{
	if (GatherableTextDataMap.Num() > 0)
	{
		return true;
	}
	
	if (PackageFileSummary.GatherableTextDataCount > 0 && PackageFileSummary.GatherableTextDataOffset > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.GatherableTextDataOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeGatherableTextDataMapInvalidOffset", PackageFilename);
			return false;
		}

		int32 MinSizePerText = 8 + 8 + 4;  // Two FStrings and as an empty array as a lower bound
		if (PackageFileSize < Tell() + PackageFileSummary.GatherableTextDataCount * MinSizePerText)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeGatherableTextDataMapInvalidCount", PackageFilename);
			return false;
		}

		GatherableTextDataMap.Reset(PackageFileSummary.GatherableTextDataCount);	
		for (int32 Index = 0; Index < PackageFileSummary.GatherableTextDataCount; ++Index)
		{
			FGatherableTextData Data;
			*this << Data;	
			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeGatherableTextDataMapInvalidEntry", PackageFilename);
				GatherableTextDataMap.Reset();
				return false;
			}
			GatherableTextDataMap.Emplace(MoveTemp(Data));	
		}
	}

	return true;
}

bool FPackageReader::SerializeThumbnailMap()
{
	if (ThumbnailMap.Num() > 0)
	{
		return true;
	}
	
	if (PackageFileSummary.ThumbnailTableOffset > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.ThumbnailTableOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeThumbnailMapInvalidOffset", PackageFilename);
			return false;
		}

		FString PackageName;
		if (!TryGetLongPackageName(PackageName))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeThumbnailMapNoPackageName", PackageFilename);
			return false;
		}

		int32 NumThumbnails = 0;
		*this << NumThumbnails;
		if (IsError() || NumThumbnails < 0)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeThumbnailMapInvalidCount", PackageFilename);
			return false;
		}	

		int32 MinSizePerThumbnail = 8 + 8 + 4;  // Two FStrings and an offset 
		if (PackageFileSize < Tell() + PackageFileSummary.GatherableTextDataCount * MinSizePerThumbnail)
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeThumbnailMapInvalidCount", PackageFilename);
			return false;
		}
		ThumbnailMap.Reset(NumThumbnails);
		
		for (int32 Index=0; Index < NumThumbnails; ++Index)
		{
			FString ObjectClassName;
			*this << ObjectClassName;
			FString ObjectPathWithoutPackageName;
			*this << ObjectPathWithoutPackageName;
			int32 Offset = 0;
			*this << Offset;

			if (IsError())
			{
				UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeThumbnailMapInvalidEntry", PackageFilename);
				return false;
			}
			
			FObjectFullNameAndThumbnail Thumbnail;
			Thumbnail.ObjectFullName = FName(WriteToString<FName::StringBufferSize>(ObjectClassName, TEXT(" "), PackageName, TEXT("."), ObjectPathWithoutPackageName));
			Thumbnail.FileOffset = Offset;
			ThumbnailMap.Emplace(MoveTemp(Thumbnail));
		}
	}

	return true;
}

bool FPackageReader::SerializeEditorOnlyFlags(TBitArray<>& OutImportUsedInGame, TBitArray<>& OutSoftPackageUsedInGame)
{
	if (AssetRegistryDependencyDataOffset == INDEX_NONE &&
		!(PackageFileSummary.GetPackageFlags() & PKG_FilterEditorOnly) &&
		StartSerializeSection(PackageFileSummary.AssetRegistryDataOffset))
	{
		UE::AssetRegistry::FDeserializePackageData DeserializePackageData;
		UE::AssetRegistry::EReadPackageDataMainErrorCode Error;
		if (!DeserializePackageData.DoSerialize(*this, PackageFileSummary, Error))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("EReadPackageDataMainErrorCode::InvalidTagCount", PackageFilename);
			return false;
		}
		AssetRegistryDependencyDataOffset = DeserializePackageData.DependencyDataOffset;
	}

	if (AssetRegistryDependencyDataOffset == INDEX_NONE)
	{
		// For cooked packages or old package versions that did not write out the dependency flags,
		// set default values of the flags
		OutImportUsedInGame.Init(true, ImportMap.Num());
		OutSoftPackageUsedInGame.Init(true, SoftPackageReferenceList.Num());
		return true;
	}

	if (!StartSerializeSection(AssetRegistryDependencyDataOffset))
	{
		return false;
	}

	if (!UE::AssetRegistry::ReadPackageDataDependencies(*this, OutImportUsedInGame, OutSoftPackageUsedInGame) ||
		OutImportUsedInGame.Num() != ImportMap.Num() ||
		OutSoftPackageUsedInGame.Num() != SoftPackageReferenceList.Num())
	{
		UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeAssetRegistryDependencyData", PackageFilename);
		return false;
	}

	return true;
}

bool FPackageReader::SerializeSearchableNamesMap(FLinkerTables& OutSearchableNames)
{
	if (UEVer() >= VER_UE4_ADDED_SEARCHABLE_NAMES && PackageFileSummary.SearchableNamesOffset > 0)
	{
		if (!StartSerializeSection(PackageFileSummary.SearchableNamesOffset))
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSearchableNamesMapInvalidOffset", PackageFilename);
			return false;
		}

		OutSearchableNames.SerializeSearchableNamesMap(*this);
		if (IsError())
		{
			UE_PACKAGEREADER_CORRUPTPACKAGE_WARNING("SerializeSearchableNamesMapInvalidSearchableNamesMap", PackageFilename);
			return false;
		}
	}

	return true;
}

void FPackageReader::Serialize( void* V, int64 Length )
{
	check(Loader);
	Loader->Serialize( V, Length );
	if (Loader->IsError())
	{
		SetError();
	}
}

bool FPackageReader::Precache( int64 PrecacheOffset, int64 PrecacheSize )
{
	check(Loader);
	return Loader->Precache( PrecacheOffset, PrecacheSize );
}

void FPackageReader::Seek( int64 InPos )
{
	check(Loader);
	Loader->Seek( InPos );
	if (Loader->IsError())
	{
		SetError();
	}
}

int64 FPackageReader::Tell()
{
	check(Loader);
	return Loader->Tell();
}

int64 FPackageReader::TotalSize()
{
	check(Loader);
	return Loader->TotalSize();
}

uint32 FPackageReader::GetPackageFlags() const
{
	return PackageFileSummary.GetPackageFlags();
}

FArchive& FPackageReader::operator<<( FName& Name )
{
	int32 NameIndex;
	FArchive& Ar = *this;
	Ar << NameIndex;

	if( !NameMap.IsValidIndex(NameIndex) )
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("Bad name index %i/%i when reading package %s"), NameIndex, NameMap.Num(), *PackageFilename );
		SetError();
		return *this;
	}

	// if the name wasn't loaded (because it wasn't valid in this context)
	if (NameMap[NameIndex] == NAME_None)
	{
		int32 TempNumber;
		Ar << TempNumber;
		Name = NAME_None;
	}
	else
	{
		int32 Number;
		Ar << Number;
		// simply create the name from the NameMap's name and the serialized instance number
		Name = FName(NameMap[NameIndex], Number);
	}

	return *this;
}

void FPackageReader::ConvertLinkerTableToPaths(FName InPackageName, TArray<FObjectExport>& InExportMap, TArray<FObjectImport>& InImportMap,
	TArray<FSoftObjectPath>& OutExports, TArray<FSoftObjectPath>& OutImports)
{
	TMap<FPackageIndex, FSoftObjectPath> ObjectForIndex;
	TFunction<FSoftObjectPath& (FPackageIndex Index)> GetSoftObjectPath;
	FSoftObjectPath EmptySoftObjectPath;
	int32 Counter = 0;
	int32 NumExports = InExportMap.Num();
	int32 NumImports = InImportMap.Num();
	int32 NumObjects = NumExports + NumImports;

	ObjectForIndex.Reserve(NumObjects);
	GetSoftObjectPath = [&InImportMap, &InExportMap, &ObjectForIndex, &GetSoftObjectPath, &EmptySoftObjectPath,
		&Counter, NumObjects, InPackageName]
	(FPackageIndex Index) -> FSoftObjectPath&
	{
		if (++Counter > (NumObjects + 1) * 2)
		{
			// Recursive overflow should be impossible because every call fills in a new element of the table
			check(false);
		}
		ON_SCOPE_EXIT
		{
			--Counter;
		};
		if (Index.IsNull())
		{
			return EmptySoftObjectPath;
		}
		FSoftObjectPath* Existing = ObjectForIndex.Find(Index);
		if (Existing)
		{
			return *Existing;
		}

		FPackageIndex ParentIndex;
		FName ObjectName;
		if (Index.IsExport())
		{
			int32 ExportIndex = Index.ToExport();
			if (ExportIndex < InExportMap.Num())
			{
				FObjectExport& Export = InExportMap[ExportIndex];
				ParentIndex = Export.OuterIndex;
				ObjectName = Export.ObjectName;
			}
		}
		else
		{
			int32 ImportIndex = Index.ToImport();
			if (ImportIndex < InImportMap.Num())
			{
				FObjectImport& Import = InImportMap[ImportIndex];
				ParentIndex = Import.OuterIndex;
				ObjectName = Import.ObjectName;
			}
		}
		FSoftObjectPath Result;
		if (!ObjectName.IsNone())
		{
			FSoftObjectPath& ParentPath = GetSoftObjectPath(ParentIndex);
			if (ParentPath.IsNull())
			{
				if (Index.IsExport())
				{
					Result = FSoftObjectPath(InPackageName, ObjectName, FString());
				}
				else
				{
					Result = FSoftObjectPath(ObjectName, NAME_None, FString());
				}
			}
			else if (ParentPath.GetAssetFName().IsNone())
			{
				Result = FSoftObjectPath(ParentPath.GetLongPackageFName(), ObjectName, FString());
			}
			else if (ParentPath.GetSubPathString().IsEmpty())
			{
				Result = FSoftObjectPath(ParentPath.GetLongPackageFName(), ParentPath.GetAssetFName(),
					ObjectName.ToString());
			}
			else
			{
				Result = FSoftObjectPath(ParentPath.GetLongPackageFName(), ParentPath.GetAssetFName(),
					ParentPath.GetSubPathString() + TEXT(".") + ObjectName.ToString());
			}
		}

		// Note we have to look up the Element again in ObjectForIndex. We can not cache a FindOrAdd Result for Index
		// because we have potentially modified ObjectForIndex by calling GetSoftObjectPath(ParentIndex).
		return ObjectForIndex.Add(Index, Result);
	};
	OutExports.Reset(NumExports);
	for (int32 ExportIndex = 0; ExportIndex < NumExports; ++ExportIndex)
	{
		OutExports.Add(GetSoftObjectPath(FPackageIndex::FromExport(ExportIndex)));
	}
	OutImports.Reset(NumImports);
	for (int32 ImportIndex = 0; ImportIndex < NumImports; ++ImportIndex)
	{
		OutImports.Add(GetSoftObjectPath(FPackageIndex::FromImport(ImportIndex)));
	}
}


namespace UE::AssetRegistry
{
	class FNameMapAwareArchive : public FArchiveProxy
	{
		TArray<FNameEntryId>	NameMap;

	public:

		FNameMapAwareArchive(FArchive& Inner)
			: FArchiveProxy(Inner)
		{}

		FORCEINLINE virtual FArchive& operator<<(FName& Name) override
		{
			FArchive& Ar = *this;
			int32 NameIndex;
			Ar << NameIndex;
			int32 Number = 0;
			Ar << Number;

			if (NameMap.IsValidIndex(NameIndex))
			{
				// if the name wasn't loaded (because it wasn't valid in this context)
				FNameEntryId MappedName = NameMap[NameIndex];

				// simply create the name from the NameMap's name and the serialized instance number
				Name = FName::CreateFromDisplayId(MappedName, Number);
			}
			else
			{
				Name = FName();
				SetCriticalError();
			}

			return *this;
		}

		void SerializeNameMap(const FPackageFileSummary& PackageFileSummary)
		{
			Seek(PackageFileSummary.NameOffset);
			NameMap.Reserve(PackageFileSummary.NameCount);
			FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);
			for (int32 Idx = NameMap.Num(); Idx < PackageFileSummary.NameCount; ++Idx)
			{
				*this << NameEntry;
				NameMap.Emplace(FName(NameEntry).GetDisplayIndex());
			}
		}
	};
	FString ReconstructFullClassPath(FArchive& BinaryArchive, const FString& PackageName, const FPackageFileSummary& PackageFileSummary, const FString& AssetClassName,
		const TArray<FObjectImport>* InImports = nullptr, const TArray<FObjectExport>* InExports = nullptr)
	{
		FName ClassFName(*AssetClassName);
		FLinkerTables LinkerTables;
		if (!InImports || !InExports)
		{
			FNameMapAwareArchive NameMapArchive(BinaryArchive);
			NameMapArchive.SerializeNameMap(PackageFileSummary);

			// Load the linker tables
			if (!InImports)
			{
				BinaryArchive.Seek(PackageFileSummary.ImportOffset);
				for (int32 ImportMapIndex = 0; ImportMapIndex < PackageFileSummary.ImportCount; ++ImportMapIndex)
				{
					NameMapArchive << LinkerTables.ImportMap.Emplace_GetRef();
				}
			}
			if (!InExports)
			{
				BinaryArchive.Seek(PackageFileSummary.ExportOffset);
				for (int32 ExportMapIndex = 0; ExportMapIndex < PackageFileSummary.ExportCount; ++ExportMapIndex)
				{
					NameMapArchive << LinkerTables.ExportMap.Emplace_GetRef();
				}
			}
		}
		if (InImports)
		{
			LinkerTables.ImportMap = *InImports;
		}
		if (InExports)
		{
			LinkerTables.ExportMap = *InExports;
		}

		FString ClassPathName;

		// Now look through the exports' classes and find the one matching the asset class
		for (const FObjectExport& Export : LinkerTables.ExportMap)
		{
			if (Export.ClassIndex.IsImport())
			{
				if (LinkerTables.ImportMap[Export.ClassIndex.ToImport()].ObjectName == ClassFName)
				{
					ClassPathName = LinkerTables.GetImportPathName(Export.ClassIndex.ToImport());
					break;
				}					
			}
			else if (Export.ClassIndex.IsExport())
			{
				if (LinkerTables.ExportMap[Export.ClassIndex.ToExport()].ObjectName == ClassFName)
				{
					ClassPathName = LinkerTables.GetExportPathName(PackageName, Export.ClassIndex.ToExport());
					break;
				}
			}
		}
		if (ClassPathName.IsEmpty())
		{
			UE_LOG(LogAssetRegistry, Error, TEXT("Failed to find an import or export matching asset class short name \"%s\"."), *AssetClassName);
			// Just pass through the short class name
			ClassPathName = AssetClassName;
		}


		return ClassPathName;
	}

	bool FDeserializePackageData::DoSerialize(FArchive& BinaryArchive, const FPackageFileSummary& PackageFileSummary, EReadPackageDataMainErrorCode& OutError)
	{
		// To avoid large patch sizes, we have frozen cooked package format at the format before VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS
		const bool bPreDependencyFormat = PackageFileSummary.GetFileVersionUE() < VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS || !!(PackageFileSummary.GetPackageFlags() & PKG_FilterEditorOnly);

		// Load offsets to optionally-read data
		if (bPreDependencyFormat)
		{
			DependencyDataOffset = INDEX_NONE;
		}
		else
		{
			BinaryArchive << DependencyDataOffset;
		}

		// Load the object count
		ObjectCount = 0;
		BinaryArchive << ObjectCount;
		const int64 PackageFileSize = BinaryArchive.TotalSize();
		const int32 MinBytesPerObject = 1;
		if (BinaryArchive.IsError() || ObjectCount < 0 || PackageFileSize < BinaryArchive.Tell() + ObjectCount * MinBytesPerObject)
		{
			OutError = EReadPackageDataMainErrorCode::InvalidObjectCount;
			return false;
		}

		return true;
	}

	bool FDeserializeObjectPackageData::DoSerialize(FArchive& BinaryArchive, EReadPackageDataMainErrorCode& OutError)
	{
		const int32 MinBytesPerTag = 1;
		const int64 PackageFileSize = BinaryArchive.TotalSize();

		BinaryArchive << ObjectPath;
		BinaryArchive << ObjectClassName;
		// @todo make sure this is a full path name
		BinaryArchive << TagCount;
		if (BinaryArchive.IsError() || TagCount < 0 || PackageFileSize < BinaryArchive.Tell() + TagCount * MinBytesPerTag)
		{
			OutError = EReadPackageDataMainErrorCode::InvalidTagCount;
			return false;
		}		

		return true;
	}

	bool FDeserializeTagData::DoSerialize(FArchive& BinaryArchive, EReadPackageDataMainErrorCode& OutError)
	{
		BinaryArchive << Key;
		BinaryArchive << Value;
		if (BinaryArchive.IsError())
		{
			OutError = EReadPackageDataMainErrorCode::InvalidTag;
			return false;
		}

		return true;
	}

	// See the corresponding WritePackageData defined in SavePackageUtilities.cpp in CoreUObject module
	bool ReadPackageDataMain(FArchive& BinaryArchive, const FString& PackageName, const FPackageFileSummary& PackageFileSummary, int64& OutDependencyDataOffset,
		TArray<FAssetData*>& OutAssetDataList, EReadPackageDataMainErrorCode& OutError, const TArray<FObjectImport>* InImports, const TArray<FObjectExport>* InExports)
	{
		OutError = EReadPackageDataMainErrorCode::Unknown;

		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);
		const int64 PackageFileSize = BinaryArchive.TotalSize();
		const bool bIsMapPackage = (PackageFileSummary.GetPackageFlags() & PKG_ContainsMap) != 0;

		FDeserializePackageData DeserializePackageData;
		if (!DeserializePackageData.DoSerialize(BinaryArchive, PackageFileSummary, OutError))
		{
			return false;
		}

		OutDependencyDataOffset = DeserializePackageData.DependencyDataOffset;

		// Worlds that were saved before they were marked public do not have asset data so we will synthesize it here to make sure we see all legacy umaps
		// We will also do this for maps saved after they were marked public but no asset data was saved for some reason. A bug caused this to happen for some maps.
		if (bIsMapPackage)
		{
			const bool bLegacyPackage = PackageFileSummary.GetFileVersionUE() < VER_UE4_PUBLIC_WORLDS;
			const bool bNoMapAsset = (DeserializePackageData.ObjectCount == 0);
			if (bLegacyPackage || bNoMapAsset)
			{
				FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
				OutAssetDataList.Add(new FAssetData(FName(*PackageName), FName(*PackagePath), FName(*AssetName), FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")), FAssetDataTagMap(), PackageFileSummary.ChunkIDs, PackageFileSummary.GetPackageFlags()));
			}
		}

		// UAsset files usually only have one asset, maps and redirectors have multiple
		for (int32 ObjectIdx = 0; ObjectIdx < DeserializePackageData.ObjectCount; ++ObjectIdx)
		{
			FDeserializeObjectPackageData ObjectPackageData;
			if (!ObjectPackageData.DoSerialize(BinaryArchive, OutError))
			{
				return false;
			}

			FAssetDataTagMap TagsAndValues;
			TagsAndValues.Reserve(ObjectPackageData.TagCount);

			for (int32 TagIdx = 0; TagIdx < ObjectPackageData.TagCount; ++TagIdx)
			{
				FDeserializeTagData TagData;
				if (!TagData.DoSerialize(BinaryArchive, OutError))
				{
					return false;
				}

				if (!TagData.Key.IsEmpty() && !TagData.Value.IsEmpty())
				{
					TagsAndValues.Add(FName(*TagData.Key), TagData.Value);
				}
			}

			// Before worlds were RF_Public, other non-public assets were added to the asset data table in map packages.
			// Here we simply skip over them
			if (bIsMapPackage && PackageFileSummary.GetFileVersionUE() < VER_UE4_PUBLIC_WORLDS)
			{
				if (ObjectPackageData.ObjectPath != FPackageName::GetLongPackageAssetName(PackageName))
				{
					continue;
				}
			}

			// if we have an object path that starts with the package then this asset is outer-ed to another package
			const bool bFullObjectPath = ObjectPackageData.ObjectPath.StartsWith(TEXT("/"), ESearchCase::CaseSensitive);

			// if we do not have a full object path already, build it
			if (!bFullObjectPath)
			{
				// if we do not have a full object path, ensure that we have a top level object for the package and not a sub object
				if (!ensureMsgf(!ObjectPackageData.ObjectPath.Contains(TEXT("."), ESearchCase::CaseSensitive), TEXT("Cannot make FAssetData for sub object %s in package %s!"), *ObjectPackageData.ObjectPath, *PackageName))
				{
					UE_ASSET_LOG(LogAssetRegistry, Warning, *PackageName, TEXT("Cannot make FAssetData for sub object %s!"), *ObjectPackageData.ObjectPath);
					continue;
				}
				ObjectPackageData.ObjectPath = PackageName + TEXT(".") + ObjectPackageData.ObjectPath;
			}
			// Previously export couldn't have its outer as an import
			else if (PackageFileSummary.GetFileVersionUE() < VER_UE4_NON_OUTER_PACKAGE_IMPORT)
			{
				UE_ASSET_LOG(LogAssetRegistry, Warning, *PackageName, TEXT("Package has invalid export %s, resave source package!"), *ObjectPackageData.ObjectPath);
				continue;
			}

			// Create a new FAssetData for this asset and update it with the gathered data
			if (!ObjectPackageData.ObjectClassName.IsEmpty() && FPackageName::IsShortPackageName(ObjectPackageData.ObjectClassName))
			{
				int64 CurrentPos = BinaryArchive.Tell();
				ObjectPackageData.ObjectClassName = ReconstructFullClassPath(BinaryArchive, PackageName, PackageFileSummary,
					ObjectPackageData.ObjectClassName, InImports, InExports);
				BinaryArchive.Seek(CurrentPos);
			}
			OutAssetDataList.Add(new FAssetData(PackageName, ObjectPackageData.ObjectPath, FTopLevelAssetPath(ObjectPackageData.ObjectClassName), MoveTemp(TagsAndValues), PackageFileSummary.ChunkIDs, PackageFileSummary.GetPackageFlags()));
		}

		return true;
	}

	// See the corresponding WriteAssetRegistryPackageData defined in SavePackageUtilities.cpp in CoreUObject module
	bool ReadPackageDataDependencies(FArchive& BinaryArchive, TBitArray<>& OutImportUsedInGame, TBitArray<>& OutSoftPackageUsedInGame)
	{
		BinaryArchive << OutImportUsedInGame;
		BinaryArchive << OutSoftPackageUsedInGame;
		return !BinaryArchive.IsError();
	}
}
