// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/HashTable.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "FileCache/FileCache.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "IO/IoDispatcher.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreDelegates.h"
#include "Misc/MemoryReadStream.h"
#include "Misc/SecureHash.h"
#include "RHI.h"
#include "RHIDefinitions.h"
#include "Serialization/Archive.h"
#include "Shader.h"
#include "ShaderCodeLibrary.h"
#include "Templates/RefCounting.h"
#include "UObject/NameTypes.h"

#if WITH_EDITOR
class FCbFieldView;
class FCbWriter;
#endif

// enable visualization in the desktop Development builds only as it has a memory hit and writes files
#define UE_SCA_VISUALIZE_SHADER_USAGE			(!WITH_EDITOR && UE_BUILD_DEVELOPMENT && PLATFORM_DESKTOP)

// enable deep, manual debugging of leaked preload groups. This level of information slows the engine down and is only needed when chasing tricky bugs
#define UE_SCA_DEBUG_PRELOADING					(0)

struct FShaderMapEntry
{
	uint32 ShaderIndicesOffset = 0u;
	uint32 NumShaders = 0u;
	uint32 FirstPreloadIndex = 0u;
	uint32 NumPreloadEntries = 0u;

	friend FArchive& operator <<(FArchive& Ar, FShaderMapEntry& Ref)
	{
		return Ar << Ref.ShaderIndicesOffset << Ref.NumShaders << Ref.FirstPreloadIndex << Ref.NumPreloadEntries;
	}
};

static FArchive& operator <<(FArchive& Ar, FFileCachePreloadEntry& Ref)
{
	return Ar << Ref.Offset << Ref.Size;
}

struct FShaderCodeEntry
{
	uint64 Offset = 0;
	uint32 Size = 0;
	uint32 UncompressedSize = 0;
	uint8 Frequency;

	EShaderFrequency GetFrequency() const
	{
		return (EShaderFrequency)Frequency;
	}

	friend FArchive& operator <<(FArchive& Ar, FShaderCodeEntry& Ref)
	{
		return Ar << Ref.Offset << Ref.Size << Ref.UncompressedSize << Ref.Frequency;
	}
};

// Portion of shader code archive that's serialize to disk
class FSerializedShaderArchive
{
public:

	/** Hashes of all shadermaps in the library */
	TArray<FSHAHash> ShaderMapHashes;

	/** Output hashes of all shaders in the library */
	TArray<FSHAHash> ShaderHashes;

	/** An array of a shadermap descriptors. Each shadermap can reference an arbitrary number of shaders */
	TArray<FShaderMapEntry> ShaderMapEntries;

	/** An array of all shaders descriptors, deduplicated */
	TArray<FShaderCodeEntry> ShaderEntries;

	/** An array of entries for the bytes of shadercode that need to be preloaded for a shadermap.
	  * Each shadermap has a range in this array, beginning of which is stored in FShaderMapEntry.FirstPreloadIndex. */
	TArray<FFileCachePreloadEntry> PreloadEntries;

	/** Flat array of shaders referenced by all shadermaps. Each shadermap has a range in this array, beginning of which is
	  * stored as ShaderIndicesOffset in the shadermap's descriptor (FShaderMapEntry).
	  */
	TArray<uint32> ShaderIndices;

	FHashTable ShaderMapHashTable;
	FHashTable ShaderHashTable;

#if WITH_EDITOR
	/** Mapping from shadermap hashes to an array of asset names - this is used for on-disk storage as it is shorter. */
	TMap<FSHAHash, FShaderMapAssetPaths> ShaderCodeToAssets;

	enum class EAssetInfoVersion : uint8
	{
		CurrentVersion = 2
	};

	struct FDebugStats
	{
		int32 NumAssets;
		int64 ShadersSize;
		int64 ShadersUniqueSize;
		int32 NumShaders;
		int32 NumUniqueShaders;
		int32 NumShaderMaps;
	};

	struct FExtendedDebugStats
	{
		/** Textual contents, should match the binary layout in terms of order */
		FString TextualRepresentation;

		/** Minimum number of shaders in any given shadermap */
		uint32 MinNumberOfShadersPerSM;

		/** Median number of shaders in shadermaps */
		uint32 MedianNumberOfShadersPerSM;

