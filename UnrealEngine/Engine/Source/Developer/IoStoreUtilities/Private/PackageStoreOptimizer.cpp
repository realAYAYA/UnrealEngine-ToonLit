// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageStoreOptimizer.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Serialization/BufferWriter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/NameBatchSerialization.h"
#include "Containers/Map.h"
#include "UObject/UObjectHash.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/Object.h"
#include "Serialization/MemoryReader.h"
#include "UObject/Package.h"
#include "Misc/SecureHash.h"
#include "Serialization/LargeMemoryReader.h"

DEFINE_LOG_CATEGORY_STATIC(LogPackageStoreOptimizer, Log, All);

// modified copy from SavePackage
EObjectMark GetExcludedObjectMarksForTargetPlatform(const ITargetPlatform* TargetPlatform)
{
	EObjectMark Marks = OBJECTMARK_NotForTargetPlatform;
	if (!TargetPlatform->AllowsEditorObjects())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_EditorOnly);
	}
	if (TargetPlatform->IsServerOnly())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForServer);
	}
	if (TargetPlatform->IsClientOnly())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForClient);
	}
	return Marks;
}

// modified copy from SavePackage
EObjectMark GetExcludedObjectMarksForObject(const UObject* Object, const ITargetPlatform* TargetPlatform)
{
	EObjectMark Marks = OBJECTMARK_NOMARKS;
	if (!Object->NeedsLoadForClient())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForClient);
	}
	if (!Object->NeedsLoadForServer())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForServer);
	}
#if WITH_ENGINE
	// NotForServer && NotForClient implies EditorOnly
	const bool bIsEditorOnlyObject = (Marks & OBJECTMARK_NotForServer) && (Marks & OBJECTMARK_NotForClient);
	const bool bTargetAllowsEditorObjects = TargetPlatform->AllowsEditorObjects();

	// no need to query the target platform if the object is editoronly and the targetplatform doesn't allow editor objects 
	const bool bCheckTargetPlatform = !bIsEditorOnlyObject || bTargetAllowsEditorObjects;
	if (bCheckTargetPlatform && (!Object->NeedsLoadForTargetPlatform(TargetPlatform) || !TargetPlatform->AllowObject(Object)))
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_NotForTargetPlatform);
	}
#endif
	if (Object->IsEditorOnly())
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_EditorOnly);
	}
	if ((Marks & OBJECTMARK_NotForClient) && (Marks & OBJECTMARK_NotForServer))
	{
		Marks = (EObjectMark)(Marks | OBJECTMARK_EditorOnly);
	}
	return Marks;
}

// modified copy from PakFileUtilities
static FString RemapLocalizationPathIfNeeded(const FString& Path, FString* OutRegion)
{
	static constexpr TCHAR L10NString[] = TEXT("/L10N/");
	static constexpr int32 L10NPrefixLength = sizeof(L10NString) / sizeof(TCHAR) - 1;

	int32 BeginL10NOffset = Path.Find(L10NString, ESearchCase::IgnoreCase);
	if (BeginL10NOffset >= 0)
	{
		int32 EndL10NOffset = BeginL10NOffset + L10NPrefixLength;
		int32 NextSlashIndex = Path.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, EndL10NOffset);
		int32 RegionLength = NextSlashIndex - EndL10NOffset;
		if (RegionLength >= 2)
		{
			FString NonLocalizedPath = Path.Mid(0, BeginL10NOffset) + Path.Mid(NextSlashIndex);
			if (OutRegion)
			{
				*OutRegion = Path.Mid(EndL10NOffset, RegionLength);
				OutRegion->ToLowerInline();
			}
			return NonLocalizedPath;
		}
	}
	return Path;
}

void FPackageStoreOptimizer::Initialize()
{
	FindScriptObjects();
}

void FPackageStoreOptimizer::Initialize(const FIoBuffer& ScriptObjectsBuffer)
{
	LoadScriptObjectsBuffer(ScriptObjectsBuffer);
}

FPackageStorePackage* FPackageStoreOptimizer::CreateMissingPackage(const FName& Name) const
{
	FPackageStorePackage* Package = new FPackageStorePackage();
	Package->Name = Name;
	Package->Id = FPackageId::FromName(Name);
	Package->SourceName = *RemapLocalizationPathIfNeeded(Name.ToString(), &Package->Region);
	return Package;
}

FPackageStorePackage* FPackageStoreOptimizer::CreatePackageFromCookedHeader(const FName& Name, const FIoBuffer& CookedHeaderBuffer) const
{
	FPackageStorePackage* Package = new FPackageStorePackage();
	Package->Id = FPackageId::FromName(Name);
	Package->Name = Name;
	FString NameStr = Name.ToString();
	Package->SourceName = *RemapLocalizationPathIfNeeded(NameStr, &Package->Region);

	FCookedHeaderData CookedHeaderData = LoadCookedHeader(CookedHeaderBuffer);
	if (!CookedHeaderData.Summary.bUnversioned)
	{
		FZenPackageVersioningInfo& VersioningInfo = Package->VersioningInfo.Emplace();
		VersioningInfo.ZenVersion = EZenPackageVersion::Latest;
		VersioningInfo.PackageVersion = CookedHeaderData.Summary.GetFileVersionUE();
		VersioningInfo.LicenseeVersion = CookedHeaderData.Summary.GetFileVersionLicenseeUE();
		VersioningInfo.CustomVersions = CookedHeaderData.Summary.GetCustomVersionContainer();
	}
	Package->PackageFlags = CookedHeaderData.Summary.GetPackageFlags();
	Package->CookedHeaderSize = CookedHeaderData.Summary.TotalHeaderSize;
	for (int32 I = 0; I < CookedHeaderData.Summary.NamesReferencedFromExportDataCount; ++I)
	{
		Package->NameMapBuilder.AddName(CookedHeaderData.SummaryNames[I]);
	}

	TArray<FPackageStorePackage::FUnresolvedImport> Imports;
	ProcessImports(CookedHeaderData, Package, Imports);
	ProcessExports(CookedHeaderData, Package, Imports.GetData());
	ProcessPreloadDependencies(CookedHeaderData, Package);
	
	CreateExportBundles(Package);

	return Package;
}

