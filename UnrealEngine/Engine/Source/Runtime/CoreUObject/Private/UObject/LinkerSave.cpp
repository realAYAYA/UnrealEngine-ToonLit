// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerSave.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Optional.h"
#include "Serialization/BulkData.h"
#include "Serialization/LargeMemoryWriter.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "Templates/Casts.h"
#include "UObject/LazyObjectPtr.h"
#include "UObject/SoftObjectPtr.h"
#include "Internationalization/TextPackageNamespaceUtil.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectThreadContext.h"
#include "UObject/UnrealType.h"
#include "HAL/PlatformStackWalk.h"
#if WITH_EDITORONLY_DATA
#include "IO/IoDispatcher.h"
#include "Serialization/DerivedData.h"
#endif

/*----------------------------------------------------------------------------
	FLinkerSave.
----------------------------------------------------------------------------*/

/** A mapping of package name to generated script SHA keys */
TMap<FString, TArray<uint8> > FLinkerSave::PackagesToScriptSHAMap;

FLinkerSave::FLinkerSave(UPackage* InParent, const TCHAR* InFilename, bool bForceByteSwapping, bool bInSaveUnversioned)
:	FLinker(ELinkerType::Save, InParent)
,	Saver(nullptr)
{
	SetFilename(InFilename);

	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = IFileManager::Get().CreateFileWriter( InFilename, 0 );
		if( !Saver )
		{
			TCHAR LastErrorText[1024];
			uint32 LastError = FPlatformMisc::GetLastError();
			if (LastError != 0)
			{
				FPlatformMisc::GetSystemErrorMessage(LastErrorText, UE_ARRAY_COUNT(LastErrorText), LastError);
			}
			else
			{
				FCString::Strcpy(LastErrorText, TEXT("Unknown failure reason."));
			}
			UE_LOG(LogLinker, Error, TEXT("Error opening file '%s': %s"), InFilename, LastErrorText);
			return; // Caller must test this->Saver to detect the failure
		}

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag = PACKAGE_FILE_TAG;
		Summary.SetToLatestFileVersions(bInSaveUnversioned);
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		Summary.CompatibleWithEngineVersion = FEngineVersion::CompatibleWith();
		Summary.SetPackageFlags(Package ? Package->GetPackageFlags() : 0);

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			Summary.LocalizationId = TextNamespaceUtil::GetPackageNamespace(LinkerRoot);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		if (Package)
		{
#if WITH_EDITORONLY_DATA
			Summary.PackageName = Package->GetName();
#endif
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		this->SetIsSaving(true);
		this->SetIsPersistent(true);
		ArForceByteSwapping		= bForceByteSwapping;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			SetLocalizationNamespace(Summary.LocalizationId);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
	}
}


FLinkerSave::FLinkerSave(UPackage* InParent, FArchive *InSaver, bool bForceByteSwapping, bool bInSaveUnversioned)
: FLinker(ELinkerType::Save, InParent)
, Saver(nullptr)
{
	SetFilename(TEXT("$$Memory$$"));
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = InSaver;
		check(Saver);
#if WITH_EDITOR
		ArDebugSerializationFlags = Saver->ArDebugSerializationFlags;
#endif
		

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag = PACKAGE_FILE_TAG;
		Summary.SetToLatestFileVersions(bInSaveUnversioned);
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		Summary.CompatibleWithEngineVersion = FEngineVersion::CompatibleWith();
		Summary.SetPackageFlags(Package ? Package->GetPackageFlags() : 0);

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			Summary.LocalizationId = TextNamespaceUtil::GetPackageNamespace(LinkerRoot);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		if (Package)
		{
#if WITH_EDITORONLY_DATA
			Summary.PackageName = Package->GetName();
#endif
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		this->SetIsSaving(true);
		this->SetIsPersistent(true);
		ArForceByteSwapping = bForceByteSwapping;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			SetLocalizationNamespace(Summary.LocalizationId);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
	}
}