		/** Maximum number of shaders in any given shadermap */
		uint32 MaxNumberofShadersPerSM;

		/** For the top shaders (descending), number of shadermaps in which they are used. Expected to be limited to a small number (10) */
		TArray<int32> TopShaderUsages;

		/** Number of shaers per frequency. */
		int32 NumShadersPerFrequency[SF_NumFrequencies];

		/** Uncompressed size of all shaders of a given frequency. */
		uint64 UncompressedSizePerFrequency[SF_NumFrequencies];

		/** Compressed (individually) size of all shaders of a given frequency. */
		uint64 CompressedSizePerFrequency[SF_NumFrequencies];
	};
#endif

	FSerializedShaderArchive()
	{
	}

	int64 GetAllocatedSize() const
	{
		return ShaderHashes.GetAllocatedSize() +
			ShaderEntries.GetAllocatedSize() +
			ShaderMapHashes.GetAllocatedSize() +
			ShaderMapEntries.GetAllocatedSize() +
			PreloadEntries.GetAllocatedSize() +
			ShaderIndices.GetAllocatedSize()
#if WITH_EDITOR
			+ ShaderCodeToAssets.GetAllocatedSize()
#endif // WITH_EDITOR
			;
	}

	void Empty()
	{
		EmptyShaderMaps();

		ShaderHashes.Empty();
		ShaderEntries.Empty();
		ShaderHashTable.Clear();
	}

	void EmptyShaderMaps()
	{
		ShaderMapHashes.Empty();
		ShaderMapEntries.Empty();
		PreloadEntries.Empty();
		ShaderIndices.Empty();
		ShaderMapHashTable.Clear();
#if WITH_EDITOR
		ShaderCodeToAssets.Empty();
#endif
	}

	int32 GetNumShaderMaps() const
	{
		return ShaderMapEntries.Num();
	}

	int32 GetNumShaders() const
	{
		return ShaderEntries.Num();
	}

	bool IsEmpty() const
	{
		return ShaderMapEntries.IsEmpty() && ShaderEntries.IsEmpty() && PreloadEntries.IsEmpty()
#if WITH_EDITOR
			&& ShaderCodeToAssets.IsEmpty()
#endif
			;
	}

	RENDERCORE_API int32 FindShaderMapWithKey(const FSHAHash& Hash, uint32 Key) const;
	RENDERCORE_API int32 FindShaderMap(const FSHAHash& Hash) const;
	RENDERCORE_API bool FindOrAddShaderMap(const FSHAHash& Hash, int32& OutIndex, const FShaderMapAssetPaths* AssociatedAssets);

	RENDERCORE_API int32 FindShaderWithKey(const FSHAHash& Hash, uint32 Key) const;
	RENDERCORE_API int32 FindShader(const FSHAHash& Hash) const;
	RENDERCORE_API bool FindOrAddShader(const FSHAHash& Hash, int32& OutIndex);
	RENDERCORE_API void RemoveLastAddedShader();

	RENDERCORE_API void DecompressShader(int32 Index, const TArray<TArray<uint8>>& ShaderCode, TArray<uint8>& OutDecompressedShader) const;

	RENDERCORE_API void Finalize();
	RENDERCORE_API void Serialize(FArchive& Ar);
#if WITH_EDITOR
	RENDERCORE_API void SaveAssetInfo(FArchive& Ar);
	RENDERCORE_API bool LoadAssetInfo(const FString& Filename);
	RENDERCORE_API void CreateAsChunkFrom(const FSerializedShaderArchive& Parent, const TSet<FName>& PackagesInChunk, TArray<int32>& OutShaderCodeEntriesNeeded);
	RENDERCORE_API void CollectStatsAndDebugInfo(FDebugStats& OutDebugStats, FExtendedDebugStats* OutExtendedDebugStats);
	RENDERCORE_API void DumpContentsInPlaintext(FString& OutText) const;
	RENDERCORE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FSerializedShaderArchive& Archive);
	RENDERCORE_API friend bool LoadFromCompactBinary(FCbFieldView Field, FSerializedShaderArchive& OutArchive);