FPackageStorePackage* FPackageStoreOptimizer::CreatePackageFromPackageStoreHeader(const FName& Name, const FIoBuffer& Buffer, const FPackageStoreEntryResource& PackageStoreEntry) const
{
	FPackageStorePackage* Package = new FPackageStorePackage();
	Package->Id = FPackageId::FromName(Name);

	// The package id should be generated from the original name
	// Support for optional package is not implemented when loading from the package store yet however
	FString NameStr = Name.ToString();
	int32 Index = NameStr.Find(FPackagePath::GetOptionalSegmentExtensionModifier());
	if (Index != INDEX_NONE)
	{
		unimplemented();
	}

	Package->Name = Name;
	Package->SourceName = *RemapLocalizationPathIfNeeded(NameStr, &Package->Region);

	FPackageStoreHeaderData PackageStoreHeaderData = LoadPackageStoreHeader(Buffer, PackageStoreEntry);
	if (PackageStoreHeaderData.VersioningInfo.IsSet())
	{
		Package->VersioningInfo.Emplace(PackageStoreHeaderData.VersioningInfo.GetValue());
	}
	Package->PackageFlags = PackageStoreHeaderData.Summary.PackageFlags;
	Package->CookedHeaderSize = PackageStoreHeaderData.Summary.CookedHeaderSize;
	for (FDisplayNameEntryId DisplayId : PackageStoreHeaderData.NameMap)
	{
		Package->NameMapBuilder.AddName(DisplayId);
	}
	ProcessImports(PackageStoreHeaderData, Package);
	ProcessExports(PackageStoreHeaderData, Package);
	ProcessPreloadDependencies(PackageStoreHeaderData, Package);
	CreateExportBundles(Package);

	return Package;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FPackageStoreOptimizer::FCookedHeaderData FPackageStoreOptimizer::LoadCookedHeader(const FIoBuffer& CookedHeaderBuffer) const
{
	FCookedHeaderData CookedHeaderData;
	TArrayView<const uint8> MemView(CookedHeaderBuffer.Data(), CookedHeaderBuffer.DataSize());
	FMemoryReaderView Ar(MemView);

	FPackageFileSummary& Summary = CookedHeaderData.Summary;
	{
		TGuardValue<int32> GuardAllowUnversionedContentInEditor(GAllowUnversionedContentInEditor, 1);
		Ar << Summary;
	}

	Ar.SetFilterEditorOnly((CookedHeaderData.Summary.GetPackageFlags() & EPackageFlags::PKG_FilterEditorOnly) != 0);

	if (Summary.NameCount > 0)
	{
		Ar.Seek(Summary.NameOffset);

		FNameEntrySerialized NameEntry(ENAME_LinkerConstructor);

		CookedHeaderData.SummaryNames.Reserve(Summary.NameCount);
		for (int32 I = 0; I < Summary.NameCount; ++I)
		{
			Ar << NameEntry;
			CookedHeaderData.SummaryNames.Add(NameEntry);
		}
	}

	class FNameReaderProxyArchive
		: public FArchiveProxy
	{
	public:
		using FArchiveProxy::FArchiveProxy;

		FNameReaderProxyArchive(FArchive& InAr, const TArray<FName>& InNameMap)
			: FArchiveProxy(InAr)
			, NameMap(InNameMap)
		{
			// Replicate the filter editor only state of the InnerArchive as FArchiveProxy will
			// not intercept it.
			FArchive::SetFilterEditorOnly(InAr.IsFilterEditorOnly());
		}

		FArchive& operator<<(FName& Name)
		{
			int32 NameIndex = 0;
			int32 Number = 0;
			InnerArchive << NameIndex << Number;

			if (!NameMap.IsValidIndex(NameIndex))
			{
				UE_LOG(LogPackageStoreOptimizer, Fatal, TEXT("Bad name index %i/%i"), NameIndex, NameMap.Num());
			}

			const FName& MappedName = NameMap[NameIndex];
			Name = FName::CreateFromDisplayId(MappedName.GetDisplayIndex(), Number);

			return *this;
		}

	private:
		const TArray<FName>& NameMap;
	};
	FNameReaderProxyArchive ProxyAr(Ar, CookedHeaderData.SummaryNames);

	if (Summary.ImportCount > 0)
	{
		CookedHeaderData.ObjectImports.Reserve(Summary.ImportCount);
		ProxyAr.Seek(Summary.ImportOffset);
		for (int32 I = 0; I < Summary.ImportCount; ++I)
		{
			ProxyAr << CookedHeaderData.ObjectImports.AddDefaulted_GetRef();
		}
	}

	if (Summary.PreloadDependencyCount > 0)
	{
		CookedHeaderData.PreloadDependencies.Reserve(Summary.PreloadDependencyCount);
		ProxyAr.Seek(Summary.PreloadDependencyOffset);
		for (int32 I = 0; I < Summary.PreloadDependencyCount; ++I)
		{
			ProxyAr << CookedHeaderData.PreloadDependencies.AddDefaulted_GetRef();
		}
	}

	if (Summary.ExportCount > 0)
	{
		CookedHeaderData.ObjectExports.Reserve(Summary.ExportCount);
		ProxyAr.Seek(Summary.ExportOffset);
		for (int32 I = 0; I < Summary.ExportCount; ++I)
		{
			FObjectExport& ObjectExport = CookedHeaderData.ObjectExports.AddDefaulted_GetRef();
			ProxyAr << ObjectExport;
		}
	}

	return CookedHeaderData;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

FPackageStoreOptimizer::FPackageStoreHeaderData FPackageStoreOptimizer::LoadPackageStoreHeader(const FIoBuffer& PackageStoreHeaderBuffer, const FPackageStoreEntryResource& PackageStoreEntry) const
{
	FPackageStoreHeaderData PackageStoreHeaderData;

	const uint8* HeaderData = PackageStoreHeaderBuffer.Data();

	FZenPackageSummary& Summary = PackageStoreHeaderData.Summary;
	Summary = *reinterpret_cast<const FZenPackageSummary*>(HeaderData);
	check(PackageStoreHeaderBuffer.DataSize() == Summary.HeaderSize);

	TArrayView<const uint8> HeaderDataView(HeaderData + sizeof(FZenPackageSummary), Summary.HeaderSize - sizeof(FZenPackageSummary));
	FMemoryReaderView HeaderDataReader(HeaderDataView);
	
	if (Summary.bHasVersioningInfo)
	{
		FZenPackageVersioningInfo& VersioningInfo = PackageStoreHeaderData.VersioningInfo.Emplace();
		HeaderDataReader << VersioningInfo;
	}

	TArray<FDisplayNameEntryId>& NameMap = PackageStoreHeaderData.NameMap;
	NameMap = LoadNameBatch(HeaderDataReader);

	for (FPackageId PackageId : PackageStoreEntry.ImportedPackageIds)
	{
		PackageStoreHeaderData.ImportedPackageIds.Add(PackageId);
	}

	PackageStoreHeaderData.ImportedPublicExportHashes =
		MakeArrayView<const uint64>(
			reinterpret_cast<const uint64*>(HeaderData + Summary.ImportedPublicExportHashesOffset),
			(Summary.ImportMapOffset - Summary.ImportedPublicExportHashesOffset) / sizeof(uint64));

	PackageStoreHeaderData.Imports =
		MakeArrayView<const FPackageObjectIndex>(
			reinterpret_cast<const FPackageObjectIndex*>(HeaderData + Summary.ImportMapOffset),
			(Summary.ExportMapOffset - Summary.ImportMapOffset) / sizeof(FPackageObjectIndex));

	PackageStoreHeaderData.Exports =
		MakeArrayView<const FExportMapEntry>(
			reinterpret_cast<const FExportMapEntry*>(HeaderData + Summary.ExportMapOffset),
			(Summary.ExportBundleEntriesOffset - Summary.ExportMapOffset) / sizeof(FExportMapEntry));

	PackageStoreHeaderData.ExportBundleHeaders =
		MakeArrayView<const FExportBundleHeader>(
			reinterpret_cast<const FExportBundleHeader*>(HeaderData + Summary.GraphDataOffset),
			PackageStoreEntry.ExportInfo.ExportBundleCount);

	PackageStoreHeaderData.ExportBundleEntries = 
		MakeArrayView<const FExportBundleEntry>(
			reinterpret_cast<const FExportBundleEntry*>(HeaderData + Summary.ExportBundleEntriesOffset),
			PackageStoreEntry.ExportInfo.ExportCount * FExportBundleEntry::ExportCommandType_Count);

	const uint64 ExportBundleHeadersSize = sizeof(FExportBundleHeader) * PackageStoreEntry.ExportInfo.ExportBundleCount;
	const uint64 ArcsDataOffset = Summary.GraphDataOffset + ExportBundleHeadersSize;
	const uint64 ArcsDataSize = Summary.HeaderSize - ArcsDataOffset;

	FMemoryReaderView ArcsAr(MakeArrayView<const uint8>(HeaderData + ArcsDataOffset, ArcsDataSize));

	int32 InternalArcsCount = 0;
	ArcsAr << InternalArcsCount;

	for (int32 Idx = 0; Idx < InternalArcsCount; ++Idx)
	{
		FPackageStorePackage::FInternalArc& InternalArc = PackageStoreHeaderData.InternalArcs.AddDefaulted_GetRef();
		ArcsAr << InternalArc.FromExportBundleIndex;
		ArcsAr << InternalArc.ToExportBundleIndex;
	}

	for (FPackageId ImportedPackageId : PackageStoreHeaderData.ImportedPackageIds)
	{
		int32 ExternalArcsCount = 0;
		ArcsAr << ExternalArcsCount;

		for (int32 Idx = 0; Idx < ExternalArcsCount; ++Idx)
		{
			FPackageStorePackage::FExternalArc ExternalArc;
			ArcsAr << ExternalArc.FromImportIndex;
			uint8 FromCommandType = 0;
			ArcsAr << FromCommandType;
			ExternalArc.FromCommandType = static_cast<FExportBundleEntry::EExportCommandType>(FromCommandType);
			ArcsAr << ExternalArc.ToExportBundleIndex;

			PackageStoreHeaderData.ExternalArcs.Add(ExternalArc);
		}
	}

	return PackageStoreHeaderData;
}

void FPackageStoreOptimizer::ResolveImport(FPackageStorePackage::FUnresolvedImport* Imports, const FObjectImport* ObjectImports, int32 LocalImportIndex) const
{
	FPackageStorePackage::FUnresolvedImport* Import = Imports + LocalImportIndex;
	if (Import->FullName.Len() == 0)
	{
		Import->FullName.Reserve(256);

		const FObjectImport* ObjectImport = ObjectImports + LocalImportIndex;
		if (ObjectImport->OuterIndex.IsNull())
		{
			FName PackageName = ObjectImport->ObjectName;
			PackageName.AppendString(Import->FullName);
			Import->FullName.ToLowerInline();
			Import->FromPackageId = FPackageId::FromName(PackageName);
			Import->FromPackageName = PackageName;
			Import->FromPackageNameLen = Import->FullName.Len();
			Import->bIsScriptImport = Import->FullName.StartsWith(TEXT("/Script/"));
			Import->bIsImportOfPackage = true;
		}
		else
		{
			const int32 OuterIndex = ObjectImport->OuterIndex.ToImport();
			ResolveImport(Imports, ObjectImports, OuterIndex);
			FPackageStorePackage::FUnresolvedImport* OuterImport = Imports + OuterIndex;
			check(OuterImport->FullName.Len() > 0);
			Import->bIsScriptImport = OuterImport->bIsScriptImport;
			Import->FullName.Append(OuterImport->FullName);
			Import->FullName.AppendChar(TEXT('/'));
			ObjectImport->ObjectName.AppendString(Import->FullName);
			Import->FullName.ToLowerInline();
			Import->FromPackageId = OuterImport->FromPackageId;
			Import->bIsImportOptional = ObjectImport->bImportOptional;
			Import->FromPackageName = OuterImport->FromPackageName;
			Import->FromPackageNameLen = OuterImport->FromPackageNameLen;
		}
	}
}

uint64 FPackageStoreOptimizer::GetPublicExportHash(FStringView PackageRelativeExportPath)
{
	check(PackageRelativeExportPath.Len() > 1);
	check(PackageRelativeExportPath[0] == '/');
	return CityHash64(reinterpret_cast<const char*>(PackageRelativeExportPath.GetData() + 1), (PackageRelativeExportPath.Len() - 1) * sizeof(TCHAR));
}

void FPackageStoreOptimizer::ProcessImports(const FCookedHeaderData& CookedHeaderData, FPackageStorePackage* Package, TArray<FPackageStorePackage::FUnresolvedImport>& UnresolvedImports) const
{
	int32 ImportCount = CookedHeaderData.ObjectImports.Num();
	UnresolvedImports.SetNum(ImportCount);
	Package->Imports.SetNum(ImportCount);

	TSet<FPackageId> ImportedPackageIds;
	for (int32 ImportIndex = 0; ImportIndex < ImportCount; ++ImportIndex)
	{
		ResolveImport(UnresolvedImports.GetData(), CookedHeaderData.ObjectImports.GetData(), ImportIndex);
		FPackageStorePackage::FUnresolvedImport& UnresolvedImport = UnresolvedImports[ImportIndex];
		if (!UnresolvedImport.bIsScriptImport )
		{
			if (UnresolvedImport.bIsImportOfPackage)
			{
				ImportedPackageIds.Add(UnresolvedImport.FromPackageId);
			}
		}
	}
	Package->ImportedPackageIds = ImportedPackageIds.Array();
	Algo::Sort(Package->ImportedPackageIds);

	for (int32 ImportIndex = 0; ImportIndex < ImportCount; ++ImportIndex)
	{
		FPackageStorePackage::FUnresolvedImport& UnresolvedImport = UnresolvedImports[ImportIndex];
		if (UnresolvedImport.bIsScriptImport)
		{
			FPackageObjectIndex ScriptObjectIndex = FPackageObjectIndex::FromScriptPath(UnresolvedImport.FullName);
			if (!ScriptObjectsMap.Contains(ScriptObjectIndex))
			{
				UE_LOG(LogPackageStoreOptimizer, Warning, TEXT("Package '%s' is referencing missing script import '%s'"), *Package->Name.ToString(), *UnresolvedImport.FullName);
			}
			Package->Imports[ImportIndex] = ScriptObjectIndex;
		}
		else if (!UnresolvedImport.bIsImportOfPackage)
		{
			bool bFoundPackageIndex = false;
			for (uint32 PackageIndex = 0, PackageCount = static_cast<uint32>(Package->ImportedPackageIds.Num()); PackageIndex < PackageCount; ++PackageIndex)
			{
				FPackageId FromPackageId = UnresolvedImport.FromPackageId;
				if (FromPackageId == Package->ImportedPackageIds[PackageIndex])
				{
					FStringView PackageRelativeName = FStringView(UnresolvedImport.FullName).RightChop(UnresolvedImport.FromPackageNameLen);
					check(PackageRelativeName.Len());
					FPackageImportReference PackageImportRef(PackageIndex, Package->ImportedPublicExportHashes.Num());
					Package->Imports[ImportIndex] = FPackageObjectIndex::FromPackageImportRef(PackageImportRef);
					uint64 ExportHash = GetPublicExportHash(PackageRelativeName);
					Package->ImportedPublicExportHashes.Add(ExportHash);
					bFoundPackageIndex = true;
					break;
				}
			}
			check(bFoundPackageIndex);
		}
	}
}

void FPackageStoreOptimizer::ProcessImports(const FPackageStoreHeaderData& PackageStoreHeaderData, FPackageStorePackage* Package) const
{
	Package->ImportedPackageIds = PackageStoreHeaderData.ImportedPackageIds;
	Package->ImportedPublicExportHashes = PackageStoreHeaderData.ImportedPublicExportHashes;
	Package->Imports = PackageStoreHeaderData.Imports;
}

void FPackageStoreOptimizer::ResolveExport(
	FPackageStorePackage::FExport* Exports,
	const FObjectExport* ObjectExports,
	const int32 LocalExportIndex,
	const FName& PackageName,
	FPackageStorePackage::FUnresolvedImport* Imports,
	const FObjectImport* ObjectImports) const
{
	FPackageStorePackage::FExport* Export = Exports + LocalExportIndex;
	if (Export->FullName.Len() == 0)
	{
		Export->FullName.Reserve(256);
		const FObjectExport* ObjectExport = ObjectExports + LocalExportIndex;
		if (ObjectExport->OuterIndex.IsNull())
		{
			PackageName.AppendString(Export->FullName);
			Export->FullName.AppendChar(TEXT('/'));
			ObjectExport->ObjectName.AppendString(Export->FullName);
			Export->FullName.ToLowerInline();
			check(Export->FullName.Len() > 0);
		}
		else
		{
			FString* OuterName = nullptr;
			if (ObjectExport->OuterIndex.IsExport())
			{
				int32 OuterExportIndex = ObjectExport->OuterIndex.ToExport();
				ResolveExport(Exports, ObjectExports, OuterExportIndex, PackageName, Imports, ObjectImports);
				OuterName = &Exports[OuterExportIndex].FullName;
			}
			else
			{
				check(Imports && ObjectImports);
				int32 OuterImportIndex = ObjectExport->OuterIndex.ToImport();
				ResolveImport(Imports, ObjectImports, OuterImportIndex);
				OuterName = &Imports[OuterImportIndex].FullName;

			}
			check(OuterName && OuterName->Len() > 0);
			Export->FullName.Append(*OuterName);
			Export->FullName.AppendChar(TEXT('/'));
			ObjectExport->ObjectName.AppendString(Export->FullName);
			Export->FullName.ToLowerInline();
		}
	}
}

void FPackageStoreOptimizer::ResolveExport(FPackageStorePackage::FExport* Exports, const int32 LocalExportIndex, const FName& PackageName) const
{
	FPackageStorePackage::FExport* Export = Exports + LocalExportIndex;
	if (Export->FullName.Len() == 0)
	{
		Export->FullName.Reserve(256);
		if (Export->OuterIndex.IsNull())
		{
			PackageName.AppendString(Export->FullName);
			Export->FullName.AppendChar(TEXT('/'));
			Export->ObjectName.AppendString(Export->FullName);
			Export->FullName.ToLowerInline();
			check(Export->FullName.Len() > 0);
		}
		else
		{
			check(Export->OuterIndex.IsExport());
			int32 OuterExportIndex = Export->OuterIndex.ToExport();
			ResolveExport(Exports, OuterExportIndex, PackageName);
			FString& OuterName = Exports[OuterExportIndex].FullName;
			check(OuterName.Len() > 0);
			Export->FullName.Append(OuterName);
			Export->FullName.AppendChar(TEXT('/'));
			Export->ObjectName.AppendString(Export->FullName);
			Export->FullName.ToLowerInline();
		}
	}
}

void FPackageStoreOptimizer::ProcessExports(const FCookedHeaderData& CookedHeaderData, FPackageStorePackage* Package, FPackageStorePackage::FUnresolvedImport* Imports) const
{
	int32 ExportCount = CookedHeaderData.ObjectExports.Num();
	Package->Exports.SetNum(ExportCount);
	Package->ExportGraphNodes.Reserve(ExportCount * 2);

	auto PackageObjectIdFromPackageIndex =
		[](const TArray<FPackageObjectIndex>& Imports, const FPackageIndex& PackageIndex) -> FPackageObjectIndex
	{
		if (PackageIndex.IsImport())
		{
			return Imports[PackageIndex.ToImport()];
		}
		if (PackageIndex.IsExport())
		{
			return FPackageObjectIndex::FromExportIndex(PackageIndex.ToExport());
		}
		return FPackageObjectIndex();
	};

	FString PackageNameStr = Package->Name.ToString();
	TMap<uint64, const FPackageStorePackage::FExport*> SeenPublicExportHashes;
	for (int32 ExportIndex = 0; ExportIndex < ExportCount; ++ExportIndex)
	{
		const FObjectExport& ObjectExport = CookedHeaderData.ObjectExports[ExportIndex];
		Package->ExportsSerialSize += ObjectExport.SerialSize;

		FPackageStorePackage::FExport& Export = Package->Exports[ExportIndex];
		Export.ObjectName = ObjectExport.ObjectName;
		Export.ObjectFlags = ObjectExport.ObjectFlags;
		Export.CookedSerialOffset = ObjectExport.SerialOffset;
		Export.SerialOffset = ObjectExport.SerialOffset - CookedHeaderData.Summary.TotalHeaderSize;
		Export.SerialSize = ObjectExport.SerialSize;
		Export.bNotForClient = ObjectExport.bNotForClient;
		Export.bNotForServer = ObjectExport.bNotForServer;
		Export.bIsPublic = (Export.ObjectFlags & RF_Public) > 0 || ObjectExport.bGeneratePublicHash;
		ResolveExport(Package->Exports.GetData(), CookedHeaderData.ObjectExports.GetData(), ExportIndex, Package->Name, Imports, CookedHeaderData.ObjectImports.GetData());
		if (Export.bIsPublic)
		{
			check(Export.FullName.Len() > 0);
			FStringView PackageRelativeName = FStringView(Export.FullName).RightChop(PackageNameStr.Len());
			check(PackageRelativeName.Len());
			Export.PublicExportHash = GetPublicExportHash(PackageRelativeName);
			const FPackageStorePackage::FExport* FindCollidingExport = SeenPublicExportHashes.FindRef(Export.PublicExportHash);
			if (FindCollidingExport)
			{
				UE_LOG(LogPackageStoreOptimizer, Fatal, TEXT("Export hash collision in package \"%s\": \"%s\" and \"%s"), *PackageNameStr, PackageRelativeName.GetData(), *FindCollidingExport->FullName.RightChop(PackageNameStr.Len()));
			}
			SeenPublicExportHashes.Add(Export.PublicExportHash, &Export);
		}

		Export.OuterIndex = PackageObjectIdFromPackageIndex(Package->Imports, ObjectExport.OuterIndex);
		Export.ClassIndex = PackageObjectIdFromPackageIndex(Package->Imports, ObjectExport.ClassIndex);
		Export.SuperIndex = PackageObjectIdFromPackageIndex(Package->Imports, ObjectExport.SuperIndex);
		Export.TemplateIndex = PackageObjectIdFromPackageIndex(Package->Imports, ObjectExport.TemplateIndex);

		for (uint8 CommandType = 0; CommandType < FExportBundleEntry::ExportCommandType_Count; ++CommandType)
		{
			FPackageStorePackage::FExportGraphNode& Node = Package->ExportGraphNodes.AddDefaulted_GetRef();
			Node.BundleEntry.CommandType = FExportBundleEntry::EExportCommandType(CommandType);
			Node.BundleEntry.LocalExportIndex = ExportIndex;
			Node.bIsPublic = Export.bIsPublic;
			Export.Nodes[CommandType] = &Node;
		}
	}
}

void FPackageStoreOptimizer::ProcessExports(const FPackageStoreHeaderData& PackageStoreHeaderData, FPackageStorePackage* Package) const
{
	const int32 ExportCount = PackageStoreHeaderData.Exports.Num();
	Package->Exports.SetNum(ExportCount);
	Package->ExportGraphNodes.Reserve(ExportCount * 2);

	const TArray<FDisplayNameEntryId>& NameMap = PackageStoreHeaderData.NameMap;

	Package->ImportedPublicExportHashes = PackageStoreHeaderData.ImportedPublicExportHashes;
	for (int32 ExportIndex = 0; ExportIndex < ExportCount; ++ExportIndex)
	{
		const FExportMapEntry& ExportEntry =  PackageStoreHeaderData.Exports[ExportIndex];
		Package->ExportsSerialSize += ExportEntry.CookedSerialSize;

		FPackageStorePackage::FExport& Export = Package->Exports[ExportIndex];
		Export.ObjectName = ExportEntry.ObjectName.ResolveName(NameMap);
		Export.PublicExportHash = ExportEntry.PublicExportHash;
		Export.OuterIndex = ExportEntry.OuterIndex;
		Export.ClassIndex = ExportEntry.ClassIndex;
		Export.SuperIndex = ExportEntry.SuperIndex;
		Export.TemplateIndex = ExportEntry.TemplateIndex;
		Export.ObjectFlags = ExportEntry.ObjectFlags;
		Export.CookedSerialOffset = ExportEntry.CookedSerialOffset;
		Export.SerialSize = ExportEntry.CookedSerialSize;
		Export.bNotForClient = ExportEntry.FilterFlags == EExportFilterFlags::NotForClient;
		Export.bNotForServer = ExportEntry.FilterFlags == EExportFilterFlags::NotForServer;
		Export.bIsPublic = (ExportEntry.ObjectFlags & RF_Public) > 0;

		for (uint8 CommandType = 0; CommandType < FExportBundleEntry::ExportCommandType_Count; ++CommandType)
		{
			FPackageStorePackage::FExportGraphNode& Node = Package->ExportGraphNodes.AddDefaulted_GetRef();
			Node.BundleEntry.CommandType = FExportBundleEntry::EExportCommandType(CommandType);
			Node.BundleEntry.LocalExportIndex = ExportIndex;
			Node.bIsPublic = Export.bIsPublic;
			Export.Nodes[CommandType] = &Node;
		}

		Package->NameMapBuilder.MarkNameAsReferenced(Export.ObjectName);
	}

	for (int32 ExportIndex = 0; ExportIndex < ExportCount; ++ExportIndex)
	{
		ResolveExport(Package->Exports.GetData(), ExportIndex, Package->Name);
	}

	uint64 ExportSerialOffset = 0;
	for (const FExportBundleHeader& ExportBundleHeader : PackageStoreHeaderData.ExportBundleHeaders)
	{
		int32 ExportEntryIndex = ExportBundleHeader.FirstEntryIndex;
		for (uint32 Idx = 0; Idx < ExportBundleHeader.EntryCount; ++Idx)
		{
			const FExportBundleEntry& BundleEntry = PackageStoreHeaderData.ExportBundleEntries[ExportEntryIndex++];
			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				FPackageStorePackage::FExport& Export = Package->Exports[BundleEntry.LocalExportIndex];
				Export.SerialOffset = ExportSerialOffset;
				ExportSerialOffset += Export.SerialSize;
			}
		}
	}
	check(ExportSerialOffset == Package->ExportsSerialSize);
}

TArray<FPackageStorePackage*> FPackageStoreOptimizer::SortPackagesInLoadOrder(const TMap<FPackageId, FPackageStorePackage*>& PackagesMap) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SortPackagesInLoadOrder);
	TArray<FPackageStorePackage*> Packages;
	PackagesMap.GenerateValueArray(Packages);
	Algo::Sort(Packages, [](const FPackageStorePackage* A, const FPackageStorePackage* B)
	{
		return A->Id < B->Id;
	});

	TMap<FPackageStorePackage*, TArray<FPackageStorePackage*>> SortedEdges;
	for (FPackageStorePackage* Package : Packages)
	{
		for (FPackageId ImportedPackageId : Package->ImportedPackageIds)
		{
			FPackageStorePackage* FindImportedPackage = PackagesMap.FindRef(ImportedPackageId);
			if (FindImportedPackage)
			{
				TArray<FPackageStorePackage*>& SourceArray = SortedEdges.FindOrAdd(FindImportedPackage);
				SourceArray.Add(Package);
			}
		}
	}
	for (auto& KV : SortedEdges)
	{
		TArray<FPackageStorePackage*>& SourceArray = KV.Value;
		Algo::Sort(SourceArray, [](const FPackageStorePackage* A, const FPackageStorePackage* B)
		{
			return A->Id < B->Id;
		});
	}

	TArray<FPackageStorePackage*> Result;
	Result.Reserve(Packages.Num());
	struct
	{
		void Visit(FPackageStorePackage* Package)
		{
			if (Package->bPermanentMark || Package->bTemporaryMark)
			{
				return;
			}
			Package->bTemporaryMark = true;
			TArray<FPackageStorePackage*>* TargetNodes = Edges.Find(Package);
			if (TargetNodes)
			{
				for (FPackageStorePackage* ToNode : *TargetNodes)
				{
					Visit(ToNode);
				}
			}
			Package->bTemporaryMark = false;
			Package->bPermanentMark = true;
			Result.Add(Package);
		}

		TMap<FPackageStorePackage*, TArray<FPackageStorePackage*>>& Edges;
		TArray<FPackageStorePackage*>& Result;
	} Visitor{ SortedEdges, Result };

	for (FPackageStorePackage* Package : Packages)
	{
		Visitor.Visit(Package);
	}
	check(Result.Num() == Packages.Num());
	Algo::Reverse(Result);
	Swap(Result, Packages);
	return Packages;
}

