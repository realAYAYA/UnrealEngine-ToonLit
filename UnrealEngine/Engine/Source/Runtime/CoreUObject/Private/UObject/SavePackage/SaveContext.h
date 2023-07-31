// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Serialization/StructuredArchive.h"
#include "Serialization/UnversionedPropertySerialization.h"
#include "Templates/PimplPtr.h"
#include "Templates/UniquePtr.h"
#include "UObject/LinkerSave.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/SavePackage/SavePackageUtilities.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectThreadContext.h"

/**
 * Wraps an object tagged as export along with some of its harvested settings
 */
struct FTaggedExport
{
	UObject* Obj;
	uint32 bNotAlwaysLoadedForEditorGame : 1;
	/**
	 * Indicate that this export should have a public hash even if it isn't marked as RF_Public
	 * This will artificially mark the object RF_Public in the linker tables so the iostore generates the public hash
	 */
	uint32 bGeneratePublicHash : 1;
	/**
	 * Indicate if the object that directly referenced this export was optional
	 * Used to determine mandatory objects in the game save realm
	 */
	uint32 bFromOptionalReference : 1;

	FTaggedExport()
		: Obj(nullptr)
		, bNotAlwaysLoadedForEditorGame(false)
		, bGeneratePublicHash(false)
		, bFromOptionalReference(false)
	{}

	FTaggedExport(UObject* InObj, bool bInNotAlwaysLoadedForEditorGame = true, bool bInFromOptionalReference = false)
		: Obj(InObj)
		, bNotAlwaysLoadedForEditorGame(bInNotAlwaysLoadedForEditorGame)
		, bGeneratePublicHash(false)
		, bFromOptionalReference(bInFromOptionalReference)
	{}

	inline bool operator == (const FTaggedExport& Other) const
	{
		return Obj == Other.Obj;
	}
};

inline uint32 GetTypeHash(const FTaggedExport& Export)
{
	return GetTypeHash(Export.Obj);
}

/** 
 * Available save realm during save package harvesting 
 * A realm is the set of objects gathered and referenced for a particular domain/context
 */
enum class ESaveRealm : uint32
{
	Game		= 0,
	Optional,
	Editor,
	RealmCount,
	None		= RealmCount
};

/** Reason for harvested illegal reference */
enum class EIllegalRefReason : uint8
{
	None = 0,
	ReferenceToOptional,
	ReferenceFromOptionalToMissingGameExport,
	UnsaveableClass,
	UnsaveableOuter,
};

/** Small struct to store illegal references harvested during save */
struct FIllegalReference
{
	UObject* From = nullptr;
	UObject* To = nullptr;
	EIllegalRefReason Reason;
	FString FormatStringArg;
};

/** Hold the harvested exports and imports for a realm */
struct FHarvestedRealm
{

	~FHarvestedRealm()
	{
		CloseLinkerArchives();

		if (TempFilename.IsSet())
		{
			IFileManager::Get().Delete(*TempFilename.GetValue());
		}
		if (TextFormatTempFilename.IsSet())
		{
			IFileManager::Get().Delete(*TextFormatTempFilename.GetValue());
		}
	}

	void AddImport(UObject* InObject)
	{
		Imports.Add(InObject);
	}

	void AddExport(FTaggedExport InTagObj)
	{
		Exports.Add(MoveTemp(InTagObj));
	}

	void AddExcluded(UObject* InObject)
	{
		Excluded.Add(InObject);
	}

	bool IsImport(UObject* InObject) const
	{
		return Imports.Contains(InObject);
	}

	bool IsExport(UObject* InObject) const
	{
		return Exports.Contains(InObject);
	}

	bool IsIncluded(UObject* InObject) const
	{
		return IsImport(InObject) || IsExport(InObject);
	}

	bool IsExcluded(UObject* InObject) const
	{
		return Excluded.Contains(InObject);
	}

	TSet<FTaggedExport>& GetExports()
	{
		return Exports;
	}