#endif

	friend FArchive& operator<<(FArchive& Ar, FSerializedShaderArchive& Ref)
	{
		Ref.Serialize(Ar);
		return Ar;
	}
};

// run-time only debugging facility
struct FShaderUsageVisualizer
{
#if UE_SCA_VISUALIZE_SHADER_USAGE
	/** Lock guarding access to visualization structures. */
	FCriticalSection VisualizeLock;

	/** Total number of shaders. */
	int32 NumShaders;

	/** Shader indices that we explicitly preloaded (does not include shaders preloaded as part of a compressed group). */
	TSet<int32> ExplicitlyPreloadedShaders;

	/** Shader indices that we preloaded (either explicitly or because they were a part of a compressed group). */
	TSet<int32> PreloadedShaders;

	/** Shader indices that we decompressed. */
	TSet<int32> DecompressedShaders;

	/** Shader indices that were created. */
	TSet<int32> CreatedShaders;

	void Initialize(const int32 InNumShaders);

	inline void MarkExplicitlyPreloadedForVisualization(int32 ShaderIndex)
	{
		extern int32 GShaderCodeLibraryVisualizeShaderUsage;
		if (LIKELY(GShaderCodeLibraryVisualizeShaderUsage))
		{
			FScopeLock Lock(&VisualizeLock);
			ExplicitlyPreloadedShaders.Add(ShaderIndex);
		}
	}

	inline void MarkPreloadedForVisualization(int32 ShaderIndex)
	{
		extern int32 GShaderCodeLibraryVisualizeShaderUsage;
		if (LIKELY(GShaderCodeLibraryVisualizeShaderUsage))
		{
			FScopeLock Lock(&VisualizeLock);
			PreloadedShaders.Add(ShaderIndex);
		}
	}

	inline void MarkDecompressedForVisualization(int32 ShaderIndex)
	{
		extern int32 GShaderCodeLibraryVisualizeShaderUsage;
		if (LIKELY(GShaderCodeLibraryVisualizeShaderUsage))
		{
			FScopeLock Lock(&VisualizeLock);
			DecompressedShaders.Add(ShaderIndex);
		}
	}

	inline void MarkCreatedForVisualization(int32 ShaderIndex)
	{
		extern int32 GShaderCodeLibraryVisualizeShaderUsage;
		if (LIKELY(GShaderCodeLibraryVisualizeShaderUsage))
		{
			FScopeLock Lock(&VisualizeLock);
			CreatedShaders.Add(ShaderIndex);
		}
	}

	void SaveShaderUsageBitmap(const FString& Name, EShaderPlatform ShaderPlatform);
#else
	inline void Initialize(const int32 InNumShaders) {}
	inline void MarkPreloadedForVisualization(int32 ShaderIndex) {}
	inline void MarkExplicitlyPreloadedForVisualization(int32 ShaderIndex) {}
	inline void MarkDecompressedForVisualization(int32 ShaderIndex) {}
	inline void MarkCreatedForVisualization(int32 ShaderIndex) {}
	inline void SaveShaderUsageBitmap(const FString& Name, EShaderPlatform ShaderPlatform) {}
#endif // UE_SCA_VISUALIZE_SHADER_USAGE
};

class FShaderCodeArchive : public FRHIShaderLibrary
{
public:
	static FShaderCodeArchive* Create(EShaderPlatform InPlatform, FArchive& Ar, const FString& InDestFilePath, const FString& InLibraryDir, const FString& InLibraryName);

	virtual ~FShaderCodeArchive();

	virtual bool IsNativeLibrary() const override { return false; }

	int64 GetSizeBytes() const
	{
		return sizeof(*this) +
			SerializedShaders.GetAllocatedSize() +
			ShaderPreloads.GetAllocatedSize();
	}

	virtual int32 GetNumShaders() const override { return SerializedShaders.ShaderEntries.Num(); }
	virtual int32 GetNumShaderMaps() const override { return SerializedShaders.ShaderMapEntries.Num(); }
	virtual int32 GetNumShadersForShaderMap(int32 ShaderMapIndex) const override { return SerializedShaders.ShaderMapEntries[ShaderMapIndex].NumShaders; }

