// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "Containers/StringConv.h"
#include "Containers/Ticker.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "GenericPlatform/GenericPlatformChunkInstall.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformTime.h"
#include "HAL/UnrealMemory.h"
#include "IO/IoContainerId.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Math/BigInt.h"
#include "Math/NumericLimits.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AES.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/CompressionFlags.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/IEngineCrypto.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeRWLock.h"
#include "Misc/SecureHash.h"
#include "RSA.h"
#include "Serialization/Archive.h"
#include "Serialization/MemoryImage.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Templates/Function.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FChunkCacheWorker;
class FFileIoStore;
class FFilePackageStoreBackend;
class FOutputDevice;
class IAsyncReadFileHandle;
class IMappedFileHandle;
namespace UE::PakFile::Private { class FPakFileDirectoryVisitorBase; }
struct FIoContainerHeader;

PAKFILE_API DECLARE_LOG_CATEGORY_EXTERN(LogPakFile, Log, All);
DECLARE_FLOAT_ACCUMULATOR_STAT_EXTERN(TEXT("Total pak file read time"), STAT_PakFile_Read, STATGROUP_PakFile, PAKFILE_API);

DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Num open pak file handles"), STAT_PakFile_NumOpenHandles, STATGROUP_PakFile, PAKFILE_API);

#define PAK_TRACKER 0

// ENABLE_PAKFILE_RUNTIME_PRUNING allows pruning the DirectoryIndex at runtime after all Paks have loaded rather than loading only the already-pruned DirectoryIndex
// This requires extra cputime to make reads of the DirectoryIndex ThreadSafe, and will be removed in a future version
#ifndef ENABLE_PAKFILE_RUNTIME_PRUNING
#define ENABLE_PAKFILE_RUNTIME_PRUNING 1
#endif
#define ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE ENABLE_PAKFILE_RUNTIME_PRUNING && !UE_BUILD_SHIPPING

// Define the type of a chunk hash. Currently selectable between SHA1 and CRC32.
#define PAKHASH_USE_CRC	1
#if PAKHASH_USE_CRC
typedef uint32 TPakChunkHash;
#else
typedef FSHAHash TPakChunkHash;
#endif

PAKFILE_API TPakChunkHash ComputePakChunkHash(const void* InData, int64 InDataSizeInBytes);
FORCEINLINE FString ChunkHashToString(const TPakChunkHash& InHash)
{
#if PAKHASH_USE_CRC
	return FString::Printf(TEXT("%08X"), InHash);
#else
	return LexToString(InHash);
#endif
}

struct FPakChunkSignatureCheckFailedData
{
	FPakChunkSignatureCheckFailedData(const FString& InPakFilename, const TPakChunkHash& InExpectedHash, const TPakChunkHash& InReceivedHash, int32 InChunkIndex)
		: PakFilename(InPakFilename)
		, ChunkIndex(InChunkIndex)
		, ExpectedHash(InExpectedHash)
		, ReceivedHash(InReceivedHash)
	{
	}
	FString PakFilename;
	int32 ChunkIndex;
	TPakChunkHash ExpectedHash;
	TPakChunkHash ReceivedHash;

