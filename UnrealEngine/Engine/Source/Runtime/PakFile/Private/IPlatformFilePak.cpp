// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPlatformFilePak.h"
#include "HAL/FileManager.h"
#include "Math/GuardedInt.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Async/AsyncWork.h"
#include "Serialization/MemoryReader.h"
#include "HAL/IConsoleManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreDelegatesInternal.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManagerGeneric.h"
#include "HAL/IPlatformFileModule.h"
#include "SignedArchiveReader.h"
#include "Misc/AES.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "Async/AsyncFileHandle.h"
#include "Templates/Greater.h"
#include "Serialization/ArchiveProxy.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/Base64.h"
#include "HAL/DiskUtilizationTracker.h"
#include "Stats/StatsMisc.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/ThreadHeartBeat.h"
#if !(IS_PROGRAM || WITH_EDITOR)
#include "Misc/ConfigCacheIni.h"
#endif
#include "ProfilingDebugging/CsvProfiler.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/Fnv.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Async/MappedFileHandle.h"
#include "IoDispatcherFileBackend.h"
#include "Misc/PackageName.h"
#include "Misc/PathViews.h"

#include "ProfilingDebugging/LoadTimeTracker.h"
#include "IO/IoContainerHeader.h"
#include "FilePackageStore.h"
#include "Compression/OodleDataCompression.h"
#include "IO/IoStore.h"
#include "String/RemoveFrom.h"
#include "Algo/AnyOf.h"

DEFINE_LOG_CATEGORY(LogPakFile);

DEFINE_STAT(STAT_PakFile_Read);
DEFINE_STAT(STAT_PakFile_NumOpenHandles);

CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, FileIO);
CSV_DEFINE_CATEGORY(FileIOVerbose, false);


#if CSV_PROFILER
int64 GTotalLoaded = 0;
int64 GTotalLoadedLastTick = 0;
#endif






#ifndef DISABLE_NONUFS_INI_WHEN_COOKED
#define DISABLE_NONUFS_INI_WHEN_COOKED 0
#endif
#ifndef ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
#define ALLOW_INI_OVERRIDE_FROM_COMMANDLINE 0
#endif
#ifndef HAS_PLATFORM_PAK_INSTALL_CHECK
#define HAS_PLATFORM_PAK_INSTALL_CHECK 0
#endif
#ifndef ALL_PAKS_WILDCARD
#define ALL_PAKS_WILDCARD "*.pak"
#endif 

#ifndef MOUNT_STARTUP_PAKS_WILDCARD
#define MOUNT_STARTUP_PAKS_WILDCARD ALL_PAKS_WILDCARD
#endif

static FString GMountStartupPaksWildCard = TEXT(MOUNT_STARTUP_PAKS_WILDCARD);



int32 GetPakchunkIndexFromPakFile(const FString& InFilename)
{
	return FGenericPlatformMisc::GetPakchunkIndexFromPakFile(InFilename);
}

#if !UE_BUILD_SHIPPING
static void TestRegisterEncryptionKey(const TArray<FString>& Args)
{
	if (Args.Num() == 2)
	{
		FGuid EncryptionKeyGuid;
		FAES::FAESKey EncryptionKey;
		if (FGuid::Parse(Args[0], EncryptionKeyGuid))
		{
			TArray<uint8> KeyBytes;
			if (FBase64::Decode(Args[1], KeyBytes))
			{
				check(KeyBytes.Num() == sizeof(FAES::FAESKey));
				FMemory::Memcpy(EncryptionKey.Key, &KeyBytes[0], sizeof(EncryptionKey.Key));

				FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(EncryptionKeyGuid, EncryptionKey);
			}
		}
	}
}

static FAutoConsoleCommand CVar_TestRegisterEncryptionKey(
	TEXT("pak.TestRegisterEncryptionKey"),
	TEXT("Test dynamic encryption key registration. params: <guid> <base64key>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(TestRegisterEncryptionKey));
#endif

TPakChunkHash ComputePakChunkHash(const void* InData, int64 InDataSizeInBytes)
{
#if PAKHASH_USE_CRC
	return FCrc::MemCrc32(InData, IntCastChecked<int32>(InDataSizeInBytes));
#else
	FSHAHash Hash;
	FSHA1::HashBuffer(InData, InDataSizeInBytes, Hash.Hash);
	return Hash;
#endif
}

#ifndef EXCLUDE_NONPAK_UE_EXTENSIONS
#define EXCLUDE_NONPAK_UE_EXTENSIONS 1	// Use .Build.cs file to disable this if the game relies on accessing loose files
#endif

FFilenameSecurityDelegate& FPakPlatformFile::GetFilenameSecurityDelegate()
{
	static FFilenameSecurityDelegate Delegate;
	return Delegate;
}

FPakCustomEncryptionDelegate& FPakPlatformFile::GetPakCustomEncryptionDelegate()
{
	static FPakCustomEncryptionDelegate Delegate;
	return Delegate;
}

FPakPlatformFile::FPakSigningFailureHandlerData& FPakPlatformFile::GetPakSigningFailureHandlerData()
{
	static FPakSigningFailureHandlerData Instance;
	return Instance;
}

void FPakPlatformFile::BroadcastPakChunkSignatureCheckFailure(const FPakChunkSignatureCheckFailedData& InData)
{
	FPakSigningFailureHandlerData& HandlerData = GetPakSigningFailureHandlerData();
	FScopeLock Lock(&HandlerData.GetLock());
	HandlerData.GetPakChunkSignatureCheckFailedDelegate().Broadcast(InData);
}

void FPakPlatformFile::BroadcastPakPrincipalSignatureTableCheckFailure(const FString& InFilename)
{
	FPakSigningFailureHandlerData& HandlerData = GetPakSigningFailureHandlerData();
	FScopeLock Lock(&HandlerData.GetLock());
	HandlerData.GetPrincipalSignatureTableCheckFailedDelegate().Broadcast(InFilename);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FPakPlatformFile::BroadcastPakMasterSignatureTableCheckFailure(const FString& InFilename)
{
	return BroadcastPakPrincipalSignatureTableCheckFailure(InFilename);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS


FPakSetIndexSettings& FPakPlatformFile::GetPakSetIndexSettingsDelegate()
{
	static FPakSetIndexSettings Delegate;
	return Delegate;
}

void FPakPlatformFile::GetPrunedFilenamesInChunk(const FString& InPakFilename, const TArray<int32>& InChunkIDs, TArray<FString>& OutFileList)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);

	for (const FPakListEntry& Pak : Paks)
	{
		if (Pak.PakFile && Pak.PakFile->GetFilename() == InPakFilename)
		{
			Pak.PakFile->GetPrunedFilenamesInChunk(InChunkIDs, OutFileList);
			break;
		}
	}
}

void FPakPlatformFile::GetFilenamesFromIostoreByBlockIndex(const FString& InContainerName, const TArray<int32>& InBlockIndex, TArray<FString>& OutFileList)
{
	FPakPlatformFile* PakPlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
	if (!PakPlatformFile || !PakPlatformFile->IoDispatcherFileBackend.IsValid())
	{
		return;
	}

	const TMap<FGuid, FAES::FAESKey> Keys = UE::FEncryptionKeyManager::Get().GetAllKeys();

	FScopeLock ScopedLock(&PakPlatformFile->PakListCritical);
	for (const FPakListEntry& PakListEntry : PakPlatformFile->PakFiles)
	{
		if (FPaths::GetBaseFilename(PakListEntry.PakFile->PakFilename) == InContainerName)
		{
			TUniquePtr<FIoStoreReader> IoStoreReader(new FIoStoreReader());
			FIoStatus Status = IoStoreReader->Initialize(*FPaths::ChangeExtension(PakListEntry.PakFile->PakFilename, TEXT("")),  Keys);
			if (Status.IsOk())
			{
				IoStoreReader->GetFilenamesByBlockIndex(InBlockIndex, OutFileList);
			}
	
			break;
		}
	}
}

bool FPakPlatformFile::DirectoryExistsInPrunedPakFiles(const TCHAR* Directory)
{
	FString StandardPath = Directory;
	FPaths::MakeStandardFilename(StandardPath);

	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);

	// Check all pak files.
	for (int32 PakIndex = 0; PakIndex < Paks.Num(); PakIndex++)
	{
		if (Paks[PakIndex].PakFile->DirectoryExistsInPruned(*StandardPath))
		{
			return true;
		}
	}
	return false;
}

bool FPakPlatformFile::FindFileInPakFiles(TArray<FPakListEntry>& Paks, const TCHAR* Filename,
	TRefCountPtr<FPakFile>* OutPakFile, FPakEntry* OutEntry)
{
	FString StandardFilename(Filename);
	FPaths::MakeStandardFilename(StandardFilename);

	TArray<const FPakListEntry*, TInlineAllocator<1>> PaksWithDeleteRecord;
	bool bFoundOlderVersionOfDeleteRecordPak = false;

	for (int32 PakIndex = 0; PakIndex < Paks.Num(); PakIndex++)
	{
		const FPakListEntry& PakEntry = Paks[PakIndex];
		FPakFile* PakFile = PakEntry.PakFile.GetReference();
		if (!PakFile)
		{
			continue;
		}

		if (PaksWithDeleteRecord.Num() > 0)
		{
			if (Algo::AnyOf(PaksWithDeleteRecord, [&PakEntry](const FPakListEntry* DeletedPakEntry)
				{
					return DeletedPakEntry->ReadOrder > PakEntry.ReadOrder &&
						DeletedPakEntry->PakFile->PakchunkIndex == PakEntry.PakFile->PakchunkIndex;
				}))
			{
				// Found a delete record in a higher priority patch level, and this is an earlier version of the same file.
				// Don't search in the file.
				bFoundOlderVersionOfDeleteRecordPak = true;
				continue;
			}
		}

		FPakFile::EFindResult FindResult = PakFile->Find(StandardFilename, OutEntry);
		if (FindResult == FPakFile::EFindResult::Found)
		{
			if (OutPakFile != NULL)
			{
				*OutPakFile = PakFile;
			}
			UE_CLOG(!PaksWithDeleteRecord.IsEmpty(), LogPakFile, Verbose,
				TEXT("Delete Record: Ignored delete record for %s - found it in %s instead (asset was moved or duplicated between chunks)"),
				Filename, *PakFile->GetFilename());
			return true;
		}
		else if (FindResult == FPakFile::EFindResult::FoundDeleted)
		{
			PaksWithDeleteRecord.Add(&PakEntry);
			UE_LOG(LogPakFile, Verbose, TEXT("Delete Record: Found a delete record for %s in %s"),
				Filename, *PakFile->GetFilename());
		}
	}

	if (!PaksWithDeleteRecord.IsEmpty())
	{
		UE_CLOG(bFoundOlderVersionOfDeleteRecordPak, LogPakFile, Verbose,
			TEXT("Delete Record: Accepted a delete record for %s"), Filename);
		UE_CLOG(!bFoundOlderVersionOfDeleteRecordPak, LogPakFile, Warning,
			TEXT("Delete Record: No lower priority pak files looking for %s. (maybe not downloaded?)"), Filename);
	}
	return false;
}

bool FPakPlatformFile::FindFileInPakFiles(const TCHAR* Filename, TRefCountPtr<FPakFile>* OutPakFile,
	FPakEntry* OutEntry)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);

	return FindFileInPakFiles(Paks, Filename, OutPakFile, OutEntry);
}

bool FPakPlatformFile::DirectoryExists(const TCHAR* Directory)
{
	// Check pak files first.
	if (DirectoryExistsInPrunedPakFiles(Directory))
	{
		return true;
	}
	// Directory does not exist in any of the pak files, continue searching using inner platform file.
	bool Result = LowerLevel->DirectoryExists(Directory);
	return Result;
}

bool FPakPlatformFile::CreateDirectory(const TCHAR* Directory)
{
	// Directories can be created only under the normal path
	return LowerLevel->CreateDirectory(Directory);
}

bool FPakPlatformFile::DeleteDirectory(const TCHAR* Directory)
{
	// Even if the same directory exists outside of pak files it will never
	// get truly deleted from pak and will still be reported by Iterate functions.
	// Fail in cases like this.
	if (DirectoryExistsInPrunedPakFiles(Directory))
	{
		return false;
	}
	// Directory does not exist in pak files so it's safe to delete.
	return LowerLevel->DeleteDirectory(Directory);
}

FFileStatData FPakPlatformFile::GetStatData(const TCHAR* FilenameOrDirectory)
{
	// Check pak files first.
	FPakEntry FileEntry;
	TRefCountPtr<FPakFile> PakFile;
	if (FindFileInPakFiles(FilenameOrDirectory, &PakFile, &FileEntry))
	{
		return FFileStatData(
			PakFile->GetTimestamp(),
			PakFile->GetTimestamp(),
			PakFile->GetTimestamp(),
			(FileEntry.CompressionMethodIndex != 0) ? FileEntry.UncompressedSize : FileEntry.Size,
			false,	// IsDirectory
			true	// IsReadOnly
		);
	}

	// Then check pak directories
	if (DirectoryExistsInPrunedPakFiles(FilenameOrDirectory))
	{
		FDateTime DirectoryTimeStamp = FDateTime::MinValue();
		return FFileStatData(
			DirectoryTimeStamp,
			DirectoryTimeStamp,
			DirectoryTimeStamp,
			-1,		// FileSize
			true,	// IsDirectory
			true	// IsReadOnly
		);
	}

	// Fall back to lower level.
	FFileStatData FileStatData;
	if (IsNonPakFilenameAllowed(FilenameOrDirectory))
	{
		FileStatData = LowerLevel->GetStatData(FilenameOrDirectory);
	}

	return FileStatData;
}

namespace UE::PakFile::Private
{

/** Helper class to filter out files which have already been visited in one of the pak files. */
class FPreventDuplicatesVisitorBase
{
public:
	/** Visited files. */
	TSet<FString>& VisitedFiles;
	FString NormalizedFilename;

	FPreventDuplicatesVisitorBase(TSet<FString>& InVisitedFiles)
		: VisitedFiles(InVisitedFiles)
	{
	}

	bool CheckDuplicate(const TCHAR* FilenameOrDirectory)
	{
		NormalizedFilename.Reset();
		NormalizedFilename.AppendChars(FilenameOrDirectory, TCString<TCHAR>::Strlen(FilenameOrDirectory));
		FPaths::MakeStandardFilename(NormalizedFilename);
		if (VisitedFiles.Contains(NormalizedFilename))
		{
			return true;
		}
		VisitedFiles.Add(NormalizedFilename);
		return false;
	}
};

class FPreventDuplicatesVisitor : public FPreventDuplicatesVisitorBase, public IPlatformFile::FDirectoryVisitor
{
public:
	/** Wrapped visitor. */
	FDirectoryVisitor& Visitor;

	/** Constructor. */
	FPreventDuplicatesVisitor(FDirectoryVisitor& InVisitor, TSet<FString>& InVisitedFiles)
		: FPreventDuplicatesVisitorBase(InVisitedFiles)
		, Visitor(InVisitor)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
	{
		if (CheckDuplicate(FilenameOrDirectory))
		{
			// Already visited, continue iterating.
			return true;
		}
		return Visitor.CallShouldVisitAndVisit(*NormalizedFilename, bIsDirectory);
	}
};

/**
 * A file/directory visitor for files in PakFiles, used to share code for FDirectoryVisitor and FDirectoryStatVisitor
 * when iterating over files in pakfiles.
 */
class FPakFileDirectoryVisitorBase
{
public:
	FPakFileDirectoryVisitorBase()
	{
	}
	virtual ~FPakFileDirectoryVisitorBase() { }

	virtual bool ShouldVisitLeafPathname(FStringView LeafNormalizedPathname) = 0;
	virtual bool Visit(const FString& Filename, const FString& NormalizedFilename, bool bIsDir, FPakFile& PakFile) = 0;
	// No need for CallShouldVisitAndVisit because we call ShouldVisitLeafPathname separately in all cases
};

/** FPakFileDirectoryVisitorBase for a FDirectoryVisitor. */
class FPakFileDirectoryVisitor : public FPakFileDirectoryVisitorBase
{
public:
	FPakFileDirectoryVisitor(IPlatformFile::FDirectoryVisitor& InInner)
		: Inner(InInner)
	{
	}
	virtual bool ShouldVisitLeafPathname(FStringView LeafNormalizedPathname) override
	{
		return Inner.ShouldVisitLeafPathname(LeafNormalizedPathname);
	}
	virtual bool Visit(const FString& Filename, const FString& NormalizedFilename,
		bool bIsDir, FPakFile& PakFile) override
	{
		return Inner.Visit(*NormalizedFilename, bIsDir);
	}

	IPlatformFile::FDirectoryVisitor& Inner;
};

}

bool FPakPlatformFile::IterateDirectory(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	return IterateDirectoryInternal(Directory, Visitor, false /* bRecursive */);
}

bool FPakPlatformFile::IterateDirectoryInternal(const TCHAR* Directory,
	IPlatformFile::FDirectoryVisitor& Visitor, bool bRecursive)
{
	using namespace UE::PakFile::Private;

	FPakFileDirectoryVisitor PakVisitor(Visitor);
	TSet<FString> FilesVisitedInPak;
	bool Result = IterateDirectoryInPakFiles(Directory, PakVisitor, bRecursive, FilesVisitedInPak);
	if (Result && LowerLevel->DirectoryExists(Directory))
	{
		// Iterate inner filesystem but don't visit any files that were found in the Paks
		FPreventDuplicatesVisitor PreventDuplicatesVisitor(Visitor, FilesVisitedInPak);
		IPlatformFile::FDirectoryVisitor& LowerLevelVisitor(
			// For performance, skip using PreventDuplicatedVisitor if there were no hits in pak
			FilesVisitedInPak.Num() ? PreventDuplicatesVisitor : Visitor
		);
		if (bRecursive)
		{
			Result = LowerLevel->IterateDirectoryRecursively(Directory, LowerLevelVisitor);
		}
		else
		{
			Result = LowerLevel->IterateDirectory(Directory, LowerLevelVisitor);
		}
	}
	return Result;
}

bool FPakPlatformFile::IterateDirectoryInPakFiles(const TCHAR* Directory,
	UE::PakFile::Private::FPakFileDirectoryVisitorBase& Visitor, bool bRecursive, TSet<FString>& FilesVisitedInPak)
{
	bool Result = true;

	TArray<FPakListEntry> Paks;
	FString StandardDirectory = Directory;
	FPaths::MakeStandardFilename(StandardDirectory);

	bool bIsDownloadableDir =
		(
			FPaths::HasProjectPersistentDownloadDir() &&
			StandardDirectory.StartsWith(FPaths::ProjectPersistentDownloadDir())
			) ||
		StandardDirectory.StartsWith(FPaths::CloudDir());

	// don't look for in pak files for target-only locations
	if (!bIsDownloadableDir)
	{
		GetMountedPaks(Paks);
	}

	// Iterate pak files first
	FString NormalizationBuffer;
	TSet<FString> FilesVisitedInThisPak;
	auto ShouldVisit = [&Visitor](FStringView UnnormalizedPath)
	{
		FStringView NormalizedPath = UE::String::RemoveFromEnd(UnnormalizedPath, TEXTVIEW("/"));
		return Visitor.ShouldVisitLeafPathname(FPathViews::GetCleanFilename(NormalizedPath));
	};
	for (int32 PakIndex = 0; PakIndex < Paks.Num(); PakIndex++)
	{
		FPakFile& PakFile = *Paks[PakIndex].PakFile;

		const bool bIncludeFiles = true;
		const bool bIncludeFolders = true;

		FilesVisitedInThisPak.Reset();
		PakFile.FindPrunedFilesAtPathInternal(*StandardDirectory, ShouldVisit, FilesVisitedInThisPak,
			bIncludeFiles, bIncludeFolders, bRecursive);
		for (TSet<FString>::TConstIterator SetIt(FilesVisitedInThisPak); SetIt && Result; ++SetIt)
		{
			const FString& Filename = *SetIt;
			bool bIsDir = Filename.Len() && Filename[Filename.Len() - 1] == '/';
			const FString* NormalizedFilename;
			if (bIsDir)
			{
				NormalizationBuffer.Reset(Filename.Len());
				NormalizationBuffer.AppendChars(*Filename, Filename.Len() - 1); // Chop off the trailing /
				NormalizedFilename = &NormalizationBuffer;
			}
			else
			{
				NormalizedFilename = &Filename;
			}
			if (!FilesVisitedInPak.Contains(*NormalizedFilename))
			{
				FilesVisitedInPak.Add(*NormalizedFilename);
				Result = Visitor.Visit(Filename, *NormalizedFilename, bIsDir, PakFile) && Result;
			}
		}
	}
	return Result;
}

bool FPakPlatformFile::IterateDirectoryRecursively(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor)
{
	return IterateDirectoryInternal(Directory, Visitor, true /* bRecursive */);
}

namespace UE::PakFile::Private
{

class FPreventDuplicatesStatVisitor : public FPreventDuplicatesVisitorBase, public IPlatformFile::FDirectoryStatVisitor
{
public:
	/** Wrapped visitor. */
	FDirectoryStatVisitor& Visitor;

	/** Constructor. */
	FPreventDuplicatesStatVisitor(FDirectoryStatVisitor& InVisitor, TSet<FString>& InVisitedFiles)
		: FPreventDuplicatesVisitorBase(InVisitedFiles)
		, Visitor(InVisitor)
	{}
	virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData)
	{
		if (CheckDuplicate(FilenameOrDirectory))
		{
			// Already visited, continue iterating.
			return true;
		}
		return Visitor.CallShouldVisitAndVisit(*NormalizedFilename, StatData);
	}
};

/** FPakFileDirectoryVisitorBase for a FDirectoryStatVisitor. */
class FPakFileDirectoryStatVisitor : public FPakFileDirectoryVisitorBase
{
public:
	FPakFileDirectoryStatVisitor(FPakPlatformFile& InPlatformFile, IPlatformFile::FDirectoryStatVisitor& InInner)
		: PlatformFile(InPlatformFile)
		, Inner(InInner)
	{
	}
	virtual bool ShouldVisitLeafPathname(FStringView LeafNormalizedPathname) override
	{
		return Inner.ShouldVisitLeafPathname(LeafNormalizedPathname);
	}
	virtual bool Visit(const FString& Filename, const FString& NormalizedFilename,
		bool bIsDir, FPakFile& PakFile) override
	{
		int64 FileSize = -1;
		if (!bIsDir)
		{
			FPakEntry FileEntry;
			if (PlatformFile.FindFileInPakFiles(*Filename, nullptr, &FileEntry))
			{
				FileSize = (FileEntry.CompressionMethodIndex != 0) ? FileEntry.UncompressedSize : FileEntry.Size;
			}
		}

		const FFileStatData StatData(
			PakFile.GetTimestamp(),
			PakFile.GetTimestamp(),
			PakFile.GetTimestamp(),
			FileSize,
			bIsDir,
			true	// IsReadOnly
		);

		return Inner.Visit(*NormalizedFilename, StatData);
	}

	FPakPlatformFile& PlatformFile;
	IPlatformFile::FDirectoryStatVisitor& Inner;
};

}

bool FPakPlatformFile::IterateDirectoryStat(const TCHAR* Directory, IPlatformFile::FDirectoryStatVisitor& Visitor)
{
	return IterateDirectoryStatInternal(Directory, Visitor, false /* bRecursive */);
}

bool FPakPlatformFile::IterateDirectoryStatInternal(const TCHAR* Directory,
	IPlatformFile::FDirectoryStatVisitor& Visitor, bool bRecursive)
{
	using namespace UE::PakFile::Private;

	FPakFileDirectoryStatVisitor PakVisitor(*this, Visitor);
	TSet<FString> FilesVisitedInPak;
	bool Result = IterateDirectoryInPakFiles(Directory, PakVisitor, bRecursive, FilesVisitedInPak);
	if (Result && LowerLevel->DirectoryExists(Directory))
	{
		// Iterate inner filesystem but don't visit any files that were found in the Paks
		FPreventDuplicatesStatVisitor PreventDuplicatesVisitor(Visitor, FilesVisitedInPak);
		IPlatformFile::FDirectoryStatVisitor& LowerLevelVisitor(
			// For performance, skip using PreventDuplicatedVisitor if there were no hits in pak
			FilesVisitedInPak.Num() ? PreventDuplicatesVisitor : Visitor);
		if (bRecursive)
		{
			Result = LowerLevel->IterateDirectoryStatRecursively(Directory, LowerLevelVisitor);
		}
		else
		{
			Result = LowerLevel->IterateDirectoryStat(Directory, LowerLevelVisitor);
		}
	}
	return Result;
}

bool FPakPlatformFile::IterateDirectoryStatRecursively(const TCHAR* Directory,
	IPlatformFile::FDirectoryStatVisitor& Visitor)
{
	return IterateDirectoryStatInternal(Directory, Visitor, true/* bRecursive */);
}

void FPakPlatformFile::FindFiles(TArray<FString>& FoundFiles, const TCHAR* Directory, const TCHAR* FileExtension)
{
	if (LowerLevel->DirectoryExists(Directory))
	{
		LowerLevel->FindFiles(FoundFiles, Directory, FileExtension);
	}

	bool bRecursive = false;
	FindFilesInternal(FoundFiles, Directory, FileExtension, bRecursive);
}

void FPakPlatformFile::FindFilesRecursively(TArray<FString>& FoundFiles,
	const TCHAR* Directory, const TCHAR* FileExtension)
{
	if (LowerLevel->DirectoryExists(Directory))
	{
		LowerLevel->FindFilesRecursively(FoundFiles, Directory, FileExtension);
	}

	bool bRecursive = true;
	FindFilesInternal(FoundFiles, Directory, FileExtension, bRecursive);
}

void FPakPlatformFile::FindFilesInternal(TArray<FString>& FoundFiles,
	const TCHAR* Directory, const TCHAR* FileExtension, bool bRecursive)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);
	if (Paks.Num())
	{
		TSet<FString> FilesVisited;
		FilesVisited.Append(FoundFiles);

		FString StandardDirectory = Directory;
		FStringView FileExtensionStr = FileExtension;
		FPaths::MakeStandardFilename(StandardDirectory);
		bool bIncludeFiles = true;
		bool bIncludeFolders = false;

		auto ShouldVisit = [FileExtensionStr](FStringView Filename)
		{
			// filter out files by FileExtension
			return FileExtensionStr.Len() == 0 || Filename.EndsWith(FileExtensionStr, ESearchCase::IgnoreCase);
		};

		TArray<FString> FilesInPak;
		FilesInPak.Reserve(64);
		for (int32 PakIndex = 0; PakIndex < Paks.Num(); PakIndex++)
		{
			FPakFile& PakFile = *Paks[PakIndex].PakFile;
			PakFile.FindPrunedFilesAtPathInternal(*StandardDirectory, ShouldVisit, FilesInPak,
				bIncludeFiles, bIncludeFolders, bRecursive);
		}

		for (const FString& Filename : FilesInPak)
		{
			// make sure we don't add duplicates to FoundFiles
			bool bVisited = false;
			FilesVisited.Add(Filename, &bVisited);
			if (!bVisited)
			{
				FoundFiles.Add(Filename);
			}
		}
	}
}

bool FPakPlatformFile::DeleteDirectoryRecursively(const TCHAR* Directory)
{
	// Can't delete directories existing in pak files. See DeleteDirectory(..) for more info.
	if (DirectoryExistsInPrunedPakFiles(Directory))
	{
		return false;
	}
	// Directory does not exist in pak files so it's safe to delete.
	return LowerLevel->DeleteDirectoryRecursively(Directory);
}

bool FPakPlatformFile::CreateDirectoryTree(const TCHAR* Directory)
{
	// Directories can only be created only under the normal path
	return LowerLevel->CreateDirectoryTree(Directory);
}

void FPakPlatformFile::GetPrunedFilenamesInPakFile(const FString& InPakFilename, TArray<FString>& OutFileList)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);

	for (const FPakListEntry& Pak : Paks)
	{
		if (Pak.PakFile && Pak.PakFile->GetFilename() == InPakFilename)
		{
			Pak.PakFile->GetPrunedFilenames(OutFileList);
			break;
		}
	}
}

void FPakPlatformFile::GetFilenamesFromIostoreContainer(const FString& InContainerName, TArray<FString>& OutFileList)
{
	FPakPlatformFile* PakPlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
	if (!PakPlatformFile || !PakPlatformFile->IoDispatcherFileBackend.IsValid())
	{
		return;
	}

	const TMap<FGuid, FAES::FAESKey> Keys = UE::FEncryptionKeyManager::Get().GetAllKeys();

	FScopeLock ScopedLock(&PakPlatformFile->PakListCritical);
	for (const FPakListEntry& PakListEntry : PakPlatformFile->PakFiles)
	{
		if (FPaths::GetBaseFilename(PakListEntry.PakFile->PakFilename) == InContainerName)
		{
			TUniquePtr<FIoStoreReader> IoStoreReader(new FIoStoreReader());
			FIoStatus Status = IoStoreReader->Initialize(*FPaths::ChangeExtension(PakListEntry.PakFile->PakFilename, TEXT("")), Keys);
			if (Status.IsOk())
			{
				IoStoreReader->GetFilenames(OutFileList);
			}
			break;
		}
	}
}

void FPakPlatformFile::ForeachPackageInIostoreWhile(TFunctionRef<bool(FName)> Predicate)
{
	FPakPlatformFile* PakPlatformFile = static_cast<FPakPlatformFile*>(FPlatformFileManager::Get().FindPlatformFile(FPakPlatformFile::GetTypeName()));
	if (!PakPlatformFile || !PakPlatformFile->IoDispatcherFileBackend.IsValid())
	{
		return;
	}

	const TMap<FGuid, FAES::FAESKey> Keys = UE::FEncryptionKeyManager::Get().GetAllKeys();

	FScopeLock ScopedLock(&PakPlatformFile->PakListCritical);
	for (const FPakListEntry& PakListEntry : PakPlatformFile->PakFiles)
	{
		TUniquePtr<FIoStoreReader> IoStoreReader(new FIoStoreReader());
		FIoStatus Status = IoStoreReader->Initialize(*FPaths::ChangeExtension(PakListEntry.PakFile->PakFilename, TEXT("")), Keys);
		if (Status.IsOk())
		{
			const FIoDirectoryIndexReader& DirectoryIndex = IoStoreReader->GetDirectoryIndexReader();

			const bool Result = DirectoryIndex.IterateDirectoryIndex(
				FIoDirectoryIndexHandle::RootDirectory(),
				TEXT(""),
				[Predicate](FStringView Filename, uint32) -> bool
				{
					const FStringView Ext = FPathViews::GetExtension(Filename);
					if (Ext != TEXTVIEW("umap") && Ext != TEXTVIEW("uasset"))
					{
						return true; // ignore non package files
					}

					TStringBuilder<256> PackageNameBuilder;
					if (FPackageName::TryConvertFilenameToLongPackageName(Filename, PackageNameBuilder))
					{
						return Invoke(Predicate, FName(PackageNameBuilder.ToView()));
					}

					return true; // ignore not mapped packages
				});
			if (!Result)
			{
				return;
			}
		}
	}
}

#if !defined(PLATFORM_BYPASS_PAK_PRECACHE)
	#error "PLATFORM_BYPASS_PAK_PRECACHE must be defined."
#endif

#define USE_PAK_PRECACHE (!PLATFORM_BYPASS_PAK_PRECACHE && !IS_PROGRAM && !WITH_EDITOR) // you can turn this off to use the async IO stuff without the precache

/**
* Precaching
*/

void FPakPlatformFile::GetPakEncryptionKey(FAES::FAESKey& OutKey, const FGuid& InEncryptionKeyGuid)
{
	OutKey.Reset();

	if (!UE::FEncryptionKeyManager::Get().TryGetKey(InEncryptionKeyGuid, OutKey))
	{
		if (!InEncryptionKeyGuid.IsValid() && FCoreDelegates::GetPakEncryptionKeyDelegate().IsBound())
		{
			FCoreDelegates::GetPakEncryptionKeyDelegate().Execute(OutKey.Key);
		}
		else
		{
			UE_LOG(LogPakFile, Fatal, TEXT("Failed to find requested encryption key %s"), *InEncryptionKeyGuid.ToString());
		}
	}
}

TMap<FName, TSharedPtr<const FPakSignatureFile, ESPMode::ThreadSafe>> FPakPlatformFile::PakSignatureFileCache;
FCriticalSection FPakPlatformFile::PakSignatureFileCacheLock;

TSharedPtr<const FPakSignatureFile, ESPMode::ThreadSafe> FPakPlatformFile::GetPakSignatureFile(const TCHAR* InFilename)
{
	FName FilenameFName(InFilename);
	{
		FScopeLock Lock(&PakSignatureFileCacheLock);
		if (TSharedPtr<const FPakSignatureFile, ESPMode::ThreadSafe>* ExistingSignatureFile = PakSignatureFileCache.Find(FilenameFName))
		{
			return *ExistingSignatureFile;
		}
	}

	static FRSAKeyHandle PublicKey = []() -> FRSAKeyHandle
	{
		TDelegate<void(TArray<uint8>&, TArray<uint8>&)>& Delegate = FCoreDelegates::GetPakSigningKeysDelegate();
		if (Delegate.IsBound())
		{
			TArray<uint8> Exponent;
			TArray<uint8> Modulus;
			Delegate.Execute(Exponent, Modulus);
			return FRSA::CreateKey(Exponent, TArray<uint8>(), Modulus);
		}
		return InvalidRSAKeyHandle;
	}();

	TSharedPtr<FPakSignatureFile, ESPMode::ThreadSafe> NewSignatureFile;

	if (PublicKey != InvalidRSAKeyHandle)
	{
		FString SignaturesFilename = FPaths::ChangeExtension(InFilename, TEXT("sig"));
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*SignaturesFilename));
		if (Reader != nullptr)
		{
			NewSignatureFile = MakeShared<FPakSignatureFile, ESPMode::ThreadSafe>();
			NewSignatureFile->Serialize(*Reader);

			if (!NewSignatureFile->DecryptSignatureAndValidate(PublicKey, InFilename))
			{
				// We don't need to act on this failure as the decrypt function will already have dumped out log messages
				// and fired the signature check fail handler
				NewSignatureFile.Reset();
			}

			{
				FScopeLock Lock(&PakSignatureFileCacheLock);
				if (TSharedPtr<const FPakSignatureFile, ESPMode::ThreadSafe>* ExistingSignatureFile = PakSignatureFileCache.Find(FilenameFName))
				{
					return *ExistingSignatureFile;
				}
				PakSignatureFileCache.Add(FilenameFName, NewSignatureFile);
			}
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Couldn't find pak signature file '%s'"), InFilename);
			BroadcastPakPrincipalSignatureTableCheckFailure(InFilename);
		}
	}

	return NewSignatureFile;
}

void FPakPlatformFile::RemoveCachedPakSignaturesFile(const TCHAR* InFilename)
{
	FName FilenameFName(InFilename);
	FScopeLock Lock(&PakSignatureFileCacheLock);
	PakSignatureFileCache.Remove(FilenameFName);
}

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PakCache Sync Decrypts (Uncompressed Path)"), STAT_PakCache_SyncDecrypts, STATGROUP_PakFile);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("PakCache Decrypt Time"), STAT_PakCache_DecryptTime, STATGROUP_PakFile);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PakCache Async Decrypts (Compressed Path)"), STAT_PakCache_CompressedDecrypts, STATGROUP_PakFile);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("PakCache Async Decrypts (Uncompressed Path)"), STAT_PakCache_UncompressedDecrypts, STATGROUP_PakFile);

void DecryptData(uint8* InData, uint64 InDataSize, FGuid InEncryptionKeyGuid)
{
	uint32 DataSize = IntCastChecked<uint32>(InDataSize);
	if (FPakPlatformFile::GetPakCustomEncryptionDelegate().IsBound())
	{
		FPakPlatformFile::GetPakCustomEncryptionDelegate().Execute(InData, DataSize, InEncryptionKeyGuid);
	}
	else
	{
		SCOPE_SECONDS_ACCUMULATOR(STAT_PakCache_DecryptTime);
		FAES::FAESKey Key;
		FPakPlatformFile::GetPakEncryptionKey(Key, InEncryptionKeyGuid);
		check(Key.IsValid());
		FAES::DecryptData(InData, DataSize, Key);
	}
}

#if !UE_BUILD_SHIPPING
static int32 GPakCache_ForceDecompressionFails = 0;
static FAutoConsoleVariableRef CVar_ForceDecompressionFails(
	TEXT("ForceDecompressionFails"),
	GPakCache_ForceDecompressionFails,
	TEXT("If > 0, then force decompression failures to test the panic sync read fallback.")
);
static bool GPakCache_ForcePakProcessedReads = false;
static FAutoConsoleVariableRef CVar_ForcePakProcessReads(
	TEXT("ForcePakProcessReads"),
	GPakCache_ForcePakProcessedReads,
	TEXT("If true, then Asynchronous reads from pak files will always used the FPakProcessedReadRequest system that is ordinarily only used on compressed files.")
);
static bool GetPakCacheForcePakProcessedReads()
{
	static bool bInitialValue = (GPakCache_ForcePakProcessedReads = FParse::Param(FCommandLine::Get(), TEXT("ForcePakProcessReads")));
	return GPakCache_ForcePakProcessedReads;
}
static FName GPakFakeCompression(TEXT("PakFakeCompression"));

#endif

class FPakSizeRequest : public IAsyncReadRequest
{
public:
	FPakSizeRequest(FAsyncFileCallBack* CompleteCallback, int64 InFileSize)
		: IAsyncReadRequest(CompleteCallback, true, nullptr)
	{
		Size = InFileSize;
		SetComplete();
	}

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		// Even though SetComplete called in the constructor and sets bCompleteAndCallbackCalled=true, we still need to implement WaitComplete as
		// the CompleteCallback can end up starting async tasks that can overtake the constructor execution and need to wait for the constructor to finish.
		while (!*(volatile bool*)&bCompleteAndCallbackCalled);
	}

	virtual void CancelImpl() override
	{
	}

	virtual void ReleaseMemoryOwnershipImpl() override
	{
	}
};

#if USE_PAK_PRECACHE
#include "Async/TaskGraphInterfaces.h"
#define PAK_CACHE_GRANULARITY (1024*64)
static_assert((PAK_CACHE_GRANULARITY % FPakInfo::MaxChunkDataSize) == 0, "PAK_CACHE_GRANULARITY must be set to a multiple of FPakInfo::MaxChunkDataSize");
#define PAK_CACHE_MAX_REQUESTS (8)
#define PAK_CACHE_MAX_PRIORITY_DIFFERENCE_MERGE (AIOP_Normal - AIOP_MIN)
#define PAK_EXTRA_CHECKS DO_CHECK

DECLARE_MEMORY_STAT(TEXT("PakCache Current"), STAT_PakCacheMem, STATGROUP_Memory);
DECLARE_MEMORY_STAT(TEXT("PakCache High Water"), STAT_PakCacheHighWater, STATGROUP_Memory);

#if CSV_PROFILER
volatile int64 GPreCacheHotBlocksCount = 0;
volatile int64 GPreCacheColdBlocksCount = 0;
volatile int64 GPreCacheTotalLoaded = 0;
int64 GPreCacheTotalLoadedLastTick = 0;

volatile int64 GPreCacheSeeks = 0;
volatile int64 GPreCacheBadSeeks = 0;
volatile int64 GPreCacheContiguousReads = 0;
#endif

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("PakCache Signing Chunk Hash Time"), STAT_PakCache_SigningChunkHashTime, STATGROUP_PakFile);
DECLARE_MEMORY_STAT(TEXT("PakCache Signing Chunk Hash Size"), STAT_PakCache_SigningChunkHashSize, STATGROUP_PakFile);


static int32 GPakCache_Enable = 1;
static FAutoConsoleVariableRef CVar_Enable(
	TEXT("pakcache.Enable"),
	GPakCache_Enable,
	TEXT("If > 0, then enable the pak cache.")
);

int32 GPakCache_CachePerPakFile = 0;
static FAutoConsoleVariableRef CVar_CachePerPakFile(
	TEXT("pakcache.CachePerPakFile"),
	GPakCache_CachePerPakFile,
	TEXT("if > 0, then each pak file will have it's own cache")
);

int32 GPakCache_UseNewTrim = 0;
static FAutoConsoleVariableRef CVar_UseNewTrim(
	TEXT("pakcache.UseNewTrim"),
	GPakCache_UseNewTrim,
	TEXT("if > 0, then we'll use a round robin per pak file trim")
);

int32 GPakCache_MaxBlockMemory = 128;
static FAutoConsoleVariableRef CVar_MaxBlockMemory(
	TEXT("pakcache.MaxBlockMemory"),
	GPakCache_MaxBlockMemory,
	TEXT("A soft memory budget in MB for the max memory used for precaching, that we'll try and adhere to ")
);


int32 GPakCache_MaxRequestsToLowerLevel = 2;
static FAutoConsoleVariableRef CVar_MaxRequestsToLowerLevel(
	TEXT("pakcache.MaxRequestsToLowerLevel"),
	GPakCache_MaxRequestsToLowerLevel,
	TEXT("Controls the maximum number of IO requests submitted to the OS filesystem at one time. Limited by PAK_CACHE_MAX_REQUESTS.")
);

int32 GPakCache_MaxRequestSizeToLowerLevelKB = 1024;
static FAutoConsoleVariableRef CVar_MaxRequestSizeToLowerLevelKB(
	TEXT("pakcache.MaxRequestSizeToLowerLevellKB"),
	GPakCache_MaxRequestSizeToLowerLevelKB,
	TEXT("Controls the maximum size (in KB) of IO requests submitted to the OS filesystem.")
);

int32 GPakCache_NumUnreferencedBlocksToCache = 10;
static FAutoConsoleVariableRef CVar_NumUnreferencedBlocksToCache(
	TEXT("pakcache.NumUnreferencedBlocksToCache"),
	GPakCache_NumUnreferencedBlocksToCache,
	TEXT("Controls the maximum number of unreferenced blocks to keep. This is a classic disk cache and the maxmimum wasted memory is pakcache.MaxRequestSizeToLowerLevellKB * pakcache.NumUnreferencedBlocksToCache.")
);

float GPakCache_TimeToTrim = 0.0f;
static FAutoConsoleVariableRef CVar_PakCache_TimeToTrim(
	TEXT("pakcache.TimeToTrim"),
	GPakCache_TimeToTrim,
	TEXT("Controls how long to hold onto a cached but unreferenced block for.")
);

int32 GPakCache_EnableNoCaching = 0;
static FAutoConsoleVariableRef CVar_EnableNoCaching(
	TEXT("pakcache.EnableNoCaching"),
	GPakCache_EnableNoCaching,
	TEXT("if > 0, then we'll allow a read requests pak cache memory to be ditched early")
);


class FPakPrecacher;

typedef uint64 FJoinedOffsetAndPakIndex;
static FORCEINLINE uint16 GetRequestPakIndexLow(FJoinedOffsetAndPakIndex Joined)
{
	return uint16((Joined >> 48) & 0xffff);
}

static FORCEINLINE int64 GetRequestOffset(FJoinedOffsetAndPakIndex Joined)
{
	return int64(Joined & 0xffffffffffffll);
}

static FORCEINLINE FJoinedOffsetAndPakIndex MakeJoinedRequest(uint16 PakIndex, int64 Offset)
{
	check(Offset >= 0);
	return (FJoinedOffsetAndPakIndex(PakIndex) << 48) | Offset;
}

enum
{
	IntervalTreeInvalidIndex = 0
};


typedef uint32 TIntervalTreeIndex; // this is the arg type of TSparseArray::operator[]

static uint32 GNextSalt = 1;

// This is like TSparseArray, only a bit safer and I needed some restrictions on resizing.
template<class TItem>
class TIntervalTreeAllocator
{
	TArray<TItem> Items;
	TArray<int32> FreeItems; //@todo make this into a linked list through the existing items
	uint32 Salt;
	uint32 SaltMask;
public:
	TIntervalTreeAllocator()
	{
		check(GNextSalt < 4);
		Salt = (GNextSalt++) << 30;
		SaltMask = MAX_uint32 << 30;
		verify((Alloc() & ~SaltMask) == IntervalTreeInvalidIndex); // we want this to always have element zero so we can figure out an index from a pointer
	}
	inline TIntervalTreeIndex Alloc()
	{
		int32 Result;
		if (FreeItems.Num())
		{
			Result = FreeItems.Pop();
		}
		else
		{
			Result = Items.Num();
			Items.AddUninitialized();

		}
		new ((void*)&Items[Result]) TItem();
		return Result | Salt;;
	}
	void EnsureNoRealloc(int32 NeededNewNum)
	{
		if (FreeItems.Num() + Items.GetSlack() < NeededNewNum)
		{
			Items.Reserve(Items.Num() + NeededNewNum);
		}
	}
	FORCEINLINE TItem& Get(TIntervalTreeIndex InIndex)
	{
		TIntervalTreeIndex Index = InIndex & ~SaltMask;
		check((InIndex & SaltMask) == Salt && Index != IntervalTreeInvalidIndex && Index >= 0 && Index < (uint32)Items.Num()); //&& !FreeItems.Contains(Index));
		return Items[Index];
	}
	FORCEINLINE void Free(TIntervalTreeIndex InIndex)
	{
		TIntervalTreeIndex Index = InIndex & ~SaltMask;
		check((InIndex & SaltMask) == Salt && Index != IntervalTreeInvalidIndex && Index >= 0 && Index < (uint32)Items.Num()); //&& !FreeItems.Contains(Index));
		Items[Index].~TItem();
		FreeItems.Push(Index);
		if (FreeItems.Num() + 1 == Items.Num())
		{
			// get rid everything to restore memory coherence
			Items.Empty();
			FreeItems.Empty();
			verify((Alloc() & ~SaltMask) == IntervalTreeInvalidIndex); // we want this to always have element zero so we can figure out an index from a pointer
		}
	}
	FORCEINLINE void CheckIndex(TIntervalTreeIndex InIndex)
	{
		TIntervalTreeIndex Index = InIndex & ~SaltMask;
		check((InIndex & SaltMask) == Salt && Index != IntervalTreeInvalidIndex && Index >= 0 && Index < (uint32)Items.Num()); // && !FreeItems.Contains(Index));
	}
};

class FIntervalTreeNode
{
public:
	TIntervalTreeIndex LeftChildOrRootOfLeftList;
	TIntervalTreeIndex RootOfOnList;
	TIntervalTreeIndex RightChildOrRootOfRightList;

	FIntervalTreeNode()
		: LeftChildOrRootOfLeftList(IntervalTreeInvalidIndex)
		, RootOfOnList(IntervalTreeInvalidIndex)
		, RightChildOrRootOfRightList(IntervalTreeInvalidIndex)
	{
	}
	~FIntervalTreeNode()
	{
		check(LeftChildOrRootOfLeftList == IntervalTreeInvalidIndex && RootOfOnList == IntervalTreeInvalidIndex && RightChildOrRootOfRightList == IntervalTreeInvalidIndex); // this routine does not handle recursive destruction
	}
};

static TIntervalTreeAllocator<FIntervalTreeNode> GIntervalTreeNodeNodeAllocator;

static FORCEINLINE uint64 HighBit(uint64 x)
{
	return x & (1ull << 63);
}

static FORCEINLINE bool IntervalsIntersect(uint64 Min1, uint64 Max1, uint64 Min2, uint64 Max2)
{
	return !(Max2 < Min1 || Max1 < Min2);
}

template<typename TItem>
// this routine assume that the pointers remain valid even though we are reallocating
static void AddToIntervalTree_Dangerous(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint32 CurrentShift,
	uint32 MaxShift
)
{
	while (true)
	{
		if (*RootNode == IntervalTreeInvalidIndex)
		{
			*RootNode = GIntervalTreeNodeNodeAllocator.Alloc();
		}

		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(*RootNode);

		if (MinShifted == MaxShifted && CurrentShift < MaxShift)
		{
			CurrentShift++;
			RootNode = (!MinShifted) ? &Root.LeftChildOrRootOfLeftList : &Root.RightChildOrRootOfRightList;
		}
		else
		{
			TItem& Item = Allocator.Get(Index);
			if (MinShifted != MaxShifted) // crosses middle
			{
				Item.Next = Root.RootOfOnList;
				Root.RootOfOnList = Index;
			}
			else // we are at the leaf
			{
				if (!MinShifted)
				{
					Item.Next = Root.LeftChildOrRootOfLeftList;
					Root.LeftChildOrRootOfLeftList = Index;
				}
				else
				{
					Item.Next = Root.RightChildOrRootOfRightList;
					Root.RightChildOrRootOfRightList = Index;
				}
			}
			return;
		}
	}
}

template<typename TItem>
static void AddToIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint32 StartShift,
	uint32 MaxShift
)
{
	GIntervalTreeNodeNodeAllocator.EnsureNoRealloc(1 + MaxShift - StartShift);
	TItem& Item = Allocator.Get(Index);
	check(Item.Next == IntervalTreeInvalidIndex);
	uint64 MinInterval = GetRequestOffset(Item.OffsetAndPakIndex);
	uint64 MaxInterval = MinInterval + Item.Size - 1;
	AddToIntervalTree_Dangerous(RootNode, Allocator, Index, MinInterval, MaxInterval, StartShift, MaxShift);

}

template<typename TItem>
static FORCEINLINE bool ScanNodeListForRemoval(
	TIntervalTreeIndex* Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint64 MinInterval,
	uint64 MaxInterval
)
{
	while (*Iter != IntervalTreeInvalidIndex)
	{

		TItem& Item = Allocator.Get(*Iter);
		if (*Iter == Index)
		{
			*Iter = Item.Next;
			Item.Next = IntervalTreeInvalidIndex;
			return true;
		}
		Iter = &Item.Next;
	}
	return false;
}

template<typename TItem>
static bool RemoveFromIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint32 CurrentShift,
	uint32 MaxShift
)
{
	bool bResult = false;
	if (*RootNode != IntervalTreeInvalidIndex)
	{
		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(*RootNode);

		if (!MinShifted && !MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				bResult = ScanNodeListForRemoval(&Root.LeftChildOrRootOfLeftList, Allocator, Index, MinInterval, MaxInterval);
			}
			else
			{
				bResult = RemoveFromIntervalTree(&Root.LeftChildOrRootOfLeftList, Allocator, Index, MinInterval, MaxInterval, CurrentShift + 1, MaxShift);
			}
		}
		else if (!MinShifted && MaxShifted)
		{
			bResult = ScanNodeListForRemoval(&Root.RootOfOnList, Allocator, Index, MinInterval, MaxInterval);
		}
		else
		{
			if (CurrentShift == MaxShift)
			{
				bResult = ScanNodeListForRemoval(&Root.RightChildOrRootOfRightList, Allocator, Index, MinInterval, MaxInterval);
			}
			else
			{
				bResult = RemoveFromIntervalTree(&Root.RightChildOrRootOfRightList, Allocator, Index, MinInterval, MaxInterval, CurrentShift + 1, MaxShift);
			}
		}
		if (bResult)
		{
			if (Root.LeftChildOrRootOfLeftList == IntervalTreeInvalidIndex && Root.RootOfOnList == IntervalTreeInvalidIndex && Root.RightChildOrRootOfRightList == IntervalTreeInvalidIndex)
			{
				check(&Root == &GIntervalTreeNodeNodeAllocator.Get(*RootNode));
				GIntervalTreeNodeNodeAllocator.Free(*RootNode);
				*RootNode = IntervalTreeInvalidIndex;
			}
		}
	}
	return bResult;
}

template<typename TItem>
static bool RemoveFromIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	TIntervalTreeIndex Index,
	uint32 StartShift,
	uint32 MaxShift
)
{
	TItem& Item = Allocator.Get(Index);
	uint64 MinInterval = GetRequestOffset(Item.OffsetAndPakIndex);
	uint64 MaxInterval = MinInterval + Item.Size - 1;
	return RemoveFromIntervalTree(RootNode, Allocator, Index, MinInterval, MaxInterval, StartShift, MaxShift);
}

template<typename TItem>
static FORCEINLINE void ScanNodeListForRemovalFunc(
	TIntervalTreeIndex* Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
)
{
	while (*Iter != IntervalTreeInvalidIndex)
	{
		TItem& Item = Allocator.Get(*Iter);
		uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
		uint64 LastByte = Offset + uint64(Item.Size) - 1;

		// save the value and then clear it.
		TIntervalTreeIndex NextIndex = Item.Next;
		if (IntervalsIntersect(MinInterval, MaxInterval, Offset, LastByte) && Func(*Iter))
		{
			*Iter = NextIndex; // this may have already be deleted, so cannot rely on the memory block
		}
		else
		{
			Iter = &Item.Next;
		}
	}
}

template<typename TItem>
static void MaybeRemoveOverlappingNodesInIntervalTree(
	TIntervalTreeIndex* RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
)
{
	if (*RootNode != IntervalTreeInvalidIndex)
	{
		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(*RootNode);
		uint64 Center = (MinNode + MaxNode + 1) >> 1;

		//UE_LOG(LogTemp, Warning, TEXT("Exploring Node %X [%d, %d] %d%d     interval %llX %llX    node interval %llX %llX   center %llX  "), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted, MinInterval, MaxInterval, MinNode, MaxNode, Center);


		if (!MinShifted)
		{
			if (CurrentShift == MaxShift)
			{
				//UE_LOG(LogTemp, Warning, TEXT("LeftBottom %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				ScanNodeListForRemovalFunc(&Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, Func);
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("LeftRecur %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				MaybeRemoveOverlappingNodesInIntervalTree(&Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, FMath::Min(MaxInterval, Center - 1), MinNode, Center - 1, CurrentShift + 1, MaxShift, Func);
			}
		}

		//UE_LOG(LogTemp, Warning, TEXT("Center %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
		ScanNodeListForRemovalFunc(&Root.RootOfOnList, Allocator, MinInterval, MaxInterval, Func);

		if (MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				//UE_LOG(LogTemp, Warning, TEXT("RightBottom %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				ScanNodeListForRemovalFunc(&Root.RightChildOrRootOfRightList, Allocator, MinInterval, MaxInterval, Func);
			}
			else
			{
				//UE_LOG(LogTemp, Warning, TEXT("RightRecur %X [%d, %d] %d%d"), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted);
				MaybeRemoveOverlappingNodesInIntervalTree(&Root.RightChildOrRootOfRightList, Allocator, FMath::Max(MinInterval, Center), MaxInterval, Center, MaxNode, CurrentShift + 1, MaxShift, Func);
			}
		}

		//UE_LOG(LogTemp, Warning, TEXT("Done Exploring Node %X [%d, %d] %d%d     interval %llX %llX    node interval %llX %llX   center %llX  "), *RootNode, CurrentShift, MaxShift, !!MinShifted, !!MaxShifted, MinInterval, MaxInterval, MinNode, MaxNode, Center);

		if (Root.LeftChildOrRootOfLeftList == IntervalTreeInvalidIndex && Root.RootOfOnList == IntervalTreeInvalidIndex && Root.RightChildOrRootOfRightList == IntervalTreeInvalidIndex)
		{
			check(&Root == &GIntervalTreeNodeNodeAllocator.Get(*RootNode));
			GIntervalTreeNodeNodeAllocator.Free(*RootNode);
			*RootNode = IntervalTreeInvalidIndex;
		}
	}
}


template<typename TItem>
static FORCEINLINE bool ScanNodeList(
	TIntervalTreeIndex Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
)
{
	while (Iter != IntervalTreeInvalidIndex)
	{
		TItem& Item = Allocator.Get(Iter);
		uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
		uint64 LastByte = Offset + uint64(Item.Size) - 1;
		if (IntervalsIntersect(MinInterval, MaxInterval, Offset, LastByte))
		{
			if (!Func(Iter))
			{
				return false;
			}
		}
		Iter = Item.Next;
	}
	return true;
}

template<typename TItem>
static bool OverlappingNodesInIntervalTree(
	TIntervalTreeIndex RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
)
{
	if (RootNode != IntervalTreeInvalidIndex)
	{
		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(MaxInterval << CurrentShift);
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(RootNode);
		uint64 Center = (MinNode + MaxNode + 1) >> 1;

		if (!MinShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeList(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTree(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, FMath::Min(MaxInterval, Center - 1), MinNode, Center - 1, CurrentShift + 1, MaxShift, Func))
				{
					return false;
				}
			}
		}
		if (!ScanNodeList(Root.RootOfOnList, Allocator, MinInterval, MaxInterval, Func))
		{
			return false;
		}
		if (MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeList(Root.RightChildOrRootOfRightList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTree(Root.RightChildOrRootOfRightList, Allocator, FMath::Max(MinInterval, Center), MaxInterval, Center, MaxNode, CurrentShift + 1, MaxShift, Func))
				{
					return false;
				}
			}
		}
	}
	return true;
}

template<typename TItem>
static bool ScanNodeListWithShrinkingInterval(
	TIntervalTreeIndex Iter,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64& MaxInterval,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
)
{
	while (Iter != IntervalTreeInvalidIndex)
	{
		TItem& Item = Allocator.Get(Iter);
		uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
		uint64 LastByte = Offset + uint64(Item.Size) - 1;
		//UE_LOG(LogTemp, Warning, TEXT("Test Overlap %llu %llu %llu %llu"), MinInterval, MaxInterval, Offset, LastByte);
		if (IntervalsIntersect(MinInterval, MaxInterval, Offset, LastByte))
		{
			//UE_LOG(LogTemp, Warning, TEXT("Overlap %llu %llu %llu %llu"), MinInterval, MaxInterval, Offset, LastByte);
			if (!Func(Iter))
			{
				return false;
			}
		}
		Iter = Item.Next;
	}
	return true;
}

template<typename TItem>
static bool OverlappingNodesInIntervalTreeWithShrinkingInterval(
	TIntervalTreeIndex RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64& MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	TFunctionRef<bool(TIntervalTreeIndex)> Func
)
{
	if (RootNode != IntervalTreeInvalidIndex)
	{

		int64 MinShifted = HighBit(MinInterval << CurrentShift);
		int64 MaxShifted = HighBit(FMath::Min(MaxInterval, MaxNode) << CurrentShift); // since MaxInterval is changing, we cannot clamp it during recursion.
		FIntervalTreeNode& Root = GIntervalTreeNodeNodeAllocator.Get(RootNode);
		uint64 Center = (MinNode + MaxNode + 1) >> 1;

		if (!MinShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeListWithShrinkingInterval(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTreeWithShrinkingInterval(Root.LeftChildOrRootOfLeftList, Allocator, MinInterval, MaxInterval, MinNode, Center - 1, CurrentShift + 1, MaxShift, Func)) // since MaxInterval is changing, we cannot clamp it during recursion.
				{
					return false;
				}
			}
		}
		if (!ScanNodeListWithShrinkingInterval(Root.RootOfOnList, Allocator, MinInterval, MaxInterval, Func))
		{
			return false;
		}
		MaxShifted = HighBit(FMath::Min(MaxInterval, MaxNode) << CurrentShift); // since MaxInterval is changing, we cannot clamp it during recursion.
		if (MaxShifted)
		{
			if (CurrentShift == MaxShift)
			{
				if (!ScanNodeListWithShrinkingInterval(Root.RightChildOrRootOfRightList, Allocator, MinInterval, MaxInterval, Func))
				{
					return false;
				}
			}
			else
			{
				if (!OverlappingNodesInIntervalTreeWithShrinkingInterval(Root.RightChildOrRootOfRightList, Allocator, FMath::Max(MinInterval, Center), MaxInterval, Center, MaxNode, CurrentShift + 1, MaxShift, Func))
				{
					return false;
				}
			}
		}
	}
	return true;
}


template<typename TItem>
static void MaskInterval(
	TIntervalTreeIndex Index,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint32 BytesToBitsShift,
	uint64* Bits
)
{
	TItem& Item = Allocator.Get(Index);
	uint64 Offset = uint64(GetRequestOffset(Item.OffsetAndPakIndex));
	uint64 LastByte = Offset + uint64(Item.Size) - 1;
	uint64 InterMinInterval = FMath::Max(MinInterval, Offset);
	uint64 InterMaxInterval = FMath::Min(MaxInterval, LastByte);
	if (InterMinInterval <= InterMaxInterval)
	{
		uint32 FirstBit = uint32((InterMinInterval - MinInterval) >> BytesToBitsShift);
		uint32 LastBit = uint32((InterMaxInterval - MinInterval) >> BytesToBitsShift);
		uint32 FirstQWord = FirstBit >> 6;
		uint32 LastQWord = LastBit >> 6;
		uint32 FirstBitQWord = FirstBit & 63;
		uint32 LastBitQWord = LastBit & 63;
		if (FirstQWord == LastQWord)
		{
			Bits[FirstQWord] |= ((MAX_uint64 << FirstBitQWord) & (MAX_uint64 >> (63 - LastBitQWord)));
		}
		else
		{
			Bits[FirstQWord] |= (MAX_uint64 << FirstBitQWord);
			for (uint32 QWordIndex = FirstQWord + 1; QWordIndex < LastQWord; QWordIndex++)
			{
				Bits[QWordIndex] = MAX_uint64;
			}
			Bits[LastQWord] |= (MAX_uint64 >> (63 - LastBitQWord));
		}
	}
}



template<typename TItem>
static void OverlappingNodesInIntervalTreeMask(
	TIntervalTreeIndex RootNode,
	TIntervalTreeAllocator<TItem>& Allocator,
	uint64 MinInterval,
	uint64 MaxInterval,
	uint64 MinNode,
	uint64 MaxNode,
	uint32 CurrentShift,
	uint32 MaxShift,
	uint32 BytesToBitsShift,
	uint64* Bits
)
{
	OverlappingNodesInIntervalTree(
		RootNode,
		Allocator,
		MinInterval,
		MaxInterval,
		MinNode,
		MaxNode,
		CurrentShift,
		MaxShift,
		[&Allocator, MinInterval, MaxInterval, BytesToBitsShift, Bits](TIntervalTreeIndex Index) -> bool
	{
		MaskInterval(Index, Allocator, MinInterval, MaxInterval, BytesToBitsShift, Bits);
		return true;
	}
	);
}



class IPakRequestor
{
	friend class FPakPrecacher;
	FJoinedOffsetAndPakIndex OffsetAndPakIndex; // this is used for searching and filled in when you make the request
	uint64 UniqueID;
	TIntervalTreeIndex InRequestIndex;
public:
	IPakRequestor()
		: OffsetAndPakIndex(MAX_uint64) // invalid value
		, UniqueID(0)
		, InRequestIndex(IntervalTreeInvalidIndex)
	{
	}
	virtual ~IPakRequestor()
	{
	}
	virtual void RequestIsComplete()
	{
	}
};

static FPakPrecacher* PakPrecacherSingleton = nullptr;

class FPakPrecacher
{
	enum class EInRequestStatus
	{
		Complete,
		Waiting,
		InFlight,
		Num
	};

	enum class EBlockStatus
	{
		InFlight,
		Complete,
		Num
	};

	IPlatformFile* LowerLevel;
	FCriticalSection CachedFilesScopeLock;
	FJoinedOffsetAndPakIndex LastReadRequest;
	uint64 NextUniqueID;
	int64 BlockMemory;
	int64 BlockMemoryHighWater;
	FThreadSafeCounter RequestCounter;

	struct FCacheBlock
	{
		FJoinedOffsetAndPakIndex OffsetAndPakIndex;
		int64 Size;
		uint8 *Memory;
		uint32 InRequestRefCount;
		TIntervalTreeIndex Index;
		TIntervalTreeIndex Next;
		EBlockStatus Status;
		double TimeNoLongerReferenced;

		FCacheBlock()
			: OffsetAndPakIndex(0)
			, Size(0)
			, Memory(nullptr)
			, InRequestRefCount(0)
			, Index(IntervalTreeInvalidIndex)
			, Next(IntervalTreeInvalidIndex)
			, Status(EBlockStatus::InFlight)
			, TimeNoLongerReferenced(0)
		{
		}
	};

	struct FPakInRequest
	{
		FJoinedOffsetAndPakIndex OffsetAndPakIndex;
		int64 Size;
		IPakRequestor* Owner;
		uint64 UniqueID;
		TIntervalTreeIndex Index;
		TIntervalTreeIndex Next;
		EAsyncIOPriorityAndFlags PriorityAndFlags;
		EInRequestStatus Status;

		FPakInRequest()
			: OffsetAndPakIndex(0)
			, Size(0)
			, Owner(nullptr)
			, UniqueID(0)
			, Index(IntervalTreeInvalidIndex)
			, Next(IntervalTreeInvalidIndex)
			, PriorityAndFlags(AIOP_MIN)
			, Status(EInRequestStatus::Waiting)
		{
		}

		EAsyncIOPriorityAndFlags GetPriority() const
		{
			return PriorityAndFlags & AIOP_PRIORITY_MASK;
		}
	};

	struct FPakData
	{
		IAsyncReadFileHandle* Handle;
		FPakFile* ActualPakFile;
		int64 TotalSize;
		uint64 MaxNode;
		uint32 StartShift;
		uint32 MaxShift;
		uint32 BytesToBitsShift;
		FName Name;

		TIntervalTreeIndex InRequests[AIOP_NUM][(int32)EInRequestStatus::Num];
		TIntervalTreeIndex CacheBlocks[(int32)EBlockStatus::Num];

		TSharedPtr<const FPakSignatureFile, ESPMode::ThreadSafe> Signatures;

		FPakData(FPakFile* InActualPakFile, IAsyncReadFileHandle* InHandle, FName InName, int64 InTotalSize)
			: Handle(InHandle)
			, ActualPakFile(InActualPakFile)
			, TotalSize(InTotalSize)
			, StartShift(0)
			, MaxShift(0)
			, BytesToBitsShift(0)
			, Name(InName)
			, Signatures(nullptr)
		{
			check(Handle && TotalSize > 0 && Name != NAME_None);
			for (int32 Index = 0; Index < AIOP_NUM; Index++)
			{
				for (int32 IndexInner = 0; IndexInner < (int32)EInRequestStatus::Num; IndexInner++)
				{
					InRequests[Index][IndexInner] = IntervalTreeInvalidIndex;
				}
			}
			for (int32 IndexInner = 0; IndexInner < (int32)EBlockStatus::Num; IndexInner++)
			{
				CacheBlocks[IndexInner] = IntervalTreeInvalidIndex;
			}
			uint64 StartingLastByte = FMath::Max((uint64)TotalSize, uint64(PAK_CACHE_GRANULARITY + 1));
			StartingLastByte--;

			{
				uint64 LastByte = StartingLastByte;
				while (!HighBit(LastByte))
				{
					LastByte <<= 1;
					StartShift++;
				}
			}
			{
				uint64 LastByte = StartingLastByte;
				uint64 Block = (uint64)PAK_CACHE_GRANULARITY;

				while (Block)
				{
					Block >>= 1;
					LastByte >>= 1;
					BytesToBitsShift++;
				}
				BytesToBitsShift--;
				check(1 << BytesToBitsShift == PAK_CACHE_GRANULARITY);
				MaxShift = StartShift;
				while (LastByte)
				{
					LastByte >>= 1;
					MaxShift++;
				}
				MaxNode = MAX_uint64 >> StartShift;
				check(MaxNode >= StartingLastByte && (MaxNode >> 1) < StartingLastByte);
				//				UE_LOG(LogTemp, Warning, TEXT("Test %d %llX %llX "), MaxShift, (uint64(PAK_CACHE_GRANULARITY) << (MaxShift + 1)), (uint64(PAK_CACHE_GRANULARITY) << MaxShift));
				check(MaxShift && (uint64(PAK_CACHE_GRANULARITY) << (MaxShift + 1)) == 0 && (uint64(PAK_CACHE_GRANULARITY) << MaxShift) != 0);
			}
		}
	};
	TMap<FPakFile*, uint16> CachedPaks;
	TArray<FPakData> CachedPakData;

	TIntervalTreeAllocator<FPakInRequest> InRequestAllocator;
	TIntervalTreeAllocator<FCacheBlock> CacheBlockAllocator;
	TMap<uint64, TIntervalTreeIndex> OutstandingRequests;

	TArray < TArray<FJoinedOffsetAndPakIndex>> OffsetAndPakIndexOfSavedBlocked;

	struct FRequestToLower
	{
		IAsyncReadRequest* RequestHandle;
		TIntervalTreeIndex BlockIndex;
		int64 RequestSize;
		uint8* Memory;
		FRequestToLower()
			: RequestHandle(nullptr)
			, BlockIndex(IntervalTreeInvalidIndex)
			, RequestSize(0)
			, Memory(nullptr)
		{
		}
	};

	FRequestToLower RequestsToLower[PAK_CACHE_MAX_REQUESTS];
	TArray<IAsyncReadRequest*> RequestsToDelete;
	int32 NotifyRecursion;

	uint32 Loads;
	uint32 Frees;
	uint64 LoadSize;
	EAsyncIOPriorityAndFlags AsyncMinPriority;
	FCriticalSection SetAsyncMinimumPriorityScopeLock;
	bool bEnableSignatureChecks;
public:
	int64 GetBlockMemory() { return BlockMemory; }
	int64 GetBlockMemoryHighWater() { return BlockMemoryHighWater; }

	static void Init(IPlatformFile* InLowerLevel, bool bInEnableSignatureChecks) 
	{
		if (!PakPrecacherSingleton)
		{
			verify(!FPlatformAtomics::InterlockedCompareExchangePointer((void**)& PakPrecacherSingleton, new FPakPrecacher(InLowerLevel, bInEnableSignatureChecks), nullptr));
		}
		check(PakPrecacherSingleton);
	}

	static void Shutdown()
	{
		if (PakPrecacherSingleton)
		{
			FPakPrecacher* LocalPakPrecacherSingleton = PakPrecacherSingleton;
			if (LocalPakPrecacherSingleton && LocalPakPrecacherSingleton == FPlatformAtomics::InterlockedCompareExchangePointer((void**)&PakPrecacherSingleton, nullptr, LocalPakPrecacherSingleton))
			{
				LocalPakPrecacherSingleton->TrimCache(true);
				double StartTime = FPlatformTime::Seconds();
				while (!LocalPakPrecacherSingleton->IsProbablyIdle())
				{
					FPlatformProcess::SleepNoStats(0.001f);
					if (FPlatformTime::Seconds() - StartTime > 10.0)
					{
						UE_LOG(LogPakFile, Error, TEXT("FPakPrecacher was not idle after 10s, exiting anyway and leaking."));
						return;
					}
				}
				delete PakPrecacherSingleton;
				PakPrecacherSingleton = nullptr;
			}
		}
		check(!PakPrecacherSingleton);
	}

	static FPakPrecacher& Get()
	{
		check(PakPrecacherSingleton);
		return *PakPrecacherSingleton;
	}

	FPakPrecacher(IPlatformFile* InLowerLevel, bool bInEnableSignatureChecks) 
		: LowerLevel(InLowerLevel)
		, LastReadRequest(0)
		, NextUniqueID(1)
		, BlockMemory(0)
		, BlockMemoryHighWater(0)
		, NotifyRecursion(0)
		, Loads(0)
		, Frees(0)
		, LoadSize(0)
		, AsyncMinPriority(AIOP_MIN)
		, bEnableSignatureChecks(bInEnableSignatureChecks)
	{
		check(LowerLevel && FPlatformProcess::SupportsMultithreading());
//		GPakCache_MaxRequestsToLowerLevel = FMath::Max(FMath::Min(FPlatformMisc::NumberOfIOWorkerThreadsToSpawn(), GPakCache_MaxRequestsToLowerLevel), 1);
		check(GPakCache_MaxRequestsToLowerLevel <= PAK_CACHE_MAX_REQUESTS);
	}

	void StartSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 IndexToFill);
	void DoSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 IndexToFill);

	int32 GetRequestCount() const
	{
		return RequestCounter.GetValue();
	}

	IPlatformFile* GetLowerLevelHandle()
	{
		check(LowerLevel);
		return LowerLevel;
	}

	uint16* RegisterPakFile(FPakFile* InActualPakFile, FName File, int64 PakFileSize)
	{
		// CachedFilesScopeLock is locked
		uint16* PakIndexPtr = CachedPaks.Find(InActualPakFile);

		if (!PakIndexPtr)
		{
			if (!InActualPakFile->GetIsMounted())
			{
				// The PakFile was unmounted already; reject the read. If we added it now we would have a dangling PakFile pointer in CachedPaks
				// and would never be notified to remove it.
				return nullptr;
			}
			FString PakFilename = File.ToString();
			check(CachedPakData.Num() < MAX_uint16);
			IAsyncReadFileHandle* Handle = LowerLevel->OpenAsyncRead(*PakFilename);
			if (!Handle)
			{
				return nullptr;
			}
			CachedPakData.Add(FPakData(InActualPakFile, Handle, File, PakFileSize));
			PakIndexPtr = &CachedPaks.Add(InActualPakFile, static_cast<uint16>(CachedPakData.Num() - 1));
			FPakData& Pak = CachedPakData[*PakIndexPtr];


			if (OffsetAndPakIndexOfSavedBlocked.Num() == 0)
			{
				// the 1st cache must exist and is used by all sharing pak files
				OffsetAndPakIndexOfSavedBlocked.AddDefaulted(1);
			}

			static bool bFirst = true;
			if (bFirst)
			{
				if (FParse::Param(FCommandLine::Get(), TEXT("CachePerPak")))
				{
					GPakCache_CachePerPakFile = 1;
				}

				if (FParse::Param(FCommandLine::Get(), TEXT("NewTrimCache")))
				{
					GPakCache_UseNewTrim = 1;
				}
				FParse::Value(FCommandLine::Get(), TEXT("PakCacheMaxBlockMemory="), GPakCache_MaxBlockMemory);
				bFirst = false;
			}

			if (Pak.ActualPakFile->GetCacheType() == FPakFile::ECacheType::Individual || GPakCache_CachePerPakFile != 0 )
			{
				Pak.ActualPakFile->SetCacheIndex(OffsetAndPakIndexOfSavedBlocked.Num());
				OffsetAndPakIndexOfSavedBlocked.AddDefaulted(1);
			}
			else
			{
				Pak.ActualPakFile->SetCacheIndex(0);
			}

			UE_LOG(LogPakFile, Log, TEXT("New pak file %s added to pak precacher."), *PakFilename);

			// Load signature data
			Pak.Signatures = FPakPlatformFile::GetPakSignatureFile(*PakFilename);

			if (Pak.Signatures.IsValid())
			{
				// We should never get here unless the signature file exists and is validated. The original FPakFile creation
				// on the main thread would have failed and the pak would never have been mounted otherwise, and then we would
				// never have issued read requests to the pak precacher.
				check(Pak.Signatures);
				
				// Check that we have the correct match between signature and pre-cache granularity
				int64 NumPakChunks = Align(PakFileSize, FPakInfo::MaxChunkDataSize) / FPakInfo::MaxChunkDataSize;
				ensure(NumPakChunks == Pak.Signatures->ChunkHashes.Num());
			}
		}
		return PakIndexPtr;
	}

#if !UE_BUILD_SHIPPING
	void SimulatePakFileCorruption()
	{
		FScopeLock Lock(&CachedFilesScopeLock);

		for (FPakData& PakData : CachedPakData)
		{
			for (const TPakChunkHash& Hash : PakData.Signatures->ChunkHashes)
			{
				*((uint8*)&Hash) |= 0x1;
			}
		}
	}
#endif

private: // below here we assume CachedFilesScopeLock until we get to the next section

	uint16 GetRequestPakIndex(FJoinedOffsetAndPakIndex OffsetAndPakIndex)
	{
		uint16 Result = GetRequestPakIndexLow(OffsetAndPakIndex);
		check(Result < CachedPakData.Num());
		return Result;
	}

	FJoinedOffsetAndPakIndex FirstUnfilledBlockForRequest(TIntervalTreeIndex NewIndex, FJoinedOffsetAndPakIndex ReadHead = 0)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(NewIndex);
		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		int64 Size = Request.Size;
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Size > 0 && Request.GetPriority() >= AIOP_MIN && Request.GetPriority() <= AIOP_MAX && Request.Status != EInRequestStatus::Complete && Request.Owner);
		if (PakIndex != GetRequestPakIndex(ReadHead))
		{
			// this is in a different pak, so we ignore the read head position
			ReadHead = 0;
		}
		if (ReadHead)
		{
			// trim to the right of the read head
			int64 Trim = FMath::Max(Offset, GetRequestOffset(ReadHead)) - Offset;
			Offset += Trim;
			Size -= Trim;
		}

		static TArray<uint64> InFlightOrDone;

		int64 FirstByte = AlignDown(Offset, PAK_CACHE_GRANULARITY);
		int64 LastByte = Align(Offset + Size, PAK_CACHE_GRANULARITY) - 1;
		uint32 NumBits = IntCastChecked<uint32>((PAK_CACHE_GRANULARITY + LastByte - FirstByte) / PAK_CACHE_GRANULARITY);
		uint32 NumQWords = (NumBits + 63) >> 6;
		InFlightOrDone.Reset();
		InFlightOrDone.AddZeroed(NumQWords);
		if (NumBits != NumQWords * 64)
		{
			uint32 Extras = NumQWords * 64 - NumBits;
			InFlightOrDone[NumQWords - 1] = (MAX_uint64 << (64 - Extras));
		}

		if (Pak.CacheBlocks[(int32)EBlockStatus::Complete] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}
		if (Request.Status == EInRequestStatus::Waiting && Pak.CacheBlocks[(int32)EBlockStatus::InFlight] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}
		for (uint32 Index = 0; Index < NumQWords; Index++)
		{
			if (InFlightOrDone[Index] != MAX_uint64)
			{
				uint64 Mask = InFlightOrDone[Index];
				int64 FinalOffset = FirstByte + PAK_CACHE_GRANULARITY * 64 * Index;
				while (Mask & 1)
				{
					FinalOffset += PAK_CACHE_GRANULARITY;
					Mask >>= 1;
				}
				return MakeJoinedRequest(PakIndex, FinalOffset);
			}
		}
		return MAX_uint64;
	}

	bool AddRequest(FPakInRequest& Request, TIntervalTreeIndex NewIndex)
	{
		// CachedFilesScopeLock is locked
		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Request.Size > 0 && Request.GetPriority() >= AIOP_MIN && Request.GetPriority() <= AIOP_MAX && Request.Status == EInRequestStatus::Waiting && Request.Owner);

		static TArray<uint64> InFlightOrDone;

		int64 FirstByte = AlignDown(Offset, PAK_CACHE_GRANULARITY);
		int64 LastByte = Align(Offset + Request.Size, PAK_CACHE_GRANULARITY) - 1;
		uint32 NumBits = IntCastChecked<uint32>((PAK_CACHE_GRANULARITY + LastByte - FirstByte) / PAK_CACHE_GRANULARITY);
		uint32 NumQWords = (NumBits + 63) >> 6;
		InFlightOrDone.Reset();
		InFlightOrDone.AddZeroed(NumQWords);
		if (NumBits != NumQWords * 64)
		{
			uint32 Extras = NumQWords * 64 - NumBits;
			InFlightOrDone[NumQWords - 1] = (MAX_uint64 << (64 - Extras));
		}

		if (Pak.CacheBlocks[(int32)EBlockStatus::Complete] != IntervalTreeInvalidIndex)
		{
			Request.Status = EInRequestStatus::Complete;
			OverlappingNodesInIntervalTree<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this, &Pak, FirstByte, LastByte](TIntervalTreeIndex Index) -> bool
			{
				CacheBlockAllocator.Get(Index).InRequestRefCount++;
				MaskInterval(Index, CacheBlockAllocator, FirstByte, LastByte, Pak.BytesToBitsShift, &InFlightOrDone[0]);
				return true;
			}
			);
			for (uint32 Index = 0; Index < NumQWords; Index++)
			{
				if (InFlightOrDone[Index] != MAX_uint64)
				{
					Request.Status = EInRequestStatus::Waiting;
					break;
				}
			}
		}

		if (Request.Status == EInRequestStatus::Waiting)
		{
			if (Pak.CacheBlocks[(int32)EBlockStatus::InFlight] != IntervalTreeInvalidIndex)
			{
				Request.Status = EInRequestStatus::InFlight;
				OverlappingNodesInIntervalTree<FCacheBlock>(
					Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
					CacheBlockAllocator,
					FirstByte,
					LastByte,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[this, &Pak, FirstByte, LastByte](TIntervalTreeIndex Index) -> bool
				{
					CacheBlockAllocator.Get(Index).InRequestRefCount++;
					MaskInterval(Index, CacheBlockAllocator, FirstByte, LastByte, Pak.BytesToBitsShift, &InFlightOrDone[0]);
					return true;
				}
				);

				for (uint32 Index = 0; Index < NumQWords; Index++)
				{
					if (InFlightOrDone[Index] != MAX_uint64)
					{
						Request.Status = EInRequestStatus::Waiting;
						break;
					}
				}
			}
		}
		else
		{
#if PAK_EXTRA_CHECKS
			OverlappingNodesInIntervalTree<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this, &Pak, FirstByte, LastByte](TIntervalTreeIndex Index) -> bool
			{
				check(0); // if we are complete, then how come there are overlapping in flight blocks?
				return true;
			}
			);
#endif
		}
		{
			AddToIntervalTree<FPakInRequest>(
				&Pak.InRequests[Request.GetPriority()][(int32)Request.Status],
				InRequestAllocator,
				NewIndex,
				Pak.StartShift,
				Pak.MaxShift
				);
		}
		check(&Request == &InRequestAllocator.Get(NewIndex));
		if (Request.Status == EInRequestStatus::Complete)
		{
			NotifyComplete(NewIndex);
			return true;
		}
		else if (Request.Status == EInRequestStatus::Waiting)
		{
			StartNextRequest();
		}
		return false;
	}

	void ClearBlock(FCacheBlock &Block)
	{
		UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) ClearBlock"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);

		if (Block.Memory)
		{
			check(Block.Size);
			BlockMemory -= Block.Size;
			DEC_MEMORY_STAT_BY(STAT_PakCacheMem, Block.Size);
			check(BlockMemory >= 0);

			FMemory::Free(Block.Memory);
			Block.Memory = nullptr;
		}
		Block.Next = IntervalTreeInvalidIndex;
		CacheBlockAllocator.Free(Block.Index);
	}

	void ClearRequest(FPakInRequest& DoneRequest)
	{
		uint64 Id = DoneRequest.UniqueID;
		TIntervalTreeIndex Index = DoneRequest.Index;

		DoneRequest.OffsetAndPakIndex = 0;
		DoneRequest.Size = 0;
		DoneRequest.Owner = nullptr;
		DoneRequest.UniqueID = 0;
		DoneRequest.Index = IntervalTreeInvalidIndex;
		DoneRequest.Next = IntervalTreeInvalidIndex;
		DoneRequest.PriorityAndFlags = AIOP_MIN;
		DoneRequest.Status = EInRequestStatus::Num;

		verify(OutstandingRequests.Remove(Id) == 1);
		RequestCounter.Decrement();
		InRequestAllocator.Free(Index);
	}

	void TrimCache(bool bDiscardAll = false, uint16 StartPakIndex = 65535)
	{

		if (GPakCache_UseNewTrim && !bDiscardAll)
		{
			StartPakIndex = 0;
			uint16 EndPakIndex = IntCastChecked<uint16>(CachedPakData.Num());

			// TODO: remove this array, add a bool to the cache object
			TArray<bool> CacheVisitedAlready;
			CacheVisitedAlready.AddDefaulted(OffsetAndPakIndexOfSavedBlocked.Num());

			int64 MemoryBudget = GPakCache_MaxBlockMemory * (1024 * 1024);
			bool AlreadyRemovedBlocksBecauseOfMemoryOverage = false;

			while (BlockMemory > MemoryBudget)
			{
				for (int i = 0; i < CacheVisitedAlready.Num(); i++)
				{
					CacheVisitedAlready[i] = false;
				}
				// if we iterate over all the pak files and caches and can't remove something then we'll break out of the while.
				bool NoneToRemove = true;
				// CachedFilesScopeLock is locked
				for (uint16 RealPakIndex = StartPakIndex; RealPakIndex < EndPakIndex; RealPakIndex++)
				{
					if (!CachedPakData[RealPakIndex].Handle)
					{
						// This PakData has been unmounted and is no longer a valid entry
						continue;
					}
					int32 CacheIndex = CachedPakData[RealPakIndex].ActualPakFile->GetCacheIndex();
					if (CacheIndex < 0 || OffsetAndPakIndexOfSavedBlocked.Num() <= CacheIndex)
					{
						// It appears that rare crashes in shipped builds can hit this case.
						// Without the CacheIndex we will no longer be able to trim the cache. This isn't a problem if the PakFile
						// has actually been deleted, but that doesn't appear to be the case since Handle is still non-null.
						UE_LOG(LogPakFile, Error, TEXT("TrimCache1: Non-deleted Pak File %s has invalid CacheIndex %d."), *CachedPakData[RealPakIndex].Name.ToString(), CacheIndex);
						continue;
					}
					if (CacheVisitedAlready[CacheIndex] == true)
						continue;
					CacheVisitedAlready[CacheIndex] = true;

					int32 NumToKeep = bDiscardAll ? 0 : GPakCache_NumUnreferencedBlocksToCache;
					int32 NumToRemove = FMath::Max<int32>(0, OffsetAndPakIndexOfSavedBlocked[CacheIndex].Num() - NumToKeep);
					if (!bDiscardAll)
						NumToRemove = 1;

					if (NumToRemove && OffsetAndPakIndexOfSavedBlocked[CacheIndex].Num())
					{
						NoneToRemove = false;
						for (int32 Index = 0; Index < NumToRemove; Index++)
						{
							FJoinedOffsetAndPakIndex OffsetAndPakIndex = OffsetAndPakIndexOfSavedBlocked[CacheIndex][Index];
							uint16 PakIndex = GetRequestPakIndex(OffsetAndPakIndex);
							int64 Offset = GetRequestOffset(OffsetAndPakIndex);
							FPakData& Pak = CachedPakData[PakIndex];
							MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
								&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
								CacheBlockAllocator,
								Offset,
								Offset,
								0,
								Pak.MaxNode,
								Pak.StartShift,
								Pak.MaxShift,
								[this](TIntervalTreeIndex BlockIndex) -> bool
							{
								FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
								if (!Block.InRequestRefCount)
								{
									UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) Discard Cached"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
									ClearBlock(Block);
									return true;
								}
								return false;
							}
							);
						}
						OffsetAndPakIndexOfSavedBlocked[CacheIndex].RemoveAt(0, NumToRemove, EAllowShrinking::No);
						AlreadyRemovedBlocksBecauseOfMemoryOverage = true;
					}
				}
				if (NoneToRemove)
				{
					break;
				}
			}
			if (GPakCache_TimeToTrim != 0.0f)
			{
				// we'll trim based on time rather than trying to keep within a memory budget
				double CurrentTime = FPlatformTime::Seconds();
				// CachedFilesScopeLock is locked
				for (uint16 RealPakIndex = StartPakIndex; RealPakIndex < EndPakIndex; RealPakIndex++)
				{
					if (!CachedPakData[RealPakIndex].Handle)
					{
						// This PakData has been unmounted and is no longer a valid entry
						continue;
					}
					int32 CacheIndex = CachedPakData[RealPakIndex].ActualPakFile->GetCacheIndex();
					if (CacheIndex < 0 || OffsetAndPakIndexOfSavedBlocked.Num() <= CacheIndex)
					{
						// It appears that rare crashes in shipped builds can hit this case.
						// Without the CacheIndex we will no longer be able to trim the cache. This isn't a problem if the PakFile
						// has actually been deleted, but that doesn't appear to be the case since Handle is still non-null.
						UE_LOG(LogPakFile, Error, TEXT("TrimCache2: Non-deleted Pak File %s has invalid CacheIndex %d."), *CachedPakData[RealPakIndex].Name.ToString(), CacheIndex);
						continue;
					}

					int32 NumToRemove = 0;

					if (OffsetAndPakIndexOfSavedBlocked[CacheIndex].Num())
					{
						for (int32 Index = 0; Index < OffsetAndPakIndexOfSavedBlocked[CacheIndex].Num(); Index++)
						{
							FJoinedOffsetAndPakIndex OffsetAndPakIndex = OffsetAndPakIndexOfSavedBlocked[CacheIndex][Index];
							uint16 PakIndex = GetRequestPakIndex(OffsetAndPakIndex);
							int64 Offset = GetRequestOffset(OffsetAndPakIndex);
							FPakData& Pak = CachedPakData[PakIndex];
							bool bRemovedAll = true;
							MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
								&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
								CacheBlockAllocator,
								Offset,
								Offset,
								0,
								Pak.MaxNode,
								Pak.StartShift,
								Pak.MaxShift,
								[this, CurrentTime, &bRemovedAll](TIntervalTreeIndex BlockIndex) -> bool
							{
								FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
								if (!Block.InRequestRefCount && (CurrentTime - Block.TimeNoLongerReferenced >= GPakCache_TimeToTrim))
								{
									UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) Discard Cached Based on Time"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
									ClearBlock(Block);
									return true;
								}
								bRemovedAll = false;
								return false;
							}
							);
							if (!bRemovedAll)
								break;
							NumToRemove++;
						}
						if (NumToRemove)
						{
							OffsetAndPakIndexOfSavedBlocked[CacheIndex].RemoveAt(0, NumToRemove, EAllowShrinking::No);
						}
					}
				}
			}
		}
		else
		{
			uint16 EndPakIndex = 65535;
			if (StartPakIndex != 65535)
			{
				EndPakIndex = StartPakIndex + 1;
			}
			else
			{
				StartPakIndex = 0;
				EndPakIndex = IntCastChecked<uint16>(CachedPakData.Num());
			}

			// CachedFilesScopeLock is locked
			for (uint16 RealPakIndex = StartPakIndex; RealPakIndex < EndPakIndex; RealPakIndex++)
			{
				if (!CachedPakData[RealPakIndex].Handle)
				{
					// This PakData has been unmounted and is no longer a valid entry
					continue;
				}
				int32 CacheIndex = CachedPakData[RealPakIndex].ActualPakFile->GetCacheIndex();
				if (CacheIndex < 0 || OffsetAndPakIndexOfSavedBlocked.Num() <= CacheIndex)
				{
					// It appears that rare crashes in shipped builds can hit this case.
					// Without the CacheIndex we will no longer be able to trim the cache. This isn't a problem if the PakFile
					// has actually been deleted, but that doesn't appear to be the case since Handle is still non-null.
					UE_LOG(LogPakFile, Error, TEXT("TrimCache3: Non-deleted Pak File %s has invalid CacheIndex %d."), *CachedPakData[RealPakIndex].Name.ToString(), CacheIndex);
					continue;
				}
				int32 NumToKeep = bDiscardAll ? 0 : GPakCache_NumUnreferencedBlocksToCache;
				int32 NumToRemove = FMath::Max<int32>(0, OffsetAndPakIndexOfSavedBlocked[CacheIndex].Num() - NumToKeep);
				if (NumToRemove)
				{
					for (int32 Index = 0; Index < NumToRemove; Index++)
					{
						FJoinedOffsetAndPakIndex OffsetAndPakIndex = OffsetAndPakIndexOfSavedBlocked[CacheIndex][Index];
						uint16 PakIndex = GetRequestPakIndex(OffsetAndPakIndex);
						int64 Offset = GetRequestOffset(OffsetAndPakIndex);
						FPakData& Pak = CachedPakData[PakIndex];
						MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
							&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
							CacheBlockAllocator,
							Offset,
							Offset,
							0,
							Pak.MaxNode,
							Pak.StartShift,
							Pak.MaxShift,
							[this](TIntervalTreeIndex BlockIndex) -> bool
						{
							FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
							if (!Block.InRequestRefCount)
							{
								UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) Discard Cached"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
								ClearBlock(Block);
								return true;
							}
							return false;
						}
						);
					}
					OffsetAndPakIndexOfSavedBlocked[CacheIndex].RemoveAt(0, NumToRemove, EAllowShrinking::No);
				}
			}
		}
	}

	void RemoveRequest(TIntervalTreeIndex Index)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(Index);
		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		int64 Size = Request.Size;
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Request.Size > 0 && Request.GetPriority() >= AIOP_MIN && Request.GetPriority() <= AIOP_MAX && int32(Request.Status) >= 0 && int32(Request.Status) < int32(EInRequestStatus::Num));

		bool RequestDontCache = (Request.PriorityAndFlags & AIOP_FLAG_DONTCACHE) != 0;


		if (RemoveFromIntervalTree<FPakInRequest>(&Pak.InRequests[Request.GetPriority()][(int32)Request.Status], InRequestAllocator, Index, Pak.StartShift, Pak.MaxShift))
		{

			int64 OffsetOfLastByte = Offset + Size - 1;
			MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
				&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				Offset,
				OffsetOfLastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this, OffsetOfLastByte, RequestDontCache](TIntervalTreeIndex BlockIndex) -> bool
			{
				FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
				check(Block.InRequestRefCount);
				if (!--Block.InRequestRefCount)
				{
					if (GPakCache_NumUnreferencedBlocksToCache && GetRequestOffset(Block.OffsetAndPakIndex) + Block.Size > OffsetOfLastByte) // last block
					{
						if (RequestDontCache && GPakCache_EnableNoCaching != 0)
						{
							uint16 BlocksPakIndex = GetRequestPakIndexLow(Block.OffsetAndPakIndex);
							int32 BlocksCacheIndex = CachedPakData[BlocksPakIndex].ActualPakFile->GetCacheIndex();
							Block.TimeNoLongerReferenced = 0.0;
							OffsetAndPakIndexOfSavedBlocked[BlocksCacheIndex].Remove(Block.OffsetAndPakIndex);
							ClearBlock(Block);
							return true;
						}
						else
						{
							uint16 BlocksPakIndex = GetRequestPakIndexLow(Block.OffsetAndPakIndex);
							int32 BlocksCacheIndex = CachedPakData[BlocksPakIndex].ActualPakFile->GetCacheIndex();
							Block.TimeNoLongerReferenced = FPlatformTime::Seconds();
							OffsetAndPakIndexOfSavedBlocked[BlocksCacheIndex].Remove(Block.OffsetAndPakIndex);
							OffsetAndPakIndexOfSavedBlocked[BlocksCacheIndex].Add(Block.OffsetAndPakIndex);
						}
						return false;
					}
					ClearBlock(Block);
					return true;
				}
				return false;
			}
			);
			if (!Pak.ActualPakFile->GetUnderlyingCacheTrimDisabled())
			{
				TrimCache(false, PakIndex);
			}
			OverlappingNodesInIntervalTree<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				Offset,
				Offset + Size - 1,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[this](TIntervalTreeIndex BlockIndex) -> bool
			{
				FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
				check(Block.InRequestRefCount);
				Block.InRequestRefCount--;
				return true;
			}
			);
		}
		else
		{
			check(0); // not found
		}
		ClearRequest(Request);
	}

	void NotifyComplete(TIntervalTreeIndex RequestIndex)
	{
		// CachedFilesScopeLock is locked
		FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);

		uint16 PakIndex = GetRequestPakIndex(Request.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Request.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + Request.Size <= Pak.TotalSize && Request.Size > 0 && Request.GetPriority() >= AIOP_MIN && Request.GetPriority() <= AIOP_MAX && Request.Status == EInRequestStatus::Complete);

		check(Request.Owner && Request.UniqueID);

		if (Request.Status == EInRequestStatus::Complete && Request.UniqueID == Request.Owner->UniqueID && RequestIndex == Request.Owner->InRequestIndex &&  Request.OffsetAndPakIndex == Request.Owner->OffsetAndPakIndex)
		{
			UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) Notify complete"), Request.OffsetAndPakIndex, Request.OffsetAndPakIndex + Request.Size);
			Request.Owner->RequestIsComplete();
			return;
		}
		else
		{
			check(0); // request should have been found
		}
	}

	FJoinedOffsetAndPakIndex GetNextBlock(EAsyncIOPriorityAndFlags& OutPriority)
	{
		EAsyncIOPriorityAndFlags AsyncMinPriorityLocal = AsyncMinPriority;

		// CachedFilesScopeLock is locked
		uint16 BestPakIndex = 0;
		FJoinedOffsetAndPakIndex BestNext = MAX_uint64;

		OutPriority = AIOP_MIN;
		bool bAnyOutstanding = false;
		for (int32 Priority = AIOP_MAX; ; Priority--)
		{
			if (Priority < AsyncMinPriorityLocal && bAnyOutstanding)
			{
				break;
			}
			for (int32 Pass = 0; ; Pass++)
			{
				FJoinedOffsetAndPakIndex LocalLastReadRequest = Pass ? 0 : LastReadRequest;

				uint16 PakIndex = GetRequestPakIndex(LocalLastReadRequest);
				int64 Offset = GetRequestOffset(LocalLastReadRequest);
				check(Offset <= CachedPakData[PakIndex].TotalSize);


				for (; BestNext == MAX_uint64 && PakIndex < CachedPakData.Num(); PakIndex++)
				{
					FPakData& Pak = CachedPakData[PakIndex];
					if (Pak.InRequests[Priority][(int32)EInRequestStatus::Complete] != IntervalTreeInvalidIndex)
					{
						bAnyOutstanding = true;
					}
					if (Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting] != IntervalTreeInvalidIndex)
					{
						uint64 Limit = uint64(Pak.TotalSize - 1);
						if (BestNext != MAX_uint64 && GetRequestPakIndex(BestNext) == PakIndex)
						{
							Limit = GetRequestOffset(BestNext) - 1;
						}

						OverlappingNodesInIntervalTreeWithShrinkingInterval<FPakInRequest>(
							Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
							InRequestAllocator,
							uint64(Offset),
							Limit,
							0,
							Pak.MaxNode,
							Pak.StartShift,
							Pak.MaxShift,
							[this, &Pak, &BestNext, &BestPakIndex, PakIndex, &Limit, LocalLastReadRequest](TIntervalTreeIndex Index) -> bool
						{
							FJoinedOffsetAndPakIndex First = FirstUnfilledBlockForRequest(Index, LocalLastReadRequest);
							check(LocalLastReadRequest != 0 || First != MAX_uint64); // if there was not trimming, and this thing is in the waiting list, then why was no start block found?
							if (First < BestNext)
							{
								BestNext = First;
								BestPakIndex = PakIndex;
								Limit = GetRequestOffset(BestNext) - 1;
							}
							return true; // always have to keep going because we want the smallest one
						}
						);
					}
				}
				if (!LocalLastReadRequest)
				{
					break; // this was a full pass
				}
			}

			if (Priority == AIOP_MIN || BestNext != MAX_uint64)
			{
				OutPriority = (EAsyncIOPriorityAndFlags)Priority;
				break;
			}
		}
		return BestNext;
	}

	bool AddNewBlock()
	{
		// CachedFilesScopeLock is locked
		EAsyncIOPriorityAndFlags RequestPriority;
		FJoinedOffsetAndPakIndex BestNext = GetNextBlock(RequestPriority);
		check(RequestPriority < AIOP_NUM);
		if (BestNext == MAX_uint64)
		{
			return false;
		}
		uint16 PakIndex = GetRequestPakIndex(BestNext);
		int64 Offset = GetRequestOffset(BestNext);
		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset < Pak.TotalSize);
		int64 FirstByte = AlignDown(Offset, PAK_CACHE_GRANULARITY);
		int64 LastByte = FMath::Min(Align(FirstByte + (GPakCache_MaxRequestSizeToLowerLevelKB * 1024), PAK_CACHE_GRANULARITY) - 1, Pak.TotalSize - 1);
		check(FirstByte >= 0 && LastByte < Pak.TotalSize && LastByte >= 0 && LastByte >= FirstByte);

		uint32 NumBits = IntCastChecked<uint32>((PAK_CACHE_GRANULARITY + LastByte - FirstByte) / PAK_CACHE_GRANULARITY);
		uint32 NumQWords = (NumBits + 63) >> 6;

		static TArray<uint64> InFlightOrDone;
		InFlightOrDone.Reset();
		InFlightOrDone.AddZeroed(NumQWords);
		if (NumBits != NumQWords * 64)
		{
			uint32 Extras = NumQWords * 64 - NumBits;
			InFlightOrDone[NumQWords - 1] = (MAX_uint64 << (64 - Extras));
		}

		if (Pak.CacheBlocks[(int32)EBlockStatus::Complete] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}
		if (Pak.CacheBlocks[(int32)EBlockStatus::InFlight] != IntervalTreeInvalidIndex)
		{
			OverlappingNodesInIntervalTreeMask<FCacheBlock>(
				Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
				CacheBlockAllocator,
				FirstByte,
				LastByte,
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				Pak.BytesToBitsShift,
				&InFlightOrDone[0]
				);
		}

		static TArray<uint64> Requested;
		Requested.Reset();
		Requested.AddZeroed(NumQWords);
		for (int32 Priority = AIOP_MAX;; Priority--)
		{
			if (Priority + PAK_CACHE_MAX_PRIORITY_DIFFERENCE_MERGE < RequestPriority)
			{
				break;
			}
			if (Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting] != IntervalTreeInvalidIndex)
			{
				OverlappingNodesInIntervalTreeMask<FPakInRequest>(
					Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
					InRequestAllocator,
					FirstByte,
					LastByte,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					Pak.BytesToBitsShift,
					&Requested[0]
					);
			}
			if (Priority == AIOP_MIN)
			{
				break;
			}
		}


		int64 Size = PAK_CACHE_GRANULARITY * 64 * NumQWords;
		for (uint32 Index = 0; Index < NumQWords; Index++)
		{
			uint64 NotAlreadyInFlightAndRequested = ((~InFlightOrDone[Index]) & Requested[Index]);
			if (NotAlreadyInFlightAndRequested != MAX_uint64)
			{
				Size = PAK_CACHE_GRANULARITY * 64 * Index;
				while (NotAlreadyInFlightAndRequested & 1)
				{
					Size += PAK_CACHE_GRANULARITY;
					NotAlreadyInFlightAndRequested >>= 1;
				}
				break;
			}
		}
		check(Size > 0 && Size <= (GPakCache_MaxRequestSizeToLowerLevelKB * 1024));
		Size = FMath::Min(FirstByte + Size, LastByte + 1) - FirstByte;

		TIntervalTreeIndex NewIndex = CacheBlockAllocator.Alloc();

		FCacheBlock& Block = CacheBlockAllocator.Get(NewIndex);
		Block.Index = NewIndex;
		Block.InRequestRefCount = 0;
		Block.Memory = nullptr;
		Block.OffsetAndPakIndex = MakeJoinedRequest(PakIndex, FirstByte);
		Block.Size = Size;
		Block.Status = EBlockStatus::InFlight;

		AddToIntervalTree<FCacheBlock>(
			&Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
			CacheBlockAllocator,
			NewIndex,
			Pak.StartShift,
			Pak.MaxShift
			);

		TArray<TIntervalTreeIndex> Inflights;

		for (int32 Priority = AIOP_MAX;; Priority--)
		{
			if (Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting] != IntervalTreeInvalidIndex)
			{
				MaybeRemoveOverlappingNodesInIntervalTree<FPakInRequest>(
					&Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
					InRequestAllocator,
					uint64(FirstByte),
					uint64(FirstByte + Size - 1),
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[this, &Block, &Inflights](TIntervalTreeIndex RequestIndex) -> bool
				{
					Block.InRequestRefCount++;
					if (FirstUnfilledBlockForRequest(RequestIndex) == MAX_uint64)
					{
						InRequestAllocator.Get(RequestIndex).Next = IntervalTreeInvalidIndex;
						Inflights.Add(RequestIndex);
						return true;
					}
					return false;
				}
				);
			}
#if PAK_EXTRA_CHECKS
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight],
				InRequestAllocator,
				uint64(FirstByte),
				uint64(FirstByte + Size - 1),
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[](TIntervalTreeIndex) -> bool
			{
				check(0); // if this is in flight, then why does it overlap my new block
				return false;
			}
			);
			OverlappingNodesInIntervalTree<FPakInRequest>(
				Pak.InRequests[Priority][(int32)EInRequestStatus::Complete],
				InRequestAllocator,
				uint64(FirstByte),
				uint64(FirstByte + Size - 1),
				0,
				Pak.MaxNode,
				Pak.StartShift,
				Pak.MaxShift,
				[](TIntervalTreeIndex) -> bool
			{
				check(0); // if this is complete, then why does it overlap my new block
				return false;
			}
			);
#endif
			if (Priority == AIOP_MIN)
			{
				break;
			}
		}
		for (TIntervalTreeIndex Fli : Inflights)
		{
			FPakInRequest& CompReq = InRequestAllocator.Get(Fli);
			CompReq.Status = EInRequestStatus::InFlight;
			AddToIntervalTree(&Pak.InRequests[CompReq.GetPriority()][(int32)EInRequestStatus::InFlight], InRequestAllocator, Fli, Pak.StartShift, Pak.MaxShift);
		}

		StartBlockTask(Block);
		return true;

	}

	int32 OpenTaskSlot()
	{
		int32 IndexToFill = -1;
		for (int32 Index = 0; Index < GPakCache_MaxRequestsToLowerLevel; Index++)
		{
			if (!RequestsToLower[Index].RequestHandle)
			{
				IndexToFill = Index;
				break;
			}
		}
		return IndexToFill;
	}


	bool HasRequestsAtStatus(EInRequestStatus Status)
	{
		for (uint16 PakIndex = 0; PakIndex < CachedPakData.Num(); PakIndex++)
		{
			FPakData& Pak = CachedPakData[PakIndex];
			for (int32 Priority = AIOP_MAX;; Priority--)
			{
				if (Pak.InRequests[Priority][(int32)Status] != IntervalTreeInvalidIndex)
				{
					return true;
				}
				if (Priority == AIOP_MIN)
				{
					break;
				}
			}
		}
		return false;
	}

	bool CanStartAnotherTask()
	{
		if (OpenTaskSlot() < 0)
		{
			return false;
		}
		return HasRequestsAtStatus(EInRequestStatus::Waiting);
	}
	void ClearOldBlockTasks()
	{
		if (!NotifyRecursion)
		{
			TArray<IAsyncReadRequest*> Swapped;
			{
				FScopeLock Lock(&CachedFilesScopeLock);
				Swapped = (MoveTemp(RequestsToDelete));
				check(RequestsToDelete.IsEmpty());
			}

			for (IAsyncReadRequest* Elem : Swapped)
			{
				while (!Elem->PollCompletion())
				{
					FPlatformProcess::Sleep(0);
				}
				delete Elem;
			}
			Swapped.Empty();
		}
	}
	void StartBlockTask(FCacheBlock& Block)
	{
		// CachedFilesScopeLock is locked
#define CHECK_REDUNDANT_READS (0)
#if CHECK_REDUNDANT_READS
		static struct FRedundantReadTracker
		{
			TMap<int64, double> LastReadTime;
			int32 NumRedundant;
			FRedundantReadTracker()
				: NumRedundant(0)
			{
			}

			void CheckBlock(int64 Offset, int64 Size)
			{
				double NowTime = FPlatformTime::Seconds();
				int64 StartBlock = Offset / PAK_CACHE_GRANULARITY;
				int64 LastBlock = (Offset + Size - 1) / PAK_CACHE_GRANULARITY;
				for (int64 CurBlock = StartBlock; CurBlock <= LastBlock; CurBlock++)
				{
					double LastTime = LastReadTime.FindRef(CurBlock);
					if (LastTime > 0.0 && NowTime - LastTime < 3.0)
					{
						NumRedundant++;
						FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Redundant read at block %d, %6.1fms ago       (%d total redundant blocks)\r\n"), int32(CurBlock), 1000.0f * float(NowTime - LastTime), NumRedundant);
					}
					LastReadTime.Add(CurBlock, NowTime);
				}
			}
		} RedundantReadTracker;
#else
		static struct FRedundantReadTracker
		{
			FORCEINLINE void CheckBlock(int64 Offset, int64 Size)
			{
			}
		} RedundantReadTracker;

#endif

		int32 IndexToFill = OpenTaskSlot();
		if (IndexToFill < 0)
		{
			check(0);
			return;
		}
		EAsyncIOPriorityAndFlags Priority = AIOP_Normal; // the lower level requests are not prioritized at the moment
		check(Block.Status == EBlockStatus::InFlight);
		UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) StartBlockTask"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
		uint16 PakIndex = GetRequestPakIndex(Block.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		RequestsToLower[IndexToFill].BlockIndex = Block.Index;
		RequestsToLower[IndexToFill].RequestSize = Block.Size;
		RequestsToLower[IndexToFill].Memory = nullptr;
		check(&CacheBlockAllocator.Get(RequestsToLower[IndexToFill].BlockIndex) == &Block);

#if USE_PAK_PRECACHE && CSV_PROFILER
		FPlatformAtomics::InterlockedAdd(&GPreCacheTotalLoaded, Block.Size);
		FPlatformAtomics::InterlockedAdd(&GTotalLoaded, Block.Size);
#endif

        // FORT HACK
        // DO NOT BRING BACK
        // FORT HACK
        bool bDoCheck = true;
#if PLATFORM_IOS
        static const int32 Range = 100;
        static const int32 Offset = 500;
        static int32 RandomCheckCount = FMath::Rand() % Range + Offset;
        bDoCheck = --RandomCheckCount <= 0;
        if (bDoCheck)
        {
            RandomCheckCount = FMath::Rand() % Range + Offset;
        }
#endif
		FAsyncFileCallBack CallbackFromLower =
			[this, IndexToFill, bDoCheck](bool bWasCanceled, IAsyncReadRequest* Request)
		{
			if (bEnableSignatureChecks && bDoCheck)
			{
				StartSignatureCheck(bWasCanceled, Request, IndexToFill);
			}
			else
			{
				NewRequestsToLowerComplete(bWasCanceled, Request, IndexToFill);
			}
		};

		RequestsToLower[IndexToFill].RequestHandle = Pak.Handle->ReadRequest(GetRequestOffset(Block.OffsetAndPakIndex), Block.Size, Priority, &CallbackFromLower);
		RedundantReadTracker.CheckBlock(GetRequestOffset(Block.OffsetAndPakIndex), Block.Size);

#if CSV_PROFILER
		FJoinedOffsetAndPakIndex OldLastReadRequest = LastReadRequest;
		LastReadRequest = Block.OffsetAndPakIndex + Block.Size;

		if (OldLastReadRequest != Block.OffsetAndPakIndex)
		{
			if (GetRequestPakIndexLow(OldLastReadRequest) != GetRequestPakIndexLow(Block.OffsetAndPakIndex))
			{
				GPreCacheBadSeeks++;
			}
			else
			{
				GPreCacheSeeks++;
			}
		}
		else
		{
			GPreCacheContiguousReads++;
		}
#endif
		Loads++;
		LoadSize += Block.Size;
	}

	void CompleteRequest(bool bWasCanceled, uint8* Memory, TIntervalTreeIndex BlockIndex)
	{
		FCacheBlock& Block = CacheBlockAllocator.Get(BlockIndex);
		uint16 PakIndex = GetRequestPakIndex(Block.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(Block.OffsetAndPakIndex);
		FPakData& Pak = CachedPakData[PakIndex];
		check(!Block.Memory && Block.Size);
		check(!bWasCanceled); // this is doable, but we need to transition requests back to waiting, inflight etc.

		if (!RemoveFromIntervalTree<FCacheBlock>(&Pak.CacheBlocks[(int32)EBlockStatus::InFlight], CacheBlockAllocator, Block.Index, Pak.StartShift, Pak.MaxShift))
		{
			check(0);
		}

		if (Block.InRequestRefCount == 0 || bWasCanceled)
		{
			check(Block.Size > 0);
			FMemory::Free(Memory);
			UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) Cancelled"), Block.OffsetAndPakIndex, Block.OffsetAndPakIndex + Block.Size);
			ClearBlock(Block);
		}
		else
		{
			Block.Memory = Memory;
			check(Block.Memory && Block.Size);
			BlockMemory += Block.Size;
			check(BlockMemory > 0);
			check(Block.Size > 0);
			INC_MEMORY_STAT_BY(STAT_PakCacheMem, Block.Size);

			if (BlockMemory > BlockMemoryHighWater)
			{
				BlockMemoryHighWater = BlockMemory;
				SET_MEMORY_STAT(STAT_PakCacheHighWater, BlockMemoryHighWater);

#if 1
				static int64 LastPrint = 0;
				if (BlockMemoryHighWater / 1024 / 1024 / 16 != LastPrint)
				{
					LastPrint = BlockMemoryHighWater / 1024 / 1024 / 16;
					//FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Precache HighWater %dMB\r\n"), int32(LastPrint));
					UE_LOG(LogPakFile, Log, TEXT("Precache HighWater %dMB\r\n"), int32(LastPrint * 16));
				}
#endif
			}
			Block.Status = EBlockStatus::Complete;
			AddToIntervalTree<FCacheBlock>(
				&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
				CacheBlockAllocator,
				Block.Index,
				Pak.StartShift,
				Pak.MaxShift
				);
			TArray<TIntervalTreeIndex> Completeds;
			for (int32 Priority = AIOP_MAX;; Priority--)
			{
				if (Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight] != IntervalTreeInvalidIndex)
				{
					MaybeRemoveOverlappingNodesInIntervalTree<FPakInRequest>(
						&Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight],
						InRequestAllocator,
						uint64(Offset),
						uint64(Offset + Block.Size - 1),
						0,
						Pak.MaxNode,
						Pak.StartShift,
						Pak.MaxShift,
						[this, &Completeds](TIntervalTreeIndex RequestIndex) -> bool
					{
						if (FirstUnfilledBlockForRequest(RequestIndex) == MAX_uint64)
						{
							InRequestAllocator.Get(RequestIndex).Next = IntervalTreeInvalidIndex;
							Completeds.Add(RequestIndex);
							return true;
						}
						return false;
					}
					);
				}
				if (Priority == AIOP_MIN)
				{
					break;
				}
			}
			for (TIntervalTreeIndex Comp : Completeds)
			{
				FPakInRequest& CompReq = InRequestAllocator.Get(Comp);
				CompReq.Status = EInRequestStatus::Complete;
				AddToIntervalTree(&Pak.InRequests[CompReq.GetPriority()][(int32)EInRequestStatus::Complete], InRequestAllocator, Comp, Pak.StartShift, Pak.MaxShift);
				NotifyComplete(Comp); // potentially scary recursion here
			}
		}

		TrimCache();
	}

	bool StartNextRequest()
	{
		if (CanStartAnotherTask())
		{
			return AddNewBlock();
		}
		return false;
	}

	bool GetCompletedRequestData(FPakInRequest& DoneRequest, uint8* Result)
	{
		// CachedFilesScopeLock is locked
		check(DoneRequest.Status == EInRequestStatus::Complete);
		uint16 PakIndex = GetRequestPakIndex(DoneRequest.OffsetAndPakIndex);
		int64 Offset = GetRequestOffset(DoneRequest.OffsetAndPakIndex);
		int64 Size = DoneRequest.Size;

		FPakData& Pak = CachedPakData[PakIndex];
		check(Offset + DoneRequest.Size <= Pak.TotalSize && DoneRequest.Size > 0 && DoneRequest.GetPriority() >= AIOP_MIN && DoneRequest.GetPriority() <= AIOP_MAX && DoneRequest.Status == EInRequestStatus::Complete);

		int64 BytesCopied = 0;

#if 0 // this path removes the block in one pass, however, this is not what we want because it wrecks precaching, if we change back GetCompletedRequest needs to maybe start a new request and the logic of the IAsyncFile read needs to change
		MaybeRemoveOverlappingNodesInIntervalTree<FCacheBlock>(
			&Pak.CacheBlocks[(int32)EBlockStatus::Complete],
			CacheBlockAllocator,
			Offset,
			Offset + Size - 1,
			0,
			Pak.MaxNode,
			Pak.StartShift,
			Pak.MaxShift,
			[this, Offset, Size, &BytesCopied, Result, &Pak](TIntervalTreeIndex BlockIndex) -> bool
		{
			FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
			int64 BlockOffset = GetRequestOffset(Block.OffsetAndPakIndex);
			check(Block.Memory && Block.Size && BlockOffset >= 0 && BlockOffset + Block.Size <= Pak.TotalSize);

			int64 OverlapStart = FMath::Max(Offset, BlockOffset);
			int64 OverlapEnd = FMath::Min(Offset + Size, BlockOffset + Block.Size);
			check(OverlapEnd > OverlapStart);
			BytesCopied += OverlapEnd - OverlapStart;
			FMemory::Memcpy(Result + OverlapStart - Offset, Block.Memory + OverlapStart - BlockOffset, OverlapEnd - OverlapStart);
			check(Block.InRequestRefCount);
			if (!--Block.InRequestRefCount)
			{
				ClearBlock(Block);
				return true;
			}
			return false;
		}
		);

		if (!RemoveFromIntervalTree<FPakInRequest>(&Pak.InRequests[DoneRequest.GetPriority()][(int32)EInRequestStatus::Complete], InRequestAllocator, DoneRequest.Index, Pak.StartShift, Pak.MaxShift))
		{
			check(0); // not found
		}
		ClearRequest(DoneRequest);
#else
		OverlappingNodesInIntervalTree<FCacheBlock>(
			Pak.CacheBlocks[(int32)EBlockStatus::Complete],
			CacheBlockAllocator,
			Offset,
			Offset + Size - 1,
			0,
			Pak.MaxNode,
			Pak.StartShift,
			Pak.MaxShift,
			[this, Offset, Size, &BytesCopied, Result, &Pak](TIntervalTreeIndex BlockIndex) -> bool
		{
			FCacheBlock &Block = CacheBlockAllocator.Get(BlockIndex);
			int64 BlockOffset = GetRequestOffset(Block.OffsetAndPakIndex);
			check(Block.Memory && Block.Size && BlockOffset >= 0 && BlockOffset + Block.Size <= Pak.TotalSize);

			int64 OverlapStart = FMath::Max(Offset, BlockOffset);
			int64 OverlapEnd = FMath::Min(Offset + Size, BlockOffset + Block.Size);
			check(OverlapEnd > OverlapStart);
			BytesCopied += OverlapEnd - OverlapStart;
			FMemory::Memcpy(Result + OverlapStart - Offset, Block.Memory + OverlapStart - BlockOffset, OverlapEnd - OverlapStart);
			return true;
		}
		);
#endif
		check(BytesCopied == Size);


		return true;
	}

	///// Below here are the thread entrypoints

public:

	void NewRequestsToLowerComplete(bool bWasCanceled, IAsyncReadRequest* Request, int32 Index)
	{
		LLM_SCOPE(ELLMTag::FileSystem);
		ClearOldBlockTasks();

		FScopeLock Lock(&CachedFilesScopeLock);
		RequestsToLower[Index].RequestHandle = Request;
		NotifyRecursion++;
		if (!RequestsToLower[Index].Memory) // might have already been filled in by the signature check
		{
			RequestsToLower[Index].Memory = Request->GetReadResults();
		}
		CompleteRequest(bWasCanceled, RequestsToLower[Index].Memory, RequestsToLower[Index].BlockIndex);
		RequestsToLower[Index].RequestHandle = nullptr;
		RequestsToDelete.Add(Request);
		RequestsToLower[Index].BlockIndex = IntervalTreeInvalidIndex;
		StartNextRequest();
		NotifyRecursion--;
	}

	bool QueueRequest(IPakRequestor* Owner, FPakFile* InActualPakFile, FName File, int64 PakFileSize, int64 Offset, int64 Size, EAsyncIOPriorityAndFlags PriorityAndFlags)
	{
		CSV_SCOPED_TIMING_STAT(FileIOVerbose, PakPrecacherQueueRequest);
		check(Owner && File != NAME_None && Size > 0 && Offset >= 0 && Offset < PakFileSize && (PriorityAndFlags&AIOP_PRIORITY_MASK) >= AIOP_MIN && (PriorityAndFlags&AIOP_PRIORITY_MASK) <= AIOP_MAX);
		FScopeLock Lock(&CachedFilesScopeLock);
		uint16* PakIndexPtr = RegisterPakFile(InActualPakFile, File, PakFileSize);
		if (PakIndexPtr == nullptr)
		{
			return false;
		}
		// Use NotifyRecursion to turn off maintenance functions like ClearOldBlockTasks that can BusyWait
		// (and thereby reenter this class and take locks in the wrong order) while we are holding the lock.
		++NotifyRecursion;
		ON_SCOPE_EXIT{ --NotifyRecursion; };

		uint16 PakIndex = *PakIndexPtr;
		FPakData& Pak = CachedPakData[PakIndex];
		check(Pak.Name == File && Pak.TotalSize == PakFileSize && Pak.Handle);

		TIntervalTreeIndex RequestIndex = InRequestAllocator.Alloc();
		FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);
		FJoinedOffsetAndPakIndex RequestOffsetAndPakIndex = MakeJoinedRequest(PakIndex, Offset);
		Request.OffsetAndPakIndex = RequestOffsetAndPakIndex;
		Request.Size = Size;
		Request.PriorityAndFlags = PriorityAndFlags;
		Request.Status = EInRequestStatus::Waiting;
		Request.Owner = Owner;
		Request.UniqueID = NextUniqueID++;
		Request.Index = RequestIndex;
		check(Request.Next == IntervalTreeInvalidIndex);
		Owner->OffsetAndPakIndex = Request.OffsetAndPakIndex;
		Owner->UniqueID = Request.UniqueID;
		Owner->InRequestIndex = RequestIndex;
		check(!OutstandingRequests.Contains(Request.UniqueID));
		OutstandingRequests.Add(Request.UniqueID, RequestIndex);
		RequestCounter.Increment();

		if (AddRequest(Request, RequestIndex))
		{
#if USE_PAK_PRECACHE && CSV_PROFILER
			FPlatformAtomics::InterlockedIncrement(&GPreCacheHotBlocksCount);
#endif
			UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) QueueRequest HOT"), RequestOffsetAndPakIndex, RequestOffsetAndPakIndex + Request.Size);
		}
		else
		{
#if USE_PAK_PRECACHE && CSV_PROFILER
			FPlatformAtomics::InterlockedIncrement(&GPreCacheColdBlocksCount);
#endif
			UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakReadRequest[%016llX, %016llX) QueueRequest COLD"), RequestOffsetAndPakIndex, RequestOffsetAndPakIndex + Request.Size);
		}

		TrimCache();
		return true;
	}

	void SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags NewPriority)
	{
		bool bStartNewRequests = false;
		{
			FScopeLock Lock(&SetAsyncMinimumPriorityScopeLock);
			if (AsyncMinPriority != NewPriority)
			{
				if (NewPriority < AsyncMinPriority)
				{
					bStartNewRequests = true;
				}
				AsyncMinPriority = NewPriority;
			}
		}

		if (bStartNewRequests)
		{
			FScopeLock Lock(&CachedFilesScopeLock);
			StartNextRequest();
		}
	}

	bool GetCompletedRequest(IPakRequestor* Owner, uint8* UserSuppliedMemory)
	{
		check(Owner);
		ClearOldBlockTasks();

		FScopeLock Lock(&CachedFilesScopeLock);
		TIntervalTreeIndex RequestIndex = OutstandingRequests.FindRef(Owner->UniqueID);
		static_assert(IntervalTreeInvalidIndex == 0, "FindRef will return 0 for something not found");
		if (RequestIndex)
		{
			FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);
			check(Owner == Request.Owner && Request.Status == EInRequestStatus::Complete && Request.UniqueID == Request.Owner->UniqueID && RequestIndex == Request.Owner->InRequestIndex &&  Request.OffsetAndPakIndex == Request.Owner->OffsetAndPakIndex);
			return GetCompletedRequestData(Request, UserSuppliedMemory);
		}
		return false; // canceled
	}

	void CancelRequest(IPakRequestor* Owner)
	{
		check(Owner);
		ClearOldBlockTasks();

		FScopeLock Lock(&CachedFilesScopeLock);
		TIntervalTreeIndex RequestIndex = OutstandingRequests.FindRef(Owner->UniqueID);
		static_assert(IntervalTreeInvalidIndex == 0, "FindRef will return 0 for something not found");
		if (RequestIndex)
		{
			FPakInRequest& Request = InRequestAllocator.Get(RequestIndex);
			check(Owner == Request.Owner && Request.UniqueID == Request.Owner->UniqueID && RequestIndex == Request.Owner->InRequestIndex &&  Request.OffsetAndPakIndex == Request.Owner->OffsetAndPakIndex);
			RemoveRequest(RequestIndex);
		}
		StartNextRequest();
	}

	bool IsProbablyIdle() // nothing to prevent new requests from being made before I return
	{
		FScopeLock Lock(&CachedFilesScopeLock);
		return !HasRequestsAtStatus(EInRequestStatus::Waiting) && !HasRequestsAtStatus(EInRequestStatus::InFlight);
	}

	void Unmount(FName PakFile, FPakFile* UnmountedPak)
	{
		FScopeLock Lock(&CachedFilesScopeLock);

		for (TMap<FPakFile*, uint16>::TIterator It(CachedPaks); It; ++It)
		{
			if( It->Key->GetFilenameName() == PakFile )
			{
				uint16 PakIndex = It->Value;
				TrimCache(true);
				FPakData& Pak = CachedPakData[PakIndex];
				int64 Offset = MakeJoinedRequest(PakIndex, 0);

				bool bHasOutstandingRequests = false;

				OverlappingNodesInIntervalTree<FCacheBlock>(
					Pak.CacheBlocks[(int32)EBlockStatus::Complete],
					CacheBlockAllocator,
					0,
					Offset + Pak.TotalSize - 1,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
				{
					check(!"Pak cannot be unmounted with outstanding requests");
					bHasOutstandingRequests = true;
					return false;
				}
				);
				OverlappingNodesInIntervalTree<FCacheBlock>(
					Pak.CacheBlocks[(int32)EBlockStatus::InFlight],
					CacheBlockAllocator,
					0,
					Offset + Pak.TotalSize - 1,
					0,
					Pak.MaxNode,
					Pak.StartShift,
					Pak.MaxShift,
					[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
				{
					check(!"Pak cannot be unmounted with outstanding requests");
					bHasOutstandingRequests = true;
					return false;
				}
				);
				for (int32 Priority = AIOP_MAX;; Priority--)
				{
					OverlappingNodesInIntervalTree<FPakInRequest>(
						Pak.InRequests[Priority][(int32)EInRequestStatus::InFlight],
						InRequestAllocator,
						0,
						Offset + Pak.TotalSize - 1,
						0,
						Pak.MaxNode,
						Pak.StartShift,
						Pak.MaxShift,
						[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
					{
						check(!"Pak cannot be unmounted with outstanding requests");
						bHasOutstandingRequests = true;
						return false;
					}
					);
					OverlappingNodesInIntervalTree<FPakInRequest>(
						Pak.InRequests[Priority][(int32)EInRequestStatus::Complete],
						InRequestAllocator,
						0,
						Offset + Pak.TotalSize - 1,
						0,
						Pak.MaxNode,
						Pak.StartShift,
						Pak.MaxShift,
						[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
					{
						check(!"Pak cannot be unmounted with outstanding requests");
						bHasOutstandingRequests = true;
						return false;
					}
					);
					OverlappingNodesInIntervalTree<FPakInRequest>(
						Pak.InRequests[Priority][(int32)EInRequestStatus::Waiting],
						InRequestAllocator,
						0,
						Offset + Pak.TotalSize - 1,
						0,
						Pak.MaxNode,
						Pak.StartShift,
						Pak.MaxShift,
						[&bHasOutstandingRequests](TIntervalTreeIndex BlockIndex) -> bool
					{
						check(!"Pak cannot be unmounted with outstanding requests");
						bHasOutstandingRequests = true;
						return false;
					}
					);
					if (Priority == AIOP_MIN)
					{
						break;
					}
				}
				if (!bHasOutstandingRequests)
				{
					UE_LOG(LogPakFile, Log, TEXT("Pak file %s removed from pak precacher."), *PakFile.ToString());
					if (Pak.ActualPakFile != UnmountedPak)
					{
						if (UnmountedPak)
						{
							UE_LOG(LogPakFile, Warning, TEXT("FPakPrecacher::Unmount found multiple PakFiles with the name %s. Unmounting all of them."), *PakFile.ToString());
						}
						Pak.ActualPakFile->SetIsMounted(false);
					}

					It.RemoveCurrent();
					check(Pak.Handle);
					delete Pak.Handle;
					Pak.Handle = nullptr;
					Pak.ActualPakFile = nullptr;
					int32 NumToTrim = 0;
					for (int32 Index = CachedPakData.Num() - 1; Index >= 0; Index--)
					{
						if (!CachedPakData[Index].Handle)
						{
							NumToTrim++;
						}
						else
						{
							break;
						}
					}
					if (NumToTrim)
					{
						CachedPakData.RemoveAt(CachedPakData.Num() - NumToTrim, NumToTrim);
						LastReadRequest = 0;
					}
				}
				else
				{
					UE_LOG(LogPakFile, Log, TEXT("Pak file %s was NOT removed from pak precacher because it had outstanding requests."), *PakFile.ToString());
				}

			}
		}

		// Even if we did not find the PakFile, mark it unmounted (and do this inside the CachedFilesScopeLock)
		// This will allow us to reject a RegisterPakFile request that could be coming from another thread from a not-yet-canceled FPakReadRequest
		if (UnmountedPak)
		{
			UnmountedPak->SetIsMounted(false);
		}
	}


	// these are not threadsafe and should only be used for synthetic testing
	uint64 GetLoadSize()
	{
		return LoadSize;
	}
	uint32 GetLoads()
	{
		return Loads;
	}
	uint32 GetFrees()
	{
		return Frees;
	}

	void DumpBlocks()
	{
		while (!FPakPrecacher::Get().IsProbablyIdle())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitDumpBlocks);
			FPlatformProcess::SleepNoStats(0.001f);
		}
		FScopeLock Lock(&CachedFilesScopeLock);
		bool bDone = !HasRequestsAtStatus(EInRequestStatus::Waiting) && !HasRequestsAtStatus(EInRequestStatus::InFlight) && !HasRequestsAtStatus(EInRequestStatus::Complete);

		if (!bDone)
		{
			UE_LOG(LogPakFile, Log, TEXT("PakCache has outstanding requests with %llu total memory."), BlockMemory);
		}
		else
		{
			UE_LOG(LogPakFile, Log, TEXT("PakCache has no outstanding requests with %llu total memory."), BlockMemory);
		}
	}
};

static void WaitPrecache(const TArray<FString>& Args)
{
	uint32 Frees = FPakPrecacher::Get().GetFrees();
	uint32 Loads = FPakPrecacher::Get().GetLoads();
	uint64 LoadSize = FPakPrecacher::Get().GetLoadSize();

	double StartTime = FPlatformTime::Seconds();

	while (!FPakPrecacher::Get().IsProbablyIdle())
	{
		check(Frees == FPakPrecacher::Get().GetFrees()); // otherwise we are discarding things, which is not what we want for this synthetic test
		QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitPrecache);
		FPlatformProcess::SleepNoStats(0.001f);
	}
	Loads = FPakPrecacher::Get().GetLoads() - Loads;
	LoadSize = FPakPrecacher::Get().GetLoadSize() - LoadSize;
	float TimeSpent = FloatCastChecked<float>(FPlatformTime::Seconds() - StartTime, UE::LWC::DefaultFloatPrecision);
	float LoadSizeMB = float(LoadSize) / (1024.0f * 1024.0f);
	float MBs = LoadSizeMB / TimeSpent;
	UE_LOG(LogPakFile, Log, TEXT("Loaded %4d blocks (align %4dKB) totalling %7.2fMB in %4.2fs   = %6.2fMB/s"), Loads, PAK_CACHE_GRANULARITY / 1024, LoadSizeMB, TimeSpent, MBs);
}

static FAutoConsoleCommand WaitPrecacheCmd(
	TEXT("pak.WaitPrecache"),
	TEXT("Debug command to wait on the pak precache."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&WaitPrecache)
);

static void DumpBlocks(const TArray<FString>& Args)
{
	FPakPrecacher::Get().DumpBlocks();
}

static FAutoConsoleCommand DumpBlocksCmd(
	TEXT("pak.DumpBlocks"),
	TEXT("Debug command to spew the outstanding blocks."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&DumpBlocks)
);

static FCriticalSection FPakReadRequestEvent;

class FPakAsyncReadFileHandle;

struct FCachedAsyncBlock
{
	/**
	 * Assigned in FPakAsyncReadFileHandle::StartBlock to store the handle for the raw read request.
	 * Readable only under FPakAsyncReadFileHandle->CriticalSection, or from RawReadCallback.
	 * Can not be written under CriticalSection until after RawRequest->WaitCompletion.
	 * Set to null under critical section from DoProcessing or from cancelation.
	 */
	class FPakReadRequest* RawRequest;
	/**
	 * compressed, encrypted and/or signature not checked
	 * Set to null in FPakAsyncReadFileHandle::StartBlock. RawReadRequest and DoProcessing can assign
	 * and modify it outside of FPakAsyncReadFileHandle->CriticalSection.
	 * Can not be read/written by any other thread until RawRequest is set to null and bCPUWorkIsComplete is set to false.
	 */
	uint8* Raw;
	/** decompressed, deencrypted and signature checked */
	uint8* Processed;
	FGraphEventRef CPUWorkGraphEvent;
	int32 RawSize;
	int32 DecompressionRawSize;
	int32 ProcessedSize;
	/**
	 * How many requests touch the block that are still alive and uncanceled. Accessed only within FPakAsyncReadFileHandle->CriticalSection.
	 * When the reference count goes to 0, the block is removed from Blocks, but async threads might still have a pointer to it.
	 * Block is deleted when refcount is 0 and async thread has finished with it (bCPUWorkIsComplete =true).
	 */
	int32 RefCount;
	int32 BlockIndex;
	/**
	 * The block has been requested, and is still referenced, either from still-alive requests or from the async load and processing  of the block.
	 * Accessed only within FPakAsyncReadFileHandle->CriticalSection. Modified when requests start, cancel/destroy, and when processing finishes. 
	 */
	bool bInFlight;
	/**
	 * The block is in flight and has finished loaded and processing by async threads. Is true only when bInFlight is true. 
	 * Starts false, and is set to true when and only when DoProcessing finishes with it. Cleared when block is no longer referenced.
	 * Accessed only within FPakAsyncReadFileHandle->CriticalSection. 
	 */
	bool bCPUWorkIsComplete;
	/**
	 * True if and only if all requests touching the block canceled before the block finished processing. The block is removed from Blocks,
	 * present in OutstandingCancelMapBlock, and still referenced as the Block pointer on the async thread.
	 * Accessed only within FPakAsyncReadFileHandle->CriticalSection.
	 */
	bool bCancelledBlock;
	FCachedAsyncBlock()
		: RawRequest(0)
		, Raw(nullptr)
		, Processed(nullptr)
		, RawSize(0)
		, DecompressionRawSize(0)
		, ProcessedSize(0)
		, RefCount(0)
		, BlockIndex(-1)
		, bInFlight(false)
		, bCPUWorkIsComplete(false)
		, bCancelledBlock(false)
	{
	}
};


class FPakReadRequestBase : public IAsyncReadRequest, public IPakRequestor
{
protected:

	int64 Offset;
	int64 BytesToRead;
	FEvent* WaitEvent;
	FCachedAsyncBlock* BlockPtr;
	FName PanicPakFile;
	EAsyncIOPriorityAndFlags PriorityAndFlags;
	bool bRequestOutstanding;
	bool bNeedsRemoval;
	bool bInternalRequest; // we are using this internally to deal with compressed, encrypted and signed, so we want the memory back from a precache request.

public:
	FPakReadRequestBase(FName InPakFile, int64 PakFileSize, FAsyncFileCallBack* CompleteCallback, int64 InOffset, int64 InBytesToRead, EAsyncIOPriorityAndFlags InPriorityAndFlags, uint8* UserSuppliedMemory, bool bInInternalRequest = false, FCachedAsyncBlock* InBlockPtr = nullptr)
		: IAsyncReadRequest(CompleteCallback, false, UserSuppliedMemory)
		, Offset(InOffset)
		, BytesToRead(InBytesToRead)
		, WaitEvent(nullptr)
		, BlockPtr(InBlockPtr)
		, PanicPakFile(InPakFile)
		, PriorityAndFlags(InPriorityAndFlags)
		, bRequestOutstanding(true)
		, bNeedsRemoval(true)
		, bInternalRequest(bInInternalRequest)
	{
	}

	virtual ~FPakReadRequestBase()
	{
		if (bNeedsRemoval)
		{
			FPakPrecacher::Get().CancelRequest(this);
		}
		if (Memory && !bUserSuppliedMemory)
		{
			// this can happen with a race on cancel, it is ok, they didn't take the memory, free it now
			check(BytesToRead > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			FMemory::Free(Memory);
		}
		Memory = nullptr;
	}

	// IAsyncReadRequest Interface

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			if (bRequestOutstanding)
			{
				check(!WaitEvent);
				WaitEvent = FPlatformProcess::GetSynchEventFromPool(true);
			}
		}
		if (WaitEvent)
		{
			if (TimeLimitSeconds == 0.0f)
			{
				WaitEvent->Wait();
				check(!bRequestOutstanding);
			}
			else
			{
				WaitEvent->Wait(static_cast<uint32>(TimeLimitSeconds * 1000.0f));
			}
			FScopeLock Lock(&FPakReadRequestEvent);
			FPlatformProcess::ReturnSynchEventToPool(WaitEvent);
			WaitEvent = nullptr;
		}
	}
	virtual void CancelImpl() override
	{
		check(!WaitEvent); // you canceled from a different thread that you waited from
		FPakPrecacher::Get().CancelRequest(this);
		bNeedsRemoval = false;
		if (bRequestOutstanding)
		{
			bRequestOutstanding = false;
			SetComplete();
		}
	}

	virtual void ReleaseMemoryOwnershipImpl() override
	{
		DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
	}

	FCachedAsyncBlock& GetBlock()
	{
		check(bInternalRequest && BlockPtr);
		return *BlockPtr;
	}
};

class FPakReadRequest : public FPakReadRequestBase
{
public:

	FPakReadRequest(FPakFile* InActualPakFile, FName InPakFile, int64 PakFileSize, FAsyncFileCallBack* CompleteCallback, int64 InOffset, int64 InBytesToRead, EAsyncIOPriorityAndFlags InPriorityAndFlags, uint8* UserSuppliedMemory, bool bInInternalRequest = false, FCachedAsyncBlock* InBlockPtr = nullptr)
		: FPakReadRequestBase(InPakFile, PakFileSize, CompleteCallback, InOffset, InBytesToRead, InPriorityAndFlags, UserSuppliedMemory, bInInternalRequest, InBlockPtr)
	{
		check(Offset >= 0 && BytesToRead > 0);
		check(bInternalRequest || ( InPriorityAndFlags & AIOP_FLAG_PRECACHE ) == 0 || !bUserSuppliedMemory); // you never get bits back from a precache request, so why supply memory?

		if (!FPakPrecacher::Get().QueueRequest(this, InActualPakFile, InPakFile, PakFileSize, Offset, BytesToRead, InPriorityAndFlags))
		{
			bRequestOutstanding = false;
			SetComplete();
		}
	}

	virtual void RequestIsComplete() override
	{
		check(bRequestOutstanding);
		if (!bCanceled && (bInternalRequest || (PriorityAndFlags & AIOP_FLAG_PRECACHE) == 0))
		{
			if (!bUserSuppliedMemory)
			{
				check(!Memory);
				Memory = (uint8*)FMemory::Malloc(BytesToRead);
				check(BytesToRead > 0);
				INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			}
			else
			{
				check(Memory);
			}
			if (!FPakPrecacher::Get().GetCompletedRequest(this, Memory))
			{
				check(bCanceled);
			}
		}
		SetDataComplete();
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			bRequestOutstanding = false;
			if (WaitEvent)
			{
				WaitEvent->Trigger();
			}
			SetAllComplete();
		}
	}

	void PanicSyncRead(uint8* Buffer)
	{
		IFileHandle* Handle = IPlatformFile::GetPlatformPhysical().OpenRead(*PanicPakFile.ToString());
		UE_CLOG(!Handle, LogPakFile, Fatal, TEXT("PanicSyncRead failed to open pak file %s"), *PanicPakFile.ToString());
		if (!Handle->Seek(Offset))
		{
			UE_LOG(LogPakFile, Fatal, TEXT("PanicSyncRead failed to seek pak file %s   %d bytes at %lld "), *PanicPakFile.ToString(), BytesToRead, Offset);
		}

		if (!Handle->Read(Buffer, BytesToRead))
		{
			UE_LOG(LogPakFile, Fatal, TEXT("PanicSyncRead failed to read pak file %s   %d bytes at %lld "), *PanicPakFile.ToString(), BytesToRead, Offset);
		}	
		delete Handle;
	}
};

class FPakEncryptedReadRequest : public FPakReadRequestBase
{
	int64 OriginalOffset;
	int64 OriginalSize;
	FGuid EncryptionKeyGuid;

public:

	FPakEncryptedReadRequest(FPakFile* InActualPakFile, FName InPakFile, int64 PakFileSize, FAsyncFileCallBack* CompleteCallback, int64 InPakFileStartOffset, int64 InFileOffset, int64 InBytesToRead, EAsyncIOPriorityAndFlags InPriorityAndFlags, uint8* UserSuppliedMemory, const FGuid& InEncryptionKeyGuid, bool bInInternalRequest = false, FCachedAsyncBlock* InBlockPtr = nullptr)
		: FPakReadRequestBase(InPakFile, PakFileSize, CompleteCallback, InPakFileStartOffset + InFileOffset, InBytesToRead, InPriorityAndFlags, UserSuppliedMemory, bInInternalRequest, InBlockPtr)
		, OriginalOffset(InPakFileStartOffset + InFileOffset)
		, OriginalSize(InBytesToRead)
		, EncryptionKeyGuid(InEncryptionKeyGuid)
	{
		Offset = InPakFileStartOffset + AlignDown(InFileOffset, FAES::AESBlockSize);
		BytesToRead = Align(InFileOffset + InBytesToRead, FAES::AESBlockSize) - AlignDown(InFileOffset, FAES::AESBlockSize);

		if (!FPakPrecacher::Get().QueueRequest(this, InActualPakFile, InPakFile, PakFileSize, Offset, BytesToRead, InPriorityAndFlags))
		{
			bRequestOutstanding = false;
			SetComplete();
		}
	}

	virtual void RequestIsComplete() override
	{
		check(bRequestOutstanding);
		if (!bCanceled && (bInternalRequest || ( PriorityAndFlags & AIOP_FLAG_PRECACHE) == 0 ))
		{
			uint8* OversizedBuffer = nullptr;
			if (OriginalOffset != Offset || OriginalSize != BytesToRead)
			{
				// We've read some bytes from before the requested offset, so we need to grab that larger amount
				// from read request and then cut out the bit we want!
				OversizedBuffer = (uint8*)FMemory::Malloc(BytesToRead);
			}
			uint8* DestBuffer = Memory;

			if (!bUserSuppliedMemory)
			{
				check(!Memory);
				DestBuffer = (uint8*)FMemory::Malloc(OriginalSize);
				INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, OriginalSize);
			}
			else
			{
				check(DestBuffer);
			}

			if (!FPakPrecacher::Get().GetCompletedRequest(this, OversizedBuffer != nullptr ? OversizedBuffer : DestBuffer))
			{
				check(bCanceled);
				if (!bUserSuppliedMemory)
				{
					check(!Memory && DestBuffer);
					FMemory::Free(DestBuffer);
					DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, OriginalSize);
					DestBuffer = nullptr;
				}
				if (OversizedBuffer)
				{
					FMemory::Free(OversizedBuffer);
					OversizedBuffer = nullptr;
				}
			}
			else
			{
				Memory = DestBuffer;
				check(Memory);
				INC_DWORD_STAT(STAT_PakCache_UncompressedDecrypts);

				if (OversizedBuffer)
				{
					check(IsAligned(BytesToRead, FAES::AESBlockSize));
					DecryptData(OversizedBuffer, BytesToRead, EncryptionKeyGuid);
					FMemory::Memcpy(Memory, OversizedBuffer + (OriginalOffset - Offset), OriginalSize);
					FMemory::Free(OversizedBuffer);
				}
				else
				{
					check(IsAligned(OriginalSize, FAES::AESBlockSize));
					DecryptData(Memory, OriginalSize, EncryptionKeyGuid);
				}
			}
		}
		SetDataComplete();
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			bRequestOutstanding = false;
			if (WaitEvent)
			{
				WaitEvent->Trigger();
			}
			SetAllComplete();
		}
	}
};

class FPakProcessedReadRequest : public IAsyncReadRequest
{
	FPakAsyncReadFileHandle* Owner;
	int64 Offset;
	int64 BytesToRead;
	FEvent* WaitEvent;
	FThreadSafeCounter CompleteRace; // this is used to resolve races with natural completion and cancel; there can be only one.
	EAsyncIOPriorityAndFlags PriorityAndFlags;
	bool bRequestOutstanding;
	bool bHasCancelled;
	bool bHasCompleted;

	TSet<FCachedAsyncBlock*> MyCanceledBlocks;

public:
	FPakProcessedReadRequest(FPakAsyncReadFileHandle* InOwner, FAsyncFileCallBack* CompleteCallback, int64 InOffset, int64 InBytesToRead, EAsyncIOPriorityAndFlags InPriorityAndFlags, uint8* UserSuppliedMemory)
		: IAsyncReadRequest(CompleteCallback, false, UserSuppliedMemory)
		, Owner(InOwner)
		, Offset(InOffset)
		, BytesToRead(InBytesToRead)
		, WaitEvent(nullptr)
		, PriorityAndFlags(InPriorityAndFlags)
		, bRequestOutstanding(true)
		, bHasCancelled(false)
		, bHasCompleted(false)
	{
		check(Offset >= 0 && BytesToRead > 0);
		check( ( PriorityAndFlags & AIOP_FLAG_PRECACHE ) == 0 || !bUserSuppliedMemory); // you never get bits back from a precache request, so why supply memory?
	}

	virtual ~FPakProcessedReadRequest()
	{
		UE_CLOG(!bCompleteAndCallbackCalled, LogPakFile, Fatal, TEXT("IAsyncReadRequests must not be deleted until they are completed."));
		check(!MyCanceledBlocks.Num());
		DoneWithRawRequests();
		if (Memory && !bUserSuppliedMemory)
		{
			// this can happen with a race on cancel, it is ok, they didn't take the memory, free it now
			check(BytesToRead > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
			FMemory::Free(Memory);
		}
		Memory = nullptr;
	}

	virtual void WaitCompletionImpl(float TimeLimitSeconds) override
	{
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			if (bRequestOutstanding)
			{
				check(!WaitEvent);
				WaitEvent = FPlatformProcess::GetSynchEventFromPool(true);
			}
		}
		if (WaitEvent)
		{
			if (TimeLimitSeconds == 0.0f)
			{
				WaitEvent->Wait();
				check(!bRequestOutstanding);
			}
			else
			{
				WaitEvent->Wait(static_cast<uint32>(TimeLimitSeconds * 1000.0f));
			}
			FScopeLock Lock(&FPakReadRequestEvent);
			FPlatformProcess::ReturnSynchEventToPool(WaitEvent);
			WaitEvent = nullptr;
		}
	}

	virtual void CancelImpl() override
	{
		check(!WaitEvent); // you canceled from a different thread that you waited from
		if (CompleteRace.Increment() == 1)
		{
			if (bRequestOutstanding)
			{
				CancelRawRequests();
				if (!MyCanceledBlocks.Num())
				{
					bRequestOutstanding = false;
					SetComplete();
				}
			}
		}
	}

	virtual void ReleaseMemoryOwnershipImpl() override
	{
		DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
	}

	void RequestIsComplete()
	{
		// Owner->CriticalSection is locked
		if (CompleteRace.Increment() == 1)
		{
			check(bRequestOutstanding);
			if (!bCanceled && ( PriorityAndFlags & AIOP_FLAG_PRECACHE) == 0 )
			{
				GatherResults();
			}
			SetDataComplete();
			{
				FScopeLock Lock(&FPakReadRequestEvent);
				bRequestOutstanding = false;
				if (WaitEvent)
				{
					WaitEvent->Trigger();
				}
				SetAllComplete();
			}
		}
	}
	bool CancelBlockComplete(FCachedAsyncBlock* BlockPtr)
	{
		check(MyCanceledBlocks.Contains(BlockPtr));
		MyCanceledBlocks.Remove(BlockPtr);
		if (!MyCanceledBlocks.Num())
		{
			FScopeLock Lock(&FPakReadRequestEvent);
			bRequestOutstanding = false;
			if (WaitEvent)
			{
				WaitEvent->Trigger();
			}
			SetComplete();
			return true;
		}
		return false;
	}


	void GatherResults();
	void DoneWithRawRequests();
	bool CheckCompletion(const FPakEntry& FileEntry, int32 BlockIndex, TArray<FCachedAsyncBlock*>& Blocks);
	void CancelRawRequests();
};

FAutoConsoleTaskPriority CPrio_AsyncIOCPUWorkTaskPriority(
	TEXT("TaskGraph.TaskPriorities.AsyncIOCPUWork"),
	TEXT("Task and thread priority for decompression, decryption and signature checking of async IO from a pak file."),
	ENamedThreads::BackgroundThreadPriority, // if we have background priority task threads, then use them...
	ENamedThreads::NormalTaskPriority, // .. at normal task priority
	ENamedThreads::NormalTaskPriority // if we don't have background threads, then use normal priority threads at normal task priority instead
);

class FAsyncIOCPUWorkTask
{
	FPakAsyncReadFileHandle& Owner;
	FCachedAsyncBlock* BlockPtr;

public:
	FORCEINLINE FAsyncIOCPUWorkTask(FPakAsyncReadFileHandle& InOwner, FCachedAsyncBlock* InBlockPtr)
		: Owner(InOwner)
		, BlockPtr(InBlockPtr)
	{
	}
	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncIOCPUWorkTask, STATGROUP_TaskGraphTasks);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_AsyncIOCPUWorkTaskPriority.Get();
	}
	FORCEINLINE static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent);
};

class FAsyncIOSignatureCheckTask
{
	bool bWasCanceled;
	IAsyncReadRequest* Request;
	int32 IndexToFill;

public:
	FORCEINLINE FAsyncIOSignatureCheckTask(bool bInWasCanceled, IAsyncReadRequest* InRequest, int32 InIndexToFill)
		: bWasCanceled(bInWasCanceled)
		, Request(InRequest)
		, IndexToFill(InIndexToFill)
	{
	}

	static FORCEINLINE TStatId GetStatId()
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FAsyncIOSignatureCheckTask, STATGROUP_TaskGraphTasks);
	}
	static FORCEINLINE ENamedThreads::Type GetDesiredThread()
	{
		return CPrio_AsyncIOCPUWorkTaskPriority.Get();
	}
	FORCEINLINE static ESubsequentsMode::Type GetSubsequentsMode()
	{
		return ESubsequentsMode::TrackSubsequents;
	}
	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		FPakPrecacher::Get().DoSignatureCheck(bWasCanceled, Request, IndexToFill);
	}
};

void FPakPrecacher::StartSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 Index)
{
	TGraphTask<FAsyncIOSignatureCheckTask>::CreateTask().ConstructAndDispatchWhenReady(bWasCanceled, Request, Index);
}

void FPakPrecacher::DoSignatureCheck(bool bWasCanceled, IAsyncReadRequest* Request, int32 Index)
{
	int32 SignatureIndex = -1;
	int64 NumSignaturesToCheck = 0;
	const uint8* Data = nullptr;
	int64 RequestSize = 0;
	int64 RequestOffset = 0;
	uint16 PakIndex;
	FSHAHash PrincipalSignatureHash;
	static const int64 MaxHashesToCache = 16;

#if PAKHASH_USE_CRC
	TPakChunkHash HashCache[MaxHashesToCache] = { 0 };
#else
	TPakChunkHash HashCache[MaxHashesToCache];
#endif

	{
		// Try and keep lock for as short a time as possible. Find our request and copy out the data we need
		FScopeLock Lock(&CachedFilesScopeLock);
		FRequestToLower& RequestToLower = RequestsToLower[Index];
		RequestToLower.RequestHandle = Request;
		RequestToLower.Memory = Request->GetReadResults();

		NumSignaturesToCheck = Align(RequestToLower.RequestSize, FPakInfo::MaxChunkDataSize) / FPakInfo::MaxChunkDataSize;
		check(NumSignaturesToCheck >= 1);

		FCacheBlock& Block = CacheBlockAllocator.Get(RequestToLower.BlockIndex);
		RequestOffset = GetRequestOffset(Block.OffsetAndPakIndex);
		check((RequestOffset % FPakInfo::MaxChunkDataSize) == 0);
		RequestSize = RequestToLower.RequestSize;
		PakIndex = GetRequestPakIndex(Block.OffsetAndPakIndex);
		Data = RequestToLower.Memory;
		SignatureIndex = IntCastChecked<int32>(RequestOffset / FPakInfo::MaxChunkDataSize);

		FPakData& PakData = CachedPakData[PakIndex];
		PrincipalSignatureHash = PakData.Signatures->DecryptedHash;

		for (int32 CacheIndex = 0; CacheIndex < FMath::Min(NumSignaturesToCheck, MaxHashesToCache); ++CacheIndex)
		{
			HashCache[CacheIndex] = PakData.Signatures->ChunkHashes[SignatureIndex + CacheIndex];
		}
	}

	check(Data);
	check(NumSignaturesToCheck > 0);
	check(RequestSize > 0);
	check(RequestOffset >= 0);

	// Hash the contents of the incoming buffer and check that it matches what we expected
	for (int64 SignedChunkIndex = 0; SignedChunkIndex < NumSignaturesToCheck; ++SignedChunkIndex, ++SignatureIndex)
	{
		int64 Size = FMath::Min(RequestSize, (int64)FPakInfo::MaxChunkDataSize);

		if ((SignedChunkIndex > 0) && ((SignedChunkIndex % MaxHashesToCache) == 0))
		{
			FScopeLock Lock(&CachedFilesScopeLock);
			FPakData& PakData = CachedPakData[PakIndex];
			for (int32 CacheIndex = 0; (CacheIndex < MaxHashesToCache) && ((SignedChunkIndex + CacheIndex) < NumSignaturesToCheck); ++CacheIndex)
			{
				HashCache[CacheIndex] = PakData.Signatures->ChunkHashes[SignatureIndex + CacheIndex];
			}
		}

		{
			SCOPE_SECONDS_ACCUMULATOR(STAT_PakCache_SigningChunkHashTime);

			TPakChunkHash ThisHash = ComputePakChunkHash(Data, Size);
			bool bChunkHashesMatch = (ThisHash == HashCache[SignedChunkIndex % MaxHashesToCache]);

			if (!bChunkHashesMatch)
			{
				FScopeLock Lock(&CachedFilesScopeLock);
				FPakData* PakData = &CachedPakData[PakIndex];

				UE_LOG(LogPakFile, Warning, TEXT("Pak chunk signing mismatch on chunk [%i/%i]! Expected %s, Received %s"), SignatureIndex, PakData->Signatures->ChunkHashes.Num() - 1, *ChunkHashToString(PakData->Signatures->ChunkHashes[SignatureIndex]), *ChunkHashToString(ThisHash));

				// Check the signatures are still as we expected them
				if (PakData->Signatures->DecryptedHash != PakData->Signatures->ComputeCurrentPrincipalHash())
				{
					UE_LOG(LogPakFile, Warning, TEXT("Principal signature table has changed since initialization!"));
				}

				FPakChunkSignatureCheckFailedData FailedData(PakData->Name.ToString(), HashCache[SignedChunkIndex % MaxHashesToCache], ThisHash, SignatureIndex);
				FPakPlatformFile::BroadcastPakChunkSignatureCheckFailure(FailedData);
			}
		}

		INC_MEMORY_STAT_BY(STAT_PakCache_SigningChunkHashSize, Size);

		RequestOffset += Size;
		Data += Size;
		RequestSize -= Size;
	}

	NewRequestsToLowerComplete(bWasCanceled, Request, Index);
}

class FPakAsyncReadFileHandle final : public IAsyncReadFileHandle
{
	/** Name of the PakFile that contains the FileEntry read by this handle. Read-only after construction. */
	FName PakFile;
	/**
	 * Pointer to the PakFile that contains the FileEntry read by this handle.
	 * The pointer is read-only after construction (the PakFile exceeds the lifetime of *this).
	 */
	TRefCountPtr<FPakFile> ActualPakFile;
	/** Size of the PakFile that contains the FileEntry read by this handle. Read-only after construction. */
	int64 PakFileSize;
	/**
	 * Number of bytes between start of the PakFile and start of the payload (AFTER the FileEntry struct)
	 * of the FileEntry read by this handle. Read-only after construction.
	 */
	int64 OffsetInPak;
	/** Number of bytes of the payload after being uncompressed. Read-only after construction. */
	int64 UncompressedFileSize;
	/** PakFile's metadata about the FileEntry read by this handle. Read-only after construction. */
	FPakEntry FileEntry;

	/**
	 * Set of Requests created by ReadRequest that will still need to access *this. Requests are removed from
	 * LiveRequests from their destructor or when they have canceled and their blocks have finished processing.
	 * The set is accessed only within this->CriticalSection.
	 */
	TSet<FPakProcessedReadRequest*> LiveRequests;
	/**
	 * Information about each compression block in the payload, including a refcount for how many LiveRequests
	 * requested the block. Empty and unused if the payload is not compressed.
	 * The array is allocated and filled with null during construction.
	 * Pointers in the array are accessed only within this->CriticalSection. Allocated pointers are
	 * copied by value into the async threads for loading and processing. Allocations are
	 * cleared and reused after their request refcount goes to 0, unless they are canceled before their
	 * processing completes. In that case they are removed from Blocks and later deleted when processing finishes.
	 * Thread synchronization rules differ for each element of the block, see comments on struct FCachedAsyncBlock	 */
	TArray<FCachedAsyncBlock*> Blocks;
	/** Callback we construct to call our RawReadCallback after each block's read. Read-only after construction. */
	FAsyncFileCallBack ReadCallbackFunction;
	FCriticalSection CriticalSection;
	int32 NumLiveRawRequests;
	FName CompressionMethod;
	int64 CompressedChunkOffset;
	FGuid EncryptionKeyGuid;

	TMap<FCachedAsyncBlock*, FPakProcessedReadRequest*> OutstandingCancelMapBlock;

	FCachedAsyncBlock& GetBlock(int32 Index)
	{
		if (!Blocks[Index])
		{
			Blocks[Index] = new FCachedAsyncBlock;
			Blocks[Index]->BlockIndex = Index;
		}
		return *Blocks[Index];
	}


public:
	FPakAsyncReadFileHandle(const FPakEntry* InFileEntry, const TRefCountPtr<FPakFile>& InPakFile, const TCHAR* Filename)
		: PakFile(InPakFile->GetFilenameName())
		, ActualPakFile(InPakFile)
		, PakFileSize(InPakFile->TotalSize())
		, FileEntry(*InFileEntry)
		, NumLiveRawRequests(0)
		, CompressedChunkOffset(0)
		, EncryptionKeyGuid(InPakFile->GetInfo().EncryptionKeyGuid)
	{
		OffsetInPak = FileEntry.Offset + FileEntry.GetSerializedSize(InPakFile->GetInfo().Version);
		UncompressedFileSize = FileEntry.UncompressedSize;
		int64 CompressedFileSize = FileEntry.UncompressedSize;
		CompressionMethod = InPakFile->GetInfo().GetCompressionMethod(FileEntry.CompressionMethodIndex);
#if !UE_BUILD_SHIPPING
		if (GetPakCacheForcePakProcessedReads() && CompressionMethod.IsNone() && UncompressedFileSize)
		{
			check(FileEntry.CompressionBlocks.Num() == 0);
			CompressionMethod = GPakFakeCompression;
			FileEntry.CompressionBlockSize = 65536;
			int64 EndSize = 0;
			while (EndSize < UncompressedFileSize)
			{
				FPakCompressedBlock& CompressedBlock = FileEntry.CompressionBlocks.Emplace_GetRef();
				CompressedBlock.CompressedStart = EndSize + OffsetInPak - (InPakFile->GetInfo().HasRelativeCompressedChunkOffsets() ? FileEntry.Offset : 0);
				CompressedBlock.CompressedEnd = CompressedBlock.CompressedStart + FileEntry.CompressionBlockSize;
				EndSize += FileEntry.CompressionBlockSize;
				if (EndSize > UncompressedFileSize)
				{
					CompressedBlock.CompressedEnd -= EndSize - UncompressedFileSize;
					EndSize = UncompressedFileSize;
				}
			}
		}
#endif
		if (!CompressionMethod.IsNone() && UncompressedFileSize)
		{
			check(FileEntry.CompressionBlocks.Num());
			CompressedFileSize = FileEntry.CompressionBlocks.Last().CompressedEnd - FileEntry.CompressionBlocks[0].CompressedStart;
			check(CompressedFileSize >= 0);
			const int32 CompressionBlockSize = FileEntry.CompressionBlockSize;
			check((UncompressedFileSize + CompressionBlockSize - 1) / CompressionBlockSize == FileEntry.CompressionBlocks.Num());
			Blocks.AddDefaulted(FileEntry.CompressionBlocks.Num());
			CompressedChunkOffset = InPakFile->GetInfo().HasRelativeCompressedChunkOffsets() ? FileEntry.Offset : 0;
		}
		UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakPlatformFile::OpenAsyncRead[%016llX, %016llX) %s"), OffsetInPak, OffsetInPak + CompressedFileSize, Filename);
		check(PakFileSize > 0 && OffsetInPak + CompressedFileSize <= PakFileSize && OffsetInPak >= 0);

		ReadCallbackFunction = [this](bool bWasCancelled, IAsyncReadRequest* Request)
		{
			RawReadCallback(bWasCancelled, Request);
		};

	}
	~FPakAsyncReadFileHandle()
	{
		FScopeLock ScopedLock(&CriticalSection);
		if (LiveRequests.Num() > 0 || NumLiveRawRequests > 0)
		{
			UE_LOG(LogPakFile, Fatal, TEXT("LiveRequests.Num or NumLiveRawReqeusts was > 0 in ~FPakAsyncReadFileHandle!"));
		}
		check(!LiveRequests.Num()); // must delete all requests before you delete the handle
		check(!NumLiveRawRequests); // must delete all requests before you delete the handle
		for (FCachedAsyncBlock* Block : Blocks)
		{
			if (Block)
			{
				check(Block->RefCount == 0);
				ClearBlock(*Block, true);
				delete Block;
			}
		}
	}

	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override
	{
		return new FPakSizeRequest(CompleteCallback, UncompressedFileSize);
	}
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriorityAndFlags PriorityAndFlags = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr, uint8* UserSuppliedMemory = nullptr) override
	{
		LLM_SCOPE(ELLMTag::FileSystem);

		if (BytesToRead == MAX_int64)
		{
			BytesToRead = UncompressedFileSize - Offset;
		}
		check(Offset + BytesToRead <= UncompressedFileSize && Offset >= 0);
		if (CompressionMethod == NAME_None)
		{
			check(Offset + BytesToRead + OffsetInPak <= PakFileSize);
			check(!Blocks.Num());

			if (FileEntry.IsEncrypted())
			{
				// Note that the lifetime of FPakEncryptedReadRequest is within our lifetime, so we can send the raw pointer in
				return new FPakEncryptedReadRequest(ActualPakFile, PakFile, PakFileSize, CompleteCallback, OffsetInPak, Offset, BytesToRead, PriorityAndFlags, UserSuppliedMemory, EncryptionKeyGuid);
			}
			else
			{
				// Note that the lifetime of FPakReadRequest is within our lifetime, so we can send the raw pointer in
				return new FPakReadRequest(ActualPakFile, PakFile, PakFileSize, CompleteCallback, OffsetInPak + Offset, BytesToRead, PriorityAndFlags, UserSuppliedMemory);
			}
		}
		bool bAnyUnfinished = false;
		FPakProcessedReadRequest* Result;
		{
			FScopeLock ScopedLock(&CriticalSection);
			check(Blocks.Num());
			int32 FirstBlock = IntCastChecked<int32>(Offset / FileEntry.CompressionBlockSize);
			int32 LastBlock = IntCastChecked<int32>((Offset + BytesToRead - 1) / FileEntry.CompressionBlockSize);

			check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

			Result = new FPakProcessedReadRequest(this, CompleteCallback, Offset, BytesToRead, PriorityAndFlags, UserSuppliedMemory);
			for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
			{

				FCachedAsyncBlock& Block = GetBlock(BlockIndex);
				Block.RefCount++;
				if (!Block.bInFlight)
				{
					check(Block.RefCount == 1);
					StartBlock(BlockIndex, PriorityAndFlags);
					bAnyUnfinished = true;
				}
				if (!Block.Processed)
				{
					bAnyUnfinished = true;
				}
			}
			check(!LiveRequests.Contains(Result));
			LiveRequests.Add(Result);
			if (!bAnyUnfinished)
			{
				Result->RequestIsComplete();
			}
		}
		return Result;
	}

	void StartBlock(int32 BlockIndex, EAsyncIOPriorityAndFlags PriorityAndFlags)
	{
		// this->CriticalSection is locked
		FCachedAsyncBlock& Block = GetBlock(BlockIndex);
		Block.bInFlight = true;
		check(!Block.RawRequest && !Block.Processed && !Block.Raw && !Block.CPUWorkGraphEvent.GetReference() && !Block.ProcessedSize && !Block.RawSize && !Block.bCPUWorkIsComplete);
		Block.RawSize = IntCastChecked<int32>(FileEntry.CompressionBlocks[BlockIndex].CompressedEnd - FileEntry.CompressionBlocks[BlockIndex].CompressedStart);
		Block.DecompressionRawSize = Block.RawSize;
		if (FileEntry.IsEncrypted())
		{
			Block.RawSize = Align(Block.RawSize, FAES::AESBlockSize);
		}
		NumLiveRawRequests++;
		// Note that the lifetime of FPakEncryptedReadRequest is within our lifetime, so we can send the raw pointer in
		Block.RawRequest = new FPakReadRequest(ActualPakFile, PakFile, PakFileSize, &ReadCallbackFunction, FileEntry.CompressionBlocks[BlockIndex].CompressedStart + CompressedChunkOffset, Block.RawSize, PriorityAndFlags, nullptr, true, &Block);
	}
	void RawReadCallback(bool bWasCancelled, IAsyncReadRequest* InRequest)
	{
		// CAUTION, no lock here!
		FPakReadRequest* Request = static_cast<FPakReadRequest*>(InRequest);

		FCachedAsyncBlock& Block = Request->GetBlock();
		check((Block.RawRequest == Request || (!Block.RawRequest && Block.RawSize)) // we still might be in the constructor so the assignment hasn't happened yet
			&& !Block.Processed && !Block.Raw);

		Block.Raw = Request->GetReadResults();
		FPlatformMisc::MemoryBarrier();
		if (Block.bCancelledBlock || !Block.Raw)
		{
			check(Block.bCancelledBlock);
			if (Block.Raw)
			{
				FMemory::Free(Block.Raw);
				Block.Raw = nullptr;
				check(Block.RawSize > 0);
				Block.RawSize = 0;
			}
		}
		else
		{
			check(Block.Raw);
			// Even though Raw memory has already been loaded, we're treating it as part of the AsyncFileMemory budget until it's processed.
			INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.RawSize);
			Block.ProcessedSize = FileEntry.CompressionBlockSize;
			if (Block.BlockIndex == Blocks.Num() - 1)
			{
				Block.ProcessedSize = FileEntry.UncompressedSize % FileEntry.CompressionBlockSize;
				if (!Block.ProcessedSize)
				{
					Block.ProcessedSize = FileEntry.CompressionBlockSize; // last block was a full block
				}
			}
			check(Block.ProcessedSize && !Block.bCPUWorkIsComplete);
		}
		Block.CPUWorkGraphEvent = TGraphTask<FAsyncIOCPUWorkTask>::CreateTask().ConstructAndDispatchWhenReady(*this, &Block);
	}
	void DoProcessing(FCachedAsyncBlock* BlockPtr)
	{
		FCachedAsyncBlock& Block = *BlockPtr;
		check(!Block.Processed);
		uint8* Output = nullptr;
		if (Block.Raw)
		{
			check(Block.Raw && Block.RawSize && !Block.Processed);

#if !UE_BUILD_SHIPPING
			bool bCorrupted = false;
			if (GPakCache_ForceDecompressionFails && FMath::FRand() < 0.001f)
			{
				int32 CorruptOffset = FMath::Clamp(FMath::RandRange(0, Block.RawSize - 1), 0, Block.RawSize - 1);
				uint8 CorruptValue = uint8(FMath::Clamp(FMath::RandRange(0, 255), 0, 255));
				if (Block.Raw[CorruptOffset] != CorruptValue)
				{
					UE_LOG(LogPakFile, Error, TEXT("Forcing corruption of decompression source data (predecryption) to verify panic read recovery.  Offset = %d, Value = 0x%x"), CorruptOffset, int32(CorruptValue));
					Block.Raw[CorruptOffset] = CorruptValue;
					bCorrupted = true;
				}
			}
#endif


			if (FileEntry.IsEncrypted())
			{
				INC_DWORD_STAT(STAT_PakCache_CompressedDecrypts);
				check(IsAligned(Block.RawSize, FAES::AESBlockSize));
				DecryptData(Block.Raw, Block.RawSize, EncryptionKeyGuid);
			}

			check(Block.ProcessedSize > 0);
			INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.ProcessedSize);
			Output = (uint8*)FMemory::Malloc(Block.ProcessedSize);
			if (FileEntry.IsEncrypted())
			{
				check(Align(Block.DecompressionRawSize, FAES::AESBlockSize) == Block.RawSize);
			}
			else
			{
				check(Block.DecompressionRawSize == Block.RawSize);
			}

			bool bFailed = false;
#if !UE_BUILD_SHIPPING
			if (CompressionMethod != GPakFakeCompression)
#endif
			{
				bFailed = !FCompression::UncompressMemory(CompressionMethod, Output, Block.ProcessedSize, Block.Raw, Block.DecompressionRawSize);
			}
#if !UE_BUILD_SHIPPING
			else
			{
				if (bCorrupted)
				{
					bFailed = true;
				}
				else
				{
					check(Block.ProcessedSize == Block.DecompressionRawSize);
					FMemory::Memcpy(Output, Block.Raw, Block.ProcessedSize);
				}
			}
			if (bCorrupted && !bFailed)
			{
				UE_LOG(LogPakFile, Error, TEXT("The payload was corrupted, but this did not trigger a decompression failed.....pretending it failed anyway because otherwise it can crash later."));
				bFailed = true;
			}
#endif

			if (bFailed)
			{
				{
					const FString HexBytes = BytesToHex(Block.Raw, FMath::Min(Block.DecompressionRawSize, 32));
					UE_LOG(LogPakFile, Error, TEXT("Pak Decompression failed. PakFile:%s, EntryOffset:%lld, EntrySize:%lld, Method:%s, ProcessedSize:%d, RawSize:%d, Crc32:%u, BlockIndex:%d, Encrypt:%d, Delete:%d, Output:%p, Raw:%p, Processed:%p, Bytes:[%s...]"),
						*PakFile.ToString(), FileEntry.Offset, FileEntry.Size, *CompressionMethod.ToString(), Block.ProcessedSize, Block.DecompressionRawSize,
						FCrc::MemCrc32(Block.Raw, Block.DecompressionRawSize), Block.BlockIndex, FileEntry.IsEncrypted() ? 1 : 0, FileEntry.IsDeleteRecord() ? 1 : 0, Output, Block.Raw, Block.Processed, *HexBytes);
				}
				uint8* TempBuffer = (uint8*)FMemory::Malloc(Block.RawSize);
				{
					FScopeLock ScopedLock(&CriticalSection);
					UE_CLOG(!Block.RawRequest, LogPakFile, Fatal, TEXT("Cannot retry because Block.RawRequest is null."));

					Block.RawRequest->PanicSyncRead(TempBuffer);
				}

				if (FileEntry.IsEncrypted())
				{
					DecryptData(TempBuffer, Block.RawSize, EncryptionKeyGuid);
				}
				if (FMemory::Memcmp(TempBuffer, Block.Raw, Block.DecompressionRawSize) != 0)
				{
					UE_LOG(LogPakFile, Warning, TEXT("Panic re-read (and decrypt if applicable) resulted in a different buffer."));

					int32 Offset = 0;
					for (; Offset < Block.DecompressionRawSize; Offset++)
					{
						if (TempBuffer[Offset] != Block.Raw[Offset])
						{
							break;
						}
					}
					UE_CLOG(Offset >= Block.DecompressionRawSize, LogPakFile, Fatal, TEXT("Buffers were different yet all bytes were the same????"));

					UE_LOG(LogPakFile, Warning, TEXT("Buffers differ at offset %d."), Offset);
					const FString HexBytes1 = BytesToHex(Block.Raw + Offset, FMath::Min(Block.DecompressionRawSize - Offset, 64));
					UE_LOG(LogPakFile, Warning, TEXT("Original read (and decrypt) %s"), *HexBytes1);
					const FString HexBytes2 = BytesToHex(TempBuffer + Offset, FMath::Min(Block.DecompressionRawSize - Offset, 64));
					UE_LOG(LogPakFile, Warning, TEXT("Panic reread  (and decrypt) %s"), *HexBytes2);
				}
				if (!FCompression::UncompressMemory(CompressionMethod, Output, Block.ProcessedSize, TempBuffer, Block.DecompressionRawSize))
				{
					UE_LOG(LogPakFile, Fatal, TEXT("Retry was NOT sucessful."));
				}
				else
				{
					UE_LOG(LogPakFile, Warning, TEXT("Retry was sucessful."));
				}
				FMemory::Free(TempBuffer);
			}
			FMemory::Free(Block.Raw);
			Block.Raw = nullptr;
			check(Block.RawSize > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.RawSize);
			Block.RawSize = 0;
		}
		else
		{
			check(Block.ProcessedSize == 0);
		}

		{
			FScopeLock ScopedLock(&CriticalSection);
			check(!Block.Processed);
			Block.Processed = Output;
			if (Block.RawRequest)
			{
				Block.RawRequest->WaitCompletion();
				delete Block.RawRequest;
				Block.RawRequest = nullptr;
				NumLiveRawRequests--;
			}
			if (Block.RefCount > 0)
			{
				check(&Block == Blocks[Block.BlockIndex] && !Block.bCancelledBlock);
				TArray<FPakProcessedReadRequest*, TInlineAllocator<4> > CompletedRequests;
				for (FPakProcessedReadRequest* Req : LiveRequests)
				{
					if (Req->CheckCompletion(FileEntry, Block.BlockIndex, Blocks))
					{
						CompletedRequests.Add(Req);
					}
				}
				for (FPakProcessedReadRequest* Req : CompletedRequests)
				{
					if (LiveRequests.Contains(Req))
					{
						Req->RequestIsComplete();
					}
				}
				Block.bCPUWorkIsComplete = true;
			}
			else
			{
				check(&Block != Blocks[Block.BlockIndex] && Block.bCancelledBlock);
				// must have been canceled, clean up
				FPakProcessedReadRequest* Owner;

				check(OutstandingCancelMapBlock.Contains(&Block));
				Owner = OutstandingCancelMapBlock[&Block];
				OutstandingCancelMapBlock.Remove(&Block);
				check(LiveRequests.Contains(Owner));

				if (Owner->CancelBlockComplete(&Block))
				{
					LiveRequests.Remove(Owner);
				}
				ClearBlock(Block);
				delete &Block;
			}
		}
	}
	void ClearBlock(FCachedAsyncBlock& Block, bool bForDestructorShouldAlreadyBeClear = false)
	{
		// this->CriticalSection is locked

		check(!Block.RawRequest);
		Block.RawRequest = nullptr;
		//check(!Block.CPUWorkGraphEvent || Block.CPUWorkGraphEvent->IsComplete());
		Block.CPUWorkGraphEvent = nullptr;
		if (Block.Raw)
		{
			check(!bForDestructorShouldAlreadyBeClear);
			// this was a cancel, clean it up now
			FMemory::Free(Block.Raw);
			Block.Raw = nullptr;
			check(Block.RawSize > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.RawSize);
		}
		Block.RawSize = 0;
		if (Block.Processed)
		{
			check(bForDestructorShouldAlreadyBeClear == false);
			FMemory::Free(Block.Processed);
			Block.Processed = nullptr;
			check(Block.ProcessedSize > 0);
			DEC_MEMORY_STAT_BY(STAT_AsyncFileMemory, Block.ProcessedSize);
		}
		Block.ProcessedSize = 0;
		Block.bCPUWorkIsComplete = false;
		Block.bInFlight = false;
	}

	void RemoveRequest(FPakProcessedReadRequest* Req, int64 Offset, int64 BytesToRead, const bool& bAlreadyCancelled)
	{
		FScopeLock ScopedLock(&CriticalSection);
		if (bAlreadyCancelled)
		{
			check(!LiveRequests.Contains(Req));
			return;
		}

		check(LiveRequests.Contains(Req));
		LiveRequests.Remove(Req);
		int32 FirstBlock = IntCastChecked<int32>(Offset / FileEntry.CompressionBlockSize);
		int32 LastBlock = IntCastChecked<int32>((Offset + BytesToRead - 1) / FileEntry.CompressionBlockSize);
		check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

		for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
		{
			FCachedAsyncBlock& Block = GetBlock(BlockIndex);
			check(Block.RefCount > 0);
			if (!--Block.RefCount)
			{
				// If this no-longer-referenced block is still held by the RawReadThread+DoProcessing functions, DoProcessing will crash when it assumes
				// the block has been canceled and assumes it is present in OutstandingCancelMapBlock
				// We have to fatally assert instead of doing what cancel does, because RemoveRequest is called from the request's destructor,
				// and therefore we don't have a persistent request that can take responsibility for staying alive until the canceled block finishes processing
				UE_CLOG(Block.bInFlight && !Block.bCPUWorkIsComplete, LogPakFile, Fatal, TEXT("RemoveRequest called on Request that still has a block in processing."));
				if (Block.RawRequest)
				{
					Block.RawRequest->Cancel();
					Block.RawRequest->WaitCompletion();
					delete Block.RawRequest;
					Block.RawRequest = nullptr;
					NumLiveRawRequests--;
				}
				ClearBlock(Block);
			}
		}
	}

	void HandleCanceledRequest(TSet<FCachedAsyncBlock*>& MyCanceledBlocks, FPakProcessedReadRequest* Req, int64 Offset, int64 BytesToRead, bool& bHasCancelledRef)
	{
		FScopeLock ScopedLock(&CriticalSection);
		check(!bHasCancelledRef);
		bHasCancelledRef = true;
		check(LiveRequests.Contains(Req));
		int32 FirstBlock = IntCastChecked<int32>(Offset / FileEntry.CompressionBlockSize);
		int32 LastBlock = IntCastChecked<int32>((Offset + BytesToRead - 1) / FileEntry.CompressionBlockSize);
		check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

		for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
		{
			FCachedAsyncBlock& Block = GetBlock(BlockIndex);
			check(Block.RefCount > 0);
			if (!--Block.RefCount)
			{
				if (Block.bInFlight && !Block.bCPUWorkIsComplete)
				{
					MyCanceledBlocks.Add(&Block);
					Blocks[BlockIndex] = nullptr;
					check(!OutstandingCancelMapBlock.Contains(&Block));
					OutstandingCancelMapBlock.Add(&Block, Req);
					Block.bCancelledBlock = true;
					FPlatformMisc::MemoryBarrier();
					Block.RawRequest->Cancel();
				}
				else
				{
					ClearBlock(Block);
				}
			}
		}

		if (!MyCanceledBlocks.Num())
		{
			LiveRequests.Remove(Req);
		}
	}


	void GatherResults(uint8* Memory, int64 Offset, int64 BytesToRead)
	{
		// CriticalSection is locked
		int32 FirstBlock = IntCastChecked<int32>(Offset / FileEntry.CompressionBlockSize);
		int32 LastBlock = IntCastChecked<int32>((Offset + BytesToRead - 1) / FileEntry.CompressionBlockSize);
		check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

		for (int32 BlockIndex = FirstBlock; BlockIndex <= LastBlock; BlockIndex++)
		{
			FCachedAsyncBlock& Block = GetBlock(BlockIndex);
			check(Block.RefCount > 0 && Block.Processed && Block.ProcessedSize);
			int64 BlockStart = int64(BlockIndex) * int64(FileEntry.CompressionBlockSize);
			int64 BlockEnd = BlockStart + Block.ProcessedSize;

			int64 SrcOffset = 0;
			int64 DestOffset = BlockStart - Offset;
			if (DestOffset < 0)
			{
				SrcOffset -= DestOffset;
				DestOffset = 0;
			}
			int64 CopySize = Block.ProcessedSize;
			if (DestOffset + CopySize > BytesToRead)
			{
				CopySize = BytesToRead - DestOffset;
			}
			if (SrcOffset + CopySize > Block.ProcessedSize)
			{
				CopySize = Block.ProcessedSize - SrcOffset;
			}
			check(CopySize > 0 && DestOffset >= 0 && DestOffset + CopySize <= BytesToRead);
			check(SrcOffset >= 0 && SrcOffset + CopySize <= Block.ProcessedSize);
			FMemory::Memcpy(Memory + DestOffset, Block.Processed + SrcOffset, CopySize);

			check(Block.RefCount > 0);
		}
	}
};

void FPakProcessedReadRequest::CancelRawRequests()
{
	Owner->HandleCanceledRequest(MyCanceledBlocks, this, Offset, BytesToRead, bHasCancelled);
}

void FPakProcessedReadRequest::GatherResults()
{
	// Owner->CriticalSection is locked
	if (!bUserSuppliedMemory)
	{
		check(!Memory);
		Memory = (uint8*)FMemory::Malloc(BytesToRead);
		INC_MEMORY_STAT_BY(STAT_AsyncFileMemory, BytesToRead);
	}
	check(Memory);
	Owner->GatherResults(Memory, Offset, BytesToRead);
}

void FPakProcessedReadRequest::DoneWithRawRequests()
{
	Owner->RemoveRequest(this, Offset, BytesToRead, bHasCancelled);
}

bool FPakProcessedReadRequest::CheckCompletion(const FPakEntry& FileEntry, int32 BlockIndex, TArray<FCachedAsyncBlock*>& Blocks)
{
	// Owner->CriticalSection is locked
	if (!bRequestOutstanding || bHasCompleted || bHasCancelled)
	{
		return false;
	}
	{
		int64 BlockStart = int64(BlockIndex) * int64(FileEntry.CompressionBlockSize);
		int64 BlockEnd = (int64(BlockIndex) + 1) * int64(FileEntry.CompressionBlockSize);
		if (Offset >= BlockEnd || Offset + BytesToRead <= BlockStart)
		{
			return false;
		}
	}
	int32 FirstBlock = IntCastChecked<int32>(Offset / FileEntry.CompressionBlockSize);
	int32 LastBlock = IntCastChecked<int32>((Offset + BytesToRead - 1) / FileEntry.CompressionBlockSize);
	check(FirstBlock >= 0 && FirstBlock < Blocks.Num() && LastBlock >= 0 && LastBlock < Blocks.Num() && FirstBlock <= LastBlock);

	for (int32 MyBlockIndex = FirstBlock; MyBlockIndex <= LastBlock; MyBlockIndex++)
	{
		check(Blocks[MyBlockIndex]);
		if (!Blocks[MyBlockIndex]->Processed)
		{
			return false;
		}
	}
	bHasCompleted = true;
	return true;
}

void FAsyncIOCPUWorkTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	SCOPED_NAMED_EVENT(FAsyncIOCPUWorkTask_DoTask, FColor::Cyan);
	Owner.DoProcessing(BlockPtr);
}

#endif  


#if PAK_TRACKER
TMap<FString, int32> FPakPlatformFile::GPakSizeMap;

void FPakPlatformFile::TrackPak(const TCHAR* Filename, const FPakEntry* PakEntry)
{
	FString Key(Filename);

	if (!GPakSizeMap.Find(Key))
	{
		GPakSizeMap.Add(Key, PakEntry->Size);
	}
}
#endif

class FBypassPakAsyncReadFileHandle final : public IAsyncReadFileHandle
{
	FName PakFile;
	int64 PakFileSize;
	int64 OffsetInPak;
	int64 UncompressedFileSize;
	FPakEntry FileEntry;
	IAsyncReadFileHandle* LowerHandle;

public:
	FBypassPakAsyncReadFileHandle(const FPakEntry* InFileEntry, const TRefCountPtr<FPakFile>& InPakFile, const TCHAR* Filename)
		: PakFile(InPakFile->GetFilenameName())
		, PakFileSize(InPakFile->TotalSize())
		, FileEntry(*InFileEntry)
	{
		OffsetInPak = FileEntry.Offset + FileEntry.GetSerializedSize(InPakFile->GetInfo().Version);
		UncompressedFileSize = FileEntry.UncompressedSize;
		int64 CompressedFileSize = FileEntry.UncompressedSize;
		check(FileEntry.CompressionMethodIndex == 0);
		UE_LOG(LogPakFile, VeryVerbose, TEXT("FPakPlatformFile::OpenAsyncRead (FBypassPakAsyncReadFileHandle)[%016llX, %016llX) %s"), OffsetInPak, OffsetInPak + CompressedFileSize, Filename);
		check(PakFileSize > 0 && OffsetInPak + CompressedFileSize <= PakFileSize && OffsetInPak >= 0);

		LowerHandle = IPlatformFile::GetPlatformPhysical().OpenAsyncRead(*InPakFile->GetFilename());
	}
	~FBypassPakAsyncReadFileHandle()
	{
		delete LowerHandle;
	}

	virtual IAsyncReadRequest* SizeRequest(FAsyncFileCallBack* CompleteCallback = nullptr) override
	{
		if (!LowerHandle)
		{
			return nullptr;
		}
		return new FPakSizeRequest(CompleteCallback, UncompressedFileSize);
	}
	virtual IAsyncReadRequest* ReadRequest(int64 Offset, int64 BytesToRead, EAsyncIOPriorityAndFlags PriorityAndFlags = AIOP_Normal, FAsyncFileCallBack* CompleteCallback = nullptr, uint8* UserSuppliedMemory = nullptr) override
	{
		if (!LowerHandle)
		{
			return nullptr;
		}
		if (BytesToRead == MAX_int64)
		{
			BytesToRead = UncompressedFileSize - Offset;
		}
		check(Offset + BytesToRead <= UncompressedFileSize && Offset >= 0);
		check(FileEntry.CompressionMethodIndex == 0);
		check(Offset + BytesToRead + OffsetInPak <= PakFileSize);


#if CSV_PROFILER
		FPlatformAtomics::InterlockedAdd(&GTotalLoaded, BytesToRead);
#endif

		return LowerHandle->ReadRequest(Offset + OffsetInPak, BytesToRead, PriorityAndFlags, CompleteCallback, UserSuppliedMemory);
	}
	virtual bool UsesCache() override
	{
		return LowerHandle->UsesCache();
	}
};

IAsyncReadFileHandle* FPakPlatformFile::OpenAsyncRead(const TCHAR* Filename)
{
	CSV_SCOPED_TIMING_STAT(FileIOVerbose, PakOpenAsyncRead);
#if USE_PAK_PRECACHE
	if (FPlatformProcess::SupportsMultithreading() && GPakCache_Enable > 0)
	{
		FPakEntry FileEntry;
		TRefCountPtr<FPakFile> PakFile;
		bool bFoundEntry = FindFileInPakFiles(Filename, &PakFile, &FileEntry);
		if (bFoundEntry && PakFile && PakFile->GetFilenameName() != NAME_None)
		{
#if PAK_TRACKER
			TrackPak(Filename, &FileEntry);
#endif

			return new FPakAsyncReadFileHandle(&FileEntry, PakFile, Filename);
		}
	}
#elif PLATFORM_BYPASS_PAK_PRECACHE
	{
		FPakEntry FileEntry;
		TRefCountPtr<FPakFile> PakFile;
		bool bFoundEntry = FindFileInPakFiles(Filename, &PakFile, &FileEntry);
		if (bFoundEntry && PakFile && PakFile->GetFilenameName() != NAME_None && FileEntry.CompressionMethodIndex == 0 && !FileEntry.IsEncrypted())
		{
#if PAK_TRACKER
			TrackPak(Filename, &FileEntry);
#endif
			return new FBypassPakAsyncReadFileHandle(&FileEntry, PakFile, Filename);
		}
	}
#endif
	return IPlatformFile::OpenAsyncRead(Filename);
}

void FPakPlatformFile::SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags Priority)
{
#if USE_PAK_PRECACHE
	if (FPlatformProcess::SupportsMultithreading() && GPakCache_Enable > 0)
	{
		FPakPrecacher::Get().SetAsyncMinimumPriority(Priority);
	}
#elif PLATFORM_BYPASS_PAK_PRECACHE
	IPlatformFile::GetPlatformPhysical().SetAsyncMinimumPriority(Priority);
#endif
}

void FPakPlatformFile::Tick()
{
#if USE_PAK_PRECACHE && CSV_PROFILER
	if (PakPrecacherSingleton != nullptr)
	{
		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherRequests, FPakPrecacher::Get().GetRequestCount(), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherHotBlocksCount, (int32)GPreCacheHotBlocksCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherColdBlocksCount, (int32)GPreCacheColdBlocksCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherTotalLoadedMB, (int32)(GPreCacheTotalLoaded / (1024 * 1024)), ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(FileIO, PakPrecacherBlockMemoryMB, (int32)(FPakPrecacher::Get().GetBlockMemory() / (1024 * 1024)), ECsvCustomStatOp::Set);



		if (GPreCacheTotalLoadedLastTick != 0)
		{
			int64 diff = GPreCacheTotalLoaded - GPreCacheTotalLoadedLastTick;
			diff /= 1024;
			CSV_CUSTOM_STAT(FileIO, PakPrecacherPerFrameKB, (int32)diff, ECsvCustomStatOp::Set);
		}
		GPreCacheTotalLoadedLastTick = GPreCacheTotalLoaded;

		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherSeeks, (int32)GPreCacheSeeks, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherBadSeeks, (int32)GPreCacheBadSeeks, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(FileIOVerbose, PakPrecacherContiguousReads, (int32)GPreCacheContiguousReads, ECsvCustomStatOp::Set);
		
		CSV_CUSTOM_STAT(FileIOVerbose, PakLoads, (int32)PakPrecacherSingleton->Get().GetLoads(), ECsvCustomStatOp::Set);

}
#endif
#if TRACK_DISK_UTILIZATION && CSV_PROFILER
	CSV_CUSTOM_STAT(DiskIO, OutstandingIORequests, int32(GDiskUtilizationTracker.GetOutstandingRequests()), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(DiskIO, BusyTime, float(GDiskUtilizationTracker.GetShortTermStats().GetTotalIOTimeInSeconds()), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(DiskIO, IdleTime, float(GDiskUtilizationTracker.GetShortTermStats().GetTotalIdleTimeInSeconds()), ECsvCustomStatOp::Set);
#endif

#if CSV_PROFILER

	int64 LocalTotalLoaded = GTotalLoaded;
	if (IoDispatcherFileBackend.IsValid())
	{
		LocalTotalLoaded += FIoDispatcher::Get().GetTotalLoaded();
	}

	CSV_CUSTOM_STAT(FileIOVerbose, TotalLoadedMB, (int32)(LocalTotalLoaded / (1024 * 1024)), ECsvCustomStatOp::Set);
	if (GTotalLoadedLastTick != 0)
	{
		int64 diff = LocalTotalLoaded - GTotalLoadedLastTick;
		diff /= 1024;
		CSV_CUSTOM_STAT(FileIO, PerFrameKB, (int32)diff, ECsvCustomStatOp::Set);
	}
	GTotalLoadedLastTick = LocalTotalLoaded;
#endif

}

class FMappedFilePakProxy final : public IMappedFileHandle
{
	IMappedFileHandle* LowerLevel;
	int64 OffsetInPak;
	int64 PakSize;
	FString DebugFilename;
public:
	FMappedFilePakProxy(IMappedFileHandle* InLowerLevel, int64 InOffset, int64 InSize, int64 InPakSize, const TCHAR* InDebugFilename)
		: IMappedFileHandle(InSize)
		, LowerLevel(InLowerLevel)
		, OffsetInPak(InOffset)
		, PakSize(InPakSize)
		, DebugFilename(InDebugFilename)
	{
		check(PakSize >= 0);
	}
	virtual ~FMappedFilePakProxy()
	{
		// we don't own lower level, it is shared
	}
	virtual IMappedFileRegion* MapRegion(int64 Offset = 0, int64 BytesToMap = MAX_int64, bool bPreloadHint = false) override
	{
		//check(Offset + OffsetInPak < PakSize); // don't map zero bytes and don't map off the end of the (real) file
		check(Offset < GetFileSize()); // don't map zero bytes and don't map off the end of the (virtual) file
		BytesToMap = FMath::Min<int64>(BytesToMap, GetFileSize() - Offset);
		check(BytesToMap > 0); // don't map zero bytes
		//check(Offset + BytesToMap <= GetFileSize()); // don't map zero bytes and don't map off the end of the (virtual) file
		//check(Offset + OffsetInPak + BytesToMap <= PakSize); // don't map zero bytes and don't map off the end of the (real) file
		return LowerLevel->MapRegion(Offset + OffsetInPak, BytesToMap, bPreloadHint);
	}
};


#if !UE_BUILD_SHIPPING

static void MappedFileTest(const TArray<FString>& Args)
{
	FString TestFile(TEXT("../../../Engine/Config/BaseDeviceProfiles.ini"));
	if (Args.Num() > 0)
	{
		TestFile = Args[0];
	}

	while (true)
	{
		IMappedFileHandle* Handle = FPlatformFileManager::Get().GetPlatformFile().OpenMapped(*TestFile);
		IMappedFileRegion *Region = Handle->MapRegion();

		int64 Size = Region->GetMappedSize();
		const char* Data = (const char *)Region->GetMappedPtr();

		delete Region;
		delete Handle;
	}


}

static FAutoConsoleCommand MappedFileTestCmd(
	TEXT("MappedFileTest"),
	TEXT("Tests the file mappings through the low level."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&MappedFileTest)
);
#endif

static int32 GMMIO_Enable = 1;
static FAutoConsoleVariableRef CVar_MMIOEnable(
	   TEXT("mmio.enable"),
	   GMMIO_Enable,
	   TEXT("If > 0, then enable memory mapped IO on platforms that support it.")
	   );


IMappedFileHandle* FPakPlatformFile::OpenMapped(const TCHAR* Filename)
{
	if (!GMMIO_Enable)
	{
		return nullptr;
	}

#if !UE_BUILD_SHIPPING
	if (bLookLooseFirst && IsNonPakFilenameAllowed(Filename))
	{
		IMappedFileHandle* Handle = LowerLevel->OpenMapped(Filename);
		if (Handle != nullptr)
		{
			return Handle;
		}
	}
#endif

	// Check pak files first
	FPakEntry FileEntry;
	TRefCountPtr<FPakFile> PakEntry;
	if (FindFileInPakFiles(Filename, &PakEntry, &FileEntry) && PakEntry.IsValid())
	{
		if (FileEntry.CompressionMethodIndex != 0 || (FileEntry.Flags & FPakEntry::Flag_Encrypted) != 0)
		{
			// can't map compressed or encrypted files
			return nullptr;
		}
		FScopeLock Lock(&PakEntry->MappedFileHandleCriticalSection);
		if (!PakEntry->MappedFileHandle)
		{
			PakEntry->MappedFileHandle = LowerLevel->OpenMapped(*PakEntry->GetFilename());
		}
		if (!PakEntry->MappedFileHandle)
		{
			return nullptr;
		}
		return new FMappedFilePakProxy(PakEntry->MappedFileHandle, FileEntry.Offset + FileEntry.GetSerializedSize(PakEntry->GetInfo().Version), FileEntry.UncompressedSize, PakEntry->TotalSize(), Filename);
	}
	if (IsNonPakFilenameAllowed(Filename))
	{
		return LowerLevel->OpenMapped(Filename);
	}
	return nullptr;
}


/**
 * Class to handle correctly reading from a compressed file within a compressed package
 */
class FPakSimpleEncryption
{
public:
	enum
	{
		Alignment = FAES::AESBlockSize,
	};

	static FORCEINLINE int64 AlignReadRequest(int64 Size)
	{
		return Align(Size, Alignment);
	}

	static FORCEINLINE void DecryptBlock(void* Data, int64 Size, const FGuid& EncryptionKeyGuid)
	{
		INC_DWORD_STAT(STAT_PakCache_SyncDecrypts);
		DecryptData((uint8*)Data, Size, EncryptionKeyGuid);
	}
};

struct FCompressionScratchBuffers
{
	FCompressionScratchBuffers()
		: TempBufferSize(0)
		, ScratchBufferSize(0)
		, LastPakEntryOffset(-1)
		, LastDecompressedBlock(0xFFFFFFFF)
		, Next(nullptr)
	{}

	int64				TempBufferSize;
	TUniquePtr<uint8[]>	TempBuffer;
	int64				ScratchBufferSize;
	TUniquePtr<uint8[]>	ScratchBuffer;

	int64 LastPakEntryOffset;
	FSHAHash LastPakIndexHash;
	uint32 LastDecompressedBlock;

	FCompressionScratchBuffers* Next;

	void EnsureBufferSpace(int64 CompressionBlockSize, int64 ScrachSize)
	{
		if (TempBufferSize < CompressionBlockSize)
		{
			TempBufferSize = CompressionBlockSize;
			TempBuffer = MakeUnique<uint8[]>(TempBufferSize);
		}
		if (ScratchBufferSize < ScrachSize)
		{
			ScratchBufferSize = ScrachSize;
			ScratchBuffer = MakeUnique<uint8[]>(ScratchBufferSize);
		}
	}
};

/**
 * Thread local class to manage working buffers for file compression
 */
class FCompressionScratchBuffersStack : public TThreadSingleton<FCompressionScratchBuffersStack>
{
public:
	FCompressionScratchBuffersStack()
		: bFirstInUse(false)
		, RecursionList(nullptr)
	{}

private:
	FCompressionScratchBuffers* Acquire()
	{
		if (!bFirstInUse)
		{
			bFirstInUse = true;
			return &First;
		}
		FCompressionScratchBuffers* Top = new FCompressionScratchBuffers;
		Top->Next = RecursionList;
		RecursionList = Top;
		return Top;
	}

	void Release(FCompressionScratchBuffers* Top)
	{
		check(bFirstInUse);
		if (!RecursionList)
		{
			check(Top == &First);
			bFirstInUse = false;
		}
		else
		{
			check(Top == RecursionList);
			RecursionList = Top->Next;
			delete Top;
		}
	}

	bool bFirstInUse;
	FCompressionScratchBuffers First;
	FCompressionScratchBuffers* RecursionList;

	friend class FScopedCompressionScratchBuffers;
};

class FScopedCompressionScratchBuffers
{
public:
	FScopedCompressionScratchBuffers()
		: Inner(FCompressionScratchBuffersStack::Get().Acquire())
	{}

	~FScopedCompressionScratchBuffers()
	{
		FCompressionScratchBuffersStack::Get().Release(Inner);
	}

	FCompressionScratchBuffers* operator->() const
	{
		return Inner;
	}

private:
	FCompressionScratchBuffers* Inner;
};

/**
 * Class to handle correctly reading from a compressed file within a pak
 */
template< typename EncryptionPolicy = FPakNoEncryption >
class FPakCompressedReaderPolicy
{
public:
	class FPakUncompressTask : public FNonAbandonableTask
	{
	public:
		uint8*				UncompressedBuffer;
		int32				UncompressedSize;
		uint8*				CompressedBuffer;
		int32				CompressedSize;
		FName				CompressionFormat;
		void*				CopyOut;
		int64				CopyOffset;
		int64				CopyLength;
		FGuid				EncryptionKeyGuid;

		void DoWork()
		{
			// Decrypt and Uncompress from memory to memory.
			int64 EncryptionSize = EncryptionPolicy::AlignReadRequest(CompressedSize);
			EncryptionPolicy::DecryptBlock(CompressedBuffer, EncryptionSize, EncryptionKeyGuid);
			FCompression::UncompressMemory(CompressionFormat, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressedSize);
			if (CopyOut)
			{
				FMemory::Memcpy(CopyOut, UncompressedBuffer + CopyOffset, CopyLength);
			}
		}

		FORCEINLINE TStatId GetStatId() const
		{
			// TODO: This is called too early in engine startup.
			return TStatId();
			//RETURN_QUICK_DECLARE_CYCLE_STAT(FPakUncompressTask, STATGROUP_ThreadPoolAsyncTasks);
		}
	};

	FPakCompressedReaderPolicy(const FPakFile& InPakFile, const FPakEntry& InPakEntry, TAcquirePakReaderFunction& InAcquirePakReader)
		: PakFile(InPakFile)
		, PakEntry(InPakEntry)
		, AcquirePakReader(InAcquirePakReader)
	{
	}

	~FPakCompressedReaderPolicy()
	{
	}

	/** Pak file that own this file data */
	const FPakFile&		PakFile;
	/** Pak file entry for this file. */
	FPakEntry			PakEntry;
	/** Function that gives us an FArchive to read from. The result should never be cached, but acquired and used within the function doing the serialization operation */
	TAcquirePakReaderFunction AcquirePakReader;

	FORCEINLINE int64 FileSize() const
	{
		return PakEntry.UncompressedSize;
	}

	void Serialize(int64 DesiredPosition, void* V, int64 Length)
	{
		const int32 CompressionBlockSize = PakEntry.CompressionBlockSize;
		uint32 CompressionBlockIndex = (uint32)(DesiredPosition / CompressionBlockSize);
		uint8* WorkingBuffers[2];
		int64 DirectCopyStart = DesiredPosition % PakEntry.CompressionBlockSize;
		FAsyncTask<FPakUncompressTask> UncompressTask;
		FScopedCompressionScratchBuffers ScratchSpace;
		bool bStartedUncompress = false;

		FName CompressionMethod = PakFile.GetInfo().GetCompressionMethod(PakEntry.CompressionMethodIndex);
		checkf(FCompression::IsFormatValid(CompressionMethod), 
			TEXT("Attempting to use compression format %s when loading a file from a .pak, but that compression format is not available.\n")
			TEXT("If you are running a program (like UnrealPak) you may need to pass the .uproject on the commandline so the plugin can be found.\n")
			TEXT("It's also possible that a necessary compression plugin has not been loaded yet, and this file needs to be forced to use zlib compression.\n")
			TEXT("Unfortunately, the code that can check this does not have the context of the filename that is being read. You will need to look in the callstack in a debugger.\n")
			TEXT("See ExtensionsToNotUsePluginCompression in [Pak] section of Engine.ini to add more extensions."),
			*CompressionMethod.ToString(), TEXT("Unknown"));

		int64 WorkingBufferRequiredSize = FCompression::GetMaximumCompressedSize(CompressionMethod,CompressionBlockSize);
		if ( CompressionMethod != NAME_Oodle )
		{
			// an amount to extra allocate, in case one block's compressed size is bigger than GetMaximumCompressedSize
			// @todo this should not be needed, can it be removed?
			double SlopMultiplier = 1.1f;
			WorkingBufferRequiredSize = (int64)((double)WorkingBufferRequiredSize * SlopMultiplier );
		}

		WorkingBufferRequiredSize = EncryptionPolicy::AlignReadRequest(WorkingBufferRequiredSize);
		const bool bExistingScratchBufferValid = ScratchSpace->TempBufferSize >= CompressionBlockSize;
		ScratchSpace->EnsureBufferSpace(CompressionBlockSize, WorkingBufferRequiredSize * 2);
		WorkingBuffers[0] = ScratchSpace->ScratchBuffer.Get();
		WorkingBuffers[1] = ScratchSpace->ScratchBuffer.Get() + WorkingBufferRequiredSize;

		FSharedPakReader PakReader = AcquirePakReader();

		while (Length > 0)
		{
			const FPakCompressedBlock& Block = PakEntry.CompressionBlocks[CompressionBlockIndex];
			int64 Pos = CompressionBlockIndex * CompressionBlockSize;
			int64 CompressedBlockSize = Block.CompressedEnd - Block.CompressedStart;
			int64 UncompressedBlockSize = FMath::Min<int64>(PakEntry.UncompressedSize - Pos, PakEntry.CompressionBlockSize);

			if (CompressedBlockSize > UncompressedBlockSize)
			{
				UE_LOG(LogPakFile, Verbose, TEXT("Bigger compressed? Block[%d]: %d -> %d > %d [%d min %d]"), CompressionBlockIndex, Block.CompressedStart, Block.CompressedEnd, UncompressedBlockSize, PakEntry.UncompressedSize - Pos, PakEntry.CompressionBlockSize);
			}


			int64 ReadSize = EncryptionPolicy::AlignReadRequest(CompressedBlockSize);
			int64 WriteSize = FMath::Min<int64>(UncompressedBlockSize - DirectCopyStart, Length);

			const bool bCurrentScratchTempBufferValid = 
				bExistingScratchBufferValid && !bStartedUncompress
				// ensure this object was the last reader from the scratch buffer and the last thing it decompressed was this block.
				&& (ScratchSpace->LastPakEntryOffset == PakEntry.Offset)
				&& (ScratchSpace->LastPakIndexHash == PakFile.GetInfo().IndexHash)
				&& (ScratchSpace->LastDecompressedBlock == CompressionBlockIndex)
				// ensure the previous decompression destination was the scratch buffer.
				&& !(DirectCopyStart == 0 && Length >= CompressionBlockSize); 

			if (bCurrentScratchTempBufferValid)
			{
				// Reuse the existing scratch buffer to avoid repeatedly deserializing and decompressing the same block.
				FMemory::Memcpy(V, ScratchSpace->TempBuffer.Get() + DirectCopyStart, WriteSize);
			}
			else
			{
				PakReader->Seek(Block.CompressedStart + (PakFile.GetInfo().HasRelativeCompressedChunkOffsets() ? PakEntry.Offset : 0));
				PakReader->Serialize(WorkingBuffers[CompressionBlockIndex & 1], ReadSize);
				if (bStartedUncompress)
				{
					UncompressTask.EnsureCompletion();
					bStartedUncompress = false;
				}

				FPakUncompressTask& TaskDetails = UncompressTask.GetTask();
				TaskDetails.EncryptionKeyGuid = PakFile.GetInfo().EncryptionKeyGuid;

				if (DirectCopyStart == 0 && Length >= CompressionBlockSize)
				{
					// Block can be decompressed directly into output buffer
					TaskDetails.CompressionFormat = CompressionMethod;
					TaskDetails.UncompressedBuffer = (uint8*)V;
					TaskDetails.UncompressedSize = IntCastChecked<int32>(UncompressedBlockSize);
					TaskDetails.CompressedBuffer = WorkingBuffers[CompressionBlockIndex & 1];
					TaskDetails.CompressedSize = IntCastChecked<int32>(CompressedBlockSize);
					TaskDetails.CopyOut = nullptr;
					ScratchSpace->LastDecompressedBlock = 0xFFFFFFFF;
					ScratchSpace->LastPakIndexHash = FSHAHash();
					ScratchSpace->LastPakEntryOffset = -1;
				}
				else
				{
					// Block needs to be copied from a working buffer
					TaskDetails.CompressionFormat = CompressionMethod;
					TaskDetails.UncompressedBuffer = ScratchSpace->TempBuffer.Get();
					TaskDetails.UncompressedSize = IntCastChecked<int32>(UncompressedBlockSize);
					TaskDetails.CompressedBuffer = WorkingBuffers[CompressionBlockIndex & 1];
					TaskDetails.CompressedSize = IntCastChecked<int32>(CompressedBlockSize);
					TaskDetails.CopyOut = V;
					TaskDetails.CopyOffset = DirectCopyStart;
					TaskDetails.CopyLength = WriteSize;
					ScratchSpace->LastDecompressedBlock = CompressionBlockIndex;
					ScratchSpace->LastPakIndexHash = PakFile.GetInfo().IndexHash;
					ScratchSpace->LastPakEntryOffset = PakEntry.Offset;
				}

				if (Length == WriteSize)
				{
					UncompressTask.StartSynchronousTask();
				}
				else
				{
					UncompressTask.StartBackgroundTask();
				}

				bStartedUncompress = true;
			}
		
			V = (void*)((uint8*)V + WriteSize);
			Length -= WriteSize;
			DirectCopyStart = 0;
			++CompressionBlockIndex;
		}

		if (bStartedUncompress)
		{
			UncompressTask.EnsureCompletion();
		}
	}
};

bool FPakEntry::VerifyPakEntriesMatch(const FPakEntry& FileEntryA, const FPakEntry& FileEntryB)
{
	bool bResult = true;
	if (FileEntryA.Size != FileEntryB.Size)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header file size mismatch, got: %lld, expected: %lld"), FileEntryB.Size, FileEntryA.Size);
		bResult = false;
	}
	if (FileEntryA.UncompressedSize != FileEntryB.UncompressedSize)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header uncompressed file size mismatch, got: %lld, expected: %lld"), FileEntryB.UncompressedSize, FileEntryA.UncompressedSize);
		bResult = false;
	}
	if (FileEntryA.CompressionMethodIndex != FileEntryB.CompressionMethodIndex)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak header file compression method mismatch, got: %d, expected: %d"), FileEntryB.CompressionMethodIndex, FileEntryA.CompressionMethodIndex);
		bResult = false;
	}
	if (FMemory::Memcmp(FileEntryA.Hash, FileEntryB.Hash, sizeof(FileEntryA.Hash)) != 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Pak file hash does not match its index entry"));
		bResult = false;
	}
	return bResult;
}

bool FPakPlatformFile::IsNonPakFilenameAllowed(const FString& InFilename)
{
	bool bAllowed = true;

#if EXCLUDE_NONPAK_UE_EXTENSIONS
	if (PakFiles.Num() || UE_BUILD_SHIPPING)
	{
		FName Ext = FName(*FPaths::GetExtension(InFilename));
		bAllowed = !ExcludedNonPakExtensions.Contains(Ext);
		UE_CLOG(!bAllowed, LogPakFile, VeryVerbose, TEXT("Access to file '%s' is limited to pak contents due to file extension being listed in ExcludedNonPakExtensions."), *InFilename)
	}
#endif

	bool bIsIniFile = InFilename.EndsWith(IniFileExtension);
#if DISABLE_NONUFS_INI_WHEN_COOKED
	bool bSkipIniFile = bIsIniFile && !InFilename.EndsWith(GameUserSettingsIniFilename);
	if (FPlatformProperties::RequiresCookedData() && bSkipIniFile)
	{
		bAllowed = false;
	}
#endif
#if ALLOW_INI_OVERRIDE_FROM_COMMANDLINE
	FString FileList;
	if (bIsIniFile && FParse::Value(FCommandLine::Get(), TEXT("-iniFile="), FileList, false))
	{
		TArray<FString> Files;
		FileList.ParseIntoArray(Files, TEXT(","), true);
		for (int32 Index = 0; Index < Files.Num(); Index++)
		{
			if (InFilename == Files[Index])
			{
				bAllowed = true;
				UE_LOG(LogPakFile, Log, TEXT(" Override -inifile: %s"), *InFilename);
				break;
			}
		}
	}
#endif
#if !DISABLE_CHEAT_CVARS && !UE_BUILD_SHIPPING
	if (bIsIniFile && !bAllowed)
	{
		FString OverrideConsoleVariablesPath;
		FParse::Value(FCommandLine::Get(), TEXT("-cvarsini="), OverrideConsoleVariablesPath);

		if (!OverrideConsoleVariablesPath.IsEmpty() && InFilename == OverrideConsoleVariablesPath)
		{
			bAllowed = true;
		}
	}
#endif

	FFilenameSecurityDelegate& FilenameSecurityDelegate = GetFilenameSecurityDelegate();
	if (bAllowed)
	{
		if (FilenameSecurityDelegate.IsBound())
		{
			bAllowed = FilenameSecurityDelegate.Execute(*InFilename);;
		}
	}

	return bAllowed;
}

#if !HAS_PLATFORM_PAK_INSTALL_CHECK
bool FPakPlatformFile::IsPakFileInstalled(const FString& InFilename)
{
#if ENABLE_PLATFORM_CHUNK_INSTALL
	IPlatformChunkInstall* ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
	if (ChunkInstall)
	{
		// if a platform supports chunk style installs, make sure that the chunk a pak file resides in is actually fully installed before accepting pak files from it
		int32 PakchunkIndex = GetPakchunkIndexFromPakFile(InFilename);
		if (PakchunkIndex != INDEX_NONE)
		{
			if (ChunkInstall->GetPakchunkLocation(PakchunkIndex) == EChunkLocation::NotAvailable)
			{
				return false;
			}
		}
	}
#endif

	return true;
}
#endif //HAS_PLATFORM_PAK_INSTALL_CHECK



FSharedPakReader::FSharedPakReader(FArchive* InArchive, FPakFile* InPakFile)
	: Archive(InArchive)
	, PakFile(InPakFile)
{
	check(PakFile);
}

FSharedPakReader::~FSharedPakReader()
{
	if(Archive)
	{
		PakFile->ReturnSharedReader(Archive);
		Archive = nullptr;
	}
}

FSharedPakReader::FSharedPakReader(FSharedPakReader&& Other)
	: Archive(Other.Archive)
	, PakFile(Other.PakFile)
{
	Other.Archive = nullptr;
	Other.PakFile = nullptr;
}

FSharedPakReader& FSharedPakReader::operator=(FSharedPakReader&& Other)
{
	if (Archive)
	{
		PakFile->ReturnSharedReader(Archive);
	}

	Archive = Other.Archive;
	PakFile = Other.PakFile;

	Other.Archive = nullptr;
	Other.PakFile = nullptr;

	return *this;
}


#if IS_PROGRAM
FPakFile::FPakFile(const TCHAR* Filename, bool bIsSigned)
	: PakFilename(Filename)
	, PakFilenameName(Filename)
	, PathHashSeed(0)
	, NumEntries(0)
	, CachedTotalSize(0)
	, bSigned(bIsSigned)
	, bIsValid(false)
	, bHasPathHashIndex(false)
	, bHasFullDirectoryIndex(false)
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	, bWillPruneDirectoryIndex(false)
	, bNeedsLegacyPruning(false)
#endif
	, PakchunkIndex(GetPakchunkIndexFromPakFile(Filename))
 	, MappedFileHandle(nullptr)
	, CacheType(FPakFile::ECacheType::Shared)
	, CacheIndex(-1)
	, UnderlyingCacheTrimDisabled(false)
	, bIsMounted(false)
{
	FSharedPakReader Reader = GetSharedReader(NULL);
	if (Reader)
	{
		Timestamp = IFileManager::Get().GetTimeStamp(Filename);
		Initialize(Reader.GetArchive());
	}
}
#endif

FPakFile::FPakFile(IPlatformFile* LowerLevel, const TCHAR* Filename, bool bIsSigned, bool bLoadIndex)
	: PakFilename(Filename)
	, PakFilenameName(Filename)
	, PathHashSeed(0)
	, NumEntries(0)
	, CachedTotalSize(0)
	, bSigned(bIsSigned)
	, bIsValid(false)
	, bHasPathHashIndex(false)
	, bHasFullDirectoryIndex(false)
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	, bWillPruneDirectoryIndex(false)
	, bNeedsLegacyPruning(false)
#endif
	, PakchunkIndex(GetPakchunkIndexFromPakFile(Filename))
	, MappedFileHandle(nullptr)
	, CacheType(FPakFile::ECacheType::Shared)
	, CacheIndex(-1)
	, UnderlyingCacheTrimDisabled(false)
	, bIsMounted(false)
{
	FSharedPakReader Reader = GetSharedReader(LowerLevel);
	if (Reader)
	{
		Timestamp = LowerLevel->GetTimeStamp(Filename);
		Initialize(Reader.GetArchive(), bLoadIndex);
	}
}

#if WITH_EDITOR
FPakFile::FPakFile(FArchive* Archive)
	: PathHashSeed(0)
	, NumEntries(0)
	, bSigned(false)
	, bIsValid(false)
	, bHasPathHashIndex(false)
	, bHasFullDirectoryIndex(false)
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	, bWillPruneDirectoryIndex(false)
	, bNeedsLegacyPruning(false)
#endif
	, PakchunkIndex(INDEX_NONE)
	, MappedFileHandle(nullptr)
	, CacheType(FPakFile::ECacheType::Shared)
	, CacheIndex(-1)
	, UnderlyingCacheTrimDisabled(false)
	, bIsMounted(false)
{
	Initialize(*Archive);
}
#endif

FPakFile::~FPakFile()
{
	delete MappedFileHandle;
}

bool FPakFile::PassedSignatureChecks() const
{
	return Decryptor.IsValid() && Decryptor->IsValid();
}


FArchive* FPakFile::CreatePakReader(IPlatformFile* LowerLevel, const TCHAR* Filename)
{
	auto MakeArchive = [&]() -> FArchive* { 
		if (LowerLevel)
		{
			if( IFileHandle* Handle = LowerLevel->OpenRead(Filename) )
			{
				return new FArchiveFileReaderGeneric(Handle, Filename, Handle->Size());
			}
			else
			{
				return nullptr;
			}
		}
		else
		{
			return IFileManager::Get().CreateFileReader(Filename); 
		}
	};

	bool bNeedsDecryptor = false;
	if (FPlatformProperties::RequiresCookedData())
	{
		bool bShouldCheckSignature = bSigned || FParse::Param(FCommandLine::Get(), TEXT("signedpak")) || FParse::Param(FCommandLine::Get(), TEXT("signed"));
#if !UE_BUILD_SHIPPING
		bShouldCheckSignature &= !FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog"));
#endif
		if (bShouldCheckSignature)
		{
			bNeedsDecryptor = true;
		}			
	}

	if(bNeedsDecryptor && !Decryptor.IsValid())
	{
		TUniquePtr<FArchive> DecryptorReader{ MakeArchive() };
		if (DecryptorReader.IsValid())
		{
			Decryptor = MakeUnique<FChunkCacheWorker>(MoveTemp(DecryptorReader), Filename);
		}

		if (!Decryptor.IsValid() || !Decryptor->IsValid())
		{
			return nullptr;
		}
	}

	// Now we either have a Decryptor or we don't need it
	check(!bNeedsDecryptor || Decryptor.IsValid());

	TUniquePtr<FArchive> Archive{ MakeArchive() };
	if (!Archive.IsValid())
	{
		return nullptr;
	}

	if (Decryptor.IsValid())
	{
		return new FSignedArchiveReader(Archive.Release(), Decryptor.Get());
	}
	else
	{
		return Archive.Release();
	}
}

bool ShouldCheckPak()
{
	static bool bShouldCheckPak = FParse::Param(FCommandLine::Get(), TEXT("checkpak"));
	return bShouldCheckPak;
}

void FPakFile::Initialize(FArchive& Reader, bool bLoadIndex)
{
	CachedTotalSize = Reader.TotalSize();
	bool bShouldLoad = false;
	int32 CompatibleVersion = FPakInfo::PakFile_Version_Latest;

	LLM_SCOPE(ELLMTag::FileSystem);

	// Serialize trailer and check if everything is as expected.
	// start up one to offset the -- below
	CompatibleVersion++;
	int64 FileInfoPos = -1;
	do
	{
		// try the next version down
		CompatibleVersion--;

		FileInfoPos = CachedTotalSize - Info.GetSerializedSize(CompatibleVersion);
		if (FileInfoPos >= 0)
		{
			Reader.Seek(FileInfoPos);
			Reader.Precache(FileInfoPos, 0); // Inform the archive that we're going to repeatedly serialize from the current location

			SCOPED_BOOT_TIMING("PakFile_SerilizeTrailer");

			// Serialize trailer and check if everything is as expected.
			Info.Serialize(Reader, CompatibleVersion);
			if (Info.Magic == FPakInfo::PakFile_Magic)
			{
				bShouldLoad = true;
			}
		}
	} while (!bShouldLoad && CompatibleVersion >= FPakInfo::PakFile_Version_Initial);

	if (bShouldLoad)
	{
		UE_CLOG(Info.Magic != FPakInfo::PakFile_Magic, LogPakFile, Fatal, TEXT("Trailing magic number (%ud) in '%s' is different than the expected one. Verify your installation."), Info.Magic, *PakFilename);
		UE_CLOG(!(Info.Version >= FPakInfo::PakFile_Version_Initial && Info.Version <= CompatibleVersion), LogPakFile, Fatal, TEXT("Invalid pak file version (%d) in '%s'. Verify your installation."), Info.Version, *PakFilename);
		UE_CLOG(!(Info.IndexOffset >= 0 && Info.IndexOffset < CachedTotalSize), LogPakFile, Fatal, TEXT("Index offset for pak file '%s' is invalid (%lld is bigger than file size %lld)"), *PakFilename, Info.IndexOffset, CachedTotalSize);
		UE_CLOG(!((Info.IndexOffset + Info.IndexSize) >= 0 && (Info.IndexOffset + Info.IndexSize) <= CachedTotalSize), LogPakFile, Fatal, TEXT("Index end offset for pak file '%s' is invalid (%lld)"), *PakFilename, Info.IndexOffset + Info.IndexSize);

		// If we aren't using a dynamic encryption key, process the pak file using the embedded key
		if (!Info.EncryptionKeyGuid.IsValid() || UE::FEncryptionKeyManager::Get().ContainsKey(Info.EncryptionKeyGuid))
		{
			if (bLoadIndex)
			{
				LoadIndex(Reader);
			}

			if (ShouldCheckPak())
			{
				ensure(Check());
			}
		}

		if (Decryptor.IsValid())
		{
			TSharedPtr<const FPakSignatureFile, ESPMode::ThreadSafe> SignatureFile = Decryptor->GetSignatures();
			if (SignatureFile->SignatureData.Num() == UE_ARRAY_COUNT(FSHAHash::Hash))
			{
				bIsValid = (FMemory::Memcmp(SignatureFile->SignatureData.GetData(), Info.IndexHash.Hash, SignatureFile->SignatureData.Num()) == 0);
			}
			else
			{
				bIsValid = false;
			}
		}
		else
		{
			// LoadIndex should crash in case of an error, so just assume everything is ok if we got here.
			bIsValid = true;
		}
	}
}

void FPakFile::LoadIndex(FArchive& Reader)
{
	if (Info.Version >= FPakInfo::PakFile_Version_PathHashIndex)
	{
		if (!LoadIndexInternal(Reader))
		{
			// Index loading failed. Try again
			if (!LoadIndexInternal(Reader))
			{
				UE_LOG(LogPakFile, Fatal, TEXT("Corrupt pak index detected on pak file: %s"), *PakFilename);
			}
		}
	}
	else
	{
		SCOPED_BOOT_TIMING("PakFile_LoadLegacy");
		if (!LoadLegacyIndex(Reader))
		{
			// Index loading failed. Try again
			if (!LoadLegacyIndex(Reader))
			{
				UE_LOG(LogPakFile, Fatal, TEXT("Corrupt pak index detected on pak file: %s"), *PakFilename);
			}
		}
	}
}

bool FPakFile::LoadIndexInternal(FArchive& Reader)
{
	bHasPathHashIndex = false;
	bHasFullDirectoryIndex = false;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	bNeedsLegacyPruning = false;
	bWillPruneDirectoryIndex = false;
#endif

	FGuardedInt64 IndexEndPosition = FGuardedInt64(Info.IndexOffset) + Info.IndexSize;
	if (Info.IndexOffset < 0 || 
		Info.IndexSize < 0 ||
		IndexEndPosition.InvalidOrGreaterThan(CachedTotalSize) ||
		IntFitsIn<int32>(Info.IndexSize) == false)
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Corrupted index offset in pak file."));
		return false;
	}

	TArray<uint8> PrimaryIndexData;
	Reader.Seek(Info.IndexOffset);
	PrimaryIndexData.SetNum((int32)(Info.IndexSize));
	{
		SCOPED_BOOT_TIMING("PakFile_LoadPrimaryIndex");
		Reader.Serialize(PrimaryIndexData.GetData(), Info.IndexSize);
	}

	FSHAHash ComputedHash;
	{
		SCOPED_BOOT_TIMING("PakFile_HashPrimaryIndex");
		if (!DecryptAndValidateIndex(Reader, PrimaryIndexData, Info.IndexHash, ComputedHash))
		{
			UE_LOG(LogPakFile, Log, TEXT("Corrupt pak PrimaryIndex detected!"));
			UE_LOG(LogPakFile, Log, TEXT(" Filename: %s"), *PakFilename);
			UE_LOG(LogPakFile, Log, TEXT(" Encrypted: %d"), Info.bEncryptedIndex);
			UE_LOG(LogPakFile, Log, TEXT(" Total Size: %lld"), Reader.TotalSize());
			UE_LOG(LogPakFile, Log, TEXT(" Index Offset: %lld"), Info.IndexOffset);
			UE_LOG(LogPakFile, Log, TEXT(" Index Size: %lld"), Info.IndexSize);
			UE_LOG(LogPakFile, Log, TEXT(" Stored Index Hash: %s"), *Info.IndexHash.ToString());
			UE_LOG(LogPakFile, Log, TEXT(" Computed Index Hash: %s"), *ComputedHash.ToString());
			return false;
		}
	}

	FMemoryReader PrimaryIndexReader(PrimaryIndexData);

	// Read the scalar data (mount point, numentries, etc) and all entries.
	NumEntries = 0;
	PrimaryIndexReader << MountPoint;
	// We are just deserializing a string, which could get however long. Since we know it's a path
	// and paths are bound to file system rules, we know it can't get absurdly long (e.g. windows is _at best_ 32k)
	// we just sanity check it to prevent operating on massive buffers and risking overflows.
	if (MountPoint.Len() > 65535)
	{
		UE_LOG(LogPakFile, Error, TEXT("Corrupt pak index data: MountPoint path is longer than 65k"));
		return false;
	}

	MakeDirectoryFromPath(MountPoint);
	PrimaryIndexReader << NumEntries;
	if (NumEntries < 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Corrupt pak index data: Negative entries count in pak file."));
		return false;
	}
	PrimaryIndexReader << PathHashSeed;

	bool bReaderHasPathHashIndex = false;
	int64 PathHashIndexOffset = INDEX_NONE;
	int64 PathHashIndexSize = 0;
	FSHAHash PathHashIndexHash;
	PrimaryIndexReader << bReaderHasPathHashIndex;
	if (bReaderHasPathHashIndex)
	{
		PrimaryIndexReader << PathHashIndexOffset;
		PrimaryIndexReader << PathHashIndexSize;
		PrimaryIndexReader << PathHashIndexHash;
		bReaderHasPathHashIndex = bReaderHasPathHashIndex && PathHashIndexOffset != INDEX_NONE;
	}

	bool bReaderHasFullDirectoryIndex = false;
	int64 FullDirectoryIndexOffset = INDEX_NONE;
	int64 FullDirectoryIndexSize = 0;
	FSHAHash FullDirectoryIndexHash;
	PrimaryIndexReader << bReaderHasFullDirectoryIndex;
	if (bReaderHasFullDirectoryIndex)
	{
		PrimaryIndexReader << FullDirectoryIndexOffset;
		PrimaryIndexReader << FullDirectoryIndexSize;
		PrimaryIndexReader << FullDirectoryIndexHash;
		bReaderHasFullDirectoryIndex = bReaderHasFullDirectoryIndex && FullDirectoryIndexOffset  != INDEX_NONE;
	}
	{
		SCOPED_BOOT_TIMING("PakFile_SerializeEncodedEntries");
		PrimaryIndexReader << EncodedPakEntries;
	}

	int32 FilesNum = 0;
	PrimaryIndexReader << FilesNum;
	if (FilesNum < 0)
	{
		// Should not be possible for any values in the PrimaryIndex to be invalid, since we verified the index hash
		UE_LOG(LogPakFile, Log, TEXT("Corrupt pak PrimaryIndex detected!"));
		UE_LOG(LogPakFile, Log, TEXT(" FilesNum: %d"), FilesNum);
		return false;
	}
	Files.SetNum(FilesNum);
	if (FilesNum > 0)
	{
		SCOPED_BOOT_TIMING("PakFile_SerializeUnencodedEntries");
		FPakEntry* FileEntries = Files.GetData();
		for (int32 FileIndex = 0; FileIndex < FilesNum; ++FileIndex)
		{
			FileEntries[FileIndex].Serialize(PrimaryIndexReader, Info.Version);
		}
	}

	// Decide which SecondaryIndex(es) to load
	bool bWillUseFullDirectoryIndex;
	bool bWillUsePathHashIndex;
	bool bReadFullDirectoryIndex;
	if (bReaderHasPathHashIndex && bReaderHasFullDirectoryIndex)
	{
		bWillUseFullDirectoryIndex = IsPakKeepFullDirectory();
		bWillUsePathHashIndex = !bWillUseFullDirectoryIndex;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
		bool bWantToReadFullDirectoryIndex = IsPakKeepFullDirectory() || IsPakValidatePruning() || IsPakDelayPruning();
#else
		bool bWantToReadFullDirectoryIndex = IsPakKeepFullDirectory();
#endif
		bReadFullDirectoryIndex = bReaderHasFullDirectoryIndex && bWantToReadFullDirectoryIndex;
	}
	else if (bReaderHasPathHashIndex)
	{
		bWillUsePathHashIndex = true;
		bWillUseFullDirectoryIndex = false;
		bReadFullDirectoryIndex = false;
	}
	else if (bReaderHasFullDirectoryIndex)
	{
		// We don't support creating the PathHash Index at runtime; we want to move to having only the PathHashIndex, so supporting not having it at all is not useful enough to write
		bWillUsePathHashIndex = false;
		bWillUseFullDirectoryIndex = true;
		bReadFullDirectoryIndex = true;
	}
	else
	{
		// It should not be possible for PrimaryIndexes to be built without a PathHashIndex AND without a FullDirectoryIndex; CreatePakFile in UnrealPak.exe has a check statement for it.
		UE_LOG(LogPakFile, Log, TEXT("Corrupt pak PrimaryIndex detected!"));
		UE_LOG(LogPakFile, Log, TEXT(" bReaderHasPathHashIndex: false"));
		UE_LOG(LogPakFile, Log, TEXT(" bReaderHasFullDirectoryIndex: false"));
		return false;
	}

	// Load the Secondary Index(es)
	TArray<uint8> PathHashIndexData;
	FMemoryReader PathHashIndexReader(PathHashIndexData);
	if (bWillUsePathHashIndex)
	{
		FGuardedInt64 PathHashIndexEndPosition = FGuardedInt64(PathHashIndexOffset) + PathHashIndexSize;
		if (PathHashIndexOffset < 0 || 
			PathHashIndexSize < 0 ||
			PathHashIndexEndPosition.InvalidOrGreaterThan(CachedTotalSize) || 
			IntFitsIn<int32>(PathHashIndexSize) == false)
		{
			// Should not be possible for these values (which came from the PrimaryIndex) to be invalid, since we verified the index hash of the PrimaryIndex
			UE_LOG(LogPakFile, Log, TEXT("Corrupt pak PrimaryIndex detected!"));
			UE_LOG(LogPakFile, Log, TEXT(" Filename: %s"), *PakFilename);
			UE_LOG(LogPakFile, Log, TEXT(" Total Size: %lld"), Reader.TotalSize());
			UE_LOG(LogPakFile, Log, TEXT(" PathHashIndexOffset : %lld"), PathHashIndexOffset);
			UE_LOG(LogPakFile, Log, TEXT(" PathHashIndexSize: %lld"), PathHashIndexSize);
			return false;
		}
		Reader.Seek(PathHashIndexOffset);
		PathHashIndexData.SetNum((int32)(PathHashIndexSize));
		{
			SCOPED_BOOT_TIMING("PakFile_LoadPathHashIndex");
			Reader.Serialize(PathHashIndexData.GetData(), PathHashIndexSize);
		}

		{
			SCOPED_BOOT_TIMING("PakFile_HashPathHashIndex");
			if (!DecryptAndValidateIndex(Reader, PathHashIndexData, PathHashIndexHash, ComputedHash))
			{
				UE_LOG(LogPakFile, Log, TEXT("Corrupt pak PathHashIndex detected!"));
				UE_LOG(LogPakFile, Log, TEXT(" Filename: %s"), *PakFilename);
				UE_LOG(LogPakFile, Log, TEXT(" Encrypted: %d"), Info.bEncryptedIndex);
				UE_LOG(LogPakFile, Log, TEXT(" Total Size: %lld"), Reader.TotalSize());
				UE_LOG(LogPakFile, Log, TEXT(" Index Offset: %lld"), FullDirectoryIndexOffset);
				UE_LOG(LogPakFile, Log, TEXT(" Index Size: %lld"), FullDirectoryIndexSize);
				UE_LOG(LogPakFile, Log, TEXT(" Stored Index Hash: %s"), *PathHashIndexHash.ToString());
				UE_LOG(LogPakFile, Log, TEXT(" Computed Index Hash: %s"), *ComputedHash.ToString());
				return false;
			}
		}

		{
			SCOPED_BOOT_TIMING("PakFile_SerializePathHashIndex");
			PathHashIndexReader << PathHashIndex;
		}
		bHasPathHashIndex = true;
	}
	
	if (!bReadFullDirectoryIndex)
	{
		check(bWillUsePathHashIndex); // Need to confirm that we have read the PathHashIndex bytes
		// Store the PrunedDirectoryIndex in our DirectoryIndex
		{
			SCOPED_BOOT_TIMING("PakFile_SerializePrunedDirectoryIndex");
			PathHashIndexReader << DirectoryIndex;
		}
		bHasFullDirectoryIndex = false;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
		bWillPruneDirectoryIndex = false;
#endif
	}
	else
	{
		FGuardedInt64 FullDirectoryIndexEndPosition = FGuardedInt64(FullDirectoryIndexOffset) + FullDirectoryIndexSize;
		if (FullDirectoryIndexOffset  < 0 || 
			FullDirectoryIndexSize < 0 ||
			FullDirectoryIndexEndPosition.InvalidOrGreaterThan(CachedTotalSize) || 
			IntFitsIn<int32>(FullDirectoryIndexSize) == false)
		{
			// Should not be possible for these values (which came from the PrimaryIndex) to be invalid, since we verified the index hash of the PrimaryIndex
			UE_LOG(LogPakFile, Log, TEXT("Corrupt pak PrimaryIndex detected!"));
			UE_LOG(LogPakFile, Log, TEXT(" Filename: %s"), *PakFilename);
			UE_LOG(LogPakFile, Log, TEXT(" Total Size: %lld"), Reader.TotalSize());
			UE_LOG(LogPakFile, Log, TEXT(" FullDirectoryIndexOffset : %lld"), FullDirectoryIndexOffset );
			UE_LOG(LogPakFile, Log, TEXT(" FullDirectoryIndexSize: %lld"), FullDirectoryIndexSize);
			return false;
		}
		TArray<uint8> FullDirectoryIndexData;
		Reader.Seek(FullDirectoryIndexOffset );
		FullDirectoryIndexData.SetNum((int32)(FullDirectoryIndexSize));
		{
			SCOPED_BOOT_TIMING("PakFile_LoadDirectoryIndex");
			Reader.Serialize(FullDirectoryIndexData.GetData(), FullDirectoryIndexSize);
		}

		{
			SCOPED_BOOT_TIMING("PakFile_HashDirectoryIndex");
			if (!DecryptAndValidateIndex(Reader, FullDirectoryIndexData, FullDirectoryIndexHash, ComputedHash))
			{
				UE_LOG(LogPakFile, Log, TEXT("Corrupt pak FullDirectoryIndex detected!"));
				UE_LOG(LogPakFile, Log, TEXT(" Filename: %s"), *PakFilename);
				UE_LOG(LogPakFile, Log, TEXT(" Encrypted: %d"), Info.bEncryptedIndex);
				UE_LOG(LogPakFile, Log, TEXT(" Total Size: %lld"), Reader.TotalSize());
				UE_LOG(LogPakFile, Log, TEXT(" Index Offset: %lld"), FullDirectoryIndexOffset);
				UE_LOG(LogPakFile, Log, TEXT(" Index Size: %lld"), FullDirectoryIndexSize);
				UE_LOG(LogPakFile, Log, TEXT(" Stored Index Hash: %s"), *FullDirectoryIndexHash.ToString());
				UE_LOG(LogPakFile, Log, TEXT(" Computed Index Hash: %s"), *ComputedHash.ToString());
				return false;
			}
		}

		FMemoryReader SecondaryIndexReader(FullDirectoryIndexData);
		{
			SCOPED_BOOT_TIMING("PakFile_SerializeDirectoryIndex");
			SecondaryIndexReader << DirectoryIndex;
		}
		bHasFullDirectoryIndex = true;

#if ENABLE_PAKFILE_RUNTIME_PRUNING
		if (bWillUseFullDirectoryIndex)
		{
			bWillPruneDirectoryIndex = false;
		}
		else
		{
			// Store the PrunedDirectoryIndex from the PrimaryIndex in our PrunedDirectoryIndex, to be used for verification and to be swapped into DirectoryIndex later
			check(bWillUsePathHashIndex); // Need to confirm that we have read the PathHashIndex bytes
			{
				SCOPED_BOOT_TIMING("PakFile_SerializePrunedDirectoryIndex");
				PathHashIndexReader << PrunedDirectoryIndex;
			}
			bWillPruneDirectoryIndex = true;
			bSomePakNeedsPruning = true;
		}
#endif
	}

	UE_LOG(LogPakFile, Verbose, TEXT("PakFile PrimaryIndexSize=%d"), Info.IndexSize);
	UE_LOG(LogPakFile, Verbose, TEXT("PakFile PathHashIndexSize=%d"), PathHashIndexSize);
	UE_LOG(LogPakFile, Verbose, TEXT("PakFile FullDirectoryIndexSize=%d"), FullDirectoryIndexSize);

	check(bHasFullDirectoryIndex || bHasPathHashIndex);
	return true;
}

bool FPakFile::LoadLegacyIndex(FArchive& Reader)
{
	bHasPathHashIndex = false;
	bHasFullDirectoryIndex = false;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	bNeedsLegacyPruning = false;
	bWillPruneDirectoryIndex = false;
#endif

	// Load index into memory first.
	FGuardedInt64 IndexEndPosition = FGuardedInt64(Info.IndexOffset) + Info.IndexSize;
	if (Info.IndexSize < 0 ||
		Info.IndexOffset < 0 ||
		IndexEndPosition.InvalidOrGreaterThan(CachedTotalSize) ||
		IntFitsIn<int32>(Info.IndexSize) == false)
	{
		UE_LOG(LogPakFile, Fatal, TEXT("Corrupted index offset/size in pak file."));
		return false;
	}

	TArray<uint8> IndexData;
	IndexData.SetNum((int32)(Info.IndexSize));

	Reader.Seek(Info.IndexOffset);
	Reader.Serialize(IndexData.GetData(), Info.IndexSize);

	FSHAHash ComputedHash;
	if (!DecryptAndValidateIndex(Reader, IndexData, Info.IndexHash, ComputedHash))
	{
		UE_LOG(LogPakFile, Log, TEXT("Corrupt pak index detected!"));
		UE_LOG(LogPakFile, Log, TEXT(" Filename: %s"), *PakFilename);
		UE_LOG(LogPakFile, Log, TEXT(" Encrypted: %d"), Info.bEncryptedIndex);
		UE_LOG(LogPakFile, Log, TEXT(" Total Size: %lld"), Reader.TotalSize());
		UE_LOG(LogPakFile, Log, TEXT(" Index Offset: %lld"), Info.IndexOffset);
		UE_LOG(LogPakFile, Log, TEXT(" Index Size: %lld"), Info.IndexSize);
		UE_LOG(LogPakFile, Log, TEXT(" Stored Index Hash: %s"), *Info.IndexHash.ToString());
		UE_LOG(LogPakFile, Log, TEXT(" Computed Index Hash: %s"), *ComputedHash.ToString());
		return false;
	}


	FMemoryReader IndexReader(IndexData);

	// Read the default mount point and all entries.
	NumEntries = 0;
	IndexReader << MountPoint;

	// We are just deserializing a string, which could get however long. Since we know it's a path
	// and paths are bound to file system rules, we know it can't get absurdly long (e.g. windows is _at best_ 32k)
	// we just sanity check it to prevent operating on massive buffers and risking overflows.
	if (MountPoint.Len() > 65535)
	{
		UE_LOG(LogPakFile, Error, TEXT("Corrupt pak index data: MountPoint path is longer than 65k"));
		return false;
	}
	IndexReader << NumEntries;

	if (NumEntries < 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Corrupt pak index data: NumEntries is negative"));
		return false;
	}

	MakeDirectoryFromPath(MountPoint);

	FPakEntryPair PakEntryPair;
	auto ReadNextEntry = [&PakEntryPair, &IndexReader, this]() -> FPakEntryPair&
	{
		IndexReader << PakEntryPair.Filename;
		PakEntryPair.Info.Reset();
		PakEntryPair.Info.Serialize(IndexReader, this->Info.Version);
		return PakEntryPair;
	};

	TMap<uint64, FString> CollisionDetection;
	int32 NumEncodedEntries = 0;
	int32 NumDeletedEntries = 0;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	bool bCreatePathHash = !IsPakKeepFullDirectory();
#else
	// Pruning of legacy files is no longer supported; we will keep the entire directory with no way to prune it.  There is no need to create the PathHashIndex since we will have the FullDirectoryIndex.
	bool bCreatePathHash = false;
#endif
	FPathHashIndex* PathHashToWrite = bCreatePathHash ? &PathHashIndex : nullptr;
	FPakFile::EncodePakEntriesIntoIndex(NumEntries, ReadNextEntry, *PakFilename, Info, MountPoint, NumEncodedEntries, NumDeletedEntries, &PathHashSeed,
		&DirectoryIndex, PathHashToWrite, EncodedPakEntries, Files, &CollisionDetection, Info.Version);
	check(NumEncodedEntries + Files.Num() + NumDeletedEntries == NumEntries);
	Files.Shrink();
	EncodedPakEntries.Shrink();

	bHasPathHashIndex = bCreatePathHash;
	bHasFullDirectoryIndex = true;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	if (!IsPakKeepFullDirectory())
	{
		bNeedsLegacyPruning = true;
		bWillPruneDirectoryIndex = true;
		bSomePakNeedsPruning = true;
		// We cannot prune during this call because config settings have not yet been loaded and we need the settings for DirectoryIndexKeepFiles before we can prune
		// PrunedDirectoryIndex will be created and swapped with DirectoryIndex in OptimizeMemoryUsageForMountedPaks, and bHasFullDirectoryIndex will be set to false then
	}
	else
	{
		bNeedsLegacyPruning = false;
		bWillPruneDirectoryIndex = false;
	}
#endif

	check(bHasFullDirectoryIndex || bHasPathHashIndex);
	return true;
}

bool FPakFile::DecryptAndValidateIndex(FArchive& Reader, TArray<uint8>& IndexData, FSHAHash& InExpectedHash, FSHAHash& OutActualHash)
{
	// Decrypt if necessary
	if (Info.bEncryptedIndex)
	{
		DecryptData(IndexData.GetData(), IndexData.Num(), Info.EncryptionKeyGuid);
	}

	// Check SHA1 value.
	FSHA1::HashBuffer(IndexData.GetData(), IndexData.Num(), OutActualHash.Hash);
	return InExpectedHash == OutActualHash;
}

/*** This is a copy of FFnv::MemFnv64 from before the bugfix for swapped Offset and Prime. It is used to decode legacy paks that have hashes created from the prebugfix version of the function */
static uint64 LegacyMemFnv64(const void* InData, int32 Length, uint64 InOffset)
{
	// constants from above reference
	static const uint64 Offset = 0x00000100000001b3;
	static const uint64 Prime = 0xcbf29ce484222325;

	const uint8* __restrict Data = (uint8*)InData;

	uint64 Fnv = Offset + InOffset; // this is not strictly correct as the offset should be prime and InOffset could be arbitrary
	for (; Length; --Length)
	{
		Fnv ^= *Data++;
		Fnv *= Prime;
	}

	return Fnv;
}

uint64 FPakFile::HashPath(const TCHAR* RelativePathFromMount, uint64 Seed, int32 PakFileVersion)
{
	FString LowercaseRelativePath(RelativePathFromMount);
	LowercaseRelativePath.ToLowerInline();
	if (PakFileVersion >= FPakInfo::PakFile_Version_Fnv64BugFix)
	{
		return FFnv::MemFnv64(*LowercaseRelativePath, LowercaseRelativePath.Len() * sizeof(TCHAR), Seed);
	}
	else
	{
		return LegacyMemFnv64(*LowercaseRelativePath, LowercaseRelativePath.Len() * sizeof(TCHAR), Seed);
	}
}

void FPakFile::EncodePakEntriesIntoIndex(int32 InNumEntries, const ReadNextEntryFunction& InReadNextEntry, const TCHAR* InPakFilename, const FPakInfo& InPakInfo, const FString& MountPoint,
	int32& OutNumEncodedEntries, int32& OutNumDeletedEntries, uint64* OutPathHashSeed,
	FDirectoryIndex* OutDirectoryIndex, FPathHashIndex* OutPathHashIndex, TArray<uint8>& OutEncodedPakEntries, TArray<FPakEntry>& OutNonEncodableEntries,
	TMap<uint64, FString>* InOutCollisionDetection, int32 PakFileVersion)
{
	uint64 PathHashSeed = 0;
	if (OutPathHashSeed || OutPathHashIndex)
	{
		FString LowercasePakFilename(InPakFilename);
		LowercasePakFilename.ToLowerInline();
		PathHashSeed = FCrc::StrCrc32(*LowercasePakFilename);
		if (OutPathHashSeed)
		{
			*OutPathHashSeed = PathHashSeed;
		}
	}

	OutNumEncodedEntries = 0;
	OutNumDeletedEntries = 0;
	FMemoryWriter CompressedEntryWriter(OutEncodedPakEntries);
	for (int32 EntryCount = 0; EntryCount < InNumEntries; ++EntryCount)
	{
		FPakEntryPair& Pair = InReadNextEntry();
		// Add the Entry and get an FPakEntryLocation for it
		const FPakEntry& PakEntry = Pair.Info;
		FPakEntryLocation EntryLocation;
		if (!PakEntry.IsDeleteRecord())
		{
			EntryLocation = FPakEntryLocation::CreateFromOffsetIntoEncoded(OutEncodedPakEntries.Num());
			if (EncodePakEntry(CompressedEntryWriter, PakEntry, InPakInfo))
			{
				++OutNumEncodedEntries;
			}
			else
			{
				int32 ListIndex = OutNonEncodableEntries.Num();
				EntryLocation = FPakEntryLocation::CreateFromListIndex(ListIndex);
				OutNonEncodableEntries.Add(PakEntry);

				// PakEntries in the index have some values that are different from the in-place pakentries stored next to each file's payload.  EncodePakEntry handles that internally if encoding succeeded.
				FPakEntry& StoredPakEntry = OutNonEncodableEntries[ListIndex];
				FMemory::Memset(StoredPakEntry.Hash, 0); // Hash is 0-filled
				StoredPakEntry.Verified = true; // Validation of the hash is impossible, so Verified is set to true
			}
		}
		else
		{
			++OutNumDeletedEntries;
		}

		// Add the Entry into the requested Indexes
		AddEntryToIndex(Pair.Filename, EntryLocation, MountPoint, PathHashSeed, OutDirectoryIndex, OutPathHashIndex, InOutCollisionDetection, PakFileVersion);
	}
}

void FPakFile::PruneDirectoryIndex(FDirectoryIndex& InOutDirectoryIndex, FDirectoryIndex* PrunedDirectoryIndex, const FString& MountPoint)
{
	// Caller holds WriteLock on DirectoryIndexLock
	TArray<FString> FileWildCards, DirectoryWildCards, OldWildCards;
	GConfig->GetArray(TEXT("Pak"), TEXT("DirectoryIndexKeepFiles"), FileWildCards, GEngineIni);
	GConfig->GetArray(TEXT("Pak"), TEXT("DirectoryIndexKeepEmptyDirectories"), DirectoryWildCards, GEngineIni);
	GConfig->GetArray(TEXT("Pak"), TEXT("DirectoryRootsToKeepInMemoryWhenUnloadingPakEntryFilenames"), OldWildCards, GEngineIni); // Legacy name, treated as both KeepFiles and KeepEmptyDirectories
	DirectoryWildCards.Append(OldWildCards);
	FileWildCards.Append(OldWildCards);
	int32 NumKeptEntries = 0;

	if (PrunedDirectoryIndex)
	{
		PrunedDirectoryIndex->Empty();
	}

	TMap<FString, bool> KeepDirectory;

	// Clear out those portions of the Index allowed by the user.
	if (DirectoryWildCards.Num() > 0 || FileWildCards.Num() > 0)
	{
		for (auto It = InOutDirectoryIndex.CreateIterator(); It; ++It)
		{
			const FString& DirectoryName = It.Key();
			FPakDirectory& OriginalDirectory = It.Value();
			const FString FullDirectoryName = PakPathCombine(MountPoint, DirectoryName);
			check(IsPathInDirectoryFormat(FullDirectoryName));
			FPakDirectory* PrunedDirectory = nullptr;
			bool bKeepDirectory = false;

			TArray<FString> RemoveFilenames;
			for (auto FileIt = It->Value.CreateIterator(); FileIt; ++FileIt)
			{
				const FString& FileNameWithoutPath = FileIt->Key;
				const FString FullFilename = PakPathCombine(FullDirectoryName, FileNameWithoutPath);
				bool bKeepFile = false;

				for (const FString& WildCard : FileWildCards)
				{
					if (FullFilename.MatchesWildcard(WildCard))
					{
						bKeepFile = true;
						break;
					}
				}

				if (bKeepFile)
				{
					bKeepDirectory = true;
					if (PrunedDirectoryIndex)
					{
						if (!PrunedDirectory)
						{
							PrunedDirectory = &PrunedDirectoryIndex->Add(DirectoryName);
						}
						PrunedDirectory->Add(FileNameWithoutPath, FileIt->Value);
					}
				}
				else
				{
					if (!PrunedDirectoryIndex)
					{
						RemoveFilenames.Add(FileNameWithoutPath);
					}
				}
			}
			if (!PrunedDirectoryIndex)
			{
				for (const FString& FileNameWithoutPath : RemoveFilenames)
				{
					OriginalDirectory.Remove(FileNameWithoutPath);
				}
			}

			if (!bKeepDirectory)
			{
				for (const FString& WildCard : DirectoryWildCards)
				{
					if (FullDirectoryName.MatchesWildcard(WildCard))
					{
						bKeepDirectory = true;
						break;
					}
				}
			}
			KeepDirectory.FindOrAdd(DirectoryName) = bKeepDirectory;
		}

		// For each kept directory, mark that we need to keep all of its parents up to the mount point
		for (const TPair<FString, bool>& Pair : KeepDirectory)
		{
			if (Pair.Value)
			{
				FString CurrentDirectory = Pair.Key;
				FString UnusedCleanFileName;
				while (CurrentDirectory != MountPoint)
				{
					if (!SplitPathInline(CurrentDirectory, UnusedCleanFileName))
					{
						break;
					}
					bool& bOldValue = KeepDirectory.FindOrAdd(CurrentDirectory);
					if (bOldValue)
					{
						break;
					}
					bOldValue = true;
				}
			}
		}

		// Prune all of the directories
		for (const TPair<FString, bool>& Pair : KeepDirectory)
		{
			const FString& DirectoryName = Pair.Key;
			bool bKeep = Pair.Value;
			if (bKeep)
			{
				if (PrunedDirectoryIndex)
				{
					PrunedDirectoryIndex->FindOrAdd(DirectoryName);
				}
			}
			else
			{
				if (!PrunedDirectoryIndex)
				{
					InOutDirectoryIndex.Remove(DirectoryName);
				}
			}
		}
	}
	else
	{
		if (!PrunedDirectoryIndex)
		{
			InOutDirectoryIndex.Empty();
		}
	}
}

FPakFile::EFindResult FPakFile::GetPakEntry(const FPakEntryLocation& PakEntryLocation, FPakEntry* OutEntry) const
{
	return GetPakEntry(PakEntryLocation, OutEntry, EncodedPakEntries, Files, Info);
}

FPakFile::EFindResult FPakFile::GetPakEntry(const FPakEntryLocation& PakEntryLocation, FPakEntry* OutEntry, const TArray<uint8>& EncodedPakEntries, const TArray<FPakEntry>& Files, const FPakInfo& Info)
{
	bool bDeleted = PakEntryLocation.IsInvalid();
	if (OutEntry != NULL)
	{
		if (!bDeleted)
		{
			// The FPakEntry structures are bit-encoded, so decode it.
			int32 EncodedOffset = PakEntryLocation.GetAsOffsetIntoEncoded();
			if (EncodedOffset >= 0)
			{
				check(EncodedOffset < EncodedPakEntries.Num());
				DecodePakEntry(EncodedPakEntries.GetData() + EncodedOffset, *OutEntry, Info);
			}
			else
			{
				int32 ListIndex = PakEntryLocation.GetAsListIndex();
				check(ListIndex >= 0);
				const FPakEntry& FoundEntry = Files[ListIndex];
				//*OutEntry = **FoundEntry;
				OutEntry->Offset = FoundEntry.Offset;
				OutEntry->Size = FoundEntry.Size;
				OutEntry->UncompressedSize = FoundEntry.UncompressedSize;
				OutEntry->CompressionMethodIndex = FoundEntry.CompressionMethodIndex;
				OutEntry->CompressionBlocks = FoundEntry.CompressionBlocks;
				OutEntry->CompressionBlockSize = FoundEntry.CompressionBlockSize;
				OutEntry->Flags = FoundEntry.Flags;
			}
		}
		else
		{
			// entry was deleted, build dummy entry to indicate the deleted entry
			(*OutEntry) = FPakEntry();
			OutEntry->SetDeleteRecord(true);
		}

		// Index PakEntries do not store their hash, so verification against the hash is impossible.
		// Initialize the OutEntry's Hash to 0 and mark it as already verified.
		// TODO: Verified and Hash are checked by FPakFileHandle, which is used when opening files from PakFiles synchronously;
		//       it is not currently used by asynchronous reads in FPakPrecacher, and we can likely remove it from use in FPakFileHandle as well
		FMemory::Memset(OutEntry->Hash, 0);
		OutEntry->Verified = true;
	}

	return bDeleted ? EFindResult::FoundDeleted : EFindResult::Found;
}

struct FPakFile::FIndexSettings
{
	FIndexSettings()
	{
		bKeepFullDirectory = true;
		bValidatePruning = false;
		bDelayPruning = false;
		bWritePathHashIndex = true;
		bWriteFullDirectoryIndex = true;

		// Paks are mounted before config files are read, so the licensee needs to hardcode all settings used for runtime index loading rather than specifying them in ini
		if (FPakPlatformFile::GetPakSetIndexSettingsDelegate().IsBound())
		{
			FPakPlatformFile::GetPakSetIndexSettingsDelegate().Execute(bKeepFullDirectory, bValidatePruning, bDelayPruning);
		}

		// Settings not used at runtime can be read from ini
#if !UE_BUILD_SHIPPING
		GConfig->GetBool(TEXT("Pak"), TEXT("WritePathHashIndex"), bWritePathHashIndex, GEngineIni);
		GConfig->GetBool(TEXT("Pak"), TEXT("WriteFullDirectoryIndex"), bWriteFullDirectoryIndex, GEngineIni);
#endif

#if IS_PROGRAM || WITH_EDITOR
		// Directory pruning is not enabled in the editor or in development programs because there is no need to save the memory in those environments and some development features require not pruning
		bKeepFullDirectory = true;
#else
		bKeepFullDirectory = bKeepFullDirectory || !FPlatformProperties::RequiresCookedData();
#endif
#if !UE_BUILD_SHIPPING
		const TCHAR* CommandLine = FCommandLine::Get();
		FParse::Bool(CommandLine, TEXT("ForcePakKeepFullDirectory="), bKeepFullDirectory);
#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
		FParse::Bool(CommandLine, TEXT("ForcePakValidatePruning="), bValidatePruning);
		FParse::Bool(CommandLine, TEXT("ForcePakDelayPruning="), bDelayPruning);
#endif
		FParse::Bool(CommandLine, TEXT("ForcePakWritePathHashIndex="), bWritePathHashIndex);
		FParse::Bool(CommandLine, TEXT("ForcePakWriteFullDirectoryIndex="), bWriteFullDirectoryIndex);
#endif
	}

	bool bKeepFullDirectory;
	bool bValidatePruning;
	bool bDelayPruning;
	bool bWritePathHashIndex;
	bool bWriteFullDirectoryIndex;
};

FPakFile::FIndexSettings& FPakFile::GetIndexSettings()
{
	static FIndexSettings IndexLoadParams;
	return IndexLoadParams;
}

bool FPakFile::IsPakKeepFullDirectory()
{
	FIndexSettings& IndexLoadParams = GetIndexSettings();
	return IndexLoadParams.bKeepFullDirectory;
}

bool FPakFile::IsPakValidatePruning()
{
#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
	FIndexSettings& IndexLoadParams = GetIndexSettings();
	return IndexLoadParams.bValidatePruning;
#else
	return false;
#endif
}

bool FPakFile::IsPakDelayPruning()
{
	FIndexSettings& IndexLoadParams = GetIndexSettings();
	return IndexLoadParams.bDelayPruning;
}

bool FPakFile::IsPakWritePathHashIndex()
{
	FIndexSettings& IndexLoadParams = GetIndexSettings();
	return IndexLoadParams.bWritePathHashIndex;
}

bool FPakFile::IsPakWriteFullDirectoryIndex()
{
	FIndexSettings& IndexLoadParams = GetIndexSettings();
	return IndexLoadParams.bWriteFullDirectoryIndex;
}

bool FPakFile::RequiresDirectoryIndexLock() const
{
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	return bWillPruneDirectoryIndex;
#else
	return false; 
#endif
}

bool FPakFile::ShouldValidatePrunedDirectory() const
{
#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
	return IsPakValidatePruning() && bWillPruneDirectoryIndex && !bNeedsLegacyPruning;
#else
	return false;
#endif
}


void FPakFile::AddEntryToIndex(const FString& Filename, const FPakEntryLocation& EntryLocation, const FString& MountPoint, uint64 PathHashSeed,
	FDirectoryIndex* DirectoryIndex, FPathHashIndex* PathHashIndex, TMap<uint64, FString>* CollisionDetection, int32 PakFileVersion)
{
	FString RelativePathFromMount;
	if (FPaths::IsRelative(Filename))
	{
		RelativePathFromMount = Filename;
	}
	else
	{
		check(IsPathInDirectoryFormat(MountPoint));
		RelativePathFromMount = Filename;
		check(Filename.Len() > MountPoint.Len());
		bool bSucceeded = GetRelativePathFromMountInline(RelativePathFromMount, MountPoint);
		check(bSucceeded);
	}

	if (DirectoryIndex)
	{
		FString RelativeDirectoryFromMount = RelativePathFromMount;
		FString CleanFileName;
		SplitPathInline(RelativeDirectoryFromMount, CleanFileName);
		FPakDirectory* Directory = DirectoryIndex->Find(RelativeDirectoryFromMount);
		if (Directory == nullptr)
		{
			// add the parent directories up to the mount point (mount point relative path is "/")
			FString CurrentDirectory(RelativeDirectoryFromMount);
			FString UnusedCleanFileName;
			while (!CurrentDirectory.IsEmpty())
			{
				// FPaths::GetPath doesn't handle our / at the end of directories, so call FindLastChar(/)
				if (!SplitPathInline(CurrentDirectory, UnusedCleanFileName))
				{
					break;
				}
				DirectoryIndex->FindOrAdd(CurrentDirectory);
			}

			// Add the new directory; this has to come after the addition of the parent directories because we want to use the pointer afterwards and adding other directories would invalidate it
			Directory = &DirectoryIndex->Add(RelativeDirectoryFromMount);
		}
		Directory->Add(CleanFileName, EntryLocation);
	}

	// Add the entry into the PathHash index
	if (CollisionDetection || PathHashIndex)
	{
		uint64 PathHash = FPakFile::HashPath(*RelativePathFromMount, PathHashSeed, PakFileVersion);
		if (CollisionDetection)
		{
			FString* ExistingFilename = CollisionDetection->Find(PathHash);
			checkf(!ExistingFilename || ExistingFilename->Equals(RelativePathFromMount), TEXT("Hash collision for two Filenames within a PakFile.  Filename1 == '%s'.  Filename2 == '%s'.  Hash='0x%x'.")
				TEXT(" Collision handling is not yet implemented; to resolve the conflict, modify one of the Filenames."), **ExistingFilename, *RelativePathFromMount, PathHash);
			CollisionDetection->Add(PathHash, RelativePathFromMount);
		}
		if (PathHashIndex)
		{
			PathHashIndex->Add(PathHash, EntryLocation);
		}
	}
}

// Take a pak entry and byte encode it into a smaller representation
bool FPakFile::EncodePakEntry(FArchive& Ar, const FPakEntry& InPakEntry, const FPakInfo& InInfo)
{
	// See notes in DecodePakEntry for the output layout

	check(Ar.IsSaving()); // This function is encode only, and promises not to modify InEntry, but we want to use << which takes non-const so we need to assert that Ar is a loader not a saver
	check(!InPakEntry.IsDeleteRecord()); // Deleted PakEntries cannot be encoded, caller must check for IsDeleteRecord and handle it separately by e.g. not adding the Entry to the FileList and instead giving the referencer an invalid FPakEntryLocation
	FPakEntry& PakEntry = const_cast<FPakEntry&>(InPakEntry);

	const uint32 CompressedBlockAlignment = PakEntry.IsEncrypted() ? FAES::AESBlockSize : 1;
	const int64 HeaderSize = PakEntry.GetSerializedSize(InInfo.Version);

	// This data fits into a bitfield (described in DecodePakEntry), and the data has
	// to fit within a certain range of bits.
	if (PakEntry.CompressionMethodIndex >= (1 << 6))
	{
		return false;
	}
	if (PakEntry.CompressionBlocks.Num() >= (1 << 16))
	{
		return false;
	}
	if (PakEntry.CompressionMethodIndex != 0)
	{
		if (PakEntry.CompressionBlocks.Num() > 0 && ((InInfo.HasRelativeCompressedChunkOffsets() ? 0 : PakEntry.Offset) + HeaderSize != PakEntry.CompressionBlocks[0].CompressedStart))
		{
			return false;
		}
		if (PakEntry.CompressionBlocks.Num() == 1)
		{
			uint64 Base = InInfo.HasRelativeCompressedChunkOffsets() ? 0 : PakEntry.Offset;
			uint64 AlignedBlockSize = Align(PakEntry.CompressionBlocks[0].CompressedEnd - PakEntry.CompressionBlocks[0].CompressedStart, CompressedBlockAlignment);
			if ((Base + HeaderSize + PakEntry.Size) != (PakEntry.CompressionBlocks[0].CompressedStart + AlignedBlockSize))
			{
				return false;
			}
		}
		if (PakEntry.CompressionBlocks.Num() > 1)
		{
			for (int i = 1; i < PakEntry.CompressionBlocks.Num(); ++i)
			{
				uint64 PrevBlockSize = PakEntry.CompressionBlocks[i - 1].CompressedEnd - PakEntry.CompressionBlocks[i - 1].CompressedStart;
				PrevBlockSize = Align(PrevBlockSize, CompressedBlockAlignment);
				if (PakEntry.CompressionBlocks[i].CompressedStart != (PakEntry.CompressionBlocks[i - 1].CompressedStart + PrevBlockSize))
				{
					return false;
				}
			}
		}
	}

	// This entry can be encoded, so let's do it!

	bool bIsOffset32BitSafe = PakEntry.Offset <= MAX_uint32;
	bool bIsSize32BitSafe = PakEntry.Size <= MAX_uint32;
	bool bIsUncompressedSize32BitSafe = PakEntry.UncompressedSize <= MAX_uint32;
	
	// If CompressionBlocks.Num() == 1, we set CompressionBlockSize == UncompressedSize and record CompressBlockSizePacked=0
	// Otherwise, we encode CompressionBlockSize as a 6-bit multiple of 1 << 11.
	// If CompressionBlockSize is not a multiple of 1 << 11, or is a larger multiple than 6 bits we can not encode correctly.
	// In that case we set the packed field to its maximum value (0x3F) and send the unencoded CompressionBlockSize as a separate value.
	uint32 CompressionBlockSizePacked = 0;
	if (PakEntry.CompressionBlocks.Num() > 1)
	{
		CompressionBlockSizePacked = (PakEntry.CompressionBlockSize >> 11) & 0x3F;
		if ((CompressionBlockSizePacked << 11) != PakEntry.CompressionBlockSize)
		{
			CompressionBlockSizePacked = 0x3F;
		}
	}

	// Build the Flags field.
	uint32 Flags =
		(bIsOffset32BitSafe ? (1 << 31) : 0)
		| (bIsUncompressedSize32BitSafe ? (1 << 30) : 0)
		| (bIsSize32BitSafe ? (1 << 29) : 0)
		| (PakEntry.CompressionMethodIndex << 23)
		| (PakEntry.IsEncrypted() ? (1 << 22) : 0)
		| (PakEntry.CompressionBlocks.Num() << 6)
		| CompressionBlockSizePacked
		;

	Ar << Flags;
	
	// if we write 0x3F for CompressionBlockSize then send the field
	if ( CompressionBlockSizePacked == 0x3F )
	{
		uint32 Value = (uint32)PakEntry.CompressionBlockSize;
		Ar << Value;
	}

	// Build the Offset field.
	if (bIsOffset32BitSafe)
	{
		uint32 Value = (uint32)PakEntry.Offset;
		Ar << Value;
	}
	else
	{
		Ar << PakEntry.Offset;
	}

	// Build the Uncompressed Size field.
	if (bIsUncompressedSize32BitSafe)
	{
		uint32 Value = (uint32)PakEntry.UncompressedSize;
		Ar << Value;
	}
	else
	{
		Ar << PakEntry.UncompressedSize;
	}

	// Any additional data is for compressed file data.
	if (PakEntry.CompressionMethodIndex != 0)
	{
		// Build the Compressed Size field.
		if (bIsSize32BitSafe)
		{
			uint32 Value = (uint32)PakEntry.Size;
			Ar << Value;
		}
		else
		{
			Ar << PakEntry.Size;
		}

		// Build the Compression Blocks array.
		if (PakEntry.CompressionBlocks.Num() > 1 || (PakEntry.CompressionBlocks.Num() == 1 && PakEntry.IsEncrypted()))
		{
			for (int CompressionBlockIndex = 0; CompressionBlockIndex < PakEntry.CompressionBlocks.Num(); ++CompressionBlockIndex)
			{
				uint32 Value = IntCastChecked<uint32>(PakEntry.CompressionBlocks[CompressionBlockIndex].CompressedEnd - PakEntry.CompressionBlocks[CompressionBlockIndex].CompressedStart);
				Ar << Value;
			}
		}
	}

	return true;
}

void FPakFile::DecodePakEntry(const uint8* SourcePtr, FPakEntry& OutEntry, const FPakInfo& InInfo)
{
	// Grab the big bitfield value:
	// Bit 31 = Offset 32-bit safe?
	// Bit 30 = Uncompressed size 32-bit safe?
	// Bit 29 = Size 32-bit safe?
	// Bits 28-23 = Compression method
	// Bit 22 = Encrypted
	// Bits 21-6 = Compression blocks count
	// Bits 5-0 = Compression block size
	uint32 Value = *(uint32*)SourcePtr;
	SourcePtr += sizeof(uint32);
	
	uint32 CompressionBlockSize = 0;
	if ( (Value & 0x3f) == 0x3f ) // flag value to load a field
	{
		CompressionBlockSize = *(uint32*)SourcePtr;
		SourcePtr += sizeof(uint32);
	}
	else
	{
		// for backwards compatibility with old paks :
		CompressionBlockSize = ((Value & 0x3f) << 11);
	}

	// Filter out the CompressionMethod.
	OutEntry.CompressionMethodIndex = (Value >> 23) & 0x3f;

	// Test for 32-bit safe values. Grab it, or memcpy the 64-bit value
	// to avoid alignment exceptions on platforms requiring 64-bit alignment
	// for 64-bit variables.
	//
	// Read the Offset.
	bool bIsOffset32BitSafe = (Value & (1 << 31)) != 0;
	if (bIsOffset32BitSafe)
	{
		OutEntry.Offset = *(uint32*)SourcePtr;
		SourcePtr += sizeof(uint32);
	}
	else
	{
		FMemory::Memcpy(&OutEntry.Offset, SourcePtr, sizeof(int64));
		SourcePtr += sizeof(int64);
	}

	// Read the UncompressedSize.
	bool bIsUncompressedSize32BitSafe = (Value & (1 << 30)) != 0;
	if (bIsUncompressedSize32BitSafe)
	{
		OutEntry.UncompressedSize = *(uint32*)SourcePtr;
		SourcePtr += sizeof(uint32);
	}
	else
	{
		FMemory::Memcpy(&OutEntry.UncompressedSize, SourcePtr, sizeof(int64));
		SourcePtr += sizeof(int64);
	}

	// Fill in the Size.
	if (OutEntry.CompressionMethodIndex != 0)
	{
		// Size is only present if compression is applied.
		bool bIsSize32BitSafe = (Value & (1 << 29)) != 0;
		if (bIsSize32BitSafe)
		{
			OutEntry.Size = *(uint32*)SourcePtr;
			SourcePtr += sizeof(uint32);
		}
		else
		{
			FMemory::Memcpy(&OutEntry.Size, SourcePtr, sizeof(int64));
			SourcePtr += sizeof(int64);
		}
	}
	else
	{
		// The Size is the same thing as the UncompressedSize when
		// CompressionMethod == COMPRESS_None.
		OutEntry.Size = OutEntry.UncompressedSize;
	}

	// Filter the encrypted flag.
	OutEntry.SetEncrypted((Value & (1 << 22)) != 0);

	// This should clear out any excess CompressionBlocks that may be valid in the user's
	// passed in entry.
	uint32 CompressionBlocksCount = (Value >> 6) & 0xffff;
	OutEntry.CompressionBlocks.Empty(CompressionBlocksCount);
	OutEntry.CompressionBlocks.SetNum(CompressionBlocksCount);
	
	OutEntry.CompressionBlockSize = 0;
	if (CompressionBlocksCount > 0)
	{
		OutEntry.CompressionBlockSize = CompressionBlockSize;
		// Per the comment in Encode, if CompressionBlocksCount == 1, we use UncompressedSize for CompressionBlockSize
		if (CompressionBlocksCount == 1)
		{
			OutEntry.CompressionBlockSize = IntCastChecked<uint32>(OutEntry.UncompressedSize);
		}
		ensure(OutEntry.CompressionBlockSize != 0);
	}

	// Set bDeleteRecord to false, because it obviously isn't deleted if we are here.
	OutEntry.SetDeleteRecord(false);

	// Base offset to the compressed data
	int64 BaseOffset = InInfo.HasRelativeCompressedChunkOffsets() ? 0 : OutEntry.Offset;

	// Handle building of the CompressionBlocks array.
	if (OutEntry.CompressionBlocks.Num() == 1 && !OutEntry.IsEncrypted())
	{
		// If the number of CompressionBlocks is 1, we didn't store any extra information.
		// Derive what we can from the entry's file offset and size.
		FPakCompressedBlock& CompressedBlock = OutEntry.CompressionBlocks[0];
		CompressedBlock.CompressedStart = BaseOffset + OutEntry.GetSerializedSize(InInfo.Version);
		CompressedBlock.CompressedEnd = CompressedBlock.CompressedStart + OutEntry.Size;
	}
	else if (OutEntry.CompressionBlocks.Num() > 0)
	{
		// Get the right pointer to start copying the CompressionBlocks information from.
		uint32* CompressionBlockSizePtr = (uint32*)SourcePtr;

		// Alignment of the compressed blocks
		uint64 CompressedBlockAlignment = OutEntry.IsEncrypted() ? FAES::AESBlockSize : 1;

		// CompressedBlockOffset is the starting offset. Everything else can be derived from there.
		int64 CompressedBlockOffset = BaseOffset + OutEntry.GetSerializedSize(InInfo.Version);
		for (int CompressionBlockIndex = 0; CompressionBlockIndex < OutEntry.CompressionBlocks.Num(); ++CompressionBlockIndex)
		{
			FPakCompressedBlock& CompressedBlock = OutEntry.CompressionBlocks[CompressionBlockIndex];
			CompressedBlock.CompressedStart = CompressedBlockOffset;
			CompressedBlock.CompressedEnd = CompressedBlockOffset + *CompressionBlockSizePtr++;
			CompressedBlockOffset += Align(CompressedBlock.CompressedEnd - CompressedBlock.CompressedStart, CompressedBlockAlignment);
		}
	}
}

bool FPakFile::NormalizeDirectoryQuery(const TCHAR* InPath, FString& OutRelativePathFromMount) const
{
	OutRelativePathFromMount = InPath;
	MakeDirectoryFromPath(OutRelativePathFromMount);
	return GetRelativePathFromMountInline(OutRelativePathFromMount, MountPoint);
}

const FPakDirectory* FPakFile::FindPrunedDirectoryInternal(const FString& RelativePathFromMount) const
{
	// Caller holds FScopedPakDirectoryIndexAccess
	const FPakDirectory* PakDirectory = nullptr;

#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
	if (ShouldValidatePrunedDirectory())
	{
		PakDirectory = DirectoryIndex.Find(RelativePathFromMount);
		const FPakDirectory* PrunedPakDirectory = PrunedDirectoryIndex.Find(RelativePathFromMount);
		if ((PakDirectory != nullptr) != (PrunedPakDirectory != nullptr))
		{
			TSet<FString> FullFoundFiles, PrunedFoundFiles;
			FString ReportedDirectoryName = MountPoint + RelativePathFromMount;
			if (PakDirectory)
			{
				FullFoundFiles.Add(ReportedDirectoryName);
			}
			if (PrunedPakDirectory)
			{
				PrunedFoundFiles.Add(ReportedDirectoryName);
			}
			ValidateDirectorySearch(FullFoundFiles, PrunedFoundFiles, *ReportedDirectoryName);
		}
	}
	else
#endif
	{
		PakDirectory = DirectoryIndex.Find(RelativePathFromMount);
	}
	return PakDirectory;
}

bool FPakFile::Check()
{
	UE_LOG(LogPakFile, Display, TEXT("Checking pak file \"%s\". This may take a while..."), *PakFilename);
	double StartTime = FPlatformTime::Seconds();

	FSharedPakReader PakReader = GetSharedReader(nullptr);
	int32 ErrorCount = 0;
	int32 FileCount = 0;

	// If the pak file is signed, we can do a fast check by just reading a single byte from the start of
	// each signing block. The signed archive reader will bring in that whole signing block and compare
	// against the signature table and fire the handler
	if (bSigned)
	{
		FDelegateHandle DelegateHandle;
		FPakPlatformFile::FPakSigningFailureHandlerData& HandlerData = FPakPlatformFile::GetPakSigningFailureHandlerData();

		{
			FScopeLock Lock(&HandlerData.GetLock());
			DelegateHandle = HandlerData.GetPakChunkSignatureCheckFailedDelegate().AddLambda([&ErrorCount](const FPakChunkSignatureCheckFailedData&)
			{
				++ErrorCount;
			});
		}

		int64 CurrentPos = 0;
		const int64 Size = PakReader->TotalSize();
		while (CurrentPos < Size)
		{
			PakReader->Seek(CurrentPos);
			uint8 Byte = 0;
			PakReader.GetArchive() << Byte;
			CurrentPos += FPakInfo::MaxChunkDataSize;
		}

		if (DelegateHandle.IsValid())
		{
			FScopeLock Lock(&HandlerData.GetLock());
			HandlerData.GetPakChunkSignatureCheckFailedDelegate().Remove(DelegateHandle);
		}
	}
	else
	{
		const bool bIncludeDeleted = true;
		TCHAR EntryNameBuffer[256];
		auto GetEntryName = [&EntryNameBuffer](const FPakFile::FPakEntryIterator& It)
		{
			const FString* EntryFilename = It.TryGetFilename();
			if (EntryFilename)
			{
				TCString<TCHAR>::Snprintf(EntryNameBuffer, sizeof(EntryNameBuffer), TEXT("\"%s\""), **EntryFilename);
			}
			else
			{
				TCString<TCHAR>::Snprintf(EntryNameBuffer, sizeof(EntryNameBuffer), TEXT("file at offset %u"), It.Info().Offset);
			}
			return EntryNameBuffer;
		};
		for (FPakFile::FPakEntryIterator It(*this, bIncludeDeleted); It; ++It, ++FileCount)
		{
			const FPakEntry& EntryFromIndex = It.Info();
			if (EntryFromIndex.IsDeleteRecord())
			{
				UE_LOG(LogPakFile, Verbose, TEXT("%s Deleted."), GetEntryName(It));
				continue;
			}

			void* FileContents = FMemory::Malloc(EntryFromIndex.Size);
			PakReader->Seek(EntryFromIndex.Offset);
			uint32 SerializedCrcTest = 0;
			FPakEntry EntryFromPayload;
			EntryFromPayload.Serialize(PakReader.GetArchive(), GetInfo().Version);
			if (!EntryFromPayload.IndexDataEquals(EntryFromIndex))
			{
				UE_LOG(LogPakFile, Error, TEXT("Index FPakEntry does not match Payload FPakEntry for %s."), GetEntryName(It));
				ErrorCount++;
			}
			PakReader->Serialize(FileContents, EntryFromIndex.Size);

			uint8 TestHash[20];
			FSHA1::HashBuffer(FileContents, EntryFromIndex.Size, TestHash);
			if (FMemory::Memcmp(TestHash, EntryFromPayload.Hash, sizeof(TestHash)) != 0)
			{
				UE_LOG(LogPakFile, Error, TEXT("Hash mismatch for %s."), GetEntryName(It));
				ErrorCount++;
			}
			else
			{
				UE_LOG(LogPakFile, Verbose, TEXT("%s OK. [%s]"), GetEntryName(It), *Info.GetCompressionMethod(EntryFromIndex.CompressionMethodIndex).ToString());
			}
			FMemory::Free(FileContents);
		}
		if (ErrorCount == 0)
		{
			UE_LOG(LogPakFile, Display, TEXT("Pak file \"%s\" healthy, %d files checked."), *PakFilename, FileCount);
		}
		else
		{
			UE_LOG(LogPakFile, Display, TEXT("Pak file \"%s\" corrupted (%d errors out of %d files checked.)."), *PakFilename, ErrorCount, FileCount);
		}
	}

	double EndTime = FPlatformTime::Seconds();
	double ElapsedTime = EndTime - StartTime;
	UE_LOG(LogPakFile, Display, TEXT("Pak file \"%s\" checked in %.2fs"), *PakFilename, ElapsedTime);

	return ErrorCount == 0;
}

bool CheckIoStoreContainerBlockSignatures(const TCHAR* InContainerPath)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CheckIoStoreContainerBlockSignatures);
	UE_LOG(LogPakFile, Display, TEXT("Checking container file \"%s\"..."), InContainerPath);
	double StartTime = FPlatformTime::Seconds();

	FIoStoreTocResource TocResource;
	FIoStatus Status = FIoStoreTocResource::Read(InContainerPath, EIoStoreTocReadOptions::Default, TocResource);
	if (!Status.IsOk())
	{
		UE_LOG(LogPakFile, Error, TEXT("Failed reading toc file \"%s\"."), InContainerPath);
		return false;
	}

	if (TocResource.ChunkBlockSignatures.Num() != TocResource.CompressionBlocks.Num())
	{
		UE_LOG(LogPakFile, Error, TEXT("Toc file \"%s\" doesn't contain any chunk block signatures."), InContainerPath);
		return false;
	}

	TUniquePtr<FArchive> ContainerFileReader;
	int32 LastPartitionIndex = -1;
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	TArray<uint8> BlockBuffer;
	BlockBuffer.SetNum(static_cast<int32>(TocResource.Header.CompressionBlockSize));
	const int32 BlockCount = TocResource.CompressionBlocks.Num();
	int32 ErrorCount = 0;
	FString ContainerBasePath = FPaths::ChangeExtension(InContainerPath, TEXT(""));
	TStringBuilder<256> UcasFilePath;
	for (int32 BlockIndex = 0; BlockIndex < BlockCount; ++BlockIndex)
	{
		const FIoStoreTocCompressedBlockEntry& CompressionBlockEntry = TocResource.CompressionBlocks[BlockIndex];
		uint64 BlockRawSize = Align(CompressionBlockEntry.GetCompressedSize(), FAES::AESBlockSize);
		check(BlockRawSize <= TocResource.Header.CompressionBlockSize);
		const int32 PartitionIndex = int32(CompressionBlockEntry.GetOffset() / TocResource.Header.PartitionSize);
		const uint64 PartitionRawOffset = CompressionBlockEntry.GetOffset() % TocResource.Header.PartitionSize;
		if (PartitionIndex != LastPartitionIndex)
		{
			UcasFilePath.Reset();
			UcasFilePath.Append(ContainerBasePath);
			if (PartitionIndex > 0)
			{
				UcasFilePath.Append(FString::Printf(TEXT("_s%d"), PartitionIndex));
			}
			UcasFilePath.Append(TEXT(".ucas"));
			IFileHandle* ContainerFileHandle = Ipf.OpenRead(*UcasFilePath, /* allowwrite */ false);
			if (!ContainerFileHandle)
			{
				UE_LOG(LogPakFile, Error, TEXT("Failed opening container file \"%s\"."), *UcasFilePath);
				return false;
			}
			ContainerFileReader.Reset(new FArchiveFileReaderGeneric(ContainerFileHandle, *UcasFilePath, ContainerFileHandle->Size(), 256 << 10));
			LastPartitionIndex = PartitionIndex;
		}
		ContainerFileReader->Seek(PartitionRawOffset);
		ContainerFileReader->Precache(PartitionRawOffset, 0); // Without this buffering won't work due to the first read after a seek always being uncached
		ContainerFileReader->Serialize(BlockBuffer.GetData(), BlockRawSize);
		FSHAHash BlockHash;
		FSHA1::HashBuffer(BlockBuffer.GetData(), BlockRawSize, BlockHash.Hash);
		if (TocResource.ChunkBlockSignatures[BlockIndex] != BlockHash)
		{
			UE_LOG(LogPakFile, Warning, TEXT("Hash mismatch for block [%i/%i]! Expected %s, Received %s"), BlockIndex, BlockCount, *TocResource.ChunkBlockSignatures[BlockIndex].ToString(), *BlockHash.ToString());

			FPakChunkSignatureCheckFailedData Data(*UcasFilePath, TPakChunkHash(), TPakChunkHash(), BlockIndex);
#if PAKHASH_USE_CRC
			Data.ExpectedHash = GetTypeHash(TocResource.ChunkBlockSignatures[BlockIndex]);
			Data.ReceivedHash = GetTypeHash(BlockHash);
#else
			Data.ExpectedHash = TocResource.ChunkBlockSignatures[BlockIndex];
			Data.ReceivedHash = BlockHash;
#endif
			FPakPlatformFile::BroadcastPakChunkSignatureCheckFailure(Data);
			++ErrorCount;
		}
	}

	double EndTime = FPlatformTime::Seconds();
	double ElapsedTime = EndTime - StartTime;
	UE_LOG(LogPakFile, Display, TEXT("Container file \"%s\" checked in %.2fs"), InContainerPath, ElapsedTime);

	return ErrorCount == 0;
}

#if DO_CHECK
/**
* FThreadCheckingArchiveProxy - checks that inner archive is only used from the specified thread ID
*/
class FThreadCheckingArchiveProxy : TUniquePtr<FArchive>, public FArchiveProxy
{
public:

	const uint32 ThreadId;

	FThreadCheckingArchiveProxy(FArchive* InReader, uint32 InThreadId)
		: TUniquePtr(InReader) // Make sure proxy is destroyed before InReader
		, FArchiveProxy(*InReader)
		, ThreadId(InThreadId)
	{}

	//~ Begin FArchiveProxy Interface
	virtual void Serialize(void* Data, int64 Length) override
	{
		if (FPlatformTLS::GetCurrentThreadId() != ThreadId)
		{
			UE_LOG(LogPakFile, Error, TEXT("Attempted serialize using thread-specific pak file reader on the wrong thread.  Reader for thread %d used by thread %d."), ThreadId, FPlatformTLS::GetCurrentThreadId());
		}
		InnerArchive.Serialize(Data, Length);
	}

	virtual void Seek(int64 InPos) override
	{
		if (FPlatformTLS::GetCurrentThreadId() != ThreadId)
		{
			UE_LOG(LogPakFile, Error, TEXT("Attempted seek using thread-specific pak file reader on the wrong thread.  Reader for thread %d used by thread %d."), ThreadId, FPlatformTLS::GetCurrentThreadId());
		}
		InnerArchive.Seek(InPos);
	}
	//~ End FArchiveProxy Interface
};
#endif //DO_CHECK

void FPakFile::GetPrunedFilenames(TArray<FString>& OutFileList) const
{
	for (FFilenameIterator It(*this, true /* bIncludeDeleted */); It; ++It)
	{
		OutFileList.Add(PakPathCombine(MountPoint, It.Filename()));
	}
}

void FPakFile::GetPrunedFilenamesInChunk(const TArray<int32>& InChunkIDs, TArray<FString>& OutFileList) const
{
	for (FFilenameIterator It(*this, true /* bIncludeDeleted */); It; ++It)
	{
		const FPakEntry& File = It.Info();
		int64 FileStart = File.Offset;
		int64 FileEnd = File.Offset + File.Size;

		for (int64 LocalChunkID : InChunkIDs)
		{
			int64 ChunkStart = LocalChunkID * FPakInfo::MaxChunkDataSize;
			int64 ChunkEnd = ChunkStart + FPakInfo::MaxChunkDataSize;

			if (FileStart < ChunkEnd && FileEnd > ChunkStart)
			{
				OutFileList.Add(It.Filename());
				break;
			}
		}
	}
}


/**
 * Search the given FDirectoryIndex for all files under the given Directory.  Helper for FindFilesAtPath, called separately on the DirectoryIndex or Pruned DirectoryIndex. Does not use
 * FScopedPakDirectoryIndexAccess internally; caller is responsible for calling from within a lock.
 * Returned paths are full paths (include the mount point)
 */
template <typename ShouldVisitFunc, class ContainerType>
void FPakFile::FindFilesAtPathInIndex(const FDirectoryIndex& TargetIndex, ContainerType& OutFiles,
	const FString& Directory, const ShouldVisitFunc& ShouldVisit, bool bIncludeFiles, bool bIncludeDirectories,
	bool bRecursive) const
{
	// Early out if MountPoint is not matching directory
	if (!Directory.StartsWith(MountPoint))
	{
		return;
	}

	FStringView RelativeSearch(FStringView(Directory).RightChop(MountPoint.Len()));

	TArray<FString> DirectoriesInPak; // List of all unique directories at path
	for (TMap<FString, FPakDirectory>::TConstIterator It(TargetIndex); It; ++It)
	{
		// Check if the file is under the specified path.
		if (FStringView(It.Key()).StartsWith(RelativeSearch))
		{
			FString PakPath = PakPathCombine(MountPoint, It.Key());
			if (bRecursive == true)
			{
				// Add everything
				if (bIncludeFiles)
				{
					for (FPakDirectory::TConstIterator DirectoryIt(It.Value()); DirectoryIt; ++DirectoryIt)
					{
						const FString& FilePathUnderDirectory = DirectoryIt.Key();
						if (ShouldVisit(FilePathUnderDirectory))
						{
							OutFiles.Add(PakPathCombine(PakPath, FilePathUnderDirectory));
						}
					}
				}
				if (bIncludeDirectories)
				{
					if (Directory != PakPath)
					{
						if (ShouldVisit(PakPath))
						{
							DirectoriesInPak.Add(MoveTemp(PakPath));
						}
					}
				}
			}
			else
			{
				int32 SubDirIndex = PakPath.Len() > Directory.Len() ? PakPath.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Directory.Len() + 1) : INDEX_NONE;
				// Add files in the specified folder only.
				if (bIncludeFiles && SubDirIndex == INDEX_NONE)
				{
					for (FPakDirectory::TConstIterator DirectoryIt(It.Value()); DirectoryIt; ++DirectoryIt)
					{
						const FString& FilePathUnderDirectory = DirectoryIt.Key();
						if (ShouldVisit(FilePathUnderDirectory))
						{
							OutFiles.Add(PakPathCombine(PakPath, FilePathUnderDirectory));
						}
					}
				}
				// Add sub-folders in the specified folder only
				if (bIncludeDirectories && SubDirIndex >= 0)
				{
					FString SubDirPath = PakPath.Left(SubDirIndex + 1);
					if (ShouldVisit(SubDirPath))
					{
						DirectoriesInPak.AddUnique(MoveTemp(SubDirPath));
					}
				}
			}
		}
	}
	OutFiles.Append(MoveTemp(DirectoriesInPak));
}

template <typename ShouldVisitFunc, class ContainerType>
void FPakFile::FindPrunedFilesAtPathInternal(const TCHAR* InPath, const ShouldVisitFunc& ShouldVisit, ContainerType& OutFiles,
	bool bIncludeFiles, bool bIncludeDirectories, bool bRecursive) const
{
	// Make sure all directory names end with '/'.
	FString Directory(InPath);
	MakeDirectoryFromPath(Directory);

	// Check the specified path is under the mount point of this pak file.
	// The reverse case (MountPoint StartsWith Directory) is needed to properly handle
	// pak files that are a subdirectory of the actual directory.
	if (!Directory.StartsWith(MountPoint) && !MountPoint.StartsWith(Directory))
	{
		return;
	}

	FScopedPakDirectoryIndexAccess ScopeAccess(*this);
#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
	if (ShouldValidatePrunedDirectory())
	{
		TSet<FString> FullFoundFiles, PrunedFoundFiles;
		FindFilesAtPathInIndex(DirectoryIndex, FullFoundFiles, Directory, ShouldVisit,
			bIncludeFiles, bIncludeDirectories, bRecursive);
		FindFilesAtPathInIndex(PrunedDirectoryIndex, PrunedFoundFiles, Directory, ShouldVisit,
			bIncludeFiles, bIncludeDirectories, bRecursive);
		ValidateDirectorySearch(FullFoundFiles, PrunedFoundFiles, InPath);

		for (const FString& FoundFile : FullFoundFiles)
		{
			OutFiles.Add(FoundFile);
		}
	}
	else
#endif
	{
		FindFilesAtPathInIndex(DirectoryIndex, OutFiles, Directory, ShouldVisit,
			bIncludeFiles, bIncludeDirectories, bRecursive);
	}
}

void FPakFile::FindPrunedFilesAtPath(const TCHAR* InPath, TArray<FString>& OutFiles,
	bool bIncludeFiles, bool bIncludeDirectories, bool bRecursive) const
{
	auto ShouldVisit = [](FStringView Path) { return true; };
	FindPrunedFilesAtPathInternal(InPath, ShouldVisit, OutFiles, bIncludeFiles, bIncludeDirectories, bRecursive);
}


#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
void FPakFile::ValidateDirectorySearch(const TSet<FString>& FullFoundFiles, const TSet<FString>& PrunedFoundFiles, const TCHAR* InPath) const
{
	TArray<FString> MissingFromPruned;
	for (const FString& FileInFull : FullFoundFiles)
	{
		if (!PrunedFoundFiles.Contains(FileInFull))
		{
			MissingFromPruned.Add(FileInFull);
		}
	}
	TArray<FString> MissingFromFull;
	for (const FString& FileInPruned : PrunedFoundFiles)
	{
		if (!FullFoundFiles.Contains(FileInPruned))
		{
			MissingFromFull.Add(FileInPruned);
		}
	}

	if (MissingFromPruned.Num() == 0 && MissingFromFull.Num() == 0)
	{
		return;
	}

	TArray<FString> WildCards, OldWildCards;
	GConfig->GetArray(TEXT("Pak"), TEXT("IndexValidationIgnore"), WildCards, GEngineIni);
	auto IsIgnore = [&WildCards](const FString& FilePath)
	{
		for (const FString& WildCard : WildCards)
		{
			if (FilePath.MatchesWildcard(WildCard))
			{
				return true;
			}
		}
		return false;
	};
	auto StripIgnores = [&IsIgnore](TArray<FString>& FilePaths)
	{
		for (int Idx = FilePaths.Num() - 1; Idx >= 0; --Idx)
		{
			if (IsIgnore(FilePaths[Idx]))
			{
				FilePaths.RemoveAtSwap(Idx);
			}
		}
	};

	StripIgnores(MissingFromPruned);
	StripIgnores(MissingFromFull);

	if (MissingFromPruned.Num() == 0 && MissingFromFull.Num() == 0)
	{
		return;
	}
	MissingFromPruned.Sort();
	MissingFromFull.Sort();

	// TODO: Restore this as an Error once we modify IPlatformFile::IterateDirectoryRecursively to declare its filefilter so we can ignore the spurious
	// discovered files that are not part of the fully filtered query
	UE_LOG(LogPakFile, Error, TEXT("FindPrunedFilesAtPath('%s') for PakFile '%s' found a different list in the FullDirectory than in the PrunedDirectory. ")
		TEXT("Change the calling code or add the files to Engine:[Pak]:WildcardsToKeepInPakStringIndex or Engine:[Pak]:IndexValidationIgnore."),
		InPath, *PakFilename);

#if !NO_LOGGING && !UE_BUILD_SHIPPING
	// Logging callstacks is expensive (multiple seconds long). Only do it the first time a path is seen, and only for the first
	// few paths.
	static TSet<FString> AlreadyLoggedCallstack;
	static FCriticalSection AlreadyLoggedCallstackLock;
	constexpr int32 CallstackLogDirsMax = 10;
	bool bShouldLogCallstack = false;
	if (AlreadyLoggedCallstack.Num() < CallstackLogDirsMax) // check to avoid taking critical section if unnecessary
	{
		FScopeLock AlreadyLoggedCallstackScopeLock(&AlreadyLoggedCallstackLock);
		if (AlreadyLoggedCallstack.Num() < CallstackLogDirsMax) // check again since other thread may have modified it
		{
			bool bAlreadyLogged;
			AlreadyLoggedCallstack.Add(FString(InPath), &bAlreadyLogged);
			bShouldLogCallstack = !bAlreadyLogged;
		}
	}
	if (bShouldLogCallstack)
	{
		UE_LOG(LogPakFile, Warning, TEXT("Callstack of FindPrunedFilesAtPath('%s'):"), InPath);
		FDebug::DumpStackTraceToLog(ELogVerbosity::Warning);
	}
#endif

	if (MissingFromPruned.Num() > 0)
	{
		for (const FString& Missing : MissingFromPruned)
		{
			UE_LOG(LogPakFile, Warning, TEXT("MissingPrunedPakFile: %s"), *Missing);
		}
	}
	if (MissingFromFull.Num() > 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Some files in the PrunedDirectory are missing from the FullDirectory.  This is a logic error in FPakFile since the PrunedDirectory should be a subset of the FullDirectory."));
		for (const FString& Missing : MissingFromFull)
		{
			UE_LOG(LogPakFile, Warning, TEXT("MissingFullPakFile: %s"), *Missing);
		}
	}
}
#endif

bool FPakFile::RecreatePakReaders(IPlatformFile* LowerLevel)
{
	FScopeLock ScopedLock(&ReadersCriticalSection);

	if (CurrentlyUsedReaders > 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("Recreating pak readers while we have readers loaned out, this may be lead to crashes or decryption problems."));
	}

	// need to reset the decryptor as it will hold a pointer to the first created pak reader
	Decryptor.Reset();

	TArray<FArchiveAndLastAccessTime> TempReaders;

	// Create a new PakReader *per* instance that was already mapped
	for (const FArchiveAndLastAccessTime& Reader : Readers)
	{
		TUniquePtr<FArchive> PakReader = TUniquePtr<FArchive>(CreatePakReader(LowerLevel, *GetFilename()));
		if (!PakReader)
		{
			UE_LOG(LogPakFile, Warning, TEXT("Unable to re-create pak \"%s\" handle"), *GetFilename());
			return false;
		}
		TempReaders.Add(FArchiveAndLastAccessTime{ MoveTemp(PakReader), Reader.LastAccessTime });
	}

	// replace the current Readers with the newly created pak readers leaving them to out of scope
	Readers= MoveTemp(TempReaders);

	return true;
}

LLM_DEFINE_TAG(PakSharedReaders);

FSharedPakReader FPakFile::GetSharedReader(IPlatformFile* LowerLevel)
{
	LLM_SCOPE_BYTAG(PakSharedReaders);
	FArchive* PakReader = nullptr;
	{
		FScopeLock ScopedLock(&ReadersCriticalSection);
		if (Readers.Num())
		{
			FArchiveAndLastAccessTime Reader = Readers.Pop();
			PakReader = Reader.Archive.Release();
		}
		else
		{
			// Create a new FArchive reader and pass it to the new handle.
			PakReader = CreatePakReader(LowerLevel, *GetFilename());

			if (!PakReader)
			{
				UE_LOG(LogPakFile, Warning, TEXT("Unable to create pak \"%s\" handle"), *GetFilename());
			}
		}
		++CurrentlyUsedReaders;
	}

	return FSharedPakReader(PakReader, this);
}

void FPakFile::ReturnSharedReader(FArchive* Archive)
{
	FScopeLock ScopedLock(&ReadersCriticalSection);
	--CurrentlyUsedReaders;
	Readers.Push(FArchiveAndLastAccessTime{ TUniquePtr<FArchive>{Archive }, FPlatformTime::Seconds()});
}

void FPakFile::ReleaseOldReaders(double MaxAgeSeconds)
{
	if (ReadersCriticalSection.TryLock())
	{
		ON_SCOPE_EXIT
		{
			ReadersCriticalSection.Unlock();
		};
		double SearchTime = FPlatformTime::Seconds() - MaxAgeSeconds;
		for (int32 i = Readers.Num() - 1; i >= 0; --i)
		{
			const FArchiveAndLastAccessTime& Reader = Readers[i];
			if (Reader.LastAccessTime <= SearchTime)
			{
				// Remove this and all readers older than it (pushed before it)
				Readers.RemoveAt(0, i + 1);
				break;
			}
		}

		if (Readers.Num() == 0 && CurrentlyUsedReaders == 0)
		{
			Decryptor.Reset();
		}
	}
}

const FPakEntryLocation* FPakFile::FindLocationFromIndex(const FString& FullPath, const FString& MountPoint, const FPathHashIndex& PathHashIndex, uint64 PathHashSeed, int32 PakFileVersion)
{
	const TCHAR* RelativePathFromMount = GetRelativeFilePathFromMountPointer(FullPath, MountPoint);
	if (!RelativePathFromMount)
	{
		return nullptr;
	}
	uint64 PathHash = HashPath(RelativePathFromMount, PathHashSeed, PakFileVersion);
	return PathHashIndex.Find(PathHash);
}

const FPakEntryLocation* FPakFile::FindLocationFromIndex(const FString& FullPath, const FString& MountPoint, const FDirectoryIndex& DirectoryIndex)
{
	if (!FullPath.StartsWith(MountPoint))
	{
		return nullptr;
	}
	const TCHAR* RelativePathFromMount = (*FullPath) + MountPoint.Len();
	FString RelativeDirName(RelativePathFromMount);
	FString CleanFileName;
	if (RelativeDirName.IsEmpty())
	{
		return nullptr;
	}
	SplitPathInline(RelativeDirName, CleanFileName);
	const FPakDirectory* PakDirectory = DirectoryIndex.Find(RelativeDirName);
	if (PakDirectory)
	{
		return PakDirectory->Find(CleanFileName);
	}
	return nullptr;
}

FPakFile::EFindResult FPakFile::Find(const FString& FullPath, FPakEntry* OutEntry) const
{
	//QUICK_SCOPE_CYCLE_COUNTER(PakFileFind);

	const FPakEntryLocation* PakEntryLocation;
#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
	if (IsPakValidatePruning() && bHasPathHashIndex && bHasFullDirectoryIndex)
	{
		const FPakEntryLocation* PathHashLocation = nullptr;
		PathHashLocation = FindLocationFromIndex(FullPath, MountPoint, PathHashIndex, PathHashSeed, Info.Version);

		const FPakEntryLocation* DirectoryLocation = nullptr;

		{
			FScopedPakDirectoryIndexAccess ScopeAccess(*this);
			DirectoryLocation = FindLocationFromIndex(FullPath, MountPoint, DirectoryIndex);
		}

		if ((PathHashLocation != nullptr) != (DirectoryLocation != nullptr))
		{
			const TCHAR* FoundName = TEXT("PathHashIndex");
			const TCHAR* NotFoundName = TEXT("FullDirectoryIndex");
			if (!PathHashLocation)
			{
				Swap(FoundName, NotFoundName);
			}
			UE_LOG(LogPakFile, Error, TEXT("PathHashIndex does not match FullDirectoryIndex. Pakfile '%s' has '%s' in its %s but not in its %s."),
				*PakFilename, *FullPath, FoundName, NotFoundName);
		}
		PakEntryLocation = PathHashLocation ? PathHashLocation : DirectoryLocation;
	}
	else
#endif
	{
		if (bHasPathHashIndex)
		{
			PakEntryLocation = FindLocationFromIndex(FullPath, MountPoint, PathHashIndex, PathHashSeed, Info.Version);
		}
		else
		{
			check(bHasFullDirectoryIndex);
			FScopedPakDirectoryIndexAccess ScopeAccess(*this);
			PakEntryLocation = FindLocationFromIndex(FullPath, MountPoint, DirectoryIndex);
		}
	}
	if (!PakEntryLocation)
	{
		return EFindResult::NotFound;
	}

	return GetPakEntry(*PakEntryLocation, OutEntry);
}

#if !UE_BUILD_SHIPPING
class FPakExec : private FSelfRegisteringExec
{
	FPakPlatformFile& PlatformFile;

public:

	FPakExec(FPakPlatformFile& InPlatformFile)
		: PlatformFile(InPlatformFile)
	{}

protected:
	/** Console commands **/
	virtual bool Exec_Dev(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override
	{
		if (FParse::Command(&Cmd, TEXT("Mount")))
		{
			PlatformFile.HandleMountCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("Unmount")))
		{
			PlatformFile.HandleUnmountCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PakList")))
		{
			PlatformFile.HandlePakListCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("PakCorrupt")))
		{
			PlatformFile.HandlePakCorruptCommand(Cmd, Ar);
			return true;
		}
		else if (FParse::Command(&Cmd, TEXT("ReloadPakReaders")))
		{
			PlatformFile.HandleReloadPakReadersCommand(Cmd, Ar);
			return true;
		}
		return false;
	}
};
static TUniquePtr<FPakExec> GPakExec;

void FPakPlatformFile::HandleMountCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString PakFilename = FParse::Token(Cmd, false);
	if (!PakFilename.IsEmpty())
	{
		const FString MountPoint = FParse::Token(Cmd, false);
		Mount(*PakFilename, 0, MountPoint.IsEmpty() ? NULL : *MountPoint);
	}
}

void FPakPlatformFile::HandleUnmountCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	const FString PakFilename = FParse::Token(Cmd, false);
	if (!PakFilename.IsEmpty())
	{
		Unmount(*PakFilename);
	}
}

void FPakPlatformFile::HandlePakListCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);
	for (auto Pak : Paks)
	{
		Ar.Logf(TEXT("%s Mounted to %s"), *Pak.PakFile->GetFilename(), *Pak.PakFile->GetMountPoint());
	}
}

void FPakPlatformFile::HandlePakCorruptCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
#if USE_PAK_PRECACHE
	FPakPrecacher::Get().SimulatePakFileCorruption();
#endif
}

void FPakPlatformFile::HandleReloadPakReadersCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);
	for (FPakListEntry& Pak : Paks)
	{
		Pak.PakFile->RecreatePakReaders(LowerLevel);
	}
}
#endif // !UE_BUILD_SHIPPING

FPakPlatformFile::FPakPlatformFile()
	: LowerLevel(NULL)
	, bSigned(false)
{
	UE::FEncryptionKeyManager::Get().OnKeyAdded().AddRaw(this, &FPakPlatformFile::RegisterEncryptionKey);
}

FPakPlatformFile::~FPakPlatformFile()
{
	UE_LOG(LogPakFile, Log, TEXT("Destroying PakPlatformFile"));

	FTSTicker::GetCoreTicker().RemoveTicker(RetireReadersHandle);

	UE::FEncryptionKeyManager::Get().OnKeyAdded().RemoveAll(this);
	FCoreDelegates::OnFEngineLoopInitComplete.RemoveAll(this);

	FCoreDelegates::OnMountAllPakFiles.Unbind();
	FCoreDelegates::MountPak.Unbind();
	FCoreDelegates::OnUnmountPak.Unbind();
	FCoreDelegates::OnOptimizeMemoryUsageForMountedPaks.Unbind();

#if USE_PAK_PRECACHE
	FPakPrecacher::Shutdown();
#endif
	{
		FScopeLock ScopedLock(&PakListCritical);
		for (int32 PakFileIndex = 0; PakFileIndex < PakFiles.Num(); PakFileIndex++)
		{
			PakFiles[PakFileIndex].PakFile.SafeRelease();
		}
	}
}

void FPakPlatformFile::FindPakFilesInDirectory(IPlatformFile* LowLevelFile, const TCHAR* Directory, const FString& WildCard, TArray<FString>& OutPakFiles)
{
	// Helper class to find all pak files.
	class FPakSearchVisitor : public IPlatformFile::FDirectoryVisitor
	{
		TArray<FString>& FoundPakFiles;
		FString WildCard;
		bool bSkipOptionalPakFiles;

	public:
		FPakSearchVisitor(TArray<FString>& InFoundPakFiles, const FString& InWildCard, bool bInSkipOptionalPakFiles)
			: FoundPakFiles(InFoundPakFiles)
			, WildCard(InWildCard)
			, bSkipOptionalPakFiles(bInSkipOptionalPakFiles)
		{}
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
		{
			if (bIsDirectory == false)
			{
				FString Filename(FilenameOrDirectory);
				if(Filename.MatchesWildcard(WildCard))
				{
					if (!FPakPlatformFile::IsPakFileInstalled(Filename))
					{
						return true;
					}

#if !UE_BUILD_SHIPPING
					if (bSkipOptionalPakFiles == false || Filename.Find("optional") == INDEX_NONE)
#endif
					{
						FoundPakFiles.Add(Filename);
					}
				}
			}
			return true;
		}
	};

	bool bSkipOptionalPakFiles = FParse::Param(FCommandLine::Get(), TEXT("SkipOptionalPakFiles"));

	// Find all pak files.
	FPakSearchVisitor Visitor(OutPakFiles, WildCard, bSkipOptionalPakFiles);
	LowLevelFile->IterateDirectoryRecursively(Directory, Visitor);
}

void FPakPlatformFile::FindAllPakFiles(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders, const FString& WildCard, TArray<FString>& OutPakFiles)
{
	// Find pak files from the specified directories.	
	for (int32 FolderIndex = 0; FolderIndex < PakFolders.Num(); ++FolderIndex)
	{
		FindPakFilesInDirectory(LowLevelFile, *PakFolders[FolderIndex], WildCard, OutPakFiles);
	}

	// alert anyone listening
	if (OutPakFiles.Num() == 0)
	{
		FCoreDelegates::NoPakFilesMountedDelegate.Broadcast();
	}
}

void FPakPlatformFile::GetPakFolders(const TCHAR* CmdLine, TArray<FString>& OutPakFolders)
{
#if !UE_BUILD_SHIPPING
	// Command line folders
	FString PakDirs;
	if (FParse::Value(CmdLine, TEXT("-pakdir="), PakDirs))
	{
		TArray<FString> CmdLineFolders;
		PakDirs.ParseIntoArray(CmdLineFolders, TEXT("*"), true);
		OutPakFolders.Append(CmdLineFolders);
	}
#endif

	// @todo plugin urgent: Needs to handle plugin Pak directories, too
	// Hardcoded locations
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::ProjectContentDir()));
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::ProjectSavedDir()));
	OutPakFolders.Add(FString::Printf(TEXT("%sPaks/"), *FPaths::EngineContentDir()));
}

bool FPakPlatformFile::CheckIfPakFilesExist(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders)
{
	TArray<FString> FoundPakFiles;
	FindAllPakFiles(LowLevelFile, PakFolders, TEXT(ALL_PAKS_WILDCARD), FoundPakFiles);
	return FoundPakFiles.Num() > 0;
}

bool FPakPlatformFile::ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const
{
#if WITH_EDITOR
	if (FParse::Param(CmdLine, TEXT("UsePaks")))
	{
		TArray<FString> PakFolders;
		GetPakFolders(CmdLine, PakFolders);
		if (!CheckIfPakFilesExist(Inner, PakFolders))
		{
			UE_LOG(LogPakFile, Warning, TEXT("No Pak files were found when checking to make Pak Environment"));
		}
		return true;
	}
#endif //if WITH_EDITOR

	bool Result = false;
#if (!WITH_EDITOR || IS_MONOLITHIC || UE_FORCE_USE_PAKS)
	if (!FParse::Param(CmdLine, TEXT("NoPak")))
	{
#if UE_FORCE_USE_PAKS
		// Target wants pak file to be used even if no pak files currently exist (they can be downloaded later at runtime)
		Result = true;
#else
		TArray<FString> PakFolders;
		GetPakFolders(CmdLine, PakFolders);
		Result = CheckIfPakFilesExist(Inner, PakFolders);
#endif // UE_FORCE_USE_PAKS
	}
#endif //if (!WITH_EDITOR || IS_MONOLITHIC || UE_FORCE_USE_PAKS)
	return Result;
}

static bool PakPlatformFile_IsForceUseIoStore(const TCHAR* CmdLine)
{
#if UE_FORCE_USE_IOSTORE
	return true;
#elif WITH_IOSTORE_IN_EDITOR
	return FParse::Param(CmdLine, TEXT("UseIoStore"));
#else
	return false;
#endif
}

bool FPakPlatformFile::Initialize(IPlatformFile* Inner, const TCHAR* CmdLine)
{
	UE_LOG(LogPakFile, Log, TEXT("Initializing PakPlatformFile"));

	LLM_SCOPE(ELLMTag::FileSystem);
	SCOPED_BOOT_TIMING("FPakPlatformFile::Initialize");
	// Inner is required.
	check(Inner != NULL);
	LowerLevel = Inner;

	RetireReadersHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("RetirePakReaders"), 1.0f, [this](float){ this->ReleaseOldReaders(); return true; });

#if EXCLUDE_NONPAK_UE_EXTENSIONS && !WITH_EDITOR
	// Extensions for file types that should only ever be in a pak file. Used to stop unnecessary access to the lower level platform file
	ExcludedNonPakExtensions.Add(TEXT("uasset"));
	ExcludedNonPakExtensions.Add(TEXT("umap"));
	ExcludedNonPakExtensions.Add(TEXT("ubulk"));
	ExcludedNonPakExtensions.Add(TEXT("uexp"));
	ExcludedNonPakExtensions.Add(TEXT("uptnl"));
	ExcludedNonPakExtensions.Add(TEXT("ushaderbytecode"));
#endif

#if DISABLE_NONUFS_INI_WHEN_COOKED
	IniFileExtension = TEXT(".ini");
	GameUserSettingsIniFilename = TEXT("GameUserSettings.ini");
#endif

	// Signed if we have keys, and are not running with fileopenlog in non-shipping builds (currently results in a deadlock).
	bSigned = FCoreDelegates::GetPakSigningKeysDelegate().IsBound();
#if !UE_BUILD_SHIPPING
	bSigned &= !FParse::Param(FCommandLine::Get(), TEXT("fileopenlog"));
#endif

	FString StartupPaksWildcard = GMountStartupPaksWildCard;
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("StartupPaksWildcard="), StartupPaksWildcard);

	// initialize the bLookLooseFirst setting
	bLookLooseFirst = FParse::Param(FCommandLine::Get(), TEXT("LookLooseFirst"));
#endif

	FString GlobalUTocPath = FString::Printf(TEXT("%sPaks/global.utoc"), *FPaths::ProjectContentDir());
	const bool bShouldMountGlobal = FPlatformFileManager::Get().GetPlatformFile().FileExists(*GlobalUTocPath);
	if (bShouldMountGlobal || PakPlatformFile_IsForceUseIoStore(CmdLine))
	{
		if (ShouldCheckPak())
		{
			ensure(CheckIoStoreContainerBlockSignatures(*GlobalUTocPath));
		}

		FIoDispatcher& IoDispatcher = FIoDispatcher::Get();
		IoDispatcherFileBackend = CreateIoDispatcherFileBackend();
		IoDispatcher.Mount(IoDispatcherFileBackend.ToSharedRef());
		PackageStoreBackend = MakeShared<FFilePackageStoreBackend>();
		FPackageStore::Get().Mount(PackageStoreBackend.ToSharedRef());

		if (bShouldMountGlobal)
		{
			TIoStatusOr<FIoContainerHeader> IoDispatcherMountStatus = IoDispatcherFileBackend->Mount(*GlobalUTocPath, 0, FGuid(), FAES::FAESKey());
			if (IoDispatcherMountStatus.IsOk())
			{
				UE_LOG(LogPakFile, Display, TEXT("Initialized I/O dispatcher file backend. Mounted the global container: %s"), *GlobalUTocPath);
				IoDispatcher.OnSignatureError().AddLambda([](const FIoSignatureError& Error)
				{
					FPakChunkSignatureCheckFailedData FailedData(Error.ContainerName, TPakChunkHash(), TPakChunkHash(), Error.BlockIndex);
#if PAKHASH_USE_CRC
					FailedData.ExpectedHash = GetTypeHash(Error.ExpectedHash);
					FailedData.ReceivedHash = GetTypeHash(Error.ActualHash);
#else
					FailedData.ExpectedHash = Error.ExpectedHash;
					FailedData.ReceivedHash = Error.ActualHash;
#endif
					FPakPlatformFile::BroadcastPakChunkSignatureCheckFailure(FailedData);
				});
			}
			else
			{
				UE_LOG(LogPakFile, Error, TEXT("Initialized I/O dispatcher file backend. Failed to mount the global container: '%s'"), *IoDispatcherMountStatus.Status().ToString());
			}
		}
		else
		{
			UE_LOG(LogPakFile, Display, TEXT("Initialized I/O dispatcher file backend. Running with -useiostore without the global container."));
		}
	}

	// Find and mount pak files from the specified directories.
	TArray<FString> PakFolders;
	GetPakFolders(FCommandLine::Get(), PakFolders);
	MountAllPakFiles(PakFolders, *StartupPaksWildcard);

#if !UE_BUILD_SHIPPING
	GPakExec = MakeUnique<FPakExec>(*this);
#endif // !UE_BUILD_SHIPPING

	FCoreDelegates::OnMountAllPakFiles.BindRaw(this, &FPakPlatformFile::MountAllPakFiles);
	FCoreDelegates::MountPak.BindRaw(this, &FPakPlatformFile::HandleMountPakDelegate);
	FCoreDelegates::OnUnmountPak.BindRaw(this, &FPakPlatformFile::HandleUnmountPakDelegate);
	FCoreDelegates::OnOptimizeMemoryUsageForMountedPaks.BindRaw(this, &FPakPlatformFile::OptimizeMemoryUsageForMountedPaks);
	FCoreInternalDelegates::GetCurrentlyMountedPaksDelegate().BindLambda([this]()
		{
			TArray<FPakListEntry> Paks;
			GetMountedPaks(Paks);

			TArray<FMountedPakInfo> PakInfo;
			PakInfo.Reserve(Paks.Num());
			
			for (const FPakListEntry& Entry : Paks)
			{
				PakInfo.Emplace(FMountedPakInfo(Entry.PakFile, Entry.ReadOrder));
			}

			return PakInfo;
		});

	FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FPakPlatformFile::OptimizeMemoryUsageForMountedPaks);

	return !!LowerLevel;
}

void FPakPlatformFile::InitializeNewAsyncIO()
{
#if USE_PAK_PRECACHE
#if !WITH_EDITOR
	if (FPlatformProcess::SupportsMultithreading() && !FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog")))
	{
		FPakPrecacher::Init(LowerLevel, FCoreDelegates::GetPakSigningKeysDelegate().IsBound());
	}
	else
#endif
	{
		UE_CLOG(FParse::Param(FCommandLine::Get(), TEXT("FileOpenLog")), LogPakFile, Display, TEXT("Disabled pak precacher to get an accurate load order. This should only be used to collect gameopenorder.log, as it is quite slow."));
		GPakCache_Enable = 0;
	}
#endif
}

#if ENABLE_PAKFILE_RUNTIME_PRUNING
bool FPakFile::bSomePakNeedsPruning = false;
#endif

#if !UE_BUILD_SHIPPING
uint64 GetRecursiveAllocatedSize(const FPakDirectory& Index)
{
	uint64 Size = Index.GetAllocatedSize();
	for (const TPair<FString, FPakEntryLocation>& kvpair : Index)
	{
		Size += kvpair.Key.GetAllocatedSize();
	}
	return Size;
}
uint64 GetRecursiveAllocatedSize(const FPakFile::FDirectoryIndex& Index)
{
	uint64 Size = Index.GetAllocatedSize();
	for (const TPair<FString, FPakDirectory>& kvpair : Index)
	{
		Size += kvpair.Key.GetAllocatedSize();
		Size += GetRecursiveAllocatedSize(kvpair.Value);
	}
	return Size;
}
#endif

static float GPakReaderReleaseDelay = 5.0f;
static FAutoConsoleVariableRef CVarPakReaderReleaseDelay(
	TEXT("pak.ReaderReleaseDelay"),
	GPakReaderReleaseDelay,
	TEXT("If > 0, then synchronous pak readers older than this will be deleted.")
);

void FPakPlatformFile::ReleaseOldReaders()
{
	if (GPakReaderReleaseDelay == 0.0f) {
		return;
	}

	TArray<FPakListEntry> LocalPaks;
	GetMountedPaks(LocalPaks);
	for (FPakListEntry& Entry : LocalPaks)
	{
		Entry.PakFile->ReleaseOldReaders(GPakReaderReleaseDelay);
	}
}

void FPakPlatformFile::OptimizeMemoryUsageForMountedPaks()
{
#if !UE_BUILD_SHIPPING
	// UE_DEPRECATED(4.26, "UnloadPakEntryFilenamesIfPossible is deprecated.")
	bool bUnloadPakEntryFilenamesIfPossible = false;
	GConfig->GetBool(TEXT("Pak"), TEXT("UnloadPakEntryFilenamesIfPossible"), bUnloadPakEntryFilenamesIfPossible, GEngineIni);
	if (bUnloadPakEntryFilenamesIfPossible)
	{
		UE_LOG(LogPakFile, Warning, TEXT("The UnloadPakEntryFilenamesIfPossible has been deprecated and is no longer sufficient to specify the unloading of pak files.\n")
			TEXT("The choice to not load pak files is now made earlier than Ini settings are available.\n")
			TEXT("To specify that filenames should be removed from the runtime PakFileIndex, use the new runtime delegate FPakPlatformFile::GetPakSetIndexSettingsDelegate().\n")
			TEXT("In a global variable constructor that executes before the process main function, bind this delegate to a function that sets the output bool bKeepFullDirectory to false.\n")
			TEXT("See FShooterPreMainCallbacks in Samples\\Games\\ShooterGame\\Source\\ShooterGame\\Private\\ShooterGameModule.cpp for an example binding."));
	}
#endif

#if ENABLE_PAKFILE_RUNTIME_PRUNING || !UE_BUILD_SHIPPING
	TArray<FPakListEntry> Paks;
	bool bNeedsPaks = false;
#endif

#if !UE_BUILD_SHIPPING
	bNeedsPaks = true;
#endif
#if ENABLE_PAKFILE_RUNTIME_PRUNING 
	bNeedsPaks = bNeedsPaks || FPakFile::bSomePakNeedsPruning;
#endif
	if (bNeedsPaks)
	{
		GetMountedPaks(Paks);
	}

#if ENABLE_PAKFILE_RUNTIME_PRUNING
	if (FPakFile::bSomePakNeedsPruning)
	{
		for (auto& Pak : Paks)
		{
			FPakFile* PakFile = Pak.PakFile;
			if (PakFile->bWillPruneDirectoryIndex)
			{
				check(PakFile->bHasPathHashIndex);
				FWriteScopeLock DirectoryLock(PakFile->DirectoryIndexLock);
				if (PakFile->bNeedsLegacyPruning)
				{
					FPakFile::PruneDirectoryIndex(PakFile->DirectoryIndex, &PakFile->PrunedDirectoryIndex, PakFile->MountPoint);
					PakFile->bNeedsLegacyPruning = false;
				}

				Swap(PakFile->DirectoryIndex, PakFile->PrunedDirectoryIndex);
				PakFile->PrunedDirectoryIndex.Empty();
				PakFile->bHasFullDirectoryIndex = false;
				PakFile->bWillPruneDirectoryIndex = false;
			}
		}
	}
#endif

#if !UE_BUILD_SHIPPING
	uint64 DirectoryHashSize = 0;
	uint64 PathHashSize = 0;
	uint64 EntriesSize = 0;

	for (auto& Pak : Paks)
	{
		FPakFile* PakFile = Pak.PakFile;
		{
			FPakFile::FScopedPakDirectoryIndexAccess ScopeAccess(*PakFile);
			DirectoryHashSize += GetRecursiveAllocatedSize(PakFile->DirectoryIndex);
#if ENABLE_PAKFILE_RUNTIME_PRUNING 
			{
				DirectoryHashSize += GetRecursiveAllocatedSize(PakFile->PrunedDirectoryIndex);
			}
		}
#endif
		PathHashSize += PakFile->PathHashIndex.GetAllocatedSize();
		EntriesSize += PakFile->EncodedPakEntries.GetAllocatedSize();
		EntriesSize += PakFile->Files.GetAllocatedSize();
	}
	UE_LOG(LogPakFile, Log, TEXT("AllPaks IndexSizes: DirectoryHashSize=%d, PathHashSize=%d, EntriesSize=%d, TotalSize=%d"), DirectoryHashSize, PathHashSize, EntriesSize, DirectoryHashSize + PathHashSize + EntriesSize);
#endif
}


bool FPakPlatformFile::Mount(const TCHAR* InPakFilename, uint32 PakOrder, const TCHAR* InPath /*= nullptr*/, bool bLoadIndex /*= true*/, FPakListEntry* OutPakListEntry /*= nullptr*/)
{
	LLM_SCOPE(ELLMTag::FileSystem);
	bool bPakSuccess = false;
	bool bIoStoreSuccess = true;
	if (LowerLevel->FileExists(InPakFilename))
	{
		TRefCountPtr<FPakFile> Pak = new FPakFile(LowerLevel, InPakFilename, bSigned, bLoadIndex);
		if (Pak.GetReference()->IsValid())
		{
			if (!Pak->GetInfo().EncryptionKeyGuid.IsValid() || UE::FEncryptionKeyManager::Get().ContainsKey(Pak->GetInfo().EncryptionKeyGuid))
			{
				if (InPath != nullptr)
				{
					Pak->SetMountPoint(InPath);
				}
				FString PakFilename = InPakFilename;
				if (PakFilename.EndsWith(TEXT("_P.pak")))
				{
					// Prioritize based on the chunk version number
					// Default to version 1 for single patch system
					uint32 ChunkVersionNumber = 1;
					FString StrippedPakFilename = PakFilename.LeftChop(6);
					int32 VersionEndIndex = PakFilename.Find("_", ESearchCase::CaseSensitive, ESearchDir::FromEnd);
					if (VersionEndIndex != INDEX_NONE && VersionEndIndex > 0)
					{
						int32 VersionStartIndex = PakFilename.Find("_", ESearchCase::CaseSensitive, ESearchDir::FromEnd, VersionEndIndex - 1);
						if (VersionStartIndex != INDEX_NONE)
						{
							VersionStartIndex++;
							FString VersionString = PakFilename.Mid(VersionStartIndex, VersionEndIndex - VersionStartIndex);
							if (VersionString.IsNumeric())
							{
								int32 ChunkVersionSigned = FCString::Atoi(*VersionString);
								if (ChunkVersionSigned >= 1)
								{
									// Increment by one so that the first patch file still gets more priority than the base pak file
									ChunkVersionNumber = (uint32)ChunkVersionSigned + 1;
								}
							}
						}
					}
					PakOrder += 100 * ChunkVersionNumber;
				}
				{
					// Add new pak file
					FScopeLock ScopedLock(&PakListCritical);
					FPakListEntry Entry;
					Entry.ReadOrder = PakOrder;
					Entry.PakFile = Pak;
					Pak->SetIsMounted(true);
					PakFiles.Add(Entry);
					PakFiles.StableSort();

					if (OutPakListEntry)
					{
						*OutPakListEntry = MoveTemp(Entry);
					}
				}
				bPakSuccess = true;
			}
			else
			{
				UE_LOG(LogPakFile, Display, TEXT("Deferring mount of pak \"%s\" until encryption key '%s' becomes available"), InPakFilename, *Pak->GetInfo().EncryptionKeyGuid.ToString());

				check(!UE::FEncryptionKeyManager::Get().ContainsKey(Pak->GetInfo().EncryptionKeyGuid));
				FPakListDeferredEntry& Entry = PendingEncryptedPakFiles[PendingEncryptedPakFiles.Add(FPakListDeferredEntry())];
				Entry.Filename = InPakFilename;
				Entry.Path = InPath;
				Entry.ReadOrder = PakOrder;
				Entry.EncryptionKeyGuid = Pak->GetInfo().EncryptionKeyGuid;
				Entry.PakchunkIndex = Pak->PakchunkIndex;

				Pak.SafeRelease();
				return false;
			}
		}
		else
		{
			UE_LOG(LogPakFile, Warning, TEXT("Failed to mount pak \"%s\", pak is invalid."), InPakFilename);
		}

		if (bPakSuccess && IoDispatcherFileBackend.IsValid())
		{
			FGuid EncryptionKeyGuid = Pak->GetInfo().EncryptionKeyGuid;
			FAES::FAESKey EncryptionKey;

			if (!UE::FEncryptionKeyManager::Get().TryGetKey(EncryptionKeyGuid, EncryptionKey))
			{
				if (!EncryptionKeyGuid.IsValid() && FCoreDelegates::GetPakEncryptionKeyDelegate().IsBound())
				{
					FCoreDelegates::GetPakEncryptionKeyDelegate().Execute(EncryptionKey.Key);
				}
			}

			FString UtocPath = FPaths::ChangeExtension(InPakFilename, TEXT(".utoc"));
			if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*UtocPath))
			{
				if (ShouldCheckPak())
				{
					ensure(CheckIoStoreContainerBlockSignatures(*UtocPath));
				}
				TIoStatusOr<FIoContainerHeader> MountResult = IoDispatcherFileBackend->Mount(*UtocPath, PakOrder, EncryptionKeyGuid, EncryptionKey);
				if (MountResult.IsOk())
				{
					UE_LOG(LogPakFile, Display, TEXT("Mounted IoStore container \"%s\""), *UtocPath);
					Pak->IoContainerHeader = MakeUnique<FIoContainerHeader>(MountResult.ConsumeValueOrDie());
					PackageStoreBackend->Mount(Pak->IoContainerHeader.Get(), PakOrder);
#if WITH_EDITOR
					FString OptionalSegmentUtocPath = FPaths::ChangeExtension(InPakFilename, FString::Printf(TEXT("%s.utoc"), FPackagePath::GetOptionalSegmentExtensionModifier()));
					if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*OptionalSegmentUtocPath))
					{
						MountResult = IoDispatcherFileBackend->Mount(*OptionalSegmentUtocPath, PakOrder, EncryptionKeyGuid, EncryptionKey);
						if (MountResult.IsOk())
						{
							Pak->OptionalSegmentIoContainerHeader = MakeUnique<FIoContainerHeader>(MountResult.ConsumeValueOrDie());
							PackageStoreBackend->Mount(Pak->OptionalSegmentIoContainerHeader.Get(), PakOrder);
							UE_LOG(LogPakFile, Display, TEXT("Mounted optional segment extension IoStore container \"%s\""), *OptionalSegmentUtocPath);
						}
						else
						{
							UE_LOG(LogPakFile, Warning, TEXT("Failed to mount optional segment extension IoStore container \"%s\" [%s]"), *OptionalSegmentUtocPath, *MountResult.Status().ToString());
						}
					}
#endif
				}
				else
				{
					bIoStoreSuccess = false;
					UE_LOG(LogPakFile, Warning, TEXT("Failed to mount IoStore container \"%s\" [%s]"), *UtocPath, *MountResult.Status().ToString());
				}
			}
			else
			{
				bIoStoreSuccess = false;
				UE_LOG(LogPakFile, Warning, TEXT("IoStore container \"%s\" not found"), *UtocPath);
			}
		}

		if (bPakSuccess && FCoreInternalDelegates::GetOnPakMountOperation().IsBound())
		{
			FCoreInternalDelegates::GetOnPakMountOperation().Broadcast(EMountOperation::Mount, InPakFilename, PakOrder);
		}

		if (bPakSuccess)
		{
			double OnPakFileMounted2Time = 0.0;
			{
				FScopedDurationTimer Timer(OnPakFileMounted2Time);

PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (FCoreDelegates::OnPakFileMounted2.IsBound())
				{
					// Avoid calling Broadcast if not in use; Broadcast even on an unsubscribed
					// non-threadsafe delegate is not threadsafe.
					FCoreDelegates::OnPakFileMounted2.Broadcast(*Pak);
				}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

				FCoreDelegates::GetOnPakFileMounted2().Broadcast(*Pak);
			}

			UE_LOG(LogPakFile, Display, TEXT("Mounted Pak file '%s', mount point: '%s'"), InPakFilename, *Pak->GetMountPoint());
			UE_LOG(LogPakFile, Verbose, TEXT("OnPakFileMounted2Time == %lf"), OnPakFileMounted2Time);
						
			// skip this check for the default mountpoint, it is a frequently used known-good mount point
			FString NormalizedPakMountPoint = FPaths::CreateStandardFilename(Pak->GetMountPoint());
			bool bIsMountingToRoot = NormalizedPakMountPoint == FPaths::CreateStandardFilename(FPaths::RootDir());
#if WITH_EDITOR
			bIsMountingToRoot |= NormalizedPakMountPoint == FPaths::CreateStandardFilename(FPaths::GameFeatureRootPrefix());
#endif
			if (!bIsMountingToRoot)
			{
				FString OutPackageName;
				const FString& MountPoint = Pak->GetMountPoint();
				if (!FPackageName::TryConvertFilenameToLongPackageName(MountPoint, OutPackageName))
				{
					// Possibly the mount point is a parent path of mount points, e.g. <ProjectRoot>/Plugins,
					// parent path of <ProjectRoot>/Plugins/PluginA and <ProjectRoot>/Plugins/PluginB.
					// Do not warn in that case.
					FString MountPointAbsPath = FPaths::ConvertRelativePathToFull(MountPoint);
					bool bParentOfMountPoint = false;
					for (const FString& ExistingMountPoint : FPackageName::QueryMountPointLocalAbsPaths())
					{
						if (FPathViews::IsParentPathOf(MountPointAbsPath, ExistingMountPoint))
						{
							bParentOfMountPoint = true;
							break;
						}
					}
					if (!bParentOfMountPoint)
					{
						UE_LOG(LogPakFile, Display,
							TEXT("Mount point '%s' is not mounted to a valid Root Path yet, ")
							TEXT("assets in this pak file may not be accessible until a corresponding UFS Mount Point is added through FPackageName::RegisterMountPoint."),
							*MountPoint);
					}
				}
			}
		}
		else
		{
			Pak.SafeRelease();
		}
	}
	else
	{
		UE_LOG(LogPakFile, Warning, TEXT("Failed to open pak \"%s\""), InPakFilename);
	}
	return bPakSuccess && bIoStoreSuccess;
}

bool FPakPlatformFile::Unmount(const TCHAR* InPakFilename)
{
	TRefCountPtr<FPakFile> UnmountedPak;
	bool bRemovedContainerFile = false;
	{
		FScopeLock ScopedLock(&PakListCritical);
		for (int32 PakIndex = 0; PakIndex < PakFiles.Num(); PakIndex++)
		{
			FPakListEntry& PakListEntry = PakFiles[PakIndex];
			if (PakFiles[PakIndex].PakFile->GetFilename() == InPakFilename)
			{
				UnmountedPak = MoveTemp(PakListEntry.PakFile);
				PakFiles.RemoveAt(PakIndex);
				break;
			}
		}
	}

	if (UnmountedPak)
	{
		RemoveCachedPakSignaturesFile(*UnmountedPak->GetFilename());
	}

	if (IoDispatcherFileBackend.IsValid())
	{
		if (UnmountedPak)
		{
			PackageStoreBackend->Unmount(UnmountedPak->IoContainerHeader.Get());
		}
		FString ContainerPath = FPaths::ChangeExtension(InPakFilename, FString());
		bRemovedContainerFile = IoDispatcherFileBackend->Unmount(*ContainerPath);
#if WITH_EDITOR
		if (UnmountedPak && UnmountedPak->OptionalSegmentIoContainerHeader.IsValid())
		{
			PackageStoreBackend->Unmount(UnmountedPak->OptionalSegmentIoContainerHeader.Get());
			FString OptionalSegmentContainerPath = ContainerPath + FPackagePath::GetOptionalSegmentExtensionModifier();
			IoDispatcherFileBackend->Unmount(*OptionalSegmentContainerPath);
		}
#endif
	}

	if (UnmountedPak)
	{
		UnmountedPak->Readers.Empty();
	}
#if USE_PAK_PRECACHE
	if (GPakCache_Enable)
	{
		// If the Precacher is running, we need to clear the IsMounted flag inside of its lock,
		// to avoid races with RegisterPakFile which reads the flag inside of the lock
		FPakPrecacher::Get().Unmount(InPakFilename, UnmountedPak.GetReference());
		check(!UnmountedPak || !UnmountedPak->GetIsMounted())
	}
	else
#endif
	{
		if (UnmountedPak)
		{
			UnmountedPak->SetIsMounted(false);
		}
	}
	return UnmountedPak.IsValid() || bRemovedContainerFile;
}

bool FPakPlatformFile::ReloadPakReaders()
{
	TArray<FPakListEntry> Paks;
	GetMountedPaks(Paks);
	for (FPakListEntry& Pak : Paks)
	{
		if (!Pak.PakFile->RecreatePakReaders(LowerLevel))
		{
			return false;
		}
	}

	if (IoDispatcherFileBackend)
	{
		IoDispatcherFileBackend->ReopenAllFileHandles();
	}

	return true;
}

IFileHandle* FPakPlatformFile::CreatePakFileHandle(const TCHAR* Filename, const TRefCountPtr<FPakFile>& PakFile, const FPakEntry* FileEntry)
{
	IFileHandle* Result = nullptr;
	TAcquirePakReaderFunction AcquirePakReader = [StoredPakFile=TRefCountPtr<FPakFile>(PakFile), LowerLevelPlatformFile = LowerLevel]() -> FSharedPakReader
	{
		return StoredPakFile->GetSharedReader(LowerLevelPlatformFile);
	};

	// Create the handle.
	const TRefCountPtr<const FPakFile>& ConstPakFile = (const TRefCountPtr<const FPakFile>&)PakFile;
	if (FileEntry->CompressionMethodIndex != 0 && PakFile->GetInfo().Version >= FPakInfo::PakFile_Version_CompressionEncryption)
	{
		if (FileEntry->IsEncrypted())
		{
			Result = new FPakFileHandle< FPakCompressedReaderPolicy<FPakSimpleEncryption> >(ConstPakFile, *FileEntry, AcquirePakReader);
		}
		else
		{
			Result = new FPakFileHandle< FPakCompressedReaderPolicy<> >(ConstPakFile, *FileEntry, AcquirePakReader);
		}
	}
	else if (FileEntry->IsEncrypted())
	{
		Result = new FPakFileHandle< FPakReaderPolicy<FPakSimpleEncryption> >(ConstPakFile, *FileEntry, AcquirePakReader);
	}
	else
	{
		Result = new FPakFileHandle<>(ConstPakFile, *FileEntry, AcquirePakReader);
	}

	return Result;
}

int32 FPakPlatformFile::MountAllPakFiles(const TArray<FString>& PakFolders)
{
	return MountAllPakFiles(PakFolders, TEXT(ALL_PAKS_WILDCARD));
}

int32 FPakPlatformFile::MountAllPakFiles(const TArray<FString>& PakFolders, const FString& WildCard)
{
	int32 NumPakFilesMounted = 0;

	bool bMountPaks = true;
	TArray<FString> PaksToLoad;
#if !UE_BUILD_SHIPPING
	// Optionally get a list of pak filenames to load, only these paks will be mounted
	FString CmdLinePaksToLoad;
	if (FParse::Value(FCommandLine::Get(), TEXT("-paklist="), CmdLinePaksToLoad))
	{
		CmdLinePaksToLoad.ParseIntoArray(PaksToLoad, TEXT("+"), true);
	}
#endif

	if (bMountPaks)
	{
		TArray<FString> FoundPakFiles;
		FindAllPakFiles(LowerLevel, PakFolders, WildCard, FoundPakFiles);

		// HACK: If no pak files are found with the wildcard, fallback to mounting everything.
		if (FoundPakFiles.Num() == 0)
		{
			FindAllPakFiles(LowerLevel, PakFolders, ALL_PAKS_WILDCARD, FoundPakFiles);
		}

		// Sort in descending order.
		FoundPakFiles.Sort(TGreater<FString>());
		// Mount all found pak files

		TArray<FPakListEntry> ExistingPaks;
		GetMountedPaks(ExistingPaks);
		TSet<FString> ExistingPaksFileName;
		// Find the single pak we just mounted
		for (const FPakListEntry& Pak : ExistingPaks)
		{
			ExistingPaksFileName.Add(Pak.PakFile->GetFilename());
		}


		for (int32 PakFileIndex = 0; PakFileIndex < FoundPakFiles.Num(); PakFileIndex++)
		{
			const FString& PakFilename = FoundPakFiles[PakFileIndex];

			UE_LOG(LogPakFile, Display, TEXT("Found Pak file %s attempting to mount."), *PakFilename);

			if (PaksToLoad.Num() && !PaksToLoad.Contains(FPaths::GetBaseFilename(PakFilename)))
			{
				continue;
			}

			if (ExistingPaksFileName.Contains(PakFilename))
			{
				UE_LOG(LogPakFile, Display, TEXT("Pak file %s already exists."), *PakFilename);
				continue;
			}

			uint32 PakOrder = GetPakOrderFromPakFilePath(PakFilename);

			UE_LOG(LogPakFile, Display, TEXT("Mounting pak file %s."), *PakFilename);

			SCOPED_BOOT_TIMING("Pak_Mount");
			if (Mount(*PakFilename, PakOrder))
			{
				++NumPakFilesMounted;
			}
		}
	}
	return NumPakFilesMounted;
}

int32 FPakPlatformFile::GetPakOrderFromPakFilePath(const FString& PakFilePath)
{
	if (PakFilePath.StartsWith(FString::Printf(TEXT("%sPaks/%s-"), *FPaths::ProjectContentDir(), FApp::GetProjectName())))
	{
		return 4;
	}
	else if (PakFilePath.StartsWith(FPaths::ProjectContentDir()))
	{
		return 3;
	}
	else if (PakFilePath.StartsWith(FPaths::EngineContentDir()))
	{
		return 2;
	}
	else if (PakFilePath.StartsWith(FPaths::ProjectSavedDir()))
	{
		return 1;
	}

	return 0;
}

IPakFile* FPakPlatformFile::HandleMountPakDelegate(const FString& PakFilePath, int32 PakOrder)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Mounting pak file: %s \n"), *PakFilePath);

	if (PakOrder == INDEX_NONE)
	{
		PakOrder = GetPakOrderFromPakFilePath(PakFilePath);
	}
	
	FPakListEntry Pak;
	if (Mount(*PakFilePath, PakOrder, nullptr, true, &Pak))
	{
		return Pak.PakFile;
	}
	return nullptr;
}

bool FPakPlatformFile::HandleUnmountPakDelegate(const FString& PakFilePath)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Unmounting pak file: %s \n"), *PakFilePath);

	return Unmount(*PakFilePath);
}

void FPakPlatformFile::RegisterEncryptionKey(const FGuid& InGuid, const FAES::FAESKey& InKey)
{
	int32 NumMounted = 0;
	TSet<int32> ChunksToNotify;

	for (const FPakListDeferredEntry& Entry : PendingEncryptedPakFiles)
	{
		if (Entry.EncryptionKeyGuid == InGuid)
		{
			if (Mount(*Entry.Filename, Entry.ReadOrder, Entry.Path.Len() == 0 ? nullptr : *Entry.Path))
			{
				UE_LOG(LogPakFile, Log, TEXT("Successfully mounted deferred pak file '%s'"), *Entry.Filename);
				NumMounted++;

				int32 PakchunkIndex = GetPakchunkIndexFromPakFile(Entry.Filename);
				if (PakchunkIndex != INDEX_NONE)
				{
					ChunksToNotify.Add(PakchunkIndex);
				}
			}
			else
			{
				UE_LOG(LogPakFile, Warning, TEXT("Failed to mount deferred pak file '%s'"), *Entry.Filename);
			}
		}
	}

	if (NumMounted > 0)
	{
		IPlatformChunkInstall * ChunkInstall = FPlatformMisc::GetPlatformChunkInstall();
		if (ChunkInstall)
		{
			for (int32 PakchunkIndex : ChunksToNotify)
			{
				ChunkInstall->ExternalNotifyChunkAvailable(PakchunkIndex);
			}
		}

		PendingEncryptedPakFiles.RemoveAll([InGuid](const FPakListDeferredEntry& Entry) { return Entry.EncryptionKeyGuid == InGuid; });

		{
			LLM_SCOPE(ELLMTag::FileSystem);
			OptimizeMemoryUsageForMountedPaks();
		}

		UE_LOG(LogPakFile, Log, TEXT("Registered encryption key '%s': %d pak files mounted, %d remain pending"), *InGuid.ToString(), NumMounted, PendingEncryptedPakFiles.Num());
	}
}

IFileHandle* FPakPlatformFile::OpenRead(const TCHAR* Filename, bool bAllowWrite)
{
	IFileHandle* Result = NULL;

#if !UE_BUILD_SHIPPING
	if (bLookLooseFirst && IsNonPakFilenameAllowed(Filename))
	{
		Result = LowerLevel->OpenRead(Filename, bAllowWrite);
		if (Result != nullptr)
		{
			return Result;
		}
	}
#endif


	TRefCountPtr<FPakFile> PakFile;
	FPakEntry FileEntry;
	if (FindFileInPakFiles(Filename, &PakFile, &FileEntry))
	{
#if PAK_TRACKER
		TrackPak(Filename, &FileEntry);
#endif

		Result = CreatePakFileHandle(Filename, PakFile, &FileEntry);

		if (Result)
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			FCoreDelegates::OnFileOpenedForReadFromPakFile.Broadcast(*PakFile->GetFilename(), Filename);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
			FCoreDelegates::GetOnFileOpenedForReadFromPakFile().Broadcast(*PakFile->GetFilename(), Filename);
		}
	}
	else
	{
		if (IsNonPakFilenameAllowed(Filename))
		{
			// Default to wrapped file
			Result = LowerLevel->OpenRead(Filename, bAllowWrite);
		}
	}
	return Result;
}

IFileHandle* FPakPlatformFile::OpenWrite(const TCHAR* Filename, bool bAppend, bool bAllowRead)
{
	// No modifications allowed on pak files.
	if (FindFileInPakFiles(Filename))
	{
		return nullptr;
	}
	// Use lower level to handle writing.
	return LowerLevel->OpenWrite(Filename, bAppend, bAllowRead);
}

const TCHAR* FPakPlatformFile::GetMountStartupPaksWildCard()
{
	return *GMountStartupPaksWildCard;
}

void FPakPlatformFile::SetMountStartupPaksWildCard(const FString& WildCard)
{
	GMountStartupPaksWildCard = WildCard;
}


EChunkLocation::Type FPakPlatformFile::GetPakChunkLocation(int32 InPakchunkIndex) const
{
	FScopeLock ScopedLock(&PakListCritical);

	for (const FPakListEntry& PakEntry : PakFiles)
	{
		if (PakEntry.PakFile->PakchunkIndex == InPakchunkIndex)
		{
			return EChunkLocation::LocalFast;
		}
	}

	for (const FPakListDeferredEntry& PendingPak : PendingEncryptedPakFiles)
	{
		if (PendingPak.PakchunkIndex == InPakchunkIndex)
		{
			return EChunkLocation::NotAvailable;
		}
	}

	return EChunkLocation::DoesNotExist;
}

bool FPakPlatformFile::AnyChunksAvailable() const
{
	FScopeLock ScopedLock(&PakListCritical);

	for (const FPakListEntry& PakEntry : PakFiles)
	{
		if (PakEntry.PakFile->PakchunkIndex != INDEX_NONE)
		{
			return true;
		}
	}

	for (const FPakListDeferredEntry& PendingPak : PendingEncryptedPakFiles)
	{
		if (PendingPak.PakchunkIndex != INDEX_NONE)
		{
			return true;
		}
	}

	return false;
}

bool FPakPlatformFile::BufferedCopyFile(IFileHandle& Dest, IFileHandle& Source, const int64 FileSize, uint8* Buffer, const int64 BufferSize) const
{
	int64 RemainingSizeToCopy = FileSize;
	// Continue copying chunks using the buffer
	while (RemainingSizeToCopy > 0)
	{
		const int64 SizeToCopy = FMath::Min(BufferSize, RemainingSizeToCopy);
		if (Source.Read(Buffer, SizeToCopy) == false)
		{
			return false;
		}
		if (Dest.Write(Buffer, SizeToCopy) == false)
		{
			return false;
		}
		RemainingSizeToCopy -= SizeToCopy;
	}
	return true;
}

bool FPakPlatformFile::CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags, EPlatformFileWrite WriteFlags)
{
#if !UE_BUILD_SHIPPING
	if (bLookLooseFirst && LowerLevel->FileExists(From))
	{
		return LowerLevel->CopyFile(To, From, ReadFlags, WriteFlags);
	}
#endif


	bool Result = false;
	FPakEntry FileEntry;
	TRefCountPtr<FPakFile> PakFile;
	if (FindFileInPakFiles(From, &PakFile, &FileEntry))
	{
		// Copy from pak to LowerLevel->
		// Create handles both files.
		TUniquePtr<IFileHandle> DestHandle(LowerLevel->OpenWrite(To, false, (WriteFlags & EPlatformFileWrite::AllowRead) != EPlatformFileWrite::None));
		TUniquePtr<IFileHandle> SourceHandle(CreatePakFileHandle(From, PakFile, &FileEntry));

		if (DestHandle && SourceHandle)
		{
			const int64 BufferSize = 64 * 1024; // Copy in 64K chunks.
			uint8* Buffer = (uint8*)FMemory::Malloc(BufferSize);
			Result = BufferedCopyFile(*DestHandle, *SourceHandle, SourceHandle->Size(), Buffer, BufferSize);
			FMemory::Free(Buffer);
		}
	}
	else
	{
		Result = LowerLevel->CopyFile(To, From, ReadFlags, WriteFlags);
	}
	return Result;
}

/**
 * Module for the pak file
 */
class FPakFileModule : public IPlatformFileModule
{
public:
	virtual IPlatformFile* GetPlatformFile() override
	{
		check(Singleton.IsValid());
		return Singleton.Get();
	}

	virtual void StartupModule() override
	{
		Singleton = MakeUnique<FPakPlatformFile>();
		FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("RSA"));
	}

	virtual void ShutdownModule() override
	{
		// remove ourselves from the platform file chain (there can be late writes after the shutdown).
		if (Singleton.IsValid())
		{
			if (FPlatformFileManager::Get().FindPlatformFile(Singleton.Get()->GetName()))
			{
				FPlatformFileManager::Get().RemovePlatformFile(Singleton.Get());
			}
		}

		Singleton.Reset();
	}

	TUniquePtr<IPlatformFile> Singleton;
};

void FPakFile::AddSpecialFile(const FPakEntry& Entry, const FString& Filename)
{
	MakeDirectoryFromPath(MountPoint);

	// TODO: This function is not threadsafe; readers of the Indexes will be invalidated when we modify them
	// To make it threadsafe would require always holding the lock around any read of either index, which is
	// more expensive than we want to support this debug feature
	FPakEntryLocation EntryLocation;
	if (!Entry.IsDeleteRecord())
	{
		// Add new file info.
		TArray<uint8> NewEncodedPakEntries;
		FMemoryWriter MemoryWriter(NewEncodedPakEntries);
		EntryLocation = FPakEntryLocation::CreateFromOffsetIntoEncoded(EncodedPakEntries.Num());
		if (EncodePakEntry(MemoryWriter, Entry, Info))
		{
			EncodedPakEntries.Append(NewEncodedPakEntries);
			EncodedPakEntries.Shrink();
		}
		else
		{
			EntryLocation = FPakEntryLocation::CreateFromListIndex(Files.Num());
			Files.Add(Entry);
			Files.Shrink();
		}
		NumEntries++;
	}

	FPathHashIndex* PathHashToWrite = bHasPathHashIndex ? &PathHashIndex : nullptr;
	AddEntryToIndex(Filename, EntryLocation, MountPoint, PathHashSeed, &DirectoryIndex, PathHashToWrite, nullptr /* CollisionDetection */, Info.Version);
}

void FPakPlatformFile::MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>& InFiles)
{
	for (int k = 0; k < InFiles.Num(); k++)
	{
		TRefCountPtr<FPakFile> NewPakFile;
		for (int i = 0; i < InFiles[k].Num(); i++)
		{
			FPakEntry FileEntry;
			TRefCountPtr<FPakFile> ExistingRealPakFile;
			bool bFoundEntry = FindFileInPakFiles(*InFiles[k][i], &ExistingRealPakFile, &FileEntry);
			if (bFoundEntry && ExistingRealPakFile && ExistingRealPakFile->GetFilenameName() != NAME_None)
			{
				if (NewPakFile == nullptr)
				{
					// Mount another copy of the existing real PakFile, but don't allow it to Load the index, so it intializes as empty
					const bool bLoadIndex = false;
					if (Mount(*ExistingRealPakFile->GetFilename(), 500, *ExistingRealPakFile->MountPoint, bLoadIndex))
					{
						// we successfully mounted the file, find the empty pak file we just added.
						for (int j = 0; j < PakFiles.Num(); j++)
						{
							FPakFile& PotentialNewPakFile = *PakFiles[j].PakFile;
							if (PotentialNewPakFile.PakFilename == ExistingRealPakFile->PakFilename &&  // It has the right name
								PotentialNewPakFile.CachedTotalSize == ExistingRealPakFile->CachedTotalSize && // And it has the right size
								PotentialNewPakFile.GetNumFiles() == 0) // And it didn't load the index
							{
								NewPakFile = &PotentialNewPakFile;
								break;
							}
						}

						if (NewPakFile != nullptr)
						{
							NewPakFile->SetCacheType(FPakFile::ECacheType::Individual);
						}
					}
				}

				if (NewPakFile != nullptr)
				{
//					NewPakFile->SetUnderlyingCacheTrimDisabled(true);
					NewPakFile->AddSpecialFile(FileEntry, *InFiles[k][i]);
				}

			}
		}

	}
}

IMPLEMENT_MODULE(FPakFileModule, PakFile);

#if !UE_BUILD_SHIPPING
static void AsyncFileTest(const TArray<FString>& Args)
{
	FString TestFile;
	if (Args.Num() == 0)
	{
		UE_LOG(LogPakFile, Error, TEXT("pak.AsyncFileTest requires a filename argument: \"pak.AsyncFileTest <filename> <size> <offset>\""));
		return;
	}

	TestFile = Args[0];
	int64 Size = 1;
	if (Args.Num() > 1)
	{
		Size = -1;
		LexFromString(Size, *Args[1]);
		if (Size <= 0)
		{
			UE_LOG(LogPakFile, Error, TEXT("pak.AsyncFileTest size must be > 0: \"pak.AsyncFileTest <filename> <size> <offset>\""));
			return;
		}
	}

	int64 Offset = 0;
	if (Args.Num() > 2)
	{
		Offset = -1;
		LexFromString(Offset, *Args[2]);
		if (Size < 0)
		{
			UE_LOG(LogPakFile, Error, TEXT("pak.AsyncFileTest offset must be >= 0: \"pak.AsyncFileTest <filename> <size> <offset>\""));
			return;
		}
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TUniquePtr<IAsyncReadFileHandle> FileHandle(PlatformFile.OpenAsyncRead(*TestFile));
	check(FileHandle);
	{
		TUniquePtr<IAsyncReadRequest> SizeRequest(FileHandle->SizeRequest());
		if (!SizeRequest)
		{
			UE_LOG(LogPakFile, Error, TEXT("pak.AsyncFileTest: SizeRequest failed for %s."), *TestFile, Size, Offset);
			return;
		}
		SizeRequest->WaitCompletion();
		int64 TotalSize = SizeRequest->GetSizeResults();
		SizeRequest.Reset();
		if (Offset + Size > TotalSize)
		{
			UE_LOG(LogPakFile, Error, TEXT("pak.AsyncFileTest: Requested size offset is out of range for %s. Size=%" INT64_FMT", Offset=%" INT64_FMT ", End=%" INT64_FMT ", Available Size = %" INT64_FMT "."),
				*TestFile, Size, Offset, Size + Offset, TotalSize);
			return;
		}

		TUniquePtr<IAsyncReadRequest> ReadRequest(FileHandle->ReadRequest(Offset, Size));
		if (!ReadRequest)
		{
			UE_LOG(LogPakFile, Error, TEXT("pak.AsyncFileTest: ReadRequest failed for %s size %" INT64_FMT " offset %" INT64_FMT "."), *TestFile, Size, Offset);
			return;
		}

		ReadRequest.Reset();
		FPlatformProcess::Sleep(3.0f);
	}
	FileHandle.Reset();

	UE_LOG(LogPakFile, Display, TEXT("pak.AsyncFileTest: ReadRequest succeeded with no errors for %s size %" INT64_FMT " offset %" INT64_FMT "."), *TestFile, Size, Offset);
}

static FAutoConsoleCommand AsyncFileTestCmd(
	TEXT("pak.AsyncFileTest"),
	TEXT("Read a block of data from a file using an AsyncFileHandle. params: <filename> <size> <offset>"),
	FConsoleCommandWithArgsDelegate::CreateStatic(AsyncFileTest));
#endif