	virtual int32 GetShaderIndex(int32 ShaderMapIndex, int32 i) const override
	{
		const FShaderMapEntry& ShaderMapEntry = SerializedShaders.ShaderMapEntries[ShaderMapIndex];
		return SerializedShaders.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
	}

	virtual int32 FindShaderMapIndex(const FSHAHash& Hash) override
	{
		return SerializedShaders.FindShaderMap(Hash);
	}

	virtual int32 FindShaderIndex(const FSHAHash& Hash) override
	{
		return SerializedShaders.FindShader(Hash);
	}

	virtual FSHAHash GetShaderHash(int32 ShaderMapIndex, int32 ShaderIndex) override
	{
		return SerializedShaders.ShaderHashes[GetShaderIndex(ShaderMapIndex, ShaderIndex)];
	};

	virtual bool PreloadShader(int32 ShaderIndex, FGraphEventArray& OutCompletionEvents) override;

	virtual bool PreloadShaderMap(int32 ShaderMapIndex, FGraphEventArray& OutCompletionEvents) override;

	virtual void ReleasePreloadedShader(int32 ShaderIndex) override;

	virtual TRefCountPtr<FRHIShader> CreateShader(int32 Index) override;
	virtual void Teardown() override;

	void OnShaderPreloadFinished(int32 ShaderIndex, const IMemoryReadStreamRef& PreloadData);

protected:
	FShaderCodeArchive(EShaderPlatform InPlatform, const FString& InLibraryDir, const FString& InLibraryName);

	FORCENOINLINE void CheckShaderCreation(void* ShaderPtr, int32 Index)
	{
	}

	struct FShaderPreloadEntry
	{
		FGraphEventRef PreloadEvent;
		void* Code = nullptr;
		uint32 FramePreloadStarted = ~0u;
		uint32 NumRefs : 31;
		uint32 bNeverToBePreloaded : 1;

		FShaderPreloadEntry()
			: NumRefs(0)
			, bNeverToBePreloaded(0)
		{
		}
	};

	bool WaitForPreload(FShaderPreloadEntry& ShaderPreloadEntry);

	// Library directory
	FString LibraryDir;

	// Offset at where shader code starts in a code library
	int64 LibraryCodeOffset;

	// Library file handle for async reads
	IFileCacheHandle* FileCacheHandle;

	// The shader code present in the library
	FSerializedShaderArchive SerializedShaders;

	TArray<FShaderPreloadEntry> ShaderPreloads;
	FRWLock ShaderPreloadLock;

	/** debug visualizer - in Shipping compiles out to an empty struct with no-op functions */
	FShaderUsageVisualizer DebugVisualizer;
};



namespace ShaderCodeArchive
{
	/** Decompresses the shader into caller-provided memory. Caller is assumed to allocate at least ShaderEntry uncompressed size value.
	 * The engine will crash (LogFatal) if this function fails.
	 */
	RENDERCORE_API void DecompressShaderWithOodle(uint8* OutDecompressedShader, int64 UncompressedSize, const uint8* CompressedShaderCode, int64 CompressedSize);
	
	// We decompression, group, and recompress shaders when they are added to iostore containers in UnrealPak, where we don't have access to cvars - so there's no way 
	// to configure - so we force Oodle and allow specification of the parameters here.
	RENDERCORE_API bool CompressShaderWithOodle(uint8* OutCompressedShader, int64& OutCompressedSize, const uint8* InUncompressedShaderCode, int64 InUncompressedSize, FOodleDataCompression::ECompressor InOodleCompressor, FOodleDataCompression::ECompressionLevel InOodleLevel);
}

/** Descriptor of a shader map. This concept exists in run time, so this class describes the information stored in the library for a particular FShaderMap */
struct FIoStoreShaderMapEntry
{
	/** Offset to an the first shader index referenced by this shader map in the array of shader indices. */
	uint32 ShaderIndicesOffset = 0u;
	/** Number of shaders in this shader map. */
	uint32 NumShaders = 0u;

	friend FArchive& operator <<(FArchive& Ar, FIoStoreShaderMapEntry& Ref)
	{
		return Ar << Ref.ShaderIndicesOffset << Ref.NumShaders;
	}
};