	FPakChunkSignatureCheckFailedData() : ChunkIndex(0) {}
};
/** Delegate for allowing a game to restrict the accessing of non-pak files */
DECLARE_DELEGATE_RetVal_OneParam(bool, FFilenameSecurityDelegate, const TCHAR* /*InFilename*/);
DECLARE_DELEGATE_ThreeParams(FPakCustomEncryptionDelegate, uint8* /*InData*/, uint32 /*InDataSize*/, FGuid /*InEncryptionKeyGuid*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FPakChunkSignatureCheckFailedHandler, const FPakChunkSignatureCheckFailedData&);
DECLARE_MULTICAST_DELEGATE_OneParam(FPakPrincipalSignatureTableCheckFailureHandler, const FString&);
/** Delegate which allows a project to configure Index Pruning.  This is a delegate instead of a config file because config files are loaded after the first PakFiles */
DECLARE_DELEGATE_ThreeParams(FPakSetIndexSettings, bool& /* bKeepFullDirectory*/, bool& /* bValidatePruning */, bool& /* bDelayPruning */);

UE_DEPRECATED("5.1", "Use FPakPrincipalSignatureTableCheckFailureHandler instead")
typedef FPakPrincipalSignatureTableCheckFailureHandler FPakMasterSignatureTableCheckFailureHandler;
/**
 * Struct which holds pak file info (version, index offset, hash value).
 */
struct FPakInfo
{
	enum 
	{
		/** Magic number to use in header */
		PakFile_Magic = 0x5A6F12E1,
		/** Size of cached data. */
		MaxChunkDataSize = 64*1024,
		/** Length of a compression format name */
		CompressionMethodNameLen = 32,
		/** Number of allowed different methods */
		MaxNumCompressionMethods=5, // when we remove patchcompatibilitymode421 we can reduce this to 4
	};

	/** Version numbers. */
	enum
	{
		PakFile_Version_Initial = 1,
		PakFile_Version_NoTimestamps = 2,
		PakFile_Version_CompressionEncryption = 3,
		PakFile_Version_IndexEncryption = 4,
		PakFile_Version_RelativeChunkOffsets = 5,
		PakFile_Version_DeleteRecords = 6,
		PakFile_Version_EncryptionKeyGuid = 7,
		PakFile_Version_FNameBasedCompressionMethod = 8,
		PakFile_Version_FrozenIndex = 9,
		PakFile_Version_PathHashIndex = 10,
		PakFile_Version_Fnv64BugFix = 11,


		PakFile_Version_Last,
		PakFile_Version_Invalid,
		PakFile_Version_Latest = PakFile_Version_Last - 1
	};

	/** Pak file magic value. */
	uint32 Magic;
	/** Pak file version. */
	int32 Version;
	/** Offset to pak file index. */
	int64 IndexOffset;
	/** Size (in bytes) of pak file index. */
	int64 IndexSize;
	/** SHA1 of the bytes in the index, used to check for data corruption when loading the index. */
	FSHAHash IndexHash;
	/** Flag indicating if the pak index has been encrypted. */
	uint8 bEncryptedIndex;
	/** Encryption key guid. Empty if we should use the embedded key. */
	FGuid EncryptionKeyGuid;
	/** Compression methods used in this pak file (FNames, saved as FStrings) */
	TArray<FName> CompressionMethods;

	/**
	 * Constructor.
	 */
	FPakInfo()
		: Magic(PakFile_Magic)
		, Version(PakFile_Version_Latest)
		, IndexOffset(-1)
		, IndexSize(0)
		, bEncryptedIndex(0)
	{
		// we always put in a NAME_None entry as index 0, so that an uncompressed PakEntry will have CompressionMethodIndex of 0 and can early out easily
		CompressionMethods.Add(NAME_None);
	}

	/**
	 * Gets the size of data serialized by this struct.
	 *
	 * @return Serialized data size.
	 */
	int64 GetSerializedSize(int32 InVersion = PakFile_Version_Latest) const
	{
		int64 Size = sizeof(Magic) + sizeof(Version) + sizeof(IndexOffset) + sizeof(IndexSize) + sizeof(IndexHash) + sizeof(bEncryptedIndex);
		if (InVersion >= PakFile_Version_EncryptionKeyGuid) Size += sizeof(EncryptionKeyGuid);
		if (InVersion >= PakFile_Version_FNameBasedCompressionMethod) Size += CompressionMethodNameLen * MaxNumCompressionMethods;
		if (InVersion >= PakFile_Version_FrozenIndex && InVersion < PakFile_Version_PathHashIndex) Size += sizeof(bool);

		return Size;
	}

	/**
	 */
	int64 HasRelativeCompressedChunkOffsets() const
	{
		return Version >= PakFile_Version_RelativeChunkOffsets;
	}

	/**
	 * Serializes this struct.
	 *
	 * @param Ar Archive to serialize data with.
	 */
	void Serialize(FArchive& Ar, int32 InVersion)
	{
		if (Ar.IsLoading() && Ar.TotalSize() < (Ar.Tell() + GetSerializedSize(InVersion)))
		{
			Magic = 0;
			return;
		}

		if (Ar.IsSaving() || InVersion >= PakFile_Version_EncryptionKeyGuid)
		{
			Ar << EncryptionKeyGuid;
		}
		Ar << bEncryptedIndex;
		Ar << Magic;
		if (Magic != PakFile_Magic)
		{
			// handle old versions by failing out now (earlier versions will be attempted)
			Magic = 0;
			return;
		}

		Ar << Version;
		Ar << IndexOffset;
		Ar << IndexSize;
		Ar << IndexHash;

		if (Ar.IsLoading())
		{
			if (Version < PakFile_Version_IndexEncryption)
			{
				bEncryptedIndex = false;
			}

			if (Version < PakFile_Version_EncryptionKeyGuid)
			{
				EncryptionKeyGuid.Invalidate();
			}
		}

		if (Version >= PakFile_Version_FrozenIndex && Version < PakFile_Version_PathHashIndex)
		{
			bool bIndexIsFrozen = false;
			Ar << bIndexIsFrozen;
			if (bIndexIsFrozen)
			{
				UE_LOG(LogPakFile, Fatal, TEXT("PakFile was frozen with version FPakInfo::PakFile_Version_FrozenIndex, which is no longer supported. Regenerate Paks."));
			}
		}

		if (Version < PakFile_Version_FNameBasedCompressionMethod)
		{
			// for old versions, put in some known names that we may have used
			CompressionMethods.Add(NAME_Zlib);
			CompressionMethods.Add(NAME_Gzip);
			CompressionMethods.Add(TEXT("Oodle"));
		}
		else
		{
			// we need to serialize a known size, so make a buffer of "strings"
			const int32 BufferSize = CompressionMethodNameLen * MaxNumCompressionMethods;
			ANSICHAR Methods[BufferSize];
			if (Ar.IsLoading())
			{
				Ar.Serialize(Methods, BufferSize);
				for (int32 Index = 0; Index < MaxNumCompressionMethods; Index++)
				{
					ANSICHAR* MethodString = &Methods[Index * CompressionMethodNameLen];
					if (MethodString[0] != 0)
					{
						// Make sure if the file has garbage we don't read off in to lala land.
						MethodString[CompressionMethodNameLen-1] = 0;
						CompressionMethods.Add(FName(MethodString));
					}
				}
			}
			else
			{
				// we always zero out fully what we write out so that reading in is simple
				FMemory::Memzero(Methods, BufferSize);

				for (int32 Index = 1; Index < CompressionMethods.Num(); Index++)
				{
					ANSICHAR* MethodString = &Methods[(Index - 1) * CompressionMethodNameLen];
					FCStringAnsi::Strcpy(MethodString, CompressionMethodNameLen, TCHAR_TO_ANSI(*CompressionMethods[Index].ToString()));
				}
				Ar.Serialize(Methods, BufferSize);
			}
		}
	}

	int32 GetCompressionMethodIndex(FName CompressionMethod)
	{
		// look for existing method
		for (uint8 Index = 0; Index < CompressionMethods.Num(); Index++)
		{
			if (CompressionMethods[Index] == CompressionMethod)
			{
				return Index;
			}
		}

		checkf(CompressionMethod.ToString().Len() < CompressionMethodNameLen, TEXT("Compression method name, %s, is too long for pak file serialization. You can increase CompressionMethodNameLen, but then will have to handle version management."), *CompressionMethod.ToString());
		// CompressionMethods always has None at Index 0, that we don't serialize, so we can allow for one more in the array
		checkf(CompressionMethods.Num() <= MaxNumCompressionMethods, TEXT("Too many unique compression methods in one pak file. You can increase MaxNumCompressionMethods, but then will have to handle version management."));

		// add it if it didn't exist
		return CompressionMethods.Add(CompressionMethod);
	}

	FName GetCompressionMethod(uint32 Index) const
	{
		return CompressionMethods[Index];
	}
	
	TOptional<FName> TryGetCompressionMethod(uint32 Index) const
	{
		if (IntFitsIn<int32>(Index) == false)
		{
			return TOptional<FName>();
		}
		return (int32)Index >= CompressionMethods.Num() ? TOptional<FName>() : TOptional<FName>(CompressionMethods[(int32)Index]);
	}
};

/**
 * Struct storing offsets and sizes of a compressed block.
 */
struct FPakCompressedBlock
{
	/** Offset of the start of a compression block. Offset is relative to the start of the compressed chunk data */
	int64 CompressedStart;
	/** Offset of the end of a compression block. This may not align completely with the start of the next block. Offset is relative to the start of the compressed chunk data. */
	int64 CompressedEnd;

	bool operator == (const FPakCompressedBlock& B) const
	{
		return CompressedStart == B.CompressedStart && CompressedEnd == B.CompressedEnd;
	}

	bool operator != (const FPakCompressedBlock& B) const
	{
		return !(*this == B);
	}
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FPakCompressedBlock& Block)
{
	Ar << Block.CompressedStart;
	Ar << Block.CompressedEnd;
	return Ar;
}

/**
 * Struct holding info about a single file stored in pak file.
 *
 * CHANGE THIS FILE RARELY AND WITH GREAT CARE. MODIFICATIONS
 * WILL RESULT IN EVERY PAK ENTRY IN AN EXISTING INSTALL HAVING TO
 * TO BE PATCHED.
 *
*  On Fortnite that would be 15GB of data 
* (250k pak entries * 64kb patch block) just to add/change/remove 
 * a field.
 * 
 */
struct FPakEntry
{
	static const uint8 Flag_None = 0x00;
	static const uint8 Flag_Encrypted = 0x01;
	static const uint8 Flag_Deleted = 0x02;

	/** Offset into pak file where the file is stored.*/
	int64 Offset;
	/** Serialized file size. */
	int64 Size;
	/** Uncompressed file size. */
	int64 UncompressedSize;
	/** File SHA1 value. */
	uint8 Hash[20];
	/** Array of compression blocks that describe how to decompress this pak entry. */
	TArray<FPakCompressedBlock> CompressionBlocks;
	/** Size of a compressed block in the file. */
	uint32 CompressionBlockSize;
	/** Index into the compression methods in this pakfile. */
	uint32 CompressionMethodIndex;
	/** Pak entry flags. */
	uint8 Flags;
	/** Flag is set to true when FileHeader has been checked against PakHeader. It is not serialized. */
	mutable bool  Verified;

	/**
	 * Constructor.
	 */
	FPakEntry()
	{
		Reset();
	}

	/**
	 * Gets the size of data serialized by this struct.
	 *
	 * @return Serialized data size.
	 */
	int64 GetSerializedSize(int32 Version) const
	{
		int64 SerializedSize = sizeof(Offset) + sizeof(Size) + sizeof(UncompressedSize) + sizeof(Hash);

		if (Version >= FPakInfo::PakFile_Version_FNameBasedCompressionMethod)
		{
			SerializedSize += sizeof(CompressionMethodIndex);
		}
		else
		{
			SerializedSize += sizeof(int32); // Old CompressedMethod var from pre-fname based compression methods
		}

		if (Version >= FPakInfo::PakFile_Version_CompressionEncryption)
		{
			SerializedSize += sizeof(Flags) + sizeof(CompressionBlockSize);
			if(CompressionMethodIndex != 0)
			{
				SerializedSize += sizeof(FPakCompressedBlock) * CompressionBlocks.Num() + sizeof(int32);
			}
		}
		if (Version < FPakInfo::PakFile_Version_NoTimestamps)
		{
			// Timestamp
			SerializedSize += sizeof(int64);
		}
		return SerializedSize;
	}

	/**
	 * Compares two FPakEntry structs.
	 */
	bool operator == (const FPakEntry& B) const
	{
		return IndexDataEquals(B) &&
			FMemory::Memcmp(Hash, B.Hash, sizeof(Hash)) == 0;
	}

	/**
	 * Compares two FPakEntry structs.
	 */
	bool operator != (const FPakEntry& B) const
	{
		return !(*this == B);
	}

	bool IndexDataEquals(const FPakEntry& B) const
	{
		// Offset are only in the Index and so are not compared
		// Hash is only in the payload and so are not compared
		// Verified is only in the payload and is mutable and so is not compared
		return Size == B.Size &&
			UncompressedSize == B.UncompressedSize &&
			CompressionMethodIndex == B.CompressionMethodIndex &&
			Flags == B.Flags &&
			CompressionBlockSize == B.CompressionBlockSize &&
			CompressionBlocks == B.CompressionBlocks;
	}

	void Reset()
	{
		Offset = -1;
		Size = 0;
		UncompressedSize = 0;
		FMemory::Memset(Hash, 0, sizeof(Hash));
		CompressionBlocks.Reset();
		CompressionBlockSize = 0;
		CompressionMethodIndex = 0;
		Flags = Flag_None;
		Verified = false;
	}

	/**
	 * Serializes FPakEntry struct.
	 *
	 * @param Ar Archive to serialize data with.
	 * @param Entry Data to serialize.
	 */
	void Serialize(FArchive& Ar, int32 Version)
	{
		Ar << Offset;
		Ar << Size;
		Ar << UncompressedSize;
		if (Version < FPakInfo::PakFile_Version_FNameBasedCompressionMethod)
		{
			int32 LegacyCompressionMethod;
			Ar << LegacyCompressionMethod;
			if (LegacyCompressionMethod == COMPRESS_None)
			{
				CompressionMethodIndex = 0;
			}
			else if (LegacyCompressionMethod & COMPRESS_ZLIB)
			{
				CompressionMethodIndex = 1;
			}
			else if (LegacyCompressionMethod & COMPRESS_GZIP)
			{
				CompressionMethodIndex = 2;
			}
			else if (LegacyCompressionMethod & COMPRESS_Custom)
			{
				CompressionMethodIndex = 3;
			}
			else
			{
				UE_LOG(LogPakFile, Fatal, TEXT("Found an unknown compression type in pak file, will need to be supported for legacy files"));
			}
		}
		else
		{
			Ar << CompressionMethodIndex;
		}
		if (Version <= FPakInfo::PakFile_Version_Initial)
		{
			FDateTime Timestamp;
			Ar << Timestamp;
		}
		Ar.Serialize(Hash, sizeof(Hash));
		if (Version >= FPakInfo::PakFile_Version_CompressionEncryption)
		{
			if(CompressionMethodIndex != 0)
			{
				Ar << CompressionBlocks;
			}
			Ar << Flags;
			Ar << CompressionBlockSize;
		}
	}

	FORCEINLINE void SetFlag( uint8 InFlag, bool bValue )
	{
		if( bValue )
		{
			Flags |= InFlag;
		}
		else
		{
			Flags &= ~InFlag;
		}
	}

	FORCEINLINE bool GetFlag( uint8 InFlag ) const
	{
		return (Flags & InFlag) == InFlag;
	}
	
	FORCEINLINE bool IsEncrypted() const             { return GetFlag(Flag_Encrypted); }
	FORCEINLINE void SetEncrypted( bool bEncrypted ) { SetFlag( Flag_Encrypted, bEncrypted ); }

	FORCEINLINE bool IsDeleteRecord() const                { return GetFlag(Flag_Deleted); }
	FORCEINLINE void SetDeleteRecord( bool bDeleteRecord ) { SetFlag(Flag_Deleted, bDeleteRecord ); }


	/**
	* Verifies two entries match to check for corruption.
	*
	* @param FileEntryA Entry 1.
	* @param FileEntryB Entry 2.
	*/
	static bool VerifyPakEntriesMatch(const FPakEntry& FileEntryA, const FPakEntry& FileEntryB);
};

/**
 * An identifier for the location of an FPakEntry in an FDirectoryIndex or an FPathHashIndex.
 * Contains a byte offset into the encoded array of FPakEntry data, an index into the list of unencodable FPakEntries, or a marker indicating invalidity
 */
struct FPakEntryLocation
{
public:
	/*
	 * 0x00000000 - 0x7ffffffe: EncodedOffset from 0 to MaxIndex
	 * 0x7fffffff: Unused, interpreted as Invalid
	 * 0x80000000: Invalid
	 * 0x80000001 - 0xffffffff: FileIndex from MaxIndex to 0
	*/
	static const int32 Invalid = MIN_int32;
	static const int32 MaxIndex = MAX_int32 - 1;

	FPakEntryLocation() : Index(Invalid)
	{
	}
	FPakEntryLocation(const FPakEntryLocation& Other) = default;
	FPakEntryLocation& operator=(const FPakEntryLocation& other) = default;

	static FPakEntryLocation CreateInvalid()
	{
		return FPakEntryLocation();
	}

	static FPakEntryLocation CreateFromOffsetIntoEncoded(int32 Offset)
	{
		check(0 <= Offset && Offset <= MaxIndex);
		return FPakEntryLocation(Offset);
	}

	static FPakEntryLocation CreateFromListIndex(int32 ListIndex)
	{
		check(0 <= ListIndex && ListIndex <= MaxIndex);
		return FPakEntryLocation(-ListIndex - 1);
	}

	bool IsInvalid() const
	{
		return Index <= Invalid || MaxIndex < Index;
	}

	bool IsOffsetIntoEncoded() const
	{
		return 0 <= Index && Index <= MaxIndex;
	}

	bool IsListIndex() const
	{
		return (-MaxIndex - 1) <= Index && Index <= -1;
	}

	int32 GetAsOffsetIntoEncoded() const
	{
		if (IsOffsetIntoEncoded())
		{
			return Index;
		}
		else
		{
			return -1;
		}
	}
	int32 GetAsListIndex() const
	{
		if (IsListIndex())
		{
			return -(Index + 1);
		}
		else
		{
			return -1;
		}
	}

	void Serialize(FArchive& Ar)
	{
		Ar << Index;
	}

	bool operator==(const FPakEntryLocation& Other) const
	{
		return Index == Other.Index;
	}
private:
	explicit FPakEntryLocation(int32 InIndex) : Index(InIndex)
	{
	}

	int32 Index;
};
FORCEINLINE FArchive& operator<<(FArchive& Ar, FPakEntryLocation& PakEntryLocation)
{
	PakEntryLocation.Serialize(Ar);
	return Ar;
}


class FPakFile;

// Wrapper for a pointer to a shared pak reader archive that has been temporarily acquired. 
class FSharedPakReader final 
{
	friend class FPakFile;

	FArchive* Archive = nullptr;
	FPakFile* PakFile = nullptr; // Pak file to return ownership to on destruction

	PAKFILE_API FSharedPakReader(FArchive* InArchive, FPakFile* InPakFile);

public:
	PAKFILE_API ~FSharedPakReader();

	FSharedPakReader(const FSharedPakReader& Other) = delete;
	FSharedPakReader& operator=(const FSharedPakReader& Other) = delete;
	PAKFILE_API FSharedPakReader(FSharedPakReader&& Other);
	PAKFILE_API FSharedPakReader& operator=(FSharedPakReader&& Other);

	explicit operator bool() const { return Archive != nullptr; }
	bool operator==(nullptr_t) { return Archive == nullptr; }
	bool operator!=(nullptr_t) { return Archive != nullptr; }
	FArchive* operator->() { return Archive; }

	
	// USE WITH CARE, the FSharedPakReader must live longer than this reference to prevent the archive being used by another thread. Do not call on a temporary return value!
	FArchive& GetArchive() { return *Archive; } 

};

/** Pak directory type mapping a filename to an FPakEntryLocation. */
typedef TMap<FString, FPakEntryLocation> FPakDirectory;

/* Convenience struct for building FPakFile indexes from an enumeration of (Filename,FPakEntry) pairs */
struct FPakEntryPair
{
	FString Filename;
	FPakEntry Info;
};

/**
 * Pak file.
 */
class FPakFile : FNoncopyable, public FRefCountBase, public IPakFile
{
public:
	/** Index data that provides a map from the hash of a Filename to an FPakEntryLocation */
	typedef TMap<uint64, FPakEntryLocation> FPathHashIndex;
	/** Index data that keeps an in-memory directoryname/filename tree to map a Filename to an FPakEntryLocation */
	typedef TMap<FString, FPakDirectory> FDirectoryIndex;

	/** Pak files can share a cache or have their own */
	enum class ECacheType : uint8
	{
		Shared,
		Individual,
	};

	/** A ReadLock wrapper that must be used to prevent threading errors around any call to FindPrunedDirectory or internal uses of DirectoryIndex */
	struct FScopedPakDirectoryIndexAccess
	{
		FScopedPakDirectoryIndexAccess(const FPakFile& InPakFile)
#if ENABLE_PAKFILE_RUNTIME_PRUNING
			: PakFile(InPakFile)
			, bRequiresDirectoryIndexLock(PakFile.RequiresDirectoryIndexLock())
#endif
		{
#if ENABLE_PAKFILE_RUNTIME_PRUNING
			if (bRequiresDirectoryIndexLock)
			{
				PakFile.DirectoryIndexLock.ReadLock();
			}
#endif
		}
#if ENABLE_PAKFILE_RUNTIME_PRUNING
		~FScopedPakDirectoryIndexAccess()
		{
			if (bRequiresDirectoryIndexLock)
			{
				PakFile.DirectoryIndexLock.ReadUnlock();
			}
		}
		const FPakFile& PakFile;
		bool bRequiresDirectoryIndexLock;
#endif
	};

	/** Recreates the pak reader for each thread */
	PAKFILE_API bool RecreatePakReaders(IPlatformFile* LowerLevel);

	struct FArchiveAndLastAccessTime 
	{
		TUniquePtr<FArchive> Archive;
		double LastAccessTime;
	};

private:
	friend class FPakPlatformFile;

	/** Pak filename. */
	FString PakFilename;
	FName PakFilenameName;
	/** Archive to serialize the pak file from. */
	TUniquePtr<class FChunkCacheWorker> Decryptor;
	/** List of readers and when they were last used. */
	TArray<FArchiveAndLastAccessTime> Readers;
	/** How many readers have been loaned out and not yet returned. If this is >0 we should not destroy the decryptor. */
	std::atomic<int32> CurrentlyUsedReaders = 0;
	/** Critical section for accessing Readers. */
	FCriticalSection ReadersCriticalSection;
	/** Pak file info (trailer). */
	FPakInfo Info;
	/** Mount point. */
	FString MountPoint;
	/** Info on all files stored in pak. */
	TArray<FPakEntry> Files;
	/** Pak Index organized as a map of directories to support searches by path.  This Index is pruned at runtime of all FileNames and Paths that are not allowed by DirectoryIndexKeepFiles */
	FDirectoryIndex DirectoryIndex;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	/** Temporary-lifetime copy of the Pruned DirectoryIndex; all Pruned files have been removed form this copy.  This copy is used for validation that no queries are missing during runtime, and will be swapped into the DirectoryIndex when Pak Mounting is complete */
	FDirectoryIndex PrunedDirectoryIndex;
	/** ReaderWriter lock to guard DirectoryIndex iteration from being interrupted by the swap of PrunedDirectoryIndex */
	mutable FRWLock DirectoryIndexLock;
#endif

	/** Index data that provides a map from the hash of a Filename to an FPakEntryLocation */
	FPathHashIndex PathHashIndex;
	/* FPakEntries that have been serialized into a compacted format in an array of bytes. */
	TArray<uint8> EncodedPakEntries;
	/* The seed passed to the hash function for hashing filenames in this pak.  Differs per pack so that the same filename in different paks has different hashes */
	uint64 PathHashSeed;

	/** The number of file entries in the pak file */
	int32 NumEntries;
	/** Timestamp of this pak file. */
	FDateTime Timestamp;	
	/** TotalSize of the pak file */
	int64 CachedTotalSize;
	/** True if this is a signed pak file. */
	bool bSigned;
	/** True if this pak file is valid and usable. */
	bool bIsValid;
	/* True if the PathHashIndex has been populated for this PakFile */
	bool bHasPathHashIndex;
	/* True if the DirectoryIndex has not been pruned and still contains a Filename for every FPakEntry in this PakFile */
	bool bHasFullDirectoryIndex;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
	/* True if we have a FullDirectoryIndex that we will modify in OptimizeMemoryUsageForMountedPaks and therefore need to guard access with DirectoryIndexLock */
	bool bWillPruneDirectoryIndex;
	/* True if the Index of this PakFile was a legacy index that did not have the precomputed Pruned DirectoryIndex and we need to compute it before swapping the Pruned DirectoryIndex*/
	bool bNeedsLegacyPruning;
#endif
	/** ID for the chunk this pakfile is part of. INDEX_NONE if this isn't a pak chunk (derived from filename) */
	int32 PakchunkIndex;

	class IMappedFileHandle* MappedFileHandle;
	FCriticalSection MappedFileHandleCriticalSection;

	/** The type of cache this pak file should have */
	ECacheType	CacheType;
	/** The index of this pak file into the cache array, -1 = not initialized */
	int32		CacheIndex;
	/** Allow the cache of a pak file to never shrink, should be used with caution, it will burn memory */
	bool UnderlyingCacheTrimDisabled;
	/** Record of whether the pak file is still mounted, so PakPrecacher can reject requests to register it. */
	bool bIsMounted;

	TUniquePtr<FIoContainerHeader> IoContainerHeader;
#if WITH_EDITOR
	TUniquePtr<FIoContainerHeader> OptionalSegmentIoContainerHeader;
#endif

	static inline int32 CDECL CompareFilenameHashes(const void* Left, const void* Right)
	{
		const uint64* LeftHash = (const uint64*)Left;
		const uint64* RightHash = (const uint64*)Right;
		if (*LeftHash < *RightHash)
		{
			return -1;
		}
		if (*LeftHash > *RightHash)
		{
			return 1;
		}
		return 0;
	}

	PAKFILE_API FArchive* CreatePakReader(IPlatformFile* LowerLevel, const TCHAR* Filename);
	PAKFILE_API FArchive* SetupSignedPakReader(FArchive* Reader, const TCHAR* Filename);


public:
	// IPakFile interface, for users of PakFiles that cannot have a dependency on this header
	virtual const FString& PakGetPakFilename() const override
	{
		return PakFilename;
	}

	virtual bool PakContains(const FString& FullPath) const override
	{
		return Find(FullPath, nullptr) == EFindResult::Found;
	}

	virtual int32 PakGetPakchunkIndex() const override
	{
		return PakchunkIndex;
	}

	virtual void PakVisitPrunedFilenames(IPlatformFile::FDirectoryVisitor& Visitor) const override
	{
		for (FFilenameIterator It(*this); It; ++It)
		{
			Visitor.CallShouldVisitAndVisit(*It.Filename(), false);
		}
	}

	virtual const FString& PakGetMountPoint() const override
	{
		return MountPoint;
	}


	void SetUnderlyingCacheTrimDisabled(bool InUnderlyingCacheTrimDisabled) { UnderlyingCacheTrimDisabled = InUnderlyingCacheTrimDisabled; }
	bool GetUnderlyingCacheTrimDisabled(void) { return UnderlyingCacheTrimDisabled; }

	void SetCacheType(ECacheType InCacheType) { CacheType = InCacheType; }
	ECacheType GetCacheType(void) { return CacheType; }
	void SetCacheIndex(int32 InCacheIndex) { CacheIndex = InCacheIndex; }
	int32 GetCacheIndex(void) { return CacheIndex; }
	void SetIsMounted(bool bInIsMounted) { bIsMounted = bInIsMounted; }
	bool GetIsMounted() const { return bIsMounted; }
#if IS_PROGRAM
	/**
	* Opens a pak file given its filename.
	*
	* @param Filename Pak filename.
	* @param bIsSigned true if the pak is signed
	*/
	PAKFILE_API FPakFile(const TCHAR* Filename, bool bIsSigned);
#endif

	/**
	 * Creates a pak file using the supplied file handle.
	 *
	 * @param LowerLevel Lower level platform file.
	 * @param Filename Filename.
	 * @param bIsSigned = true if the pak is signed.
	 */
	PAKFILE_API FPakFile(IPlatformFile* LowerLevel, const TCHAR* Filename, bool bIsSigned, bool bLoadIndex = true);

	/**
	 * Creates a pak file using the supplied archive.
	 *
	 * @param Archive	Pointer to the archive which contains the pak file data.
	 */
#if WITH_EDITOR
	PAKFILE_API FPakFile(FArchive* Archive);
#endif

private:
	/** Private destructor, use AddRef/Release instead */
	PAKFILE_API virtual ~FPakFile();
	friend class FRefCountBase;

public:

	/**
	 * Checks if the pak file is valid.
	 *
	 * @return true if this pak file is valid, false otherwise.
	 */
	bool IsValid() const
	{
		return bIsValid;
	}

	/**
	 * Checks if the pak has valid chunk signature checking data, and that the data passed the initial signing check
	 *
	 * @return true if this pak file has passed the initial signature checking phase
	 */
	PAKFILE_API bool PassedSignatureChecks() const;

	/**
	 * Gets pak filename.
	 *
	 * @return Pak filename.
	 */
	const FString& GetFilename() const
	{
		return PakFilename;
	}
	FName GetFilenameName() const
	{
		return PakFilenameName;
	}

	int64 TotalSize() const
	{
		return CachedTotalSize;
	}

	/**
	 * Gets the number of files in this pak.
	 */
	virtual int32 GetNumFiles() const override
	{
		return NumEntries;
	}

	/** Returns the FullPath (includes Mount) Filename found in Pruned DirectoryIndex */
	PAKFILE_API void GetPrunedFilenames(TArray<FString>& OutFileList) const;

	/** Returns the RelativePathFromMount Filename for every Filename found in the Pruned DirectoryIndex that points to a PakEntry in the given Chunk */
	PAKFILE_API void GetPrunedFilenamesInChunk(const TArray<int32>& InChunkIDs, TArray<FString>& OutFileList) const;

	/**
	 * Gets shared pak file archive for given thread.
	 *
	 * @return Pointer to pak file archive used to read data from pak.
	 */
	PAKFILE_API FSharedPakReader GetSharedReader(IPlatformFile* LowerLevel);

	// Return a shared pak reader. Should only be called from the FSharedPakReader's destructor.
	PAKFILE_API void ReturnSharedReader(FArchive* SharedReader);

	// Delete all readers that haven't been used in MaxAgeSeconds.
	PAKFILE_API void ReleaseOldReaders(double MaxAgeSeconds);

	/**
	 * Finds an entry in the pak file matching the given filename.
	 *
	 * @param Filename File to find.
	 * @param OutEntry The optional address of an FPakEntry instance where the found file information should be stored. Pass NULL to only check for file existence.
	 * @return Returns true if the file was found, false otherwise.
	 */
	enum class EFindResult : uint8
	{
		NotFound,
		Found,
		FoundDeleted,
	};
	PAKFILE_API EFindResult Find(const FString& FullPath, FPakEntry* OutEntry) const;

	/**
	 * Sets the pak file mount point.
	 *
	 * @param Path New mount point path.
	 */
	void SetMountPoint(const TCHAR* Path)
	{
		MountPoint = Path;
		MakeDirectoryFromPath(MountPoint);
	}

	/**
	 * Gets pak file mount point.
	 *
	 * @return Mount point path.
	 */
	const FString& GetMountPoint() const
	{
		return MountPoint;
	}

	template <class ContainerType>
	UE_DEPRECATED(5.4, "Use version that takes a TArray<FString>& instead.")
	void FindPrunedFilesAtPath(ContainerType& OutFiles, const TCHAR* InPath,
		bool bIncludeFiles = true, bool bIncludeDirectories = false, bool bRecursive = false) const
	{
		TArray<FString> LocalFiles;
		FindPrunedFilesAtPath(InPath, LocalFiles, bIncludeFiles, bIncludeDirectories, bRecursive);
		for (FString& File : LocalFiles)
		{
			OutFiles.Add(File);
		}
	}

	/**
	 * Looks for files or directories within the Pruned DirectoryIndex of the pak file.
	 * The Pruned DirectoryIndex does not have entries for most Files in the pak; they were removed to save memory.
	 * A project can specify which FileNames and DirectoryNames can be marked to keep in the DirectoryIndex; see FPakFile::FIndexSettings and FPakFile::PruneDirectoryIndex
	 * Returned paths are full paths (include the mount point)
	 *
	 * @param InPath Path to look for files or folder at.
	 * @param OutFiles List of files or folder matching search criteria.
	 * @param bIncludeFiles If true OutFiles will include matching files.
	 * @param bIncludeDirectories If true OutFiles will include matching folders.
	 * @param bRecursive If true, sub-folders will also be checked.
	 */
	PAKFILE_API void FindPrunedFilesAtPath(const TCHAR* InPath, TArray<FString>& OutFiles,
		bool bIncludeFiles = true, bool bIncludeDirectories = false, bool bRecursive = false) const;

	/**
	 * Finds a directory in pak file.
	 *
	 * @param InPath Directory path.
	 * @return Pointer to a map with directory contents if the directory was found, NULL otherwise.
	 */
	const FPakDirectory* FindPrunedDirectory(const TCHAR* InPath) const
	{
		// Caller holds FScopedPakDirectoryIndexAccess
		FString RelativePathFromMount;
		if (!NormalizeDirectoryQuery(InPath, RelativePathFromMount))
		{
			return nullptr;
		}

		return FindPrunedDirectoryInternal(RelativePathFromMount);
	}

	/**
	 * Checks if a directory exists in pak file.
	 *
	 * @param InPath Directory path.
	 * @return true if the given path exists in pak file, false otherwise.
	 */
	bool DirectoryExistsInPruned(const TCHAR* InPath) const
	{
		FString RelativePathFromMount;
		if (!NormalizeDirectoryQuery(InPath, RelativePathFromMount))
		{
			return false;
		}

		FScopedPakDirectoryIndexAccess ScopeAccess(*this);
		return FindPrunedDirectoryInternal(RelativePathFromMount) != nullptr;
	}

	/**
	 * Checks the validity of the pak data by reading out the data for every file in the pak
	 *
	 * @return true if the pak file is valid
	 */
	PAKFILE_API bool Check();

	/** Base functionality for iterating over the DirectoryIndex. */
	class FBaseIterator
	{
	private:
		/** Owner pak file. */
		const FPakFile& PakFile;
		/** Outer iterator over Directories when using the FDirectoryIndex. */
		FDirectoryIndex::TConstIterator DirectoryIndexIt;
		/** Inner iterator over Files when using the FDirectoryIndex. */
		FPakDirectory::TConstIterator DirectoryIt;
		/** Iterator when using the FPathHashIndex. */
		FPathHashIndex::TConstIterator PathHashIt;
		/** The cached filename for return in Filename(). */
		mutable FString CachedFilename;
		/* The PakEntry for return in Info */
		mutable FPakEntry PakEntry;
		/* Whether to use the PathHashIndex. */
		bool bUsePathHash;
		/** Whether to include delete records in the iteration. */
		bool bIncludeDeleted;
#if ENABLE_PAKFILE_RUNTIME_PRUNING
		/** Whether this iterator needs to ReadLock and ReadUnlock due to use of the DirectoryIndex */
		bool bRequiresDirectoryIndexLock;
#endif

	public:
		FBaseIterator& operator++()
		{
			if (bUsePathHash)
			{
				++PathHashIt;
			}
			else
			{
				++DirectoryIt;
			}
			AdvanceToValid();
			return *this;
		}

		/** conversion to "bool" returning true if the iterator is valid. */
		FORCEINLINE explicit operator bool() const
		{
			if (bUsePathHash)
			{
				return !!PathHashIt;
			}
			else
			{
				return !!DirectoryIndexIt;
			}
		}

		/** inverse of the "bool" operator */
		FORCEINLINE bool operator !() const
		{
			return !(bool)*this;
		}

		/** Return the FPakEntry. Invalid to call unless the iterator is currently valid. */
		const FPakEntry& Info() const
		{
			PakFile.GetPakEntry(GetPakEntryIndex(), &PakEntry);
			return PakEntry;
		}

		bool HasFilename() const
		{
			return !bUsePathHash;
		}

	protected:
		FBaseIterator(const FPakFile& InPakFile, bool bInIncludeDeleted, bool bInUsePathHash)
			: PakFile(InPakFile)
			, DirectoryIndexIt(FDirectoryIndex())
			, DirectoryIt(FPakDirectory())
			, PathHashIt(PakFile.PathHashIndex)
			, bUsePathHash(bInUsePathHash)
			, bIncludeDeleted(bInIncludeDeleted)
#if ENABLE_PAKFILE_RUNTIME_PRUNING
			, bRequiresDirectoryIndexLock(false)
#endif
		{
			if (bUsePathHash)
			{
				check(PakFile.bHasPathHashIndex);
			}
			else
			{
#if ENABLE_PAKFILE_RUNTIME_PRUNING
				bRequiresDirectoryIndexLock = PakFile.RequiresDirectoryIndexLock();
				if (bRequiresDirectoryIndexLock)
				{
					PakFile.DirectoryIndexLock.ReadLock();
				}
#endif
				DirectoryIndexIt.~TConstIterator();
				new (&DirectoryIndexIt) FDirectoryIndex::TConstIterator(InPakFile.DirectoryIndex);
				if (DirectoryIndexIt)
				{
					DirectoryIt.~TConstIterator();
					new (&DirectoryIt) FPakDirectory::TConstIterator(DirectoryIndexIt.Value());
				}
			}

			AdvanceToValid();
		}

#if ENABLE_PAKFILE_RUNTIME_PRUNING
		~FBaseIterator()
		{
			if (bRequiresDirectoryIndexLock)
			{
				PakFile.DirectoryIndexLock.ReadUnlock();
			}
		}
#endif

		/** Return the current filename, as the RelativePath from the MountPoint.  Only available when using the FDirectoryIndex, otherwise always returns empty string. Invalid to call unless the iterator is currently valid. */
		const FString& Filename() const
		{
			if (bUsePathHash)
			{
				// Filenames are not supported, CachedFilename is always empty 
			}
			else
			{
				checkf(DirectoryIndexIt && DirectoryIt, TEXT("It is not legal to call Filename() on an invalid iterator"));
				if (CachedFilename.IsEmpty())
				{
					CachedFilename = PakPathCombine(DirectoryIndexIt.Key(), DirectoryIt.Key());
				}
			}
			return CachedFilename;
		}

		/** Return the arbitrary index of the iteration. Invalid to call unless the iterator is currently valid. */
		const FPakEntryLocation GetPakEntryIndex() const
		{
			if (bUsePathHash)
			{
				return PathHashIt.Value();
			}
			else
			{
				return DirectoryIt.Value();
			}
		}

	private:

		/* Skips over deleted records and moves to the next Directory in the DirectoryIndex when necessary. */
		FORCEINLINE void AdvanceToValid()
		{
			if (bUsePathHash)
			{
				while (PathHashIt && !bIncludeDeleted && Info().IsDeleteRecord())
				{
					++PathHashIt;
				}
			}
			else
			{
				while (DirectoryIndexIt && (!DirectoryIt || (!bIncludeDeleted && Info().IsDeleteRecord())))
				{
					if (DirectoryIt)
					{
						++DirectoryIt;
					}
					else
					{
						// No more files in the current directory, jump to the next one.
						++DirectoryIndexIt;
						if (DirectoryIndexIt)
						{
							DirectoryIt.~TConstIterator();
							new (&DirectoryIt) FPakDirectory::TConstIterator(DirectoryIndexIt.Value());
						}
					}
				}
				CachedFilename.Reset();
			}
		}
	};

	/** Iterator class for every FPakEntry in the FPakFile, but does not provide filenames unless the PakFile has an unpruned DirectoryIndex. */
	class FPakEntryIterator : public FBaseIterator
	{
	public:
		FPakEntryIterator(const FPakFile& InPakFile, bool bInIncludeDeleted = false)
			: FBaseIterator(InPakFile, bInIncludeDeleted, !InPakFile.bHasFullDirectoryIndex /* bUsePathHash */)
		{
		}

		const FString* TryGetFilename() const
		{
			if (HasFilename())
			{
				return &Filename();
			}
			else
			{
				return nullptr;
			}
		}
	};

	/** Iterator class used to iterate over just the files in the pak for which we have filenames. */
	class FFilenameIterator : public FBaseIterator
	{
	public:
		/**
		 * Constructor.
		 *
		 * @param InPakFile Pak file to iterate.
		 */
		FFilenameIterator(const FPakFile& InPakFile, bool bInIncludeDeleted = false)
			: FBaseIterator(InPakFile, bInIncludeDeleted, false /* bUsePathHash */)
		{
		}

		using FBaseIterator::Filename;
	};

	/**
	 * Gets this pak file info.
	 *
	 * @return Info about this pak file.
	 */
	const FPakInfo& GetInfo() const
	{
		return Info;
	}

	/**
	 * Gets this pak file's tiemstamp.
	 *
	 * @return Timestamp.
	 */
	const FDateTime& GetTimestamp() const
	{
		return Timestamp;
	}

	/**
	 * Returns whether filenames currently exist in the DirectoryIndex for all files in the Pak.
	 *
	 * @return true if filenames are present for all FPakEntry, false otherwise.
	 */
	bool HasFilenames() const
	{
		return bHasFullDirectoryIndex;
	}

	// FPakFile helper functions shared between the runtime and UnrealPak.exe
	/*
	 * Given a FPakEntry from the index, seek to the payload and read the hash of the payload out of the payload entry
	 * Warning: Slow function, do not use in performance critical operations
	 *
	 * @param PakEntry the FPakEntry from the index, which has the Offset to read to
	 * @param OutBuffer a buffer at least sizeof(FPakEntry::Hash) in size, into which the hash will be copied
	 */
	void ReadHashFromPayload(const FPakEntry& PakEntry, uint8* OutBuffer)
	{
		if (PakEntry.IsDeleteRecord())
		{
			FMemory::Memset(OutBuffer, 0, sizeof(FPakEntry::Hash));
		}
		else
		{
			TUniquePtr<FArchive> Reader {CreatePakReader(nullptr, *GetFilename())};
			Reader->Seek(PakEntry.Offset);
			FPakEntry SerializedEntry;
			SerializedEntry.Serialize(*Reader, GetInfo().Version);
			FMemory::Memcpy(OutBuffer, &SerializedEntry.Hash, sizeof(SerializedEntry.Hash));
		}
	}

	/** Hash the given full-path filename using the hash function used by FPakFiles, with the given FPakFile-specific seed, with version provided for legacy pak files that used different hash function */
	static PAKFILE_API uint64 HashPath(const TCHAR* RelativePathFromMount, uint64 Seed, int32 PakFileVersion);

	/** Read a list of (Filename, FPakEntry) pairs from a provided enumeration, attempt to encode each one,
	  * store each one in the appropriate given encoded and/or unencoded array, and populate the given
	  * Directories to map each filename to the location for the FPakEntry

	  * @param InNumEntries How many entries will be read from ReadNextEntryFunction
	  * @param ReadNextEntryFunction Callback called repeatedly to enumerate the (Filename,FPakEntry) pairs to be encoded
	  * @param InPakFilename Filename for the pak containing the files, used to create the hashseed for the given pak
	  * @param InPakInfo PakInfo for the given pak, used for serialization flags
	  * @param MountPoint Directory into which the pak will be mounted, used to create the Directory and PathHash indexes
	  * @param OutNumEncodedEntries How many entries were written to the bytes in OutEncodedPakEntries
	  * @param OutNumDeletedEntries How many entries were skipped and not stored because the input FPakEntry was a Delete record
	  * @param OutPathHashSeed optional out param to get a copy of the pakfile-specific hashseed
	  * @param OutDirectoryIndex optional output FDirectoryIndex
	  * @param OutPathHashIndex optional output FPathHashIndex
	  * @param OutEncodedPakEntries array of bytes into which the encoded FPakEntries are stored.  Values in OutDirectoryIndex and OutPathHashIndex can be offsets into this array indicated the start point for the encoding of the given FPakEntry
	  * @param OutNonEncodableEntries A list of all the FPakEntries that could not be encoded.  Values in OutDirectoryIndex and OutPathHashIndex can be indices into this list.
	  * @param InOutCollisionDetection Optional parameter to detect hash collisions.  If present, each hashed filename will be check()'d for a collision against a different filename in InOutCollisionDetection, and will be added into InOutCollisionDetection
	  * @param PakFileVersion Version of the pakfile containing the index, to support legacy formats
	  */
	typedef TFunction<FPakEntryPair & ()> ReadNextEntryFunction;
	static PAKFILE_API void EncodePakEntriesIntoIndex(int32 InNumEntries, const ReadNextEntryFunction& InReadNextEntry, const TCHAR* InPakFilename, const FPakInfo& InPakInfo, const FString& MountPoint,
		int32& OutNumEncodedEntries, int32& OutNumDeletedEntries, uint64* OutPathHashSeed,
		FDirectoryIndex* OutDirectoryIndex, FPathHashIndex* OutPathHashIndex, TArray<uint8>& OutEncodedPakEntries, TArray<FPakEntry>& OutNonEncodableEntries, TMap<uint64, FString>* InOutCollisionDetection,
		int32 PakFileVersion);

	/** Lookup the FPakEntryLocation stored in the given PathHashIndex, return nullptr if not found */
	static PAKFILE_API const FPakEntryLocation* FindLocationFromIndex(const FString& FullPath, const FString& MountPoint, const FPathHashIndex& PathHashIndex, uint64 PathHashSeed, int32 PakFileVersion);

	/** Lookup the FPakEntryLocation stored in the given DirectoryIndex, return nullptr if not found */
	static PAKFILE_API const FPakEntryLocation* FindLocationFromIndex(const FString& FullPath, const FString& MountPoint, const FDirectoryIndex& DirectoryIndex);

	/**
	  * Returns the FPakEntry pointed to by the given FPakEntryLocation inside the given EncodedPakEntries or Files
	  * Can return Found or Deleted; if the FPakEntryLocation is invalid this function assumes the FPakEntry exists in this pack but as a deleted file
	  * If OutEntry is non-null, populates it with a copy of the FPakEntry found, or sets it to
	  * an FPakEntry with SetDeleteRecord(true) if not found
	  */
	static PAKFILE_API EFindResult GetPakEntry(const FPakEntryLocation& FPakEntryLocation, FPakEntry* OutEntry, const TArray<uint8>& EncodedPakEntries, const TArray<FPakEntry>& Files, const FPakInfo& Info);

	/**
	 * Given a directory index, remove entries from it that are directed by ini to not have filenames kept at runtime.
	 *
	 * InOutDirectoryIndex - The full index from which to potentially remove entries
	 * OutDirectoryIndex - If null, InOutDirectoryIndex will have pruned entries removed.  If non-null, InOutDirectoryIndex will not be modified, and PrunedDirectoryIndex will have kept values added.
	 * MountPoint The mount point for the pak containing the index, used to provide the fullpath for filenames in the DirectoryIndex for comparison against paths in ini
	 */
	static PAKFILE_API void PruneDirectoryIndex(FDirectoryIndex& InOutDirectoryIndex, FDirectoryIndex* PrunedDirectoryIndex, const FString& MountPoint);

	/* Helper function to modify the given string to append '/' at the end of path to normalize directory names for hash and string compares */
	static void MakeDirectoryFromPath(FString& Path)
	{
		if (Path.Len() > 0 && Path[Path.Len() - 1] != '/')
		{
			Path += TEXT("/");
		}
	}
	/* Helper function to check that the given string is in our directory format (ends with '/') */
	static bool IsPathInDirectoryFormat(const FString& Path)
	{
		return Path.Len() > 0 && Path[Path.Len() - 1] == TEXT('/');
	}

	/* Helper function to join two path strings that are in the PakPath format */
	static FString PakPathCombine(const FString& Parent, const FString& Child)
	{
		// Our paths are different than FPaths, because our dirs / at the end, and "/" is the relative path to the mountdirectory and should be mapped to the empty string when joining
		check(Parent.Len() > 0 && Parent[Parent.Len() - 1] == TEXT('/'));
		if (Parent.Len() == 1)
		{
			return Child;
		}
		else if (Child.Len() == 1 && Child[0] == TEXT('/'))
		{
			return Parent;
		}
		else
		{
			check(Child.Len() == 0 || Child[0] != TEXT('/'));
			return Parent + Child;
		}
	}

	/** Helper function to split a PakDirectoryIndex-Formatted PathName into its PakDirectoryIndex-Formatted parent directory and the CleanFileName */
	static bool SplitPathInline(FString& InOutPath, FString& OutFilename)
	{
		// FPaths::GetPath doesn't handle our / at the end of directories, so we have to do string manipulation ourselves
		// The manipulation is less complicated than GetPath deals with, since we have normalized/path/strings, we have relative paths only, and we don't care about extensions
		if (InOutPath.Len() == 0)
		{
			check(false); // Filenames should have non-zero length, and the minimum directory length is 1 (The root directory is written as "/")
			return false;
		}
		else if (InOutPath.Len() == 1)
		{
			if (InOutPath[0] == TEXT('/'))
			{
				// The root directory; it has no parent.
				OutFilename.Empty();
				return false;
			}
			else
			{
				// A relative one-character path with no /; this is a direct child of in the root directory
				OutFilename = TEXT("/");
				Swap(OutFilename, InOutPath);
				return true;
			}
		}
		else
		{
			if (InOutPath[InOutPath.Len() - 1] == TEXT('/'))
			{
				// The input was a Directory; remove the trailing / since we don't keep those on the CleanFilename
				InOutPath.LeftChopInline(1, EAllowShrinking::No);
			}

			int32 Offset = 0;
			if (InOutPath.FindLastChar(TEXT('/'), Offset))
			{
				int32 FilenameStart = Offset + 1;
				OutFilename = InOutPath.Mid(FilenameStart);
				InOutPath.LeftInline(FilenameStart, EAllowShrinking::No); // The Parent Directory keeps the / at the end
			}
			else
			{
				// A relative path with no /; this is a direct child of in the root directory
				OutFilename = TEXT("/");
				Swap(OutFilename, InOutPath);
			}
			return true;
		}
	}

	/**
	 * Helper function to return Child's relative path from the mount point.  Returns false if Child is not equal to MountPoint and is not a child path under MountPoint, else returns true.
	 * Edits Child only if returning true, setting it to the relative path.
	 * If child equals MountPoint, returns true and sets Child to the relative path to the MountPoint, which is "/"
	 */
	static bool GetRelativePathFromMountInline(FString& Child, const FString& MountPoint)
	{
		if (!Child.StartsWith(MountPoint))
		{
			return false;
		}
		Child = Child.Mid(MountPoint.Len());
		if (Child.IsEmpty())
		{
			// Child equals the MountPoint
			Child = TEXT("/");
		}
		return true;
	}

	/**
	 * Helper function to return Filename's relative path from the mount point. Returns null if Child is not equal to MountPoint and is not a child path under MountPoint, else returns
	 * pointer to the offset within Child after the MountPoint.
	 * If child equals MountPoint, returns null; The MountPoint itself is not a valid Filename, since Filenames must have non-zero length and are added on to the MountPoint.
	 */
	static const TCHAR* GetRelativeFilePathFromMountPointer(const FString& Child, const FString& MountPoint)
	{
		if (!Child.StartsWith(MountPoint))
		{
			return nullptr;
		}
		const TCHAR* RelativePathFromMount = (*Child) + MountPoint.Len();
		if (RelativePathFromMount[0] == TEXT('\0'))
		{
			// Child is equal to MountPoint, invalid
			return nullptr;
		}
		return RelativePathFromMount;
	}

	/* Returns the global,const flag for whether the current process is allowing PakFiles to keep their entire DirectoryIndex (if it exists in the PakFile on disk) rather than pruning it */
	static PAKFILE_API bool IsPakKeepFullDirectory();

	/* Returns the global,const flag for whether UnrealPak should write a copy of the full PathHashIndex and Pruned DirectoryIndex to the PakFile */
	static PAKFILE_API bool IsPakWritePathHashIndex();

	/* Returns the global,const flag for whether UnrealPak should write a copy of the full DirectoryIndex to the PakFile */
	static PAKFILE_API bool IsPakWriteFullDirectoryIndex();

private:

	/**
	 * Initializes the pak file.
	 */
	PAKFILE_API void Initialize(FArchive& Reader, bool bLoadIndex = true);

	/**
	 * Loads and initializes pak file index.
	 */
	PAKFILE_API void LoadIndex(FArchive& Reader);

	/**
	  * Returns the FPakEntry pointed to by the given FPakEntryLocation, forwards to the static GetPakEntry with data from *this
	  */
	PAKFILE_API EFindResult GetPakEntry(const FPakEntryLocation& FPakEntryLocation, FPakEntry* OutEntry) const;

	/** Helper class to read IndexSettings from project delegate and commandline */
	struct FIndexSettings;

	static PAKFILE_API FIndexSettings& GetIndexSettings();

	/**
	  * Returns the global,const flag for whether the current process should run directory queries on both the DirectoryIndex and the Pruned DirectoryIndex and log an error if they don't match.
	  * Validation only occurs until the first call to OptimizeMemoryUsageForMountedPaks, after which the Full DirectoryIndex is dropped and there is nothing left to Validate
	  * Has the same effect as IsPakDelayPruning, plus the addition of the error for any mismatches.
	  */
	static PAKFILE_API bool IsPakValidatePruning();
	/**
	 * Returns the global,const flag for whether the current process should keep a copy of the Full DirectoryIndex around until OptimizeMemoryUsageForMountedPaks is called, so that systems can run
	 * directory queries against the full index until then.
	 * Note that validation will still occur if IsPakValidatePruning is true.
	 */
	static PAKFILE_API bool IsPakDelayPruning();

#if ENABLE_PAKFILE_RUNTIME_PRUNING
	/** Global flag for whether a Pak has indicated it needs Pruning */
	static PAKFILE_API bool bSomePakNeedsPruning;
#endif

	/**
	  * Returns whether read accesses against the DirectoryIndex need to be guarded using this->DirectoryIndexLock.
	  * Locking is not required if the pak is not going to be pruned or already has been; the DirectoryIndex is immutable after that point, and we can get a performance benefit by skipping the lock.
	  */
	PAKFILE_API bool RequiresDirectoryIndexLock() const;

	/**
	 * Returns whether the current Process IsPakValidatePruning and this PakFile has a Full DirectoryIndex and Pruned DirectoryIndex to validate.
	 */
	PAKFILE_API bool ShouldValidatePrunedDirectory() const;

	/**
	  * Add the given (Filename,FPakEntryLocation) value into the provided indexes
	  *
	  * @param Filename The filename to add 
	  * @param EntryLocation The FPakEntryLocation to add
	  * @param MountPoint The mount point of the pakfile containing the FPakEntryLocation, used to create the filename in the DirectoryIndex
	  * @param PathHashSeed the pakfile-specific seed for the hash of the path in the PathHasIndex
	  * @param DirectoryIndex Optional FDirectoryIndex into which to insert the (Filename, FPakEntryLocation)
	  * @param PathHashIndex Optional FPathHashIndex into which to insert the (Filename, FPakEntryLocation)
	  * @param InOutCollisionDetection Optional parameter to detect hash collisions.  If present, the hashed filename will be check()'d for a collision against a different filename in InOutCollisionDetection, and will be added into InOutCollisionDetection
	  * @param PakFileVersion Version of the pakfile containing the index, to support legacy formats
	  */
	static PAKFILE_API void AddEntryToIndex(const FString& Filename, const FPakEntryLocation& EntryLocation, const FString& MountPoint, uint64 PathHashSeed,
		FDirectoryIndex* DirectoryIndex, FPathHashIndex* PathHashIndex, TMap<uint64, FString>* CollisionDetection, int32 PakFileVersion);

	/* Encodes a pak entry as an array of bytes into the given archive.  Returns true if encoding succeeded.  If encoding did not succeed, caller will need to store the InPakEntry in an unencoded list */
	static PAKFILE_API bool EncodePakEntry(FArchive& Ar, const FPakEntry& InPakEntry, const FPakInfo& InInfo);

	/* Decodes a bit-encoded pak entry from a pointer to the start of its encoded bytes into the given OutEntry */
	static PAKFILE_API void DecodePakEntry(const uint8* SourcePtr, FPakEntry& OutEntry, const FPakInfo& InInfo);

	/* Internal index loading function that returns false if index loading fails due to an intermittent IO error. Allows LoadIndex to retry or throw a fatal as required */
	PAKFILE_API bool LoadIndexInternal(FArchive& Reader);

	/* Legacy index loading function for PakFiles saved before FPakInfo::PakFile_Version_PathHashIndex */
	PAKFILE_API bool LoadLegacyIndex(FArchive& Reader);

	/* Helper function for LoadIndexInternal; each array of Index bytes read from the file needs to be independently decrypted and checked for corruption */
	PAKFILE_API bool DecryptAndValidateIndex(FArchive& Reader, TArray<uint8>& IndexData, FSHAHash& InExpectedHash, FSHAHash& OutActualHash);

	/* Manually add a file to a pak file */
	PAKFILE_API void AddSpecialFile(const FPakEntry& Entry, const FString& Filename);

	template <typename ShouldVisitFunc, class ContainerType>
	void FindPrunedFilesAtPathInternal(const TCHAR* InPath, const ShouldVisitFunc& ShouldVisit, ContainerType& OutFiles,
		bool bIncludeFiles = true, bool bIncludeDirectories = false, bool bRecursive = false) const;

	/**
	 * Search the given FDirectoryIndex for all files under the given Directory.  Helper for FindFilesAtPath, called separately on the DirectoryIndex or Pruned DirectoryIndex. Does not use
	 * FScopedPakDirectoryIndexAccess internally; caller is responsible for calling from within a lock.
	 * Returned paths are full paths (include the mount point)
	 */
	template <typename ShouldVisitFunc, class ContainerType>
	void FindFilesAtPathInIndex(const FDirectoryIndex& TargetIndex, ContainerType& OutFiles, const FString& Directory,
		const ShouldVisitFunc& ShouldVisit, bool bIncludeFiles = true, bool bIncludeDirectories = false,
		bool bRecursive = false) const;

	/** Converts the path to a RelativePathFromMount and normalizes it to the expected format for Pak Directories.  Returns false if Path is not under the MountDir and hence can not be in this PakFile. */
	PAKFILE_API bool NormalizeDirectoryQuery(const TCHAR* InPath, FString& OutRelativePathFromMount) const;

	/**
	 * Looks up the given Normalized RelativePath in the Pruned DirectoryIndex and returns the directory if found.
	 * Validates the result if IsPakValidatePruning. Does not use a critical section; caller is responsible for calling from within an FScopedPakDirectoryIndexAccess
	 */
	PAKFILE_API const FPakDirectory* FindPrunedDirectoryInternal(const FString& RelativePathFromMount) const;

#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
	/* Logs an error if the two sets are not identical after removing all config-specified ignore paths */
	PAKFILE_API void ValidateDirectorySearch(const TSet<FString>& FoundFullFiles, const TSet<FString>& PrunedFoundFiles, const TCHAR* InPath) const;
#endif
};

/**
 * Placeholder Class
 */
class FPakNoEncryption
{
public:
	enum 
	{
		Alignment = 1,
	};

	static FORCEINLINE int64 AlignReadRequest(int64 Size) 
	{
		return Size;
	}

	static FORCEINLINE void DecryptBlock(void* Data, int64 Size, const FGuid& EncryptionKeyGuid)
	{
		// Nothing needs to be done here
	}
};

/**
 * Typedef for a function that returns an archive to use for accessing an underlying pak file
 */
typedef TFunction<FSharedPakReader()> TAcquirePakReaderFunction;

template< typename EncryptionPolicy = FPakNoEncryption >
class FPakReaderPolicy
{
public:
	/** Pak file that own this file data */
	const FPakFile&		PakFile;
	/** Pak file entry for this file. */
	FPakEntry			PakEntry;
	/** Pak file archive to read the data from. */
	TAcquirePakReaderFunction AcquirePakReader;
	/** Offset to the file in pak (including the file header). */
	int64				OffsetToFile;

	FPakReaderPolicy(const FPakFile& InPakFile,const FPakEntry& InPakEntry, TAcquirePakReaderFunction& InAcquirePakReader)
		: PakFile(InPakFile)
		, PakEntry(InPakEntry)
		, AcquirePakReader(InAcquirePakReader)
	{
		OffsetToFile = PakEntry.Offset + PakEntry.GetSerializedSize(PakFile.GetInfo().Version);
	}

	FORCEINLINE int64 FileSize() const 
	{
		return PakEntry.Size;
	}

	void Serialize(int64 DesiredPosition, void* V, int64 Length)
	{
		FGuid EncryptionKeyGuid = PakFile.GetInfo().EncryptionKeyGuid;
		const constexpr int64 Alignment = (int64)EncryptionPolicy::Alignment;
		const constexpr int64 AlignmentMask = ~(Alignment - 1);
		uint8 TempBuffer[Alignment];
		FSharedPakReader PakReader = AcquirePakReader();
		if (EncryptionPolicy::AlignReadRequest(DesiredPosition) != DesiredPosition)
		{
			int64 Start = DesiredPosition & AlignmentMask;
			int64 Offset = DesiredPosition - Start;
			int64 CopySize = FMath::Min(Alignment - Offset, Length);
			PakReader->Seek(OffsetToFile + Start);
			PakReader->Serialize(TempBuffer, Alignment);
			EncryptionPolicy::DecryptBlock(TempBuffer, Alignment, EncryptionKeyGuid);
			FMemory::Memcpy(V, TempBuffer + Offset, CopySize);
			V = (void*)((uint8*)V + CopySize);
			DesiredPosition += CopySize;
			Length -= CopySize;
			check(Length == 0 || DesiredPosition % Alignment == 0);
		}
		else
		{
			PakReader->Seek(OffsetToFile + DesiredPosition);
		}
		
		int64 CopySize = Length & AlignmentMask;
		PakReader->Serialize(V, CopySize);
		EncryptionPolicy::DecryptBlock(V, CopySize, EncryptionKeyGuid);
		Length -= CopySize;
		V = (void*)((uint8*)V + CopySize);

		if (Length > 0)
		{
			PakReader->Serialize(TempBuffer, Alignment);
			EncryptionPolicy::DecryptBlock(TempBuffer, Alignment, EncryptionKeyGuid);
			FMemory::Memcpy(V, TempBuffer, Length);
		}
	}
};

/**
 * File handle to read from pak file.
 */
template< typename ReaderPolicy = FPakReaderPolicy<> >
class FPakFileHandle : public IFileHandle
{
	/** Current read position. */
	int64 ReadPos;
	/** Class that controls reading from pak file */
	ReaderPolicy Reader;
	/** Reference to keep the PakFile referenced until we are destroyed */
	TRefCountPtr<const FPakFile> PakFile;

public:

	/**
	 * Constructs pak file handle to read from pak.
	 *
	 * @param InFilename Filename
	 * @param InPakEntry Entry in the pak file.
	 * @param InAcquirePakReaderFunction Function that returns the archive to use for serialization. The result of this should not be cached, but reacquired on each serialization operation
	 */
	FPakFileHandle(const TRefCountPtr<const FPakFile>& InPakFile, const FPakEntry& InPakEntry, TAcquirePakReaderFunction& InAcquirePakReaderFunction)
		: ReadPos(0)
		, Reader(*InPakFile, InPakEntry, InAcquirePakReaderFunction)
		, PakFile(InPakFile)
	{
		INC_DWORD_STAT(STAT_PakFile_NumOpenHandles);
	}

	/**
	 * Constructs pak file handle to read from pak.
	 *
	 * @param InFilename Filename
	 * @param InPakEntry Entry in the pak file.
	 * @param InPakFile Pak file.
	 */
	FPakFileHandle(const TRefCountPtr<const FPakFile>& InPakFile, const FPakEntry& InPakEntry, FArchive* InPakReader)
		: ReadPos(0)
		, Reader(*InPakFile, InPakEntry, InPakReader)
		, PakFile(InPakFile)
	{
		INC_DWORD_STAT(STAT_PakFile_NumOpenHandles);
	}

	/**
	 * Destructor. Cleans up the reader archive if necessary.
	 */
	virtual ~FPakFileHandle()
	{
		DEC_DWORD_STAT(STAT_PakFile_NumOpenHandles);
	}

	//~ Begin IFileHandle Interface
	virtual int64 Tell() override
	{
		return ReadPos;
	}
	virtual bool Seek(int64 NewPosition) override
	{
		if (NewPosition > Reader.FileSize() || NewPosition < 0)
		{
			return false;
		}
		ReadPos = NewPosition;
		return true;
	}
	virtual bool SeekFromEnd(int64 NewPositionRelativeToEnd) override
	{
		return Seek(Reader.FileSize() - NewPositionRelativeToEnd);
	}
	virtual bool Read(uint8* Destination, int64 BytesToRead) override
	{
		SCOPE_SECONDS_ACCUMULATOR(STAT_PakFile_Read);

		// Check that the file header is OK
		if (!Reader.PakEntry.Verified)
		{
			FPakEntry FileHeader;
			FSharedPakReader PakReader = Reader.AcquirePakReader();
			PakReader->Seek(Reader.PakEntry.Offset);
			FileHeader.Serialize(PakReader.GetArchive(), Reader.PakFile.GetInfo().Version);
			if (FPakEntry::VerifyPakEntriesMatch(Reader.PakEntry, FileHeader))
			{
				Reader.PakEntry.Verified = true;
			}
			else
			{
				//Header is corrupt, fail the read
				return false;
			}
		}
		//
		if (Reader.FileSize() >= (ReadPos + BytesToRead))
		{
			// Read directly from Pak.
			Reader.Serialize(ReadPos, Destination, BytesToRead);
			ReadPos += BytesToRead;
			return true;
		}
		else
		{
			return false;
		}
	}
	virtual bool Write(const uint8* Source, int64 BytesToWrite) override
	{
		// Writing in pak files is not allowed.
		return false;
	}
	virtual int64 Size() override
	{
		return Reader.FileSize();
	}
	virtual bool Flush(const bool bFullFlush = false) override
	{
		// pak files are read only, so don't need to support flushing
		return false;
	}
	virtual bool Truncate(int64 NewSize) override
	{
		// pak files are read only, so don't need to support truncation
		return false;
	}
	///~ End IFileHandle Interface
};

/**
 * Platform file wrapper to be able to use pak files.
 **/
class FPakPlatformFile : public IPlatformFile
{
	struct FPakListEntry
	{
		FPakListEntry()
			: ReadOrder(0)
			, PakFile(nullptr)
		{}

		uint32					ReadOrder;
		TRefCountPtr<FPakFile>	PakFile;

		FORCEINLINE bool operator < (const FPakListEntry& RHS) const
		{
			return ReadOrder > RHS.ReadOrder;
		}
	};

	struct FPakListDeferredEntry
	{
		FString Filename;
		FString Path;
		uint32 ReadOrder;
		FGuid EncryptionKeyGuid;
		int32 PakchunkIndex;
	};
	
	/** Wrapped file */
	IPlatformFile* LowerLevel;
	/** List of all available pak files. */
	TArray<FPakListEntry> PakFiles;
	/** List of all pak filenames with dynamic encryption where we don't have the key yet */
	TArray<FPakListDeferredEntry> PendingEncryptedPakFiles;
	/** True if this we're using signed content. */
	bool bSigned;
	/** Synchronization object for accessing the list of currently mounted pak files. */
	mutable FCriticalSection PakListCritical;
	/** Cache of extensions that we automatically reject if not found in pak file */
	TSet<FName> ExcludedNonPakExtensions;
	/** The extension used for ini files, used for excluding ini files */
	FString IniFileExtension;
	/** The filename for the gameusersettings ini file, used for excluding ini files, but not gameusersettings */
	FString GameUserSettingsIniFilename;
	TSharedPtr<FFileIoStore> IoDispatcherFileBackend;
	TSharedPtr<FFilePackageStoreBackend> PackageStoreBackend;

	FTSTicker::FDelegateHandle RetireReadersHandle;

#if !UE_BUILD_SHIPPING
	// if true (via -looklocalfirst) then loose/non-ufs files will be looked for before looking in the .pak file
	// this respects IsNonPakFilenameAllowed()
	bool bLookLooseFirst = false;
#endif

	/**
	 * Gets mounted pak files
	 */
	FORCEINLINE void GetMountedPaks(TArray<FPakListEntry>& Paks)
	{
		FScopeLock ScopedLock(&PakListCritical);
		Paks.Append(PakFiles);
	}

	/**
	 * Checks if a directory exists in one of the available pak files.
	 *
	 * @param Directory Directory to look for.
	 * @return true if the directory exists, false otherwise.
	 */
	PAKFILE_API bool DirectoryExistsInPrunedPakFiles(const TCHAR* Directory);

	/**
	 * Helper function to copy a file from one handle to another usuing the supplied buffer.
	 *
	 * @param Dest Destination file handle.
	 * @param Source file handle.
	 * @param FileSize size of the source file.
	 * @param Buffer Pointer to the buffer used to copy data.
	 * @param BufferSize Sizeof of the buffer.
	 * @return true if the operation was successfull, false otherwise.
	 */
	PAKFILE_API bool BufferedCopyFile(IFileHandle& Dest, IFileHandle& Source, const int64 FileSize, uint8* Buffer, const int64 BufferSize) const;

	/**
	 * Creates file handle to read from Pak file.
	 *
	 * @param Filename Filename to create the handle for.
	 * @param PakFile Pak file to read from.
	 * @param FileEntry File entry to create the handle for.
	 * @return Pointer to the new handle.
	 */
	PAKFILE_API IFileHandle* CreatePakFileHandle(const TCHAR* Filename, const TRefCountPtr<FPakFile>& PakFile, const FPakEntry* FileEntry);

	/**
	* Hardcode default load ordering of game main pak -> game content -> engine content -> saved dir
	* would be better to make this config but not even the config system is initialized here so we can't do that
	*/
	PAKFILE_API static int32 GetPakOrderFromPakFilePath(const FString& PakFilePath);

	/**
	 * Handler for device delegate to prompt us to load a new pak.	 
	 */
	PAKFILE_API IPakFile* HandleMountPakDelegate(const FString& PakFilePath, int32 PakOrder);

	/**
	 * Handler for device delegate to prompt us to unload a pak.
	 */
	PAKFILE_API bool HandleUnmountPakDelegate(const FString& PakFilePath);

	/**
	 * Finds all pak files in the given directory.
	 *
	 * @param Directory Directory to (recursively) look for pak files in
	 * @param OutPakFiles List of pak files
	 */
	PAKFILE_API static void FindPakFilesInDirectory(IPlatformFile* LowLevelFile, const TCHAR* Directory, const FString& WildCard, TArray<FString>& OutPakFiles);

	/**
	 * Finds all pak files in the known pak folders
	 *
	 * @param OutPakFiles List of all found pak files
	 */
	PAKFILE_API static void FindAllPakFiles(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders, const FString& WildCard, TArray<FString>& OutPakFiles);

	/**
	 * When security is enabled, determine if this filename can be looked for in the lower level file system
	 * 
	 * @param InFilename			Filename to check
	 * @param bAllowDirectories		Consider directories as valid filepaths?
	 */
	PAKFILE_API bool IsNonPakFilenameAllowed(const FString& InFilename);

	/**
	 * Registers a new AES key with the given guid. Triggers the mounting of any pak files that we encountered that use that key
	 *
	 * @param InEncryptionKeyGuid	Guid for this encryption key
	 * @param InKey					Encryption key
	 */
	PAKFILE_API void RegisterEncryptionKey(const FGuid& InEncryptionKeyGuid, const FAES::FAESKey& InKey);

	/**
	 * Checks with any current chunk installation system if the given pak file is installed
	 * 
	 * @param InFilename  the pak filename to check
	 * @return whether the pak file is installed
	 */
	PAKFILE_API static bool IsPakFileInstalled(const FString& InFilename);

public:

	//~ For visibility of overloads we don't override
	using IPlatformFile::IterateDirectory;
	using IPlatformFile::IterateDirectoryRecursively;
	using IPlatformFile::IterateDirectoryStat;
	using IPlatformFile::IterateDirectoryStatRecursively;

	/**
	 * Get the unique name for the pak platform file layer
	 */
	static const TCHAR* GetTypeName()
	{
		return TEXT("PakFile");
	}

	/**
	 * Get the wild card pattern used to identify paks to load on startup
	 */
	PAKFILE_API static const TCHAR* GetMountStartupPaksWildCard();

	/**
	 * Overrides the wildcard used for searching paks. Call before initialization
	 */
	PAKFILE_API static void SetMountStartupPaksWildCard(const FString& WildCard);

	/**
	* Determine location information for a given pakchunk index. Will be DoesNotExist if the pak file wasn't detected, NotAvailable if it exists but hasn't been mounted due to a missing encryption key, or LocalFast if it exists and has been mounted
	*/
	PAKFILE_API EChunkLocation::Type GetPakChunkLocation(int32 InPakchunkIndex) const;

	/**
	* Returns true if any of the mounted or pending pak files are chunks (filenames starting pakchunkN)
	*/
	PAKFILE_API bool AnyChunksAvailable() const;

	/**
	* Get a list of all pak files which have been successfully mounted
	*/
	FORCEINLINE void GetMountedPakFilenames(TArray<FString>& PakFilenames)
	{
		FScopeLock ScopedLock(&PakListCritical);
		PakFilenames.Empty(PakFiles.Num());
		for (FPakListEntry& Entry : PakFiles)
		{
			PakFilenames.Add(Entry.PakFile->GetFilename());
		}
	}

	/**
	* Get a list of all pak files which have been successfully mounted
	*/
	FORCEINLINE void GetMountedPakFilenames(TSet<FString>& PakFilenames)
	{
		FScopeLock ScopedLock(&PakListCritical);
		PakFilenames.Empty(PakFiles.Num());
		for (FPakListEntry& Entry : PakFiles)
		{
			PakFilenames.Add(Entry.PakFile->GetFilename());
		}
	}

	/**
	 * Checks if pak files exist in any of the known pak file locations.
	 */
	PAKFILE_API static bool CheckIfPakFilesExist(IPlatformFile* LowLevelFile, const TArray<FString>& PakFolders);

	/**
	 * Gets all pak file locations.
	 */
	PAKFILE_API static void GetPakFolders(const TCHAR* CmdLine, TArray<FString>& OutPakFolders);

	/**
	* Helper function for accessing pak encryption key
	*/
	PAKFILE_API static void GetPakEncryptionKey(FAES::FAESKey& OutKey, const FGuid& InEncryptionKeyGuid);

	/**
	* Load a pak signature file. Validates the contents by comparing a SHA hash of the chunk table against and encrypted version that
	* is stored within the file. Returns nullptr if the data is missing or fails the signature check. This function also calls
	* the generic pak signature failure delegates if anything is wrong.
	*/
	PAKFILE_API static TSharedPtr<const struct FPakSignatureFile, ESPMode::ThreadSafe> GetPakSignatureFile(const TCHAR* InFilename);

	/**
	 * Remove the intenrally cached pointer to the signature file for the specified pak
	 */
	PAKFILE_API static void RemoveCachedPakSignaturesFile(const TCHAR* InFilename);

	/**
	 * Constructor.
	 * 
	 * @param InLowerLevel Wrapper platform file.
	 */
	PAKFILE_API FPakPlatformFile();

	/**
	 * Destructor.
	 */
	PAKFILE_API virtual ~FPakPlatformFile();

	PAKFILE_API virtual bool ShouldBeUsed(IPlatformFile* Inner, const TCHAR* CmdLine) const override;
	PAKFILE_API virtual bool Initialize(IPlatformFile* Inner, const TCHAR* CommandLineParam) override;
	PAKFILE_API virtual void InitializeNewAsyncIO() override;

	PAKFILE_API void OptimizeMemoryUsageForMountedPaks();

	virtual IPlatformFile* GetLowerLevel() override
	{
		return LowerLevel;
	}
	virtual void SetLowerLevel(IPlatformFile* NewLowerLevel) override
	{
		LowerLevel = NewLowerLevel;
	}

	virtual const TCHAR* GetName() const override
	{
		return FPakPlatformFile::GetTypeName();
	}

	PAKFILE_API void Tick() override;

	/**
	 * Mounts a pak file at the specified path.
	 *
	 * @param InPakFilename Pak filename.
	 * @param InPath Path to mount the pak at.
	 */
	PAKFILE_API bool Mount(const TCHAR* InPakFilename, uint32 PakOrder, const TCHAR* InPath = nullptr, bool bLoadIndex = true, FPakListEntry* OutPakListEntry = nullptr);

	PAKFILE_API bool Unmount(const TCHAR* InPakFilename);

	PAKFILE_API int32 MountAllPakFiles(const TArray<FString>& PakFolders);
	PAKFILE_API int32 MountAllPakFiles(const TArray<FString>& PakFolders, const FString& WildCard);

	/**
	 * Re-creates all the pak readers
	 */
	PAKFILE_API bool ReloadPakReaders();

	/**
	 * Make unique in memory pak files from a list of named files
	 */
	PAKFILE_API virtual void MakeUniquePakFilesForTheseFiles(const TArray<TArray<FString>>& InFiles);

	/**
	 * Finds a file in the specified pak files.
	 *
	 * @param Paks Pak files to find the file in.
	 * @param Filename File to find in pak files.
	 * @param OutPakFile Optional pointer to a pak file where the filename was found.
	 * @return Pointer to pak entry if the file was found, NULL otherwise.
	 */
	PAKFILE_API static bool FindFileInPakFiles(TArray<FPakListEntry>& Paks, const TCHAR* Filename,
		TRefCountPtr<FPakFile>* OutPakFile, FPakEntry* OutEntry = nullptr);

	/**
	 * Finds a file in all available pak files.
	 *
	 * @param Filename File to find in pak files.
	 * @param OutPakFile Optional pointer to a pak file where the filename was found.
	 * @return Pointer to pak entry if the file was found, NULL otherwise.
	 */
	bool FindFileInPakFiles(const TCHAR* Filename, TRefCountPtr<FPakFile>* OutPakFile = nullptr,
		FPakEntry* OutEntry = nullptr);

	//~ Begin IPlatformFile Interface
	virtual bool FileExists(const TCHAR* Filename) override
	{
		// Check pak files first.
		if (FindFileInPakFiles(Filename))
		{
			return true;
		}
		// File has not been found in any of the pak files, continue looking in inner platform file.
		bool Result = false;
		if (IsNonPakFilenameAllowed(Filename))
		{
			Result = LowerLevel->FileExists(Filename);
		}
		return Result;
	}

	virtual int64 FileSize(const TCHAR* Filename) override
	{
		// Check pak files first
		FPakEntry FileEntry;
		if (FindFileInPakFiles(Filename, nullptr, &FileEntry))
		{
			return FileEntry.CompressionMethodIndex != 0 ? FileEntry.UncompressedSize : FileEntry.Size;
		}
		// First look for the file in the user dir.
		int64 Result = INDEX_NONE;
		if (IsNonPakFilenameAllowed(Filename))
		{
			Result = LowerLevel->FileSize(Filename);
		}
		return Result;
	}

	virtual bool DeleteFile(const TCHAR* Filename) override
	{
		// If file exists in pak file it will never get deleted.
		if (FindFileInPakFiles(Filename))
		{
			return false;
		}
		// The file does not exist in pak files, try LowerLevel->
		bool Result = false;
		if (IsNonPakFilenameAllowed(Filename))
		{
			Result = LowerLevel->DeleteFile(Filename);
		}
		return Result;
	}

	virtual bool IsReadOnly(const TCHAR* Filename) override
	{
		// Files in pak file are always read-only.
		if (FindFileInPakFiles(Filename))
		{
			return true;
		}
		// The file does not exist in pak files, try LowerLevel->
		bool Result = false;
		if (IsNonPakFilenameAllowed(Filename))
		{
			Result = LowerLevel->IsReadOnly(Filename);
		}
		return Result;
	}

	virtual bool MoveFile(const TCHAR* To, const TCHAR* From) override
	{
		// Files which exist in pak files can't be moved
		if (FindFileInPakFiles(From))
		{
			return false;
		}
		// Files not in pak are allowed to be moved.
		bool Result = false;
		if (IsNonPakFilenameAllowed(From))
		{
			Result = LowerLevel->MoveFile(To, From);
		}
		return Result;
	}

	virtual bool SetReadOnly(const TCHAR* Filename, bool bNewReadOnlyValue) override
	{
		// Files in pak file will never change their read-only flag.
		if (FindFileInPakFiles(Filename))
		{
			// This fails if soemone wants to make files from pak writable.
			return bNewReadOnlyValue;
		}
		// Try lower level
		bool Result = bNewReadOnlyValue;
		if (IsNonPakFilenameAllowed(Filename))
		{
			Result = LowerLevel->SetReadOnly(Filename, bNewReadOnlyValue);
		}
		return Result;
	}

	virtual FDateTime GetTimeStamp(const TCHAR* Filename) override
	{
		// Check pak files first.
		TRefCountPtr<FPakFile> PakFile = NULL;
		if (FindFileInPakFiles(Filename, &PakFile))
		{
			return PakFile->GetTimestamp();
		}
		// Fall back to lower level.
		FDateTime Result = FDateTime::MinValue();
		if (IsNonPakFilenameAllowed(Filename))
		{
			double StartTime = (UE_LOG_ACTIVE(LogPakFile, Verbose)) ? FPlatformTime::Seconds() : 0.0;
			Result = LowerLevel->GetTimeStamp(Filename);
			UE_LOG(LogPakFile, Verbose, TEXT("GetTimeStamp on disk (!!) for %s took %6.2fms."), Filename, float(FPlatformTime::Seconds() - StartTime) * 1000.0f);
		}
		return Result;
	}

	virtual void GetTimeStampPair(const TCHAR* FilenameA, const TCHAR* FilenameB, FDateTime& OutTimeStampA, FDateTime& OutTimeStampB) override
	{
		TRefCountPtr<FPakFile> PakFileA;
		TRefCountPtr<FPakFile> PakFileB;
		FindFileInPakFiles(FilenameA, &PakFileA);
		FindFileInPakFiles(FilenameB, &PakFileB);

		// If either file exists, we'll assume both should exist here and therefore we can skip the
		// request to the lower level platform file.
		if (PakFileA != nullptr || PakFileB != nullptr)
		{
			OutTimeStampA = PakFileA != nullptr ? PakFileA->GetTimestamp() : FDateTime::MinValue();
			OutTimeStampB = PakFileB != nullptr ? PakFileB->GetTimestamp() : FDateTime::MinValue();
		}
		else
		{
			// Fall back to lower level.
			if (IsNonPakFilenameAllowed(FilenameA) && IsNonPakFilenameAllowed(FilenameB))
			{
				LowerLevel->GetTimeStampPair(FilenameA, FilenameB, OutTimeStampA, OutTimeStampB);
			}
			else
			{
				OutTimeStampA = FDateTime::MinValue();
				OutTimeStampB = FDateTime::MinValue();
			}
		}
	}

	virtual void SetTimeStamp(const TCHAR* Filename, FDateTime DateTime) override
	{
		// No modifications allowed on files from pak (although we could theoretically allow this one).
		if (!FindFileInPakFiles(Filename))
		{
			if (IsNonPakFilenameAllowed(Filename))
			{
				LowerLevel->SetTimeStamp(Filename, DateTime);
			}
		}
	}

	virtual FDateTime GetAccessTimeStamp(const TCHAR* Filename) override
	{
		// AccessTimestamp not yet supported in pak files (although it is possible).
		TRefCountPtr<FPakFile> PakFile;
		if (FindFileInPakFiles(Filename, &PakFile))
		{
			return PakFile->GetTimestamp();
		}
		// Fall back to lower level.
		FDateTime Result = false;
		if (IsNonPakFilenameAllowed(Filename))
		{
			Result = LowerLevel->GetAccessTimeStamp(Filename);
		}
		return Result;
	}

	virtual FString GetFilenameOnDisk(const TCHAR* Filename) override
	{
		FPakEntry FileEntry;
		TRefCountPtr<FPakFile> PakFile;
		if (FindFileInPakFiles(Filename, &PakFile, &FileEntry))
		{
			const FString Path(FPaths::GetPath(Filename));
			FPakFile::FScopedPakDirectoryIndexAccess ScopeAccess(*PakFile);

			const FPakDirectory* PakDirectory = PakFile->FindPrunedDirectory(*Path);
			if (PakDirectory != nullptr)
			{
				for (FPakDirectory::TConstIterator DirectoryIt(*PakDirectory); DirectoryIt; ++DirectoryIt)
				{
					FPakEntry PakEntry;
					if (PakFile->GetPakEntry(DirectoryIt.Value(), &PakEntry) != FPakFile::EFindResult::NotFound && PakEntry.Offset == FileEntry.Offset)
					{
						const FString& RealFilename = DirectoryIt.Key();
						return Path / RealFilename;
					}
				}
			}

#if ENABLE_PAKFILE_RUNTIME_PRUNING_VALIDATE
			// The File exists in the Pak but has been pruned from its DirectoryIndex; log an error if we are validating pruning and return the original Filename.
			if (PakFile->ShouldValidatePrunedDirectory())
			{
				TSet<FString> FullFoundFiles;
				TSet<FString> PrunedFoundFiles;
				FullFoundFiles.Add(Filename);
				PakFile->ValidateDirectorySearch(FullFoundFiles, PrunedFoundFiles, Filename);
			}
#endif

			return Filename;
		}

		// Fall back to lower level.
		if (IsNonPakFilenameAllowed(Filename))
		{
			return LowerLevel->GetFilenameOnDisk(Filename);
		}
		else
		{
			return Filename;
		}
	}

	virtual ESymlinkResult IsSymlink(const TCHAR* Filename) override
	{
		return LowerLevel->IsSymlink(Filename);
	}

	PAKFILE_API virtual IFileHandle* OpenRead(const TCHAR* Filename, bool bAllowWrite = false) override;
	PAKFILE_API virtual IFileHandle* OpenWrite(const TCHAR* Filename, bool bAppend = false,
		bool bAllowRead = false) override;

	PAKFILE_API virtual bool DirectoryExists(const TCHAR* Directory) override;
	PAKFILE_API virtual bool CreateDirectory(const TCHAR* Directory) override;
	PAKFILE_API virtual bool DeleteDirectory(const TCHAR* Directory) override;
	PAKFILE_API virtual FFileStatData GetStatData(const TCHAR* FilenameOrDirectory) override;
	PAKFILE_API virtual bool IterateDirectory(const TCHAR* Directory,
		IPlatformFile::FDirectoryVisitor& Visitor) override;
	PAKFILE_API virtual bool IterateDirectoryRecursively(const TCHAR* Directory,
		IPlatformFile::FDirectoryVisitor& Visitor) override;
	PAKFILE_API virtual bool IterateDirectoryStat(const TCHAR* Directory,
		IPlatformFile::FDirectoryStatVisitor& Visitor) override;
	PAKFILE_API virtual bool IterateDirectoryStatRecursively(const TCHAR* Directory,
		IPlatformFile::FDirectoryStatVisitor& Visitor) override;
	PAKFILE_API virtual void FindFiles(TArray<FString>& FoundFiles,
		const TCHAR* Directory, const TCHAR* FileExtension) override;
	PAKFILE_API virtual void FindFilesRecursively(TArray<FString>& FoundFiles,
		const TCHAR* Directory, const TCHAR* FileExtension) override;
	PAKFILE_API virtual bool DeleteDirectoryRecursively(const TCHAR* Directory) override;
	PAKFILE_API virtual bool CreateDirectoryTree(const TCHAR* Directory) override;

	PAKFILE_API virtual bool CopyFile(const TCHAR* To, const TCHAR* From, EPlatformFileRead ReadFlags = EPlatformFileRead::None, EPlatformFileWrite WriteFlags = EPlatformFileWrite::None) override;

	PAKFILE_API virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* Filename) override;
	PAKFILE_API virtual void SetAsyncMinimumPriority(EAsyncIOPriorityAndFlags Priority) override;

	virtual IMappedFileHandle* OpenMapped(const TCHAR* Filename) override;
	/**
	 * Converts a filename to a path inside pak file.
	 *
	 * @param Filename Filename to convert.
	 * @param Pak Pak to convert the filename realative to.
	 * @param Relative filename.
	 */
	FString ConvertToPakRelativePath(const TCHAR* Filename, const FPakFile* Pak)
	{
		FString RelativeFilename(Filename);
		return RelativeFilename.Mid(Pak->GetMountPoint().Len());
	}

	FString ConvertToAbsolutePathForExternalAppForRead(const TCHAR* Filename) override
	{
		// Check in Pak file first
		TRefCountPtr<FPakFile> Pak;
		if (FindFileInPakFiles(Filename, &Pak))
		{
			return FString::Printf(TEXT("Pak: %s/%s"), *Pak->GetFilename(), *ConvertToPakRelativePath(Filename, Pak));
		}
		else
		{
			return LowerLevel->ConvertToAbsolutePathForExternalAppForRead(Filename);
		}
	}

	FString ConvertToAbsolutePathForExternalAppForWrite(const TCHAR* Filename) override
	{
		// Check in Pak file first
		TRefCountPtr<FPakFile> Pak;
		if (FindFileInPakFiles(Filename, &Pak))
		{
			return FString::Printf(TEXT("Pak: %s/%s"), *Pak->GetFilename(), *ConvertToPakRelativePath(Filename, Pak));
		}
		else
		{
			return LowerLevel->ConvertToAbsolutePathForExternalAppForWrite(Filename);
		}
	}
	//~ End IPlatformFile Interface

	// Access static delegate for loose file security
	PAKFILE_API static FFilenameSecurityDelegate& GetFilenameSecurityDelegate();

	// Access static delegate for custom encryption
	PAKFILE_API static FPakCustomEncryptionDelegate& GetPakCustomEncryptionDelegate();

	struct FPakSigningFailureHandlerData
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		FCriticalSection& GetLock() { return Lock; }
		FPakChunkSignatureCheckFailedHandler& GetPakChunkSignatureCheckFailedDelegate() { return ChunkSignatureCheckFailedDelegate; }
		FPakPrincipalSignatureTableCheckFailureHandler& GetPrincipalSignatureTableCheckFailedDelegate() { return MasterSignatureTableCheckFailedDelegate; }
		PRAGMA_ENABLE_DEPRECATION_WARNINGS

		UE_DEPRECATED("5.1", "Use GetLock instead")
		FCriticalSection Lock;

		UE_DEPRECATED("5.1", "Use GetPakChunkSignatureCheckFailedDelegate instead")
		FPakChunkSignatureCheckFailedHandler ChunkSignatureCheckFailedDelegate;

		UE_DEPRECATED("5.1", "Use GetPrincipalSignatureTableCheckFailureDelegate instead")
		FPakPrincipalSignatureTableCheckFailureHandler MasterSignatureTableCheckFailedDelegate;
	};

	// Access static delegate for handling a Pak signature check failure
	PAKFILE_API static FPakSigningFailureHandlerData& GetPakSigningFailureHandlerData();
	
	// Broadcast a signature check failure through any registered delegates in a thread safe way
	PAKFILE_API static void BroadcastPakChunkSignatureCheckFailure(const FPakChunkSignatureCheckFailedData& InData);

	// Broadcast a principal signature table failure through any registered delegates in a thread safe way
	PAKFILE_API static void BroadcastPakPrincipalSignatureTableCheckFailure(const FString& InFilename);

	UE_DEPRECATED("5.1", "Use BroadcastPakPrincipalSignatureTableCheckFailure instead")
	PAKFILE_API static void BroadcastPakMasterSignatureTableCheckFailure(const FString& InFilename);

	// Access static delegate for setting PakIndex settings.
	PAKFILE_API static FPakSetIndexSettings& GetPakSetIndexSettingsDelegate();

	/* Get a list of RelativePathFromMount for every file in the given Pak that lives in any of the given chunks.  Only searches the Pruned DirectoryIndex */
	PAKFILE_API void GetPrunedFilenamesInChunk(const FString& InPakFilename, const TArray<int32>& InChunkIDs, TArray<FString>& OutFileList);

	/** Gets a list of FullPaths (includes Mount directory) for every File in the given Pak's Pruned DirectoryIndex */
	PAKFILE_API void GetPrunedFilenamesInPakFile(const FString& InPakFilename, TArray<FString>& OutFileList);

	/** Returns the RelativePathFromMount Filename for every file found in the given Iostore Container */
	PAKFILE_API static void GetFilenamesFromIostoreContainer(const FString& InContainerName, TArray<FString>& OutFileList);

	/** Returns the RelativePathFromMount Filename for every Filename found in the Iostore Container that relates to the provided block indexes */
	PAKFILE_API static void GetFilenamesFromIostoreByBlockIndex(const FString& InContainerName, const TArray<int32>& InBlockIndex, TArray<FString>& OutFileList);

	/** Iterates Iostore Container while Predicate returns true */
	PAKFILE_API static void ForeachPackageInIostoreWhile(TFunctionRef<bool(FName)> Predicate);

	PAKFILE_API void ReleaseOldReaders();

	// BEGIN Console commands