TArray<FPackageStorePackage::FExportBundleGraphNode*> FPackageStoreOptimizer::SortExportBundleGraphNodesInLoadOrder(const TArray<FPackageStorePackage*>& Packages, FExportBundleGraphEdges& Edges) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SortExportBundleGraphNodesInLoadOrder);
	int32 NodeCount = 0;
	for (FPackageStorePackage* Package : Packages)
	{
		NodeCount += Package->ExportGraphNodes.Num();
	}
	for (auto& KV : Edges)
	{
		FPackageStorePackage::FExportBundleGraphNode* ToNode = KV.Value;
		++ToNode->IncomingEdgeCount;
	}

	auto NodeSorter = [](const FPackageStorePackage::FExportBundleGraphNode& A, const FPackageStorePackage::FExportBundleGraphNode& B)
	{
		return A.Index < B.Index;
	};

	for (FPackageStorePackage* Package : Packages)
	{
		for (FPackageStorePackage::FExportBundleGraphNode& Node : Package->ExportBundleGraphNodes)
		{
			if (Node.IncomingEdgeCount == 0)
			{
				Package->NodesWithNoIncomingEdges.HeapPush(&Node, NodeSorter);
			}
		}
	}
	TArray<FPackageStorePackage::FExportBundleGraphNode*> LoadOrder;
	LoadOrder.Reserve(NodeCount);
	while (LoadOrder.Num() < NodeCount)
	{
		bool bMadeProgress = false;
		for (FPackageStorePackage* Package : Packages)
		{
			while (Package->NodesWithNoIncomingEdges.Num())
			{
				FPackageStorePackage::FExportBundleGraphNode* RemovedNode;
				Package->NodesWithNoIncomingEdges.HeapPop(RemovedNode, NodeSorter, false);
				LoadOrder.Add(RemovedNode);
				bMadeProgress = true;
				for (auto EdgeIt = Edges.CreateKeyIterator(RemovedNode); EdgeIt; ++EdgeIt)
				{
					FPackageStorePackage::FExportBundleGraphNode* ToNode = EdgeIt.Value();
					check(ToNode->IncomingEdgeCount > 0);
					if (--ToNode->IncomingEdgeCount == 0)
					{
						ToNode->Package->NodesWithNoIncomingEdges.HeapPush(ToNode, NodeSorter);
					}
					EdgeIt.RemoveCurrent();
				}
			}
		}
		check(bMadeProgress);
	}
	return LoadOrder;
}