	const TSet<UObject*>& GetImports() const
	{
		return Imports;
	}

	const TSet<FName>& GetSoftPackageReferenceList() const
	{
		return SoftPackageReferenceList;
	}

	TSet<FName>& GetSoftPackageReferenceList()
	{
		return SoftPackageReferenceList;
	}

	const TMap<UObject*, TArray<FName>>& GetSearchableNamesObjectMap() const
	{
		return SearchableNamesObjectMap;
	}

	TMap<UObject*, TArray<FName>>& GetSearchableNamesObjectMap()
	{
		return SearchableNamesObjectMap;
	}

	const TSet<FNameEntryId>& GetNamesReferencedFromExportData() const
	{
		return NamesReferencedFromExportData;
	}

	TSet<FNameEntryId>& GetNamesReferencedFromExportData()
	{
		return NamesReferencedFromExportData;
	}

	const TSet<FNameEntryId>& GetNamesReferencedFromPackageHeader() const
	{
		return NamesReferencedFromPackageHeader;
	}

	TSet<FNameEntryId>& GetNamesReferencedFromPackageHeader()
	{
		return NamesReferencedFromPackageHeader;
	}

	const TSet<FSoftObjectPath>& GetSoftObjectPathList() const
	{
		return SoftObjectPathList;
	}

	TSet<FSoftObjectPath>& GetSoftObjectPathList() 
	{
		return SoftObjectPathList;
	}

	const TMap<UObject*, TSet<UObject*>>& GetObjectDependencies() const
	{
		return ExportObjectDependencies;
	}

	TMap<UObject*, TSet<UObject*>>& GetObjectDependencies()
	{
		return ExportObjectDependencies;
	}

	const TMap<UObject*, TSet<UObject*>>& GetNativeObjectDependencies() const
	{
		return ExportNativeObjectDependencies;
	}

	TMap<UObject*, TSet<UObject*>>& GetNativeObjectDependencies()
	{
		return ExportNativeObjectDependencies;
	}

	bool NameExists(FNameEntryId ComparisonId) const
	{
		for (FNameEntryId DisplayId : NamesReferencedFromExportData)
		{
			if (FName::GetComparisonIdFromDisplayId(DisplayId) == ComparisonId)
			{
				return true;
			}
		}
		for (FNameEntryId DisplayId : NamesReferencedFromPackageHeader)
		{
			if (FName::GetComparisonIdFromDisplayId(DisplayId) == ComparisonId)
			{
				return true;
			}
		}
		return false;
	}

	FLinkerSave* GetLinker() const
	{
		return Linker.Get();
	}

	void SetLinker(TPimplPtr<FLinkerSave> InLinker)
	{
		Linker = MoveTemp(InLinker);
	}

	bool CloseLinkerArchives()
	{
		bool bSuccess = true;
		if (Linker)
		{
			bSuccess = Linker->CloseAndDestroySaver();
		}
		StructuredArchive.Reset();
		Formatter.Reset();
		TextFormatArchive.Reset();
		return bSuccess;
	}

	FArchive* GetTextFormatArchive() const
	{
		return TextFormatArchive.Get();
	}

	void SetTextFormatArchive(TUniquePtr<FArchive> InTextArchive)
	{
		TextFormatArchive = MoveTemp(InTextArchive);
	}

	FArchiveFormatterType* GetFormatter() const
	{
		return Formatter.Get();
	}

	void SetFormatter(TUniquePtr<FArchiveFormatterType> InFormatter)
	{
		Formatter = MoveTemp(InFormatter);
	}

	FStructuredArchive* GetStructuredArchive() const
	{
		return StructuredArchive.Get();
	}

	void SetStructuredArchive(TUniquePtr<FStructuredArchive> InArchive)
	{
		StructuredArchive = MoveTemp(InArchive);
	}

	const TOptional<FString>& GetTempFilename() const
	{
		return TempFilename;
	}

	void SetTempFilename(TOptional<FString> InTemp)
	{
		TempFilename = MoveTemp(InTemp);
	}