#if !UE_BUILD_SHIPPING
	PAKFILE_API void HandlePakListCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	PAKFILE_API void HandleMountCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	PAKFILE_API void HandleUnmountCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	PAKFILE_API void HandlePakCorruptCommand(const TCHAR* Cmd, FOutputDevice& Ar);
	PAKFILE_API void HandleReloadPakReadersCommand(const TCHAR* Cmd, FOutputDevice& Ar);
#endif
	// END Console commands
	
#if PAK_TRACKER
	PAKFILE_API static TMap<FString, int32> GPakSizeMap;
	PAKFILE_API static void TrackPak(const TCHAR* Filename, const FPakEntry* PakEntry);
	static TMap<FString, int32>& GetPakMap() { return GPakSizeMap; }
#endif

	// Internal cache of pak signature files
	PAKFILE_API static TMap<FName, TSharedPtr<const struct FPakSignatureFile, ESPMode::ThreadSafe>> PakSignatureFileCache;
	PAKFILE_API static FCriticalSection PakSignatureFileCacheLock;

private:
	bool IterateDirectoryInternal(const TCHAR* Directory, IPlatformFile::FDirectoryVisitor& Visitor, bool bRecursive);
	bool IterateDirectoryInPakFiles(const TCHAR* Directory,
		UE::PakFile::Private::FPakFileDirectoryVisitorBase& Visitor, bool bRecursive,
		TSet<FString>& FilesVisitedInPak);
	bool IterateDirectoryStatInternal(const TCHAR* Directory,
		IPlatformFile::FDirectoryStatVisitor& Visitor, bool bRecursive);
	void FindFilesInternal(TArray<FString>& FoundFiles,
		const TCHAR* Directory, const TCHAR* FileExtension, bool bRecursive);
};