void FPackageStoreOptimizer::OptimizeExportBundles(const TMap<FPackageId, FPackageStorePackage*>& PackagesMap)
{
	TArray<FPackageStorePackage*> Packages = SortPackagesInLoadOrder(PackagesMap);
	for (FPackageStorePackage* Package : Packages)
	{
		Package->ExportBundleGraphNodes.Reserve(Package->GraphData.ExportBundles.Num());
		for (FPackageStorePackage::FExportBundle& ExportBundle : Package->GraphData.ExportBundles)
		{
			FPackageStorePackage::FExportBundleGraphNode& Node = Package->ExportBundleGraphNodes.AddDefaulted_GetRef();
			Node.Package = Package;
			Node.Index = Package->ExportBundleGraphNodes.Num() - 1;
			Node.ExportGraphNodes.Reserve(ExportBundle.Entries.Num());
			for (FExportBundleEntry& ExportBundleEntry : ExportBundle.Entries)
			{
				FPackageStorePackage::FExport& Export = Package->Exports[ExportBundleEntry.LocalExportIndex];
				Node.ExportGraphNodes.Add(Export.Nodes[ExportBundleEntry.CommandType]);
			}
		}
		Package->GraphData.ExportBundles.Empty();
	}

	FExportBundleGraphEdges Edges;
	TSet<FPackageStorePackage::FExportBundleGraphNode*> Dependencies;
	for (FPackageStorePackage* Package : Packages)
	{
		for (FPackageStorePackage::FExportBundleGraphNode& ExportBundleGraphNode : Package->ExportBundleGraphNodes)
		{
			for (FPackageStorePackage::FExportGraphNode* ExportGraphNode : ExportBundleGraphNode.ExportGraphNodes)
			{
				check(ExportGraphNode->ExportBundleIndex >= 0);
				for (FPackageStorePackage::FExportGraphNode* InternalDependency : ExportGraphNode->InternalDependencies)
				{
					check(InternalDependency->ExportBundleIndex >= 0);
					FPackageStorePackage::FExportBundleGraphNode* FromNode = &Package->ExportBundleGraphNodes[InternalDependency->ExportBundleIndex];
					Dependencies.Add(FromNode);
				}
				for (FPackageStorePackage::FExternalDependency& ExternalDependency : ExportGraphNode->ExternalDependencies)
				{
					FPackageObjectIndex FromImport = Package->Imports[ExternalDependency.ImportIndex];
					check(FromImport.IsPackageImport());
					FPackageImportReference FromPackageImportRef = FromImport.ToPackageImportRef();
					FPackageId FromPackageId = Package->ImportedPackageIds[FromPackageImportRef.GetImportedPackageIndex()];
					uint64 FromPublicExportHash = Package->ImportedPublicExportHashes[FromPackageImportRef.GetImportedPublicExportHashIndex()];
					FPackageStorePackage* FindFromPackage = PackagesMap.FindRef(FromPackageId);
					if (FindFromPackage)
					{
						bool bFoundExport = false;
						for (int32 ExportIndex = 0; ExportIndex < FindFromPackage->Exports.Num(); ++ExportIndex)
						{
							FPackageStorePackage::FExport& FromExport = FindFromPackage->Exports[ExportIndex];
							if (FromExport.PublicExportHash == FromPublicExportHash)
							{
								FPackageStorePackage::FExportGraphNode* FromExportGraphNode = FromExport.Nodes[ExternalDependency.ExportBundleCommandType];
								check(FromExportGraphNode->ExportBundleIndex >= 0);
								FPackageStorePackage::FExportBundleGraphNode* FromNode = &FindFromPackage->ExportBundleGraphNodes[FromExportGraphNode->ExportBundleIndex];
								Dependencies.Add(FromNode);
								bFoundExport = true;
								break;
							}
						}
					}
				}
			}
			for (FPackageStorePackage::FExportBundleGraphNode* FromNode : Dependencies)
			{
				Edges.Add(FromNode, &ExportBundleGraphNode);
			}
			Dependencies.Reset();
		}
	}

	TArray<FPackageStorePackage::FExportBundleGraphNode*> LoadOrder = SortExportBundleGraphNodesInLoadOrder(Packages, Edges);

	FPackageStorePackage* PreviousPackage = nullptr;
	for (const FPackageStorePackage::FExportBundleGraphNode* Node : LoadOrder)
	{
		check(Node);
		FPackageStorePackage* Package = Node->Package;
		check(Package);
		if (Package->CurrentBundle == nullptr || Package != PreviousPackage)
		{
			if (Package->GraphData.ExportBundles.IsEmpty())
			{
				Package->LoadOrder = NextLoadOrder++;
			}
			Package->CurrentBundle = &Package->GraphData.ExportBundles.AddDefaulted_GetRef();
		}
		for (FPackageStorePackage::FExportGraphNode* ExportGraphNode : Node->ExportGraphNodes)
		{
			Package->CurrentBundle->Entries.Add(ExportGraphNode->BundleEntry);
			ExportGraphNode->ExportBundleIndex = Package->GraphData.ExportBundles.Num() - 1;
		}
		PreviousPackage = Package;
	}
}

void FPackageStoreOptimizer::ProcessPreloadDependencies(const FCookedHeaderData& CookedHeaderData, FPackageStorePackage* Package) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessPreloadDependencies);

	auto AddInternalDependency = [](FPackageStorePackage* Package, int32 FromExportIndex, FExportBundleEntry::EExportCommandType FromExportBundleCommandType, FPackageStorePackage::FExportGraphNode* ToNode)
	{
		FPackageStorePackage::FExport& FromExport = Package->Exports[FromExportIndex];
		FPackageStorePackage::FExportGraphNode* FromNode = FromExport.Nodes[FromExportBundleCommandType];
		ToNode->InternalDependencies.Add(FromNode);
	};

	auto AddExternalDependency = [](FPackageStorePackage* Package, int32 FromImportIndex, FExportBundleEntry::EExportCommandType FromExportBundleCommandType, FPackageStorePackage::FExportGraphNode* ToNode)
	{
		FPackageObjectIndex FromImport = Package->Imports[FromImportIndex];
		if (FromImport.IsScriptImport())
		{
			return;
		}

		FPackageStorePackage::FExternalDependency& ExternalDependency = ToNode->ExternalDependencies.AddDefaulted_GetRef();
		ExternalDependency.ImportIndex = FromImportIndex;
		ExternalDependency.ExportBundleCommandType = FromExportBundleCommandType;
	};

	for (int32 ExportIndex = 0; ExportIndex < Package->Exports.Num(); ++ExportIndex)
	{
		FPackageStorePackage::FExport& Export = Package->Exports[ExportIndex];
		const FObjectExport& ObjectExport = CookedHeaderData.ObjectExports[ExportIndex];

		AddInternalDependency(Package, ExportIndex, FExportBundleEntry::ExportCommandType_Create, Export.Nodes[FExportBundleEntry::ExportCommandType_Serialize]);

		if (ObjectExport.FirstExportDependency >= 0)
		{
			int32 RunningIndex = ObjectExport.FirstExportDependency;
			for (int32 Index = ObjectExport.SerializationBeforeSerializationDependencies; Index > 0; Index--)
			{
				FPackageIndex Dep = CookedHeaderData.PreloadDependencies[RunningIndex++];
				if (Dep.IsExport())
				{
					AddInternalDependency(Package, Dep.ToExport(), FExportBundleEntry::ExportCommandType_Serialize, Export.Nodes[FExportBundleEntry::ExportCommandType_Serialize]);
				}
				else
				{
					AddExternalDependency(Package, Dep.ToImport(), FExportBundleEntry::ExportCommandType_Serialize, Export.Nodes[FExportBundleEntry::ExportCommandType_Serialize]);
				}
			}

			for (int32 Index = ObjectExport.CreateBeforeSerializationDependencies; Index > 0; Index--)
			{
				FPackageIndex Dep = CookedHeaderData.PreloadDependencies[RunningIndex++];
				if (Dep.IsExport())
				{
					AddInternalDependency(Package, Dep.ToExport(), FExportBundleEntry::ExportCommandType_Create, Export.Nodes[FExportBundleEntry::ExportCommandType_Serialize]);
				}
				else
				{
					AddExternalDependency(Package, Dep.ToImport(), FExportBundleEntry::ExportCommandType_Create, Export.Nodes[FExportBundleEntry::ExportCommandType_Serialize]);
				}
			}

			for (int32 Index = ObjectExport.SerializationBeforeCreateDependencies; Index > 0; Index--)
			{
				FPackageIndex Dep = CookedHeaderData.PreloadDependencies[RunningIndex++];
				if (Dep.IsExport())
				{
					AddInternalDependency(Package, Dep.ToExport(), FExportBundleEntry::ExportCommandType_Serialize, Export.Nodes[FExportBundleEntry::ExportCommandType_Create]);
				}
				else
				{
					AddExternalDependency(Package, Dep.ToImport(), FExportBundleEntry::ExportCommandType_Serialize, Export.Nodes[FExportBundleEntry::ExportCommandType_Create]);
				}
			}

			for (int32 Index = ObjectExport.CreateBeforeCreateDependencies; Index > 0; Index--)
			{
				FPackageIndex Dep = CookedHeaderData.PreloadDependencies[RunningIndex++];
				if (Dep.IsExport())
				{
					AddInternalDependency(Package, Dep.ToExport(), FExportBundleEntry::ExportCommandType_Create, Export.Nodes[FExportBundleEntry::ExportCommandType_Create]);
				}
				else
				{
					AddExternalDependency(Package, Dep.ToImport(), FExportBundleEntry::ExportCommandType_Create, Export.Nodes[FExportBundleEntry::ExportCommandType_Create]);
				}
			}
		}
	}
}