	const TOptional<FString>& GetTextFormatTempFilename() const
	{
		return TextFormatTempFilename;
	}

	void SetTextFormatTempFilename(TOptional<FString> InTemp)
	{
		TextFormatTempFilename = MoveTemp(InTemp);
	}

private:
	friend class FSaveContext;

	/** Linker associated with this realm. */
	TPimplPtr<FLinkerSave> Linker;

	/** Archives associated with this linker and realm. */
	TUniquePtr<FArchive> TextFormatArchive;
	TUniquePtr<FArchiveFormatterType> Formatter;
	TUniquePtr<FStructuredArchive> StructuredArchive;

	/** Temp Filename for the archive. */
	TOptional<FString> TempFilename;
	TOptional<FString> TextFormatTempFilename;

	// Set of objects excluded (import or exports) through marks or otherwise (i.e. transient flags, etc)
	TSet<UObject*> Excluded;
	// Set of objects marked as export
	TSet<FTaggedExport> Exports;
	// Set of objects marked as import
	TSet<UObject*> Imports;
	// Set of names referenced from export serialization
	TSet<FNameEntryId> NamesReferencedFromExportData;
	// Set of names referenced from the package header (import and export table object names etc)
	TSet<FNameEntryId> NamesReferencedFromPackageHeader;
	// Set of SoftObjectPath harvested in this realm
	TSet<FSoftObjectPath> SoftObjectPathList;
	// List of soft package reference found
	TSet<FName> SoftPackageReferenceList;
	// Map of objects to their list of searchable names
	TMap<UObject*, TArray<FName>> SearchableNamesObjectMap;
	// Map of objects to their dependencies
	TMap<UObject*, TSet<UObject*>> ExportObjectDependencies;
	// Map of objects to their native dependencies
	TMap<UObject*, TSet<UObject*>> ExportNativeObjectDependencies;
};


/**
 * Helper class that encapsulate the full necessary context and intermediate result to save a package
 */
class FSaveContext
{
public:
	struct FSetSaveRealmToSaveScope
	{
		FSetSaveRealmToSaveScope(FSaveContext& InContext, ESaveRealm InHarvestingRealm)
			: Context(InContext)
			, PreviousHarvestingRealm(InContext.CurrentHarvestingRealm)
		{
			Context.CurrentHarvestingRealm = InHarvestingRealm;
		}

		~FSetSaveRealmToSaveScope()
		{
			Context.CurrentHarvestingRealm = PreviousHarvestingRealm;
		}