/**
 * Structure which describes the content of the pak .sig files
 */
struct FPakSignatureFile
{
	// Magic number that tells us we're dealing with the new format sig files
	static const uint32 Magic = 0x73832DAA;

	enum class EVersion
	{
		Invalid,
		First,

		Last,
		Latest = Last - 1
	};

	// Sig file version. Set to Legacy if the sig file is of an old version
	EVersion Version = EVersion::Latest;

	// RSA encrypted hash
	TArray<uint8> EncryptedHash;

	// SHA1 hash of the chunk CRC data. Only valid after calling DecryptSignatureAndValidate
	FSHAHash DecryptedHash;

	// The actual array of data that was encrypted in the RSA block. Contains the chunk table hash and also other custom data related to the pak file
	TArray<uint8> SignatureData;

	// CRCs of each contiguous 64kb block of the pak file
	TArray<TPakChunkHash> ChunkHashes;
	
	/**
	 * Initialize and hash the CRC list then use the provided private key to encrypt the hash
	 */
	void SetChunkHashesAndSign(const TArray<TPakChunkHash>& InChunkHashes, const TArrayView<uint8>& InSignatureData, const FRSAKeyHandle InKey)
	{
		ChunkHashes = InChunkHashes;
		SignatureData = InSignatureData;
		DecryptedHash = ComputeCurrentPrincipalHash();

		TArray<uint8> NewSignatureData;
		NewSignatureData.Append(SignatureData);
		NewSignatureData.Append(DecryptedHash.Hash, UE_ARRAY_COUNT(FSHAHash::Hash));
		FRSA::EncryptPrivate(NewSignatureData, EncryptedHash, InKey);
	}