void FPackageStoreOptimizer::ProcessPreloadDependencies(const FPackageStoreHeaderData& PackageStoreHeaderData, FPackageStorePackage* Package) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessPreloadDependencies);

	for (const FPackageStorePackage::FInternalArc& InternalArc : PackageStoreHeaderData.InternalArcs)
	{
		const FExportBundleHeader& FromExportBundle = PackageStoreHeaderData.ExportBundleHeaders[InternalArc.FromExportBundleIndex];
		const FExportBundleHeader& ToExportBundle = PackageStoreHeaderData.ExportBundleHeaders[InternalArc.ToExportBundleIndex];

		uint32 FromBundleEntryIndex = FromExportBundle.FirstEntryIndex;
		const uint32 LastFromBundleEntryIndex = FromBundleEntryIndex + FromExportBundle.EntryCount;
		while (FromBundleEntryIndex < LastFromBundleEntryIndex)
		{
			const FExportBundleEntry& FromBundleEntry = PackageStoreHeaderData.ExportBundleEntries[FromBundleEntryIndex++];
			FPackageStorePackage::FExport& FromExport = Package->Exports[FromBundleEntry.LocalExportIndex];
			FPackageStorePackage::FExportGraphNode* FromNode = FromExport.Nodes[FromBundleEntry.CommandType];

			uint32 ToBundleEntryIndex = ToExportBundle.FirstEntryIndex;
			const uint32 LastToBundleEntryIndex = ToBundleEntryIndex + ToExportBundle.EntryCount;
			while (ToBundleEntryIndex < LastToBundleEntryIndex)
			{
				const FExportBundleEntry& ToBundleEntry = PackageStoreHeaderData.ExportBundleEntries[ToBundleEntryIndex++];
				FPackageStorePackage::FExport& ToExport = Package->Exports[ToBundleEntry.LocalExportIndex];
				FPackageStorePackage::FExportGraphNode* ToNode = ToExport.Nodes[ToBundleEntry.CommandType];
				ToNode->InternalDependencies.Add(FromNode);
			}
		}
	}

	for (const FPackageStorePackage::FExternalArc& ExternalArc : PackageStoreHeaderData.ExternalArcs)
	{
		const FExportBundleHeader& ToExportBundle = PackageStoreHeaderData.ExportBundleHeaders[ExternalArc.ToExportBundleIndex];
		uint32 ToBundleEntryIndex = ToExportBundle.FirstEntryIndex;
		const uint32 LastToBundleEntryIndex = ToBundleEntryIndex + ToExportBundle.EntryCount;
		while (ToBundleEntryIndex < LastToBundleEntryIndex)
		{
			const FExportBundleEntry& ToBundleEntry = PackageStoreHeaderData.ExportBundleEntries[ToBundleEntryIndex++];
			FPackageStorePackage::FExport& ToExport = Package->Exports[ToBundleEntry.LocalExportIndex];
			FPackageStorePackage::FExportGraphNode* ToNode = ToExport.Nodes[ToBundleEntry.CommandType];
			FPackageStorePackage::FExternalDependency& ExternalDependency = ToNode->ExternalDependencies.AddDefaulted_GetRef();
			ExternalDependency.ImportIndex = ExternalArc.FromImportIndex;
			ExternalDependency.ExportBundleCommandType = ExternalArc.FromCommandType;
		}
	}
}

TArray<FPackageStorePackage::FExportGraphNode*> FPackageStoreOptimizer::SortExportGraphNodesInLoadOrder(FPackageStorePackage* Package, FExportGraphEdges& Edges) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SortExportGraphNodesInLoadOrder);
	int32 NodeCount = Package->ExportGraphNodes.Num();
	for (auto& KV : Edges)
	{
		FPackageStorePackage::FExportGraphNode* ToNode = KV.Value;
		++ToNode->IncomingEdgeCount;
	}

	auto NodeSorter = [](const FPackageStorePackage::FExportGraphNode& A, const FPackageStorePackage::FExportGraphNode& B)
	{
		if (A.bIsPublic != B.bIsPublic)
		{
			return A.bIsPublic;
		}
		if (A.BundleEntry.CommandType != B.BundleEntry.CommandType)
		{
			return A.BundleEntry.CommandType < B.BundleEntry.CommandType;
		}
		return A.BundleEntry.LocalExportIndex < B.BundleEntry.LocalExportIndex;
	};

	TArray<FPackageStorePackage::FExportGraphNode*> NodesWithNoIncomingEdges;
	NodesWithNoIncomingEdges.Reserve(NodeCount);
	for (FPackageStorePackage::FExportGraphNode& Node : Package->ExportGraphNodes)
	{
		if (Node.IncomingEdgeCount == 0)
		{
			NodesWithNoIncomingEdges.HeapPush(&Node, NodeSorter);
		}
	}

	TArray<FPackageStorePackage::FExportGraphNode*> LoadOrder;
	LoadOrder.Reserve(NodeCount);
	while (NodesWithNoIncomingEdges.Num())
	{
		FPackageStorePackage::FExportGraphNode* RemovedNode;
		NodesWithNoIncomingEdges.HeapPop(RemovedNode, NodeSorter, false);
		LoadOrder.Add(RemovedNode);
		for (auto EdgeIt = Edges.CreateKeyIterator(RemovedNode); EdgeIt; ++EdgeIt)
		{
			FPackageStorePackage::FExportGraphNode* ToNode = EdgeIt.Value();
			check(ToNode->IncomingEdgeCount > 0);
			if (--ToNode->IncomingEdgeCount == 0)
			{
				NodesWithNoIncomingEdges.HeapPush(ToNode, NodeSorter);
			}
			EdgeIt.RemoveCurrent();
		}
	}
	check(LoadOrder.Num() == NodeCount);
	return LoadOrder;
}

void FPackageStoreOptimizer::CreateExportBundles(FPackageStorePackage* Package) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateExportBundles);
	FExportGraphEdges Edges;
	for (FPackageStorePackage::FExportGraphNode& ExportGraphNode : Package->ExportGraphNodes)
	{
		for (FPackageStorePackage::FExportGraphNode* InternalDependency : ExportGraphNode.InternalDependencies)
		{
			Edges.Add(InternalDependency, &ExportGraphNode);
		}
	}
	TArray<FPackageStorePackage::FExportGraphNode*> LoadOrder = SortExportGraphNodesInLoadOrder(Package, Edges);
	FPackageStorePackage::FExportBundle* CurrentBundle = nullptr;
	for (FPackageStorePackage::FExportGraphNode* Node : LoadOrder)
	{
		if (!CurrentBundle)
		{
			Package->CurrentBundle = &Package->GraphData.ExportBundles.AddDefaulted_GetRef();
		}
		Package->CurrentBundle->Entries.Add(Node->BundleEntry);
		Node->ExportBundleIndex = Package->GraphData.ExportBundles.Num() - 1;
		if (Node->bIsPublic)
		{
			CurrentBundle = nullptr;
		}
	}
}

static const TCHAR* GetExportNameSafe(const FString& ExportFullName, const FName& PackageName, int32 PackageNameLen)
{
	const bool bValidNameLen = ExportFullName.Len() > PackageNameLen + 1;
	if (bValidNameLen)
	{
		const TCHAR* ExportNameStr = *ExportFullName + PackageNameLen;
		const bool bValidNameFormat = *ExportNameStr == '/';
		if (bValidNameFormat)
		{
			return ExportNameStr + 1; // skip verified '/'
		}
		else
		{
			UE_CLOG(!bValidNameFormat, LogPackageStoreOptimizer, Warning,
				TEXT("Export name '%s' should start with '/' at position %d, i.e. right after package prefix '%s'"),
				*ExportFullName,
				PackageNameLen,
				*PackageName.ToString());
		}
	}
	else
	{
		UE_CLOG(!bValidNameLen, LogPackageStoreOptimizer, Warning,
			TEXT("Export name '%s' with length %d should be longer than package name '%s' with length %d"),
			*ExportFullName,
			PackageNameLen,
			*PackageName.ToString());
	}

	return nullptr;
};

bool FPackageStoreOptimizer::VerifyRedirect(
	const FPackageStorePackage* SourcePackage,
	FPackageStorePackage& TargetPackage,
	bool bIsBuildingDLC) const
{
	if (!SourcePackage)
	{
		if (bIsBuildingDLC)
		{
			// We can't verify against the source package but check for presence of UStructs
			for (const FPackageStorePackage::FExport& Export : TargetPackage.Exports)
			{
				if (!Export.SuperIndex.IsNull() && Export.OuterIndex.IsNull())
				{
					UE_LOG(LogPackageStoreOptimizer, Warning, TEXT("Skipping redirect to package '%s' due to presence of UStruct '%s'"), *TargetPackage.Name.ToString(), *Export.ObjectName.ToString());
					return false;
				}
			}
			return true;
		}
		return false;
	}

	const int32 ExportCount =
		SourcePackage->Exports.Num() < TargetPackage.Exports.Num() ?
		SourcePackage->Exports.Num() :
		TargetPackage.Exports.Num();

	UE_CLOG(SourcePackage->Exports.Num() != TargetPackage.Exports.Num(), LogPackageStoreOptimizer, Verbose,
		TEXT("Redirection target package '%s' (0x%llX) for source package '%s' (0x%llX)  - Has ExportCount %d vs. %d"),
		*TargetPackage.Name.ToString(),
		TargetPackage.Id.ValueForDebugging(),
		*SourcePackage->Name.ToString(),
		SourcePackage->Id.ValueForDebugging(),
		TargetPackage.Exports.Num(),
		SourcePackage->Exports.Num());

	auto AppendMismatchMessage = [&TargetPackage, SourcePackage](
		const TCHAR* Text, FName ExportName, FPackageObjectIndex TargetIndex, FPackageObjectIndex SourceIndex, FString& FailReason)
	{
		FailReason.Appendf(TEXT("Public export '%s' has %s %s vs. %s"),
			*ExportName.ToString(),
			Text,
			*TargetPackage.Exports[TargetIndex.ToExport()].FullName,
			*SourcePackage->Exports[SourceIndex.ToExport()].FullName);
	};

	const int32 TargetPackageNameLen = TargetPackage.Name.GetStringLength();
	const int32 SourcePackageNameLen = SourcePackage->Name.GetStringLength();

	bool bSuccess = true;
	int32 TargetIndex = 0;
	int32 SourceIndex = 0;
	while (TargetIndex < ExportCount && SourceIndex < ExportCount)
	{
		FString FailReason;
		const FPackageStorePackage::FExport& TargetExport = TargetPackage.Exports[TargetIndex];
		const FPackageStorePackage::FExport& SourceExport = SourcePackage->Exports[SourceIndex];

		const TCHAR* TargetExportStr = GetExportNameSafe(
			TargetExport.FullName, TargetPackage.Name, TargetPackageNameLen);
		const TCHAR* SourceExportStr = GetExportNameSafe(
			SourceExport.FullName, SourcePackage->Name, SourcePackageNameLen);

		if (!TargetExportStr || !SourceExportStr)
		{
			UE_LOG(LogPackageStoreOptimizer, Error,
				TEXT("Redirection target package '%s' (0x%llX) for source package '%s' (0x%llX) - Has some bad data from an earlier phase."),
				*TargetPackage.Name.ToString(),
				TargetPackage.Id.ValueForDebugging(),
				*SourcePackage->Name.ToString(),
				SourcePackage->Id.ValueForDebugging())
				return false;
		}

		int32 CompareResult = FCString::Stricmp(TargetExportStr, SourceExportStr);
		if (CompareResult < 0)
		{
			++TargetIndex;
		}
		else if (CompareResult > 0)
		{
			++SourceIndex;

			if (SourceExport.bIsPublic)
			{
				FailReason.Appendf(TEXT("Public source export '%s' is missing in the localized package"),
					*SourceExport.ObjectName.ToString());
			}
		}
		else
		{
			++TargetIndex;
			++SourceIndex;

			if (SourceExport.bIsPublic)
			{
				if (!TargetExport.bIsPublic)
				{
					FailReason.Appendf(TEXT("Public source export '%s' exists in the localized package")
						TEXT(", but is not a public localized export."),
						*SourceExport.ObjectName.ToString());
				}
				else if (TargetExport.ClassIndex != SourceExport.ClassIndex)
				{
					AppendMismatchMessage(TEXT("class"), TargetExport.ObjectName,
						TargetExport.ClassIndex, SourceExport.ClassIndex, FailReason);
				}
				else if (TargetExport.TemplateIndex != SourceExport.TemplateIndex)
				{
					AppendMismatchMessage(TEXT("template"), TargetExport.ObjectName,
						TargetExport.TemplateIndex, SourceExport.TemplateIndex, FailReason);
				}
				else if (TargetExport.SuperIndex != SourceExport.SuperIndex)
				{
					AppendMismatchMessage(TEXT("super"), TargetExport.ObjectName,
						TargetExport.SuperIndex, SourceExport.SuperIndex, FailReason);
				}
			}
			else if (TargetExport.bIsPublic)
			{
				FailReason.Appendf(TEXT("Export '%s' exists in the source package")
					TEXT(", but is not a public source export."),
					*TargetExport.ObjectName.ToString());
			}
		}

		if (FailReason.Len() > 0)
		{
			UE_LOG(LogPackageStoreOptimizer, Warning,
				TEXT("Redirection target package '%s' (0x%llX) for '%s' (0x%llX) - %s"),
				*TargetPackage.Name.ToString(),
				TargetPackage.Id.ValueForDebugging(),
				*SourcePackage->Name.ToString(),
				SourcePackage->Id.ValueForDebugging(),
				*FailReason);
			bSuccess = false;
		}
	}

	return bSuccess;
}