FLinkerSave::FLinkerSave(UPackage* InParent, bool bForceByteSwapping, bool bInSaveUnversioned )
:	FLinker(ELinkerType::Save, InParent)
,	Saver(nullptr)
{
	SetFilename(TEXT("$$Memory$$"));
	if (FPlatformProperties::HasEditorOnlyData())
	{
		// Create file saver.
		Saver = new FLargeMemoryWriter( 0, false, *InParent->GetLoadedPath().GetDebugName() );
		check(Saver);

		UPackage* Package = dynamic_cast<UPackage*>(LinkerRoot);

		// Set main summary info.
		Summary.Tag = PACKAGE_FILE_TAG;
		Summary.SetToLatestFileVersions(bInSaveUnversioned);
		Summary.SavedByEngineVersion = FEngineVersion::Current();
		Summary.CompatibleWithEngineVersion = FEngineVersion::CompatibleWith();
		Summary.SetPackageFlags(Package ? Package->GetPackageFlags() : 0);

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			Summary.LocalizationId = TextNamespaceUtil::GetPackageNamespace(LinkerRoot);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS

		if (Package)
		{
#if WITH_EDITORONLY_DATA
			Summary.PackageName = Package->GetName();
#endif
			Summary.ChunkIDs = Package->GetChunkIDs();
		}

		// Set status info.
		this->SetIsSaving(true);
		this->SetIsPersistent(true);
		ArForceByteSwapping		= bForceByteSwapping;

#if USE_STABLE_LOCALIZATION_KEYS
		if (GIsEditor)
		{
			SetLocalizationNamespace(Summary.LocalizationId);
		}
#endif // USE_STABLE_LOCALIZATION_KEYS
	}
}

bool FLinkerSave::CloseAndDestroySaver()
{
	bool bSuccess = true;
	if (Saver)
	{
		// first, do an explicit close to check for archive errors
		delete Saver;
	}
	Saver = nullptr;
	return bSuccess;
}

FLinkerSave::~FLinkerSave()
{
	CloseAndDestroySaver();
}

int32 FLinkerSave::MapName(FNameEntryId Id) const
{
	const int32* IndexPtr = NameIndices.Find(Id);

	if (IndexPtr)
	{
		return *IndexPtr;
	}

	return INDEX_NONE;
}

int32 FLinkerSave::MapSoftObjectPath(const FSoftObjectPath& SoftObjectPath) const
{
	const int32* IndexPtr = SoftObjectPathIndices.Find(SoftObjectPath);

	if (IndexPtr)
	{
		return *IndexPtr;
	}

	return INDEX_NONE;
}


FPackageIndex FLinkerSave::MapObject(TObjectPtr<const UObject> Object) const
{
	if (Object)
	{
		const FPackageIndex *Found = ObjectIndicesMap.Find(Object);
		if (Found)
		{
			if (IsCooking() && CurrentlySavingExport.IsExport() &&
				Object.GetPackage().GetFName() != GLongCoreUObjectPackageName && // We assume nothing in coreuobject ever loads assets in a constructor
				*Found != CurrentlySavingExport) // would be weird, but I can't be a dependency on myself
			{
				const FObjectExport& SavingExport = Exp(CurrentlySavingExport);
				bool bFoundDep = false;
				if (SavingExport.FirstExportDependency >= 0)
				{
					int32 NumDeps = SavingExport.CreateBeforeCreateDependencies + SavingExport.CreateBeforeSerializationDependencies + SavingExport.SerializationBeforeCreateDependencies + SavingExport.SerializationBeforeSerializationDependencies;
					for (int32 DepIndex = SavingExport.FirstExportDependency; DepIndex < SavingExport.FirstExportDependency + NumDeps; DepIndex++)
					{
						if (DepListForErrorChecking[DepIndex] == *Found)
						{
							bFoundDep = true;
							break;
						}
					}
				}
				if (!bFoundDep)
				{
					if (SavingExport.Object && SavingExport.Object->IsA(UClass::StaticClass()))
					{
						UClass* Class = CastChecked<UClass>(SavingExport.Object);
						if (Class->GetDefaultObject() == Object
					#if WITH_EDITORONLY_DATA
							|| Class->ClassGeneratedBy == Object
					#endif
							)
						{
							bFoundDep = true; // the class is saving a ref to the CDO...which doesn't really work or do anything useful, but it isn't an error or it is saving a reference to the class that generated it 
						}
					}
				}
				if (!bFoundDep)
				{
					UE_LOG(LogLinker, Fatal, TEXT("Attempt to map an object during save that was not listed as a dependency. Saving Export %d %s in %s. Missing Dep on %s %s."),
						CurrentlySavingExport.ForDebugging(), *SavingExport.ObjectName.ToString(), *GetArchiveName(),
						Found->IsExport() ? TEXT("Export") : TEXT("Import"), *ImpExp(*Found).ObjectName.ToString()
						);
				}
			}

			return *Found;
		}
	}
	return FPackageIndex();
}