	/**
	 * Serialize/deserialize this object to/from an FArchive
	 */
	void Serialize(FArchive& Ar)
	{
		uint32 FileMagic = Magic;
		Ar << FileMagic;

		if (Ar.IsLoading() && FileMagic != Magic)
		{
			Version = EVersion::Invalid;
			EncryptedHash.Empty();
			ChunkHashes.Empty();
			return;
		}

		Ar << Version;
		Ar << EncryptedHash;
		Ar << ChunkHashes;
	}

	/**
	 * Decrypt the chunk CRCs hash and validate that it matches the current one
	 */
	bool DecryptSignatureAndValidate(const FRSAKeyHandle InKey, const FString& InFilename)
	{
		if (Version == EVersion::Invalid)
		{
			UE_LOG(LogPakFile, Warning, TEXT("Pak signature file for '%s' was invalid"), *InFilename);
		}
		else
		{
			int32 BytesDecrypted = FRSA::DecryptPublic(EncryptedHash, SignatureData, InKey);
			if (BytesDecrypted > (int32)UE_ARRAY_COUNT(FSHAHash::Hash))
			{
				FMemory::Memcpy(DecryptedHash.Hash, SignatureData.GetData() + SignatureData.Num() - UE_ARRAY_COUNT(FSHAHash::Hash), UE_ARRAY_COUNT(FSHAHash::Hash));
				SignatureData.SetNum(SignatureData.Num() - UE_ARRAY_COUNT(FSHAHash::Hash));
				FSHAHash CurrentHash = ComputeCurrentPrincipalHash();
				if (DecryptedHash == CurrentHash)
				{
					return true;
				}
				else
				{
					UE_LOG(LogPakFile, Warning, TEXT("Pak signature table validation failed for '%s'! Expected %s, Received %s"), *InFilename, *DecryptedHash.ToString(), *CurrentHash.ToString());
				}
			}
			else
			{
				UE_LOG(LogPakFile, Warning, TEXT("Pak signature table validation failed for '%s'! Failed to decrypt signature"), *InFilename);
			}
		}

		FPakPlatformFile::BroadcastPakPrincipalSignatureTableCheckFailure(InFilename);
		return false;
	}

	/**
	 * Helper function for computing the SHA1 hash of the current chunk CRC array
	 */
	FSHAHash ComputeCurrentPrincipalHash() const
	{
		FSHAHash CurrentHash;
		FSHA1::HashBuffer(ChunkHashes.GetData(), ChunkHashes.Num() * sizeof(TPakChunkHash), CurrentHash.Hash);
		return CurrentHash;
	}

	UE_DEPRECATED("5.1", "Use ComputeCurrentPrincipalHash instead")
	FSHAHash ComputeCurrentMasterHash() const
	{
		return ComputeCurrentPrincipalHash();
	}
};