/** Descriptor of an individual shader. */
struct FIoStoreShaderCodeEntry
{
	union
	{
		uint64 Packed;
		struct
		{
			/** Shader type aka frequency (vertex, pixel, etc) */
			uint64 Frequency : SF_NumBits;	// 4 as of now

			/** Each shader belongs to a (one and only) shader group (even if it is the only one shader in that group) that is compressed and decompressed together. */
			uint64 ShaderGroupIndex : 30;

			/** Offset of the uncompressed shader in a group of shaders that are compressed / decompressed together. */
			uint64 UncompressedOffsetInGroup : 30;
		};
	};

	FIoStoreShaderCodeEntry()
		: Packed(0)
	{
	}

	EShaderFrequency GetFrequency() const
	{
		return (EShaderFrequency)Frequency;
	}

	friend FArchive& operator <<(FArchive& Ar, FIoStoreShaderCodeEntry& Ref)
	{
		return Ar << Ref.Packed;
	}
};

static_assert(sizeof(FIoStoreShaderCodeEntry) == sizeof(uint64), "To reduce memory footprint, shader code entries should be as small as possible");

/** Descriptor of a group of shaders compressed together. This groups already deduplicated, and possibly unrelated, shaders, so this is a distinct concept from a shader map. */
struct FIoStoreShaderGroupEntry
{
	/** Offset to an the first shader index referenced by this group in the array of shader indices. This extra level of indirection allows arbitrary grouping. */
	uint32 ShaderIndicesOffset = 0u;
	/** Number of shaders in this group. */
	uint32 NumShaders = 0u;

	/** Uncompressed size of the whole group. */
	uint32 UncompressedSize = 0;
	/** Compressed size of the whole group. */
	uint32 CompressedSize = 0;

	friend FArchive& operator <<(FArchive& Ar, FIoStoreShaderGroupEntry& Ref)
	{
		return Ar << Ref.ShaderIndicesOffset << Ref.NumShaders << Ref.UncompressedSize << Ref.CompressedSize;
	}

	/** Some groups can be stored uncompressed if their compression wasn't beneficial (this is very vell possible, for groups that contain only one small shader. */
	inline bool IsGroupCompressed() const
	{
		return CompressedSize != UncompressedSize;
	}
};

struct FIoStoreShaderCodeArchiveHeader
{
public:

	/** Hashes of all shadermaps in the library */
	TArray<FSHAHash> ShaderMapHashes;

	/** Output hashes of all shaders in the library */
	TArray<FSHAHash> ShaderHashes;

	/** Chunk Ids (essentially hashes) of the shader groups - needed to be serialized as they are used for preloading. */
	TArray<FIoChunkId> ShaderGroupIoHashes;

	/** An array of a shadermap descriptors. Each shadermap can reference an arbitrary number of shaders */
	TArray<FIoStoreShaderMapEntry> ShaderMapEntries;

	/** An array of all shaders descriptors, deduplicated */
	TArray<FIoStoreShaderCodeEntry> ShaderEntries;

	/** An array of shader group descriptors */
	TArray<FIoStoreShaderGroupEntry> ShaderGroupEntries;

	/** Flat array of shaders referenced by all shadermaps. Each shadermap has a range in this array, beginning of which is
	  * stored as ShaderIndicesOffset in the shadermap's descriptor (FIoStoreShaderMapEntry).
	  * This is also used by the shader groups.
	  */
	TArray<uint32> ShaderIndices;

	friend RENDERCORE_API FArchive& operator <<(FArchive& Ar, FIoStoreShaderCodeArchiveHeader& Ref);