void FPackageStoreOptimizer::ProcessRedirects(const TMap<FPackageId, FPackageStorePackage*>& PackagesMap, bool bIsBuildingDLC) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessRedirects);

	for (const auto& KV : PackagesMap)
	{
		FPackageStorePackage* Package = KV.Value;
		if (Package->SourceName.IsNone() || Package->Name == Package->SourceName)
		{
			continue;
		}
		
		FPackageId SourcePackageId = FPackageId::FromName(Package->SourceName);
		FPackageStorePackage* SourcePackage = PackagesMap.FindRef(SourcePackageId);
		Package->bIsRedirected = VerifyRedirect(SourcePackage, *Package, bIsBuildingDLC);
		
		if (Package->bIsRedirected)
		{
			UE_LOG(LogPackageStoreOptimizer, Verbose, TEXT("Adding package redirect from '%s' (0x%llX) to '%s' (0x%llX)."),
				*Package->SourceName.ToString(),
				SourcePackageId.ValueForDebugging(),
				*Package->Name.ToString(),
				Package->Id.ValueForDebugging());
		}
		else
		{
			if (Package->Region.Len() > 0 && !SourcePackage)
			{
				// no update or verification required
				UE_LOG(LogPackageStoreOptimizer, Verbose,
					TEXT("For culture '%s': Localized package '%s' (0x%llX) is unique and does not override a source package."),
					*Package->Region,
					*Package->Name.ToString(),
					Package->Id.ValueForDebugging());
			}
			else
			{
				UE_LOG(LogPackageStoreOptimizer, Display,
					TEXT("Skipping package redirect from '%s' (0x%llX) to '%s' (0x%llX) due to mismatching public exports."),
					*Package->SourceName.ToString(),
					SourcePackageId.ValueForDebugging(),
					*Package->Name.ToString(),
					Package->Id.ValueForDebugging());
			}
		}
	}
}

void FPackageStoreOptimizer::SerializeGraphData(const TArray<FPackageId>& ImportedPackageIds, FPackageStorePackage::FGraphData& GraphData, FBufferWriter& GraphArchive) const
{
	uint32 ExportBundleEntryIndex = 0;
	for (const FPackageStorePackage::FExportBundle& ExportBundle : GraphData.ExportBundles)
	{
		const uint32 EntryCount = ExportBundle.Entries.Num();
		FExportBundleHeader ExportBundleHeader{ ExportBundle.SerialOffset, ExportBundleEntryIndex, EntryCount };
		GraphArchive << ExportBundleHeader;
		ExportBundleEntryIndex += EntryCount;
	}
	Algo::Sort(GraphData.InternalArcs, [](const FPackageStorePackage::FInternalArc& A, const FPackageStorePackage::FInternalArc& B)
	{
		if (A.ToExportBundleIndex == B.ToExportBundleIndex)
		{
			return A.FromExportBundleIndex < B.FromExportBundleIndex;
		}
		return A.ToExportBundleIndex < B.ToExportBundleIndex;
	});
	int32 InternalArcsCount = GraphData.InternalArcs.Num();
	GraphArchive << InternalArcsCount;
	for (FPackageStorePackage::FInternalArc& InternalArc : GraphData.InternalArcs)
	{
		GraphArchive << InternalArc.FromExportBundleIndex;
		GraphArchive << InternalArc.ToExportBundleIndex;
	}

	for (FPackageId ImportedPackageId : ImportedPackageIds)
	{
		TArray<FPackageStorePackage::FExternalArc>* FindArcsFromImportedPackage = GraphData.ExternalArcs.Find(ImportedPackageId);
		if (!FindArcsFromImportedPackage)
		{
			int32 ExternalArcCount = 0;
			GraphArchive << ExternalArcCount;
		}
		else
		{
			Algo::Sort(*FindArcsFromImportedPackage, [](const FPackageStorePackage::FExternalArc& A, const FPackageStorePackage::FExternalArc& B)
			{
				if (A.ToExportBundleIndex == B.ToExportBundleIndex)
				{
					if (A.FromImportIndex == B.FromImportIndex)
					{
						return A.FromCommandType < B.FromCommandType;
					}
					return A.FromImportIndex < B.FromImportIndex;
				}
				return A.ToExportBundleIndex < B.ToExportBundleIndex;
			});
			int32 ExternalArcCount = FindArcsFromImportedPackage->Num();
			GraphArchive << ExternalArcCount;
			for (FPackageStorePackage::FExternalArc& Arc : *FindArcsFromImportedPackage)
			{
				GraphArchive << Arc.FromImportIndex;
				uint8 FromCommandType = uint8(Arc.FromCommandType);
				GraphArchive << FromCommandType;
				GraphArchive << Arc.ToExportBundleIndex;
			}
		}
	}
}

void FPackageStoreOptimizer::FinalizePackageHeader(FPackageStorePackage* Package) const
{
	FBufferWriter ImportedPublicExportHashesArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	for (uint64 ImportedPublicExportHash : Package->ImportedPublicExportHashes)
	{
		ImportedPublicExportHashesArchive << ImportedPublicExportHash;
	}
	Package->ImportedPublicExportHashesSize = ImportedPublicExportHashesArchive.Tell();

	FBufferWriter ImportMapArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	for (FPackageObjectIndex Import : Package->Imports)
	{
		ImportMapArchive << Import;
	}
	Package->ImportMapSize = ImportMapArchive.Tell();

	FBufferWriter ExportMapArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	for (const FPackageStorePackage::FExport& Export : Package->Exports)
	{
		FExportMapEntry ExportMapEntry;
		ExportMapEntry.CookedSerialOffset = Export.CookedSerialOffset;
		ExportMapEntry.CookedSerialSize = Export.SerialSize;
		Package->NameMapBuilder.MarkNameAsReferenced(Export.ObjectName);
		ExportMapEntry.ObjectName = Package->NameMapBuilder.MapName(Export.ObjectName);
		ExportMapEntry.PublicExportHash = Export.PublicExportHash;
		ExportMapEntry.OuterIndex = Export.OuterIndex;
		ExportMapEntry.ClassIndex = Export.ClassIndex;
		ExportMapEntry.SuperIndex = Export.SuperIndex;
		ExportMapEntry.TemplateIndex = Export.TemplateIndex;
		ExportMapEntry.ObjectFlags = Export.ObjectFlags;
		ExportMapEntry.FilterFlags = EExportFilterFlags::None;
		if (Export.bNotForClient)
		{
			ExportMapEntry.FilterFlags = EExportFilterFlags::NotForClient;
		}
		else if (Export.bNotForServer)
		{
			ExportMapEntry.FilterFlags = EExportFilterFlags::NotForServer;
		}

		ExportMapArchive << ExportMapEntry;
	}
	Package->ExportMapSize = ExportMapArchive.Tell();

	FBufferWriter ExportBundleEntriesArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	for (const FPackageStorePackage::FExportBundle& ExportBundle : Package->GraphData.ExportBundles)
	{
		for (FExportBundleEntry BundleEntry : ExportBundle.Entries)
		{
			ExportBundleEntriesArchive << BundleEntry;
		}
	}
	Package->ExportBundleEntriesSize = ExportBundleEntriesArchive.Tell();

	FBufferWriter GraphArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	SerializeGraphData(Package->ImportedPackageIds, Package->GraphData, GraphArchive);
	Package->GraphDataSize = GraphArchive.Tell();

	Package->NameMapBuilder.MarkNameAsReferenced(Package->Name);
	FMappedName MappedPackageName = Package->NameMapBuilder.MapName(Package->Name);

	FBufferWriter NameMapArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	SaveNameBatch(Package->NameMapBuilder.GetNameMap(), NameMapArchive);
	Package->NameMapSize = NameMapArchive.Tell();

	FBufferWriter VersioningInfoArchive(nullptr, 0, EBufferWriterFlags::AllowResize | EBufferWriterFlags::TakeOwnership);
	if (Package->VersioningInfo.IsSet())
	{
		VersioningInfoArchive << Package->VersioningInfo.GetValue();
		Package->VersioningInfoSize = VersioningInfoArchive.Tell();
	}

	Package->HeaderSize =
		sizeof(FZenPackageSummary)
		+ Package->VersioningInfoSize
		+ Package->NameMapSize
		+ Package->ImportedPublicExportHashesSize
		+ Package->ImportMapSize
		+ Package->ExportMapSize
		+ Package->ExportBundleEntriesSize
		+ Package->GraphDataSize;

	Package->HeaderBuffer = FIoBuffer(Package->HeaderSize);
	uint8* HeaderData = Package->HeaderBuffer.Data();
	FMemory::Memzero(HeaderData, Package->HeaderSize);
	FZenPackageSummary* PackageSummary = reinterpret_cast<FZenPackageSummary*>(HeaderData);
	PackageSummary->HeaderSize = Package->HeaderSize;
	PackageSummary->Name = MappedPackageName;
	PackageSummary->PackageFlags = Package->PackageFlags;
	PackageSummary->CookedHeaderSize = Package->CookedHeaderSize;
	FBufferWriter HeaderArchive(HeaderData, Package->HeaderSize);
	HeaderArchive.Seek(sizeof(FZenPackageSummary));

	if (Package->VersioningInfo.IsSet())
	{
		PackageSummary->bHasVersioningInfo = 1;
		HeaderArchive.Serialize(VersioningInfoArchive.GetWriterData(), VersioningInfoArchive.Tell());
	}
	else
	{
		PackageSummary->bHasVersioningInfo = 0;
	}

	HeaderArchive.Serialize(NameMapArchive.GetWriterData(), NameMapArchive.Tell());
	PackageSummary->ImportedPublicExportHashesOffset = HeaderArchive.Tell();
	HeaderArchive.Serialize(ImportedPublicExportHashesArchive.GetWriterData(), ImportedPublicExportHashesArchive.Tell());
	PackageSummary->ImportMapOffset = HeaderArchive.Tell();
	HeaderArchive.Serialize(ImportMapArchive.GetWriterData(), ImportMapArchive.Tell());
	PackageSummary->ExportMapOffset = HeaderArchive.Tell();
	HeaderArchive.Serialize(ExportMapArchive.GetWriterData(), ExportMapArchive.Tell());
	PackageSummary->ExportBundleEntriesOffset = HeaderArchive.Tell();
	HeaderArchive.Serialize(ExportBundleEntriesArchive.GetWriterData(), ExportBundleEntriesArchive.Tell());
	PackageSummary->GraphDataOffset = HeaderArchive.Tell();
	HeaderArchive.Serialize(GraphArchive.GetWriterData(), GraphArchive.Tell());
	check(HeaderArchive.Tell() == PackageSummary->HeaderSize)
}