	private:
		FSaveContext& Context;
		ESaveRealm PreviousHarvestingRealm;
	};

public:
	FSaveContext(UPackage* InPackage, UObject* InAsset, const TCHAR* InFilename, const FSavePackageArgs& InSaveArgs, FUObjectSerializeContext* InSerializeContext = nullptr)
		: Package(InPackage)
		, Asset(InAsset)
		, Filename(InFilename)
		, SaveArgs(InSaveArgs)
		, PackageWriter(InSaveArgs.SavePackageContext ? InSaveArgs.SavePackageContext->PackageWriter : nullptr)
		, SerializeContext(InSerializeContext)
		, ExcludedObjectMarks(SavePackageUtilities::GetExcludedObjectMarksForTargetPlatform(SaveArgs.GetTargetPlatform()))
	{
		// Assumptions & checks
		check(InPackage);
		check(InFilename);
		// if we are cooking we should be doing it in the editor and with a CookedPackageWriter
		check(!IsCooking() || WITH_EDITOR);
		checkf(!IsCooking() || (PackageWriter && PackageWriter->AsCookedPackageWriter()), TEXT("Cook saves require an ICookedPackageWriter"));

		SaveArgs.TopLevelFlags = UE::SavePackageUtilities::NormalizeTopLevelFlags(SaveArgs.TopLevelFlags, IsCooking());
		if (PackageWriter)
		{
			bIgnoreHeaderDiffs = SaveArgs.SavePackageContext->PackageWriterCapabilities.bIgnoreHeaderDiffs;
		}

		// if the asset wasn't provided, fetch it from the package
		if (Asset == nullptr)
		{
			Asset = InPackage->FindAssetInPackage();
		}

		TargetPackagePath = FPackagePath::FromLocalPath(InFilename);
		if (TargetPackagePath.GetHeaderExtension() == EPackageExtension::Unspecified)
		{
			TargetPackagePath.SetHeaderExtension(EPackageExtension::EmptyString);
		}

		bCanUseUnversionedPropertySerialization = CanUseUnversionedPropertySerialization(SaveArgs.GetTargetPlatform());
		bTextFormat = FString(Filename).EndsWith(FPackageName::GetTextAssetPackageExtension()) || FString(Filename).EndsWith(FPackageName::GetTextMapPackageExtension());
		static const IConsoleVariable* ProcessPrestreamingRequests = IConsoleManager::Get().FindConsoleVariable(TEXT("s.ProcessPrestreamingRequests"));
		if (ProcessPrestreamingRequests)
		{
			bIsProcessingPrestreamPackages = ProcessPrestreamingRequests->GetInt() > 0;
		}
		static const IConsoleVariable* FixupStandaloneFlags = IConsoleManager::Get().FindConsoleVariable(TEXT("save.FixupStandaloneFlags"));
		if (FixupStandaloneFlags)
		{
			bIsFixupStandaloneFlags = FixupStandaloneFlags->GetInt() != 0;
		}

		ObjectSaveContext.Set(InPackage, GetTargetPlatform(), TargetPackagePath, SaveArgs.SaveFlags);

		// Setup the harvesting flags and generate the context for harvesting the package
		SetupHarvestingRealms();
	} 

	~FSaveContext()
	{
		if (bPostSaveRootRequired && Asset)
		{
			UE::SavePackageUtilities::CallPostSaveRoot(Asset, ObjectSaveContext, bNeedPreSaveCleanup);
		}
	}

	const FSavePackageArgs& GetSaveArgs() const
	{
		return SaveArgs;
	}

	FArchiveCookData* GetCookData()
	{
		return SaveArgs.ArchiveCookData;
	}

	const ITargetPlatform* GetTargetPlatform() const
	{
		return SaveArgs.GetTargetPlatform();
	}

	UPackage* GetPackage() const
	{
		return Package;
	}

	UObject* GetAsset() const
	{
		return Asset;
	}

	const TCHAR* GetFilename() const
	{
		return Filename;
	}

	const FPackagePath& GetTargetPackagePath() const
	{
		return TargetPackagePath;
	}

	EObjectMark GetExcludedObjectMarks(ESaveRealm HarvestingRealm) const
	{
		// When considering excluded objects for a platform, do not consider editor only object and not for platform objects in the optional context as excluded
		return (HarvestingRealm == ESaveRealm::Optional) ? (EObjectMark)(ExcludedObjectMarks & ~(EObjectMark::OBJECTMARK_EditorOnly|EObjectMark::OBJECTMARK_NotForTargetPlatform)) : ExcludedObjectMarks;
	}

	EObjectFlags GetTopLevelFlags() const
	{
		return SaveArgs.TopLevelFlags;
	}

	bool IsUsingSlowTask() const
	{
		return SaveArgs.bSlowTask;
	}

	FOutputDevice* GetError() const
	{
		return SaveArgs.Error;
	}

	const FDateTime& GetFinalTimestamp() const
	{
		return SaveArgs.FinalTimeStamp;
	}

	FSavePackageContext* GetSavePackageContext() const
	{
		return SaveArgs.SavePackageContext;
	}

	bool IsCooking() const
	{
		return SaveArgs.IsCooking();
	}

	bool IsProceduralSave() const
	{
		return ObjectSaveContext.bProceduralSave;
	}

	bool IsUpdatingLoadedPath() const
	{
		return ObjectSaveContext.bUpdatingLoadedPath;
	}