	inline uint64 GetShaderUncompressedSize(int ShaderIndex)
	{
		const FIoStoreShaderCodeEntry& ThisShaderEntry = ShaderEntries[ShaderIndex];
		const FIoStoreShaderGroupEntry& GroupEntry = ShaderGroupEntries[ThisShaderEntry.ShaderGroupIndex];

		for (uint32 ShaderIdxIdx = GroupEntry.ShaderIndicesOffset, StopBeforeIdxIdx = GroupEntry.ShaderIndicesOffset + GroupEntry.NumShaders; ShaderIdxIdx < StopBeforeIdxIdx; ++ShaderIdxIdx)
		{
			int32 GroupMemberShaderIndex = ShaderIndices[ShaderIdxIdx];
			if (ShaderIndex == GroupMemberShaderIndex)
			{
				// found ourselves, now find our size by subtracting from the next neighbor or the group size
				if (LIKELY(ShaderIdxIdx < StopBeforeIdxIdx - 1))
				{
					const FIoStoreShaderCodeEntry& NextShaderEntry = ShaderEntries[ShaderIndices[ShaderIdxIdx + 1]];
					return NextShaderEntry.UncompressedOffsetInGroup - ThisShaderEntry.UncompressedOffsetInGroup;
				}
				else
				{
					return GroupEntry.UncompressedSize - ThisShaderEntry.UncompressedOffsetInGroup;
				}
			}
		}

		checkf(false, TEXT("Could not find shader index %d in its own group %d - library is corrupted."), ShaderIndex, ThisShaderEntry.ShaderGroupIndex);
		return 0;
	}

	uint64 GetAllocatedSize() const
	{
		return sizeof(*this) +
			ShaderMapHashes.GetAllocatedSize() +
			ShaderHashes.GetAllocatedSize() +
			ShaderGroupIoHashes.GetAllocatedSize() +
			ShaderMapEntries.GetAllocatedSize() +
			ShaderEntries.GetAllocatedSize() +
			ShaderGroupEntries.GetAllocatedSize() +
			ShaderIndices.GetAllocatedSize();
	}
};

class FIoStoreShaderCodeArchive : public FRHIShaderLibrary
{
public:
	RENDERCORE_API static FIoChunkId GetShaderCodeArchiveChunkId(const FString& LibraryName, FName FormatName);
	RENDERCORE_API static FIoChunkId GetShaderCodeChunkId(const FSHAHash& ShaderHash);
	/** This function creates the archive header, including splitting shaders into groups. */
	RENDERCORE_API static void CreateIoStoreShaderCodeArchiveHeader(const FName& Format, const FSerializedShaderArchive& SerializedShaders, FIoStoreShaderCodeArchiveHeader& OutHeader);
	RENDERCORE_API static void SaveIoStoreShaderCodeArchive(const FIoStoreShaderCodeArchiveHeader& Header, FArchive& OutLibraryAr);
	static FIoStoreShaderCodeArchive* Create(EShaderPlatform InPlatform, const FString& InLibraryName, FIoDispatcher& InIoDispatcher);

	virtual ~FIoStoreShaderCodeArchive();

	virtual bool IsNativeLibrary() const override { return false; }

	uint64 GetSizeBytes() const
	{
		return sizeof(*this) +
			Header.GetAllocatedSize() +
			PreloadedShaderGroups.GetAllocatedSize();
	}

	virtual int32 GetNumShaders() const override { return Header.ShaderEntries.Num(); }
	virtual int32 GetNumShaderMaps() const override { return Header.ShaderMapEntries.Num(); }
	virtual int32 GetNumShadersForShaderMap(int32 ShaderMapIndex) const override { return Header.ShaderMapEntries[ShaderMapIndex].NumShaders; }

	virtual int32 GetShaderIndex(int32 ShaderMapIndex, int32 i) const override
	{
		const FIoStoreShaderMapEntry& ShaderMapEntry = Header.ShaderMapEntries[ShaderMapIndex];
		return Header.ShaderIndices[ShaderMapEntry.ShaderIndicesOffset + i];
	}

	virtual int32 FindShaderMapIndex(const FSHAHash& Hash) override;
	virtual int32 FindShaderIndex(const FSHAHash& Hash) override;
	virtual FSHAHash GetShaderHash(int32 ShaderMapIndex, int32 ShaderIndex) override
	{
		return Header.ShaderHashes[GetShaderIndex(ShaderMapIndex, ShaderIndex)];
	}

	virtual bool PreloadShader(int32 ShaderIndex, FGraphEventArray& OutCompletionEvents) override;
	virtual bool PreloadShaderMap(int32 ShaderMapIndex, FGraphEventArray& OutCompletionEvents) override;
	virtual bool PreloadShaderMap(int32 ShaderMapIndex, FCoreDelegates::FAttachShaderReadRequestFunc AttachShaderReadRequestFunc) override;
	virtual void ReleasePreloadedShader(int32 ShaderIndex) override;
	virtual TRefCountPtr<FRHIShader> CreateShader(int32 Index) override;
	virtual void Teardown() override;

private:
	static constexpr uint32 CurrentVersion = 1;