void FPackageStoreOptimizer::FinalizePackage(FPackageStorePackage* Package)
{
	++TotalPackageCount;
	TotalExportBundleCount += Package->GraphData.ExportBundles.Num();
	uint64 PackageExportBundleEntryCount = 0;
	for (const FPackageStorePackage::FExportBundle& ExportBundle : Package->GraphData.ExportBundles)
	{
		PackageExportBundleEntryCount += ExportBundle.Entries.Num();
	}
	check(PackageExportBundleEntryCount == FExportBundleEntry::ExportCommandType_Count * Package->Exports.Num());
	TotalExportBundleEntryCount += PackageExportBundleEntryCount;

	for (FPackageStorePackage::FExportGraphNode& Node : Package->ExportGraphNodes)
	{
		check(Node.ExportBundleIndex >= 0);
		TSet<FPackageStorePackage::FInternalArc> InternalArcsSet;
		for (FPackageStorePackage::FExportGraphNode* InternalDependency : Node.InternalDependencies)
		{
			FPackageStorePackage::FInternalArc Arc;
			check(InternalDependency->ExportBundleIndex >= 0);
			Arc.FromExportBundleIndex = InternalDependency->ExportBundleIndex;
			Arc.ToExportBundleIndex = Node.ExportBundleIndex;
			if (Arc.FromExportBundleIndex != Arc.ToExportBundleIndex)
			{
				bool bIsAlreadyInSet = false;
				InternalArcsSet.Add(Arc, &bIsAlreadyInSet);
				if (!bIsAlreadyInSet)
				{
					Package->GraphData.InternalArcs.Add(Arc);
					++TotalInternalBundleArcsCount;
				}
			}
		}

		for (FPackageStorePackage::FExternalDependency& ExternalDependency : Node.ExternalDependencies)
		{
			check(ExternalDependency.ImportIndex >= 0);
			const FPackageObjectIndex& Import = Package->Imports[ExternalDependency.ImportIndex];
			check(Import.IsPackageImport());
			FPackageImportReference PackageImportRef = Import.ToPackageImportRef();
			TArray<FPackageStorePackage::FExternalArc>& ArcsFromImportedPackage = Package->GraphData.ExternalArcs.FindOrAdd(Package->ImportedPackageIds[PackageImportRef.GetImportedPackageIndex()]);
			FPackageStorePackage::FExternalArc Arc;
			Arc.FromImportIndex = ExternalDependency.ImportIndex;
			Arc.FromCommandType = ExternalDependency.ExportBundleCommandType;
			Arc.ToExportBundleIndex = Node.ExportBundleIndex;
			if (!ArcsFromImportedPackage.Contains(Arc))
			{
				ArcsFromImportedPackage.Add(Arc);
				++TotalExternalBundleArcsCount;
			}
		}
	}
	uint64 SerialOffset = 0;
	for (FPackageStorePackage::FExportBundle& ExportBundle : Package->GraphData.ExportBundles)
	{
		ExportBundle.SerialOffset = SerialOffset;
		for (const FExportBundleEntry& BundleEntry : ExportBundle.Entries)
		{
			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				const FPackageStorePackage::FExport& Export = Package->Exports[BundleEntry.LocalExportIndex];
				SerialOffset += Export.SerialSize;
			}
		}
	}

	FinalizePackageHeader(Package);
}

FIoBuffer FPackageStoreOptimizer::CreatePackageBuffer(const FPackageStorePackage* Package, const FIoBuffer& CookedExportsDataBuffer, TArray<FFileRegion>* InOutFileRegions) const
{
	check(Package->HeaderBuffer.DataSize() > 0);
	check(Package->HeaderBuffer.DataSize() == Package->HeaderSize);
	const uint64 BundleBufferSize = Package->HeaderSize + Package->ExportsSerialSize;
	FIoBuffer BundleBuffer(BundleBufferSize);
	FMemory::Memcpy(BundleBuffer.Data(), Package->HeaderBuffer.Data(), Package->HeaderSize);
	uint64 BundleBufferOffset = Package->HeaderSize;

	TArray<FFileRegion> OutputRegions;

	for (const FPackageStorePackage::FExportBundle& ExportBundle : Package->GraphData.ExportBundles)
	{
		for (const FExportBundleEntry& BundleEntry : ExportBundle.Entries)
		{
			if (BundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Serialize)
			{
				const FPackageStorePackage::FExport& Export = Package->Exports[BundleEntry.LocalExportIndex];
				check(Export.SerialOffset + Export.SerialSize <= CookedExportsDataBuffer.DataSize());
				FMemory::Memcpy(BundleBuffer.Data() + BundleBufferOffset, CookedExportsDataBuffer.Data() + Export.SerialOffset, Export.SerialSize);

				if (InOutFileRegions)
				{
					// Find overlapping regions and adjust them to match the new offset of the export data
					for (const FFileRegion& Region : *InOutFileRegions)
					{
						if (Export.SerialOffset <= Region.Offset && Region.Offset + Region.Length <= Export.SerialOffset + Export.SerialSize)
						{
							FFileRegion NewRegion = Region;
							NewRegion.Offset -= Export.SerialOffset;
							NewRegion.Offset += BundleBufferOffset;
							OutputRegions.Add(NewRegion);
						}
					}
				}
				BundleBufferOffset += Export.SerialSize;
			}
		}
	}
	check(BundleBufferOffset == BundleBuffer.DataSize());

	if (InOutFileRegions)
	{
		*InOutFileRegions = OutputRegions;
	}

	return BundleBuffer;
}

void FPackageStoreOptimizer::FindScriptObjectsRecursive(FPackageObjectIndex OuterIndex, UObject* Object)
{
	if (!Object->HasAllFlags(RF_Public))
	{
		UE_LOG(LogPackageStoreOptimizer, Verbose, TEXT("Skipping script object: %s (!RF_Public)"), *Object->GetFullName());
		return;
	}

	FString OuterFullName;
	FPackageObjectIndex OuterCDOClassIndex;
	{
		const FScriptObjectData* Outer = ScriptObjectsMap.Find(OuterIndex);
		check(Outer);
		OuterFullName = Outer->FullName;
		OuterCDOClassIndex = Outer->CDOClassIndex;
	}

	FName ObjectName = Object->GetFName();

	FString TempFullName = OuterFullName;
	TempFullName.AppendChar(TEXT('/'));
	ObjectName.AppendString(TempFullName);

	TempFullName.ToLowerInline();
	FPackageObjectIndex GlobalImportIndex = FPackageObjectIndex::FromScriptPath(TempFullName);

	FScriptObjectData* ScriptImport = ScriptObjectsMap.Find(GlobalImportIndex);
	if (ScriptImport)
	{
		UE_LOG(LogPackageStoreOptimizer, Fatal, TEXT("Import name hash collision \"%s\" and \"%s"), *TempFullName, *ScriptImport->FullName);
	}

	FPackageObjectIndex CDOClassIndex = OuterCDOClassIndex;
	if (CDOClassIndex.IsNull())
	{
		TCHAR NameBuffer[FName::StringBufferSize];
		uint32 Len = ObjectName.ToString(NameBuffer);
		if (FCString::Strncmp(NameBuffer, TEXT("Default__"), 9) == 0)
		{
			FString CDOClassFullName = OuterFullName;
			CDOClassFullName.AppendChar(TEXT('/'));
			CDOClassFullName.AppendChars(NameBuffer + 9, Len - 9);
			CDOClassFullName.ToLowerInline();

			CDOClassIndex = FPackageObjectIndex::FromScriptPath(CDOClassFullName);
		}
	}

	ScriptImport = &ScriptObjectsMap.Add(GlobalImportIndex);
	ScriptImport->GlobalIndex = GlobalImportIndex;
	ScriptImport->FullName = MoveTemp(TempFullName);
	ScriptImport->OuterIndex = OuterIndex;
	ScriptImport->ObjectName = ObjectName;
	ScriptImport->CDOClassIndex = CDOClassIndex;

	TArray<UObject*> InnerObjects;
	GetObjectsWithOuter(Object, InnerObjects, /*bIncludeNestedObjects*/false);
	for (UObject* InnerObject : InnerObjects)
	{
		FindScriptObjectsRecursive(GlobalImportIndex, InnerObject);
	}
};

void FPackageStoreOptimizer::FindScriptObjects()
{
	TArray<UPackage*> ScriptPackages;
	FindAllRuntimeScriptPackages(ScriptPackages);

	TArray<UObject*> InnerObjects;
	for (UPackage* Package : ScriptPackages)
	{
		FName ObjectName = Package->GetFName();
		FString FullName = Package->GetName();

		FullName.ToLowerInline();
		FPackageObjectIndex GlobalImportIndex = FPackageObjectIndex::FromScriptPath(FullName);

		FScriptObjectData* ScriptImport = ScriptObjectsMap.Find(GlobalImportIndex);
		checkf(!ScriptImport, TEXT("Import name hash collision \"%s\" and \"%s"), *FullName, *ScriptImport->FullName);

		ScriptImport = &ScriptObjectsMap.Add(GlobalImportIndex);
		ScriptImport->GlobalIndex = GlobalImportIndex;
		ScriptImport->FullName = FullName;
		ScriptImport->OuterIndex = FPackageObjectIndex();
		ScriptImport->ObjectName = ObjectName;

		InnerObjects.Reset();
		GetObjectsWithOuter(Package, InnerObjects, /*bIncludeNestedObjects*/false);
		for (UObject* InnerObject : InnerObjects)
		{
			FindScriptObjectsRecursive(GlobalImportIndex, InnerObject);
		}
	}

	TotalScriptObjectCount = ScriptObjectsMap.Num();
}

FIoBuffer FPackageStoreOptimizer::CreateScriptObjectsBuffer() const
{
	TArray<FScriptObjectData> ScriptObjectsAsArray;
	ScriptObjectsMap.GenerateValueArray(ScriptObjectsAsArray);
	Algo::Sort(ScriptObjectsAsArray, [](const FScriptObjectData& A, const FScriptObjectData& B)
	{
		return A.FullName < B.FullName;
	});
	
	TArray<FScriptObjectEntry> ScriptObjectEntries;
	ScriptObjectEntries.Reserve(ScriptObjectsAsArray.Num());
	FPackageStoreNameMapBuilder NameMapBuilder;
	NameMapBuilder.SetNameMapType(FMappedName::EType::Global);
	for (const FScriptObjectData& ImportData : ScriptObjectsAsArray)
	{
		NameMapBuilder.MarkNameAsReferenced(ImportData.ObjectName);
		FScriptObjectEntry& Entry = ScriptObjectEntries.AddDefaulted_GetRef();
		Entry.Mapped = NameMapBuilder.MapName(ImportData.ObjectName);
		Entry.GlobalIndex = ImportData.GlobalIndex;
		Entry.OuterIndex = ImportData.OuterIndex;
		Entry.CDOClassIndex = ImportData.CDOClassIndex;
	}

	FLargeMemoryWriter ScriptObjectsArchive(0, true);
	SaveNameBatch(NameMapBuilder.GetNameMap(), ScriptObjectsArchive);
	int32 NumScriptObjects = ScriptObjectEntries.Num();
	ScriptObjectsArchive << NumScriptObjects;
	for (FScriptObjectEntry& Entry : ScriptObjectEntries)
	{
		ScriptObjectsArchive << Entry;
	}

	int64 DataSize = ScriptObjectsArchive.TotalSize();
	return FIoBuffer(FIoBuffer::AssumeOwnership, ScriptObjectsArchive.ReleaseOwnership(), DataSize);
}