	bool IsFilterEditorOnly() const
	{
		return Package->HasAnyPackageFlags(PKG_FilterEditorOnly);
	}

	bool IsStripEditorOnly() const
	{
		return !(SaveArgs.SaveFlags & ESaveFlags::SAVE_KeepEditorOnlyCookedPackages);
	}

	bool IsForceByteSwapping() const
	{
		return SaveArgs.bForceByteSwapping;
	}

	bool IsWarningLongFilename() const
	{
		return SaveArgs.bWarnOfLongFilename;
	}

	bool IsTextFormat() const
	{
		return bTextFormat;
	}

	bool IsFromAutoSave() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_FromAutosave);
	}

	bool IsSaveToMemory() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_Async) || PackageWriter;
	}

	bool IsGenerateSaveError() const
	{
		return !(SaveArgs.SaveFlags & SAVE_NoError);
	}

	bool IsKeepGuid() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_KeepGUID);
	}

	bool IsKeepDirty() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_KeepDirty);
	}

	bool IsSaveUnversionedNative() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_Unversioned_Native);
	}

	bool IsSaveUnversionedProperties() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_Unversioned_Properties) && bCanUseUnversionedPropertySerialization;
	}

	bool IsSaveOptional() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_Optional);
	}

	bool IsSaveAutoOptional() const
	{
		return bIsSaveAutoOptional;
	}

	bool IsConcurrent() const
	{
		return !!(SaveArgs.SaveFlags & SAVE_Concurrent);
	}

	bool IsCompareLinker() const
	{
		return !!(SaveArgs.SaveFlags & ESaveFlags::SAVE_CompareLinker);
	}

	bool CanSkipEditorReferencedPackagesWhenCooking() const
	{
		return SkipEditorRefCookingSetting;
	}

	bool IsIgnoringHeaderDiff() const
	{
		return bIgnoreHeaderDiffs;
	}

	bool IsProcessingPrestreamingRequests() const
	{
		return bIsProcessingPrestreamPackages;
	}

	bool IsFixupStandaloneFlags() const
	{
		return bIsFixupStandaloneFlags;
	}

	bool ShouldRehydratePayloads() const
	{
		return (SaveArgs.SaveFlags & ESaveFlags::SAVE_RehydratePayloads) != 0;
	}

	FUObjectSerializeContext* GetSerializeContext() const
	{
		return SerializeContext;
	}

	void SetSerializeContext(FUObjectSerializeContext* InContext)
	{
		SerializeContext = InContext;
	}

	FEDLCookCheckerThreadState* GetEDLCookChecker() const
	{
		return EDLCookChecker;
	}

	void SetEDLCookChecker(FEDLCookCheckerThreadState* InCookChecker)
	{
		EDLCookChecker = InCookChecker;
	}

	uint32 GetPortFlags() const
	{
		return PPF_DeepCompareInstances | PPF_DeepCompareDSOsOnly;
	}

	bool GetPostSaveRootRequired() const
	{
		return bPostSaveRootRequired;
	}

	void SetPostSaveRootRequired(bool bInPostSaveRootRequired)
	{
		bPostSaveRootRequired = bInPostSaveRootRequired;
	}

	bool GetPreSaveCleanup() const
	{
		return bNeedPreSaveCleanup;
	}

	void SetPreSaveCleanup(bool bInNeedPreSaveCleanup)
	{
		bNeedPreSaveCleanup = bInNeedPreSaveCleanup;
	}

	bool IsStubRequested() const
	{
		return bGenerateFileStub;
	}

	void RequestStubFile()
	{
		bGenerateFileStub = true;
	}

	ESaveRealm GetCurrentHarvestingRealm() const
	{
		return CurrentHarvestingRealm;
	}

	/** Returns which save context should be saved. */
	TArray<ESaveRealm> GetHarvestedRealmsToSave();

	void MarkUnsaveable(UObject* InObject);

	bool IsUnsaveable(UObject* InObject, bool bEmitWarning = true) const;
	enum class ESaveableStatus
	{
		Success,
		PendingKill,
		Transient,
		AbstractClass,
		DeprecatedClass,
		NewerVersionExistsClass,
		OuterUnsaveable,
		__Count,
	};
	ESaveableStatus GetSaveableStatus(UObject* InObject, UObject** OutCulprit = nullptr, ESaveableStatus* OutCulpritStatus = nullptr) const;
	ESaveableStatus GetSaveableStatusNoOuter(UObject* InObject) const;

	void RecordIllegalReference(UObject* InFrom, UObject* InTo, EIllegalRefReason InReason, FString&& InOptionalReasonText = FString())
	{
		HarvestedIllegalReferences.Add({ InFrom, InTo, InReason, MoveTemp(InOptionalReasonText) });
	}

	const TArray<FIllegalReference>& GetIllegalReferences() const
	{
		return HarvestedIllegalReferences;
	}
	
	void AddImport(UObject* InObject)
	{
		GetHarvestedRealm().AddImport(InObject);
	}

	void AddExport(FTaggedExport InTagObj)
	{
		GetHarvestedRealm().AddExport(MoveTemp(InTagObj));
	}

	void AddExcluded(UObject* InObject)
	{
		GetHarvestedRealm().AddExcluded(InObject);
	}

	bool IsImport(UObject* InObject) const
	{
		return GetHarvestedRealm().IsImport(InObject);
	}

	bool IsExport(UObject* InObject) const
	{
		return GetHarvestedRealm().IsExport(InObject);
	}

	bool IsIncluded(UObject* InObject) const
	{
		return GetHarvestedRealm().IsIncluded(InObject);
	}

	bool IsExcluded(UObject* InObject) const
	{
		return GetHarvestedRealm().IsExcluded(InObject);
	}

	TSet<FTaggedExport>& GetExports()
	{
		return GetHarvestedRealm().GetExports();
	}

	const TSet<UObject*>& GetImports() const
	{
		return GetHarvestedRealm().GetImports();
	}

	const TSet<UObject*>& GetImportsUsedInGame() const
	{
		return GetHarvestedRealm(ESaveRealm::Game).GetImports();
	}

	const TSet<FName>& GetSoftPackageReferenceList() const
	{
		return GetHarvestedRealm().GetSoftPackageReferenceList();
	}

	TSet<FName>& GetSoftPackageReferenceList()
	{
		return GetHarvestedRealm().GetSoftPackageReferenceList();
	}

	const TSet<FName>& GetSoftPackagesUsedInGame() const
	{
		return GetHarvestedRealm(ESaveRealm::Game).GetSoftPackageReferenceList();
	}

	TSet<FName>& GetSoftPackagesUsedInGame()
	{
		return GetHarvestedRealm(ESaveRealm::Game).GetSoftPackageReferenceList();
	}

	const TMap<UObject*, TArray<FName>>& GetSearchableNamesObjectMap() const
	{
		return GetHarvestedRealm().GetSearchableNamesObjectMap();
	}

	TMap<UObject*, TArray<FName>>& GetSearchableNamesObjectMap()
	{
		return GetHarvestedRealm().GetSearchableNamesObjectMap();
	}

	const TSet<FNameEntryId>& GetNamesReferencedFromExportData() const
	{
		return GetHarvestedRealm().GetNamesReferencedFromExportData();
	}

	const TSet<FNameEntryId>& GetNamesReferencedFromPackageHeader() const
	{
		return GetHarvestedRealm().GetNamesReferencedFromPackageHeader();
	}

	const TSet<FSoftObjectPath>& GetSoftObjectPathList() const
	{
		return GetHarvestedRealm().GetSoftObjectPathList();
	}

	const TMap<UObject*, TSet<UObject*>>& GetObjectDependencies() const
	{
		return GetHarvestedRealm().GetObjectDependencies();
	}

	const TMap<UObject*, TSet<UObject*>>& GetNativeObjectDependencies() const
	{
		return GetHarvestedRealm().GetNativeObjectDependencies();
	}

	bool NameExists(FNameEntryId ComparisonId) const
	{
		return GetHarvestedRealm().NameExists(ComparisonId);
	}

	const FCustomVersionContainer& GetCustomVersions() const
	{
		return CustomVersions;
	}

	const TSet<UPackage*>& GetPrestreamPackages() const
	{
		return PrestreamPackages;
	}

	TSet<UPackage*>& GetPrestreamPackages()
	{
		return PrestreamPackages;
	}

	bool IsPrestreamPackage(UPackage* InPackage) const
	{
		return PrestreamPackages.Contains(InPackage);
	}

	void AddPrestreamPackages(UPackage* InPackage)
	{
		PrestreamPackages.Add(InPackage);
	}

	void SetCustomVersions(FCustomVersionContainer InCustomVersions)
	{
		CustomVersions = MoveTemp(InCustomVersions);
	}

	TArray<FLinkerSave*> GetLinkers() const
	{
		TArray<FLinkerSave*> Linkers;
		for (const FHarvestedRealm& Realm : HarvestedRealms)
		{
			if (FLinkerSave* Linker = Realm.GetLinker())
			{
				Linkers.Add(Realm.GetLinker());
			}
		}
		return Linkers;
	}

	FLinkerSave* GetLinker() const
	{
		return GetHarvestedRealm().GetLinker();
	}

	void UpdatePackageLinkerVersions()
	{
		FLinkerSave* Linker = GetLinker();
		check(Linker);
		Package->SetLinkerPackageVersion(Linker->UEVer());
		Package->SetLinkerLicenseeVersion(Linker->LicenseeUEVer());
		Package->SetLinkerCustomVersions(Linker->GetCustomVersions());
	}

	void UpdatePackageFileSize(int64 InFileSize)
	{
		Package->SetFileSize(InFileSize);
	}

	void SetLinker(TPimplPtr<FLinkerSave> InLinker)
	{
		GetHarvestedRealm().SetLinker(MoveTemp(InLinker));
	}

	bool CloseLinkerArchives()
	{
		return GetHarvestedRealm().CloseLinkerArchives();
	}

	FArchive* GetTextFormatArchive() const
	{
		return GetHarvestedRealm().GetTextFormatArchive();
	}

	void SetTextFormatArchive(TUniquePtr<FArchive> InTextArchive)
	{
		GetHarvestedRealm().SetTextFormatArchive(MoveTemp(InTextArchive));
	}

	FArchiveFormatterType* GetFormatter() const
	{
		return GetHarvestedRealm().GetFormatter();
	}

	void SetFormatter(TUniquePtr<FArchiveFormatterType> InFormatter)
	{
		GetHarvestedRealm().SetFormatter(MoveTemp(InFormatter));
	}

	FStructuredArchive* GetStructuredArchive() const
	{
		return GetHarvestedRealm().GetStructuredArchive();
	}

	void SetStructuredArchive(TUniquePtr<FStructuredArchive> InArchive)
	{
		GetHarvestedRealm().SetStructuredArchive(MoveTemp(InArchive));
	}

	const TOptional<FString>& GetTempFilename() const
	{
		return GetHarvestedRealm().GetTempFilename();
	}

	void SetTempFilename(TOptional<FString> InTemp)
	{
		GetHarvestedRealm().SetTempFilename(MoveTemp(InTemp));
	}

	const TOptional<FString>& GetTextFormatTempFilename() const
	{
		return GetHarvestedRealm().GetTextFormatTempFilename();
	}

	void SetTextFormatTempFilename(TOptional<FString> InTemp)
	{
		GetHarvestedRealm().SetTextFormatTempFilename(MoveTemp(InTemp));
	}

	FSavePackageResultStruct GetFinalResult()
	{
		if (Result != ESavePackageResult::Success)
		{
			return Result;
		}

		ESavePackageResult FinalResult = IsStubRequested() ? ESavePackageResult::GenerateStub : ESavePackageResult::Success;
		return FSavePackageResultStruct(FinalResult, TotalPackageSizeUncompressed,
			SerializedPackageFlags, IsCompareLinker() ? MoveTemp(GetHarvestedRealm().Linker) : nullptr);
	}

	FObjectSaveContextData& GetObjectSaveContext()
	{
		return ObjectSaveContext;
	}

	IPackageWriter* GetPackageWriter() const
	{
		return PackageWriter;
	}

	ISavePackageValidator* GetPackageValidator() const
	{
		return SaveArgs.SavePackageContext ? SaveArgs.SavePackageContext->GetValidator() : nullptr;
	}

	const FHarvestedRealm& GetHarvestedRealm(ESaveRealm Realm = ESaveRealm::None) const
	{
		return HarvestedRealms[(uint32)(Realm == ESaveRealm::None ? CurrentHarvestingRealm : Realm)];
	}
	FHarvestedRealm& GetHarvestedRealm(ESaveRealm Realm = ESaveRealm::None)
	{
		return HarvestedRealms[(uint32)(Realm == ESaveRealm::None ? CurrentHarvestingRealm : Realm)];
	}