	struct FShaderGroupPreloadEntry
	{
		FGraphEventRef PreloadEvent;
		FIoRequest IoRequest;
		uint32 FramePreloadStarted = ~0u;
		uint32 NumRefs : 31;
		uint32 bNeverToBePreloaded : 1;

#if UE_SCA_DEBUG_PRELOADING
		FString DebugInfo;
#endif

		FShaderGroupPreloadEntry()
			: NumRefs(0)
			, bNeverToBePreloaded(0)
		{
		}
	};

	FIoStoreShaderCodeArchive(EShaderPlatform InPlatform, const FString& InLibraryName, FIoDispatcher& InIoDispatcher);

	FIoDispatcher& IoDispatcher;

	/** Preloads a given shader group. */
	bool PreloadShaderGroup(int32 ShaderGroupIndex, FGraphEventArray& OutCompletionEvents, 
#if UE_SCA_DEBUG_PRELOADING
		const FString& CallsiteInfo,
#endif
		FCoreDelegates::FAttachShaderReadRequestFunc* AttachShaderReadRequestFuncPtr = nullptr);

	/** Sets up a new preload entry for preload.*/
	void SetupPreloadEntryForLoading(int32 ShaderGroupIndex, FShaderGroupPreloadEntry& PreloadEntry);

	/** Sets up a preload entry for groups that shouldn't be preloaded.*/
	void MarkPreloadEntrySkipped(int32 ShaderGroupIndex
#if UE_SCA_DEBUG_PRELOADING
		, const FString& CallsiteInfo
#endif
	);

	/** Releases a reference to a preloaded shader group, potentially deleting it. */
	void ReleasePreloadEntry(int32 ShaderGroupIndex
#if UE_SCA_DEBUG_PRELOADING
		, const FString& CallsiteInfo
#endif
	);

	/** Returns the index of shader group that a given shader belongs to. */
	inline int32 GetGroupIndexForShader(int32 ShaderIndex) const
	{
		return Header.ShaderEntries[ShaderIndex].ShaderGroupIndex;
	}

	/** Finds or adds preload info for a shader group. Assumes lock guarding access to the info taken, never returns nullptr (except when new failed and we're already broken beyond repair)*/
	inline FShaderGroupPreloadEntry* FindOrAddPreloadEntry(int32 ShaderGroupIndex)
	{
		FShaderGroupPreloadEntry*& Ptr = PreloadedShaderGroups.FindOrAdd(ShaderGroupIndex);
		if (UNLIKELY(Ptr == nullptr))
		{
			Ptr = new FShaderGroupPreloadEntry;
		}
		return Ptr;
	}

	/** Finds existing preload info for a shader group. Assumes lock guarding access to the info is taken */
	inline FShaderGroupPreloadEntry* FindExistingPreloadEntry(int32 ShaderGroupIndex)
	{
		FShaderGroupPreloadEntry** PtrPtr = PreloadedShaderGroups.Find(ShaderGroupIndex);
		checkf(PtrPtr != nullptr, TEXT("The preload entry for a shader group we assumed to exist does not exist!"));
		return *PtrPtr;
	}

	/** Returns true if the group contains only RTX shaders. We can avoid preloading it when running with RTX off. */
	bool GroupOnlyContainsRaytracingShaders(int32 ShaderGroupIndex);

	/** Archive header with all the metadata */
	FIoStoreShaderCodeArchiveHeader Header;

	/** Hash tables for faster searching for shader and shadermap hashes. */
	FHashTable ShaderMapHashTable;
	FHashTable ShaderHashTable;

	/** Mapping between the group index and preloaded groups. Should be only modified when lock is taken. */
	TMap<int32, FShaderGroupPreloadEntry*> PreloadedShaderGroups;
	/** Lock guarding access to the book-keeping info above.*/
	FRWLock PreloadedShaderGroupsLock;

	/** debug visualizer - in Shipping compiles out to an empty struct with no-op functions */
	FShaderUsageVisualizer DebugVisualizer;
};