void FLinkerSave::MarkScriptSerializationStart(const UObject* Obj) 
{
	if (ensure(Obj == CurrentlySavingExportObject))
	{
		FObjectExport& Export = ExportMap[CurrentlySavingExport.ToExport()];
		Export.ScriptSerializationStartOffset = Tell();
	}
}

void FLinkerSave::MarkScriptSerializationEnd(const UObject* Obj) 
{
	if (ensure(Obj == CurrentlySavingExportObject))
	{
		FObjectExport& Export = ExportMap[CurrentlySavingExport.ToExport()];
		Export.ScriptSerializationEndOffset = Tell();
	}
}

void FLinkerSave::Seek( int64 InPos )
{
	Saver->Seek( InPos );
}

int64 FLinkerSave::Tell()
{
	return Saver->Tell();
}

void FLinkerSave::Serialize( void* V, int64 Length )
{
#if WITH_EDITOR
	Saver->ArDebugSerializationFlags = ArDebugSerializationFlags;
	Saver->SetSerializedPropertyChain(GetSerializedPropertyChain(), GetSerializedProperty());
#endif
	Saver->Serialize( V, Length );
}

void FLinkerSave::OnPostSave(const FPackagePath& PackagePath, FObjectPostSaveContext ObjectSaveContext)
{
	for (TUniqueFunction<void(const FPackagePath&, FObjectPostSaveContext)>& Callback : PostSaveCallbacks)
	{
		Callback(PackagePath, ObjectSaveContext);
	}

	PostSaveCallbacks.Empty();
}
	
FString FLinkerSave::GetDebugName() const
{
	return GetFilename();
}