public:
	ESavePackageResult Result;

	EPropertyLocalizationGathererResultFlags GatherableTextResultFlags = EPropertyLocalizationGathererResultFlags::Empty;

	//@note FH: Most of these public members should be moved to harvested realm class 
	int64 PackageHeaderAndExportSize = 0;
	int64 TotalPackageSizeUncompressed = 0;
	int32 OffsetAfterPackageFileSummary = 0;
	int32 OffsetAfterImportMap = 0;
	int32 OffsetAfterExportMap = 0;
	int64 OffsetAfterPayloadToc = 0;
	int32 SerializedPackageFlags = 0;
	TArray<FLargeMemoryWriter, TInlineAllocator<4>> AdditionalFilesFromExports;
	FSavePackageOutputFileArray AdditionalPackageFiles;
private:

	// Create the harvesting contexts and automatic optional context gathering options
	void SetupHarvestingRealms();
		
	friend class FPackageHarvester;

	// Args
	UPackage* Package;
	UObject* Asset;
	FPackagePath TargetPackagePath;
	const TCHAR* Filename;
	FSavePackageArgs SaveArgs;
	IPackageWriter* PackageWriter;

	// State context
	FUObjectSerializeContext* SerializeContext = nullptr;
	FObjectSaveContextData ObjectSaveContext;
	bool bCanUseUnversionedPropertySerialization = false;
	bool bTextFormat = false;
	bool bIsProcessingPrestreamPackages = false;
	bool bIsFixupStandaloneFlags = false;
	bool bPostSaveRootRequired = false;
	bool bNeedPreSaveCleanup = false;
	bool bGenerateFileStub = false;
	bool bIgnoreHeaderDiffs = false;
	bool bIsSaveAutoOptional = false;

	// Config classes shared with the old Save
	FCanSkipEditorReferencedPackagesWhenCooking SkipEditorRefCookingSetting;

	// Pointer to the EDLCookChecker associated with this context
	FEDLCookCheckerThreadState* EDLCookChecker = nullptr;

	// Matching any mark in ExcludedObjectMarks indicates that an object should be excluded from being either an import or an export for this save
	const EObjectMark ExcludedObjectMarks;

	// Harvested custom versions
	FCustomVersionContainer CustomVersions;

	// The current default harvesting context being queried by the save context
	ESaveRealm CurrentHarvestingRealm = ESaveRealm::None;

	// Set of harvested content split per harvesting context
	TArray<FHarvestedRealm> HarvestedRealms;

	TArray<FIllegalReference> HarvestedIllegalReferences;

	// Set of harvested prestream packages, should be deprecated
	TSet<UPackage*> PrestreamPackages;
};

const TCHAR* LexToString(FSaveContext::ESaveableStatus Status);