void FPackageStoreOptimizer::LoadScriptObjectsBuffer(const FIoBuffer& ScriptObjectsBuffer)
{
	FLargeMemoryReader ScriptObjectsArchive(ScriptObjectsBuffer.Data(), ScriptObjectsBuffer.DataSize());
	TArray<FDisplayNameEntryId> NameMap = LoadNameBatch(ScriptObjectsArchive);
	int32 NumScriptObjects;
	ScriptObjectsArchive << NumScriptObjects;
	for (int32 Index = 0; Index < NumScriptObjects; ++Index)
	{
		FScriptObjectEntry Entry{};
		ScriptObjectsArchive << Entry;
		FScriptObjectData& ImportData = ScriptObjectsMap.Add(Entry.GlobalIndex);
		FMappedName MappedName = Entry.Mapped;
		ImportData.ObjectName = NameMap[MappedName.GetIndex()].ToName(MappedName.GetNumber());
		ImportData.GlobalIndex = Entry.GlobalIndex;
		ImportData.OuterIndex = Entry.OuterIndex;
		ImportData.CDOClassIndex = Entry.CDOClassIndex;
	}
}

FPackageStoreEntryResource FPackageStoreOptimizer::CreatePackageStoreEntry(const FPackageStorePackage* Package, const FPackageStorePackage* OptionalSegmentPackage) const
{
	FPackageStoreEntryResource Result;
	Result.Flags = Package->bIsRedirected ? EPackageStoreEntryFlags::Redirected : EPackageStoreEntryFlags::None;

	if (OptionalSegmentPackage && OptionalSegmentPackage->HasEditorData())
	{
		// AutoOptional packages are saved with editor data included
		Result.Flags |= EPackageStoreEntryFlags::AutoOptional;
	}
	
	Result.PackageName = Package->Name;
	Result.SourcePackageName = Package->SourceName;
	Result.Region = FName(*Package->Region);
	Result.ExportInfo.ExportCount = Package->Exports.Num();
	Result.ExportInfo.ExportBundleCount = Package->GraphData.ExportBundles.Num();
	Result.ImportedPackageIds = Package->ImportedPackageIds;
	Result.ShaderMapHashes = Package->ShaderMapHashes.Array();
	// Package->ShaderMapHashes is a TSet and is unsorted - we want this sorted as it's serialized
	// in to the ContainerHeader, and if not sorted leads to staging non-determinism.
	Algo::Sort(Result.ShaderMapHashes);
	if (OptionalSegmentPackage)
	{
		Result.OptionalSegmentExportInfo.ExportCount = OptionalSegmentPackage->Exports.Num();
		Result.OptionalSegmentExportInfo.ExportBundleCount = OptionalSegmentPackage->GraphData.ExportBundles.Num();
		Result.OptionalSegmentImportedPackageIds = OptionalSegmentPackage->ImportedPackageIds;
	}
	return Result;
}

FIoContainerHeader FPackageStoreOptimizer::CreateContainerHeaderInternal(const FIoContainerId& ContainerId, TArrayView<const FPackageStoreEntryResource> PackageStoreEntries, bool bIsOptional) const
{
	FIoContainerHeader Header;
	Header.ContainerId = ContainerId;
	
	int32 NonOptionalSegmentStoreEntriesCount = 0;
	int32 OptionalSegmentStoreEntriesCount = 0;
	if (bIsOptional)
	{
		for (const FPackageStoreEntryResource& Entry : PackageStoreEntries)
		{
			if (Entry.OptionalSegmentExportInfo.ExportCount)
			{
				if (Entry.IsAutoOptional())
				{
					// Auto optional packages fully replace the non-optional segment
					++NonOptionalSegmentStoreEntriesCount;
				}
				else
				{
					++OptionalSegmentStoreEntriesCount;
				}
			}
		}
	}
	else
	{
		NonOptionalSegmentStoreEntriesCount = PackageStoreEntries.Num();
	}

	struct FStoreEntriesWriter
	{
		const int32 StoreTocSize;
		FLargeMemoryWriter StoreTocArchive = FLargeMemoryWriter(0, true);
		FLargeMemoryWriter StoreDataArchive = FLargeMemoryWriter(0, true);

		void Flush(TArray<uint8>& OutputBuffer)
		{
			check(StoreTocArchive.TotalSize() == StoreTocSize);
			if (StoreTocSize)
			{
				const int32 StoreByteCount = StoreTocArchive.TotalSize() + StoreDataArchive.TotalSize();
				OutputBuffer.AddUninitialized(StoreByteCount);
				FBufferWriter PackageStoreArchive(OutputBuffer.GetData(), StoreByteCount);
				PackageStoreArchive.Serialize(StoreTocArchive.GetData(), StoreTocArchive.TotalSize());
				PackageStoreArchive.Serialize(StoreDataArchive.GetData(), StoreDataArchive.TotalSize());
			}
		}
	};

	FStoreEntriesWriter StoreEntriesWriter
	{
		static_cast<int32>(NonOptionalSegmentStoreEntriesCount * sizeof(FFilePackageStoreEntry))
	};

	FStoreEntriesWriter OptionalSegmentStoreEntriesWriter
	{
		static_cast<int32>(OptionalSegmentStoreEntriesCount * sizeof(FFilePackageStoreEntry))
	};

	auto SerializePackageEntryCArrayHeader = [](FStoreEntriesWriter& Writer, int32 Count)
	{
		const int32 RemainingTocSize = Writer.StoreTocSize - Writer.StoreTocArchive.Tell();
		const int32 OffsetFromThis = RemainingTocSize + Writer.StoreDataArchive.Tell();
		uint32 ArrayNum = Count > 0 ? Count : 0;
		uint32 OffsetToDataFromThis = ArrayNum > 0 ? OffsetFromThis : 0;

		Writer.StoreTocArchive << ArrayNum;
		Writer.StoreTocArchive << OffsetToDataFromThis;
	};

	TArray<const FPackageStoreEntryResource*> SortedPackageStoreEntries;
	SortedPackageStoreEntries.Reserve(PackageStoreEntries.Num());
	for (const FPackageStoreEntryResource& Entry : PackageStoreEntries)
	{
		SortedPackageStoreEntries.Add(&Entry);
	}
	Algo::Sort(SortedPackageStoreEntries, [](const FPackageStoreEntryResource* A, const FPackageStoreEntryResource* B)
	{
		return A->GetPackageId() < B->GetPackageId();
	});

	Header.PackageIds.Reserve(NonOptionalSegmentStoreEntriesCount);
	Header.OptionalSegmentPackageIds.Reserve(OptionalSegmentStoreEntriesCount);
	FPackageStoreNameMapBuilder RedirectsNameMapBuilder;
	RedirectsNameMapBuilder.SetNameMapType(FMappedName::EType::Container);
	TSet<FPackageId> AllLocalizedPackages;
	if (bIsOptional)
	{
		for (const FPackageStoreEntryResource* Entry : SortedPackageStoreEntries)
		{
			if (Entry->OptionalSegmentExportInfo.ExportCount)
			{
				FStoreEntriesWriter* TargetEntriesWriter;
				if (Entry->IsAutoOptional())
				{
					Header.PackageIds.Add(Entry->GetPackageId());
					TargetEntriesWriter = &StoreEntriesWriter;
				}
				else
				{
					Header.OptionalSegmentPackageIds.Add(Entry->GetPackageId());
					TargetEntriesWriter = &OptionalSegmentStoreEntriesWriter;
				}

				// OptionalStoreEntry
				FPackageStoreExportInfo OptionalSegmentExportInfo = Entry->OptionalSegmentExportInfo;
				TargetEntriesWriter->StoreTocArchive << OptionalSegmentExportInfo;

				// OptionalImportedPackages
				const TArray<FPackageId>& OptionalSegmentImportedPackageIds = Entry->OptionalSegmentImportedPackageIds;
				SerializePackageEntryCArrayHeader(*TargetEntriesWriter, OptionalSegmentImportedPackageIds.Num());
				for (FPackageId OptionalSegmentImportedPackageId : OptionalSegmentImportedPackageIds)
				{
					check(OptionalSegmentImportedPackageId.IsValid());
					TargetEntriesWriter->StoreDataArchive << OptionalSegmentImportedPackageId;
				}

				// ShaderMapHashes is N/A for optional segments
				SerializePackageEntryCArrayHeader(*TargetEntriesWriter, 0);
			}
		}
	}
	else
	{
		for (const FPackageStoreEntryResource* Entry : SortedPackageStoreEntries)
		{
			Header.PackageIds.Add(Entry->GetPackageId());
			if (Entry->IsRedirected())
			{
				RedirectsNameMapBuilder.MarkNameAsReferenced(Entry->GetSourcePackageName());
				FMappedName MappedSourcePackageName = RedirectsNameMapBuilder.MapName(Entry->GetSourcePackageName());
				if (!Entry->Region.IsNone())
				{
					if (!AllLocalizedPackages.Contains(Entry->GetSourcePackageId()))
					{
						Header.LocalizedPackages.Add({ Entry->GetSourcePackageId(), MappedSourcePackageName });
						AllLocalizedPackages.Add(Entry->GetSourcePackageId());
					}
				}
				else
				{
					Header.PackageRedirects.Add({ Entry->GetSourcePackageId(), Entry->GetPackageId(), MappedSourcePackageName });
				}
			}

			// StoreEntries
			FPackageStoreExportInfo ExportInfo = Entry->ExportInfo;
			StoreEntriesWriter.StoreTocArchive << ExportInfo;

			// ImportedPackages
			const TArray<FPackageId>& ImportedPackageIds = Entry->ImportedPackageIds;
			SerializePackageEntryCArrayHeader(StoreEntriesWriter, ImportedPackageIds.Num());
			for (FPackageId ImportedPackageId : ImportedPackageIds)
			{
				check(ImportedPackageId.IsValid());
				StoreEntriesWriter.StoreDataArchive << ImportedPackageId;
			}

			// ShaderMapHashes
			const TArray<FSHAHash>& ShaderMapHashes = Entry->ShaderMapHashes;
			SerializePackageEntryCArrayHeader(StoreEntriesWriter, ShaderMapHashes.Num());
			for (const FSHAHash& ShaderMapHash : ShaderMapHashes)
			{
				StoreEntriesWriter.StoreDataArchive << const_cast<FSHAHash&>(ShaderMapHash);
			}
		}
	}
	Header.RedirectsNameMap = RedirectsNameMapBuilder.GetNameMap();

	StoreEntriesWriter.Flush(Header.StoreEntries);
	OptionalSegmentStoreEntriesWriter.Flush(Header.OptionalSegmentStoreEntries);

	return Header;
}

FIoContainerHeader FPackageStoreOptimizer::CreateContainerHeader(const FIoContainerId& ContainerId, TArrayView<const FPackageStoreEntryResource> PackageStoreEntries) const
{
	return CreateContainerHeaderInternal(ContainerId, PackageStoreEntries, false);
}

FIoContainerHeader FPackageStoreOptimizer::CreateOptionalContainerHeader(const FIoContainerId& ContainerId, TArrayView<const FPackageStoreEntryResource> PackageStoreEntries) const
{
	return CreateContainerHeaderInternal(ContainerId, PackageStoreEntries, true);
}