const FString& FLinkerSave::GetFilename() const
{
	// When the deprecated Filename is removed from FLinker, add a separate Filename variable to FLinkerSave
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	return Filename;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FLinkerSave::SetFilename(FStringView InFilename)
{
	// When the deprecated Filename is removed from FLinker, add a separate Filename variable to FLinkerSave
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	Filename = FString(InFilename);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

FString FLinkerSave::GetArchiveName() const
{
	return Saver->GetArchiveName();
}

FArchive& FLinkerSave::operator<<( FName& InName )
{
	int32 Save = MapName(InName.GetDisplayIndex());

	check(GetSerializeContext());

	bool bNameMapped = Save != INDEX_NONE;
	if (!bNameMapped)
	{
		// Set an error on the archive and record the error on the log output if one is set.
		SetCriticalError();
		FString ErrorMessage = FString::Printf(TEXT("Name \"%s\" is not mapped when saving %s (object: %s, property: %s). This can mean that this object serialize function is not deterministic between reference harvesting and serialization."),
			*InName.ToString(),
			*GetArchiveName(),
			*GetSerializeContext()->SerializedObject->GetFullName(),
			*GetFullNameSafe(GetSerializedProperty()));
		ensureMsgf(false, TEXT("%s"), *ErrorMessage);
		if (LogOutput)
		{
			LogOutput->Logf(ELogVerbosity::Error, TEXT("%s"), *ErrorMessage);
		}
	}

	if (!CurrentlySavingExport.IsNull())
	{
		if (Save >= Summary.NamesReferencedFromExportDataCount)
		{
			SetCriticalError();
			FString ErrorMessage = FString::Printf(TEXT("Name \"%s\" is referenced from an export but not mapped in the export data names region when saving %s (object: %s, property: %s)."),
				*InName.ToString(),
				*GetArchiveName(),
				*GetSerializeContext()->SerializedObject->GetFullName(),
				*GetFullNameSafe(GetSerializedProperty()));
			ensureMsgf(false, TEXT("%s"), *ErrorMessage);
			if (LogOutput)
			{
				LogOutput->Logf(ELogVerbosity::Error, TEXT("%s"), *ErrorMessage);
			}
		}
	}

	int32 Number = InName.GetNumber();
	FArchive& Ar = *this;
	return Ar << Save << Number;
}

FArchive& FLinkerSave::operator<<( UObject*& Obj )
{
	FPackageIndex Save;
	if (Obj)
	{
		Save = MapObject(Obj);
	}
	return *this << Save;
}

FArchive& FLinkerSave::operator<<(FSoftObjectPath& SoftObjectPath)
{
	// Map soft object path to indices if we aren't currently serializing the list itself
	// and we actually built one, cooking might want to serialize soft object path directly for example
	if (!bIsWritingHeader && SoftObjectPathList.Num() > 0)
	{
		int32 Save = MapSoftObjectPath(SoftObjectPath);
		bool bPathMapped = Save != INDEX_NONE;
		if (!bPathMapped)
		{
			// Set an error on the archive and record the error on the log output if one is set.
			SetCriticalError();
			FString ErrorMessage = FString::Printf(TEXT("SoftObjectPath \"%s\" is not mapped when saving %s (object: %s, property: %s). This can mean that this object serialize function is not deterministic between reference harvesting and serialization."),
				*SoftObjectPath.ToString(),
				*GetArchiveName(),
				*GetSerializeContext()->SerializedObject->GetFullName(),
				*GetFullNameSafe(GetSerializedProperty()));
			ensureMsgf(false, TEXT("%s"), *ErrorMessage);
			if (LogOutput)
			{
				LogOutput->Logf(ELogVerbosity::Error, TEXT("%s"), *ErrorMessage);
			}
		}
		return *this << Save;
	}
	else
	{
		return FArchiveUObject::operator<<(SoftObjectPath);
	}
}

FArchive& FLinkerSave::operator<<(FLazyObjectPtr& LazyObjectPtr)
{
	FUniqueObjectGuid ID;
	ID = LazyObjectPtr.GetUniqueID();
	return *this << ID;
}

bool FLinkerSave::ShouldSkipProperty(const FProperty* InProperty) const
{
	if (TransientPropertyOverrides && !TransientPropertyOverrides->IsEmpty())
	{
		const TSet<FProperty*>* Props = TransientPropertyOverrides->Find(CurrentlySavingExportObject);
		if (Props && Props->Contains(InProperty))
		{
			return true;
		}
	}
	return false;
}

void FLinkerSave::SetSerializeContext(FUObjectSerializeContext* InLoadContext)
{
	SaveContext = InLoadContext;
	if (Saver)
	{
		Saver->SetSerializeContext(InLoadContext);
	}
}

FUObjectSerializeContext* FLinkerSave::GetSerializeContext()
{
	return SaveContext;
}

void FLinkerSave::UsingCustomVersion(const struct FGuid& Guid)
{
	FArchiveUObject::UsingCustomVersion(Guid);

	// Here we're going to try and dump the callstack that added a new custom version after package summary has been serialized
	if (Summary.GetCustomVersionContainer().GetVersion(Guid) == nullptr)
	{
		FCustomVersion RegisteredVersion = FCurrentCustomVersions::Get(Guid).GetValue();

		FString CustomVersionWarning = FString::Printf(TEXT("Unexpected custom version \"%s\" used after package %s summary has been serialized. Callstack:\n"),
			*RegisteredVersion.GetFriendlyName().ToString(), *LinkerRoot->GetName());

		const int32 MaxStackFrames = 100;
		uint64 StackFrames[MaxStackFrames];
		int32 NumStackFrames = FPlatformStackWalk::CaptureStackBackTrace(StackFrames, MaxStackFrames);

		// Convert the stack trace to text, ignore the first functions (ProgramCounterToHumanReadableString)
		const int32 IgnoreStackLinesCount = 1;		
		ANSICHAR Buffer[1024];
		const ANSICHAR* CutoffFunction = "UPackage::Save";
		for (int32 Idx = IgnoreStackLinesCount; Idx < NumStackFrames; Idx++)
		{			
			Buffer[0] = '\0';
			FPlatformStackWalk::ProgramCounterToHumanReadableString(Idx, StackFrames[Idx], Buffer, sizeof(Buffer));
			CustomVersionWarning += TEXT("\t");
			CustomVersionWarning += Buffer;
			CustomVersionWarning += "\n";
			if (FCStringAnsi::Strstr(Buffer, CutoffFunction))
			{
				// Anything below UPackage::Save is not interesting from the point of view of what we're trying to find
				break;
			}
		}

		UE_LOG(LogLinker, Warning, TEXT("%s"), *CustomVersionWarning);
	}
}

void FLinkerSave::SetUseUnversionedPropertySerialization(bool bInUseUnversioned)
{
	FArchiveUObject::SetUseUnversionedPropertySerialization(bInUseUnversioned);
	if (Saver)
	{
		Saver->SetUseUnversionedPropertySerialization(bInUseUnversioned);
	}
	if (bInUseUnversioned)
	{
		Summary.SetPackageFlags(Summary.GetPackageFlags() | PKG_UnversionedProperties);
		if (LinkerRoot)
		{
			LinkerRoot->SetPackageFlags(PKG_UnversionedProperties);
		}
	}
	else
	{
		Summary.SetPackageFlags(Summary.GetPackageFlags() & ~PKG_UnversionedProperties);
		if (LinkerRoot)
		{
			LinkerRoot->ClearPackageFlags(PKG_UnversionedProperties);
		}
	}
}

void FLinkerSave::SetFilterEditorOnly(bool bInFilterEditorOnly)
{
	FArchiveUObject::SetFilterEditorOnly(bInFilterEditorOnly);
	if (Saver)
	{
		Saver->SetFilterEditorOnly(bInFilterEditorOnly);
	}
	if (bInFilterEditorOnly)
	{
		Summary.SetPackageFlags(Summary.GetPackageFlags() | PKG_FilterEditorOnly);
		if (LinkerRoot)
		{
			LinkerRoot->SetPackageFlags(PKG_FilterEditorOnly);
		}
	}
	else
	{
		Summary.SetPackageFlags(Summary.GetPackageFlags() & ~PKG_FilterEditorOnly);
		if (LinkerRoot)
		{
			LinkerRoot->ClearPackageFlags(PKG_FilterEditorOnly);
		}
	}
}

#if WITH_EDITORONLY_DATA
UE::FDerivedData FLinkerSave::AddDerivedData(const UE::FDerivedData& Data)
{
	UE_LOG(LogLinker, Warning, TEXT("Data will not be able to load because derived data is not saved yet."));

	UE::DerivedData::Private::FCookedData CookedData;

	const FPackageId PackageId = FPackageId::FromName(LinkerRoot->GetFName());
	const int32 ChunkIndex = ++LastDerivedDataIndex;
	checkf(ChunkIndex >= 0 && ChunkIndex < (1 << 24), TEXT("ChunkIndex %d is out of range."), ChunkIndex);

	// PackageId                 ChunkIndex Type
	// [00 01 02 03 04 05 06 07] [08 09 10] [11]
	*reinterpret_cast<uint8*>(&CookedData.ChunkId[11]) = static_cast<uint8>(EIoChunkType::DerivedData);
	*reinterpret_cast<uint32*>(&CookedData.ChunkId[7]) = NETWORK_ORDER32(ChunkIndex);
	*reinterpret_cast<uint64*>(&CookedData.ChunkId[0]) = PackageId.Value();

	CookedData.Flags = Data.GetFlags();
	return UE::FDerivedData(CookedData);
}
#endif // WITH_EDITORONLY_DATA

bool FLinkerSave::SerializeBulkData(FBulkData& BulkData, const FBulkDataSerializationParams& Params) 
{
	using namespace UE::BulkData::Private;

	auto CanSaveBulkDataByReference = [](FBulkData& BulkData) -> bool
	{
		return BulkData.GetBulkDataOffsetInFile() != INDEX_NONE &&
			// We don't support yet loading from a separate file
			!BulkData.IsInSeparateFile() &&
			// It is possible to have a BulkData marked as optional without putting it into a separate file, and we
			// assume that if BulkData is optional and in a separate file, then it is in the BulkDataOptional
			// segment. Rather than changing that assumption to support optional ExternalResource bulkdata, we
			// instead require that optional inlined/endofpackagedata BulkDatas can not be read from an
			// ExternalResource and must remain inline.
			!BulkData.IsOptional() &&
			// Inline or end-of-package-file data can only be loaded from the workspace domain package file if the
			// archive used by the bulk data was actually from the package file; BULKDATA_LazyLoadable is set by
			// Serialize iff that is the case										
			(BulkData.GetBulkDataFlags() & BULKDATA_LazyLoadable);
	};
	
	if (ShouldSkipBulkData())
	{
		return false;
	}

	const EBulkDataFlags BulkDataFlags	= static_cast<EBulkDataFlags>(BulkData.GetBulkDataFlags());
	int32 ResourceIndex					= DataResourceMap.Num();
	int64 PayloadSize					= BulkData.GetBulkDataSize();
	const bool bSupportsMemoryMapping	= IsCooking() && MemoryMappingAlignment >= 0;
	const bool bSaveAsResourceIndex		= IsCooking();

#if USE_RUNTIME_BULKDATA
	const bool bCustomElementSerialization = false;
#else
	const bool bCustomElementSerialization = BulkData.SerializeBulkDataElements != nullptr;
#endif
	
	TOptional<EFileRegionType> RegionToUse;
	if (bFileRegionsEnabled)
	{
		if (IsCooking())
		{
			RegionToUse = Params.RegionType;
		}
		else if (bDeclareRegionForEachAdditionalFile)
		{
			RegionToUse = EFileRegionType::None;
		}
	}
	FBulkMetaResource SerializedMeta;
	SerializedMeta.Flags = BulkDataFlags;
	SerializedMeta.ElementCount = PayloadSize / Params.ElementSize;
	SerializedMeta.SizeOnDisk = PayloadSize;

	if (bCustomElementSerialization)
	{
		// Force 64 bit precision when using custom element serialization
		FBulkData::SetBulkDataFlagsOn(SerializedMeta.Flags, static_cast<EBulkDataFlags>(BULKDATA_Size64Bit));
	}

	EBulkDataFlags FlagsToClear = static_cast<EBulkDataFlags>(BULKDATA_PayloadAtEndOfFile | BULKDATA_PayloadInSeperateFile | BULKDATA_WorkspaceDomainPayload | BULKDATA_ForceSingleElementSerialization | BULKDATA_NoOffsetFixUp);
	if (IsCooking())
	{
		FBulkData::SetBulkDataFlagsOn(FlagsToClear, static_cast<EBulkDataFlags>(BULKDATA_SerializeCompressed));
	}

	FBulkData::ClearBulkDataFlagsOn(SerializedMeta.Flags, FlagsToClear);

	const bool bSerializeInline =
		FBulkData::HasFlags(BulkDataFlags, BULKDATA_ForceInlinePayload) ||
		(IsCooking() && (FBulkData::HasFlags(BulkDataFlags, BULKDATA_Force_NOT_InlinePayload) == false)) ||
		IsTextFormat();

	if (bSerializeInline)
	{
		FArchive& Ar = *this;

		const int64 MetaOffset = Tell();
		if (bSaveAsResourceIndex)
		{
			Ar << ResourceIndex;
		}
		else
		{
			Ar << SerializedMeta;
		}

		SerializedMeta.Offset = Tell();
		SerializedMeta.SizeOnDisk = BulkData.SerializePayload(Ar, SerializedMeta.Flags, RegionToUse);
		if (bCustomElementSerialization)
		{
			PayloadSize = SerializedMeta.SizeOnDisk;
			SerializedMeta.ElementCount = PayloadSize / Params.ElementSize; 
		}

		if (bSaveAsResourceIndex == false)
		{
			FArchive::FScopeSeekTo _(Ar, MetaOffset);
			Ar << SerializedMeta;
		}
	}
	else
	{
		FBulkData::SetBulkDataFlagsOn(SerializedMeta.Flags, static_cast<EBulkDataFlags>(BULKDATA_PayloadAtEndOfFile));

		if (bSaveBulkDataToSeparateFiles)
		{
			check(bSaveBulkDataByReference == false);
			FBulkData::SetBulkDataFlagsOn(SerializedMeta.Flags, static_cast<EBulkDataFlags>(BULKDATA_PayloadInSeperateFile | BULKDATA_NoOffsetFixUp));
		}
		
		const bool bSaveByReference = bSaveBulkDataByReference && CanSaveBulkDataByReference(BulkData);
		if (bSaveByReference)
		{
			check(IsCooking() == false);
			FBulkData::SetBulkDataFlagsOn(SerializedMeta.Flags, static_cast<EBulkDataFlags>(BULKDATA_NoOffsetFixUp | BULKDATA_WorkspaceDomainPayload | BULKDATA_PayloadInSeperateFile));
		}

		if (bSaveBulkDataToSeparateFiles && FBulkData::HasFlags(SerializedMeta.Flags, BULKDATA_OptionalPayload))
		{
			SerializedMeta.Offset = OptionalBulkDataAr.Tell();
			SerializedMeta.SizeOnDisk = BulkData.SerializePayload(OptionalBulkDataAr, SerializedMeta.Flags, RegionToUse);
		}
		else if (bSaveBulkDataToSeparateFiles && FBulkData::HasFlags(SerializedMeta.Flags, BULKDATA_MemoryMappedPayload) && bSupportsMemoryMapping)
		{
			if (int64 Padding = Align(MemoryMappedBulkDataAr.Tell(), MemoryMappingAlignment) - MemoryMappedBulkDataAr.Tell(); Padding > 0)
			{
				TArray<uint8> Zeros;
				Zeros.SetNumZeroed(int32(Padding));
				MemoryMappedBulkDataAr.Serialize(Zeros.GetData(), Padding);
			}
			SerializedMeta.Offset = MemoryMappedBulkDataAr.Tell();
			SerializedMeta.SizeOnDisk = BulkData.SerializePayload(MemoryMappedBulkDataAr, SerializedMeta.Flags, RegionToUse);
		}
		else
		{
			if (bSaveBulkDataToSeparateFiles && FBulkData::HasFlags(SerializedMeta.Flags, BULKDATA_DuplicateNonOptionalPayload))
			{
				SerializedMeta.DuplicateFlags = SerializedMeta.Flags;
				SerializedMeta.DuplicateOffset = OptionalBulkDataAr.Tell();
				SerializedMeta.DuplicateSizeOnDisk = BulkData.SerializePayload(OptionalBulkDataAr, SerializedMeta.Flags, RegionToUse);

				FBulkData::ClearBulkDataFlagsOn(SerializedMeta.DuplicateFlags, BULKDATA_DuplicateNonOptionalPayload);
				FBulkData::SetBulkDataFlagsOn(SerializedMeta.DuplicateFlags, BULKDATA_OptionalPayload);
			}

			if (bSaveByReference)
			{
				SerializedMeta.Offset = BulkData.GetBulkDataOffsetInFile();
				SerializedMeta.SizeOnDisk = BulkData.GetBulkDataSizeOnDisk();
			}
			else
			{
				SerializedMeta.Offset = BulkDataAr.Tell();
				SerializedMeta.SizeOnDisk = BulkData.SerializePayload(BulkDataAr, SerializedMeta.Flags, RegionToUse);
			}
		}

		if (bCustomElementSerialization)
		{
			PayloadSize = SerializedMeta.SizeOnDisk;
			SerializedMeta.ElementCount = PayloadSize / Params.ElementSize; 
		}

		FArchive& Ar = *this;
		if (bSaveAsResourceIndex)
		{
			Ar << ResourceIndex;
		}
		else
		{
			Ar << SerializedMeta;
		}
	}

	FObjectDataResource& DataResource = DataResourceMap.AddDefaulted_GetRef();
	DataResource.RawSize				= PayloadSize;
	DataResource.SerialSize				= SerializedMeta.SizeOnDisk;
	DataResource.SerialOffset			= SerializedMeta.Offset;
	DataResource.DuplicateSerialOffset	= SerializedMeta.DuplicateOffset;
	DataResource.LegacyBulkDataFlags	= SerializedMeta.Flags;
	DataResource.OuterIndex				= ObjectIndicesMap.FindRef(Params.Owner);

	SerializedBulkData.Add(&BulkData, ResourceIndex);

	return true;
}

void FLinkerSave::OnPostSaveBulkData()
{
#if WITH_EDITOR
	if (bUpdatingLoadedPath)
	{
		for (TPair<FBulkData*, int32>& Kv : SerializedBulkData)
		{
			FBulkData& BulkData = *Kv.Key;
			const FObjectDataResource& DataResource = DataResourceMap[Kv.Value];
			BulkData.SetFlagsFromDiskWrittenValues(static_cast<EBulkDataFlags>(DataResource.LegacyBulkDataFlags), DataResource.SerialOffset, DataResource.SerialSize, Summary.BulkDataStartOffset);
		}
	}
#endif

	SerializedBulkData.Empty();
}
