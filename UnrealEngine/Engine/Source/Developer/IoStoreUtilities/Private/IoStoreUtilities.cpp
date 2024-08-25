// Copyright Epic Games, Inc. All Rights Reserved.

#include "IoStoreUtilities.h"

#include "IoStoreLooseFiles.h"
#include "Algo/TopologicalSort.h"
#include "Async/AsyncWork.h"
#include "CookMetadata.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Hash/CityHash.h"
#include "Hash/xxhash.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "IO/IoDispatcher.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Misc/AES.h"
#include "Misc/CoreDelegates.h"
#include "Misc/KeyChainUtilities.h"
#include "Misc/WildcardString.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Archive.h"
#include "Serialization/BulkData.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/BufferWriter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Settings/ProjectPackagingSettings.h" // for EAssetRegistryWritebackMethod
#include "IO/IoStoreOnDemand.h"
#include "IO/PackageStore.h"
#include "UObject/Class.h"
#include "UObject/NameBatchSerialization.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/ObjectResource.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeLock.h"
#include "Async/ParallelFor.h"
#include "Async/AsyncFileHandle.h"
#include "Async/Async.h"
#include "RSA.h"
#include "Misc/AssetRegistryInterface.h"
#include "AssetRegistry/AssetRegistryState.h"
#include "Misc/OutputDeviceFile.h"
#include "Misc/FeedbackContext.h"
#include "Serialization/LargeMemoryReader.h"
#include "Misc/StringBuilder.h"
#include "Async/Future.h"
#include "Algo/MaxElement.h"
#include "Algo/Sort.h"
#include "Algo/StableSort.h"
#include "Algo/IsSorted.h"
#include "PackageStoreOptimizer.h"
#include "ShaderCodeArchive.h"
#include "ZenStoreHttpClient.h"
#include "IPlatformFilePak.h"
#include "ZenStoreWriter.h"
#include "IO/IoContainerHeader.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "IO/IoStore.h"
#include "ZenFileSystemManifest.h"
#include "IPlatformFileSandboxWrapper.h"
#include "Misc/PathViews.h"
#include "HAL/FileManagerGeneric.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/ZenPackageHeader.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, IoStoreUtilities);

#define IOSTORE_CPU_SCOPE(NAME) TRACE_CPUPROFILER_EVENT_SCOPE(IoStore##NAME);
#define IOSTORE_CPU_SCOPE_DATA(NAME, DATA) TRACE_CPUPROFILER_EVENT_SCOPE(IoStore##NAME);

TRACE_DECLARE_MEMORY_COUNTER(IoStoreUsedFileBufferMemory, TEXT("IoStore/UsedFileBufferMemory"));

// Helper to format numbers with comma separators to help readability: 1,234 vs 1234.
static FString NumberString(uint64 N) { return FText::AsNumber(N).ToString(); }

// Used for tracking how the chunk will get deployed and thus what classification its size is.
struct FIoStoreChunkSource
{
	FIoStoreTocChunkInfo ChunkInfo;
	UE::Cook::EPluginSizeTypes SizeType;

};

static const FName DefaultCompressionMethod = NAME_Zlib;
static const uint64 DefaultCompressionBlockSize = 64 << 10;
static const uint64 DefaultCompressionBlockAlignment = 64 << 10;
static const uint64 DefaultMemoryMappingAlignment = 16 << 10;

static TUniquePtr<FIoStoreReader> CreateIoStoreReader(const TCHAR* Path, const FKeyChain& KeyChain);
bool UploadIoStoreContainerFiles(const UE::IO::IAS::FIoStoreUploadParams& UploadParams, TConstArrayView<FString> ContainerFiles, const FKeyChain& KeyChain);

/*
* Provides access to previously compressed chunks to the iostore writer, allowing
* a) avoiding recompressing things and b) tweaks to compressors dont cause massive patches.
*/
class FIoStoreChunkDatabase : public IIoStoreWriterReferenceChunkDatabase
{
public:
	static const uint8 IoChunkTypeCount = (uint8)EIoChunkType::MAX;

	TArray<TUniquePtr<FIoStoreReader>> Readers;
	struct FReaderChunks
	{
		int32 ReaderIndex;
		TMap<FIoChunkHash, FIoStoreTocChunkInfo> Chunks;
		TSet<FIoChunkId> ChunkIds;

		std::atomic_uint64_t ChangedChunkCount[IoChunkTypeCount];
		std::atomic_uint64_t NewChunkCount[IoChunkTypeCount];
		std::atomic_uint64_t UsedChunkCount[IoChunkTypeCount];
	};

	struct FMissingContainerInfo
	{
		std::atomic_uint64_t RequestedChunkCount[IoChunkTypeCount];
	};

	TMap<FIoContainerId, TUniquePtr<FMissingContainerInfo>> MissingContainerIds;

	TMap<FIoContainerId, FString> ContainerNameMap;

	TMap<FIoContainerId, TUniquePtr<FReaderChunks>> ChunkDatabase;


	std::atomic_int32_t RequestCount = 0;
	int32 FulfillCount = 0;
	int32 ContainerNotFound = 0;
	int64 FulfillBytes = 0;
	int64 FulfillBytesPerChunk[(int8)EIoChunkType::MAX] = {};
	uint32 CompressionBlockSize = 0;

	bool Init(const FString& InGlobalContainerFileName, const FKeyChain& InDecryptionKeychain)
	{
		double StartTime = FPlatformTime::Seconds();

		TUniquePtr<FIoStoreReader> GlobalReader = CreateIoStoreReader(*InGlobalContainerFileName, InDecryptionKeychain);
		if (GlobalReader.IsValid() == false)
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed to open reference chunk container %s"), *InGlobalContainerFileName);
			return false;
		}

		FString Directory = FPaths::GetPath(InGlobalContainerFileName);
		FPaths::NormalizeDirectoryName(Directory);

		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFiles(FoundContainerFiles, *(Directory / TEXT("*.utoc")), true, false);
		TArray<FString> ContainerFilePaths;
		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Directory / Filename);
		}

		CompressionBlockSize = 0;
		int64 IoChunkCount = 0;
		for (const FString& ContainerFilePath : ContainerFilePaths)
		{
			TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, InDecryptionKeychain);
			if (Reader.IsValid() == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to open reference chunk container %s"), *ContainerFilePath);
				return false;
			}

			if (ChunkDatabase.Contains(Reader->GetContainerId()))
			{
				UE_LOG(LogIoStore, Error, TEXT("Duplicate container id found in reference chunk container directory %s"), *ContainerFilePath);
				UE_LOG(LogIoStore, Error, TEXT("is duplicate of %s"), *ContainerNameMap[Reader->GetContainerId()]);
				return false;
			}

			TUniquePtr<FReaderChunks> ReaderChunks(new FReaderChunks());

			ContainerNameMap.Add(Reader->GetContainerId(), *ContainerFilePath);

			Reader->EnumerateChunks([ReaderChunks = ReaderChunks.Get()](FIoStoreTocChunkInfo&& ChunkInfo)
			{
				ReaderChunks->ChunkIds.Add(ChunkInfo.Id);
				ReaderChunks->Chunks.Add(TPair<FIoChunkHash, FIoStoreTocChunkInfo>(ChunkInfo.Hash, MoveTemp(ChunkInfo)));
				return true;
			});

			if (Readers.Num() == 0)
			{
				CompressionBlockSize = Reader->GetCompressionBlockSize();
			}
			else if (Reader->GetCompressionBlockSize() != CompressionBlockSize)
			{
				UE_LOG(LogIoStore, Error, TEXT("Reference chunk containers had different compression block sizes, failing to init reference db (%u and %u)"), CompressionBlockSize, Reader->GetCompressionBlockSize());
				return false;
			}

			IoChunkCount += ReaderChunks->Chunks.Num();
			ReaderChunks->ReaderIndex = Readers.Num();
			ChunkDatabase.Add(Reader->GetContainerId(), MoveTemp(ReaderChunks));
			Readers.Add(MoveTemp(Reader));

			
		}

		UE_LOG(LogIoStore, Display, TEXT("Block reference loaded %d containers and %s chunks, in %.1f seconds"), Readers.Num(), *FText::AsNumber(IoChunkCount).ToString(), FPlatformTime::Seconds() - StartTime);
		return true;
	}

	virtual void NotifyAddedToWriter(const FIoContainerId& InContainerId) override
	{
		// If we don't have this container in our chunk database, add it to a tracking
		// structure so we can see how many chunks we're missing.
		if (ChunkDatabase.Contains(InContainerId) == false)
		{
			MissingContainerIds.Add(InContainerId, MakeUnique<FMissingContainerInfo>());
		}
	}

	virtual uint32 GetCompressionBlockSize() const override
	{
		return CompressionBlockSize;
	}

	// Returns whether we expect to be able to load the chunk from the reference chunk database.
	// This can be called from any thread, though in the presence of existing hashes it's single threaded.
	virtual bool ChunkExists(const TPair<FIoContainerId, FIoChunkHash>& InChunkKey, const FIoChunkId& InChunkId, uint32& OutNumChunkBlocks)
	{
		RequestCount.fetch_add(1, std::memory_order_relaxed);

		TUniquePtr<FReaderChunks>* ReaderChunks = ChunkDatabase.Find(InChunkKey.Key);
		if (ReaderChunks == nullptr)
		{
			// Container doesn't exist - likely provided the path to a different project.
			TUniquePtr<FMissingContainerInfo>* MissingContainerHitCount = MissingContainerIds.Find(InChunkKey.Key);

			if (MissingContainerHitCount == nullptr)
			{
				UE_LOG(LogIoStore, Warning, TEXT("We got a container id that was never added! id = %llu"), InChunkKey.Key.Value());
			}
			else
			{
				MissingContainerHitCount[0]->RequestedChunkCount[(uint8)InChunkId.GetChunkType()].fetch_add(1, std::memory_order_relaxed);
			}

			ContainerNotFound++;
			return false;
		}

		FIoStoreTocChunkInfo* ChunkInfo = ReaderChunks[0]->Chunks.Find(InChunkKey.Value);
		if (ChunkInfo == nullptr)
		{
			// No exact chunk data match - this is a normal exit condition for a changed block or
			// a new block.
			bool bChangedBlock = ReaderChunks[0]->ChunkIds.Contains(InChunkId);
			if (bChangedBlock)
			{
				ReaderChunks[0]->ChangedChunkCount[(uint8)InChunkId.GetChunkType()].fetch_add(1, std::memory_order_relaxed);
			}
			else
			{
				ReaderChunks[0]->NewChunkCount[(uint8)InChunkId.GetChunkType()].fetch_add(1, std::memory_order_relaxed);
			}
			return false;
		}

		OutNumChunkBlocks = ChunkInfo->NumCompressedBlocks;
		return true;
	}


	// Not thread safe, called from the BeginCompress dispatch thread.
	virtual bool RetrieveChunk(const TPair<FIoContainerId, FIoChunkHash>& InChunkKey, TUniqueFunction<void(TIoStatusOr<FIoStoreCompressedReadResult>)> InCompleteCallback)
	{
		TUniquePtr<FReaderChunks>* ReaderChunks = ChunkDatabase.Find(InChunkKey.Key);
		if (ReaderChunks == nullptr)
		{
			// This should never happen now as we wrap this in a ChunkExists call.
			UE_LOG(LogIoStore, Warning, TEXT("RetrieveChunk can't find the container - invariant violated!"));
			return false;
		}

		FIoStoreTocChunkInfo* ChunkInfo = ReaderChunks[0]->Chunks.Find(InChunkKey.Value);
		if (ChunkInfo == nullptr)
		{
			// This should never happen now as we wrap this in a ChunkExists call.
			UE_LOG(LogIoStore, Warning, TEXT("RetrieveChunk can't find the chunnk - invariant violated!"));
			return false;
		}

		ReaderChunks[0]->UsedChunkCount[(uint8)ChunkInfo->Id.GetChunkType()].fetch_add(1, std::memory_order_relaxed);

		uint64 TotalCompressedSize = 0;
		uint64 TotalUncompressedSize = 0;
		uint32 CompressedBlockCount = 0;
		Readers[ReaderChunks[0]->ReaderIndex]->EnumerateCompressedBlocksForChunk(ChunkInfo->Id, [&TotalUncompressedSize, &CompressedBlockCount, &TotalCompressedSize](const FIoStoreTocCompressedBlockInfo& BlockInfo)
		{			
			TotalCompressedSize += BlockInfo.CompressedSize;
			TotalUncompressedSize += BlockInfo.UncompressedSize;
			CompressedBlockCount ++;
			return true;
		});

		FulfillBytesPerChunk[(int8)ChunkInfo->ChunkType] += TotalCompressedSize;
		FulfillBytes += TotalCompressedSize;
		FulfillCount++;

		//
		// At this point we know we can use the block so we can go async.
		//
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, Id = ChunkInfo->Id, ReaderIndex = ReaderChunks[0]->ReaderIndex, CompleteCallback = MoveTemp(InCompleteCallback)]()
		{
			TIoStatusOr<FIoStoreCompressedReadResult> Result = Readers[ReaderIndex]->ReadCompressed(Id, FIoReadOptions());
			CompleteCallback(Result);
		}, TStatId(), nullptr, ENamedThreads::AnyHiPriThreadNormalTask);

		return true;
	}
};

struct FReleasedPackages
{
	TSet<FName> PackageNames;
	TMap<FPackageId, FName> PackageIdToName;
};

static void LoadKeyChain(const TCHAR* CmdLine, FKeyChain& OutCryptoSettings)
{
	OutCryptoSettings.SetSigningKey(InvalidRSAKeyHandle);
	OutCryptoSettings.GetEncryptionKeys().Empty();

	// First, try and parse the keys from a supplied crypto key cache file
	FString CryptoKeysCacheFilename;
	if (FParse::Value(CmdLine, TEXT("cryptokeys="), CryptoKeysCacheFilename))
	{
		UE_LOG(LogIoStore, Display, TEXT("Parsing crypto keys from a crypto key cache file '%s'"), *CryptoKeysCacheFilename);
		KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, OutCryptoSettings);
	}
	else if (FParse::Param(CmdLine, TEXT("encryptionini")))
	{
		FString ProjectDir, EngineDir, Platform;

		if (FParse::Value(CmdLine, TEXT("projectdir="), ProjectDir, false)
			&& FParse::Value(CmdLine, TEXT("enginedir="), EngineDir, false)
			&& FParse::Value(CmdLine, TEXT("platform="), Platform, false))
		{
			UE_LOG(LogIoStore, Warning, TEXT("A legacy command line syntax is being used for crypto config. Please update to using the -cryptokey parameter as soon as possible as this mode is deprecated"));

			FConfigFile EngineConfig;

			FConfigCacheIni::LoadExternalIniFile(EngineConfig, TEXT("Engine"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
			bool bDataCryptoRequired = false;
			EngineConfig.GetBool(TEXT("PlatformCrypto"), TEXT("PlatformRequiresDataCrypto"), bDataCryptoRequired);

			if (!bDataCryptoRequired)
			{
				return;
			}

			FConfigFile ConfigFile;
			FConfigCacheIni::LoadExternalIniFile(ConfigFile, TEXT("Crypto"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
			bool bSignPak = false;
			bool bEncryptPakIniFiles = false;
			bool bEncryptPakIndex = false;
			bool bEncryptAssets = false;
			bool bEncryptPak = false;

			if (ConfigFile.Num())
			{
				UE_LOG(LogIoStore, Display, TEXT("Using new format crypto.ini files for crypto configuration"));

				static const TCHAR* SectionName = TEXT("/Script/CryptoKeys.CryptoKeysSettings");

				ConfigFile.GetBool(SectionName, TEXT("bEnablePakSigning"), bSignPak);
				ConfigFile.GetBool(SectionName, TEXT("bEncryptPakIniFiles"), bEncryptPakIniFiles);
				ConfigFile.GetBool(SectionName, TEXT("bEncryptPakIndex"), bEncryptPakIndex);
				ConfigFile.GetBool(SectionName, TEXT("bEncryptAssets"), bEncryptAssets);
				bEncryptPak = bEncryptPakIniFiles || bEncryptPakIndex || bEncryptAssets;

				if (bSignPak)
				{
					FString PublicExpBase64, PrivateExpBase64, ModulusBase64;
					ConfigFile.GetString(SectionName, TEXT("SigningPublicExponent"), PublicExpBase64);
					ConfigFile.GetString(SectionName, TEXT("SigningPrivateExponent"), PrivateExpBase64);
					ConfigFile.GetString(SectionName, TEXT("SigningModulus"), ModulusBase64);

					TArray<uint8> PublicExp, PrivateExp, Modulus;
					FBase64::Decode(PublicExpBase64, PublicExp);
					FBase64::Decode(PrivateExpBase64, PrivateExp);
					FBase64::Decode(ModulusBase64, Modulus);

					OutCryptoSettings.SetSigningKey(FRSA::CreateKey(PublicExp, PrivateExp, Modulus));

					UE_LOG(LogIoStore, Display, TEXT("Parsed signature keys from config files."));
				}

				if (bEncryptPak)
				{
					FString EncryptionKeyString;
					ConfigFile.GetString(SectionName, TEXT("EncryptionKey"), EncryptionKeyString);

					if (EncryptionKeyString.Len() > 0)
					{
						TArray<uint8> Key;
						FBase64::Decode(EncryptionKeyString, Key);
						check(Key.Num() == sizeof(FAES::FAESKey::Key));
						FNamedAESKey NewKey;
						NewKey.Name = TEXT("Default");
						NewKey.Guid = FGuid();
						FMemory::Memcpy(NewKey.Key.Key, &Key[0], sizeof(FAES::FAESKey::Key));
						OutCryptoSettings.GetEncryptionKeys().Add(NewKey.Guid, NewKey);
						UE_LOG(LogIoStore, Display, TEXT("Parsed AES encryption key from config files."));
					}
				}
			}
			else
			{
				static const TCHAR* SectionName = TEXT("Core.Encryption");

				UE_LOG(LogIoStore, Display, TEXT("Using old format encryption.ini files for crypto configuration"));

				FConfigCacheIni::LoadExternalIniFile(ConfigFile, TEXT("Encryption"), *FPaths::Combine(EngineDir, TEXT("Config\\")), *FPaths::Combine(ProjectDir, TEXT("Config/")), true, *Platform);
				ConfigFile.GetBool(SectionName, TEXT("SignPak"), bSignPak);
				ConfigFile.GetBool(SectionName, TEXT("EncryptPak"), bEncryptPak);

				if (bSignPak)
				{
					FString RSAPublicExp, RSAPrivateExp, RSAModulus;
					ConfigFile.GetString(SectionName, TEXT("rsa.publicexp"), RSAPublicExp);
					ConfigFile.GetString(SectionName, TEXT("rsa.privateexp"), RSAPrivateExp);
					ConfigFile.GetString(SectionName, TEXT("rsa.modulus"), RSAModulus);

					//TODO: Fix me!
					//OutSigningKey.PrivateKey.Exponent.Parse(RSAPrivateExp);
					//OutSigningKey.PrivateKey.Modulus.Parse(RSAModulus);
					//OutSigningKey.PublicKey.Exponent.Parse(RSAPublicExp);
					//OutSigningKey.PublicKey.Modulus = OutSigningKey.PrivateKey.Modulus;

					UE_LOG(LogIoStore, Display, TEXT("Parsed signature keys from config files."));
				}

				if (bEncryptPak)
				{
					FString EncryptionKeyString;
					ConfigFile.GetString(SectionName, TEXT("aes.key"), EncryptionKeyString);
					FNamedAESKey NewKey;
					NewKey.Name = TEXT("Default");
					NewKey.Guid = FGuid();
					if (EncryptionKeyString.Len() == 32 && TCString<TCHAR>::IsPureAnsi(*EncryptionKeyString))
					{
						for (int32 Index = 0; Index < 32; ++Index)
						{
							NewKey.Key.Key[Index] = (uint8)EncryptionKeyString[Index];
						}
						OutCryptoSettings.GetEncryptionKeys().Add(NewKey.Guid, NewKey);
						UE_LOG(LogIoStore, Display, TEXT("Parsed AES encryption key from config files."));
					}
				}
			}
		}
	}
	else
	{
		UE_LOG(LogIoStore, Display, TEXT("Using command line for crypto configuration"));

		FString EncryptionKeyString;
		FParse::Value(CmdLine, TEXT("aes="), EncryptionKeyString, false);

		if (EncryptionKeyString.Len() > 0)
		{
			UE_LOG(LogIoStore, Warning, TEXT("A legacy command line syntax is being used for crypto config. Please update to using the -cryptokey parameter as soon as possible as this mode is deprecated"));

			FNamedAESKey NewKey;
			NewKey.Name = TEXT("Default");
			NewKey.Guid = FGuid();
			const uint32 RequiredKeyLength = sizeof(NewKey.Key);

			// Error checking
			if (EncryptionKeyString.Len() < RequiredKeyLength)
			{
				UE_LOG(LogIoStore, Fatal, TEXT("AES encryption key must be %d characters long"), RequiredKeyLength);
			}

			if (EncryptionKeyString.Len() > RequiredKeyLength)
			{
				UE_LOG(LogIoStore, Warning, TEXT("AES encryption key is more than %d characters long, so will be truncated!"), RequiredKeyLength);
				EncryptionKeyString.LeftInline(RequiredKeyLength);
			}

			if (!FCString::IsPureAnsi(*EncryptionKeyString))
			{
				UE_LOG(LogIoStore, Fatal, TEXT("AES encryption key must be a pure ANSI string!"));
			}

			const auto AsAnsi = StringCast<ANSICHAR>(*EncryptionKeyString);
			check(AsAnsi.Length() == RequiredKeyLength);
			FMemory::Memcpy(NewKey.Key.Key, AsAnsi.Get(), RequiredKeyLength);
			OutCryptoSettings.GetEncryptionKeys().Add(NewKey.Guid, NewKey);
			UE_LOG(LogIoStore, Display, TEXT("Parsed AES encryption key from command line."));
		}
	}

	FString EncryptionKeyOverrideGuidString;
	FGuid EncryptionKeyOverrideGuid;
	if (FParse::Value(CmdLine, TEXT("EncryptionKeyOverrideGuid="), EncryptionKeyOverrideGuidString))
	{
		UE_LOG(LogIoStore, Display, TEXT("Using encryption key override '%s'"), *EncryptionKeyOverrideGuidString);
		FGuid::Parse(EncryptionKeyOverrideGuidString, EncryptionKeyOverrideGuid);
	}
	OutCryptoSettings.SetPrincipalEncryptionKey(OutCryptoSettings.GetEncryptionKeys().Find(EncryptionKeyOverrideGuid));
}

struct FContainerSourceFile 
{
	FString NormalizedPath;
	FString DestinationPath;
	bool bNeedsCompression = false;
	bool bNeedsEncryption = false;
};

struct FContainerSourceSpec
{
	FName Name;
	FString OutputPath;
	FString OptionalOutputPath;
	FString StageLooseFileRootPath;
	TArray<FContainerSourceFile> SourceFiles;
	FString PatchTargetFile;
	TArray<FString> PatchSourceContainerFiles;
	FString EncryptionKeyOverrideGuid;
	bool bGenerateDiffPatch = false;
	bool bOnDemand = false;
};

struct FCookedFileStatData
{
	enum EFileType
	{
		PackageHeader,
		PackageData,
		BulkData,
		OptionalBulkData,
		MemoryMappedBulkData,
		ShaderLibrary,
		Regions,
		OptionalSegmentPackageHeader,
		OptionalSegmentPackageData,
		OptionalSegmentBulkData,
		Invalid
	};

	int64 FileSize = 0;
	EFileType FileType = Invalid;
};

static int32 GetFullExtensionStartIndex(FStringView Path)
{
	int32 ExtensionStartIndex = -1;
	for (int32 Index = Path.Len() - 1; Index >= 0; --Index)
	{
		if (FPathViews::IsSeparator(Path[Index]))
		{
			break;
		}
		else if (Path[Index] == '.')
		{
			ExtensionStartIndex = Index;
		}
	}
	return ExtensionStartIndex;
}

static FStringView GetBaseFilenameWithoutAnyExtension(FStringView Path)
{
	int32 ExtensionStartIndex = GetFullExtensionStartIndex(Path);
	if (ExtensionStartIndex < 0)
	{
		return FStringView();
	}
	else
	{
		return Path.Left(ExtensionStartIndex);
	}
}

static FStringView GetFullExtension(FStringView Path)
{
	int32 ExtensionStartIndex = GetFullExtensionStartIndex(Path);
	if (ExtensionStartIndex < 0)
	{
		return FStringView();
	}
	else
	{
		return Path.RightChop(ExtensionStartIndex);
	}
}

class FCookedFileStatMap
{
public:
	FCookedFileStatMap()
	{
		Extensions.Emplace(TEXT(".umap"), FCookedFileStatData::PackageHeader);
		Extensions.Emplace(TEXT(".uasset"), FCookedFileStatData::PackageHeader);
		Extensions.Emplace(TEXT(".uexp"), FCookedFileStatData::PackageData);
		Extensions.Emplace(TEXT(".ubulk"), FCookedFileStatData::BulkData);
		Extensions.Emplace(TEXT(".uptnl"), FCookedFileStatData::OptionalBulkData);
		Extensions.Emplace(TEXT(".m.ubulk"), FCookedFileStatData::MemoryMappedBulkData);
		Extensions.Emplace(*FString::Printf(TEXT(".uexp%s"), FFileRegion::RegionsFileExtension), FCookedFileStatData::Regions);
		Extensions.Emplace(*FString::Printf(TEXT(".uptnl%s"), FFileRegion::RegionsFileExtension), FCookedFileStatData::Regions);
		Extensions.Emplace(*FString::Printf(TEXT(".ubulk%s"), FFileRegion::RegionsFileExtension), FCookedFileStatData::Regions);
		Extensions.Emplace(*FString::Printf(TEXT(".m.ubulk%s"), FFileRegion::RegionsFileExtension), FCookedFileStatData::Regions);
		Extensions.Emplace(TEXT(".ushaderbytecode"), FCookedFileStatData::ShaderLibrary);
		Extensions.Emplace(TEXT(".o.umap"), FCookedFileStatData::OptionalSegmentPackageHeader);
		Extensions.Emplace(TEXT(".o.uasset"), FCookedFileStatData::OptionalSegmentPackageHeader);
		Extensions.Emplace(TEXT(".o.uexp"), FCookedFileStatData::OptionalSegmentPackageData);
		Extensions.Emplace(TEXT(".o.ubulk"), FCookedFileStatData::OptionalSegmentBulkData);
	}

	int32 Num() const
	{
		return Map.Num();
	}

	void Add(const TCHAR* Path, int64 FileSize)
	{
		FString NormalizedPath(Path);
		FPaths::NormalizeFilename(NormalizedPath);

		FStringView NormalizePathView(NormalizedPath);
		int32 ExtensionStartIndex = GetFullExtensionStartIndex(NormalizePathView);
		if (ExtensionStartIndex < 0)
		{
			return;
		}
		FStringView Extension = NormalizePathView.RightChop(ExtensionStartIndex);
		const FCookedFileStatData::EFileType* FindFileType = FindFileTypeFromExtension(Extension);
		if (!FindFileType)
		{
			return;
		}

		FCookedFileStatData& CookedFileStatData = Map.Add(MoveTemp(NormalizedPath));
		CookedFileStatData.FileType = *FindFileType;
		CookedFileStatData.FileSize = FileSize;
	}

	const FCookedFileStatData* Find(FStringView NormalizedPath) const
	{
		return Map.FindByHash(GetTypeHash(NormalizedPath), NormalizedPath);
	}

private:
	const FCookedFileStatData::EFileType* FindFileTypeFromExtension(const FStringView& Extension)
	{
		for (const auto& Pair : Extensions)
		{
			if (Pair.Key == Extension)
			{
				return &Pair.Value;
			}
		}
		return nullptr;
	}

	TArray<TTuple<FString, FCookedFileStatData::EFileType>> Extensions;
	TMap<FString, FCookedFileStatData> Map;
};

struct FContainerTargetSpec;

struct FShaderInfo
{
	enum EShaderType { Normal, Global, Inline };

	FIoChunkId ChunkId;
	FIoBuffer CodeIoBuffer;
	// the smaller the order, the more likely this will be loaded
	uint32 LoadOrderFactor = MAX_uint32;
	TSet<struct FCookedPackage*> ReferencedByPackages;
	TMap<FContainerTargetSpec*, EShaderType> TypeInContainer;

	// This is used to ensure build determinism and such must be stable
	// across builds.
	static bool Sort(const FShaderInfo* A, const FShaderInfo* B)
	{
		if (A->LoadOrderFactor == B->LoadOrderFactor)
		{
			// Shader chunk IDs are the hash of the shader so this is consistent across builds.
			return FMemory::Memcmp(A->ChunkId.GetData(), B->ChunkId.GetData(), A->ChunkId.GetSize()) < 0;
		}
		return A->LoadOrderFactor < B->LoadOrderFactor;
	}
};

struct FCookedPackage
{
	FPackageId GlobalPackageId;
	FName PackageName;
	FName SourcePackageName; // Source package name for redirected package
	TArray<FShaderInfo*> Shaders;
	TArray<FSHAHash> ShaderMapHashes;
	uint64 UAssetSize = 0;
	uint64 OptionalSegmentUAssetSize = 0;
	uint64 UExpSize = 0;
	uint64 TotalBulkDataSize = 0;
	uint64 LoadOrder = MAX_uint64; // Ordered by dependencies
	uint64 DiskLayoutOrder = MAX_uint64; // Final order after considering input order files
	FPackageStoreEntryResource PackageStoreEntry;
	int32 PreOrderNumber = -1; // Used for sorting in load order
	bool bPermanentMark = false; // Used for sorting in load order
	bool bIsLocalized = false;
};

struct FLegacyCookedPackage
	: public FCookedPackage
{
	FString FileName;
	FString OptionalSegmentFileName;
	FPackageStorePackage* OptimizedPackage = nullptr;
	FPackageStorePackage* OptimizedOptionalSegmentPackage = nullptr;
};

enum class EContainerChunkType
{
	ShaderCodeLibrary,
	ShaderCode,
	PackageData,
	BulkData,
	OptionalBulkData,
	MemoryMappedBulkData,
	OptionalSegmentPackageData,
	OptionalSegmentBulkData,
	Invalid
};

struct FContainerTargetFile
{
	FContainerTargetSpec* ContainerTarget = nullptr;
	FCookedPackage* Package = nullptr;
	FString NormalizedSourcePath;
	TOptional<FIoBuffer> SourceBuffer;
	FString DestinationPath;
	uint64 SourceSize = 0;
	uint64 IdealOrder = 0;
	FIoChunkId ChunkId;
	TArray<uint8> PackageHeaderData;
	EContainerChunkType ChunkType;
	bool bForceUncompressed = false;

	TArray<FFileRegion> FileRegions;
};

class FCookedPackageStore
{
public:
	struct FChunkInfo
	{
		FIoChunkId ChunkId;
		FName PackageName;
		FString RelativeFileName;
		TArray<FFileRegion> FileRegions;
	};

	FCookedPackageStore(const FString& InCookedDir)
		: CookedDir(InCookedDir)
	{
	}

	FIoStatus Load(const TCHAR* ManifestFilename)
	{
		IOSTORE_CPU_SCOPE(LoadCookedPackageStore);

		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(ManifestFilename));
		if (!Ar)
		{
			return FIoStatus(EIoErrorCode::NotFound);
		}
		FCbObject ManifestObject = LoadCompactBinary(*Ar).AsObject();
		FCbObject OplogObject;
		if (FCbFieldView ZenServerField = ManifestObject["zenserver"])
		{
			UE::Zen::FServiceSettings ZenServiceSettings;
			ZenServiceSettings.ReadFromCompactBinary(ZenServerField["settings"]);
			FString ProjectId = FString(ZenServerField["projectid"].AsString());
			FString OplogId = FString(ZenServerField["oplogid"].AsString());

			ZenStoreClient = MakeUnique<UE::FZenStoreHttpClient>(MoveTemp(ZenServiceSettings));
			ZenStoreClient->InitializeReadOnly(ProjectId, OplogId);

			TIoStatusOr<FCbObject> OplogStatus = ZenStoreClient->GetOplog().Get();
			if (!OplogStatus.IsOk())
			{
				return OplogStatus.Status();
			}
			
			OplogObject = OplogStatus.ConsumeValueOrDie();
		}
		else
		{
			OplogObject = ManifestObject["oplog"].AsObject();
		}

		for (FCbField& OplogEntry : OplogObject["entries"].AsArray())
		{
			FCbObject OplogObj = OplogEntry.AsObject();
			FPackageStoreEntryResource PackageStoreEntry = FPackageStoreEntryResource::FromCbObject(OplogObj["packagestoreentry"].AsObject());

			auto AddChunksFromOplog = [this, &OplogEntry, &PackageStoreEntry](const char* Field)
			{
				for (FCbField& ChunkEntry : OplogEntry[Field].AsArray())
				{
					FCbObject ChunkObj = ChunkEntry.AsObject();
					FIoChunkId ChunkId;
					ChunkId.Set(ChunkObj["id"].AsObjectId().GetView());
					FChunkInfo& ChunkInfo = ChunkInfoMap.Add(ChunkId);
					ChunkInfo.ChunkId = ChunkId;
					ChunkInfo.PackageName = PackageStoreEntry.PackageName;
					if (ChunkObj["filename"])
					{
						TStringBuilder<1024> RelativeFilename;
						RelativeFilename.Append(ChunkObj["filename"].AsString());
						ChunkInfo.RelativeFileName = RelativeFilename;
						TStringBuilder<1024> PathBuilder;
						FPathViews::AppendPath(PathBuilder, CookedDir);
						FPathViews::AppendPath(PathBuilder, RelativeFilename);
						FPathViews::NormalizeFilename(PathBuilder);
						FilenameToChunkIdMap.Add(*PathBuilder, ChunkId);
					}
					const FCbArrayView RegionsArray = ChunkObj["fileregions"].AsArrayView();
					ChunkInfo.FileRegions.Reserve(RegionsArray.Num());
					for (FCbFieldView RegionObj : RegionsArray)
					{
						FFileRegion& Region = ChunkInfo.FileRegions.AddDefaulted_GetRef();
						FFileRegion::LoadFromCompactBinary(RegionObj, Region);
					}
				}
			};

			AddChunksFromOplog("packagedata");
			AddChunksFromOplog("bulkdata");

			PackageIdToEntry.Add(PackageStoreEntry.GetPackageId(), MoveTemp(PackageStoreEntry));
		}
		return FIoStatus::Ok;
	}

	FIoChunkId GetChunkIdFromFileName(const FString& Filename) const
	{
		return FilenameToChunkIdMap.FindRef(*Filename);
	}

	const FChunkInfo* GetChunkInfoFromFileName(const FString& Filename) const
	{
		FIoChunkId ChunkId = GetChunkIdFromFileName(Filename);
		return ChunkInfoMap.Find(ChunkId);
	}

	FString GetRelativeFilenameFromChunkId(const FIoChunkId& ChunkId) const
	{
		const FChunkInfo* FindChunkInfo = ChunkInfoMap.Find(ChunkId);
		if (!FindChunkInfo)
		{
			return FString();
		}
		return FindChunkInfo->RelativeFileName;
	}

	FName GetPackageNameFromChunkId(const FIoChunkId& ChunkId) const
	{
		const FChunkInfo* FindChunkInfo = ChunkInfoMap.Find(ChunkId);
		if (!FindChunkInfo)
		{
			return NAME_None;
		}
		return FindChunkInfo->PackageName;
	}

	FName GetPackageNameFromFileName(const FString& Filename) const
	{
		FIoChunkId ChunkId = GetChunkIdFromFileName(Filename);
		return GetPackageNameFromChunkId(ChunkId);
	}

	const FPackageStoreEntryResource* GetPackageStoreEntry(FPackageId PackageId) const
	{
		return PackageIdToEntry.Find(PackageId);
	}

	bool HasZenStoreClient() const
	{
		return ZenStoreClient.IsValid();
	}

	TFuture<TIoStatusOr<FCbObject>> GetChunkInfos()
	{
		return ZenStoreClient->GetChunkInfos();
	}

	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId)
	{
		return ZenStoreClient->GetChunkSize(ChunkId);
	}

	TIoStatusOr<FIoBuffer> ReadChunk(const FIoChunkId& ChunkId)
	{
		FIoReadOptions ReadOptions;
		return ZenStoreClient->ReadChunk(ChunkId, ReadOptions.GetOffset(), ReadOptions.GetSize());
	}

	void ReadChunkAsync(const FIoChunkId& ChunkId, TFunction<void(TIoStatusOr<FIoBuffer>)>&& Callback)
	{
		class FReadChunkTask
			: public FNonAbandonableTask
		{
		public:
			FReadChunkTask(UE::FZenStoreHttpClient* InZenStoreClient, const FIoChunkId& InChunkId, TFunction<void(TIoStatusOr<FIoBuffer>)>&& InCallback)
				: ZenStoreClient(InZenStoreClient)
				, ChunkId(InChunkId)
				, Callback(MoveTemp(InCallback))
			{
			}

			void DoWork()
			{
				FIoReadOptions ReadOptions;
				Callback(ZenStoreClient->ReadChunk(ChunkId, ReadOptions.GetOffset(), ReadOptions.GetSize()));
			}

			TStatId GetStatId() const
			{
				return TStatId();
			}

		private:
			UE::FZenStoreHttpClient* ZenStoreClient;
			FIoChunkId ChunkId;
			TFunction<void(TIoStatusOr<FIoBuffer>)> Callback;
		};

		(new FAutoDeleteAsyncTask<FReadChunkTask>(ZenStoreClient.Get(), ChunkId, MoveTemp(Callback)))->StartBackgroundTask();
	}
	
	TIoStatusOr<FIoBuffer> ReadPackageHeaderFromZen(FPackageId PackageId, uint16 ChunkIndex)
	{
		if (const FPackageStoreEntryResource* Entry = PackageIdToEntry.Find(PackageId))
		{
			FIoReadOptions ReadOptions;
			ReadOptions.SetRange(0, 64 << 10);
			
			TIoStatusOr<FIoBuffer> Status = ZenStoreClient->ReadChunk(CreateIoChunkId(PackageId.Value(), ChunkIndex, EIoChunkType::ExportBundleData), ReadOptions.GetOffset(), ReadOptions.GetSize());
			if (!Status.IsOk())
			{
				return Status;
			}

			FIoBuffer Buffer = Status.ConsumeValueOrDie();
			uint32 HeaderSize = reinterpret_cast<const FZenPackageSummary*>(Buffer.Data())->HeaderSize;
			if (HeaderSize > Buffer.DataSize())
			{
				ReadOptions.SetRange(0, HeaderSize);

				Status = ZenStoreClient->ReadChunk(CreateIoChunkId(PackageId.Value(), ChunkIndex, EIoChunkType::ExportBundleData), ReadOptions.GetOffset(), ReadOptions.GetSize());
				if (!Status.IsOk())
				{
					return Status;
				}
				Buffer = Status.ConsumeValueOrDie();
			}
			
			return FIoBuffer(Buffer.Data(), HeaderSize, Buffer);
		}

		return FIoStatus(EIoErrorCode::NotFound);
	}

private:
	TUniquePtr<UE::FZenStoreHttpClient> ZenStoreClient;
	FString CookedDir;
	TMap<FPackageId, FPackageStoreEntryResource> PackageIdToEntry;
	TMap<FString, FIoChunkId> FilenameToChunkIdMap;
	TMap<FIoChunkId, FChunkInfo> ChunkInfoMap;
};

struct FFileOrderMap
{
	TMap<FName, int64> PackageNameToOrder;
	FString Name;
	int32 Priority;
	int32 Index;

	FFileOrderMap(int32 InPriority, int32 InIndex)
		: Priority(InPriority)
		, Index(InIndex)
	{
	}
};

struct FIoStoreArguments
{
	FString GlobalContainerPath;
	FString CookedDir;
	TArray<FContainerSourceSpec> Containers;
	FCookedFileStatMap CookedFileStatMap;
	TArray<FFileOrderMap> OrderMaps;
	FKeyChain KeyChain;
	FKeyChain PatchKeyChain;
	FString DLCPluginPath;
	FString DLCName;
	FString ReferenceChunkGlobalContainerFileName;
	FString CsvPath;
	FKeyChain ReferenceChunkKeys;
	FReleasedPackages ReleasedPackages;
	TUniquePtr<FCookedPackageStore> PackageStore;
	TUniquePtr<FIoBuffer> ScriptObjects;
	bool bVerifyHashDatabase = false;
	bool bSign = false;
	bool bRemapPluginContentToGame = false;
	bool bCreateDirectoryIndex = true;
	bool bClusterByOrderFilePriority = false;
	bool bFileRegions = false;
	bool bUpload = false;
	EAssetRegistryWritebackMethod WriteBackMetadataToAssetRegistry = EAssetRegistryWritebackMethod::Disabled;
	bool bWritePluginSizeSummaryJsons = false; // Only valid if WriteBackMetadataToAssetRegistry != Disabled.

	FOodleDataCompression::ECompressor ShaderOodleCompressor = FOodleDataCompression::ECompressor::Mermaid;
	FOodleDataCompression::ECompressionLevel ShaderOodleLevel = FOodleDataCompression::ECompressionLevel::Normal;

	bool IsDLC() const
	{
		return DLCPluginPath.Len() > 0;
	}
};

struct FContainerTargetSpec
{
	FIoContainerId ContainerId;
	FIoContainerId OptionalSegmentContainerId;
	FIoContainerHeader Header;
	FIoContainerHeader OptionalSegmentHeader;
	FName Name;
	FGuid EncryptionKeyGuid;
	FString OutputPath;
	FString OptionalSegmentOutputPath;
	FString StageLooseFileRootPath;
	TSharedPtr<IIoStoreWriter> IoStoreWriter;
	TSharedPtr<IIoStoreWriter> OptionalSegmentIoStoreWriter;
	TArray<FContainerTargetFile> TargetFiles;
	TArray<TUniquePtr<FIoStoreReader>> PatchSourceReaders;
	EIoContainerFlags ContainerFlags = EIoContainerFlags::None;
	TArray<FCookedPackage*> Packages;
	TSet<FShaderInfo*> GlobalShaders;
	TSet<FShaderInfo*> SharedShaders;
	TSet<FShaderInfo*> UniqueShaders;
	TSet<FShaderInfo*> InlineShaders;
	bool bGenerateDiffPatch = false;
};

using FPackageNameMap = TMap<FName, FCookedPackage*>;
using FPackageIdMap = TMap<FPackageId, FCookedPackage*>;

class FChunkEntryCsv
{
public:

	~FChunkEntryCsv()
	{
		if (OutputArchive.IsValid())
		{
			OutputArchive->Flush();
		}
	}

	void CreateOutputFile(const TCHAR* OutputFilename)
	{
		OutputArchive.Reset(IFileManager::Get().CreateFileWriter(OutputFilename));
		if (OutputArchive.IsValid())
		{
			OutputArchive->Logf(TEXT("OrderInContainer, ChunkId, PackageId, PackageName, Filename, ContainerName, Offset, OffsetOnDisk, Size, CompressedSize, Hash, ChunkType"));
		}
	}

	void AddChunk(const FString& ContainerName, int32 Index, const FIoStoreTocChunkInfo& Info, FPackageId PackageId, const FString& PackageName)
	{
		if (OutputArchive.IsValid())
		{
			OutputArchive->Logf(TEXT("%d, %s, 0x%llX, %s, %s, %s, %lld, %lld, %lld, %lld, 0x%s, %s"),
				Index,
				*BytesToHex(Info.Id.GetData(), Info.Id.GetSize()),
				PackageId.ValueForDebugging(),
				*PackageName,
				*Info.FileName,
				*ContainerName,
				Info.Offset,
				Info.OffsetOnDisk,
				Info.Size,
				Info.CompressedSize,
				*Info.Hash.ToString(),
				*LexToString(Info.ChunkType)
			);
		}
	}	

private:
	TUniquePtr<FArchive> OutputArchive;
};

void SortPackagesInLoadOrderRecursive(TArray<FCookedPackage*>& Result, FCookedPackage* Package, TArray<FCookedPackage*>& S, TArray<FCookedPackage*>& P, int32& C, const TMap<FCookedPackage*, TArray<FCookedPackage*>>& ReverseEdges)
{
	Package->PreOrderNumber = C;
	++C;
	S.Push(Package);
	P.Push(Package);
	const TArray<FCookedPackage*>* FindParents = ReverseEdges.Find(Package);
	if (FindParents)
	{
		for (FCookedPackage* Parent : *FindParents)
		{
			if (!Parent->bPermanentMark)
			{
				if (Parent->PreOrderNumber < 0)
				{
					SortPackagesInLoadOrderRecursive(Result, Parent, S, P, C, ReverseEdges);
				}
				else
				{
					while (P.Top()->PreOrderNumber > Parent->PreOrderNumber)
					{
						P.Pop();
					}
				}
			}
		}
	}
	if (P.Top() == Package)
	{
		FCookedPackage* InStronglyConnectedComponent;
		do
		{
			InStronglyConnectedComponent = S.Top();
			S.Pop();
			InStronglyConnectedComponent->bPermanentMark = true;
			Result.Add(InStronglyConnectedComponent);
		} while (InStronglyConnectedComponent != Package);
		P.Pop();
	}
}

void SortPackagesInLoadOrder(TArray<FCookedPackage*>& Packages, const TMap<FPackageId, FCookedPackage*>& PackagesMap)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SortPackagesInLoadOrder);
	Algo::Sort(Packages, [](const FCookedPackage* A, const FCookedPackage* B)
		{
			return A->GlobalPackageId < B->GlobalPackageId;
		});

	TMap<FCookedPackage*, TArray<FCookedPackage*>> ReverseEdges;
	for (FCookedPackage* Package : Packages)
	{
		for (FPackageId ImportedPackageId : Package->PackageStoreEntry.ImportedPackageIds)
		{
			FCookedPackage* FindImportedPackage = PackagesMap.FindRef(ImportedPackageId);
			if (FindImportedPackage)
			{
				TArray<FCookedPackage*>& SourceArray = ReverseEdges.FindOrAdd(FindImportedPackage);
				SourceArray.Add(Package);
			}
		}
	}
	for (auto& KV : ReverseEdges)
	{
		TArray<FCookedPackage*>& SourceArray = KV.Value;
		Algo::Sort(SourceArray, [](const FCookedPackage* A, const FCookedPackage* B)
			{
				return A->GlobalPackageId < B->GlobalPackageId;
			});
	}

	// Path based strongly connected components + topological sort of the components
	TArray<FCookedPackage*> Result;
	Result.Reserve(Packages.Num());
	TArray<int32> PackagePreOrderNumbers;
	TArray<FCookedPackage*> S;
	TArray<FCookedPackage*> P;
	for (FCookedPackage* Package : Packages)
	{
		if (!Package->bPermanentMark)
		{
			S.Reset();
			P.Reset();
			int32 C = 0;
			SortPackagesInLoadOrderRecursive(Result, Package, S, P, C, ReverseEdges);
		}
	}
	check(Result.Num() == Packages.Num());
	Algo::Reverse(Result);
	Swap(Packages, Result);
	uint64 LoadOrder = 0;
	for (FCookedPackage* Package : Packages)
	{
		Package->LoadOrder = LoadOrder++;
	}
}

class FClusterStatsCsv
{
public:

	~FClusterStatsCsv()
	{
		if (OutputArchive)
		{
			OutputArchive->Flush();
		}
	}

	void CreateOutputFile(const FString& Path)
	{
		OutputArchive.Reset(IFileManager::Get().CreateFileWriter(*Path));
		if (OutputArchive)
		{
			OutputArchive->Logf(TEXT("PackageName,ClusterUExpBytes,BytesToRead,ClustersToRead,ClusterOwner,OrderFile,OrderIndex"));
		}
	}

	void AddPackage(FName PackageName, int64 ClusterUExpBytes, int64 DepUExpBytes, uint32 NumTouchedClusters, FName ClusterOwner, const FFileOrderMap* BlameOrderMap, uint64 LocalOrder)
	{
		if (!OutputArchive.IsValid())
		{
			return;
		}
		OutputArchive->Logf(TEXT("%s,%lld,%lld,%u,%s,%s,%llu"),
			*PackageName.ToString(),
			ClusterUExpBytes,
			DepUExpBytes,
			NumTouchedClusters,
			*ClusterOwner.ToString(),
			BlameOrderMap ? *BlameOrderMap->Name : TEXT("None"),
			LocalOrder
		);
	}

	void Close()
	{
		OutputArchive.Reset();
	}

private:
	TUniquePtr<FArchive> OutputArchive;
};
FClusterStatsCsv ClusterStatsCsv;


// If bClusterByOrderFilePriority is false
//		Order packages by the order of OrderMaps with their associated priority 
//		e.g. Order map 1 with priority 0 (A, B, C) 
//			 Order map 2 with priority 10 (B, D)
//			Final order: (A, C, B, D)
//		Then cluster packages in this order. 
// If bClusterByOrderFilePriority is true
//		Cluster packages first by OrderMaps in priority order, then concatenate clusters in the array order of OrderMaps.
//		e.g. Order map 1 with priority 0 (A, B, C) 
//			 Order map 2 with priority 10 (B, D)
//			Cluster packages B, D, then A, C
//			Then reassemble clusters A, C, B, D
static void AssignPackagesDiskOrder(
	const TArray<FCookedPackage*>& Packages,
	const TArray<FFileOrderMap>& OrderMaps,
	const FPackageIdMap& PackageIdMap,
	bool bClusterByOrderFilePriority
	)
{
	IOSTORE_CPU_SCOPE(AssignPackagesDiskOrder);

	struct FCluster
	{
		TArray<FCookedPackage*> Packages;
		int32 OrderFileIndex; // Index in OrderMaps of the FFileOrderMap which contained Packages.Last()
		int32 ClusterSequence;

		FCluster(int32 InOrderFileIndex, int32 InClusterSequence)
			: OrderFileIndex(InOrderFileIndex)
			, ClusterSequence(InClusterSequence)
		{
		}
	};

	TArray<FCluster*> Clusters;
	TSet<FCookedPackage*> AssignedPackages;
	TArray<FCookedPackage*> ProcessStack;

	struct FPackageAndOrder
	{
		FCookedPackage* Package = nullptr;
		int64 LocalOrder;
		const FFileOrderMap* OrderMap;

		FPackageAndOrder(FCookedPackage* InPackage, int64 InLocalOrder, const FFileOrderMap* InOrderMap)
			: Package(InPackage)
			, LocalOrder(InLocalOrder)
			, OrderMap(InOrderMap)
		{
			check(OrderMap);
		}
	};

	// Order maps sorted by priority
	TArray<const FFileOrderMap*> PriorityOrderMaps;
	for (const FFileOrderMap& Map : OrderMaps)
	{
		PriorityOrderMaps.Add(&Map);
	}
	Algo::StableSortBy(PriorityOrderMaps, [](const FFileOrderMap* Map) { return Map->Priority; }, TGreater<int32>());

	// Create a fallback order map to avoid null checks later
	// Lowest priority, last index
	FFileOrderMap FallbackOrderMap(MIN_int32, MAX_int32);
	FallbackOrderMap.Name = TEXT("Fallback");

	TArray<FPackageAndOrder> SortedPackages;
	SortedPackages.Reserve(Packages.Num());
	for (FCookedPackage* Package : Packages)
	{
		// Default to the fallback order map 
		// Reverse the bundle load order for the fallback map (so that packages are considered before their imports)
		const FFileOrderMap* UsedOrderMap = &FallbackOrderMap;
		int64 LocalOrder = -int64(Package->LoadOrder);

		for (const FFileOrderMap* OrderMap : PriorityOrderMaps)
		{
			if (const int64* Order = OrderMap->PackageNameToOrder.Find(Package->PackageName))
			{
				LocalOrder = *Order;
				UsedOrderMap = OrderMap;
				break;
			}
		}

		SortedPackages.Emplace(Package, LocalOrder, UsedOrderMap);
	}
	const FFileOrderMap* LastBlameOrderMap = nullptr;
	int32 LastAssignedCount = 0;
	int64 LastAssignedUExpSize = 0, AssignedUExpSize = 0;
	int64 LastAssignedBulkSize = 0, AssignedBulkSize = 0;

	if (bClusterByOrderFilePriority)
	{
		// Sort by priority of the order map
		Algo::Sort(SortedPackages, [](const FPackageAndOrder& A, const FPackageAndOrder& B) {
			// Packages in the same order map should be sorted by their local ordering
			if (A.OrderMap == B.OrderMap)
			{
				return A.LocalOrder < B.LocalOrder;
			}

			// First priority, then index
			if (A.OrderMap->Priority != B.OrderMap->Priority)
			{
				return A.OrderMap->Priority > B.OrderMap->Priority;
			}

			check(A.OrderMap->Index != B.OrderMap->Index);
			return A.OrderMap->Index < B.OrderMap->Index;
		});
	}
	else
	{
		// Sort by the order of the order map (...)
		Algo::Sort(SortedPackages, [](const FPackageAndOrder& A, const FPackageAndOrder& B) {
			// Packages in the same order map should be sorted by their local ordering
			if (A.OrderMap == B.OrderMap)
			{
				return A.LocalOrder < B.LocalOrder;
			}

			// Blame order priority is not considered for the order in which we cluster packages, only for the order in which we assign packages to an order map
			return A.OrderMap->Index < B.OrderMap->Index;
		});
	}

	// Keep these containers outside of inner loops to reuse allocated memory across iterations
	// No need to allocate & free them on every single iteration, just reset element count before using them
	TSet<FCluster*> ClustersToRead;
	TSet<FCookedPackage*> VisitedDeps;
	TArray<FCookedPackage*> DepQueue;
	TArray<FCluster*> OrderedClustersToRead;

	int32 ClusterSequence = 0;
	TMap<FCookedPackage*, FCluster*> PackageToCluster;
	for (FPackageAndOrder& Entry : SortedPackages)
	{
		checkSlow(Entry.OrderMap); // Without this, Entry.OrderMap != LastBlameOrderMap convinces static analysis that Entry.OrderMap may be null
		if (Entry.OrderMap != LastBlameOrderMap)
		{
			if( LastBlameOrderMap != nullptr )
			{
				UE_LOG(LogIoStore, Display, TEXT("Ordered %d/%d packages using order file %s. %.2fMB UExp data. %.2fmb bulk data."), 
				AssignedPackages.Num() - LastAssignedCount, Packages.Num(), *LastBlameOrderMap->Name,
				(AssignedUExpSize - LastAssignedUExpSize) / 1024.0 / 1024.0,
				(AssignedBulkSize - LastAssignedBulkSize) / 1024.0 / 1024.0
				 );
			}
			LastAssignedCount = AssignedPackages.Num();
			LastAssignedUExpSize= AssignedUExpSize;
			LastAssignedBulkSize = AssignedBulkSize;
			LastBlameOrderMap = Entry.OrderMap;
		}
		if (!AssignedPackages.Contains(Entry.Package))
		{
			FCluster* Cluster = new FCluster(Entry.OrderMap->Index, ClusterSequence++);
			Clusters.Add(Cluster);
			ProcessStack.Push(Entry.Package);

			int64 ClusterBytes = 0;
			while (ProcessStack.Num())
			{
				FCookedPackage* PackageToProcess = ProcessStack.Pop(EAllowShrinking::No);
				if (!AssignedPackages.Contains(PackageToProcess))
				{
					AssignedPackages.Add(PackageToProcess);
					Cluster->Packages.Add(PackageToProcess);
					PackageToCluster.Add(PackageToProcess, Cluster);
					ClusterBytes += PackageToProcess->UExpSize;
					AssignedUExpSize += PackageToProcess->UExpSize;
					AssignedBulkSize += PackageToProcess->TotalBulkDataSize;
					
					TArray<FPackageId> AllReferencedPackageIds;
					AllReferencedPackageIds.Append(PackageToProcess->PackageStoreEntry.ImportedPackageIds);
					for (const FPackageId& ReferencedPackageId : AllReferencedPackageIds)
					{
						FCookedPackage* FindReferencedPackage = PackageIdMap.FindRef(ReferencedPackageId);
						if (FindReferencedPackage)
						{
							ProcessStack.Push(FindReferencedPackage);
						}
					}
				}
			}

			for (FCookedPackage* Package : Cluster->Packages)
			{
				int64 BytesToRead = 0;

				ClustersToRead.Reset();
				VisitedDeps.Reset();
				DepQueue.Reset();

				DepQueue.Push(Package);
				while (DepQueue.Num() > 0)
				{
					FCookedPackage* Cursor = DepQueue.Pop();
					if( VisitedDeps.Contains(Cursor) == false)
					{
						VisitedDeps.Add(Cursor);
						BytesToRead += Cursor->UExpSize;
						if (FCluster* ReadCluster = PackageToCluster.FindRef(Cursor))
						{
							ClustersToRead.Add(ReadCluster);
						}
						
						for (const FPackageId& ImportedPackageId : Cursor->PackageStoreEntry.ImportedPackageIds)
						{
							FCookedPackage* FindReferencedPackage = PackageIdMap.FindRef(ImportedPackageId);
							if (FindReferencedPackage)
							{
								DepQueue.Push(FindReferencedPackage);
							}
						}
					}
				}

				OrderedClustersToRead.Reset(ClustersToRead.Num());
				for (FCluster* ClusterToRead : ClustersToRead)
				{
					OrderedClustersToRead.Add(ClusterToRead);
				}
				Algo::SortBy(OrderedClustersToRead, [](FCluster* C) { return C->ClusterSequence; }, TLess<int32>());

				int32 NumClustersToRead = 1; // Could replace with "min seeks"
				for (int32 i = 1; i < OrderedClustersToRead.Num(); ++i)
				{
					if (OrderedClustersToRead[i]->ClusterSequence != OrderedClustersToRead[i - 1]->ClusterSequence + 1)
					{
						++NumClustersToRead;
					}
				}

				FName ClusterOwner = Entry.Package->PackageName;
				ClusterStatsCsv.AddPackage(Package->PackageName, Package == Entry.Package ? ClusterBytes : 0, BytesToRead, NumClustersToRead, ClusterOwner, Entry.OrderMap, Entry.LocalOrder);
			}
		}
	}
	UE_LOG(LogIoStore, Display, TEXT("Ordered %d packages using fallback bundle order"), AssignedPackages.Num() - LastAssignedCount);

	check(AssignedPackages.Num() == Packages.Num());
	
	if (bClusterByOrderFilePriority)
	{
		Algo::StableSortBy(Clusters, [](FCluster* Cluster) { return Cluster->OrderFileIndex; });
	}
	
	for (FCluster* Cluster : Clusters)
	{
		Algo::Sort(Cluster->Packages, [](const FCookedPackage* A, const FCookedPackage* B)
		{
			return A->LoadOrder < B->LoadOrder;
		});
	}

	uint64 LayoutIndex = 0;
	for (FCluster* Cluster : Clusters)
	{
		for (FCookedPackage* Package : Cluster->Packages)
		{
			Package->DiskLayoutOrder = LayoutIndex++;
		}
		delete Cluster;
	}
	
	ClusterStatsCsv.Close();
}

static void CreateDiskLayout(
	const TArray<FContainerTargetSpec*>& ContainerTargets,
	const TArray<FCookedPackage*>& Packages,
	const TArray<FFileOrderMap>& OrderMaps,
	const FPackageIdMap& PackageIdMap,
	bool bClusterByOrderFilePriority)
{
	IOSTORE_CPU_SCOPE(CreateDiskLayout);

	AssignPackagesDiskOrder(Packages, OrderMaps, PackageIdMap, bClusterByOrderFilePriority);

	for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		TArray<FContainerTargetFile*> SortedTargetFiles;
		SortedTargetFiles.Reserve(ContainerTarget->TargetFiles.Num());
		TMap<FIoChunkId, FContainerTargetFile*> ShaderTargetFilesMap;
		ShaderTargetFilesMap.Reserve(ContainerTarget->GlobalShaders.Num() + ContainerTarget->SharedShaders.Num() + ContainerTarget->UniqueShaders.Num() + ContainerTarget->InlineShaders.Num());
		for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
		{
			if (TargetFile.ChunkType == EContainerChunkType::ShaderCode)
			{
				ShaderTargetFilesMap.Add(TargetFile.ChunkId, &TargetFile);
			}
			else
			{
				SortedTargetFiles.Add(&TargetFile);
			}
		}
		check(ShaderTargetFilesMap.Num() == ContainerTarget->GlobalShaders.Num() + ContainerTarget->SharedShaders.Num() + ContainerTarget->UniqueShaders.Num() + ContainerTarget->InlineShaders.Num());
		Algo::Sort(SortedTargetFiles, [](const FContainerTargetFile* A, const FContainerTargetFile* B)
		{
			if (A->ChunkType != B->ChunkType)
			{
				return A->ChunkType < B->ChunkType;
			}
			if (A->ChunkType == EContainerChunkType::ShaderCodeLibrary)
			{
				return A->DestinationPath < B->DestinationPath;
			}
			if (A->Package != B->Package)
			{
				return A->Package->DiskLayoutOrder < B->Package->DiskLayoutOrder;
			}
			check(A == B)
			return false;
		});

		int32 Index = 0;
		int32 ShaderCodeInsertionIndex = -1;
		while (Index < SortedTargetFiles.Num())
		{
			FContainerTargetFile* TargetFile = SortedTargetFiles[Index];
			if (ShaderCodeInsertionIndex < 0 && TargetFile->ChunkType != EContainerChunkType::ShaderCodeLibrary)
			{
				ShaderCodeInsertionIndex = Index;
			}
			if (TargetFile->ChunkType == EContainerChunkType::PackageData)
			{
				TArray<FContainerTargetFile*, TInlineAllocator<1024>> PackageInlineShaders;

				// Since we are inserting in to a sorted array (on disk order), we have to be stably sorted
				// beforehand
				Algo::Sort(TargetFile->Package->Shaders, FShaderInfo::Sort);
				for (FShaderInfo* Shader : TargetFile->Package->Shaders)
				{
					check(Shader->ReferencedByPackages.Num() > 0);
					FShaderInfo::EShaderType* ShaderType = Shader->TypeInContainer.Find(ContainerTarget);
					if (ShaderType && *ShaderType == FShaderInfo::Inline)
					{
						check(ContainerTarget->InlineShaders.Contains(Shader));
						FContainerTargetFile* ShaderTargetFile = ShaderTargetFilesMap.FindRef(Shader->ChunkId);
						check(ShaderTargetFile);
						PackageInlineShaders.Add(ShaderTargetFile);
					}
				}
				if (!PackageInlineShaders.IsEmpty())
				{
					SortedTargetFiles.Insert(PackageInlineShaders, Index + 1);
					Index += PackageInlineShaders.Num();
				}
			}
			++Index;
		}
		if (ShaderCodeInsertionIndex < 0)
		{
			ShaderCodeInsertionIndex = 0;
		}

		auto AddShaderTargetFiles =
			[&ShaderTargetFilesMap, &SortedTargetFiles, &ShaderCodeInsertionIndex]
			(TSet<FShaderInfo*>& Shaders)
		{
			if (!Shaders.IsEmpty())
			{
				TArray<FShaderInfo*> SortedShaders = Shaders.Array();
				Algo::Sort(SortedShaders, FShaderInfo::Sort);
				TArray<FContainerTargetFile*> ShaderTargetFiles;
				ShaderTargetFiles.Reserve(SortedShaders.Num());
				for (const FShaderInfo* ShaderInfo : SortedShaders)
				{
					FContainerTargetFile* ShaderTargetFile = ShaderTargetFilesMap.FindRef(ShaderInfo->ChunkId);
					check(ShaderTargetFile);
					ShaderTargetFiles.Add(ShaderTargetFile);
				}
				SortedTargetFiles.Insert(ShaderTargetFiles, ShaderCodeInsertionIndex);
				ShaderCodeInsertionIndex += ShaderTargetFiles.Num();
			}
		};
		AddShaderTargetFiles(ContainerTarget->GlobalShaders);
		AddShaderTargetFiles(ContainerTarget->SharedShaders);
		AddShaderTargetFiles(ContainerTarget->UniqueShaders);

		check(SortedTargetFiles.Num() == ContainerTarget->TargetFiles.Num());

		uint64 IdealOrder = 0;
		for (FContainerTargetFile* TargetFile : SortedTargetFiles)
		{
			TargetFile->IdealOrder = IdealOrder++;
		}
	}
}

FContainerTargetSpec* AddContainer(
	FName Name,
	TArray<FContainerTargetSpec*>& Containers)
{
	FIoContainerId ContainerId = FIoContainerId::FromName(Name);
	for (FContainerTargetSpec* ExistingContainer : Containers)
	{
		if (ExistingContainer->Name == Name)
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Duplicate container name: '%s'"), *Name.ToString());
			return nullptr;
		}
		if (ExistingContainer->ContainerId == ContainerId)
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Hash collision for container names: '%s' and '%s'"), *Name.ToString(), *ExistingContainer->Name.ToString());
			return nullptr;
		}
	}
	
	FContainerTargetSpec* ContainerTargetSpec = new FContainerTargetSpec();
	ContainerTargetSpec->Name = Name;
	ContainerTargetSpec->ContainerId = ContainerId;
	Containers.Add(ContainerTargetSpec);
	return ContainerTargetSpec;
}

FCookedPackage& FindOrAddPackage(
	const FIoStoreArguments& Arguments,
	const FName& PackageName,
	TArray<FCookedPackage*>& Packages,
	FPackageNameMap& PackageNameMap,
	FPackageIdMap& PackageIdMap)
{
	FCookedPackage* Package = PackageNameMap.FindRef(PackageName);
	if (!Package)
	{
		FPackageId PackageId = FPackageId::FromName(PackageName);
		if (FCookedPackage* FindById = PackageIdMap.FindRef(PackageId))
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Package name hash collision \"%s\" and \"%s"), *FindById->PackageName.ToString(), *PackageName.ToString());
		}

		if (const FName* ReleasedPackageName = Arguments.ReleasedPackages.PackageIdToName.Find(PackageId))
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Package name hash collision with base game package \"%s\" and \"%s"), *ReleasedPackageName->ToString(), *PackageName.ToString());
		}

		Package = new FCookedPackage();
		Package->PackageName = PackageName;
		Package->GlobalPackageId = PackageId;
		Packages.Add(Package);
		PackageNameMap.Add(PackageName, Package);
		PackageIdMap.Add(PackageId, Package);
	}

	return *Package;
}

FLegacyCookedPackage* FindOrAddLegacyPackage(
	const FIoStoreArguments& Arguments,
	const TCHAR* FileName,
	TArray<FCookedPackage*>& Packages,
	FPackageNameMap& PackageNameMap,
	FPackageIdMap& PackageIdMap)
{
	FName PackageName = Arguments.PackageStore->GetPackageNameFromFileName(FileName);
	if (PackageName.IsNone())
	{
		return nullptr;
	}

	FCookedPackage* Package = PackageNameMap.FindRef(PackageName);
	if (!Package)
	{
		FPackageId PackageId = FPackageId::FromName(PackageName);
		if (FCookedPackage* FindById = PackageIdMap.FindRef(PackageId))
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Package name hash collision \"%s\" and \"%s"), *FindById->PackageName.ToString(), *PackageName.ToString());
		}

		if (const FName* ReleasedPackageName = Arguments.ReleasedPackages.PackageIdToName.Find(PackageId))
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Package name hash collision with base game package \"%s\" and \"%s"), *ReleasedPackageName->ToString(), *PackageName.ToString());
		}

		Package = new FLegacyCookedPackage();
		Package->PackageName = PackageName;
		Package->GlobalPackageId = PackageId;
		Packages.Add(Package);
		PackageNameMap.Add(PackageName, Package);
		PackageIdMap.Add(PackageId, Package);
	}
	return static_cast<FLegacyCookedPackage*>(Package);
}

static void ParsePackageAssetsFromFiles(TArray<FCookedPackage*>& Packages, const FPackageStoreOptimizer& PackageStoreOptimizer)
{
	IOSTORE_CPU_SCOPE(ParsePackageAssetsFromFiles);
	UE_LOG(LogIoStore, Display, TEXT("Parsing packages..."));

	TAtomic<int32> ReadCount {0};
	TAtomic<int32> ParseCount {0};
	const int32 TotalPackageCount = Packages.Num();

	TArray<FPackageFileSummary> PackageFileSummaries;
	PackageFileSummaries.SetNum(TotalPackageCount);

	TArray<FPackageFileSummary> OptionalSegmentPackageFileSummaries;
	OptionalSegmentPackageFileSummaries.SetNum(TotalPackageCount);

	uint8* UAssetMemory = nullptr;
	uint8* OptionalSegmentUAssetMemory = nullptr;

	TArray<uint8*> PackageAssetBuffers;
	PackageAssetBuffers.SetNum(TotalPackageCount);

	TArray<uint8*> OptionalSegmentPackageAssetBuffers;
	OptionalSegmentPackageAssetBuffers.SetNum(TotalPackageCount);

	UE_LOG(LogIoStore, Display, TEXT("Reading package assets..."));
	{
		IOSTORE_CPU_SCOPE(ReadUAssetFiles);

		uint64 TotalUAssetSize = 0;
		uint64 TotalOptionalSegmentUAssetSize = 0;
		for (const FCookedPackage* Package : Packages)
		{
			const FLegacyCookedPackage* LegacyPackage = static_cast<const FLegacyCookedPackage*>(Package);
			TotalUAssetSize += LegacyPackage->UAssetSize;
			TotalOptionalSegmentUAssetSize += LegacyPackage->OptionalSegmentUAssetSize;
		}
		UAssetMemory = reinterpret_cast<uint8*>(FMemory::Malloc(TotalUAssetSize));
		uint8* UAssetMemoryPtr = UAssetMemory;
		OptionalSegmentUAssetMemory = reinterpret_cast<uint8*>(FMemory::Malloc(TotalOptionalSegmentUAssetSize));
		uint8* OptionalSegmentUAssetMemoryPtr = OptionalSegmentUAssetMemory;

		for (int32 Index = 0; Index < TotalPackageCount; ++Index)
		{
			FLegacyCookedPackage* Package = static_cast<FLegacyCookedPackage*>(Packages[Index]);
			PackageAssetBuffers[Index] = UAssetMemoryPtr;
			UAssetMemoryPtr += Package->UAssetSize;
			OptionalSegmentPackageAssetBuffers[Index] = OptionalSegmentUAssetMemoryPtr;
			OptionalSegmentUAssetMemoryPtr += Package->OptionalSegmentUAssetSize;
		}

		double StartTime = FPlatformTime::Seconds();

		TAtomic<uint64> TotalReadCount{ 0 };
		TAtomic<uint64> CurrentFileIndex{ 0 };
		ParallelFor(TEXT("ReadingPackageAssets.PF"), TotalPackageCount, 1, [&ReadCount, &PackageAssetBuffers, &OptionalSegmentPackageAssetBuffers, &Packages, &CurrentFileIndex, &TotalReadCount](int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ReadUAssetFile);
			FLegacyCookedPackage* Package = static_cast<FLegacyCookedPackage*>(Packages[Index]);
			if (Package->UAssetSize)
			{
				TotalReadCount.IncrementExchange();
				uint8* Buffer = PackageAssetBuffers[Index];
				TUniquePtr<IFileHandle> FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Package->FileName));
				if (FileHandle)
				{
					bool bSuccess = FileHandle->Read(Buffer, Package->UAssetSize);
					UE_CLOG(!bSuccess, LogIoStore, Warning, TEXT("Failed reading file '%s'"), *Package->FileName);
				}
				else
				{
					UE_LOG(LogIoStore, Warning, TEXT("Couldn't open file '%s'"), *Package->FileName);
				}
			}
			if (Package->OptionalSegmentUAssetSize)
			{
				TotalReadCount.IncrementExchange();
				uint8* Buffer = OptionalSegmentPackageAssetBuffers[Index];
				TUniquePtr<IFileHandle> FileHandle(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Package->OptionalSegmentFileName));
				if (FileHandle)
				{
					bool bSuccess = FileHandle->Read(Buffer, Package->OptionalSegmentUAssetSize);
					UE_CLOG(!bSuccess, LogIoStore, Warning, TEXT("Failed reading file '%s'"), *Package->OptionalSegmentFileName);
				}
				else
				{
					UE_LOG(LogIoStore, Warning, TEXT("Couldn't open file '%s'"), *Package->OptionalSegmentFileName);
				}
			}

			uint64 LocalFileIndex = CurrentFileIndex.IncrementExchange() + 1;
			UE_CLOG(LocalFileIndex % 1000 == 0, LogIoStore, Display, TEXT("Reading %d/%d: '%s'"), LocalFileIndex, Packages.Num(), *Package->FileName);
		}, EParallelForFlags::Unbalanced);

		double EndTime = FPlatformTime::Seconds();
		UE_LOG(LogIoStore, Display, TEXT("Packages read %s files in %.2f seconds, %s total bytes, %s bytes per second"), 
			*NumberString(TotalReadCount.Load()), 
			EndTime - StartTime, 
			*NumberString(TotalOptionalSegmentUAssetSize + TotalUAssetSize),
			*NumberString((int64)((TotalOptionalSegmentUAssetSize + TotalUAssetSize) / FMath::Max(.001f, EndTime - StartTime))));
	}

	{
		IOSTORE_CPU_SCOPE(SerializeSummaries);

		ParallelFor(TotalPackageCount, [
				&ReadCount,
				&PackageAssetBuffers,
				&OptionalSegmentPackageAssetBuffers,
				&PackageFileSummaries,
				&Packages,
				&PackageStoreOptimizer](int32 Index)
		{
			FLegacyCookedPackage* Package = static_cast<FLegacyCookedPackage*>(Packages[Index]);

			if (Package->UAssetSize)
			{
				uint8* PackageBuffer = PackageAssetBuffers[Index];
				FIoBuffer CookedHeaderBuffer = FIoBuffer(FIoBuffer::Wrap, PackageBuffer, Package->UAssetSize);
				Package->OptimizedPackage = PackageStoreOptimizer.CreatePackageFromCookedHeader(Package->PackageName, CookedHeaderBuffer);
			}
			else
			{
				UE_LOG(LogIoStore, Display, TEXT("Including package %s without a .uasset file. Excluded by PakFileRules?"), *Package->PackageName.ToString());
				Package->OptimizedPackage = PackageStoreOptimizer.CreateMissingPackage(Package->PackageName);
			}
			check(Package->OptimizedPackage->GetId() == Package->GlobalPackageId);
			if (Package->OptionalSegmentUAssetSize)
			{
				uint8* OptionalSegmentPackageBuffer = OptionalSegmentPackageAssetBuffers[Index];
				FIoBuffer OptionalSegmentCookedHeaderBuffer = FIoBuffer(FIoBuffer::Wrap, OptionalSegmentPackageBuffer, Package->OptionalSegmentUAssetSize);
				Package->OptimizedOptionalSegmentPackage = PackageStoreOptimizer.CreatePackageFromCookedHeader(Package->PackageName, OptionalSegmentCookedHeaderBuffer);
				check(Package->OptimizedOptionalSegmentPackage->GetId() == Package->GlobalPackageId);
			}

			// The entry created here will have the correct set of imported packages but the export info will be updated later
			Package->PackageStoreEntry = PackageStoreOptimizer.CreatePackageStoreEntry(Package->OptimizedPackage, Package->OptimizedOptionalSegmentPackage);

		}, EParallelForFlags::Unbalanced);
	}

	FMemory::Free(UAssetMemory);
	FMemory::Free(OptionalSegmentUAssetMemory);
}

static TUniquePtr<FIoStoreReader> CreateIoStoreReader(const TCHAR* Path, const FKeyChain& KeyChain)
{
	TUniquePtr<FIoStoreReader> IoStoreReader(new FIoStoreReader());

	TMap<FGuid, FAES::FAESKey> DecryptionKeys;
	for (const auto& KV : KeyChain.GetEncryptionKeys())
	{
		DecryptionKeys.Add(KV.Key, KV.Value.Key);
	}
	FIoStatus Status = IoStoreReader->Initialize(*FPaths::ChangeExtension(Path, TEXT("")), DecryptionKeys);
	if (Status.IsOk())
	{
		return IoStoreReader;
	}
	else
	{
		UE_LOG(LogIoStore, Warning, TEXT("Failed creating IoStore reader '%s' [%s]"), Path, *Status.ToString())
		return nullptr;
	}
}

TArray<TUniquePtr<FIoStoreReader>> CreatePatchSourceReaders(const TArray<FString>& Files, const FIoStoreArguments& Arguments)
{
	TArray<TUniquePtr<FIoStoreReader>> Readers;
	for (const FString& PatchSourceContainerFile : Files)
	{
		TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*PatchSourceContainerFile, Arguments.PatchKeyChain);
		if (Reader.IsValid())
		{
			UE_LOG(LogIoStore, Display, TEXT("Loaded patch source container '%s'"), *PatchSourceContainerFile);
			Readers.Add(MoveTemp(Reader));
		}
	}
	return Readers;
}

bool LoadShaderAssetInfo(const FString& Filename, TMap<FSHAHash, TSet<FName>>& OutShaderCodeToAssets)
{
	FString JsonText; 
	if (!FFileHelper::LoadFileToString(JsonText, *Filename))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(JsonText);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	TSharedPtr<FJsonValue> AssetInfoArrayValue = JsonObject->Values.FindRef(TEXT("ShaderCodeToAssets"));
	if (!AssetInfoArrayValue.IsValid())
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> AssetInfoArray = AssetInfoArrayValue->AsArray();
	for (int32 IdxPair = 0, NumPairs = AssetInfoArray.Num(); IdxPair < NumPairs; ++IdxPair)
	{
		TSharedPtr<FJsonObject> Pair = AssetInfoArray[IdxPair]->AsObject();
		if (Pair.IsValid())
		{
			TSharedPtr<FJsonValue> ShaderMapHashJson = Pair->Values.FindRef(TEXT("ShaderMapHash"));
			if (!ShaderMapHashJson.IsValid())
			{
				continue;
			}
			FSHAHash ShaderMapHash;
			ShaderMapHash.FromString(ShaderMapHashJson->AsString());

			TSharedPtr<FJsonValue> AssetPathsArrayValue = Pair->Values.FindRef(TEXT("Assets"));
			if (!AssetPathsArrayValue.IsValid())
			{
				continue;
			}

			TSet<FName>& Assets = OutShaderCodeToAssets.Add(ShaderMapHash);
			TArray<TSharedPtr<FJsonValue>> AssetPathsArray = AssetPathsArrayValue->AsArray();
			for (int32 IdxAsset = 0, NumAssets = AssetPathsArray.Num(); IdxAsset < NumAssets; ++IdxAsset)
			{
				Assets.Add(FName(*AssetPathsArray[IdxAsset]->AsString()));
			}
		}
	}
	return true;
}

static bool ConvertToIoStoreShaderLibrary(
	const TCHAR* FileName,
	FOodleDataCompression::ECompressor InShaderOodleCompressor,
	FOodleDataCompression::ECompressionLevel InShaderOodleLevel,
	TTuple<FIoChunkId, FIoBuffer>& OutLibraryIoChunk,
	TArray<TTuple<FIoChunkId, FIoBuffer, uint32>>& OutCodeIoChunks,
	TArray<TTuple<FSHAHash, TArray<FIoChunkId>>>& OutShaderMaps,
	TArray<TTuple<FSHAHash, TSet<FName>>>& OutShaderMapAssetAssociations)
{
	IOSTORE_CPU_SCOPE(ConvertShaderLibrary);
	// ShaderArchive-MyProject-PCD3D.ushaderbytecode
	FStringView BaseFileNameView = FPathViews::GetBaseFilename(FileName);
	int32 FormatStartIndex = -1;
	if (!BaseFileNameView.FindLastChar('-', FormatStartIndex))
	{
		UE_LOG(LogIoStore, Error, TEXT("Invalid shader code library file name '%s'."), FileName);
		return false;
	}
	int32 LibraryNameStartIndex = -1;
	if (!BaseFileNameView.FindChar('-', LibraryNameStartIndex) || FormatStartIndex - LibraryNameStartIndex <= 1)
	{
		UE_LOG(LogIoStore, Error, TEXT("Invalid shader code library file name '%s'."), FileName);
		return false;
	}
	FString LibraryName(BaseFileNameView.Mid(LibraryNameStartIndex + 1, FormatStartIndex - LibraryNameStartIndex - 1));
	FName FormatName(BaseFileNameView.RightChop(FormatStartIndex + 1));
	
	TUniquePtr<FArchive> LibraryAr(IFileManager::Get().CreateFileReader(FileName));
	if (!LibraryAr)
	{
		UE_LOG(LogIoStore, Error, TEXT("Missing shader code library file '%s'."), FileName);
		return false;
	}

	uint32 Version;
	(*LibraryAr) << Version;

	FSerializedShaderArchive SerializedShaders;
	(*LibraryAr) << SerializedShaders;
	int64 OffsetToShaderCode = LibraryAr->Tell();

	FString AssetInfoFileName = FPaths::GetPath(FileName) / FString::Printf(TEXT("ShaderAssetInfo-%s-%s.assetinfo.json"), *LibraryName, *FormatName.ToString());
	TMap<FSHAHash, FShaderMapAssetPaths> ShaderCodeToAssets;
	if (!LoadShaderAssetInfo(AssetInfoFileName, ShaderCodeToAssets))
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed loading asset asset info file '%s'"), *AssetInfoFileName);
		return false;
	}

	FIoStoreShaderCodeArchiveHeader IoStoreLibraryHeader;
	FIoStoreShaderCodeArchive::CreateIoStoreShaderCodeArchiveHeader(FName(FormatName), SerializedShaders, IoStoreLibraryHeader);
	checkf(SerializedShaders.ShaderEntries.Num() == IoStoreLibraryHeader.ShaderEntries.Num(), TEXT("IoStore header has different number of shaders (%d) than the original library (%d)"), 
		IoStoreLibraryHeader.ShaderEntries.Num(), SerializedShaders.ShaderEntries.Num());
	checkf(SerializedShaders.ShaderMapEntries.Num() == IoStoreLibraryHeader.ShaderMapEntries.Num(), TEXT("IoStore header has different number of shadermaps (%d) than the original library (%d)"),
		IoStoreLibraryHeader.ShaderMapEntries.Num(), SerializedShaders.ShaderMapEntries.Num());

	// Load all the shaders, decompress them and recompress into groups. Each code chunk is shader group chunk now.
	// This also updates group's compressed size, the only value left to calculate.
	int32 GroupCount = IoStoreLibraryHeader.ShaderGroupEntries.Num();
	OutCodeIoChunks.SetNum(GroupCount);
	FCriticalSection DiskArchiveAccess;
	ParallelFor(OutCodeIoChunks.Num(),
		[&OutCodeIoChunks, &IoStoreLibraryHeader, &DiskArchiveAccess, &LibraryAr, &SerializedShaders, &OffsetToShaderCode, InShaderOodleCompressor, InShaderOodleLevel](int32 GroupIndex)
		{
			FIoStoreShaderGroupEntry& Group = IoStoreLibraryHeader.ShaderGroupEntries[GroupIndex];
			uint8* UncompressedGroupMemory = reinterpret_cast<uint8*>(FMemory::Malloc(Group.UncompressedSize));
			check(UncompressedGroupMemory);

			int32 SmallestShaderInGroupByOrdinal = MAX_int32;
			uint8* ShaderStart = UncompressedGroupMemory;
			// Not worth to special-case for group size of 1 (by converting to copy) - such groups are very fast to decompress and recompress.
			for (uint32 IdxShaderInGroup = 0; IdxShaderInGroup < Group.NumShaders; ++IdxShaderInGroup)
			{
				int32 ShaderIndex = IoStoreLibraryHeader.ShaderIndices[Group.ShaderIndicesOffset + IdxShaderInGroup];
				SmallestShaderInGroupByOrdinal = FMath::Min(SmallestShaderInGroupByOrdinal, ShaderIndex);
				const FIoStoreShaderCodeEntry& Shader = IoStoreLibraryHeader.ShaderEntries[ShaderIndex];

				checkf((reinterpret_cast<uint64>(ShaderStart) - reinterpret_cast<uint64>(UncompressedGroupMemory)) == Shader.UncompressedOffsetInGroup,
					TEXT("Shader uncompressed offset in group does not agree with its actual placement: Shader.UncompressedOffsetInGroup=%llu, actual=%llu"),
					Shader.UncompressedOffsetInGroup, (reinterpret_cast<uint64>(ShaderStart) - reinterpret_cast<uint64>(UncompressedGroupMemory))
				);

				// load and decompress the shader at the desired offset
				const FShaderCodeEntry& IndividuallyCompressedShader = SerializedShaders.ShaderEntries[ShaderIndex];
				{
					// small shaders might be stored without compression, handle them here
					if (IndividuallyCompressedShader.Size == IndividuallyCompressedShader.UncompressedSize)
					{
						// disk access has to be serialized between the for loops
						FScopeLock Lock(&DiskArchiveAccess);
						LibraryAr->Seek(OffsetToShaderCode + IndividuallyCompressedShader.Offset);
						LibraryAr->Serialize(ShaderStart, IndividuallyCompressedShader.Size);
					}
					else
					{
						uint8* CompressedShaderMemory = reinterpret_cast<uint8*>(FMemory::Malloc(IndividuallyCompressedShader.Size));

						// disk access has to be serialized between the for loops
						{
							FScopeLock Lock(&DiskArchiveAccess);
							LibraryAr->Seek(OffsetToShaderCode + IndividuallyCompressedShader.Offset);
							LibraryAr->Serialize(CompressedShaderMemory, IndividuallyCompressedShader.Size);
						}

						// This function will crash if decompression fails.
						ShaderCodeArchive::DecompressShaderWithOodle(ShaderStart, IndividuallyCompressedShader.UncompressedSize, CompressedShaderMemory, IndividuallyCompressedShader.Size);

						FMemory::Free(CompressedShaderMemory);
					}
				}

				ShaderStart += IndividuallyCompressedShader.UncompressedSize;
			}

			checkf((reinterpret_cast<uint64>(ShaderStart) - reinterpret_cast<uint64>(UncompressedGroupMemory)) == Group.UncompressedSize,
				TEXT("Uncompressed shader group size does not agree with the actual results (Group.UncompressedSize=%llu, actual=%llu)"), 
				Group.UncompressedSize, (reinterpret_cast<uint64>(ShaderStart) - reinterpret_cast<uint64>(UncompressedGroupMemory)));

			// now compress the whole group
			int64 CompressedGroupSize = 0;
			uint8* CompressedShaderGroupMemory = nullptr;
			ShaderCodeArchive::CompressShaderWithOodle(nullptr, CompressedGroupSize, UncompressedGroupMemory, Group.UncompressedSize, InShaderOodleCompressor, InShaderOodleLevel);
			checkf(CompressedGroupSize > 0, TEXT("CompressedGroupSize estimate seems wrong (%lld)"), CompressedGroupSize);

			CompressedShaderGroupMemory = reinterpret_cast<uint8*>(FMemory::Malloc(CompressedGroupSize));
			bool bCompressed = ShaderCodeArchive::CompressShaderWithOodle(CompressedShaderGroupMemory, CompressedGroupSize, UncompressedGroupMemory, Group.UncompressedSize, InShaderOodleCompressor, InShaderOodleLevel);
			checkf(bCompressed, TEXT("We could not compress the shader group after providing an estimated memory."));

			TTuple<FIoChunkId, FIoBuffer, uint32>& OutCodeIoChunk = OutCodeIoChunks[GroupIndex];
			OutCodeIoChunk.Get<0>() = IoStoreLibraryHeader.ShaderGroupIoHashes[GroupIndex];
			// This value is the load order factor for the group (the smaller, the more likely), that IoStore will use to sort the chunks.
			// We calculate it as the smallest shader index in the group, which shouldn't be bad. Arguably a better way would be to get the lowest-numbered shadermap that references _any_ shader in the group, 
			// but it's much, much slower to calculate and the resulting order would be likely the same/similar most of the time
			checkf(SmallestShaderInGroupByOrdinal >= 0 && SmallestShaderInGroupByOrdinal < IoStoreLibraryHeader.ShaderEntries.Num(), TEXT("SmallestShaderInGroupByOrdinal has an invalid value of %d (not within [0, %d) range as expected)"),
				SmallestShaderInGroupByOrdinal, IoStoreLibraryHeader.ShaderEntries.Num());
			OutCodeIoChunk.Get<2>() = static_cast<uint32>(SmallestShaderInGroupByOrdinal);

			if (CompressedGroupSize < Group.UncompressedSize)
			{
				OutCodeIoChunk.Get<1>() = FIoBuffer(CompressedGroupSize);
				FMemory::Memcpy(OutCodeIoChunk.Get<1>().GetData(), CompressedShaderGroupMemory, CompressedGroupSize);
				Group.CompressedSize = CompressedGroupSize;
			}
			else
			{
				// store uncompressed (unlikely, but happens for a 200-byte sized shader that happens to get its own group)
				OutCodeIoChunk.Get<1>() = FIoBuffer(Group.UncompressedSize);
				FMemory::Memcpy(OutCodeIoChunk.Get<1>().GetData(), UncompressedGroupMemory, Group.UncompressedSize);
				Group.CompressedSize = Group.UncompressedSize;
			}
			FMemory::Free(UncompressedGroupMemory);
			FMemory::Free(CompressedShaderGroupMemory);
		},
		EParallelForFlags::Unbalanced
	);

	// calculate and log stats
	int64 TotalUncompressedSizeViaGroups = 0;	// calculate total uncompressed size twice for a sanity check
	int64 TotalUncompressedSizeViaShaders = 0;
	int64 TotalIndividuallyCompressedSize = 0;
	int64 TotalGroupCompressedSize = 0;
	for (int32 IdxGroup = 0, NumGroups = IoStoreLibraryHeader.ShaderGroupEntries.Num(); IdxGroup < NumGroups; ++IdxGroup)
	{
		FIoStoreShaderGroupEntry& Group = IoStoreLibraryHeader.ShaderGroupEntries[IdxGroup];
		TotalGroupCompressedSize += Group.CompressedSize;
		TotalUncompressedSizeViaGroups += Group.UncompressedSize;

		// now go via shaders
		for (uint32 IdxShaderInGroup = 0; IdxShaderInGroup < Group.NumShaders; ++IdxShaderInGroup)
		{
			int32 ShaderIndex = IoStoreLibraryHeader.ShaderIndices[Group.ShaderIndicesOffset + IdxShaderInGroup];
			const FShaderCodeEntry& IndividuallyCompressedShader = SerializedShaders.ShaderEntries[ShaderIndex];

			TotalIndividuallyCompressedSize += IndividuallyCompressedShader.Size;
			TotalUncompressedSizeViaShaders += IndividuallyCompressedShader.UncompressedSize;
		}
	}

	checkf(TotalUncompressedSizeViaGroups == TotalUncompressedSizeViaShaders, TEXT("Sanity check failure: total uncompressed shader size differs if calculated via shader groups (%lld) or individual shaders (%lld)"),
		TotalUncompressedSizeViaGroups, TotalUncompressedSizeViaShaders
		);

	UE_LOG(LogIoStore, Display, TEXT("%s(%s): Recompressed %d shaders as %d groups. Library size changed from %lld KB (%.2f : 1 ratio) to %lld KB (%.2f : 1 ratio), %.2f%% of previous."),
		*LibraryName, *FormatName.ToString(),
		SerializedShaders.ShaderEntries.Num(), IoStoreLibraryHeader.ShaderGroupEntries.Num(),
		TotalIndividuallyCompressedSize / 1024, static_cast<double>(TotalUncompressedSizeViaShaders) / static_cast<double>(TotalIndividuallyCompressedSize),
		TotalGroupCompressedSize / 1024, static_cast<double>(TotalUncompressedSizeViaShaders) / static_cast<double>(TotalGroupCompressedSize),
		100.0 * static_cast<double>(TotalGroupCompressedSize) / static_cast<double>(TotalIndividuallyCompressedSize)
		);

	FLargeMemoryWriter IoStoreLibraryAr(0, true);
	FIoStoreShaderCodeArchive::SaveIoStoreShaderCodeArchive(IoStoreLibraryHeader, IoStoreLibraryAr);
	OutLibraryIoChunk.Key = FIoStoreShaderCodeArchive::GetShaderCodeArchiveChunkId(LibraryName, FormatName);
	int64 TotalSize = IoStoreLibraryAr.TotalSize();
	OutLibraryIoChunk.Value = FIoBuffer(FIoBuffer::AssumeOwnership, IoStoreLibraryAr.ReleaseOwnership(), TotalSize);

	int32 ShaderMapCount = IoStoreLibraryHeader.ShaderMapHashes.Num();
	OutShaderMaps.SetNum(ShaderMapCount);
	ParallelFor(OutShaderMaps.Num(),
		[&OutShaderMaps, &IoStoreLibraryHeader](int32 ShaderMapIndex)
		{
			TTuple<FSHAHash, TArray<FIoChunkId>>& OutShaderMap = OutShaderMaps[ShaderMapIndex];
			OutShaderMap.Key = IoStoreLibraryHeader.ShaderMapHashes[ShaderMapIndex];
			const FIoStoreShaderMapEntry& ShaderMapEntry = IoStoreLibraryHeader.ShaderMapEntries[ShaderMapIndex];
			int32 LookupIndexEnd = ShaderMapEntry.ShaderIndicesOffset + ShaderMapEntry.NumShaders;
			OutShaderMap.Value.Reserve(ShaderMapEntry.NumShaders);	// worst-case, 1 group == 1 shader
			for (int32 ShaderLookupIndex = ShaderMapEntry.ShaderIndicesOffset; ShaderLookupIndex < LookupIndexEnd; ++ShaderLookupIndex)
			{
				int32 ShaderIndex = IoStoreLibraryHeader.ShaderIndices[ShaderLookupIndex];
				int32 GroupIndex = IoStoreLibraryHeader.ShaderEntries[ShaderIndex].ShaderGroupIndex;
				OutShaderMap.Value.AddUnique(IoStoreLibraryHeader.ShaderGroupIoHashes[GroupIndex]);
			}
		},
		EParallelForFlags::Unbalanced
	);

	OutShaderMapAssetAssociations.Reserve(ShaderCodeToAssets.Num());
	for (auto& KV : ShaderCodeToAssets)
	{
		OutShaderMapAssetAssociations.Emplace(KV.Key, MoveTemp(KV.Value));
	}

	return true;
}

// Carries association between shaders and packages from shader processing to size assignment.
struct FShaderAssociationInfo
{
	struct FShaderChunkInfo
	{
		enum EType 
		{
			// This shader is referenced by one or more packages and can assign its size to
			// those packages
			Package, 
			// This shader is needed by the baseline engine in some manner and is a flat
			// size cost.
			Global, 
			// This shader isn't global or referenced by packages - theoretically shouldn't
			// exist.
			Orphan
		};

		EType Type;
		TArray<FName> ReferencedByPackages;
		uint32 CompressedSize = 0;
		UE::Cook::EPluginSizeTypes SizeType;

		// If we are shared across plugins, this is our pseudo plugin name. This is generated during size assignment.
		FString SharedPluginName;
	};
	
	struct FShaderChunkInfoKey
	{
		FName PakChunkName;
		FIoChunkId IoChunkId;

		friend uint32 GetTypeHash(const FShaderChunkInfoKey& Key) { return GetTypeHash(Key.IoChunkId); }
		friend bool operator == (const FShaderChunkInfoKey& LHS, const FShaderChunkInfoKey& RHS) { return LHS.PakChunkName == RHS.PakChunkName && LHS.IoChunkId == RHS.IoChunkId; }
	};

	TMap<FShaderChunkInfoKey, FShaderChunkInfo> ShaderChunkInfos;

	// The list of shaders referenced by each package. This can be used to look up in to ContainerShaderInfos.
	TMap<FName /* PackageName */, TArray<FShaderChunkInfoKey>> PackageShaderMap;
};

static void ProcessShaderLibraries(const FIoStoreArguments& Arguments, TArray<FContainerTargetSpec*>& ContainerTargets, TArray<FShaderInfo*> OutShaders, FShaderAssociationInfo& OutAssocInfo)
{
	IOSTORE_CPU_SCOPE(ProcessShaderLibraries);

	double LibraryStart = FPlatformTime::Seconds();

	TMap<FIoChunkId, FShaderInfo*> ChunkIdToShaderInfoMap;
	TMap<FSHAHash, TArray<FIoChunkId>> ShaderChunkIdsByShaderMapHash;
	TMap<FName, TSet<FSHAHash>> PackageNameToShaderMaps;
	TMap<FContainerTargetSpec*, TSet<FShaderInfo*>> AllContainerShaderLibraryShadersMap;

	{
		IOSTORE_CPU_SCOPE(ConvertShaderLibraries);
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
			{
				if (TargetFile.ChunkType == EContainerChunkType::ShaderCodeLibrary)
				{
					TSet<FShaderInfo*>& ContainerShaderLibraryShaders = AllContainerShaderLibraryShadersMap.FindOrAdd(ContainerTarget);

					TArray<TTuple<FSHAHash, TArray<FIoChunkId>>> ShaderMaps;
					TTuple<FIoChunkId, FIoBuffer> LibraryChunk;
					TArray<TTuple<FIoChunkId, FIoBuffer, uint32>> CodeChunks;
					TArray<TTuple<FSHAHash, TSet<FName>>> ShaderMapAssetAssociations;
				    if (!ConvertToIoStoreShaderLibrary(*TargetFile.NormalizedSourcePath, Arguments.ShaderOodleCompressor, Arguments.ShaderOodleLevel, LibraryChunk, CodeChunks, ShaderMaps, ShaderMapAssetAssociations))
					{
						UE_LOG(LogIoStore, Warning, TEXT("Failed converting shader library '%s'"), *TargetFile.NormalizedSourcePath);
						continue;
					}
					TargetFile.ChunkId = LibraryChunk.Key;
					TargetFile.SourceBuffer.Emplace(LibraryChunk.Value);
					TargetFile.SourceSize = LibraryChunk.Value.GetSize();

					const bool bIsGlobalShaderLibrary = FPaths::GetCleanFilename(TargetFile.NormalizedSourcePath).StartsWith(TEXT("ShaderArchive-Global-"));
					const FShaderInfo::EShaderType ShaderType = bIsGlobalShaderLibrary ? FShaderInfo::Global : FShaderInfo::Normal;
					for (const TTuple<FIoChunkId, FIoBuffer, uint32>& CodeChunk : CodeChunks)
					{
						const FIoChunkId& ShaderChunkId = CodeChunk.Get<0>();
						FShaderInfo* ShaderInfo = ChunkIdToShaderInfoMap.FindRef(ShaderChunkId);
						if (!ShaderInfo)
						{
							ShaderInfo = new FShaderInfo();
							ShaderInfo->ChunkId = ShaderChunkId;
							ShaderInfo->CodeIoBuffer = CodeChunk.Get<1>();
							ShaderInfo->LoadOrderFactor = CodeChunk.Get<2>();
							OutShaders.Add(ShaderInfo);
							ChunkIdToShaderInfoMap.Add(ShaderChunkId, ShaderInfo);
						}
						else
						{
							// first, make sure that the code is exactly the same
							if (ShaderInfo->CodeIoBuffer != CodeChunk.Get<1>())
							{
								UE_LOG(LogIoStore, Error, TEXT("Collision of two shader code chunks, same Id (%s), different code. Packaged game will likely crash, not being able to decompress the shaders."), *LexToString(ShaderChunkId) );
							}

							// If we already exist, then we have two separate LoadOrderFactors,
							// which one we got first affects build determinism. Take the lower.
							if (CodeChunk.Get<2>() < ShaderInfo->LoadOrderFactor)
							{
								ShaderInfo->LoadOrderFactor = CodeChunk.Get<2>();
							}
						}


						const FShaderInfo::EShaderType* CurrentShaderTypeInContainer = ShaderInfo->TypeInContainer.Find(ContainerTarget);
						if (!CurrentShaderTypeInContainer || *CurrentShaderTypeInContainer != FShaderInfo::Global)
						{
							// If a shader is both global and shared consider it to be global
							ShaderInfo->TypeInContainer.Add(ContainerTarget, ShaderType);
						}
						
						ContainerShaderLibraryShaders.Add(ShaderInfo);
					}
					for (const TTuple<FSHAHash, TSet<FName>>& ShaderMapAssetAssociation : ShaderMapAssetAssociations)
					{
						for (const FName& PackageName : ShaderMapAssetAssociation.Value)
						{
							TSet<FSHAHash>& PackageShaderMaps = PackageNameToShaderMaps.FindOrAdd(PackageName);
							PackageShaderMaps.Add(ShaderMapAssetAssociation.Key);
						}
					}

					for (TTuple<FSHAHash, TArray<FIoChunkId>>& ShaderMap : ShaderMaps)
					{
						TArray<FIoChunkId>& ShaderMapChunkIds = ShaderChunkIdsByShaderMapHash.FindOrAdd(ShaderMap.Key);
						ShaderMapChunkIds.Append(MoveTemp(ShaderMap.Value));
					}
				} // end if containerchunktype shadercodelibrary
			} // end foreach targetfile
		} // end foreach container
	}

	// 1. Update ShaderInfos with which packages we reference.
	// 2. Add to packages which shaders we use.
	// 3. Add to PackageStore what shaders we use.
	{
		IOSTORE_CPU_SCOPE(UpdatePackageStoreShaders);
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			for (FCookedPackage* Package : ContainerTarget->Packages)
			{
				TSet<FSHAHash>* FindShaderMapHashes = PackageNameToShaderMaps.Find(Package->PackageName);
				if (FindShaderMapHashes)
				{
					for (const FSHAHash& ShaderMapHash : *FindShaderMapHashes)
					{
						const TArray<FIoChunkId>* FindChunkIds = ShaderChunkIdsByShaderMapHash.Find(ShaderMapHash);
						if (!FindChunkIds)
						{
							UE_LOG(LogIoStore, Warning, TEXT("Package '%s' in '%s' referencing missing shader map '%s'"), *Package->PackageName.ToString(), *ContainerTarget->Name.ToString(), *ShaderMapHash.ToString());
							continue;
						}
						Package->ShaderMapHashes.Add(ShaderMapHash);
						for (const FIoChunkId& ShaderChunkId : *FindChunkIds)
						{
							FShaderInfo* ShaderInfo = ChunkIdToShaderInfoMap.FindRef(ShaderChunkId);
							if (!ShaderInfo)
							{
								UE_LOG(LogIoStore, Warning, TEXT("Package '%s' in '%s' referencing missing shader with chunk id '%s'"), *Package->PackageName.ToString(), *ContainerTarget->Name.ToString(), *BytesToHex(ShaderChunkId.GetData(), ShaderChunkId.GetSize()));
								continue;
							}

							check(ShaderInfo);
							ShaderInfo->ReferencedByPackages.Add(Package);
							Package->Shaders.AddUnique(ShaderInfo);
						}
					}
				}
				Algo::Sort(Package->ShaderMapHashes);
			}
		}
	}

	{
		IOSTORE_CPU_SCOPE(AddShaderChunks);
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			auto AddShaderTargetFile = [&ContainerTarget](FShaderInfo* ShaderInfo)
			{
				FContainerTargetFile& ShaderTargetFile = ContainerTarget->TargetFiles.AddDefaulted_GetRef();
				ShaderTargetFile.ContainerTarget = ContainerTarget;
				ShaderTargetFile.ChunkId = ShaderInfo->ChunkId;
				ShaderTargetFile.ChunkType = EContainerChunkType::ShaderCode;
				ShaderTargetFile.bForceUncompressed = true;
				ShaderTargetFile.SourceBuffer.Emplace(ShaderInfo->CodeIoBuffer);
				ShaderTargetFile.SourceSize = ShaderInfo->CodeIoBuffer.DataSize();
			};

			const TSet<FShaderInfo*>* FindContainerShaderLibraryShaders = AllContainerShaderLibraryShadersMap.Find(ContainerTarget);
			if (FindContainerShaderLibraryShaders)
			{
				for (FCookedPackage* Package : ContainerTarget->Packages)
				{
					for (FShaderInfo* ShaderInfo : Package->Shaders)
					{
						if (ShaderInfo->ReferencedByPackages.Num() == 1)
						{
							FShaderInfo::EShaderType* ShaderType = ShaderInfo->TypeInContainer.Find(ContainerTarget);
							if (ShaderType && *ShaderType != FShaderInfo::Global)
							{
								*ShaderType = FShaderInfo::Inline;
							}
						}
					}
				}

				for (FShaderInfo* ShaderInfo : *FindContainerShaderLibraryShaders)
				{
					FShaderAssociationInfo::FShaderChunkInfoKey ShaderChunkInfoKey = { ContainerTarget->Name, ShaderInfo->ChunkId };
					FShaderAssociationInfo::FShaderChunkInfo& ShaderChunkInfo = OutAssocInfo.ShaderChunkInfos.Add(ShaderChunkInfoKey);

					FShaderInfo::EShaderType* ShaderType = ShaderInfo->TypeInContainer.Find(ContainerTarget);
					check(ShaderType);
					if (*ShaderType == FShaderInfo::Global)
					{
						ContainerTarget->GlobalShaders.Add(ShaderInfo);
						ShaderChunkInfo.Type = FShaderAssociationInfo::FShaderChunkInfo::Global;
					}
					else if (*ShaderType == FShaderInfo::Inline)
					{
						ContainerTarget->InlineShaders.Add(ShaderInfo);

						ShaderChunkInfo.Type = FShaderAssociationInfo::FShaderChunkInfo::Package;
						checkf(ShaderInfo->ReferencedByPackages.Num() == 1, TEXT("Inline shader chunks must be referenced by 1 package only, but shader chunk %s is referenced by %d"),
							*LexToString(ShaderInfo->ChunkId), ShaderInfo->ReferencedByPackages.Num());

						ShaderChunkInfo.ReferencedByPackages.Add(ShaderInfo->ReferencedByPackages[FSetElementId::FromInteger(0)]->PackageName);
					}
					else if (ShaderInfo->ReferencedByPackages.Num() > 1)
					{
						ContainerTarget->SharedShaders.Add(ShaderInfo);

						ShaderChunkInfo.Type = FShaderAssociationInfo::FShaderChunkInfo::Package;
						
						for (FCookedPackage* Package : ShaderInfo->ReferencedByPackages)
						{
							ShaderChunkInfo.ReferencedByPackages.Add(Package->PackageName);

							TArray<FShaderAssociationInfo::FShaderChunkInfoKey>& PackageShaders = OutAssocInfo.PackageShaderMap.FindOrAdd(Package->PackageName);
							PackageShaders.Add(ShaderChunkInfoKey);
						}
					}
					else
					{
						//
						// Note that we can get here with shaders that get split off in to another container (e.g. sm6 shaders). 
						// Since they are in a different pakChunk they can't get inlined or shared. However, they still "belong" to
						// the referencing packages for association purposes.
						//
						if (ShaderInfo->ReferencedByPackages.Num())
						{
							ShaderChunkInfo.Type = FShaderAssociationInfo::FShaderChunkInfo::Package;

							for (FCookedPackage* Package : ShaderInfo->ReferencedByPackages)
							{
								ShaderChunkInfo.ReferencedByPackages.Add(Package->PackageName);

								TArray<FShaderAssociationInfo::FShaderChunkInfoKey>& PackageShaders = OutAssocInfo.PackageShaderMap.FindOrAdd(Package->PackageName);
								PackageShaders.Add(ShaderChunkInfoKey);
							}
						}
						else
						{
							ShaderChunkInfo.Type = FShaderAssociationInfo::FShaderChunkInfo::Orphan;
						}

						ContainerTarget->UniqueShaders.Add(ShaderInfo);
					}
					AddShaderTargetFile(ShaderInfo);
				}
			}
		}
	}

	double LibraryEnd = FPlatformTime::Seconds();
	UE_LOG(LogIoStore, Display, TEXT("Shaders processed in %.2f seconds"), LibraryEnd - LibraryStart);
}

void InitializeContainerTargetsAndPackages(
	const FIoStoreArguments& Arguments,
	TArray<FCookedPackage*>& Packages,
	FPackageNameMap& PackageNameMap,
	FPackageIdMap& PackageIdMap,
	TArray<FContainerTargetSpec*>& ContainerTargets)
{
	auto CreateTargetFileFromCookedFile = [
		&Arguments,
		&Packages,
		&PackageNameMap,
		&PackageIdMap](const FContainerSourceFile& SourceFile, FContainerTargetFile& OutTargetFile) -> bool
	{
		const FCookedFileStatData* OriginalCookedFileStatData = Arguments.CookedFileStatMap.Find(SourceFile.NormalizedPath);
		if (!OriginalCookedFileStatData)
		{
			UE_LOG(LogIoStore, Warning, TEXT("File not found: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}

		const FCookedFileStatData* CookedFileStatData = OriginalCookedFileStatData;
		if (CookedFileStatData->FileType == FCookedFileStatData::PackageHeader)
		{
			FStringView NormalizedSourcePathView(SourceFile.NormalizedPath);
			int32 ExtensionStartIndex = GetFullExtensionStartIndex(NormalizedSourcePathView);
			TStringBuilder<512> UexpPath;
			UexpPath.Append(NormalizedSourcePathView.Left(ExtensionStartIndex));
			UexpPath.Append(TEXT(".uexp"));
			CookedFileStatData = Arguments.CookedFileStatMap.Find(*UexpPath);
			if (!CookedFileStatData)
			{
				UE_LOG(LogIoStore, Warning, TEXT("Couldn't find .uexp file for: '%s'"), *SourceFile.NormalizedPath);
				return false;
			}
			OutTargetFile.NormalizedSourcePath = UexpPath;
		}
		else if (CookedFileStatData->FileType == FCookedFileStatData::OptionalSegmentPackageHeader)
		{
			FStringView NormalizedSourcePathView(SourceFile.NormalizedPath);
			int32 ExtensionStartIndex = GetFullExtensionStartIndex(NormalizedSourcePathView);
			TStringBuilder<512> UexpPath;
			UexpPath.Append(NormalizedSourcePathView.Left(ExtensionStartIndex));
			UexpPath.Append(TEXT(".o.uexp"));
			CookedFileStatData = Arguments.CookedFileStatMap.Find(*UexpPath);
			if (!CookedFileStatData)
			{
				UE_LOG(LogIoStore, Warning, TEXT("Couldn't find .o.uexp file for: '%s'"), *SourceFile.NormalizedPath);
				return false;
			}
			OutTargetFile.NormalizedSourcePath = UexpPath;
		}
		else
		{
			OutTargetFile.NormalizedSourcePath = SourceFile.NormalizedPath;
		}
		OutTargetFile.SourceSize = uint64(CookedFileStatData->FileSize);
		
		if (CookedFileStatData->FileType == FCookedFileStatData::ShaderLibrary)
		{
			OutTargetFile.ChunkType = EContainerChunkType::ShaderCodeLibrary;
		}
		else
		{
			FLegacyCookedPackage* Package = FindOrAddLegacyPackage(Arguments, *SourceFile.NormalizedPath, Packages, PackageNameMap, PackageIdMap);
			OutTargetFile.Package = Package;
			if (!OutTargetFile.Package)
			{
				UE_LOG(LogIoStore, Warning, TEXT("Failed to obtain package name from file name '%s'"), *SourceFile.NormalizedPath);
				return false;
			}

			switch (CookedFileStatData->FileType)
			{
			case FCookedFileStatData::PackageData:
				OutTargetFile.ChunkType = EContainerChunkType::PackageData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::ExportBundleData);
				Package->FileName = SourceFile.NormalizedPath; // .uasset path
				Package->UAssetSize = OriginalCookedFileStatData->FileSize;
				Package->UExpSize = CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::BulkData:
				OutTargetFile.ChunkType = EContainerChunkType::BulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::BulkData);
				OutTargetFile.Package->TotalBulkDataSize += CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::OptionalBulkData:
				OutTargetFile.ChunkType = EContainerChunkType::OptionalBulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::OptionalBulkData);
				Package->TotalBulkDataSize += CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::MemoryMappedBulkData:
				OutTargetFile.ChunkType = EContainerChunkType::MemoryMappedBulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::MemoryMappedBulkData);
				Package->TotalBulkDataSize += CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::OptionalSegmentPackageData:
				OutTargetFile.ChunkType = EContainerChunkType::OptionalSegmentPackageData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 1, EIoChunkType::ExportBundleData);
				Package->OptionalSegmentFileName = SourceFile.NormalizedPath; // .o.uasset path
				Package->OptionalSegmentUAssetSize = OriginalCookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::OptionalSegmentBulkData:
				OutTargetFile.ChunkType = EContainerChunkType::OptionalSegmentBulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 1, EIoChunkType::BulkData);
				break;
			default:
				UE_LOG(LogIoStore, Fatal, TEXT("Unexpected file type %d for file '%s'"), CookedFileStatData->FileType, *OutTargetFile.NormalizedSourcePath);
				return false;
			}
		}

		// Only keep the regions for the file if neither compression nor encryption are enabled, otherwise the regions will be meaningless.
		if (Arguments.bFileRegions && !SourceFile.bNeedsCompression && !SourceFile.bNeedsEncryption)
		{
			// Read the matching regions file, if it exists.
			TStringBuilder<512> RegionsFilePath;
			RegionsFilePath.Append(OutTargetFile.NormalizedSourcePath);
			RegionsFilePath.Append(FFileRegion::RegionsFileExtension);
			const FCookedFileStatData* RegionsFileStatData = Arguments.CookedFileStatMap.Find(RegionsFilePath);
			if (RegionsFileStatData)
			{
				TUniquePtr<FArchive> RegionsFile(IFileManager::Get().CreateFileReader(*RegionsFilePath));
				if (!RegionsFile.IsValid())
				{
					UE_LOG(LogIoStore, Warning, TEXT("Failed reading file '%s'"), *RegionsFilePath);
				}
				else
				{
					FFileRegion::SerializeFileRegions(*RegionsFile.Get(), OutTargetFile.FileRegions);
				}
			}
		}

		return true;
	};

	struct FChunkListItem
	{
		FIoHash RawHash;
		uint64 RawSize;
	};

	TMap<FIoChunkId, FChunkListItem> ChunkList;
	if (Arguments.PackageStore->HasZenStoreClient())
	{
		double StartChunkInfoTime = FPlatformTime::Seconds();

		TIoStatusOr<FCbObject> Chunks = Arguments.PackageStore->GetChunkInfos().Get();
		if (!Chunks.IsOk())
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed to retrieve chunk list"));
			return;
		}

		FCbObject ChunksObj = Chunks.ConsumeValueOrDie();
		for (FCbField& ChunkEntry : ChunksObj["chunkinfos"])
		{
			FCbObject ChunkObj = ChunkEntry.AsObject();
			FIoChunkId ChunkId;
			if (!LoadFromCompactBinary(ChunkObj["id"], ChunkId))
			{
				UE_LOG(LogIoStore, Warning, TEXT("Received invalid chunk id, skipping."));
				continue;
			}
			ChunkList.Add(ChunkId, { ChunkObj["rawhash"].AsHash(), ChunkObj["rawsize"].AsUInt64() });
		}

		UE_LOG(LogIoStore, Display, TEXT("Fetched '%d' chunk infos in %f seconds"), ChunkList.Num(), FPlatformTime::Seconds() - StartChunkInfoTime);

	}

	auto CreateTargetFileFromZen = [
		&ChunkList,
		&Arguments,
		&Packages,
		&PackageNameMap,
		&PackageIdMap](const FContainerSourceFile& SourceFile, FContainerTargetFile& OutTargetFile) -> bool
	{
		FCookedPackageStore& PackageStore = *Arguments.PackageStore;
		
		OutTargetFile.NormalizedSourcePath = SourceFile.NormalizedPath;
		
		FStringView Extension = GetFullExtension(SourceFile.NormalizedPath);
		if (Extension == TEXT(".ushaderbytecode"))
		{
			const FCookedFileStatData* CookedFileStatData = Arguments.CookedFileStatMap.Find(SourceFile.NormalizedPath);
			if (!CookedFileStatData)
			{
				UE_LOG(LogIoStore, Warning, TEXT("File not found: '%s'"), *SourceFile.NormalizedPath);
				return false;
			}
			OutTargetFile.ChunkType = EContainerChunkType::ShaderCodeLibrary;
			OutTargetFile.SourceSize = uint64(CookedFileStatData->FileSize);
			return true;
		}

		const FCookedPackageStore::FChunkInfo* ChunkInfo = PackageStore.GetChunkInfoFromFileName(SourceFile.NormalizedPath);
		if (!ChunkInfo)
		{
			UE_LOG(LogIoStore, Warning, TEXT("File not found in manifest: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}
		OutTargetFile.ChunkId = ChunkInfo->ChunkId;
		FChunkListItem* ChunkListItem = ChunkList.Find(OutTargetFile.ChunkId);
		if (!ChunkListItem)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Chunk size not found for: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}
		OutTargetFile.SourceSize = ChunkListItem->RawSize;

		if (ChunkInfo->PackageName.IsNone())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Package name not found for: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}

		OutTargetFile.Package = &FindOrAddPackage(Arguments, ChunkInfo->PackageName, Packages, PackageNameMap, PackageIdMap);
		const FPackageStoreEntryResource* PackageStoreEntry = PackageStore.GetPackageStoreEntry(OutTargetFile.Package->GlobalPackageId);
		if (!PackageStoreEntry)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to find package store entry for package: '%s'"), *ChunkInfo->PackageName.ToString());
			return false;
		}
		OutTargetFile.Package->PackageStoreEntry = *PackageStoreEntry;

		if (Extension == TEXT(".m.ubulk"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::MemoryMappedBulkData;
			OutTargetFile.Package->TotalBulkDataSize += OutTargetFile.SourceSize;
		}
		else if (Extension == TEXT(".ubulk"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::BulkData;
			OutTargetFile.Package->TotalBulkDataSize += OutTargetFile.SourceSize;
		}
		else if (Extension == TEXT(".uptnl"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::OptionalBulkData;
			OutTargetFile.Package->TotalBulkDataSize += OutTargetFile.SourceSize;
		}
		else if (Extension == TEXT(".uasset") || Extension == TEXT(".umap"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::PackageData;
			OutTargetFile.Package->UAssetSize = OutTargetFile.SourceSize;
		}
		else if (Extension == TEXT(".o.uasset") || Extension == TEXT(".o.umap"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::OptionalSegmentPackageData;
			OutTargetFile.Package->OptionalSegmentUAssetSize = OutTargetFile.SourceSize;
		}
		else if (Extension == TEXT(".o.ubulk"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::OptionalSegmentBulkData;
		}
		else
		{
			UE_LOG(LogIoStore, Warning, TEXT("Unexpected file: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}

		// Only keep the regions for the file if neither compression nor encryption are enabled, otherwise the regions will be meaningless.
		if (Arguments.bFileRegions && !SourceFile.bNeedsCompression && !SourceFile.bNeedsEncryption)
		{
			OutTargetFile.FileRegions = ChunkInfo->FileRegions;
		}
		
		return true;
	};
	
	for (const FContainerSourceSpec& ContainerSource : Arguments.Containers)
	{
		FContainerTargetSpec* ContainerTarget = AddContainer(ContainerSource.Name, ContainerTargets);
		ContainerTarget->OutputPath = ContainerSource.OutputPath;
		ContainerTarget->StageLooseFileRootPath = ContainerSource.StageLooseFileRootPath;
		ContainerTarget->bGenerateDiffPatch = ContainerSource.bGenerateDiffPatch;

		if (ContainerSource.bOnDemand)
		{
			ContainerTarget->ContainerFlags |= EIoContainerFlags::OnDemand;
		}

		if (Arguments.bSign)
		{
			ContainerTarget->ContainerFlags |= EIoContainerFlags::Signed;
		}

		if (!ContainerTarget->EncryptionKeyGuid.IsValid() && !ContainerSource.EncryptionKeyOverrideGuid.IsEmpty())
		{
			FGuid::Parse(ContainerSource.EncryptionKeyOverrideGuid, ContainerTarget->EncryptionKeyGuid);
		}

		ContainerTarget->PatchSourceReaders = CreatePatchSourceReaders(ContainerSource.PatchSourceContainerFiles, Arguments);

		{
			IOSTORE_CPU_SCOPE(ProcessSourceFiles);
			bool bHasOptionalSegmentPackages = false;
			for (const FContainerSourceFile& SourceFile : ContainerSource.SourceFiles)
			{
				FContainerTargetFile TargetFile;
				bool bIsValidTargetFile = Arguments.PackageStore->HasZenStoreClient()
					? CreateTargetFileFromZen(SourceFile, TargetFile)
					: CreateTargetFileFromCookedFile(SourceFile, TargetFile);

				if (!bIsValidTargetFile)
				{
					continue;
				}

				TargetFile.ContainerTarget			= ContainerTarget;
				TargetFile.DestinationPath			= SourceFile.DestinationPath;
				TargetFile.bForceUncompressed		= !SourceFile.bNeedsCompression;
				
				if (SourceFile.bNeedsCompression)
				{
					ContainerTarget->ContainerFlags |= EIoContainerFlags::Compressed;
				}

				if (SourceFile.bNeedsEncryption)
				{
					ContainerTarget->ContainerFlags |= EIoContainerFlags::Encrypted;
				}

				if (TargetFile.ChunkType == EContainerChunkType::PackageData)
				{
					check(TargetFile.Package);
					ContainerTarget->Packages.Add(TargetFile.Package);
				}
				else if (TargetFile.ChunkType == EContainerChunkType::OptionalSegmentPackageData)
				{
					bHasOptionalSegmentPackages = true;
				}

				ContainerTarget->TargetFiles.Emplace(MoveTemp(TargetFile));
			}

			if (bHasOptionalSegmentPackages)
			{
				if (ContainerSource.OptionalOutputPath.IsEmpty())
				{
					ContainerTarget->OptionalSegmentOutputPath = ContainerTarget->OutputPath + FPackagePath::GetOptionalSegmentExtensionModifier();
				}
				else
				{
					// if we have an optional output location, use that directory, combined with the name of the output path 
					ContainerTarget->OptionalSegmentOutputPath = FPaths::Combine(ContainerSource.OptionalOutputPath, FPaths::GetCleanFilename(ContainerTarget->OutputPath) + FPackagePath::GetOptionalSegmentExtensionModifier());
				}

				// The IoContainerId is the hash of the name of the container, which gets returned in the results
				// as the output path we provide with the extension removed, which for optional containers means
				// that it contains the .o in the name - so make sure we have a separate id for this.
				ContainerTarget->OptionalSegmentContainerId = FIoContainerId::FromName(*FPaths::GetCleanFilename(ContainerTarget->OptionalSegmentOutputPath));

				UE_LOG(LogIoStore, Display, TEXT("Saving optional container to: '%s', id: 0x%llx (base container id: 0x%llx)"),
					*ContainerTarget->OptionalSegmentOutputPath,
					ContainerTarget->OptionalSegmentContainerId.Value(),
					ContainerTarget->ContainerId.Value());
			}
		}
	}

	Algo::Sort(Packages, [](const FCookedPackage* A, const FCookedPackage* B)
	{
		return A->GlobalPackageId < B->GlobalPackageId;
	});
};

void LogWriterResults(const TArray<FIoStoreWriterResult>& Results)
{
	struct FContainerStats
	{
		uint64 TocCount = 0;
		uint64 TocSize = 0;
		uint64 UncompressedContainerSize = 0;
		uint64 CompressedContainerSize = 0;
		uint64 PaddingSize = 0;
	};

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("Container Summary"));
	UE_LOG(LogIoStore, Display, TEXT("=================="));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-50s %10s %15s %15s %20s %20s"),
		TEXT("Container"), TEXT("Flags"), TEXT("Chunk(s) #"), TEXT("TOC (KiB)"), TEXT("Raw Size (MiB)"), TEXT("Size (MiB)"));
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------"));

	FContainerStats TotalStats;
	FContainerStats OnDemandStats;
	for (const FIoStoreWriterResult& Result : Results)
	{
		FString CompressionInfo = TEXT("-");

		if (Result.CompressionMethod != NAME_None)
		{
			const double Procentage = (double(Result.UncompressedContainerSize - Result.CompressedContainerSize) / double(Result.UncompressedContainerSize)) * 100.0;
			CompressionInfo = FString::Printf(TEXT("(%.2lf%% %s)"),
				Procentage,
				*Result.CompressionMethod.ToString());
		}

		FString ContainerSettings = FString::Printf(TEXT("%s/%s/%s/%s/%s"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Compressed) ? TEXT("C") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Encrypted) ? TEXT("E") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Signed) ? TEXT("S") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Indexed) ? TEXT("I") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::OnDemand) ? TEXT("O") : TEXT("-"));

		UE_LOG(LogIoStore, Display, TEXT("%-50s %10s %15llu %15.2lf %20.2lf %20.2lf %s"),
			*Result.ContainerName,
			*ContainerSettings,
			Result.TocEntryCount,
			(double)Result.TocSize / 1024.0,
			(double)Result.UncompressedContainerSize / 1024.0 / 1024.0,
			(double)Result.CompressedContainerSize / 1024.0 / 1024.0,
			*CompressionInfo);

		if (EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::OnDemand))
		{
			OnDemandStats.TocCount += Result.TocEntryCount;
			OnDemandStats.TocSize += Result.TocSize;
			OnDemandStats.UncompressedContainerSize += Result.UncompressedContainerSize;
			OnDemandStats.CompressedContainerSize += Result.CompressedContainerSize;
		}

		TotalStats.TocCount += Result.TocEntryCount;
		TotalStats.TocSize += Result.TocSize;
		TotalStats.UncompressedContainerSize += Result.UncompressedContainerSize;
		TotalStats.CompressedContainerSize += Result.CompressedContainerSize;
		TotalStats.PaddingSize += Result.PaddingSize;
	}
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------"));


	if (OnDemandStats.TocCount > 0)
	{
		UE_LOG(LogIoStore, Display, TEXT("%-50s %10s %15llu %15.2lf %20.2lf %20.2lf"),
			TEXT("Total On Demand"),
			TEXT(""),
			OnDemandStats.TocCount,
			(double)OnDemandStats.TocSize / 1024.0,
			(double)OnDemandStats.UncompressedContainerSize / 1024.0 / 1024.0,
			(double)OnDemandStats.CompressedContainerSize / 1024.0 / 1024.0);
	}

	UE_LOG(LogIoStore, Display, TEXT("%-50s %10s %15llu %15.2lf %20.2lf %20.2lf"),
		TEXT("Total"),
		TEXT(""),
		TotalStats.TocCount,
		(double)TotalStats.TocSize / 1024.0,
		(double)TotalStats.UncompressedContainerSize / 1024.0 / 1024.0,
		(double)TotalStats.CompressedContainerSize / 1024.0 / 1024.0);

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("** Flags: (C)ompressed / (E)ncrypted / (S)igned) / (I)ndexed) / (O)nDemand **"));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("Compression block padding: %8.2lf MiB"), (double)TotalStats.PaddingSize / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT(""));

	UE_LOG(LogIoStore, Display, TEXT("Container Directory Index"));
	UE_LOG(LogIoStore, Display, TEXT("=========================="));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-45s %15s"), TEXT("Container"), TEXT("Size (KiB)"));
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------"));
	for (const FIoStoreWriterResult& Result : Results)
	{
		UE_LOG(LogIoStore, Display, TEXT("%-45s %15.2lf"), *Result.ContainerName, double(Result.DirectoryIndexSize) / 1024.0);
	}

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("Container Patch Report"));
	UE_LOG(LogIoStore, Display, TEXT("========================"));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-44s %16s %16s %16s %16s %16s"), TEXT("Container"), TEXT("Total #"), TEXT("Modified #"), TEXT("Added #"), TEXT("Modified (MiB)"), TEXT("Added (MiB)"));
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------"));
	for (const FIoStoreWriterResult& Result : Results)
	{
		UE_LOG(LogIoStore, Display, TEXT("%-44s %16d %16d %16d %16.2lf %16.2lf"), *Result.ContainerName, Result.TocEntryCount, Result.ModifiedChunksCount, Result.AddedChunksCount, Result.ModifiedChunksSize / 1024.0 / 1024.0, Result.AddedChunksSize / 1024.0 / 1024.0);
	}
}

void LogContainerPackageInfo(const TArray<FContainerTargetSpec*>& ContainerTargets)
{
	uint64 TotalStoreSize = 0;
	uint64 TotalPackageCount = 0;
	uint64 TotalLocalizedPackageCount = 0;

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("PackageStore"));
	UE_LOG(LogIoStore, Display, TEXT("============="));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-45s %15s %15s %15s"),
		TEXT("Container"),
		TEXT("Size (KiB)"),
		TEXT("Packages #"),
		TEXT("Localized #"));
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------"));

	for (const FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		uint64 StoreSize = ContainerTarget->Header.StoreEntries.Num();
		uint64 PackageCount = ContainerTarget->Packages.Num();
		uint64 LocalizedPackageCount = ContainerTarget->Header.LocalizedPackages.Num();

		UE_LOG(LogIoStore, Display, TEXT("%-45s %15.0lf %15llu %15llu"),
			*ContainerTarget->Name.ToString(),
			(double)StoreSize / 1024.0,
			PackageCount,
			LocalizedPackageCount);

		TotalStoreSize += StoreSize;
		TotalPackageCount += PackageCount;
		TotalLocalizedPackageCount += LocalizedPackageCount;
	}
	UE_LOG(LogIoStore, Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT("%-45s %15.0lf %15llu %15llu"),
		TEXT("Total"),
		(double)TotalStoreSize / 1024.0,
		TotalPackageCount,
		TotalLocalizedPackageCount);

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT(""));
}

class FIoStoreWriteRequestManager
{
public:
	FIoStoreWriteRequestManager(FPackageStoreOptimizer& InPackageStoreOptimizer, FCookedPackageStore* InPackageStore)
		: PackageStoreOptimizer(InPackageStoreOptimizer)
		, PackageStore(InPackageStore)
		, MemoryAvailableEvent(FPlatformProcess::GetSynchEventFromPool(false))
	{
		InitiatorThread = Async(EAsyncExecution::Thread, [this]() { InitiatorThreadFunc(); });
		RetirerThread = Async(EAsyncExecution::Thread, [this]() { RetirerThreadFunc(); });
	}

	~FIoStoreWriteRequestManager()
	{
		InitiatorQueue.CompleteAdding();
		RetirerQueue.CompleteAdding();
		InitiatorThread.Wait();
		RetirerThread.Wait();
		FPlatformProcess::ReturnSynchEventToPool(MemoryAvailableEvent);
	}

	IIoStoreWriteRequest* Read(const FContainerTargetFile& InTargetFile)
	{
		if (InTargetFile.SourceBuffer.IsSet())
		{
			return new FInMemoryWriteRequest(*this, InTargetFile);
		}
		else if (PackageStore->HasZenStoreClient())
		{
			return new FZenWriteRequest(*this, InTargetFile);
		}
		else
		{
			return new FLooseFileWriteRequest(*this, InTargetFile);
		}
	}

private:
	struct FQueueEntry;
	
	class FWriteContainerTargetFileRequest
		: public IIoStoreWriteRequest
	{
		friend class FIoStoreWriteRequestManager;

	public:
		virtual ~FWriteContainerTargetFileRequest()
		{
		}

		virtual void PrepareSourceBufferAsync(FGraphEventRef InCompletionEvent) override
		{
			CompletionEvent = InCompletionEvent;
			Manager.ScheduleLoad(this);
		}

		virtual uint64 GetOrderHint() override
		{
			return TargetFile.IdealOrder;
		}

		virtual TArrayView<const FFileRegion> GetRegions() override
		{
			return FileRegions;
		}

		virtual const FIoBuffer* GetSourceBuffer() override
		{
			return &SourceBuffer;
		}
		
		virtual void FreeSourceBuffer() override
		{
			SourceBuffer = FIoBuffer();
			Manager.OnBufferMemoryFreed(SourceBufferSize);
		}

		uint64 GetSourceBufferSize() const
		{
			return SourceBufferSize;
		}

		virtual void LoadSourceBufferAsync() = 0;

	protected:
		FWriteContainerTargetFileRequest(FIoStoreWriteRequestManager& InManager,const FContainerTargetFile& InTargetFile)
			: Manager(InManager)
			, TargetFile(InTargetFile)
			, FileRegions(TargetFile.FileRegions)
			, SourceBufferSize(TargetFile.SourceSize) { }

		void OnSourceBufferLoaded()
		{
			QueueEntry->ReleaseRef(Manager);
			CompletionEvent->DispatchSubsequents();
		}

		FIoStoreWriteRequestManager& Manager;
		const FContainerTargetFile& TargetFile;
		TArray<FFileRegion> FileRegions;

		// Note -- this is filled with the TargetFile.SourceSize value which is the size of the buffer 
		// used for IO, however it's not necessarily the size of the resulting input to iostore as
		// the buffer can be post-processed after i/o (e.g. CreateOptimizedPackage).
		uint64 SourceBufferSize;
		FGraphEventRef CompletionEvent;
		FIoBuffer SourceBuffer;
		FQueueEntry* QueueEntry = nullptr;
	};

	class FInMemoryWriteRequest
		: public FWriteContainerTargetFileRequest
	{
	public:
		FInMemoryWriteRequest(FIoStoreWriteRequestManager& InManager, const FContainerTargetFile& InTargetFile)
			: FWriteContainerTargetFileRequest(InManager, InTargetFile) { }

		virtual void LoadSourceBufferAsync() override
		{
			Manager.MemorySourceReads[(int8)TargetFile.ChunkId.GetChunkType()].IncrementExchange();
			SourceBuffer = TargetFile.SourceBuffer.GetValue();
			Manager.MemorySourceBytes[(int8)TargetFile.ChunkId.GetChunkType()] += SourceBuffer.DataSize();
			OnSourceBufferLoaded();
		}
	};

	// Used when staging from cooked files
	class FLooseFileWriteRequest
		: public FWriteContainerTargetFileRequest
	{
	public:
		FLooseFileWriteRequest(FIoStoreWriteRequestManager& InManager, const FContainerTargetFile& InTargetFile)
			: FWriteContainerTargetFileRequest(InManager, InTargetFile)
			, Package(static_cast<FLegacyCookedPackage*>(InTargetFile.Package))
		{
		}

		virtual void LoadSourceBufferAsync() override
		{
			Manager.LooseFileSourceReads[(int8)TargetFile.ChunkId.GetChunkType()].IncrementExchange();
			SourceBuffer = FIoBuffer(GetSourceBufferSize());

			QueueEntry->FileHandle.Reset(
				FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*TargetFile.NormalizedSourcePath));
			
			QueueEntry->AddRef(); // Must keep it around until we've assigned the ReadRequest pointer
			FAsyncFileCallBack Callback = [this](bool, IAsyncReadRequest* ReadRequest)
			{
				Manager.LooseFileSourceBytes[(int8)TargetFile.ChunkId.GetChunkType()] += SourceBuffer.DataSize();

				if (TargetFile.ChunkType == EContainerChunkType::PackageData)
				{
					SourceBuffer = Manager.PackageStoreOptimizer.CreatePackageBuffer(Package->OptimizedPackage, SourceBuffer);
				}
				else if (TargetFile.ChunkType == EContainerChunkType::OptionalSegmentPackageData)
				{
					check(Package->OptimizedOptionalSegmentPackage);
					SourceBuffer = Manager.PackageStoreOptimizer.CreatePackageBuffer(Package->OptimizedOptionalSegmentPackage, SourceBuffer);
				}
				OnSourceBufferLoaded();
			};

			QueueEntry->ReadRequest.Reset(
				QueueEntry->FileHandle->ReadRequest(0, SourceBuffer.DataSize(), AIOP_Normal, &Callback, SourceBuffer.Data()));
			QueueEntry->ReleaseRef(Manager);
		}

	private:
		FLegacyCookedPackage* Package;
	};

	class FZenWriteRequest
		: public FWriteContainerTargetFileRequest
	{
	public:
		FZenWriteRequest(FIoStoreWriteRequestManager& InManager,const FContainerTargetFile& InTargetFile)
			: FWriteContainerTargetFileRequest(InManager, InTargetFile) {}

		virtual void LoadSourceBufferAsync() override
		{
			Manager.ZenSourceReads[(int8)TargetFile.ChunkId.GetChunkType()].IncrementExchange();
			Manager.PackageStore->ReadChunkAsync(
				TargetFile.ChunkId,
				[this](TIoStatusOr<FIoBuffer> Status)
				{
					SourceBuffer = Status.ConsumeValueOrDie();
					Manager.ZenSourceBytes[(int8)TargetFile.ChunkId.GetChunkType()] += SourceBuffer.DataSize();
					OnSourceBufferLoaded();
				});
		}
	};

	struct FQueueEntry
	{
		FQueueEntry* Next = nullptr;
		TUniquePtr<IAsyncReadFileHandle> FileHandle;
		TUniquePtr<IAsyncReadRequest> ReadRequest;
		FWriteContainerTargetFileRequest* WriteRequest = nullptr;

		void AddRef()
		{
			++RefCount;
		}

		void ReleaseRef(FIoStoreWriteRequestManager& Manager)
		{
			if (--RefCount == 0)
			{
				Manager.ScheduleRetire(this);
			}
		}

	private:
		TAtomic<int32> RefCount{ 1 };
	};

	class FQueue
	{
	public:
		FQueue()
			: Event(FPlatformProcess::GetSynchEventFromPool(false))
		{ }

		~FQueue()
		{
			check(Head == nullptr && Tail == nullptr);
			FPlatformProcess::ReturnSynchEventToPool(Event);
		}

		void Enqueue(FQueueEntry* Entry)
		{
			check(!bIsDoneAdding);
			{
				FScopeLock _(&CriticalSection);

				if (!Tail)
				{
					Head = Tail = Entry;
				}
				else
				{
					Tail->Next = Entry;
					Tail = Entry;
				}
				Entry->Next = nullptr;
			}

			Event->Trigger();
		}

		FQueueEntry* DequeueOrWait()
		{
			for (;;)
			{
				{
					FScopeLock _(&CriticalSection);
					if (Head)
					{
						FQueueEntry* Entry = Head;
						Head = Tail = nullptr;
						return Entry;
					}
				}

				if (bIsDoneAdding)
				{
					break;
				}

				Event->Wait();
			}

			return nullptr;
		}

		void CompleteAdding()
		{
			bIsDoneAdding = true;
			Event->Trigger();
		}

	private:
		FCriticalSection CriticalSection;
		FEvent* Event = nullptr;
		FQueueEntry* Head = nullptr;
		FQueueEntry* Tail = nullptr;
		TAtomic<bool> bIsDoneAdding{ false };
	};

	void ScheduleLoad(FWriteContainerTargetFileRequest* WriteRequest)
	{
		FQueueEntry* QueueEntry = new FQueueEntry();
		QueueEntry->WriteRequest = WriteRequest;
		WriteRequest->QueueEntry = QueueEntry;
		InitiatorQueue.Enqueue(QueueEntry);
	}

	void ScheduleRetire(FQueueEntry* QueueEntry)
	{
		RetirerQueue.Enqueue(QueueEntry);
	}

	void Start(FQueueEntry* QueueEntry)
	{
		const uint64 SourceBufferSize = QueueEntry->WriteRequest->GetSourceBufferSize();

		uint64 LocalUsedBufferMemory = UsedBufferMemory.Load();
		while (LocalUsedBufferMemory > 0 && LocalUsedBufferMemory + SourceBufferSize > BufferMemoryLimit)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(WaitForBufferMemory);
			MemoryAvailableEvent->Wait();
			LocalUsedBufferMemory = UsedBufferMemory.Load();
		}

		UsedBufferMemory.AddExchange(SourceBufferSize);
		TRACE_COUNTER_ADD(IoStoreUsedFileBufferMemory, SourceBufferSize);
		QueueEntry->WriteRequest->LoadSourceBufferAsync();
	}

	void Retire(FQueueEntry* QueueEntry)
	{
		if (QueueEntry->ReadRequest.IsValid())
		{
			QueueEntry->ReadRequest->WaitCompletion();
			QueueEntry->ReadRequest.Reset();
			QueueEntry->FileHandle.Reset();
		}
		delete QueueEntry;
	}

	void OnBufferMemoryFreed(uint64 Count)
	{
		uint64 OldValue = UsedBufferMemory.SubExchange(Count);
		check(OldValue >= Count);
		TRACE_COUNTER_SUBTRACT(IoStoreUsedFileBufferMemory, Count);
		MemoryAvailableEvent->Trigger();
	}

	void InitiatorThreadFunc()
	{
		for (;;)
		{
			FQueueEntry* QueueEntry = InitiatorQueue.DequeueOrWait();
			if (!QueueEntry)
			{
				return;
			}
			while (QueueEntry)
			{
				FQueueEntry* Next = QueueEntry->Next;
				Start(QueueEntry);
				QueueEntry = Next;
			}
		}
	}

	void RetirerThreadFunc()
	{
		for (;;)
		{
			FQueueEntry* QueueEntry = RetirerQueue.DequeueOrWait();
			if (!QueueEntry)
			{
				return;
			}
			while (QueueEntry)
			{
				FQueueEntry* Next = QueueEntry->Next;
				Retire(QueueEntry);
				QueueEntry = Next;
			}
		}
	}

	FPackageStoreOptimizer& PackageStoreOptimizer;
	FCookedPackageStore* PackageStore;
	TFuture<void> InitiatorThread;
	TFuture<void> RetirerThread;
	FQueue InitiatorQueue;
	FQueue RetirerQueue;
	TAtomic<uint64> UsedBufferMemory { 0 };
	FEvent* MemoryAvailableEvent;

public:
	TAtomic<uint64> ZenSourceReads[(int8)EIoChunkType::MAX] { 0 };
	TAtomic<uint64> ZenSourceBytes[(int8)EIoChunkType::MAX]{ 0 };
	TAtomic<uint64> MemorySourceReads[(int8)EIoChunkType::MAX]{ 0 };
	TAtomic<uint64> MemorySourceBytes[(int8)EIoChunkType::MAX]{ 0 };
	TAtomic<uint64> LooseFileSourceReads[(int8)EIoChunkType::MAX]{ 0 };
	TAtomic<uint64> LooseFileSourceBytes[(int8)EIoChunkType::MAX]{ 0 };

	static constexpr uint64 BufferMemoryLimit = 2ull << 30;
};

static bool WriteUtf8StringView(FUtf8StringView InView, const FString& InFilename)
{
	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*InFilename, 0));
	if (!Ar)
	{
		return false;
	}
	UTF8CHAR UTF8BOM[] = { (UTF8CHAR)0xEF, (UTF8CHAR)0xBB, (UTF8CHAR)0xBF };
	Ar->Serialize(&UTF8BOM, sizeof(UTF8BOM));
	Ar->Serialize((void*)InView.GetData(), InView.Len() * sizeof(UTF8CHAR));
	Ar->Close();
	return true;
}

// When we emit plugin size information, we emit one json for each class of sizes we
// are monitoring.
enum class EPluginGraphSizeClass : uint8
{
	All,
	Texture,
	StaticMesh,
	SoundWave,
	SkeletalMesh,
	Shader,
	Level, 
	Animation,
	Niagara,
	Material,
	Blueprint,
	Geometry,
	Other,
	COUNT
};

static const UTF8CHAR* PluginGraphEntryClassNames[] = 
{
	UTF8TEXT("all"),
	UTF8TEXT("texture"),
	UTF8TEXT("staticmesh"),
	UTF8TEXT("soundwave"),
	UTF8TEXT("skeletalmesh"),
	UTF8TEXT("shader"),
	UTF8TEXT("level"),
	UTF8TEXT("animation"),
	UTF8TEXT("niagara"),
	UTF8TEXT("material"),
	UTF8TEXT("blueprint"),
	UTF8TEXT("geometry"),
	UTF8TEXT("other")
};

static_assert( UE_ARRAY_COUNT(PluginGraphEntryClassNames) == (size_t)EPluginGraphSizeClass::COUNT, "Must have a name for each plugin graph size class!");

struct FPluginGraphEntry
{
	uint16 IndexInEnabledPlugins = 0;

	FString Name;

	TSet<FPluginGraphEntry*> DirectDependencies;
	TSet<FPluginGraphEntry*> TotalDependencies;
	TSet<FPluginGraphEntry*> Roots;

	// Only valid if bIsRoot
	TSet<FPluginGraphEntry*> UniqueDependencies;

	uint32 DirectRefcount = 0;
	bool bIsRoot = false;

	static constexpr uint8 ClassCount = (uint8)EPluginGraphSizeClass::COUNT;
	UE::Cook::FPluginSizeInfo ExclusiveSizes[ClassCount];
	UE::Cook::FPluginSizeInfo InclusiveSizes[ClassCount];
	UE::Cook::FPluginSizeInfo UniqueSizes[ClassCount];

	uint64 ExclusiveCounts[ClassCount] = {};
	uint64 InclusiveCounts[ClassCount] = {};
	uint64 UniqueCounts[ClassCount] = {};
};

struct FPluginGraph
{
	TArray<FPluginGraphEntry> Plugins;

	TMap<FStringView, FPluginGraphEntry*> NameToPlugin;
	TArray<FPluginGraphEntry*> TopologicallySortedPlugins;
	TArray<FPluginGraphEntry*> RootPlugins;

	// Plugins that can't trace a route between a plugin with bIsRoot==true and themselves.
	TSet<FPluginGraphEntry*> UnrootedPlugins;
};


// Rework the hierarchy in to a graph where we have output edges resolved to pointers so we can pass to
// library functions.
static void GeneratePluginGraph(const UE::Cook::FCookMetadataPluginHierarchy& InPluginHierarchy, FPluginGraph& OutPluginGraph)
{
	double GeneratePluginGraphStart = FPlatformTime::Seconds();

	// Allocate up front so our pointer remains stable.
	OutPluginGraph.Plugins.Reserve(InPluginHierarchy.PluginsEnabledAtCook.Num());
	uint16 PluginIndex = 0;
	for (const UE::Cook::FCookMetadataPluginEntry& Plugin : InPluginHierarchy.PluginsEnabledAtCook)
	{
		FPluginGraphEntry& OurEntry = OutPluginGraph.Plugins.AddDefaulted_GetRef();
		OurEntry.IndexInEnabledPlugins = PluginIndex;
		OurEntry.Name = Plugin.Name;

		// Can store pointer since we reserved as a batch..
		OutPluginGraph.NameToPlugin.Add(OurEntry.Name, &OurEntry);
		PluginIndex++;
	}

	OutPluginGraph.RootPlugins.Reserve(InPluginHierarchy.RootPlugins.Num());
	for (uint16 RootIndex : InPluginHierarchy.RootPlugins)
	{
		FPluginGraphEntry* Root = OutPluginGraph.NameToPlugin[InPluginHierarchy.PluginsEnabledAtCook[RootIndex].Name];
		Root->bIsRoot = true;
		OutPluginGraph.RootPlugins.Add(Root);
	}

	for (FPluginGraphEntry& PluginEntry : OutPluginGraph.Plugins)
	{
		const UE::Cook::FCookMetadataPluginEntry& Plugin = InPluginHierarchy.PluginsEnabledAtCook[PluginEntry.IndexInEnabledPlugins];

		for (uint32 DependencyIndex = Plugin.DependencyIndexStart; DependencyIndex < Plugin.DependencyIndexEnd; DependencyIndex++)
		{
			const UE::Cook::FCookMetadataPluginEntry& DependentPlugin = InPluginHierarchy.PluginsEnabledAtCook[InPluginHierarchy.PluginDependencies[DependencyIndex]];
			PluginEntry.DirectDependencies.Add(OutPluginGraph.NameToPlugin[DependentPlugin.Name]);
		}
	}
	
	// From here on out we can operate entirely on our own data - no cook metadata structures - 
	// and we generate the various structures we need.

	// Sort the plugins topologically. This means that when we iterate linearly,
	// we know that when we hit a plugin, we've already processed the dependencies.
	// This takes some memory to track edges but is a depth first search
	// and not anything quadratic or worse.	
	double TopologicalSortStart = FPlatformTime::Seconds();
	{
		OutPluginGraph.TopologicallySortedPlugins.Reserve(OutPluginGraph.Plugins.Num());
		for (FPluginGraphEntry& Plugin : OutPluginGraph.Plugins)
		{
			OutPluginGraph.TopologicallySortedPlugins.Add(&Plugin);
		}

		auto GetElementDependencies = [&OutPluginGraph](const FPluginGraphEntry* PluginEntry) -> const TSet<FPluginGraphEntry*>&
		{
			return OutPluginGraph.NameToPlugin[PluginEntry->Name]->DirectDependencies;
		};

		Algo::TopologicalSort(OutPluginGraph.TopologicallySortedPlugins, GetElementDependencies);
	}


	// Gather the set of all dependencies. This ends up being technically
	// O(N^2) in the worst case. It's highly unlikely our plugin DAG will cause that, but we track the times
	// just so we can keep an eye on it if it ends up taking measurable amounts of time.
	double InclusiveComputeStart = FPlatformTime::Seconds();
	for (FPluginGraphEntry* Plugin : OutPluginGraph.TopologicallySortedPlugins)
	{
		for (FPluginGraphEntry* Dependency : Plugin->DirectDependencies)
		{
			Plugin->TotalDependencies.Add(Dependency);
			Dependency->DirectRefcount++;
		
			// In the worse case this is another O(N) iteration, which makes us overall O(N^2)
			for (FPluginGraphEntry* TotalDependencyEntry : Dependency->TotalDependencies)
			{
				Plugin->TotalDependencies.Add(TotalDependencyEntry);
			}
		}
	}
	double InclusiveComputeEnd = FPlatformTime::Seconds();

	// Generate the unique dependencies for the root plugins. This is the set of dependencies that only
	// belong to the root plugin and not to another. These dependencies could be referred to by another 
	// plugin within the unique set - the only requirement is that there exists no path from _another_
	// root to the dependency.
	for (FPluginGraphEntry* RootPlugin : OutPluginGraph.RootPlugins)
	{
		// Add us as a root entry for all our total dependencies so all plugins know which root they are in.
		for (FPluginGraphEntry* Dependency : RootPlugin->TotalDependencies)
		{
			OutPluginGraph.NameToPlugin[Dependency->Name]->Roots.Add(RootPlugin);
		}

		// Duplicate the TotalDependencies and then remove any dependency that
		// exists for another root.
		RootPlugin->UniqueDependencies = RootPlugin->TotalDependencies;
		for (FPluginGraphEntry* InnerRootPlugin : OutPluginGraph.RootPlugins)
		{
			if (RootPlugin == InnerRootPlugin)
			{
				continue;
			}

			for (FPluginGraphEntry* InnerRootDependency : InnerRootPlugin->TotalDependencies)
			{
				RootPlugin->UniqueDependencies.Remove(InnerRootDependency);
			}
		}
	}

	double UniqueEnd = FPlatformTime::Seconds();

	// Generate the unrooted set. These plugins can not trace a path from _any_ root plugin
	// to themselves. For projects with no root plugins, this will be all plugins.
	{
		for (FPluginGraphEntry* Plugin : OutPluginGraph.TopologicallySortedPlugins)
		{
			OutPluginGraph.UnrootedPlugins.Add(Plugin);
		}

		for (FPluginGraphEntry* Plugin : OutPluginGraph.RootPlugins)
		{
			OutPluginGraph.UnrootedPlugins.Remove(Plugin);
			for (FPluginGraphEntry* Dependency : Plugin->TotalDependencies)
			{
				OutPluginGraph.UnrootedPlugins.Remove(Plugin);
			}
		}
	}
	double GeneratePluginGraphEnd = FPlatformTime::Seconds();

	UE_LOG(LogIoStore, Log, TEXT("Generated plugin graph with %d nodes. Times: %.02f total %.02f setup, %.02f sort %.02f inclusive %.02f unique %.02f unrooted."),
		OutPluginGraph.TopologicallySortedPlugins.Num(),
		GeneratePluginGraphEnd - GeneratePluginGraphStart,
		TopologicalSortStart - GeneratePluginGraphStart,
		InclusiveComputeStart - TopologicalSortStart,
		InclusiveComputeEnd - InclusiveComputeStart,
		UniqueEnd - InclusiveComputeEnd,
		GeneratePluginGraphEnd - UniqueEnd);
}

static void InsertShadersInPluginHierarchy(UE::Cook::FCookMetadataState& InCookMetadata, FPluginGraph& InPluginGraph, FShaderAssociationInfo& InShaderAssociationInfo)
{
	const UE::Cook::FCookMetadataPluginHierarchy& PluginHierarchy = InCookMetadata.GetPluginHierarchy();

	//
	// Create any shader plugins we need. These are pseudo plugins that we create to hold the size information
	// when the packages that reference a plugin cross the plugin boundary. We name them based on their dependencies
	// so it's consistent across builds. These will exist whenever GFPs aren't placed entirely in their own pak chunk.
	//
	// The combinatorics are such that doing this for _all_ plugins isn't tenable. However, for product tracking we
	// actually only care about root GFPs. So instead of gathering all of the plugins entirely, we gather all of the
	// root plugins.
	//
	// The difficulty is that we don't necessarily _have_ any root plugins if the project hasn't defined any, so we 
	// artificially stuff such plugins under "Unrooted".
	//
	// It should be noted that the entire point of root GFPs is to separate the data entirely - so if we have any
	// of these pseudo plugins then there is a content bug as there exists a shared dependency between two "modes".
	//
	TMap<FString, TArray<FShaderAssociationInfo::FShaderChunkInfoKey>> ShaderPseudoPlugins;
	TMap<FString, TArray<FString>> PluginDependenciesOnShaders;

	TSet<FString> ShaderRootPossibles;
	ShaderRootPossibles.Add(TEXT("Unrooted"));

	for (TPair<FShaderAssociationInfo::FShaderChunkInfoKey, FShaderAssociationInfo::FShaderChunkInfo>& ShaderChunkInfo : InShaderAssociationInfo.ShaderChunkInfos)
	{
		// Only Normal shaders can be assigned
		if (ShaderChunkInfo.Value.Type != FShaderAssociationInfo::FShaderChunkInfo::Package)
		{
			continue;
		}

		TSet<FString> ShaderPlugins;
		for (FName PackageName : ShaderChunkInfo.Value.ReferencedByPackages)
		{
			FString PackageNameStr = PackageName.ToString();
			FStringView Plugin = FPackageName::SplitPackageNameRoot(PackageNameStr, nullptr);
			ShaderPlugins.Add(FString(Plugin));
		}

		if (ShaderPlugins.Num() == 1)
		{
			// We can assign the size to this plugin when the time comes - no pseudo plugin needed.
			continue;
		}

		bool bAllReferencingPluginsAreRooted = true;
		TSet<FString> ShaderRootPlugins;
		for (FString& ReferencingPluginName : ShaderPlugins)
		{
			bool bReferencingPluginIsRooted = false;
			FPluginGraphEntry** ReferencingPlugin = InPluginGraph.NameToPlugin.Find(ReferencingPluginName);
			if (ReferencingPlugin)
			{
				for (FPluginGraphEntry* RootForReferencingPlugin : (*ReferencingPlugin)->Roots)
				{
					ShaderRootPlugins.Add(RootForReferencingPlugin->Name);
					bReferencingPluginIsRooted = true;
				}
			}
				
			if (bReferencingPluginIsRooted == false)
				bAllReferencingPluginsAreRooted = false;
		}

		if (ShaderRootPlugins.Num() == 0 || bAllReferencingPluginsAreRooted == false)
		{
			// Place in the unrooted list.
			ShaderRootPlugins.Add(TEXT("Unrooted"));
		}

		TStringBuilder<256> PseudoPluginName;
		PseudoPluginName.Append(TEXT("ShaderPlugin"));

		TStringBuilder<256> NameConcatenation;

		// We can't just concat the names because it'll get too long when we use the name as a filename, which is unfortunate. That being said, we
		// only expect this to happen in degenerate cases - so if it's not too long we list the names for convenience
		// otherwise we hash it.
		for (FString& ReferencingPlugin : ShaderRootPlugins)
		{
			NameConcatenation.Append(TEXT("_"));
			NameConcatenation.Append(ReferencingPlugin);
		}

		if (NameConcatenation.Len() > 100) // arbitrary length here just to try and avoid hitting MAX_PATH (260)
		{
			FXxHash64 NameHash = FXxHash64::HashBuffer(NameConcatenation.GetData(), NameConcatenation.Len());
			uint8 HashBytes[8];
			NameHash.ToByteArray(HashBytes);

			PseudoPluginName.Append(TEXT("_"));
			UE::String::BytesToHexLower(MakeArrayView(HashBytes), PseudoPluginName);
		}
		else
		{
			PseudoPluginName.Append(NameConcatenation);
		}

		FString PseudoPluginNameStr = PseudoPluginName.ToString();
		for (FString& ReferencingPlugin : ShaderRootPlugins)
		{
			PluginDependenciesOnShaders.FindOrAdd(ReferencingPlugin).Add(PseudoPluginNameStr);
		}

		ShaderPseudoPlugins.FindOrAdd(PseudoPluginNameStr).Add(ShaderChunkInfo.Key);

		ShaderChunkInfo.Value.SharedPluginName = MoveTemp(PseudoPluginNameStr);
	}

	// We have the list of pseudo plugins we need to make... we need to copy and append to the list
	// of plugins in the cook metadata, after stripping out any previous run's pseudo plugins.
	TArray<UE::Cook::FCookMetadataPluginEntry> PluginEntries = PluginHierarchy.PluginsEnabledAtCook;
	for (int32 PluginIndex = 0; PluginIndex < PluginEntries.Num(); PluginIndex++)
	{
		if (PluginEntries[PluginIndex].Type == UE::Cook::ECookMetadataPluginType::Unassigned)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Found unassigned plugin type in cook metadata! %s"), *PluginEntries[PluginIndex].Name);
		}
		if (PluginEntries[PluginIndex].Type == UE::Cook::ECookMetadataPluginType::ShaderPseudo)
		{
			// We can do a swap because we insert these at the end in a group so there shouldn't
			// by anything else after us.
			PluginEntries.RemoveAtSwap(PluginIndex);
			PluginIndex--;
		}
	}

	for (const TPair< FString, TArray<FShaderAssociationInfo::FShaderChunkInfoKey>>& ShaderPP : ShaderPseudoPlugins)
	{
		UE::Cook::FCookMetadataPluginEntry& ShaderPluginEntry = PluginEntries.AddDefaulted_GetRef();
		ShaderPluginEntry.Name = ShaderPP.Key;
		ShaderPluginEntry.Type = UE::Cook::ECookMetadataPluginType::ShaderPseudo;
	}

	// We have to redo the dependency tree as well to add the shaders as dependencies.
	TMap<FString, int32> PluginNameToIndex;
	int32 CurrentIndex = 0;
	for (UE::Cook::FCookMetadataPluginEntry& Entry : PluginEntries)
	{
		PluginNameToIndex.Add(Entry.Name, CurrentIndex);
		CurrentIndex++;
	}

	if (IntFitsIn<uint16>(PluginEntries.Num()) == false)
	{
		UE_LOG(LogIoStore, Warning, TEXT("Post shared shader plugin count is > 65535 (%d)  - not updating cook metadata!"), PluginEntries.Num());
		return;
	}

	TArray<uint16> DependencyList;
	for (UE::Cook::FCookMetadataPluginEntry& Entry : PluginEntries)
	{
		// Add the normal dependencies. Since we didn't reorder anything we can
		// use the old dependency list
		uint32 StartIndex = (uint32)DependencyList.Num();
		for (uint32 DependencyIndex = Entry.DependencyIndexStart; DependencyIndex < Entry.DependencyIndexEnd; DependencyIndex++)
		{
			const UE::Cook::FCookMetadataPluginEntry& DependentPlugin = PluginHierarchy.PluginsEnabledAtCook[PluginHierarchy.PluginDependencies[DependencyIndex]];
			if (DependentPlugin.Type == UE::Cook::ECookMetadataPluginType::ShaderPseudo)
			{
				// If we are rerunning stage then we might already have added shader plugins
				// as dependencies, which we've removed so they won't exist in the lookup
				// (and we want to redo them anyway).
				continue;
			}
			int32* PluginIndex = PluginNameToIndex.Find(DependentPlugin.Name);
			if (PluginIndex)
			{
				DependencyList.Add(*PluginIndex);
			}
			else
			{
				UE_LOG(LogIoStore, Warning, TEXT("Couldn't find plugin %s when re-adding dependencies."), *DependentPlugin.Name);
			}
		}

		// However we also need to check for shaders
		TArray<FString>* DependenciesOnShader = PluginDependenciesOnShaders.Find(Entry.Name);
		if (DependenciesOnShader)
		{
			for (FString& ShaderPluginName : (*DependenciesOnShader))
			{
				int32* PluginIndex = PluginNameToIndex.Find(ShaderPluginName);
				if (PluginIndex)
				{
					DependencyList.Add(*PluginIndex);
				}
				else
				{
					UE_LOG(LogIoStore, Warning, TEXT("Couldn't find shader pseudo plugin %s when adding dependencies."), *ShaderPluginName);
				}
			}
		}

		Entry.DependencyIndexStart = StartIndex;
		Entry.DependencyIndexEnd = (uint32)DependencyList.Num();
	}

	// Now blast the old one away and replace.
	UE::Cook::FCookMetadataPluginHierarchy& MutablePluginHierarchy = InCookMetadata.GetMutablePluginHierarchy();
	MutablePluginHierarchy.PluginDependencies = MoveTemp(DependencyList);
	MutablePluginHierarchy.PluginsEnabledAtCook = PluginEntries;

	// Sanity check we assigned plugin types
	for (UE::Cook::FCookMetadataPluginEntry& Entry : MutablePluginHierarchy.PluginsEnabledAtCook)
	{
		if (Entry.Type == UE::Cook::ECookMetadataPluginType::Unassigned)
		{
			UE_LOG(LogIoStore, Warning, TEXT("We caused an unassigned plugin type in shader pseudo plugin generation! %s"), *Entry.Name);
		}
	}
}

/**
*	Use the name of the package to assign sizes so that we can track build size at a per-plugin level,
*	and write out jsons files for each plugin in to the cooked metadata directory.
* 
*	Plugins insert themselves in to the package's path at the top level. Content that is unassigned
*	to a plugin has either /Engine or /Game as it's top level path and will be assigned to a pseudo
*	plugin.
*/
static void UpdatePluginMetadataAndWriteJsons(
	const FString& InAssetRegistryFileName, 
	TMap<FPackageId, TArray<FIoStoreChunkSource, TInlineAllocator<2>>>& PackageToChunks, 
	FAssetRegistryState& AssetRegistry, 
	UE::Cook::FCookMetadataState& CookMetadata, 
	FShaderAssociationInfo* InShaderAssociationInfo)
{
	double WritePluginStart = FPlatformTime::Seconds();

	FPluginGraph PluginGraph;
	GeneratePluginGraph(CookMetadata.GetPluginHierarchy(), PluginGraph);

	if (InShaderAssociationInfo)
	{
		InsertShadersInPluginHierarchy(CookMetadata, PluginGraph, *InShaderAssociationInfo);

		// Generate the graph aggain after we've inserted the new "plugins".
		PluginGraph = FPluginGraph();
		GeneratePluginGraph(CookMetadata.GetPluginHierarchy(), PluginGraph);
	}

	double GeneratePluginGraphEnd = FPlatformTime::Seconds();

	TSet<FString> LoggedPluginNames;

	FTopLevelAssetPath Texture2DPath(TEXT("/Script/Engine.Texture2D"));
	FTopLevelAssetPath Texture2DArrayPath(TEXT("/Script/Engine.Texture2DArray"));
	FTopLevelAssetPath Texture3DPath(TEXT("/Script/Engine.Texture3D"));
	FTopLevelAssetPath TextureCubePath(TEXT("/Script/Engine.TextureCube"));
	FTopLevelAssetPath TextureCubeArrayPath(TEXT("/Script/Engine.TextureCubeArray"));
	FTopLevelAssetPath VirtualTextureBuilderPath(TEXT("/Script/Engine.VirtualTextureBuilder"));
	FTopLevelAssetPath StaticMeshPath(TEXT("/Script/Engine.StaticMesh"));
	FTopLevelAssetPath SoundWavePath(TEXT("/Script/Engine.SoundWave"));
	FTopLevelAssetPath SkeletalMeshPath(TEXT("/Script/Engine.SkeletalMesh"));
	FTopLevelAssetPath LevelPath(TEXT("/Script/Engine.World"));
	FTopLevelAssetPath BlueprintPath(TEXT("/Script/Engine.BlueprintGeneratedClass"));
	FTopLevelAssetPath AnimationSequencePath(TEXT("/Script/Engine.AnimSequence"));
	FTopLevelAssetPath GeometryCollectionPath(TEXT("/Script/GeometryCollectionEngine.GeometryCollection"));
	FTopLevelAssetPath NiagaraSystemPath(TEXT("/Script/Niagara.NiagaraSystem"));
	FTopLevelAssetPath MaterialInstancePath(TEXT("/Script/Engine.MaterialInstanceConstant"));
	
	if (InShaderAssociationInfo)
	{
		//
		// Create a bunch of "Assets" that we can iterate over in the same manner as a normal
		// asset and find its containing "plugin" for the purposes of assigning its size.
		//
		TArray<UE::Cook::FCookMetadataShaderPseudoAsset> ShaderPseudoAssets;

		TMap<FName, TArray<int32>> PackageDependencyMap;

		// All package shaders should now either be able to be assigned to:
		// 1. a single package (i.e. plugin)
		// 2. shared between packages in 1 plugin (i.e. a plugin)
		// 3. shared between packages across plugins (i.e. assignable to a pseudo plugin we created earlier).
		//
		uint64 CrossPluginShaderSize = 0;
		uint64 SinglePluginShaderSize = 0;
		uint64 InlineShaderSize = 0;
		uint64 GlobalShaderSize = 0;
		uint64 OrphanShaderSize = 0;
		for (TPair<FShaderAssociationInfo::FShaderChunkInfoKey, FShaderAssociationInfo::FShaderChunkInfo>& ShaderChunkInfo : InShaderAssociationInfo->ShaderChunkInfos)
		{
			if (ShaderChunkInfo.Value.Type == FShaderAssociationInfo::FShaderChunkInfo::Orphan)
			{
				OrphanShaderSize += ShaderChunkInfo.Value.CompressedSize;
				continue;
			}
			if (ShaderChunkInfo.Value.Type == FShaderAssociationInfo::FShaderChunkInfo::Global)
			{
				GlobalShaderSize += ShaderChunkInfo.Value.CompressedSize;
				continue;
			}

			check(ShaderChunkInfo.Value.Type == FShaderAssociationInfo::FShaderChunkInfo::Package);

			TStringBuilder<128> PackageName;
			if (ShaderChunkInfo.Value.SharedPluginName.Len())
			{
				// We know we belong to this plugin.
				PackageName.Append(ShaderChunkInfo.Value.SharedPluginName);
				CrossPluginShaderSize += ShaderChunkInfo.Value.CompressedSize;
			}
			else
			{
				// We know all the plugin prefixes are the same for all referrers
				if (ShaderChunkInfo.Value.ReferencedByPackages.Num() == 1)
				{
					InlineShaderSize += ShaderChunkInfo.Value.CompressedSize;
				}
				else
				{
					SinglePluginShaderSize += ShaderChunkInfo.Value.CompressedSize;
				}

				FString PackageNameStr = ShaderChunkInfo.Value.ReferencedByPackages[0].ToString();
				FStringView Plugin = FPackageName::SplitPackageNameRoot(PackageNameStr, nullptr);
				PackageName.Append(Plugin);
			}

			FPluginGraphEntry** PluginEntryPtr = PluginGraph.NameToPlugin.Find(PackageName.ToString());
			if (PluginEntryPtr)
			{
				PluginEntryPtr[0]->ExclusiveSizes[(uint8)EPluginGraphSizeClass::All][ShaderChunkInfo.Value.SizeType] += ShaderChunkInfo.Value.CompressedSize;
				PluginEntryPtr[0]->ExclusiveSizes[(uint8)EPluginGraphSizeClass::Shader][ShaderChunkInfo.Value.SizeType] += ShaderChunkInfo.Value.CompressedSize;
			}
			else
			{
				FString AllocatedPluginName(PackageName.ToString());
				bool bAlreadyLogged = false;
				LoggedPluginNames.Add(AllocatedPluginName, &bAlreadyLogged);
				if (bAlreadyLogged == false)
				{
					UE_LOG(LogIoStore, Warning, TEXT("Plugin for shader not found: %s"), *AllocatedPluginName);
				}
			}


			// What to name our package? Needs to be unique. We just concat everything so a human
			// can trace where it came from.
			PackageName.Append(TEXT("/ShaderPseudoAsset_"));

			PackageName.Append(ShaderChunkInfo.Key.PakChunkName.ToString());
			PackageName.Append(TEXT("_"));

			// shader hash (iochunkid)
			UE::String::BytesToHexLower(MakeArrayView(ShaderChunkInfo.Key.IoChunkId.GetData(), sizeof(FIoChunkId)), PackageName);

			ShaderPseudoAssets.Add({PackageName.ToString(), ShaderChunkInfo.Value.CompressedSize});

			// Track who depends on this shader by index.
			for (FName ReferencingPackage : ShaderChunkInfo.Value.ReferencedByPackages)
			{
				TArray<int32>& PackageDependencies = PackageDependencyMap.FindOrAdd(ReferencingPackage);
				PackageDependencies.Add(ShaderPseudoAssets.Num() - 1);
			}
		}

		TMap<FName, TPair<int32, int32>> FinalizedDependencyMap;

		// Now convert the package dependency map in to array ranges.
		TArray<int32> DependencyByIndex;
		for (TPair<FName, TArray<int32>>& Dependencies : PackageDependencyMap)
		{
			TPair<int32, int32>& Entry = FinalizedDependencyMap.Add(Dependencies.Key);
			Entry.Key = DependencyByIndex.Num();
			DependencyByIndex.Append(Dependencies.Value);
			Entry.Value = DependencyByIndex.Num();
		}

		// Move the new info over to the cook metadata.
		UE::Cook::FCookMetadataShaderPseudoHierarchy PSH;
		PSH.ShaderAssets = MoveTemp(ShaderPseudoAssets);
		PSH.PackageShaderDependencyMap = MoveTemp(FinalizedDependencyMap);
		PSH.DependencyList = MoveTemp(DependencyByIndex);
		CookMetadata.SetShaderPseudoHieararchy(MoveTemp(PSH));

		double TotalShaderSize = (double)(InlineShaderSize + CrossPluginShaderSize + SinglePluginShaderSize + GlobalShaderSize + OrphanShaderSize);
		UE_LOG(LogIoStore, Display, TEXT("Shader total sizes: %s single package (%.0f%%), %s single root GFP (%.0f%%), %s cross root GFP (%.0f%%), %s global (%.0f%%), %s orphan (%.0f%%) - %s assigned (%.0f%%)"),
			*NumberString(InlineShaderSize), 100.0 * InlineShaderSize / TotalShaderSize,
			*NumberString(SinglePluginShaderSize), 100.0 * SinglePluginShaderSize / TotalShaderSize,
			*NumberString(CrossPluginShaderSize), 100.0 * CrossPluginShaderSize / TotalShaderSize,
			*NumberString(GlobalShaderSize), 100.0 * GlobalShaderSize / TotalShaderSize,
			*NumberString(OrphanShaderSize), 100.0 * OrphanShaderSize / TotalShaderSize,
			*NumberString(InlineShaderSize + SinglePluginShaderSize + CrossPluginShaderSize), 100.0 * (InlineShaderSize + SinglePluginShaderSize + CrossPluginShaderSize) / TotalShaderSize
		);
	}

	double AssetPackageMapStart = FPlatformTime::Seconds();
	const TMap<FName, const FAssetPackageData*> AssetPackageMap = AssetRegistry.GetAssetPackageDataMap();
	for (const TPair<FName, const FAssetPackageData*>& AssetPackage : AssetPackageMap)
	{
		if (AssetPackage.Value->DiskSize < 0)
		{
			// No data on disk!
			continue;
		}

		// Grab the most important asset out and use it to track largest asset classes for the plugin.
		// This might be null!
		const FAssetData* AssetData = UE::AssetRegistry::GetMostImportantAsset(AssetRegistry.GetAssetsByPackageName(AssetPackage.Key), UE::AssetRegistry::EGetMostImportantAssetFlags::IgnoreSkipClasses);
		
		const TArray<FIoStoreChunkSource, TInlineAllocator<2>>* PackageChunks = PackageToChunks.Find(FPackageId::FromName(AssetPackage.Key));
		if (PackageChunks == nullptr)
		{
			// This happens when the package has been stripped by UAT prior to staging by e.g. PakDenyList.
			continue;
		}

		UE::Cook::FPluginSizeInfo PackageSizes;
		for (const FIoStoreChunkSource& ChunkInfo : *PackageChunks)
		{
			PackageSizes[ChunkInfo.SizeType] += ChunkInfo.ChunkInfo.CompressedSize;
		}

		// Assign the size to the package's plugin.
		{
			TStringBuilder<FName::StringBufferSize> PackageNameStr(InPlace, AssetPackage.Key);
			FStringView PackageName(PackageNameStr);

			FStringView PluginName = FPackageName::SplitPackageNameRoot(PackageName, nullptr);
			FPluginGraphEntry** PluginEntryPtr = PluginGraph.NameToPlugin.Find(PluginName);
			if (PluginEntryPtr)
			{
				FPluginGraphEntry* PluginEntry = *PluginEntryPtr;
				PluginEntry->ExclusiveSizes[(uint8)EPluginGraphSizeClass::All].Add(PackageSizes);

				// If we have asset class info and it's a top contender, track it also.
				if (AssetData != nullptr)
				{
					EPluginGraphSizeClass AssetSizeClass = EPluginGraphSizeClass::Other;
					if (AssetData->AssetClassPath == Texture2DPath ||
						AssetData->AssetClassPath == Texture3DPath ||
						AssetData->AssetClassPath == TextureCubePath ||
						AssetData->AssetClassPath == TextureCubeArrayPath ||
						AssetData->AssetClassPath == Texture2DArrayPath || 
						AssetData->AssetClassPath == VirtualTextureBuilderPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Texture;
					}
					else if (AssetData->AssetClassPath == StaticMeshPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::StaticMesh;
					}
					else if (AssetData->AssetClassPath == SoundWavePath)
					{
						AssetSizeClass = EPluginGraphSizeClass::SoundWave;
					}
					else if (AssetData->AssetClassPath == SkeletalMeshPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::SkeletalMesh;
					}
					else if (AssetData->AssetClassPath == AnimationSequencePath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Animation;
					}
					else if (AssetData->AssetClassPath == NiagaraSystemPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Niagara;
					}
					else if (AssetData->AssetClassPath == MaterialInstancePath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Material;
					}
					else if (AssetData->AssetClassPath == LevelPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Level;
					}
					else if (AssetData->AssetClassPath == BlueprintPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Blueprint;
					}
					else if (AssetData->AssetClassPath == GeometryCollectionPath)
					{
						AssetSizeClass = EPluginGraphSizeClass::Geometry;
					}
					
					// Note that we can't get shaders here so we don't need to handle ::Shader.

					PluginEntry->ExclusiveSizes[(uint8)AssetSizeClass].Add(PackageSizes);
					PluginEntry->ExclusiveCounts[(uint8)AssetSizeClass]++;
				}
			}
			else
			{
				FString AllocatedPluginName(PluginName);
				bool bAlreadyLogged = false;
				LoggedPluginNames.Add(MoveTemp(AllocatedPluginName), &bAlreadyLogged);
				if (bAlreadyLogged == false)
				{
					UE_LOG(LogIoStore, Display, TEXT("Plugin for package not found: %s (%.*s)"), PackageNameStr.GetData(), PluginName.Len(), PluginName.GetData());
				}
			}
		}
	}

	// Inclusive is the sum of us plus all our dependencies.
	for (FPluginGraphEntry* PluginEntry : PluginGraph.TopologicallySortedPlugins)
	{
		for (uint8 ClassIndex = 0; ClassIndex < FPluginGraphEntry::ClassCount; ClassIndex++)
		{
			PluginEntry->InclusiveSizes[ClassIndex] = PluginEntry->ExclusiveSizes[ClassIndex];
			PluginEntry->InclusiveCounts[ClassIndex] = PluginEntry->ExclusiveCounts[ClassIndex];
		}

		for (FPluginGraphEntry* Dependency : PluginEntry->TotalDependencies)
		{
			for (uint8 ClassIndex = 0; ClassIndex < FPluginGraphEntry::ClassCount; ClassIndex++)
			{
				PluginEntry->InclusiveSizes[ClassIndex].Add(Dependency->ExclusiveSizes[ClassIndex]);
				PluginEntry->InclusiveCounts[ClassIndex] += Dependency->ExclusiveCounts[ClassIndex];
			}			
		}
	}

	// Now we need to find the unique size for each root plugin. This is the size of dependencies that only
	// belong to the root plugin and not to another. Conceptually this is the "assuming all other roots are
	// installed, this is the size cost to add this plugin to the install".
	for (FPluginGraphEntry* RootPlugin : PluginGraph.RootPlugins)
	{
		for (FPluginGraphEntry* UniqueDependency : RootPlugin->UniqueDependencies)
		{
			for (uint8 ClassIndex = 0; ClassIndex < FPluginGraphEntry::ClassCount; ClassIndex++)
			{
				RootPlugin->UniqueSizes[ClassIndex].Add(UniqueDependency->ExclusiveSizes[ClassIndex]);
				RootPlugin->UniqueCounts[ClassIndex] += UniqueDependency->ExclusiveCounts[ClassIndex];
			}
		}
	}
	
	// Find the total size of all plugins that aren't rooted in the root set.
	UE::Cook::FPluginSizeInfo UnrootedTotal;
	for (FPluginGraphEntry* Plugin : PluginGraph.UnrootedPlugins)
	{		
		UnrootedTotal.Add(Plugin->ExclusiveSizes[(uint8)EPluginGraphSizeClass::All]);
	}

	double WriteBegin = FPlatformTime::Seconds();

	UE::Cook::FCookMetadataPluginHierarchy& MutablePluginHierarchy = CookMetadata.GetMutablePluginHierarchy();

	auto GeneratePluginJson = [&MutablePluginHierarchy](TUtf8StringBuilder<4096>& OutPluginMetadataJson, FStringView InName, const FPluginGraphEntry& InGraphEntry, EPluginGraphSizeClass InSizeClass)
	{
		OutPluginMetadataJson.Reset();

		uint8 SizeClass = (uint8)InSizeClass;

		if (InGraphEntry.InclusiveSizes[SizeClass].TotalSize()== 0)
		{
			// Asset type or its dependencies do not contribute to the record.
			return;
		}
		
		OutPluginMetadataJson << "{\n";
		OutPluginMetadataJson << "\t\"name\":\"" << InName << "\",\n";

		OutPluginMetadataJson << "\t\"schema_version\":4,\n";

		OutPluginMetadataJson << "\t\"is_root_plugin\":" << (InGraphEntry.bIsRoot ? TEXTVIEW("true") : TEXTVIEW("false")) << ",\n";
		OutPluginMetadataJson << "\t\"asset_sizes_class\":\"" << PluginGraphEntryClassNames[SizeClass] << "\",\n";
		OutPluginMetadataJson << "\t\"exclusive_asset_class_count\":" << InGraphEntry.ExclusiveCounts[SizeClass] << ",\n";

		OutPluginMetadataJson << "\t\"exclusive_installed\":" << InGraphEntry.ExclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::Installed] << ",\n";
		OutPluginMetadataJson << "\t\"exclusive_optional\":" << InGraphEntry.ExclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::Optional] << ",\n";
		OutPluginMetadataJson << "\t\"exclusive_ias\":" << InGraphEntry.ExclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::Streaming] << ",\n";
		OutPluginMetadataJson << "\t\"exclusive_optionalsegment\":" << InGraphEntry.ExclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::OptionalSegment] << ",\n";
		
		OutPluginMetadataJson << "\t\"inclusive_installed\":" << InGraphEntry.InclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::Installed] << ",\n";
		OutPluginMetadataJson << "\t\"inclusive_optional\":" << InGraphEntry.InclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::Optional] << ",\n";
		OutPluginMetadataJson << "\t\"inclusive_ias\":" << InGraphEntry.InclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::Streaming] << ",\n";
		OutPluginMetadataJson << "\t\"inclusive_optionalsegment\":" << InGraphEntry.InclusiveSizes[SizeClass][UE::Cook::EPluginSizeTypes::OptionalSegment] << ",\n";

		// this only has values for is_root_plugin == true.
		OutPluginMetadataJson << "\t\"unique_installed\":" << InGraphEntry.UniqueSizes[SizeClass][UE::Cook::EPluginSizeTypes::Installed] << ",\n";
		OutPluginMetadataJson << "\t\"unique_optional\":" << InGraphEntry.UniqueSizes[SizeClass][UE::Cook::EPluginSizeTypes::Optional] << ",\n";
		OutPluginMetadataJson << "\t\"unique_ias\":" << InGraphEntry.UniqueSizes[SizeClass][UE::Cook::EPluginSizeTypes::Streaming] << ",\n";
		OutPluginMetadataJson << "\t\"unique_optionalsegment\":" << InGraphEntry.UniqueSizes[SizeClass][UE::Cook::EPluginSizeTypes::OptionalSegment] << ",\n";

		OutPluginMetadataJson << "\t\"direct_refcount\":" << InGraphEntry.DirectRefcount << ",\n";

		// pass through any custom fields that were added to the cook metadata.
		if (InGraphEntry.IndexInEnabledPlugins != TNumericLimits<uint16>::Max())
		{
			const UE::Cook::FCookMetadataPluginEntry& CookMetadataEntry = MutablePluginHierarchy.PluginsEnabledAtCook[InGraphEntry.IndexInEnabledPlugins];
			for (const TPair<uint8, UE::Cook::FCookMetadataPluginEntry::CustomFieldVariantType>& CustomValue : CookMetadataEntry.CustomFields)
			{
				const FString& FieldName = MutablePluginHierarchy.CustomFieldEntries[CustomValue.Key].Name;
				UE::Cook::ECookMetadataCustomFieldType FieldType = MutablePluginHierarchy.CustomFieldEntries[CustomValue.Key].Type;

				OutPluginMetadataJson << "\t\"" << FieldName;
				
				if (CustomValue.Value.IsType<bool>())
				{
					check(FieldType == UE::Cook::ECookMetadataCustomFieldType::Bool);
					OutPluginMetadataJson << (CustomValue.Value.Get<bool>() ? "\":true,\n" : "\":false,\n");
				}
				else
				{
					check(FieldType == UE::Cook::ECookMetadataCustomFieldType::String);
					OutPluginMetadataJson << "\":\"" << CustomValue.Value.Get<FString>() << "\",\n";
				}

			}
		}

		{
			OutPluginMetadataJson << "\t\"roots\":[";

			int32 RootIndex = 0;
			for (FPluginGraphEntry* Root : InGraphEntry.Roots)
			{
				
				OutPluginMetadataJson << "\"" << Root->Name << "\"";
				if (RootIndex + 1 < InGraphEntry.Roots.Num())
				{
					OutPluginMetadataJson << ",";
				}
				RootIndex++;
			}

			OutPluginMetadataJson << "]\n";
		}



		OutPluginMetadataJson << "}\n";
	};

	//
	// Generate the plugin_summary jsons.	
	//

	// Also write a csv for easier browsing in spreadsheets.
	TUtf8StringBuilder<4096> Csv;
	Csv.Append("name,asset_sizes_class,exclusive_installed,exclusive_optional,exclusive_ias,inclusive_installed,inclusive_optional,inclusive_ias,unique_installed,unique_optional,unique_ias,direct_refcount,total_dependency_count\n");


	// This is so re-staging the same cook is consistent.	
	for (UE::Cook::FCookMetadataPluginEntry& Plugin : MutablePluginHierarchy.PluginsEnabledAtCook)
	{
		Plugin.InclusiveSizes.Zero();
		Plugin.ExclusiveSizes.Zero();
	}
	
	// Instead of writing out a ton of small event files we concat them all. There are a lot in a mature project.
	// This will be megabytes.
	TArray<UTF8CHAR> PluginMetadataFullJson;
	uint32 PluginsAddedToFullJson = 0;
	PluginMetadataFullJson.Append(UTF8TEXTVIEW("{ \"PluginSizeInfos\": ["));

	auto AddPluginJsonToFull = [&PluginMetadataFullJson, &PluginsAddedToFullJson](TUtf8StringBuilder<4096>& InJsonToAdd)
	{
		if (PluginsAddedToFullJson)
		{
			PluginMetadataFullJson.Add(UTF8TEXT(','));
		}
		PluginsAddedToFullJson++;
		PluginMetadataFullJson.Append(InJsonToAdd.GetData(), InJsonToAdd.Len());
	};

	TUtf8StringBuilder<4096> PluginMetadataJson;

	for (UE::Cook::FCookMetadataPluginEntry& Plugin : MutablePluginHierarchy.PluginsEnabledAtCook)
	{
		const FPluginGraphEntry& PluginEntry = *PluginGraph.NameToPlugin[Plugin.Name];
		if (PluginEntry.InclusiveSizes[(uint8)EPluginGraphSizeClass::All].TotalSize() == 0)
		{
			continue;
		}

		Plugin.InclusiveSizes = PluginEntry.InclusiveSizes[(uint8)EPluginGraphSizeClass::All];
		Plugin.ExclusiveSizes = PluginEntry.ExclusiveSizes[(uint8)EPluginGraphSizeClass::All];

		for (uint8 ClassIndex = 0; ClassIndex < FPluginGraphEntry::ClassCount; ClassIndex++)
		{

			GeneratePluginJson(PluginMetadataJson, Plugin.Name, PluginEntry, (EPluginGraphSizeClass)ClassIndex);

			if (PluginMetadataJson.Len()>0)
			{
				AddPluginJsonToFull(PluginMetadataJson);
				Csv.Appendf("%ls,%s,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u\n", *Plugin.Name, PluginGraphEntryClassNames[ClassIndex],
					PluginEntry.ExclusiveSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Installed],
					PluginEntry.ExclusiveSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Optional],
					PluginEntry.ExclusiveSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Streaming],
					PluginEntry.InclusiveSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Installed],
					PluginEntry.InclusiveSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Optional],
					PluginEntry.InclusiveSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Streaming],
					PluginEntry.UniqueSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Installed],
					PluginEntry.UniqueSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Optional],
					PluginEntry.UniqueSizes[ClassIndex][UE::Cook::EPluginSizeTypes::Streaming],
					PluginEntry.DirectRefcount, PluginEntry.TotalDependencies.Num());
			}
		}
	}

	// Also write a json that contains the sizes for the plugins that don't belong to any root plugin,
	// so that size information never gets lost.
	{
		FPluginGraphEntry OrphanedEntry;
		OrphanedEntry.bIsRoot = true;
		OrphanedEntry.DirectRefcount = 0;
		OrphanedEntry.InclusiveSizes[(uint8)EPluginGraphSizeClass::All] = UnrootedTotal;
		GeneratePluginJson(PluginMetadataJson, TEXT("OrphanedPlugins"), OrphanedEntry, EPluginGraphSizeClass::All);
		if (PluginMetadataJson.Len() > 0)
		{
			AddPluginJsonToFull(PluginMetadataJson);
		}

		Csv.Appendf("OrphanedPlugins,all,0,0,0,%llu,%llu,%llu,0,%u\n",
			UnrootedTotal[UE::Cook::EPluginSizeTypes::Installed], UnrootedTotal[UE::Cook::EPluginSizeTypes::Optional], UnrootedTotal[UE::Cook::EPluginSizeTypes::Streaming],
			PluginGraph.UnrootedPlugins.Num());
	}

	PluginMetadataFullJson.Append(UTF8TEXTVIEW("]}"));
	{
		FString JsonFilename = FPaths::GetPath(InAssetRegistryFileName) / TEXT("plugin_size_jsons.json");
		if (WriteUtf8StringView(MakeStringView(PluginMetadataFullJson), JsonFilename) == false)
		{
			UE_LOG(LogIoStore, Error, TEXT("Unable to write plugin json file: %s"), *JsonFilename);
			return;
		}
	}

	{
		FString CsvFilename = FPaths::GetPath(InAssetRegistryFileName) / TEXT("plugin_sizes.csv");
		if (WriteUtf8StringView(Csv.ToView(), CsvFilename) == false)
		{
			UE_LOG(LogIoStore, Error, TEXT("Unable to write plugin csv file: %s"), *CsvFilename);
			return;
		}
	}

	double WritePluginEnd = FPlatformTime::Seconds();
	UE_LOG(LogIoStore, Display, TEXT("Wrote plugin size jsons/csv in %.2f seconds [graph %.2f shaders %.2f sizes %.2f writes %.2f]"), 
		WritePluginEnd - WritePluginStart, 
		GeneratePluginGraphEnd - WritePluginStart, 
		AssetPackageMapStart - GeneratePluginGraphEnd,
		WriteBegin - AssetPackageMapStart,
		WritePluginEnd - WriteBegin);
}


static void AddChunkInfoToAssetRegistry(TMap<FPackageId, TArray<FIoStoreChunkSource, TInlineAllocator<2>>>&& PackageToChunks, FAssetRegistryState& AssetRegistry, const FShaderAssociationInfo* ShaderSizeInfo, uint64 InUnassignableShaderCodeBytes, uint64 InAssignableShaderCodeBytes, uint64 TotalCompressedSize)
{
	//
	// The asset registry has the chunks associate with each package, so we can just iterate the
	// packages, look up the chunk info, and then save the tags.
	//
	// The complicated thing is (as usual), trying to determine which asset gets the blame for the
	// data. We use the GetMostImportantAsset function for this.
	//
	const TMap<FName, const FAssetPackageData*> AssetPackageMap = AssetRegistry.GetAssetPackageDataMap();

	uint64 AssetsCompressedSize = 0;
	uint64 UpdatedAssetCount = 0;

	for (const TPair<FName, const FAssetPackageData*>& AssetPackage : AssetPackageMap)
	{
		if (AssetPackage.Value->DiskSize < 0)
		{
			// No data on disk!
			continue;
		}

		FPackageId PackageId = FPackageId::FromName(AssetPackage.Key);

		const FAssetData* AssetData = UE::AssetRegistry::GetMostImportantAsset(AssetRegistry.GetAssetsByPackageName(AssetPackage.Key), UE::AssetRegistry::EGetMostImportantAssetFlags::IgnoreSkipClasses);
		if (AssetData == nullptr)
		{
			// e.g. /Script packages.
			continue;
		}

		const TArray<FIoStoreChunkSource, TInlineAllocator<2>>* PackageChunks = PackageToChunks.Find(PackageId);
		if (PackageChunks == nullptr)
		{
			// This happens when the package has been stripped by UAT prior to staging by e.g. PakDenyList.
			continue;
		}

		UE::Cook::FPluginSizeInfo PackageCompressedSize;
		UE::Cook::FPluginSizeInfo PackageSize;
		int32 ChunkCount = 0;
		for (const FIoStoreChunkSource& ChunkInfo : *PackageChunks)
		{
			ChunkCount++;
			PackageSize[ChunkInfo.SizeType] += ChunkInfo.ChunkInfo.Size;
			PackageCompressedSize[ChunkInfo.SizeType] += ChunkInfo.ChunkInfo.CompressedSize;
		}

		FAssetDataTagMap TagsAndValues;
		TagsAndValues.Add(UE::AssetRegistry::Stage_ChunkCountFName, LexToString(ChunkCount));
		TagsAndValues.Add(UE::AssetRegistry::Stage_ChunkSizeFName, LexToString(PackageSize.TotalSize()));
		TagsAndValues.Add(UE::AssetRegistry::Stage_ChunkCompressedSizeFName, LexToString(PackageCompressedSize.TotalSize()));
		TagsAndValues.Add(UE::AssetRegistry::Stage_ChunkInstalledSizeFName, LexToString(PackageCompressedSize[UE::Cook::EPluginSizeTypes::Installed]));
		TagsAndValues.Add(UE::AssetRegistry::Stage_ChunkStreamingSizeFName, LexToString(PackageCompressedSize[UE::Cook::EPluginSizeTypes::Streaming]));
		TagsAndValues.Add(UE::AssetRegistry::Stage_ChunkOptionalSizeFName, LexToString(PackageCompressedSize[UE::Cook::EPluginSizeTypes::Optional]));
		AssetRegistry.AddTagsToAssetData(AssetData->GetSoftObjectPath(), MoveTemp(TagsAndValues));

		// We assign a package's chunks to a single asset, remove it from the list so that
		// at the end we can track how many chunks don't get assigned.
		PackageToChunks.Remove(PackageId);

		UpdatedAssetCount++;
		AssetsCompressedSize += PackageCompressedSize.TotalSize();
	}
	
	// PackageToChunks now has chunks that we never assigned to an asset, and so aren't accounted for.
	uint64 RemainingByType[(uint8)EIoChunkType::MAX] = {};
	for (auto PackageChunks : PackageToChunks)
	{
		for (FIoStoreChunkSource& Info : PackageChunks.Value)
		{
			RemainingByType[(uint8)Info.ChunkInfo.ChunkType] += Info.ChunkInfo.CompressedSize;
		}
	}
	
	// Shaders aren't in the PackageToChunks map, but we want to numbers reported to include them.
	RemainingByType[(uint8)EIoChunkType::ShaderCode] += InUnassignableShaderCodeBytes;
	AssetsCompressedSize += InAssignableShaderCodeBytes;

	double PercentAssets = 1.0f;
	if (TotalCompressedSize != 0)
	{
		PercentAssets = AssetsCompressedSize / (double)TotalCompressedSize;
	}

	UE_LOG(LogIoStore, Display, TEXT("Added chunk metadata to %s assets."), *FText::AsNumber(UpdatedAssetCount).ToString());
	UE_LOG(LogIoStore, Display, TEXT("Assets represent %s bytes of %s chunk bytes (%.1f%%)"), *FText::AsNumber(AssetsCompressedSize).ToString(), *FText::AsNumber(TotalCompressedSize).ToString(), 100 * PercentAssets);
	UE_LOG(LogIoStore, Display, TEXT("Remaining data by chunk type:"));
	for (uint8 TypeIndex = 0; TypeIndex < (uint8)EIoChunkType::MAX; TypeIndex++)
	{
		if (RemainingByType[TypeIndex] != 0)
		{
			UE_LOG(LogIoStore, Display, TEXT("    %-24s%s"), *LexToString((EIoChunkType)TypeIndex), *FText::AsNumber(RemainingByType[TypeIndex]).ToString());
		}
	}
}

// Returns the hash of the development asset registry or 0 on failure.
static uint64 LoadAssetRegistry(const FString& InAssetRegistryFileName, FAssetRegistryState& OutAssetRegistry)
{
	FAssetRegistryVersion::Type Version;
	FAssetRegistryLoadOptions Options(UE::AssetRegistry::ESerializationTarget::ForDevelopment);

	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*InAssetRegistryFileName));
	if (FileReader)
	{
		TArray64<uint8> Data;
		Data.SetNumUninitialized(FileReader->TotalSize());
		FileReader->Serialize(Data.GetData(), Data.Num());
		check(!FileReader->IsError());

		uint64 DevArHash = UE::Cook::FCookMetadataState::ComputeHashOfDevelopmentAssetRegistry(MakeMemoryView(Data));

		FLargeMemoryReader MemoryReader(Data.GetData(), Data.Num());
		if (OutAssetRegistry.Load(MemoryReader, Options, &Version))
		{
			return DevArHash;
		}
	}

	return 0;;
}

static bool SaveAssetRegistry(const FString& InAssetRegistryFileName, FAssetRegistryState& InAssetRegistry, uint64* OutDevArHash)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SavingAssetRegistry);
	FLargeMemoryWriter SerializedAssetRegistry;
	if (InAssetRegistry.Save(SerializedAssetRegistry, FAssetRegistrySerializationOptions(UE::AssetRegistry::ESerializationTarget::ForDevelopment)) == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to serialize asset registry to memory."));
		return false;
	}

	if (OutDevArHash)
	{
		*OutDevArHash = UE::Cook::FCookMetadataState::ComputeHashOfDevelopmentAssetRegistry(MakeMemoryView(SerializedAssetRegistry.GetData(), SerializedAssetRegistry.TotalSize()));
	}

	FString OutputFileName = InAssetRegistryFileName + TEXT(".temp");

	TUniquePtr<FArchive> Writer = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*OutputFileName));
	if (!Writer)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to open destination asset registry. (%s)"), *OutputFileName);
		return false;
	}

	Writer->Serialize(SerializedAssetRegistry.GetData(), SerializedAssetRegistry.TotalSize());

	// Always explicitly close to catch errors from flush/close
	Writer->Close();

	if (Writer->IsError() || Writer->IsCriticalError())
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to write asset registry to disk. (%s)"), *OutputFileName);
		return false;
	}

	// Move our temp file over the original asset registry.
	if (IFileManager::Get().Move(*InAssetRegistryFileName, *OutputFileName) == false)
	{
		// Error already logged by FileManager
		return false;
	}

	UE_LOG(LogIoStore, Display, TEXT("Saved asset registry to disk. (%s)"), *InAssetRegistryFileName);

	return true;
}

static int32 DoAssetRegistryWritebackAfterStage(const FString& InAssetRegistryFileName, FString&& InContainerDirectory, const FKeyChain& InKeyChain)
{
	// This version called after the containers are already created, when you
	// have a bunch of containers on disk and you want to add chunk info back to
	// an asset registry.

	FAssetRegistryState AssetRegistry;
	if (LoadAssetRegistry(InAssetRegistryFileName, AssetRegistry) == 0)
	{
		UE_LOG(LogIoStore, Error, TEXT("Unabled to open source asset registry: %s"), *InAssetRegistryFileName);
		return 1;
	}

	// Grab all containers in the directory
	FPaths::NormalizeDirectoryName(InContainerDirectory);
	TArray<FString> FoundContainerFiles;
	IFileManager::Get().FindFiles(FoundContainerFiles, *(InContainerDirectory / TEXT("*.utoc")), true, false);
	
	uint64 TotalCompressedSize = 0;
	
	// Grab all the package infos.
	TMap<FPackageId, TArray<FIoStoreChunkSource, TInlineAllocator<2>>> PackageToChunks;
	for (const FString& Filename : FoundContainerFiles)
	{
		TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*(InContainerDirectory / Filename), InKeyChain);
		if (Reader.IsValid() == false)
		{
			return 1; // already logged.
		}
		
		Reader->EnumerateChunks([&](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			FPackageId PackageId = FPackageId::FromValue(*(int64*)(ChunkInfo.Id.GetData()));
			// (Deployment can't be ascertained after staging - plugin jsons are not generated.)
			PackageToChunks.FindOrAdd(PackageId).Add({ChunkInfo, UE::Cook::EPluginSizeTypes::COUNT});
			TotalCompressedSize += ChunkInfo.CompressedSize;
			return true;
		});
	}

	AddChunkInfoToAssetRegistry(MoveTemp(PackageToChunks), AssetRegistry, nullptr, 0, 0, TotalCompressedSize);

	return SaveAssetRegistry(InAssetRegistryFileName, AssetRegistry, nullptr) ? 0 : 1;
}

enum class ECookMetadataFiles
{
	None = 0,
	AssetRegistry = 1,
	CookMetadata = 2,
	All = 4
};
ENUM_CLASS_FLAGS(ECookMetadataFiles);

static ECookMetadataFiles FindAndLoadMetadataFiles(
	const FString& InCookedDir, ECookMetadataFiles InRequiredFiles, 
	FAssetRegistryState& OutAssetRegistry, FString* OutAssetRegistryFileName /*optional, set on success*/,
	UE::Cook::FCookMetadataState* OutCookMetadata, FString* OutCookMetadataFileName /*optional, set on success or need*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadingAssetRegistry);

	// Look for the development registry. Should be in \\GameName\\Metadata\\DevelopmentAssetRegistry.bin, but we don't know what "GameName" is.
	TArray<FString> PossibleAssetRegistryFiles;
	IFileManager::Get().FindFilesRecursive(PossibleAssetRegistryFiles, *InCookedDir, GetDevelopmentAssetRegistryFilename(), true, false);

	if (PossibleAssetRegistryFiles.Num() > 1)
	{
		UE_LOG(LogIoStore, Warning, TEXT("Found multiple possible development asset registries:"));
		for (FString& Filename : PossibleAssetRegistryFiles)
		{
			UE_LOG(LogIoStore, Warning, TEXT("    %s"), *Filename);
		}
	}

	if (PossibleAssetRegistryFiles.Num() == 0)
	{
		if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::AssetRegistry))
		{
			UE_LOG(LogIoStore, Error, TEXT("No development asset registry file found!"));
		}
		else
		{
			UE_LOG(LogIoStore, Display, TEXT("No development asset registry file found!"));
		}
		return ECookMetadataFiles::None;
	}

	UE_LOG(LogIoStore, Display, TEXT("Using input asset registry: %s"), *PossibleAssetRegistryFiles[0]);
	uint64 LoadedDevArHash = LoadAssetRegistry(PossibleAssetRegistryFiles[0], OutAssetRegistry);

	if (LoadedDevArHash == 0)
	{
		return ECookMetadataFiles::None; // already logged
	}

	// If we found the asset registry, try and find the cook metadata that should be next to it.
	ECookMetadataFiles ResultFiles = ECookMetadataFiles::AssetRegistry;

	if (OutCookMetadata)
	{
		// The cook metadata file should be adjacent to the development asset registry.
		FString CookMetadataFileName = FPaths::GetPath(PossibleAssetRegistryFiles[0]) / UE::Cook::GetCookMetadataFilename();
		if (IFileManager::Get().FileExists(*CookMetadataFileName))
		{
			if (OutCookMetadata->ReadFromFile(CookMetadataFileName) == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to deserialize cook metadata file - invalid data. [%s]"), *CookMetadataFileName);
				if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::CookMetadata))
				{
					return ECookMetadataFiles::None;
				}
			}
			else if (OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHash() != LoadedDevArHash &&
				OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHashPostWriteback() != LoadedDevArHash) // during testing we can repeat stage after cook so we might have already edited it.
			{
				if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::CookMetadata))
				{
					UE_LOG(LogIoStore, Error,
						TEXT("Cook metadata file mismatch: Hash of associated development asset registry does not match. [%s] %llx vs %llx (%llx post writeback)"),
						*CookMetadataFileName, LoadedDevArHash, OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHash(), OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHashPostWriteback());
					return ECookMetadataFiles::None;
				}
				else
				{
					UE_LOG(LogIoStore, Display,
						TEXT("Cook metadata file mismatch: Hash of associated development asset registry does not match. [%s] %llx vs %llx (%llx post writeback)"),
						*CookMetadataFileName, LoadedDevArHash, OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHash(), OutCookMetadata->GetAssociatedDevelopmentAssetRegistryHashPostWriteback());
					OutCookMetadata->Reset();
				}
			}
			else
			{
				EnumAddFlags(ResultFiles, ECookMetadataFiles::CookMetadata);
				if (OutCookMetadataFileName)
				{
					*OutCookMetadataFileName = MoveTemp(CookMetadataFileName);
				}
			}
		}
		else
		{
			if (EnumHasAnyFlags(InRequiredFiles, ECookMetadataFiles::CookMetadata))
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to open and read cook metadata file %s"), *CookMetadataFileName);
				return ECookMetadataFiles::None;
			}

			UE_LOG(LogIoStore, Display, TEXT("No cook metadata file found, checked %s"), *CookMetadataFileName);
			if (OutCookMetadataFileName)
			{
				*OutCookMetadataFileName = FString("");
			}
		}
	}


	if (OutAssetRegistryFileName)
	{
		*OutAssetRegistryFileName = MoveTemp(PossibleAssetRegistryFiles[0]);
	}
	return ResultFiles;
}

struct FIoStoreWriterInfo
{
	UE::Cook::EPluginSizeTypes SizeType;
	FName PakChunkName;
};

static bool DoAssetRegistryWritebackDuringStage(
	EAssetRegistryWritebackMethod InMethod, 
	bool bInWritePluginMetadata,
	const FString& InCookedDir, 
	bool bInCompressionEnabled,
	TArray<TSharedPtr<IIoStoreWriter>>& InIoStoreWriters, 
	TArray<FIoStoreWriterInfo>& InIoStoreWriterInfos,
	FShaderAssociationInfo& InShaderAssociationInfo
)
{
	// This version called during container creation.
	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateAssetRegistryWithSizeInfo);
	UE_LOG(LogIoStore, Display, TEXT("Adding staging metadata to asset registry..."));

	// The overwhelming majority of time for the asset registry writeback is loading and saving.
	FString AssetRegistryFileName;
	FAssetRegistryState AssetRegistry;
	FString CookMetadataFileName;
	UE::Cook::FCookMetadataState CookMetadata;
	ECookMetadataFiles FilesNeeded = ECookMetadataFiles::AssetRegistry;

	// We always need the cook metadata in order to update the corresponding hash.
	EnumAddFlags(FilesNeeded, ECookMetadataFiles::CookMetadata);

	if (FindAndLoadMetadataFiles(InCookedDir, FilesNeeded, AssetRegistry, &AssetRegistryFileName, &CookMetadata, &CookMetadataFileName) == ECookMetadataFiles::None)
	{
		// already logged
		return false;
	}

	//
	// We want to separate out the sizes based on where they go in the end product.
	//
	uint64 UnassignableShaderCodeBytes = 0;
	uint64 AssignableShaderCodeBytes = 0;
	UE::Cook::FPluginSizeInfo ProductSize;
	TMap<FPackageId, TArray<FIoStoreChunkSource, TInlineAllocator<2>>> PackageToChunks;
	{
		int32 IoStoreWriterIndex = 0;
		for (TSharedPtr<IIoStoreWriter> IoStoreWriter : InIoStoreWriters)
		{
			IoStoreWriter->EnumerateChunks(
				[&PackageToChunks, 
				 IoStoreWriterInfo = InIoStoreWriterInfos[IoStoreWriterIndex],
				 &ProductSize, 
				 &InShaderAssociationInfo,
				 &UnassignableShaderCodeBytes,
				 &AssignableShaderCodeBytes
				 ](const FIoStoreTocChunkInfo& ChunkInfo)
			{			
				ProductSize[IoStoreWriterInfo.SizeType] += ChunkInfo.CompressedSize;

				// Shader code chunks don't have the package in their chunk id, so we have to use other data to look
				// it up and find it.
				FPackageId PackageId = FPackageId::FromValue(*(int64*)(ChunkInfo.Id.GetData()));
				if (ChunkInfo.ChunkType == EIoChunkType::ShaderCode)
				{
					// Update size info for the shader.
					FShaderAssociationInfo::FShaderChunkInfo* ShaderChunkInfo = InShaderAssociationInfo.ShaderChunkInfos.Find({IoStoreWriterInfo.PakChunkName, ChunkInfo.Id});
					ShaderChunkInfo->CompressedSize = ChunkInfo.CompressedSize;
					ShaderChunkInfo->SizeType = IoStoreWriterInfo.SizeType;

					// Shaders don't put their package in their chunk - they are just a hash. We have the list of them
					// already in the shader association so we don't bother adding here. However some can't get assigned to
					// anything and so we track that size for reporting.
					if (ShaderChunkInfo->Type == FShaderAssociationInfo::FShaderChunkInfo::Global ||
						ShaderChunkInfo->Type == FShaderAssociationInfo::FShaderChunkInfo::Orphan)
					{
						UnassignableShaderCodeBytes += ChunkInfo.CompressedSize;
					}
					else
					{
						AssignableShaderCodeBytes += ChunkInfo.CompressedSize;
					}
				}
				else
				{
					PackageToChunks.FindOrAdd(PackageId).Add({ChunkInfo, IoStoreWriterInfo.SizeType});
				}
				return true;
			});

			IoStoreWriterIndex++;
		}
	}

	if (bInWritePluginMetadata)
	{
		UpdatePluginMetadataAndWriteJsons(AssetRegistryFileName, PackageToChunks, AssetRegistry, CookMetadata, &InShaderAssociationInfo);
	}

	AddChunkInfoToAssetRegistry(MoveTemp(PackageToChunks), AssetRegistry, &InShaderAssociationInfo, UnassignableShaderCodeBytes, AssignableShaderCodeBytes, ProductSize.TotalSize());
	uint64 UpdatedDevArHash = 0;
	switch (InMethod)
	{
	case EAssetRegistryWritebackMethod::OriginalFile:
		{
			// Write to an adjacent file and move after
			if (SaveAssetRegistry(AssetRegistryFileName, AssetRegistry, &UpdatedDevArHash) == false)
			{
				return false;
			}

			break;
		}
	case EAssetRegistryWritebackMethod::AdjacentFile:
		{
			if (SaveAssetRegistry(AssetRegistryFileName.Replace(TEXT(".bin"), TEXT("Staged.bin")), AssetRegistry, &UpdatedDevArHash) == false)
			{
				return false;
			}
			break;
		}
	default:
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid asset registry writeback method (should already be handled!) (%d)"), int(InMethod));
			return false;
		}
	}

	// Since we modified the dev ar, we need to save the updated hash in the cook metadata so it can still validate.
	CookMetadata.SetSizesPresent(bInCompressionEnabled ? UE::Cook::ECookMetadataSizesPresent::Compressed : UE::Cook::ECookMetadataSizesPresent::Uncompressed);
	CookMetadata.SetAssociatedDevelopmentAssetRegistryHashPostWriteback(UpdatedDevArHash);

	FArrayWriter SerializedCookMetadata;
	CookMetadata.Serialize(SerializedCookMetadata);

	FString TempFileName = CookMetadataFileName + TEXT(".temp");
	if (FFileHelper::SaveArrayToFile(SerializedCookMetadata, *TempFileName))
	{
		// Move our temp file over the original asset registry.
		if (IFileManager::Get().Move(*CookMetadataFileName, *TempFileName) == false)
		{
			// Error already logged by FileManager
			return false;
		}
	}
	else
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to save temp file for write updated cook metadata file (%s"), *TempFileName);
		return false;
	}
	
	return true;
}

// Implements providing the chunk hashes that exist in the asset registry to the
// iostore writer to avoid reading and hashing redundently.
class FIoStoreHashDb : public IIoStoreWriterHashDatabase
{
public:
	virtual ~FIoStoreHashDb() {}

	TMap<FIoChunkId, FIoHash> Hashes;

	bool Initialize(const FString& InCookedDir)
	{
		FString AssetRegistryFileName;
		FAssetRegistryState AssetRegistry;
		if (FindAndLoadMetadataFiles(InCookedDir, ECookMetadataFiles::None, AssetRegistry, nullptr, nullptr, nullptr) == ECookMetadataFiles::None)
		{
			// already logged
			return false;
		}

		double StartTime = FPlatformTime::Seconds();

		const TMap<FName, const FAssetPackageData*>& Packages = AssetRegistry.GetAssetPackageDataMap();
		for (auto PackageIter : Packages)
		{
			for (const TPair<FIoChunkId, FIoHash>& HashIter : PackageIter.Value->ChunkHashes)
			{
				// For the moment, only bulk data types are added to teh asset registry - gate here so that
				// we remember to verify all the hashes match when they eventually get added during cook.
				if (HashIter.Key.GetChunkType() == EIoChunkType::BulkData ||
					HashIter.Key.GetChunkType() == EIoChunkType::OptionalBulkData)
				{
					Hashes.Add(HashIter.Key, HashIter.Value);
				}
			}
		}

		double EndTime = FPlatformTime::Seconds();
		UE_LOG(LogIoStore, Display, TEXT("Added %d hashes to the hash database, init took %f seconds"), Hashes.Num(), EndTime - StartTime);
		return true;
	}

	virtual bool FindHashForChunkId(const FIoChunkId& ChunkId, FIoChunkHash& OutHash) const override
	{
		const FIoHash* Exists = Hashes.Find(ChunkId);
		if (Exists)
		{
			OutHash = FIoChunkHash::CreateFromIoHash(*Exists);
			return true;
		}
		return false;
	}
};

// modified copy from PakFileUtilities
static FName RemapLocalizationPathIfNeeded(const FString& Path)
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
			return FName(NonLocalizedPath);
		}
	}
	return NAME_None;
}

void ProcessRedirects(const FIoStoreArguments& Arguments, const TMap<FPackageId, FCookedPackage*>& PackagesMap)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ProcessRedirects);

	for (const auto& KV : PackagesMap)
	{
		FCookedPackage* Package = KV.Value;
		FName LocalizedSourcePackageName = RemapLocalizationPathIfNeeded(Package->PackageName.ToString());
		if (!LocalizedSourcePackageName.IsNone())
		{
			Package->SourcePackageName = LocalizedSourcePackageName;
			Package->bIsLocalized = true;
		}
	}

	const bool bIsBuildingDLC = Arguments.IsDLC();
	if (bIsBuildingDLC && Arguments.bRemapPluginContentToGame)
	{
		for (const auto& KV : PackagesMap)
		{
			FCookedPackage* Package = KV.Value;
			const int32 DLCNameLen = Arguments.DLCName.Len() + 1;
			FString PackageNameStr = Package->PackageName.ToString();
			FString RedirectedPackageNameStr = TEXT("/Game");
			RedirectedPackageNameStr.AppendChars(*PackageNameStr + DLCNameLen, PackageNameStr.Len() - DLCNameLen);
			FName RedirectedPackageName = FName(*RedirectedPackageNameStr);
			Package->SourcePackageName = RedirectedPackageName;
		}
	}
}

void CreateContainerHeader(FContainerTargetSpec& ContainerTarget, bool bIsOptional)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CreateContainerHeader);
	FIoContainerHeader& Header = bIsOptional ? ContainerTarget.OptionalSegmentHeader : ContainerTarget.Header;
	Header.ContainerId = bIsOptional ? ContainerTarget.OptionalSegmentContainerId : ContainerTarget.ContainerId;

	int32 NonOptionalSegmentStoreEntriesCount = 0;
	int32 OptionalSegmentStoreEntriesCount = 0;
	if (bIsOptional)
	{
		for (const FCookedPackage* Package : ContainerTarget.Packages)
		{
			if (Package->PackageStoreEntry.HasOptionalSegment())
			{
				if (Package->PackageStoreEntry.IsAutoOptional())
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
		NonOptionalSegmentStoreEntriesCount = ContainerTarget.Packages.Num();
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

	TArray<const FCookedPackage*> SortedPackages(ContainerTarget.Packages);
	Algo::Sort(SortedPackages, [](const FCookedPackage* A, const FCookedPackage* B)
		{
			return A->GlobalPackageId < B->GlobalPackageId;
		});

	Header.PackageIds.Reserve(NonOptionalSegmentStoreEntriesCount);
	Header.OptionalSegmentPackageIds.Reserve(OptionalSegmentStoreEntriesCount);
	FPackageStoreNameMapBuilder RedirectsNameMapBuilder;
	RedirectsNameMapBuilder.SetNameMapType(FMappedName::EType::Container);
	TSet<FName> AllLocalizedPackages;
	if (bIsOptional)
	{
		for (const FCookedPackage* Package : SortedPackages)
		{
			const FPackageStoreEntryResource& Entry = Package->PackageStoreEntry;
			if (Entry.HasOptionalSegment())
			{
				FStoreEntriesWriter* TargetEntriesWriter;
				if (Entry.IsAutoOptional())
				{
					Header.PackageIds.Add(Package->GlobalPackageId);
					TargetEntriesWriter = &StoreEntriesWriter;
				}
				else
				{
					Header.OptionalSegmentPackageIds.Add(Package->GlobalPackageId);
					TargetEntriesWriter = &OptionalSegmentStoreEntriesWriter;
				}

				// OptionalImportedPackages
				const TArray<FPackageId>& OptionalSegmentImportedPackageIds = Entry.OptionalSegmentImportedPackageIds;
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
		for (const FCookedPackage* Package : SortedPackages)
		{
			const FPackageStoreEntryResource& Entry = Package->PackageStoreEntry;
			Header.PackageIds.Add(Package->GlobalPackageId);
			if (!Package->SourcePackageName.IsNone())
			{
				RedirectsNameMapBuilder.MarkNameAsReferenced(Package->SourcePackageName);
				FMappedName MappedSourcePackageName = RedirectsNameMapBuilder.MapName(Package->SourcePackageName);
				if (Package->bIsLocalized)
				{
					if (!AllLocalizedPackages.Contains(Package->SourcePackageName))
					{
						Header.LocalizedPackages.Add({ FPackageId::FromName(Package->SourcePackageName), MappedSourcePackageName });
						AllLocalizedPackages.Add(Package->SourcePackageName);
					}
				}
				else
				{
					Header.PackageRedirects.Add({ FPackageId::FromName(Package->SourcePackageName), Package->GlobalPackageId, MappedSourcePackageName });
				}
			}

			// ImportedPackages
			const TArray<FPackageId>& ImportedPackageIds = Entry.ImportedPackageIds;
			SerializePackageEntryCArrayHeader(StoreEntriesWriter, ImportedPackageIds.Num());
			for (FPackageId ImportedPackageId : ImportedPackageIds)
			{
				check(ImportedPackageId.IsValid());
				StoreEntriesWriter.StoreDataArchive << ImportedPackageId;
			}

			// ShaderMapHashes
			const TArray<FSHAHash>& ShaderMapHashes = Package->ShaderMapHashes;
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
}

int32 CreateTarget(const FIoStoreArguments& Arguments, const FIoStoreWriterSettings& GeneralIoWriterSettings)
{
	IOSTORE_CPU_SCOPE(CreateTarget);
	TGuardValue<int32> GuardAllowUnversionedContentInEditor(GAllowUnversionedContentInEditor, 1);

	TSharedPtr<IIoStoreWriterReferenceChunkDatabase> ChunkDatabase;
	if (Arguments.ReferenceChunkGlobalContainerFileName.Len())
	{
		ChunkDatabase = MakeShared<FIoStoreChunkDatabase>();
		if (((FIoStoreChunkDatabase&)*ChunkDatabase).Init(Arguments.ReferenceChunkGlobalContainerFileName, Arguments.ReferenceChunkKeys) == false)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to initialize reference chunk store. Pak will continue."));
		}
	}

	TSharedPtr<IIoStoreWriterHashDatabase> HashDatabase = MakeShared<FIoStoreHashDb>();
	if (((FIoStoreHashDb&)*HashDatabase).Initialize(Arguments.CookedDir) == false)
	{
		UE_LOG(LogIoStore, Display, TEXT("Unabled to initialize the hash database from the asset registry!"));
	}
	if (Arguments.bVerifyHashDatabase)
	{
		UE_LOG(LogIoStore, Display, TEXT("Hash database verification on: hashes will be checked for accuracy during this run."));
	}

	TArray<FCookedPackage*> Packages;
	FPackageNameMap PackageNameMap;
	FPackageIdMap PackageIdMap;

	FPackageStoreOptimizer PackageStoreOptimizer;
	PackageStoreOptimizer.Initialize(*Arguments.ScriptObjects);
	FIoStoreWriteRequestManager WriteRequestManager(PackageStoreOptimizer, Arguments.PackageStore.Get());

	TArray<FContainerTargetSpec*> ContainerTargets;
	UE_LOG(LogIoStore, Display, TEXT("Creating container targets..."));
	{
		IOSTORE_CPU_SCOPE(CreateContainerTargets);
		InitializeContainerTargetsAndPackages(Arguments, Packages, PackageNameMap, PackageIdMap, ContainerTargets);
	}

	TUniquePtr<FIoStoreWriterContext> IoStoreWriterContext;
	{
		IOSTORE_CPU_SCOPE(InitializeIoStoreWriters);
		IoStoreWriterContext.Reset(new FIoStoreWriterContext());
		FIoStatus IoStatus = IoStoreWriterContext->Initialize(GeneralIoWriterSettings);
		check(IoStatus.IsOk());
	}
	TArray<FString> OnDemandContainers;
	TArray<TSharedPtr<IIoStoreWriter>> IoStoreWriters;
	TArray<FIoStoreWriterInfo> IoStoreWriterInfos;
	TSharedPtr<IIoStoreWriter> GlobalIoStoreWriter;
	{
		IOSTORE_CPU_SCOPE(InitializeWriters);
		if (!Arguments.IsDLC())
		{
			IOSTORE_CPU_SCOPE(InitializeGlobalWriter);
			FIoContainerSettings GlobalContainerSettings;
			if (Arguments.bSign)
			{
				GlobalContainerSettings.SigningKey = Arguments.KeyChain.GetSigningKey();
				GlobalContainerSettings.ContainerFlags |= EIoContainerFlags::Signed;
			}
			GlobalIoStoreWriter = IoStoreWriterContext->CreateContainer(*Arguments.GlobalContainerPath, GlobalContainerSettings);
			IoStoreWriters.Add(GlobalIoStoreWriter);
			IoStoreWriterInfos.Add({UE::Cook::EPluginSizeTypes::Installed, "global"});
		}
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			IOSTORE_CPU_SCOPE(InitializeWriter);
			check(ContainerTarget->ContainerId.IsValid());

			if (ContainerTarget->OutputPath.IsEmpty())
			{
				continue;
			}

			if (!ContainerTarget->StageLooseFileRootPath.IsEmpty())
			{
				FLooseFilesWriterSettings WriterSettings;
				WriterSettings.TargetRootPath = ContainerTarget->StageLooseFileRootPath;
				ContainerTarget->IoStoreWriter = MakeLooseFilesIoStoreWriter(WriterSettings);
				IoStoreWriterInfos.Add({UE::Cook::EPluginSizeTypes::Streaming, ContainerTarget->Name}); // LooseFiles currently end up as a streamed source.
				IoStoreWriters.Add(ContainerTarget->IoStoreWriter);
			}
			else
			{
				FIoContainerSettings ContainerSettings;
				ContainerSettings.ContainerId = ContainerTarget->ContainerId;
				if (Arguments.bCreateDirectoryIndex)
				{
					ContainerSettings.ContainerFlags = ContainerTarget->ContainerFlags | EIoContainerFlags::Indexed;
				}
				if (EnumHasAnyFlags(ContainerTarget->ContainerFlags, EIoContainerFlags::Encrypted))
				{
					if (const FNamedAESKey* Key = Arguments.KeyChain.GetEncryptionKeys().Find(ContainerTarget->EncryptionKeyGuid))
					{
						ContainerSettings.EncryptionKeyGuid = ContainerTarget->EncryptionKeyGuid;
						ContainerSettings.EncryptionKey = Key->Key;
					}
					else
					{
						UE_LOG(LogIoStore, Error, TEXT("Failed to find encryption key '%s'"), *ContainerTarget->EncryptionKeyGuid.ToString());
						return -1;
					}
				}
				if (EnumHasAnyFlags(ContainerTarget->ContainerFlags, EIoContainerFlags::Signed))
				{
					ContainerSettings.SigningKey = Arguments.KeyChain.GetSigningKey();
					ContainerSettings.ContainerFlags |= EIoContainerFlags::Signed;
				}
				ContainerSettings.bGenerateDiffPatch = ContainerTarget->bGenerateDiffPatch;
				ContainerTarget->IoStoreWriter = IoStoreWriterContext->CreateContainer(*ContainerTarget->OutputPath, ContainerSettings);
				ContainerTarget->IoStoreWriter->EnableDiskLayoutOrdering(ContainerTarget->PatchSourceReaders);
				ContainerTarget->IoStoreWriter->SetReferenceChunkDatabase(ChunkDatabase);
				ContainerTarget->IoStoreWriter->SetHashDatabase(HashDatabase, Arguments.bVerifyHashDatabase);
				IoStoreWriters.Add(ContainerTarget->IoStoreWriter);

				if (EnumHasAnyFlags(ContainerTarget->ContainerFlags, EIoContainerFlags::OnDemand))
				{
					IoStoreWriterInfos.Add({UE::Cook::EPluginSizeTypes::Streaming, ContainerTarget->Name});
				}
				else
				{
					// There's no way to know whether a container is optional without parsing the filename, see
					// EIoStoreWriterType.
					FString BaseFileName = FPaths::GetBaseFilename(ContainerTarget->OutputPath, true);
					// Strip the platform identifier off the pak
					if (int32 DashIndex=0; BaseFileName.FindLastChar(TEXT('-'), DashIndex))
					{
						BaseFileName.LeftInline(DashIndex);
					}
					if (BaseFileName.EndsWith(TEXT("optional")))
					{
						IoStoreWriterInfos.Add({UE::Cook::EPluginSizeTypes::Optional, ContainerTarget->Name});
					}
					else
					{
						IoStoreWriterInfos.Add({UE::Cook::EPluginSizeTypes::Installed, ContainerTarget->Name});
					}
				}

				if (!ContainerTarget->OptionalSegmentOutputPath.IsEmpty())
				{
					ContainerSettings.ContainerId = ContainerTarget->OptionalSegmentContainerId;
					ContainerTarget->OptionalSegmentIoStoreWriter = IoStoreWriterContext->CreateContainer(*ContainerTarget->OptionalSegmentOutputPath, ContainerSettings);
					ContainerSettings.ContainerId = ContainerTarget->ContainerId;

					ContainerTarget->OptionalSegmentIoStoreWriter->SetReferenceChunkDatabase(ChunkDatabase);
					IoStoreWriters.Add(ContainerTarget->OptionalSegmentIoStoreWriter);
					IoStoreWriterInfos.Add({UE::Cook::EPluginSizeTypes::OptionalSegment, ContainerTarget->Name});
				}
				if (EnumHasAnyFlags(ContainerTarget->ContainerFlags, EIoContainerFlags::OnDemand))
				{
					OnDemandContainers.Add(ContainerTarget->OutputPath);
				}
			}
		}
	}

	const bool bIsLegacyStage = !Arguments.PackageStore->HasZenStoreClient();
	if (bIsLegacyStage)
	{
		ParsePackageAssetsFromFiles(Packages, PackageStoreOptimizer);
		if (Arguments.bFileRegions)
		{
			// The file regions for packages are relative to the start of the uexp file so we need to make them relative to the start of the export bundle chunk instead
			for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
			{
				for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
				{
					if (TargetFile.ChunkType == EContainerChunkType::PackageData)
					{
						uint64 HeaderSize = static_cast<FLegacyCookedPackage*>(TargetFile.Package)->OptimizedPackage->GetHeaderSize();
						for (FFileRegion& Region : TargetFile.FileRegions)
						{
							Region.Offset += HeaderSize;
						}
					}
					else if (TargetFile.ChunkType == EContainerChunkType::OptionalSegmentPackageData)
					{
						uint64 HeaderSize = static_cast<FLegacyCookedPackage*>(TargetFile.Package)->OptimizedOptionalSegmentPackage->GetHeaderSize();
						for (FFileRegion& Region : TargetFile.FileRegions)
						{
							Region.Offset += HeaderSize;
						}
					}
				}
			}
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Processing shader libraries, compressing with Oodle %s, level %d (%s)"), FOodleDataCompression::ECompressorToString(Arguments.ShaderOodleCompressor), (int32)Arguments.ShaderOodleLevel, FOodleDataCompression::ECompressionLevelToString(Arguments.ShaderOodleLevel));
	TArray<FShaderInfo*> Shaders;
	FShaderAssociationInfo ShaderAssocInfo;
	ProcessShaderLibraries(Arguments, ContainerTargets, Shaders, ShaderAssocInfo);

	auto AppendTargetFileChunk = [&WriteRequestManager](FContainerTargetSpec* ContainerTarget, const FContainerTargetFile& TargetFile)
	{
		FIoWriteOptions WriteOptions;
		WriteOptions.DebugName = *TargetFile.DestinationPath;
		WriteOptions.bForceUncompressed = TargetFile.bForceUncompressed;
		WriteOptions.bIsMemoryMapped = TargetFile.ChunkType == EContainerChunkType::MemoryMappedBulkData;
		WriteOptions.FileName = TargetFile.DestinationPath;
		FIoChunkId ChunkId = TargetFile.ChunkId;
		bool bIsOptionalSegmentChunk = false;
		switch (TargetFile.ChunkType)
		{
		case EContainerChunkType::OptionalSegmentPackageData:
		{
			if (TargetFile.Package->PackageStoreEntry.IsAutoOptional())
			{
				// Auto optional packages replace the non-optional part when the container is mounted
				ChunkId = CreateIoChunkId(TargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::ExportBundleData);
			}
			bIsOptionalSegmentChunk = true;
			break;
		}
		case EContainerChunkType::OptionalSegmentBulkData:
		{
			if (TargetFile.Package->PackageStoreEntry.IsAutoOptional())
			{
				// Auto optional packages replace the non-optional part when the container is mounted
				ChunkId = CreateIoChunkId(TargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::BulkData);
			}
			bIsOptionalSegmentChunk = true;
			break;
		}
		}

		if (bIsOptionalSegmentChunk)
		{
			ContainerTarget->OptionalSegmentIoStoreWriter->Append(ChunkId, WriteRequestManager.Read(TargetFile), WriteOptions);
		}
		else
		{
			ContainerTarget->IoStoreWriter->Append(TargetFile.ChunkId, WriteRequestManager.Read(TargetFile), WriteOptions);
		}
	};

	{
		IOSTORE_CPU_SCOPE(AppendChunks);
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			if (ContainerTarget->IoStoreWriter)
			{
				for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
				{
					AppendTargetFileChunk(ContainerTarget, TargetFile);
				}
			}
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Processing redirects..."));
	ProcessRedirects(Arguments, PackageIdMap);

	UE_LOG(LogIoStore, Display, TEXT("Creating disk layout..."));
	FString ClusterCSVPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("ClusterCSV="), ClusterCSVPath))
	{
		IOSTORE_CPU_SCOPE(CreateClusterCSV);
		ClusterStatsCsv.CreateOutputFile(ClusterCSVPath);
	}
	SortPackagesInLoadOrder(Packages, PackageIdMap);
	CreateDiskLayout(ContainerTargets, Packages, Arguments.OrderMaps, PackageIdMap, Arguments.bClusterByOrderFilePriority);

	{
		IOSTORE_CPU_SCOPE(AppendContainerHeaderChunks);
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			if (ContainerTarget->IoStoreWriter)
			{
				auto WriteContainerHeaderChunk = [](FIoContainerHeader& Header, IIoStoreWriter* IoStoreWriter, const FIoContainerId& IoStoreWriterId)
				{
					FLargeMemoryWriter HeaderAr(0, true);
					HeaderAr << Header;
					int64 DataSize = HeaderAr.TotalSize();
					FIoBuffer ContainerHeaderBuffer(FIoBuffer::AssumeOwnership, HeaderAr.ReleaseOwnership(), DataSize);

					// The header must have the same ID so that the loading code can find it.
					check(IoStoreWriterId == Header.ContainerId);

					FIoWriteOptions WriteOptions;
					WriteOptions.DebugName = TEXT("ContainerHeader");
					WriteOptions.bForceUncompressed = true;
					IoStoreWriter->Append(
						CreateIoChunkId(Header.ContainerId.Value(), 0, EIoChunkType::ContainerHeader),
						ContainerHeaderBuffer,
						WriteOptions);
				};

				CreateContainerHeader(*ContainerTarget, false);
				WriteContainerHeaderChunk(ContainerTarget->Header, ContainerTarget->IoStoreWriter.Get(), ContainerTarget->ContainerId);

				if (ContainerTarget->OptionalSegmentIoStoreWriter)
				{
					CreateContainerHeader(*ContainerTarget, true);
					WriteContainerHeaderChunk(ContainerTarget->OptionalSegmentHeader, ContainerTarget->OptionalSegmentIoStoreWriter.Get(), ContainerTarget->OptionalSegmentContainerId);
				}
			}

		}
	}

	uint64 InitialLoadSize = 0;
	if (GlobalIoStoreWriter)
	{
		IOSTORE_CPU_SCOPE(WriteScriptObjects);
		FIoBuffer ScriptObjectsBuffer = PackageStoreOptimizer.CreateScriptObjectsBuffer();
		InitialLoadSize = ScriptObjectsBuffer.DataSize();
		FIoWriteOptions WriteOptions;
		WriteOptions.DebugName = TEXT("ScriptObjects");
		GlobalIoStoreWriter->Append(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects), ScriptObjectsBuffer, WriteOptions);
	}

	UE_LOG(LogIoStore, Display, TEXT("Serializing container(s)..."));
	{
		IOSTORE_CPU_SCOPE(Serializing);

		TFuture<void> FlushTask = Async(EAsyncExecution::Thread, [&IoStoreWriterContext]()
		{
			IoStoreWriterContext->Flush();
		});

		while (!FlushTask.IsReady())
		{
			FlushTask.WaitFor(FTimespan::FromSeconds(2.0));
			FIoStoreWriterContext::FProgress Progress = IoStoreWriterContext->GetProgress();
			TStringBuilder<1024> ProgressStringBuilder;
			if (Progress.SerializedChunksCount >= Progress.TotalChunksCount)
			{
				ProgressStringBuilder.Appendf(TEXT("Writing tocs..."));
			}
			else if (Progress.SerializedChunksCount)
			{
				ProgressStringBuilder.Appendf(TEXT("Writing chunks (%llu/%llu)..."), Progress.SerializedChunksCount, Progress.TotalChunksCount);
				if (Progress.CompressedChunksCount)
				{
					ProgressStringBuilder.Appendf(TEXT(" [%llu compressed]"), Progress.CompressedChunksCount);
				}
				if (Progress.ScheduledCompressionTasksCount)
				{
					ProgressStringBuilder.Appendf(TEXT(" [%llu compression tasks scheduled]"), Progress.ScheduledCompressionTasksCount);
				}
				UE_LOG(LogIoStore, Display, TEXT("%s"), *ProgressStringBuilder);
			}
			else
			{
			UE_LOG(LogIoStore, Display, TEXT("Hashing chunks (%llu/%llu)..."), Progress.HashedChunksCount, Progress.TotalChunksCount);
			}
		}
	}

	{
		FIoStoreWriterContext::FProgress Progress = IoStoreWriterContext->GetProgress();
		if (Progress.HashDbChunksCount)
		{
			UE_LOG(LogIoStore, Display, TEXT("%s / %s hashes were loaded from the hash database, by type:"), *NumberString(Progress.HashDbChunksCount), *NumberString(Progress.TotalChunksCount));
		
			for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
			{
				if (Progress.HashDbChunksByType[i])
				{
					UE_LOG(LogIoStore, Display, TEXT("    %-26s %s"), *LexToString((EIoChunkType)i), *NumberString(Progress.HashDbChunksByType[i]));
				}
			}
		}
		if (Progress.RefDbChunksCount)
		{
			FIoStoreChunkDatabase& TypedChunkDatabase = ((FIoStoreChunkDatabase&)*ChunkDatabase);
			UE_LOG(LogIoStore, Display, TEXT("%s / %s chunks for %s bytes were loaded from the reference chunk database, by type:"), *NumberString(Progress.RefDbChunksCount), *NumberString(Progress.TotalChunksCount), *NumberString(TypedChunkDatabase.FulfillBytes));

			for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
			{
				if (Progress.RefDbChunksByType[i])
				{
					UE_LOG(LogIoStore, Display, TEXT("    %-26s: %s for %s bytes"), *LexToString((EIoChunkType)i), *NumberString(Progress.RefDbChunksByType[i]), *NumberString(TypedChunkDatabase.FulfillBytesPerChunk[i]));
				}
			}
		}
		if (Progress.CompressedChunksCount)
		{
			UE_LOG(LogIoStore, Display, TEXT("%s / %s chunks attempted to compress, by type:"), *NumberString(Progress.CompressedChunksCount), *NumberString(Progress.TotalChunksCount));

			for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
			{
				if (Progress.CompressedChunksByType[i] || Progress.BeginCompressChunksByType[i])
				{
					UE_LOG(LogIoStore, Display, TEXT("    %-26s %s / %s"), *LexToString((EIoChunkType)i), *NumberString(Progress.CompressedChunksByType[i]), *NumberString(Progress.BeginCompressChunksByType[i]));
				}
			}
		}
		UE_LOG(LogIoStore, Display, TEXT("Source bytes read:"));
		uint64 ZenTotalBytes = 0;
		for (uint64 b : WriteRequestManager.ZenSourceBytes)
		{
			ZenTotalBytes += b;
		}

		UE_LOG(LogIoStore, Display, TEXT("    Zen: %34s"), *NumberString(ZenTotalBytes));
		for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
		{
			if (WriteRequestManager.ZenSourceReads[i])
			{
				UE_LOG(LogIoStore, Display, TEXT("        %-22s %12s bytes over %s reads"), *LexToString((EIoChunkType)i), *NumberString(WriteRequestManager.ZenSourceBytes[i].Load()), *NumberString(WriteRequestManager.ZenSourceReads[i].Load()));
			}
		}

		uint64 LooseTotalBytes = 0;
		for (uint64 b : WriteRequestManager.LooseFileSourceBytes)
		{
			LooseTotalBytes += b;
		}
		UE_LOG(LogIoStore, Display, TEXT("    Loose File: %27s"), *NumberString(LooseTotalBytes));
		for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
		{
			if (WriteRequestManager.LooseFileSourceReads[i])
			{
				UE_LOG(LogIoStore, Display, TEXT("        %-22s %12s bytes over %s reads"), *LexToString((EIoChunkType)i), *NumberString(WriteRequestManager.LooseFileSourceBytes[i].Load()), *NumberString(WriteRequestManager.LooseFileSourceReads[i].Load()));
			}
		}

		uint64 MemoryTotalBytes = 0;
		for (uint64 b : WriteRequestManager.MemorySourceBytes)
		{
			MemoryTotalBytes += b;
		}
		UE_LOG(LogIoStore, Display, TEXT("    Memory: %31s"), *NumberString(MemoryTotalBytes));
		for (uint8 i = 0; i < (uint8)EIoChunkType::MAX; i++)
		{
			if (WriteRequestManager.MemorySourceReads[i])
			{
				UE_LOG(LogIoStore, Display, TEXT("        %-22s %12s bytes over %s reads"), *LexToString((EIoChunkType)i), *NumberString(WriteRequestManager.MemorySourceBytes[i].Load()), *NumberString(WriteRequestManager.MemorySourceReads[i].Load()));
			}
		}
	}

	if (GeneralIoWriterSettings.bCompressionEnableDDC)
	{
		FIoStoreWriterContext::FProgress Progress = IoStoreWriterContext->GetProgress();
		uint64 TotalDDCAttempts = Progress.CompressionDDCHitCount + Progress.CompressionDDCMissCount;
		double DDCHitRate = double(Progress.CompressionDDCHitCount) / TotalDDCAttempts * 100.0;
		UE_LOG(LogIoStore, Display, TEXT("Compression DDC hits: %llu/%llu (%.2f%%)"), Progress.CompressionDDCHitCount, TotalDDCAttempts, DDCHitRate);
	}

	if (Arguments.WriteBackMetadataToAssetRegistry != EAssetRegistryWritebackMethod::Disabled)
	{
		DoAssetRegistryWritebackDuringStage(
			Arguments.WriteBackMetadataToAssetRegistry, 
			Arguments.bWritePluginSizeSummaryJsons, 
			Arguments.CookedDir, 
			GeneralIoWriterSettings.CompressionMethod != NAME_None, 
			IoStoreWriters, 
			IoStoreWriterInfos,
			ShaderAssocInfo);
	}

	TArray<FIoStoreWriterResult> IoStoreWriterResults;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GetWriterResults);
		IoStoreWriterResults.Reserve(IoStoreWriters.Num());
		for (TSharedPtr<IIoStoreWriter> IoStoreWriter : IoStoreWriters)
		{
			IoStoreWriterResults.Emplace(IoStoreWriter->GetResult().ConsumeValueOrDie());
		}
	}

	FGraphEventRef WriteCsvFileTask;
	if (Arguments.CsvPath.Len() > 0)
	{
		WriteCsvFileTask = FFunctionGraphTask::CreateAndDispatchWhenReady([&Arguments, &IoStoreWriters, &IoStoreWriterResults]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(WriteCsvFiles);

			bool bPerContainerCsvFiles = FPaths::DirectoryExists(Arguments.CsvPath);
			FChunkEntryCsv AllContainersOutCsvFile;
			FChunkEntryCsv* Out = &AllContainersOutCsvFile;
			if (!bPerContainerCsvFiles)
			{
				// When CsvPath is a filename append .utoc.csv to create a unique single csv for all container files,
				// different from the unique single .pak.csv for all pak files.
				FString CsvFilename = Arguments.CsvPath + TEXT(".utoc.csv");
				AllContainersOutCsvFile.CreateOutputFile(*CsvFilename);
			}

			for (int32 Index = 0; Index < IoStoreWriters.Num(); ++Index)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(ListContainer);

				TSharedPtr<IIoStoreWriter> Writer = IoStoreWriters[Index];
				FIoStoreWriterResult& Result = IoStoreWriterResults[Index];

				TArray<FIoStoreTocChunkInfo> Chunks;
				{
					IOSTORE_CPU_SCOPE(EnumerateChunks);
					Chunks.Reserve(Result.TocEntryCount);
					Writer->EnumerateChunks([&Chunks](FIoStoreTocChunkInfo&& ChunkInfo)
					{
						Chunks.Add(MoveTemp(ChunkInfo));
						return true;
					});
				}

				{
					IOSTORE_CPU_SCOPE(SortChunks);
					auto SortKey = [](const FIoStoreTocChunkInfo& ChunkInfo) { return ChunkInfo.OffsetOnDisk; };
					Algo::SortBy(Chunks, SortKey);
				}

				{
					IOSTORE_CPU_SCOPE(WriteCsvFile);
					FChunkEntryCsv PerContainerOutCsvFile;
					if (bPerContainerCsvFiles)
					{
						// When CsvPath is a dir, then create one unique .utoc.csv per container file
						FString PerContainerCsvPath = Arguments.CsvPath / Result.ContainerName + TEXT(".utoc.csv");
						PerContainerOutCsvFile.CreateOutputFile(*PerContainerCsvPath);
						Out = &PerContainerOutCsvFile;
					}
					for (int32 EntryIndex=0; EntryIndex < Chunks.Num(); ++EntryIndex)
					{
						FIoStoreTocChunkInfo& ChunkInfo = Chunks[EntryIndex];
						FString PackageName;
						FPackageId PackageId;
						if (!ChunkInfo.bHasValidFileName)
						{
							FString FileName = Arguments.PackageStore->GetRelativeFilenameFromChunkId(ChunkInfo.Id);
							if (FileName.Len() > 0)
							{
								ChunkInfo.FileName = MoveTemp(FileName);
							}
							FName PackageFName = Arguments.PackageStore->GetPackageNameFromChunkId(ChunkInfo.Id);
							if (!PackageFName.IsNone())
							{
								PackageName = PackageFName.ToString();
								PackageId = FPackageId::FromName(FName(*PackageName));
							}
						}

						Out->AddChunk(Result.ContainerName, EntryIndex, ChunkInfo, PackageId, PackageName);
					}
				}
			}
		}, TStatId(), nullptr, ENamedThreads::AnyNormalThreadHiPriTask);
	}

	IOSTORE_CPU_SCOPE(OutputStats);

	UE_LOG(LogIoStore, Display, TEXT("Calculating stats..."));
	uint64 UExpSize = 0;
	uint64 UAssetSize = 0;
	uint64 UBulkSize = 0;
	uint64 HeaderSize = 0;
	uint64 ImportedPackagesCount = 0;
	uint64 NoImportedPackagesCount = 0;
	uint64 NameMapCount = 0;
	
	for (const FCookedPackage* Package : Packages)
	{
		UExpSize += Package->UExpSize;
		UAssetSize += Package->UAssetSize;
		UBulkSize += Package->TotalBulkDataSize;
		if (bIsLegacyStage)
		{
			const FLegacyCookedPackage* LegacyPackage = static_cast<const FLegacyCookedPackage*>(Package);
			NameMapCount += LegacyPackage->OptimizedPackage->GetNameCount();
			HeaderSize += LegacyPackage->OptimizedPackage->GetHeaderSize();
		}
		int32 PackageImportedPackagesCount = Package->PackageStoreEntry.ImportedPackageIds.Num();
		ImportedPackagesCount += PackageImportedPackagesCount;
		NoImportedPackagesCount += PackageImportedPackagesCount == 0;
	}
	
	uint64 GlobalShaderCount = 0;
	uint64 SharedShaderCount = 0;
	uint64 UniqueShaderCount = 0;
	uint64 InlineShaderCount = 0;
	uint64 GlobalShaderSize = 0;
	uint64 SharedShaderSize = 0;
	uint64 UniqueShaderSize = 0;
	uint64 InlineShaderSize = 0;
	for (const FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		for (const FShaderInfo* ShaderInfo : ContainerTarget->GlobalShaders)
		{
			++GlobalShaderCount;
			GlobalShaderSize += ShaderInfo->CodeIoBuffer.DataSize();
		}
		for (const FShaderInfo* ShaderInfo : ContainerTarget->SharedShaders)
		{
			++SharedShaderCount;
			SharedShaderSize += ShaderInfo->CodeIoBuffer.DataSize();
		}
		for (const FShaderInfo* ShaderInfo : ContainerTarget->UniqueShaders)
		{
			++UniqueShaderCount;
			UniqueShaderSize += ShaderInfo->CodeIoBuffer.DataSize();
		}
		for (const FShaderInfo* ShaderInfo : ContainerTarget->InlineShaders)
		{
			++InlineShaderCount;
			InlineShaderSize += ShaderInfo->CodeIoBuffer.DataSize();
		}
	}

	LogWriterResults(IoStoreWriterResults);
	LogContainerPackageInfo(ContainerTargets);
	
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8d Packages"), Packages.Num());
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2lf GB UExp"), (double)UExpSize / 1024.0 / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2lf GB UAsset"), (double)UAssetSize / 1024.0 / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2lf GB UBulk"), (double)UBulkSize / 1024.0 / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Global shaders"), (double)GlobalShaderSize / 1024.0 / 1024.0, GlobalShaderCount);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Shared shaders"), (double)SharedShaderSize / 1024.0 / 1024.0, SharedShaderCount);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Unique shaders"), (double)UniqueShaderSize / 1024.0 / 1024.0, UniqueShaderCount);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Inline shaders"), (double)InlineShaderSize / 1024.0 / 1024.0, InlineShaderCount);
	UE_LOG(LogIoStore, Display, TEXT(""));
	if (bIsLegacyStage)
	{
		UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Name map entries"), NameMapCount);
	}
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Imported package entries"), ImportedPackagesCount);
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Packages without imports"), NoImportedPackagesCount);
	UE_LOG(LogIoStore, Display, TEXT("Output: %8d Public runtime script objects"), PackageStoreOptimizer.GetTotalScriptObjectCount());
	if (bIsLegacyStage)
	{
		UE_LOG(LogIoStore, Display, TEXT("Output: %8.2lf GB HeaderData"), (double)HeaderSize / 1024.0 / 1024.0 / 1024.0);
	}
	UE_LOG(LogIoStore, Display, TEXT("Output: %8.2lf MB InitialLoadData"), (double)InitialLoadSize / 1024.0 / 1024.0);


	if (ChunkDatabase.IsValid())
	{
		//
		// If we are using a reference cache, the assumption is that we are expecting a high hit rate
		// on the chunks - this is supposed to be a current release, so theoretically we should only
		// miss on changed or new data, so it's useful to know where the misses are in case something
		// major happened that maybe shouldn't have.
		//
		uint64 TotalEntryBytes = 0;
		uint64 TotalMissBytes = 0;
		TMap<FIoContainerId, FString> ContainerNameMap;

		for (const FIoStoreWriterResult& Result : IoStoreWriterResults)
		{
			TotalEntryBytes += Result.TotalEntryCompressedSize;
			TotalMissBytes += Result.ReferenceCacheMissBytes;
			ContainerNameMap.Add(Result.ContainerId, Result.ContainerName);
		}

		FIoStoreChunkDatabase& ChunkDatabaseRef = (FIoStoreChunkDatabase&)*ChunkDatabase;

		uint64 TotalCandidateBytes = ChunkDatabaseRef.FulfillBytes + TotalMissBytes;

		UE_LOG(LogIoStore, Display, TEXT("Reference Chunk Database:"));
		UE_LOG(LogIoStore, Display, TEXT("    %s reused bytes out of %s candidate bytes - %.1f%% hit rate."),
			*NumberString(ChunkDatabaseRef.FulfillBytes),
			*NumberString(TotalCandidateBytes),
			100.0 * ChunkDatabaseRef.FulfillBytes / (TotalCandidateBytes));
		UE_LOG(LogIoStore, Display, TEXT("    %s candidate bytes out of %s io chunk bytes - %.1f%% coverage."),
			*NumberString(TotalCandidateBytes),
			*NumberString(TotalEntryBytes),
			100.0 * TotalCandidateBytes / (TotalEntryBytes));

		UE_LOG(LogIoStore, Display, TEXT("    %s chunks found out of %s requests"), 
			*NumberString(ChunkDatabaseRef.FulfillCount),
			*NumberString(ChunkDatabaseRef.RequestCount.load()));
		for (const TPair<FIoContainerId, TUniquePtr<FIoStoreChunkDatabase::FReaderChunks>>& ContainerInDatabase : ChunkDatabaseRef.ChunkDatabase)
		{
			for (uint8 i = 0; i < FIoStoreChunkDatabase::IoChunkTypeCount; i++)
			{
				if (ContainerInDatabase.Value->ChangedChunkCount[i] ||
					ContainerInDatabase.Value->NewChunkCount[i] ||
					ContainerInDatabase.Value->UsedChunkCount[i])
				{
					UE_LOG(LogIoStore, Display, TEXT("        %s[%s]:    %s changed, %s new, %s reused"), 
						*ContainerNameMap[ContainerInDatabase.Key],
						*LexToString((EIoChunkType)i),
						*NumberString(ContainerInDatabase.Value->ChangedChunkCount[i]),
						*NumberString(ContainerInDatabase.Value->NewChunkCount[i]),
						*NumberString(ContainerInDatabase.Value->UsedChunkCount[i]));
				}
			}
		}

		if (ChunkDatabaseRef.ContainerNotFound)
		{
			UE_LOG(LogIoStore, Display, TEXT("    %s containers were requested that weren't available. This means the "),
				*NumberString(ChunkDatabaseRef.MissingContainerIds.Num()));
			UE_LOG(LogIoStore, Display, TEXT("    previous release didn't have these containers. If that doesn't sound right verify"));
			UE_LOG(LogIoStore, Display, TEXT("    that you used reference containers from the same project. Missing containers:"));

			for (const TPair<FIoContainerId, TUniquePtr<FIoStoreChunkDatabase::FMissingContainerInfo>>& MissingId : ChunkDatabaseRef.MissingContainerIds)
			{
				 // we know this is a valid entry because the writers are what gave us the id in the first place.
				for (uint8 i = 0; i < FIoStoreChunkDatabase::IoChunkTypeCount; i++)
				{
					if (MissingId.Value->RequestedChunkCount[i])
					{
						UE_LOG(LogIoStore, Display, TEXT("        %s[%s]: %s requests"), 
							*ContainerNameMap[MissingId.Key], 
							*LexToString((EIoChunkType)i),
							*NumberString(MissingId.Value->RequestedChunkCount[i].load(std::memory_order_relaxed)));
					}
				}
			}
		}
	}

	UE_LOG(LogIoStore, Display, TEXT(""));
	if (Arguments.CsvPath.Len() > 0)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WaitForCsvFiles);
		UE_LOG(LogIoStore, Display, TEXT("Writing csv file(s) to: %s (*.utoc.csv)"), *Arguments.CsvPath);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(WriteCsvFileTask);
	}

	if (Arguments.bUpload && OnDemandContainers.IsEmpty() == false)
	{
		TIoStatusOr<UE::IO::IAS::FIoStoreUploadParams> UploadParams = UE::IO::IAS::FIoStoreUploadParams::Parse(FCommandLine::Get());
		if (UploadParams.IsOk() == false)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Skipping upload of container file(s), reason '%s'"), *UploadParams.Status().ToString());
			return 0;
		}

		if (UploadIoStoreContainerFiles(UploadParams.ConsumeValueOrDie(), OnDemandContainers, Arguments.KeyChain) == false)
		{
			return -1;
		}
	}

	return 0;
}

bool DumpIoStoreContainerInfo(const TCHAR* InContainerFilename, const FKeyChain& InKeyChain)
{
	TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(InContainerFilename, InKeyChain);
	if (!Reader.IsValid())
	{
		return false;
	}

	UE_LOG(LogIoStore, Display, TEXT("IoStore Container File: %s"), InContainerFilename);
	UE_LOG(LogIoStore, Display, TEXT("    Id: 0x%llX"), Reader->GetContainerId().Value());
	UE_LOG(LogIoStore, Display, TEXT("    Version: %d"), Reader->GetVersion());
	UE_LOG(LogIoStore, Display, TEXT("    Indexed: %d"), EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Indexed));
	UE_LOG(LogIoStore, Display, TEXT("    Signed: %d"), EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Signed));
	bool bIsEncrypted = EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Encrypted);
	UE_LOG(LogIoStore, Display, TEXT("    Encrypted: %d"), bIsEncrypted);
	if (bIsEncrypted)
	{
		UE_LOG(LogIoStore, Display, TEXT("    EncryptionKeyGuid: %s"), *Reader->GetEncryptionKeyGuid().ToString());
	}
	bool bIsCompressed = EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Compressed);
	UE_LOG(LogIoStore, Display, TEXT("    Compressed: %d"), bIsCompressed);
	if (bIsCompressed)
	{
		UE_LOG(LogIoStore, Display, TEXT("    CompressionBlockSize: %llu"), Reader->GetCompressionBlockSize());
		UE_LOG(LogIoStore, Display, TEXT("    CompressionMethods:"));
		for (FName Method : Reader->GetCompressionMethods())
		{
			UE_LOG(LogPakFile, Display, TEXT("        %s"), *Method.ToString());
		}
	}

	return true;
}

int32 CreateContentPatch(const FIoStoreArguments& Arguments, const FIoStoreWriterSettings& GeneralIoWriterSettings)
{
	UE_LOG(LogIoStore, Display, TEXT("Building patch..."));
	TUniquePtr<FIoStoreWriterContext> IoStoreWriterContext(new FIoStoreWriterContext());
	FIoStatus IoStatus = IoStoreWriterContext->Initialize(GeneralIoWriterSettings);
	check(IoStatus.IsOk());
	TArray<TSharedPtr<IIoStoreWriter>> IoStoreWriters;
	for (const FContainerSourceSpec& Container : Arguments.Containers)
	{
		TArray<TUniquePtr<FIoStoreReader>> SourceReaders = CreatePatchSourceReaders(Container.PatchSourceContainerFiles, Arguments);
		TUniquePtr<FIoStoreReader> TargetReader = CreateIoStoreReader(*Container.PatchTargetFile, Arguments.KeyChain);
		if (!TargetReader.IsValid())
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed loading target container"));
			return -1;
		}

		EIoContainerFlags TargetContainerFlags = TargetReader->GetContainerFlags();

		FIoContainerSettings ContainerSettings;
		if (Arguments.bCreateDirectoryIndex)
		{
			ContainerSettings.ContainerFlags |= EIoContainerFlags::Indexed;
		}

		ContainerSettings.ContainerId = TargetReader->GetContainerId();
		if (Arguments.bSign || EnumHasAnyFlags(TargetContainerFlags, EIoContainerFlags::Signed))
		{
			ContainerSettings.SigningKey =Arguments.KeyChain.GetSigningKey();
			ContainerSettings.ContainerFlags |= EIoContainerFlags::Signed;
		}

		if (EnumHasAnyFlags(TargetContainerFlags, EIoContainerFlags::Encrypted))
		{
			ContainerSettings.ContainerFlags |= EIoContainerFlags::Encrypted;
			const FNamedAESKey* Key = Arguments.KeyChain.GetEncryptionKeys().Find(TargetReader->GetEncryptionKeyGuid());
			if (!Key)
			{
				UE_LOG(LogIoStore, Error, TEXT("Missing encryption key for target container"));
				return -1;
			}
			ContainerSettings.EncryptionKeyGuid = Key->Guid;
			ContainerSettings.EncryptionKey = Key->Key;
		}

		TSharedPtr<IIoStoreWriter> IoStoreWriter = IoStoreWriterContext->CreateContainer(*Container.OutputPath, ContainerSettings);
		IoStoreWriters.Add(IoStoreWriter);
		TMap<FIoChunkId, FIoChunkHash> SourceHashByChunkId;
		for (const TUniquePtr<FIoStoreReader>& SourceReader : SourceReaders)
		{
			SourceReader->EnumerateChunks([&SourceHashByChunkId](const FIoStoreTocChunkInfo& ChunkInfo)
			{
				SourceHashByChunkId.Add(ChunkInfo.Id, ChunkInfo.Hash);
				return true;
			});
		}

		TMap<FIoChunkId, FString> ChunkFileNamesMap;
		TargetReader->GetDirectoryIndexReader().IterateDirectoryIndex(FIoDirectoryIndexHandle::RootDirectory(), TEXT(""),
		[&ChunkFileNamesMap, &TargetReader](FStringView Filename, uint32 TocEntryIndex) -> bool
		{
			TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = TargetReader->GetChunkInfo(TocEntryIndex);
			if (ChunkInfo.IsOk())
			{
				ChunkFileNamesMap.Add(ChunkInfo.ValueOrDie().Id, FString(Filename));
			}
			return true;
		});

		TargetReader->EnumerateChunks([&TargetReader, &SourceHashByChunkId, &IoStoreWriter, &ChunkFileNamesMap](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			FIoChunkHash* FindSourceHash = SourceHashByChunkId.Find(ChunkInfo.Id);
			if (!FindSourceHash || *FindSourceHash != ChunkInfo.Hash)
			{
				FIoReadOptions ReadOptions;
				TIoStatusOr<FIoBuffer> ChunkBuffer = TargetReader->Read(ChunkInfo.Id, ReadOptions);
				FIoWriteOptions WriteOptions;
				FString* FindFileName = ChunkFileNamesMap.Find(ChunkInfo.Id);
				if (FindFileName)
				{
					WriteOptions.FileName = *FindFileName;
					if (FindSourceHash)
					{
						UE_LOG(LogIoStore, Display, TEXT("Modified: %s"), **FindFileName);
					}
					else
					{
						UE_LOG(LogIoStore, Display, TEXT("Added: %s"), **FindFileName);
					}
				}
				WriteOptions.bIsMemoryMapped = ChunkInfo.bIsMemoryMapped;
				WriteOptions.bForceUncompressed = ChunkInfo.bForceUncompressed; 
				IoStoreWriter->Append(ChunkInfo.Id, ChunkBuffer.ConsumeValueOrDie(), WriteOptions);
			}
			return true;
		});
	}

	IoStoreWriterContext->Flush();
	TArray<FIoStoreWriterResult> Results;
	for (TSharedPtr<IIoStoreWriter> IoStoreWriter : IoStoreWriters)
	{
		Results.Emplace(IoStoreWriter->GetResult().ConsumeValueOrDie());
	}

	LogWriterResults(Results);

	return 0;
}

int32 ListContainer(
	const FKeyChain& KeyChain,
	const FString& ContainerPathOrWildcard,
	const FString& CsvPath)
{
	IOSTORE_CPU_SCOPE(ListContainer);
	TArray<FString> ContainerFilePaths;

	if (IFileManager::Get().FileExists(*ContainerPathOrWildcard))
	{
		ContainerFilePaths.Add(ContainerPathOrWildcard);
	}
	else if (IFileManager::Get().DirectoryExists(*ContainerPathOrWildcard))
	{
		FString Directory = ContainerPathOrWildcard;
		FPaths::NormalizeDirectoryName(Directory);

		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFiles(FoundContainerFiles, *(Directory / TEXT("*.utoc")), true, false);

		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Directory / Filename);
		}
	}
	else
	{
		FString Directory = FPaths::GetPath(ContainerPathOrWildcard);
		FPaths::NormalizeDirectoryName(Directory);

		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFiles(FoundContainerFiles, *ContainerPathOrWildcard, true, false);

		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Directory / Filename);
		}
	}

	if (ContainerFilePaths.Num() == 0)
	{
		UE_LOG(LogIoStore, Error, TEXT("Container '%s' doesn't exist and no container matches wildcard."), *ContainerPathOrWildcard);
		return -1;
	}
	
	// if CsvPath is a dir, not a file name, then write one csv per container to the dir
	// otherwise, write all contents to one big csv
	bool bPerContainerCsvFiles = IFileManager::Get().DirectoryExists(*CsvPath);

	FChunkEntryCsv AllContainersOutCsvFile;
	if (!bPerContainerCsvFiles)
	{
		AllContainersOutCsvFile.CreateOutputFile(*CsvPath);
	}

	for (const FString& ContainerFilePath : ContainerFilePaths)
	{
		TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, KeyChain);
		if (!Reader.IsValid())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
			continue;
		}
		
		FChunkEntryCsv PerContainerOutCsvFile;
		FChunkEntryCsv* Out;
		
		if (!bPerContainerCsvFiles)
		{
			Out = &AllContainersOutCsvFile;
		}
		else
		{
			// ContainerFilePath is a .utoc, add .csv and put in CsvPath
			FString PerContainerCsvPath = CsvPath / FPaths::GetCleanFilename(ContainerFilePath) + TEXT(".csv");
			PerContainerOutCsvFile.CreateOutputFile(*PerContainerCsvPath);
			Out = &PerContainerOutCsvFile;
		}

		if (!EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Indexed))
		{
			UE_LOG(LogIoStore, Warning, TEXT("Missing directory index for container '%s'"), *ContainerFilePath);
		}

		UE_LOG(LogIoStore, Display, TEXT("Listing container '%s'"), *ContainerFilePath);

		FString ContainerName = FPaths::GetBaseFilename(ContainerFilePath);
		TArray<FIoStoreTocChunkInfo> Chunks;

		{
			IOSTORE_CPU_SCOPE(EnumerateChunks);
			Reader->EnumerateChunks([&Chunks](FIoStoreTocChunkInfo&& ChunkInfo)
			{
				Chunks.Add(MoveTemp(ChunkInfo));
				return true;
			});
		}

		{
			IOSTORE_CPU_SCOPE(EnumerateChunks);
			auto SortKey = [](const FIoStoreTocChunkInfo& ChunkInfo) { return ChunkInfo.OffsetOnDisk; };
			Algo::SortBy(Chunks, SortKey);
		}

		{
			IOSTORE_CPU_SCOPE(WriteCsvFile);
			FString PackageName;
			for(int32 Index=0; Index < Chunks.Num(); ++Index)
			{
				const FIoStoreTocChunkInfo& ChunkInfo = Chunks[Index];

				FPackageId PackageId;
				PackageName.Reset();
				if (ChunkInfo.bHasValidFileName && FPackageName::TryConvertFilenameToLongPackageName(ChunkInfo.FileName, PackageName, nullptr))
				{
					PackageId = FPackageId::FromName(FName(*PackageName));
				}

				Out->AddChunk(ContainerName, Index, ChunkInfo, PackageId, PackageName);
			}
		}
	}

	return 0;
}

bool ListIoStoreContainer(const TCHAR* CmdLine)
{
	FKeyChain KeyChain;
	LoadKeyChain(CmdLine, KeyChain);

	FString ContainerPathOrWildcard;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ListContainer="), ContainerPathOrWildcard))
	{
		UE_LOG(LogIoStore, Error, TEXT("Missing argument -ListContainer=<ContainerFileOrWildCard>"));
		return false;
	}

	FString CsvPath;
	if (!FParse::Value(FCommandLine::Get(), TEXT("csv="), CsvPath))
	{
		UE_LOG(LogIoStore, Error, TEXT("Missing argument -Csv=<Path>"));
		return false;
	}

	return ListContainer(KeyChain, ContainerPathOrWildcard, CsvPath) == 0;
}

bool ListContainerBulkData(
	const FKeyChain& KeyChain,
	const FString& ContainerPathOrWildcard,
	const FString& OutFile)
{
	struct FPackageData
	{
		FPackageId Id;
		FString Filename;
		TArray<FBulkDataMapEntry> BulkDataMap;
	};

	struct FContainerData
	{
		FString Name;
		FString Path;
		TArray<FPackageData> Packages;
	};

	TArray<FContainerData> Containers;
	TArray<FString> ContainerFilePaths;

	if (IFileManager::Get().FileExists(*ContainerPathOrWildcard))
	{
		ContainerFilePaths.Add(ContainerPathOrWildcard);
	}
	else if (IFileManager::Get().DirectoryExists(*ContainerPathOrWildcard))
	{
		FString Directory = ContainerPathOrWildcard;
		FPaths::NormalizeDirectoryName(Directory);

		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFiles(FoundContainerFiles, *(Directory / TEXT("*.utoc")), true, false);

		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Directory / Filename);
		}
	}
	else
	{
		FString Directory = FPaths::GetPath(ContainerPathOrWildcard);
		FPaths::NormalizeDirectoryName(Directory);

		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFiles(FoundContainerFiles, *ContainerPathOrWildcard, true, false);

		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Directory / Filename);
		}
	}

	if (ContainerFilePaths.Num() == 0)
	{
		UE_LOG(LogIoStore, Error, TEXT("Container '%s' doesn't exist and no container matches wildcard."), *ContainerPathOrWildcard);
		return false;
	}

	for (const FString& ContainerFilePath : ContainerFilePaths)
	{
		FString ContainerName = FPaths::GetBaseFilename(ContainerFilePath);
		if (ContainerName == TEXT("global"))
		{
			continue;
		}

		TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, KeyChain);
		if (!Reader.IsValid())
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed to read container '%s'"), *ContainerFilePath);
			continue;
		}

		TMap<FIoChunkId, FString> FilenameByChunkId;
		Reader->GetDirectoryIndexReader().IterateDirectoryIndex(FIoDirectoryIndexHandle::RootDirectory(), TEXT(""),
			[&FilenameByChunkId, &Reader](FStringView Filename, uint32 TocEntryIndex) -> bool
			{
				TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = Reader->GetChunkInfo(TocEntryIndex);
				if (ChunkInfo.IsOk())
				{
					FilenameByChunkId.Add(ChunkInfo.ValueOrDie().Id, FString(Filename));
				}
				return true;
			});

		UE_LOG(LogIoStore, Display, TEXT("Listing bulk data in container '%s'"), *ContainerFilePath);
		FIoChunkId ChunkId = CreateIoChunkId(Reader->GetContainerId().Value(), 0, EIoChunkType::ContainerHeader);
		TIoStatusOr<FIoBuffer> Status = Reader->Read(ChunkId, FIoReadOptions());

		if (!Status.IsOk())
		{
			UE_LOG(LogIoStore, Display, TEXT("Failed to read container header '%s', reason '%s'"),
				*ContainerFilePath, *Status.Status().ToString());
			continue;
		}

		FIoContainerHeader ContainerHeader;
		{
			FIoBuffer Chunk = Status.ValueOrDie();
			FMemoryReaderView Ar(MakeArrayView(Chunk.Data(), Chunk.GetSize()));
			Ar << ContainerHeader;
		}

		FContainerData& Container = Containers.AddDefaulted_GetRef();
		Container.Path = ContainerFilePath;
		Container.Name = MoveTemp(ContainerName);

		for (const FPackageId& PackageId : ContainerHeader.PackageIds)
		{
			ChunkId = CreatePackageDataChunkId(PackageId);
			Status = Reader->Read(ChunkId, FIoReadOptions());
			if (!Status.IsOk())
			{
				UE_LOG(LogIoStore, Display, TEXT("Failed to package data"));
				continue;
			}

			FIoBuffer Chunk = Status.ValueOrDie();
			FZenPackageHeader PkgHeader = FZenPackageHeader::MakeView(Chunk.GetView());
			FPackageData& Pkg = Container.Packages.AddDefaulted_GetRef();
			Pkg.Id = PackageId;
			Pkg.BulkDataMap = PkgHeader.BulkDataMap;
			if (FString* Filename = FilenameByChunkId.Find(ChunkId))
			{
				Pkg.Filename = *Filename;
			}
		}
	}

	const FString Ext = FPaths::GetExtension(OutFile);
	if (Ext == TEXT("json"))
	{
		using FWriter = TSharedPtr<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>>; 
		using FWriterFactory = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;

		FString Json;
		FWriter Writer = FWriterFactory::Create(&Json);
		Writer->WriteArrayStart();
		
		TStringBuilder<512> Sb;
		for (const FContainerData& Container: Containers)
		{
			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("Container"), Container.Name);
			Writer->WriteArrayStart(TEXT("Packages"));
			for (const FPackageData& Pkg : Container.Packages)
			{
				if (Pkg.BulkDataMap.IsEmpty())
				{
					continue;
				}

				Writer->WriteObjectStart();
				Writer->WriteValue(TEXT("PackageId"), FString::Printf(TEXT("0x%llX"), Pkg.Id.Value()));
				Writer->WriteValue(TEXT("Filename"), Pkg.Filename);
				Writer->WriteArrayStart(TEXT("BulkData"));
				for (const FBulkDataMapEntry& Entry : Pkg.BulkDataMap)
				{
					Sb.Reset();
					LexToString(static_cast<EBulkDataFlags>(Entry.Flags), Sb);
					Writer->WriteObjectStart();
					Writer->WriteValue(TEXT("Offset"), Entry.SerialOffset);
					Writer->WriteValue(TEXT("Size"), Entry.SerialSize);
					Writer->WriteValue(TEXT("Flags"), Sb.ToString());
					Writer->WriteObjectEnd();
				}
				Writer->WriteArrayEnd();
				Writer->WriteObjectEnd();
			}
			Writer->WriteArrayEnd();
			Writer->WriteObjectEnd();
		}

		Writer->WriteArrayEnd();
		Writer->Close();

		UE_LOG(LogIoStore, Display, TEXT("Saving '%s'"), *OutFile);
		if (!FFileHelper::SaveStringToFile(Json, *OutFile))
		{
			return false;
		}
	}
	else
	{
		TUniquePtr<FArchive> CsvAr(IFileManager::Get().CreateFileWriter(*OutFile));
		CsvAr->Logf(TEXT("Container,Filename,PackageId,Offset,Size,Flags"));

		TStringBuilder<512> Sb;
		for (const FContainerData& Container: Containers)
		{
			for (const FPackageData& Pkg : Container.Packages)
			{
				for (const FBulkDataMapEntry& Entry : Pkg.BulkDataMap)
				{
					Sb.Reset();
					LexToString(static_cast<EBulkDataFlags>(Entry.Flags), Sb);
					CsvAr->Logf(TEXT("%s,%s,0x%llX,%lld,%lld,%s"),
						*Container.Name, *Pkg.Filename, Pkg.Id.Value(), Entry.SerialOffset, Entry.SerialSize, Sb.ToString());
				}
			}
		}

		UE_LOG(LogIoStore, Display, TEXT("Saving '%s'"), *OutFile);
	}

	return true;
}

bool ListIoStoreContainerBulkData(const TCHAR* CmdLine)
{
	FKeyChain KeyChain;
	LoadKeyChain(CmdLine, KeyChain);

	FString ContainerPathOrWildcard;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ListContainerBulkData="), ContainerPathOrWildcard))
	{
		UE_LOG(LogIoStore, Error, TEXT("Missing argument -ListContainerBulkData=<ContainerFileOrWildCard>"));
		return false;
	}

	FString OutFile;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Out="), OutFile))
	{
		UE_LOG(LogIoStore, Error, TEXT("Missing argument -Out=<Path.[json|csv]>"));
		return false;
	}

	return ListContainerBulkData(KeyChain, ContainerPathOrWildcard, OutFile) == 0;
}

bool LegacyListIoStoreContainer(
	const TCHAR* InContainerFilename,
	int64 InSizeFilter,
	const FString& InCSVFilename,
	const FKeyChain& InKeyChain)
{
	TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(InContainerFilename, InKeyChain);
	if (!Reader.IsValid())
	{
		return false;
	}

	if (!EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Indexed))
	{
		UE_LOG(LogIoStore, Fatal, TEXT("Missing directory index for container '%s'"), InContainerFilename);
	}

	int32 FileCount = 0;
	int64 FileSize = 0;

	TArray<FString> CompressionMethodNames;
	for (const FName& CompressionMethodName : Reader->GetCompressionMethods())
	{
		CompressionMethodNames.Add(CompressionMethodName.ToString());
	}

	TArray<FIoStoreCompressedBlockInfo> CompressionBlocks;
	TArray<FIoStoreTocCompressedBlockInfo> CompressedBlocks;
	Reader->EnumerateCompressedBlocks([&CompressedBlocks](const FIoStoreTocCompressedBlockInfo& Block)
		{
			CompressedBlocks.Add(Block);
			return true;
		});

	const FIoDirectoryIndexReader& IndexReader = Reader->GetDirectoryIndexReader();
	UE_LOG(LogIoStore, Display, TEXT("Mount point %s"), *IndexReader.GetMountPoint());

	struct FEntry
	{
		FIoChunkId ChunkId;
		FIoChunkHash Hash;
		FString FileName;
		int64 Offset;
		int64 Size;
		int32 CompressionMethodIndex;
	};
	TArray<FEntry> Entries;

	const uint64 CompressionBlockSize = Reader->GetCompressionBlockSize();
	Reader->EnumerateChunks([&Entries, CompressionBlockSize, &CompressedBlocks](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			const int32 FirstBlockIndex = int32(ChunkInfo.Offset / CompressionBlockSize);
			
			FEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.ChunkId = ChunkInfo.Id;
			Entry.Hash = ChunkInfo.Hash;
			Entry.FileName = ChunkInfo.FileName;
			Entry.Offset = CompressedBlocks[FirstBlockIndex].Offset;
			Entry.Size = ChunkInfo.CompressedSize;
			Entry.CompressionMethodIndex = CompressedBlocks[FirstBlockIndex].CompressionMethodIndex;
			return true;
		});

	struct FOffsetSort
	{
		bool operator()(const FEntry& A, const FEntry& B) const
		{
			return A.Offset < B.Offset;
		}
	};
	Entries.Sort(FOffsetSort());

	FileCount = Entries.Num();

	if (InCSVFilename.Len() > 0)
	{
		TArray<FString> Lines;
		Lines.Empty(Entries.Num() + 2);
		Lines.Add(TEXT("Filename, Offset, Size, Hash, Deleted, Compressed, CompressionMethod"));
		for (const FEntry& Entry : Entries)
		{
			bool bWasCompressed = Entry.CompressionMethodIndex != 0;
			Lines.Add(FString::Printf(
				TEXT("%s, %lld, %lld, %s, %s, %s, %d"),
				*Entry.FileName,
				Entry.Offset,
				Entry.Size,
				*Entry.Hash.ToString(),
				TEXT("false"),
				bWasCompressed ? TEXT("true") : TEXT("false"),
				Entry.CompressionMethodIndex));
		}

		if (FFileHelper::SaveStringArrayToFile(Lines, *InCSVFilename) == false)
		{
			UE_LOG(LogIoStore, Display, TEXT("Failed to save CSV file %s"), *InCSVFilename);
		}
		else
		{
			UE_LOG(LogIoStore, Display, TEXT("Saved CSV file to %s"), *InCSVFilename);
		}
	}

	for (const FEntry& Entry : Entries)
	{
		if (Entry.Size >= InSizeFilter)
		{
			UE_LOG(LogIoStore, Display, TEXT("\"%s\" offset: %lld, size: %d bytes, hash: %s, compression: %s."),
				*Entry.FileName,
				Entry.Offset,
				Entry.Size,
				*Entry.Hash.ToString(),
				*CompressionMethodNames[Entry.CompressionMethodIndex]);
		}
		FileSize += Entry.Size;
	}
	UE_LOG(LogIoStore, Display, TEXT("%d files (%lld bytes)."), FileCount, FileSize);

	return true;
}

int32 ProfileReadSpeed(const TCHAR* InCommandLine, const FKeyChain& InKeyChain)
{
	FString ContainerPath;
	if (FParse::Value(InCommandLine, TEXT("Container="), ContainerPath) == false)
	{
		UE_LOG(LogIoStore, Display, TEXT(""));
		UE_LOG(LogIoStore, Display, TEXT("ProfileReadSpeed"));
		UE_LOG(LogIoStore, Display, TEXT(""));
		UE_LOG(LogIoStore, Display, TEXT("Reads the given utoc file using given a read method. This uses FIoStoreReader, which is not"));
		UE_LOG(LogIoStore, Display, TEXT("the system the runtime uses to load and stream iostore containers! It's for utility/debug use only."));
		UE_LOG(LogIoStore, Display, TEXT(""));
		UE_LOG(LogIoStore, Display, TEXT("Arguments:"));
		UE_LOG(LogIoStore, Display, TEXT(""));
		UE_LOG(LogIoStore, Display, TEXT("    -Container=path/to/utoc                      [required] The .utoc file to read."));
		UE_LOG(LogIoStore, Display, TEXT("    -ReadType={Read, ReadAsync, ReadCompressed}  What read function to use on FIoStoreReader. Default: Read"));
		UE_LOG(LogIoStore, Display, TEXT("    -cryptokeys=path/to/crypto.json              [required if encrypted] The keys to decrypt the container."));
		UE_LOG(LogIoStore, Display, TEXT("    -MaxJobCount=#                               The number of outstanding read tasks to maintain. Default: 512."));
		UE_LOG(LogIoStore, Display, TEXT("    -Validate                                    Whether to hash the reads and verify they match. Invalid for ReadCompressed. Default: disabled"));
		return 1;
	}

	TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerPath, InKeyChain);
	if (Reader.IsValid() == false)
	{
		return 1; // Already logged
	}

	TArray<FIoChunkId> Chunks;
	Reader->EnumerateChunks([&Chunks](const FIoStoreTocChunkInfo& ChunkInfo)
	{
		Chunks.Add(ChunkInfo.Id);
		return true;
	});

	enum class EReadType
	{
		Read,
		ReadAsync,
		ReadCompressed
	};

	auto ReadTypeToString = [](EReadType InReadType)
	{
		switch (InReadType)
		{
		case EReadType::Read: return TEXT("Read");
		case EReadType::ReadAsync: return TEXT("ReadAsync");
		case EReadType::ReadCompressed: return TEXT("ReadCompressed");
		default: return TEXT("INVALID");
		}
	};

	EReadType ReadType = EReadType::Read;
	int32 MaxOutstandingJobs = 512;
	bool bValidate = false;

	bValidate = FParse::Param(InCommandLine, TEXT("Validate"));
	FParse::Value(InCommandLine, TEXT("MaxJobCount="), MaxOutstandingJobs);

	FString ReadTypeRaw;
	if (FParse::Value(InCommandLine, TEXT("ReadType="), ReadTypeRaw))
	{
		if (ReadTypeRaw.Compare(TEXT("Read"), ESearchCase::IgnoreCase) == 0)
		{
			ReadType = EReadType::Read;
		}
		else if (ReadTypeRaw.Compare(TEXT("ReadAsync"), ESearchCase::IgnoreCase) == 0)
		{
			ReadType = EReadType::ReadAsync;
		}
		else if (ReadTypeRaw.Compare(TEXT("ReadCompressed"), ESearchCase::IgnoreCase) == 0)
		{
			ReadType = EReadType::ReadCompressed;
		}
		else
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid -ReadType provided: %s. Valid are {Read, ReadAsync, ReadCompressed}"), *ReadTypeRaw);
			return 1;
		}
	}

	if (MaxOutstandingJobs <= 0)
	{
		UE_LOG(LogIoStore, Error, TEXT("Invalid -MaxJobCount provided: %d. Specify a positive integer"), MaxOutstandingJobs);
		return 1;
	}

	if (ReadType == EReadType::ReadCompressed && 
		bValidate)
	{
		UE_LOG(LogIoStore, Error, TEXT("Can't validate ReadCompressed as the data is not decompressed and thus can't be hashed"));
		return 1;
	}

	UE_LOG(LogIoStore, Display, TEXT("MaxJobCount:            %s"), *FText::AsNumber(MaxOutstandingJobs).ToString());
	UE_LOG(LogIoStore, Display, TEXT("ReadType:               %s"), ReadTypeToString(ReadType));
	UE_LOG(LogIoStore, Display, TEXT("Validation:             %s"), bValidate ? TEXT("Enabled") : TEXT("Disabled"));
	UE_LOG(LogIoStore, Display, TEXT("Container Encrypted:    %s"), EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Encrypted) ? TEXT("Yes") : TEXT("No"));
	
	// We need a resettable event, so we can't use any of the task system events.
	FEvent* JustGotSpaceEvent = FPlatformProcess::GetSynchEventFromPool();
	UE::Tasks::FTaskEvent CompletedEvent(TEXT("ProfileReadDone"));

	std::atomic_int32_t OutstandingJobs = 0;
	std::atomic_int32_t TotalJobsRemaining = Chunks.Num();
	std::atomic_int64_t BytesRead = 0;

	double StartTime = FPlatformTime::Seconds();
	UE_LOG(LogIoStore, Display, TEXT("Dispatching %s chunk reads (%s max at one time)"), *FText::AsNumber(Chunks.Num()).ToString(), *FText::AsNumber(MaxOutstandingJobs).ToString());

	for (FIoChunkId& Id : Chunks)
	{
		for (;;)
		{
			int32 CurrentOutstanding = OutstandingJobs.load();
			if (CurrentOutstanding == MaxOutstandingJobs)
			{
				// Wait for one to complete.
				JustGotSpaceEvent->Wait();
				continue;
			}
			else if (CurrentOutstanding > MaxOutstandingJobs)
			{
				UE_LOG(LogIoStore, Warning, TEXT("Synch error -- too many jobs oustanding %d"), CurrentOutstanding);
			}
			break;
		}

		OutstandingJobs++;
		UE::Tasks::Launch(TEXT("IoStoreUtil::ReadJob"), [Id = Id, &OutstandingJobs, &JustGotSpaceEvent, &TotalJobsRemaining, &BytesRead, &Reader, &CompletedEvent, MaxOutstandingJobs, ReadType, bValidate]()
		{
			FIoChunkHash ReadHash;
			bool bHashValid = false;

			switch (ReadType)
			{
			case EReadType::ReadCompressed:
				{
					FIoStoreCompressedReadResult Result = Reader->ReadCompressed(Id, FIoReadOptions()).ValueOrDie();
					BytesRead += Result.IoBuffer.GetSize();
					break;
				}
			case EReadType::Read:
				{
					FIoBuffer Result = Reader->Read(Id, FIoReadOptions()).ValueOrDie();
					BytesRead += Result.GetSize();

					if (bValidate)
					{
						ReadHash = FIoChunkHash::HashBuffer(Result.GetData(), Result.GetSize());
						bHashValid = true;
					}

					break;
				}
			case EReadType::ReadAsync:
				{
					UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> Tasks = Reader->ReadAsync(Id, FIoReadOptions());
					Tasks.BusyWait();
					FIoBuffer Result = Tasks.GetResult().ValueOrDie();

					BytesRead += Result.GetSize();

					if (bValidate)
					{
						ReadHash = FIoChunkHash::HashBuffer(Result.GetData(), Result.GetSize());
						bHashValid = true;
					}
					break;
				}
			}

			if (bHashValid)
			{
				FIoChunkHash CheckAgainstHash = Reader->GetChunkInfo(Id).ValueOrDie().Hash;
				if (ReadHash != CheckAgainstHash)
				{
					UE_LOG(LogIoStore, Warning, TEXT("Read hash mismatch: Chunk %s"), *LexToString(Id));
				}
			}

			if (OutstandingJobs.fetch_add(-1) == MaxOutstandingJobs)
			{
				// We are the first to make space in our limit, so release the dispatch thread to add more.
				JustGotSpaceEvent->Trigger();
			}

			int32 JobsRemaining = TotalJobsRemaining.fetch_add(-1);
			if ((JobsRemaining % 1000) == 1)
			{
				UE_LOG(LogIoStore, Display, TEXT("Jobs Remaining: %d"), JobsRemaining - 1);
			}

			// Were we the last job issued?
			if (JobsRemaining == 1)
			{
				CompletedEvent.Trigger();
			}
		});
	}

	{
		double WaitStartTime = FPlatformTime::Seconds();
		CompletedEvent.BusyWait();
		UE_LOG(LogIoStore, Display, TEXT("Waited %.1f seconds"), FPlatformTime::Seconds() - WaitStartTime);
	}

	FPlatformProcess::ReturnSynchEventToPool(JustGotSpaceEvent);

	double TotalTime = FPlatformTime::Seconds() - StartTime;

	int64 BytesPerSecond = int64(BytesRead.load() / TotalTime);

	UE_LOG(LogIoStore, Display, TEXT("%s bytes in %.1f seconds; %s bytes per second"), *FText::AsNumber((int64)BytesRead.load()).ToString(), TotalTime, *FText::AsNumber(BytesPerSecond).ToString());
	return 0;
}

namespace DescribeUtils
{
	struct FPackageDesc;

	struct FPackageRedirect
	{
		FPackageDesc* Source = nullptr;
		FPackageDesc* Target = nullptr;
	};

	struct FContainerDesc
	{
		FName Name;
		FIoContainerId Id;
		FGuid EncryptionKeyGuid;
		TArray<FPackageDesc*> LocalizedPackages;
		TArray<FPackageRedirect> PackageRedirects;
		bool bCompressed;
		bool bSigned;
		bool bEncrypted;
		bool bIndexed;
	};

	struct FPackageLocation
	{
		FContainerDesc* Container = nullptr;
		uint64 Offset = -1;
	};

	struct FExportDesc
	{
		FPackageDesc* Package = nullptr;
		FName Name;
		FName FullName;
		uint64 PublicExportHash;
		FPackageObjectIndex OuterIndex;
		FPackageObjectIndex ClassIndex;
		FPackageObjectIndex SuperIndex;
		FPackageObjectIndex TemplateIndex;
		uint64 SerialOffset = 0;
		uint64 SerialSize = 0;
		FSHAHash ExportHash;
	};

	struct FExportBundleEntryDesc
	{
		FExportBundleEntry::EExportCommandType CommandType = FExportBundleEntry::ExportCommandType_Count;
		int32 LocalExportIndex = -1;
		FExportDesc* Export = nullptr;
	};

	struct FImportDesc
	{
		FName Name;
		FPackageObjectIndex GlobalImportIndex;
		FExportDesc* Export = nullptr;
	};

	struct FScriptObjectDesc
	{
		FName Name;
		FName FullName;
		FPackageObjectIndex GlobalImportIndex;
		FPackageObjectIndex OuterIndex;
	};

	struct FPackageDesc
	{
		FPackageId PackageId;
		FName PackageName;
		uint32 PackageFlags = 0;
		int32 NameCount = -1;
		TArray<FPackageLocation, TInlineAllocator<1>> Locations;
		TArray<FPackageId> ImportedPackageIds;
		TArray<uint64> ImportedPublicExportHashes;
		TArray<FImportDesc> Imports;
		TArray<FExportDesc> Exports;
		TArray<FExportBundleEntryDesc> ExportBundleEntries;
	};

	// Info loaded about a set of containers for the purposes of dumping to text in Describe or exploring some other way for debugging 
	struct FContainerPackageInfo
	{
		TArray<FContainerDesc*> Containers;
		TArray<FPackageDesc*> Packages;
		TMap<FPackageObjectIndex, FScriptObjectDesc> ScriptObjectByGlobalIdMap;
		TMap<FPublicExportKey, FExportDesc*> ExportByKeyMap;

		FContainerPackageInfo() = default;
		FContainerPackageInfo(
			TArray<FContainerDesc*> InContainers,
			TArray<FPackageDesc*> InPackages,
			TMap<FPackageObjectIndex, FScriptObjectDesc> InScriptObjectByGlobalIdMap,
			TMap<FPublicExportKey, FExportDesc*> InExportByKeyMap)
			: Containers(MoveTemp(InContainers))
			, Packages(MoveTemp(InPackages))
			, ScriptObjectByGlobalIdMap(MoveTemp(InScriptObjectByGlobalIdMap))
			, ExportByKeyMap(MoveTemp(InExportByKeyMap))
		{}

		FContainerPackageInfo(const FContainerPackageInfo&) = delete;
		FContainerPackageInfo(FContainerPackageInfo&&) = default;
		FContainerPackageInfo& operator=(const FContainerPackageInfo&) = delete;
		FContainerPackageInfo& operator=(FContainerPackageInfo&&) = default;
		~FContainerPackageInfo()
		{
			for (FPackageDesc* PackageDesc : Packages)
			{
				delete PackageDesc;
			}
			for (FContainerDesc* ContainerDesc : Containers)
			{
				delete ContainerDesc;
			}
		}

		FString PackageObjectIndexToString(const FPackageDesc* Package, const FPackageObjectIndex& PackageObjectIndex, bool bIncludeName)
		{
			if (PackageObjectIndex.IsNull())
			{
				return TEXT("<null>");
			}
			else if (PackageObjectIndex.IsPackageImport())
			{
				FPublicExportKey Key = FPublicExportKey::FromPackageImport(PackageObjectIndex, Package->ImportedPackageIds, Package->ImportedPublicExportHashes);
				FExportDesc* ExportDesc = ExportByKeyMap.FindRef(Key);
				if (ExportDesc && bIncludeName)
				{
					return FString::Printf(TEXT("0x%llX '%s'"), PackageObjectIndex.Value(), *ExportDesc->FullName.ToString());
				}
				else
				{
					return FString::Printf(TEXT("0x%llX"), PackageObjectIndex.Value());
				}
			}
			else if (PackageObjectIndex.IsScriptImport())
			{
				const FScriptObjectDesc* ScriptObjectDesc = ScriptObjectByGlobalIdMap.Find(PackageObjectIndex);
				if (ScriptObjectDesc && bIncludeName)
				{
					return FString::Printf(TEXT("0x%llX '%s'"), PackageObjectIndex.Value(), *ScriptObjectDesc->FullName.ToString());
				}
				else
				{
					return FString::Printf(TEXT("0x%llX"), PackageObjectIndex.Value());
				}
			}
			else if (PackageObjectIndex.IsExport())
			{
				return FString::Printf(TEXT("%d"), PackageObjectIndex.Value());
			}
			else
			{
				return FString::Printf(TEXT("0x%llX"), PackageObjectIndex.Value());
			}
		}
	};

	// Try and read all packages inside the containers and the links between them for debugging/analysis
	TOptional<FContainerPackageInfo> TryGetContainerPackageInfo(
		const FString& GlobalContainerPath,
		const FKeyChain& KeyChain,
		bool bIncludeExportHashes
	)
	{
		if (!IFileManager::Get().FileExists(*GlobalContainerPath))
		{
			UE_LOG(LogIoStore, Error, TEXT("Global container '%s' doesn't exist."), *GlobalContainerPath);
			return {};
		}

		TUniquePtr<FIoStoreReader> GlobalReader = CreateIoStoreReader(*GlobalContainerPath, KeyChain);
		if (!GlobalReader.IsValid())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed reading global container '%s'"), *GlobalContainerPath);
			return {};
		}

		UE_LOG(LogIoStore, Display, TEXT("Loading script imports..."));

		TIoStatusOr<FIoBuffer> ScriptObjectsBuffer = GlobalReader->Read(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects), FIoReadOptions());
		if (!ScriptObjectsBuffer.IsOk())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed reading initial load meta chunk from global container '%s'"), *GlobalContainerPath);
			return {};
		}

		TMap<FPackageObjectIndex, FScriptObjectDesc> ScriptObjectByGlobalIdMap;
		FLargeMemoryReader ScriptObjectsArchive(ScriptObjectsBuffer.ValueOrDie().Data(), ScriptObjectsBuffer.ValueOrDie().DataSize());
		TArray<FDisplayNameEntryId> GlobalNameMap = LoadNameBatch(ScriptObjectsArchive);
		int32 NumScriptObjects = 0;
		ScriptObjectsArchive << NumScriptObjects;
		const FScriptObjectEntry* ScriptObjectEntries = reinterpret_cast<const FScriptObjectEntry*>(ScriptObjectsBuffer.ValueOrDie().Data() + ScriptObjectsArchive.Tell());
		for (int32 ScriptObjectIndex = 0; ScriptObjectIndex < NumScriptObjects; ++ScriptObjectIndex)
		{
			const FScriptObjectEntry& ScriptObjectEntry = ScriptObjectEntries[ScriptObjectIndex];
			FMappedName MappedName = ScriptObjectEntry.Mapped;
			check(MappedName.IsGlobal());
			FScriptObjectDesc& ScriptObjectDesc = ScriptObjectByGlobalIdMap.Add(ScriptObjectEntry.GlobalIndex);
			ScriptObjectDesc.Name = GlobalNameMap[MappedName.GetIndex()].ToName(MappedName.GetNumber());
			ScriptObjectDesc.GlobalImportIndex = ScriptObjectEntry.GlobalIndex;
			ScriptObjectDesc.OuterIndex = ScriptObjectEntry.OuterIndex;
		}
		for (auto& KV : ScriptObjectByGlobalIdMap)
		{
			FScriptObjectDesc& ScriptObjectDesc = KV.Get<1>();
			if (ScriptObjectDesc.FullName.IsNone())
			{
				TArray<FScriptObjectDesc*> ScriptObjectStack;
				FScriptObjectDesc* Current = &ScriptObjectDesc;
				FString FullName;
				while (Current)
				{
					if (!Current->FullName.IsNone())
					{
						FullName = Current->FullName.ToString();
						break;
					}
					ScriptObjectStack.Push(Current);
					Current = ScriptObjectByGlobalIdMap.Find(Current->OuterIndex);
				}
				while (ScriptObjectStack.Num() > 0)
				{
					Current = ScriptObjectStack.Pop();
					FullName /= Current->Name.ToString();
					Current->FullName = FName(FullName);
				}
			}
		}

		FString Directory = FPaths::GetPath(GlobalContainerPath);
		FPaths::NormalizeDirectoryName(Directory);

		TArray<FString> FoundContainerFiles;
		IFileManager::Get().FindFiles(FoundContainerFiles, *(Directory / TEXT("*.utoc")), true, false);
		TArray<FString> ContainerFilePaths;
		for (const FString& Filename : FoundContainerFiles)
		{
			ContainerFilePaths.Emplace(Directory / Filename);
		}

		UE_LOG(LogIoStore, Display, TEXT("Loading containers..."));

		TArray<TUniquePtr<FIoStoreReader>> Readers;

		struct FLoadContainerHeaderJob
		{
			FName ContainerName;
			FContainerDesc* ContainerDesc = nullptr;
			TArray<FPackageDesc*> Packages;
			FIoStoreReader* Reader = nullptr;
			TArray<FIoContainerHeaderLocalizedPackage> RawLocalizedPackages;
			TArray<FIoContainerHeaderPackageRedirect> RawPackageRedirects;
		};

		TArray<FLoadContainerHeaderJob> LoadContainerHeaderJobs;

		for (const FString& ContainerFilePath : ContainerFilePaths)
		{
			TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, KeyChain);
			if (!Reader.IsValid())
			{
				UE_LOG(LogIoStore, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
				continue;
			}

			FLoadContainerHeaderJob& LoadContainerHeaderJob = LoadContainerHeaderJobs.AddDefaulted_GetRef();
			LoadContainerHeaderJob.Reader = Reader.Get();
			LoadContainerHeaderJob.ContainerName = FName(FPaths::GetBaseFilename(ContainerFilePath));
			
			Readers.Emplace(MoveTemp(Reader));
		}
		
		TAtomic<int32> TotalPackageCount{ 0 };
		ParallelFor(LoadContainerHeaderJobs.Num(), [&LoadContainerHeaderJobs, &TotalPackageCount](int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LoadContainerHeader);

			FLoadContainerHeaderJob& Job = LoadContainerHeaderJobs[Index];

			FContainerDesc* ContainerDesc = new FContainerDesc();
			ContainerDesc->Name = Job.ContainerName;
			ContainerDesc->Id = Job.Reader->GetContainerId();
			ContainerDesc->EncryptionKeyGuid = Job.Reader->GetEncryptionKeyGuid();
			EIoContainerFlags Flags = Job.Reader->GetContainerFlags();
			ContainerDesc->bCompressed = bool(Flags & EIoContainerFlags::Compressed);
			ContainerDesc->bEncrypted = bool(Flags & EIoContainerFlags::Encrypted);
			ContainerDesc->bSigned = bool(Flags & EIoContainerFlags::Signed);
			ContainerDesc->bIndexed = bool(Flags & EIoContainerFlags::Indexed);
			Job.ContainerDesc = ContainerDesc;

			TIoStatusOr<FIoBuffer> IoBuffer = Job.Reader->Read(CreateIoChunkId(Job.Reader->GetContainerId().Value(), 0, EIoChunkType::ContainerHeader), FIoReadOptions());
			if (IoBuffer.IsOk())
			{
				FMemoryReaderView Ar(MakeArrayView(IoBuffer.ValueOrDie().Data(), IoBuffer.ValueOrDie().DataSize()));
				FIoContainerHeader ContainerHeader;
				Ar << ContainerHeader;

				Job.RawLocalizedPackages = ContainerHeader.LocalizedPackages;
				Job.RawPackageRedirects = ContainerHeader.PackageRedirects;

				TArrayView<FFilePackageStoreEntry> StoreEntries(reinterpret_cast<FFilePackageStoreEntry*>(ContainerHeader.StoreEntries.GetData()), ContainerHeader.PackageIds.Num());

				int32 PackageIndex = 0;
				Job.Packages.Reserve(StoreEntries.Num());
				for (FFilePackageStoreEntry& ContainerEntry : StoreEntries)
				{
					const FPackageId& PackageId = ContainerHeader.PackageIds[PackageIndex++];
					FPackageDesc* PackageDesc = new FPackageDesc();
					PackageDesc->PackageId = PackageId;
					PackageDesc->ImportedPackageIds = TArrayView<FPackageId>(ContainerEntry.ImportedPackages.Data(), ContainerEntry.ImportedPackages.Num());
					Job.Packages.Add(PackageDesc);
					++TotalPackageCount;
				}
			}
		}, EParallelForFlags::Unbalanced);

		struct FLoadPackageSummaryJob
		{
			FPackageDesc* PackageDesc = nullptr;
			FIoChunkId ChunkId;
			TArray<FLoadContainerHeaderJob*, TInlineAllocator<1>> Containers;
		};

		TArray<FLoadPackageSummaryJob> LoadPackageSummaryJobs;

		TArray<FContainerDesc*> Containers;
		TArray<FPackageDesc*> Packages;
		TMap<FPackageId, FPackageDesc*> PackageByIdMap;
		TMap<FPackageId, FLoadPackageSummaryJob*> PackageJobByIdMap;
		Containers.Reserve(LoadContainerHeaderJobs.Num());
		Packages.Reserve(TotalPackageCount);
		PackageByIdMap.Reserve(TotalPackageCount);
		PackageJobByIdMap.Reserve(TotalPackageCount);
		LoadPackageSummaryJobs.Reserve(TotalPackageCount);
		for (FLoadContainerHeaderJob& LoadContainerHeaderJob : LoadContainerHeaderJobs)
		{
			Containers.Add(LoadContainerHeaderJob.ContainerDesc);
			for (FPackageDesc* PackageDesc : LoadContainerHeaderJob.Packages)
			{
				FLoadPackageSummaryJob*& UniquePackageJob = PackageJobByIdMap.FindOrAdd(PackageDesc->PackageId);
				if (!UniquePackageJob)
				{
					Packages.Add(PackageDesc);
					PackageByIdMap.Add(PackageDesc->PackageId, PackageDesc);
					FLoadPackageSummaryJob& LoadPackageSummaryJob = LoadPackageSummaryJobs.AddDefaulted_GetRef();
					LoadPackageSummaryJob.PackageDesc = PackageDesc;
					LoadPackageSummaryJob.ChunkId = CreateIoChunkId(PackageDesc->PackageId.Value(), 0, EIoChunkType::ExportBundleData);
					UniquePackageJob = &LoadPackageSummaryJob;
				}
				UniquePackageJob->Containers.Add(&LoadContainerHeaderJob);
			}
		}
		for (FLoadContainerHeaderJob& LoadContainerHeaderJob : LoadContainerHeaderJobs)
		{
			for (const auto& RedirectPair : LoadContainerHeaderJob.RawPackageRedirects)
			{
				FPackageRedirect& PackageRedirect = LoadContainerHeaderJob.ContainerDesc->PackageRedirects.AddDefaulted_GetRef();
				PackageRedirect.Source = PackageByIdMap.FindRef(RedirectPair.SourcePackageId);
				PackageRedirect.Target = PackageByIdMap.FindRef(RedirectPair.TargetPackageId);
			}
			for (const auto& LocalizedPackage : LoadContainerHeaderJob.RawLocalizedPackages)
			{
				LoadContainerHeaderJob.ContainerDesc->LocalizedPackages.Add(PackageByIdMap.FindRef(LocalizedPackage.SourcePackageId));
			}
		}
		
		ParallelFor(LoadPackageSummaryJobs.Num(), [&LoadPackageSummaryJobs, bIncludeExportHashes](int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(LoadPackageSummary);

			FLoadPackageSummaryJob& Job = LoadPackageSummaryJobs[Index];
			for (FLoadContainerHeaderJob* LoadContainerHeaderJob : Job.Containers)
			{
				TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = LoadContainerHeaderJob->Reader->GetChunkInfo(Job.ChunkId);
				check(ChunkInfo.IsOk());
				FPackageLocation& Location = Job.PackageDesc->Locations.AddDefaulted_GetRef();
				Location.Container = LoadContainerHeaderJob->ContainerDesc;
				Location.Offset = ChunkInfo.ValueOrDie().Offset;
			}

			FIoStoreReader* Reader = Job.Containers[0]->Reader;
			FIoReadOptions ReadOptions;
			if (!bIncludeExportHashes)
			{
				ReadOptions.SetRange(0, 16 << 10);
			}
			TIoStatusOr<FIoBuffer> IoBuffer = Reader->Read(Job.ChunkId, ReadOptions);
			check(IoBuffer.IsOk());
			const uint8* PackageSummaryData = IoBuffer.ValueOrDie().Data();
			const FZenPackageSummary* PackageSummary = reinterpret_cast<const FZenPackageSummary*>(PackageSummaryData);
			if (PackageSummary->HeaderSize > IoBuffer.ValueOrDie().DataSize())
			{
				ReadOptions.SetRange(0, PackageSummary->HeaderSize);
				IoBuffer = Reader->Read(Job.ChunkId, ReadOptions);
				PackageSummaryData = IoBuffer.ValueOrDie().Data();
				PackageSummary = reinterpret_cast<const FZenPackageSummary*>(PackageSummaryData);
			}

			TArrayView<const uint8> HeaderDataView(PackageSummaryData + sizeof(FZenPackageSummary), PackageSummary->HeaderSize - sizeof(FZenPackageSummary));
			FMemoryReaderView HeaderDataReader(HeaderDataView);

			FZenPackageVersioningInfo VersioningInfo;
			if (PackageSummary->bHasVersioningInfo)
			{
				HeaderDataReader << VersioningInfo;
			}

			TArray<FDisplayNameEntryId> PackageNameMap;
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LoadNameBatch);
				PackageNameMap = LoadNameBatch(HeaderDataReader);
			}

			Job.PackageDesc->PackageName = PackageNameMap[PackageSummary->Name.GetIndex()].ToName(PackageSummary->Name.GetNumber());
			Job.PackageDesc->PackageFlags = PackageSummary->PackageFlags;
			Job.PackageDesc->NameCount = PackageNameMap.Num();
			
			Job.PackageDesc->ImportedPublicExportHashes = MakeArrayView<const uint64>(reinterpret_cast<const uint64*>(PackageSummaryData + PackageSummary->ImportedPublicExportHashesOffset), (PackageSummary->ImportMapOffset - PackageSummary->ImportedPublicExportHashesOffset) / sizeof(uint64));

			const FPackageObjectIndex* ImportMap = reinterpret_cast<const FPackageObjectIndex*>(PackageSummaryData + PackageSummary->ImportMapOffset);
			Job.PackageDesc->Imports.SetNum((PackageSummary->ExportMapOffset - PackageSummary->ImportMapOffset) / sizeof(FPackageObjectIndex));
			for (int32 ImportIndex = 0; ImportIndex < Job.PackageDesc->Imports.Num(); ++ImportIndex)
			{
				FImportDesc& ImportDesc = Job.PackageDesc->Imports[ImportIndex];
				ImportDesc.GlobalImportIndex = ImportMap[ImportIndex];
			}

			const FExportMapEntry* ExportMap = reinterpret_cast<const FExportMapEntry*>(PackageSummaryData + PackageSummary->ExportMapOffset);
			Job.PackageDesc->Exports.SetNum((PackageSummary->ExportBundleEntriesOffset - PackageSummary->ExportMapOffset) / sizeof(FExportMapEntry));
			for (int32 ExportIndex = 0; ExportIndex < Job.PackageDesc->Exports.Num(); ++ExportIndex)
			{
				const FExportMapEntry& ExportMapEntry = ExportMap[ExportIndex];
				FExportDesc& ExportDesc = Job.PackageDesc->Exports[ExportIndex];
				ExportDesc.Package = Job.PackageDesc;
				ExportDesc.Name = PackageNameMap[ExportMapEntry.ObjectName.GetIndex()].ToName(ExportMapEntry.ObjectName.GetNumber());
				ExportDesc.OuterIndex = ExportMapEntry.OuterIndex;
				ExportDesc.ClassIndex = ExportMapEntry.ClassIndex;
				ExportDesc.SuperIndex = ExportMapEntry.SuperIndex;
				ExportDesc.TemplateIndex = ExportMapEntry.TemplateIndex;
				ExportDesc.PublicExportHash = ExportMapEntry.PublicExportHash;
				ExportDesc.SerialOffset = PackageSummary->HeaderSize + ExportMapEntry.CookedSerialOffset;
				ExportDesc.SerialSize = ExportMapEntry.CookedSerialSize;
			}

			const FExportBundleEntry* ExportBundleEntries = reinterpret_cast<const FExportBundleEntry*>(PackageSummaryData + PackageSummary->ExportBundleEntriesOffset);
			const FExportBundleEntry* BundleEntry = ExportBundleEntries;
			int32 ExportBundleEntriesCount = Job.PackageDesc->Exports.Num() * 2;
			const FExportBundleEntry* BundleEntryEnd = BundleEntry + ExportBundleEntriesCount;
			Job.PackageDesc->ExportBundleEntries.Reserve(ExportBundleEntriesCount);
			while (BundleEntry < BundleEntryEnd)
			{
				FExportBundleEntryDesc& EntryDesc = Job.PackageDesc->ExportBundleEntries.AddDefaulted_GetRef();
				EntryDesc.CommandType = FExportBundleEntry::EExportCommandType(BundleEntry->CommandType);
				EntryDesc.LocalExportIndex = BundleEntry->LocalExportIndex;
				EntryDesc.Export = &Job.PackageDesc->Exports[BundleEntry->LocalExportIndex];
				if (BundleEntry->CommandType == FExportBundleEntry::ExportCommandType_Serialize)
				{
					if (bIncludeExportHashes)
					{
						check(EntryDesc.Export->SerialOffset + EntryDesc.Export->SerialSize <= IoBuffer.ValueOrDie().DataSize());
						FSHA1::HashBuffer(IoBuffer.ValueOrDie().Data() + EntryDesc.Export->SerialOffset, EntryDesc.Export->SerialSize, EntryDesc.Export->ExportHash.Hash);
					}
				}
				++BundleEntry;
			}
		}, EParallelForFlags::Unbalanced);

		UE_LOG(LogIoStore, Display, TEXT("Connecting imports and exports..."));
		TMap<FPublicExportKey, FExportDesc*> ExportByKeyMap;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ConnectImportsAndExports);

			for (FPackageDesc* PackageDesc : Packages)
			{
				for (FExportDesc& ExportDesc : PackageDesc->Exports)
				{
					if (ExportDesc.PublicExportHash)
					{
						FPublicExportKey Key = FPublicExportKey::MakeKey(PackageDesc->PackageId, ExportDesc.PublicExportHash);
						ExportByKeyMap.Add(Key, &ExportDesc);
					}
				}
			}

			ParallelFor(Packages.Num(), [&Packages](int32 Index)
			{
				FPackageDesc* PackageDesc = Packages[Index];
				for (FExportDesc& ExportDesc : PackageDesc->Exports)
				{
					if (ExportDesc.FullName.IsNone())
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(GenerateExportFullName);

						TArray<FExportDesc*> ExportStack;
						
						FExportDesc* Current = &ExportDesc;
						TStringBuilder<2048> FullNameBuilder;
						TCHAR NameBuffer[FName::StringBufferSize];
						for (;;)
						{
							if (!Current->FullName.IsNone())
							{
								Current->FullName.ToString(NameBuffer);
								FullNameBuilder.Append(NameBuffer);
								break;
							}
							ExportStack.Push(Current);
							if (Current->OuterIndex.IsNull())
							{
								PackageDesc->PackageName.ToString(NameBuffer);
								FullNameBuilder.Append(NameBuffer);
								break;
							}
							Current = &PackageDesc->Exports[Current->OuterIndex.Value()];
						}
						while (ExportStack.Num() > 0)
						{
							Current = ExportStack.Pop(EAllowShrinking::No);
							FullNameBuilder.Append(TEXT("."));
							Current->Name.ToString(NameBuffer);
							FullNameBuilder.Append(NameBuffer);
							Current->FullName = FName(FullNameBuilder);
						}
					}
				}
			}, EParallelForFlags::Unbalanced);

			for (FPackageDesc* PackageDesc : Packages)
			{
				for (FImportDesc& Import : PackageDesc->Imports)
				{
					if (!Import.GlobalImportIndex.IsNull())
					{
						if (Import.GlobalImportIndex.IsPackageImport())
						{
							FPublicExportKey Key = FPublicExportKey::FromPackageImport(Import.GlobalImportIndex, PackageDesc->ImportedPackageIds, PackageDesc->ImportedPublicExportHashes);
							Import.Export = ExportByKeyMap.FindRef(Key);
							if (!Import.Export)
							{
								UE_LOG(LogIoStore, Warning, TEXT("Missing import: 0x%llX in package 0x%llX '%s'"), Import.GlobalImportIndex.Value(), PackageDesc->PackageId.ValueForDebugging(), *PackageDesc->PackageName.ToString());
							}
							else
							{
								Import.Name = Import.Export->FullName;
							}
						}
						else
						{
							FScriptObjectDesc* ScriptObjectDesc = ScriptObjectByGlobalIdMap.Find(Import.GlobalImportIndex);
							if (ScriptObjectDesc)
							{
								Import.Name = ScriptObjectDesc->FullName;
							}
							else
							{
								UE_LOG(LogIoStore, Warning, TEXT("Missing Script Object for Import: 0x%llX in package 0x%llX '%s'"), Import.GlobalImportIndex.Value(), PackageDesc->PackageId.ValueForDebugging(), *PackageDesc->PackageName.ToString());
							}
						}
					}
				}
			}
		}

		return { 
			FContainerPackageInfo{
				MoveTemp(Containers),
				MoveTemp(Packages),
				MoveTemp(ScriptObjectByGlobalIdMap),
				MoveTemp(ExportByKeyMap),
			} 
		};
	}
}

int32 Describe(
	const FString& GlobalContainerPath,
	const FKeyChain& KeyChain,
	const FString& PackageFilter,
	const FString& OutPath,
	bool bIncludeExportHashes)
{
	using namespace DescribeUtils;
	TOptional<FContainerPackageInfo> MaybeInfo = DescribeUtils::TryGetContainerPackageInfo(GlobalContainerPath, KeyChain, bIncludeExportHashes);
	if (!MaybeInfo.IsSet())
	{
		return -1;
	}
	FContainerPackageInfo& Info = MaybeInfo.GetValue();

	const TArray<FContainerDesc*>& Containers = Info.Containers;
	const TArray<FPackageDesc*>& Packages = Info.Packages;
	const TMap<FPackageObjectIndex, FScriptObjectDesc>& ScriptObjectByGlobalIdMap = Info.ScriptObjectByGlobalIdMap;
	const TMap<FPublicExportKey, FExportDesc*>& ExportByKeyMap = Info.ExportByKeyMap;

	UE_LOG(LogIoStore, Display, TEXT("Collecting output packages..."));
	TArray<const FPackageDesc*> OutputPackages;
	TSet<FPackageId> RelevantPackages;
	TSet<FContainerDesc*> RelevantContainers;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CollectOutputPackages);

		if (PackageFilter.IsEmpty())
		{
			OutputPackages.Append(Packages);
		}
		else
		{
			TArray<FString> SplitPackageFilters;
			const TCHAR* Delimiters[] = { TEXT(","), TEXT(" ") };
			PackageFilter.ParseIntoArray(SplitPackageFilters, Delimiters, UE_ARRAY_COUNT(Delimiters), true);

			TArray<FString> PackageNameFilter;
			TSet<FPackageId> PackageIdFilter;
			for (const FString& PackageNameOrId : SplitPackageFilters)
			{
				if (PackageNameOrId.Len() > 0 && FChar::IsDigit(PackageNameOrId[0]))
				{
					uint64 Value;
					LexFromString(Value, *PackageNameOrId);
					PackageIdFilter.Add(*(FPackageId*)(&Value));
				}
				else
				{
					PackageNameFilter.Add(PackageNameOrId);
				}
			}

			TArray<const FPackageDesc*> PackageStack;
			for (const FPackageDesc* PackageDesc : Packages)
			{
				bool bInclude = false;
				if (PackageIdFilter.Contains(PackageDesc->PackageId))
				{
					bInclude = true;
				}
				else
				{
					FString PackageName = PackageDesc->PackageName.ToString();
					for (const FString& Wildcard : PackageNameFilter)
					{
						if (PackageName.MatchesWildcard(Wildcard))
						{
							bInclude = true;
							break;
						}
					}
				}
				if (bInclude)
				{
					PackageStack.Push(PackageDesc);
				}
			}
			TSet<const FPackageDesc*> Visited;
			while (PackageStack.Num() > 0)
			{
				const FPackageDesc* PackageDesc = PackageStack.Pop();
				if (!Visited.Contains(PackageDesc))
				{
					Visited.Add(PackageDesc);
					OutputPackages.Add(PackageDesc);
					RelevantPackages.Add(PackageDesc->PackageId);
					for (const FPackageLocation& Location : PackageDesc->Locations)
					{
						RelevantContainers.Add(Location.Container);
					}
					for (const FImportDesc& Import : PackageDesc->Imports)
					{
						if (Import.Export && Import.Export->Package)
						{
							PackageStack.Push(Import.Export->Package);
						}
					}
				}
			}
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Generating report..."));

	FOutputDevice* OutputOverride = GWarn;
	FString OutputFilename;
	TUniquePtr<FOutputDeviceFile> OutputBuffer;
	if (!OutPath.IsEmpty())
	{
		OutputBuffer = MakeUnique<FOutputDeviceFile>(*OutPath, true);
		OutputBuffer->SetSuppressEventTag(true);
		OutputOverride = OutputBuffer.Get();
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(GenerateReport);
		TGuardValue<ELogTimes::Type> GuardPrintLogTimes(GPrintLogTimes, ELogTimes::None);
		TGuardValue GuardPrintLogCategory(GPrintLogCategory, false);
		TGuardValue GuardPrintLogVerbosity(GPrintLogVerbosity, false);

		auto PackageObjectIndexToString = [&ScriptObjectByGlobalIdMap, &ExportByKeyMap](const FPackageDesc* Package, const FPackageObjectIndex& PackageObjectIndex, bool bIncludeName) -> FString
		{
			if (PackageObjectIndex.IsNull())
			{
				return TEXT("<null>");
			}
			else if (PackageObjectIndex.IsPackageImport())
			{
				FPublicExportKey Key = FPublicExportKey::FromPackageImport(PackageObjectIndex, Package->ImportedPackageIds, Package->ImportedPublicExportHashes);
				FExportDesc* ExportDesc = ExportByKeyMap.FindRef(Key);
				if (ExportDesc && bIncludeName)
				{
					return FString::Printf(TEXT("0x%llX '%s'"), PackageObjectIndex.Value(), *ExportDesc->FullName.ToString());
				}
				else
				{
					return FString::Printf(TEXT("0x%llX"), PackageObjectIndex.Value());
				}
			}
			else if (PackageObjectIndex.IsScriptImport())
			{
				const FScriptObjectDesc* ScriptObjectDesc = ScriptObjectByGlobalIdMap.Find(PackageObjectIndex);
				if (ScriptObjectDesc && bIncludeName)
				{
					return FString::Printf(TEXT("0x%llX '%s'"), PackageObjectIndex.Value(), *ScriptObjectDesc->FullName.ToString());
				}
				else
				{
					return FString::Printf(TEXT("0x%llX"), PackageObjectIndex.Value());
				}
			}
			else if (PackageObjectIndex.IsExport())
			{
				return FString::Printf(TEXT("%d"), PackageObjectIndex.Value());
			}
			else
			{
				return FString::Printf(TEXT("0x%llX"), PackageObjectIndex.Value());
			}
		};

		for (const FContainerDesc* ContainerDesc : Containers)
		{
			if (RelevantContainers.Num() > 0 && !RelevantContainers.Contains(ContainerDesc))
			{
				continue;
			}

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("********************************************"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Container '%s' Summary"), *ContainerDesc->Name.ToString());
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t      ContainerId: 0x%llX"), ContainerDesc->Id.Value());
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t       Compressed: %s"), ContainerDesc->bCompressed ? TEXT("Yes") : TEXT("No"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Signed: %s"), ContainerDesc->bSigned ? TEXT("Yes") : TEXT("No"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t          Indexed: %s"), ContainerDesc->bIndexed ? TEXT("Yes") : TEXT("No"));
			if (ContainerDesc->bEncrypted)
			{
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\tEncryptionKeyGuid: %s"), *ContainerDesc->EncryptionKeyGuid.ToString());
			}

			if (ContainerDesc->LocalizedPackages.Num())
			{
				bool bNeedsHeader = true;
				for (const FPackageDesc* LocalizedPackage : ContainerDesc->LocalizedPackages)
				{
					if (RelevantPackages.Num() > 0 && !RelevantPackages.Contains(LocalizedPackage->PackageId))
					{
						continue;
					}
					if (bNeedsHeader)
					{
						OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
						OutputOverride->Logf(ELogVerbosity::Display, TEXT("Localized Packages"));
						OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
						bNeedsHeader = false;
					}
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Source: 0x%llX '%s'"), LocalizedPackage->PackageId.ValueForDebugging(), *LocalizedPackage->PackageName.ToString());
				}
			}

			if (ContainerDesc->PackageRedirects.Num())
			{
				bool bNeedsHeader = true;
				for (const FPackageRedirect& Redirect : ContainerDesc->PackageRedirects)
				{
					if (RelevantPackages.Num() > 0 && !RelevantPackages.Contains(Redirect.Source->PackageId) && !RelevantPackages.Contains(Redirect.Target->PackageId))
					{
						continue;
					}
					if (bNeedsHeader)
					{
						OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
						OutputOverride->Logf(ELogVerbosity::Display, TEXT("Package Redirects"));
						OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
						bNeedsHeader = false;
					}
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Source: 0x%llX '%s'"), Redirect.Source->PackageId.ValueForDebugging(), *Redirect.Source->PackageName.ToString());
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Target: 0x%llX '%s'"), Redirect.Target->PackageId.ValueForDebugging(), *Redirect.Target->PackageName.ToString());
				}
			}
		}

		for (const FPackageDesc* PackageDesc : OutputPackages)
		{
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("********************************************"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Package '%s' Summary"), *PackageDesc->PackageName.ToString());
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t        PackageId: 0x%llX"), PackageDesc->PackageId.ValueForDebugging());
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t     PackageFlags: %X"), PackageDesc->PackageFlags);
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t        NameCount: %d"), PackageDesc->NameCount);
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t      ImportCount: %d"), PackageDesc->Imports.Num());
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t      ExportCount: %d"), PackageDesc->Exports.Num());

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Locations"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
			int32 Index = 0;
			for (const FPackageLocation& Location : PackageDesc->Locations)
			{
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\tLocation %d: '%s'"), Index++, *Location.Container->Name.ToString());
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Offset: %lld"), Location.Offset);
			}

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Imports"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
			Index = 0;
			for (const FImportDesc& Import : PackageDesc->Imports)
			{
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\tImport %d: '%s'"), Index++, *Import.Name.ToString());
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\tGlobalImportIndex: %s"), *PackageObjectIndexToString(PackageDesc, Import.GlobalImportIndex, false));
			}

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Exports"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
			Index = 0;
			for (const FExportDesc& Export : PackageDesc->Exports)
			{
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\tExport %d: '%s'"), Index++, *Export.Name.ToString());
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t       OuterIndex: %s"), *PackageObjectIndexToString(PackageDesc, Export.OuterIndex, true));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t       ClassIndex: %s"), *PackageObjectIndexToString(PackageDesc, Export.ClassIndex, true));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t       SuperIndex: %s"), *PackageObjectIndexToString(PackageDesc, Export.SuperIndex, true));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t    TemplateIndex: %s"), *PackageObjectIndexToString(PackageDesc, Export.TemplateIndex, true));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t PublicExportHash: %llu"), Export.PublicExportHash);
				if (bIncludeExportHashes)
				{
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t   ExportHash: %s"), *Export.ExportHash.ToString());
				}
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Offset: %lld"), Export.SerialOffset);
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t             Size: %lld"), Export.SerialSize);

			}

			OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Export Bundle"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
			for (const FExportBundleEntryDesc& ExportBundleEntry : PackageDesc->ExportBundleEntries)
			{
				if (ExportBundleEntry.CommandType == FExportBundleEntry::ExportCommandType_Create)
				{
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Create: %d '%s'"), ExportBundleEntry.LocalExportIndex, *ExportBundleEntry.Export->Name.ToString());
				}
				else
				{
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t        Serialize: %d '%s'"), ExportBundleEntry.LocalExportIndex, *ExportBundleEntry.Export->Name.ToString());
				}
			}
		}
	}


	return 0;
}

int32 ValidateCrossContainerRefs(
	const FString& GlobalContainerPath,
	const FKeyChain& KeyChain,
	const FString& ConfigPath,
	const FString& OutPath
	)
{
	TMultiMap<FString, FString> ValidEdges;
	TArray<FString> IgnoreRefsFromAssets, IgnoreRefsToAssets;

	FConfigFile ConfigFile;
	if (!FConfigCacheIni::LoadLocalIniFile(ConfigFile, *ConfigPath, false))
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to load config file %s"), *ConfigPath);
		return -1;
	}

	if (const FConfigSection* EdgesSection = ConfigFile.FindSection(TEXT("Edges")))
	{
		for (auto It = EdgesSection->CreateConstIterator(); It; ++It)
		{
			ValidEdges.Add(It.Key().ToString(), It.Value().GetValue());
		}
	}
	if (const FConfigSection* DefaultEdgesSection = ConfigFile.FindSection(TEXT("DefaultEdges")))
	{
		for (auto It = DefaultEdgesSection->CreateConstIterator(); It; ++It)
		{
			ValidEdges.Add(FString(), It.Key().ToString());
		}
	}
	if (const FConfigSection* IgnoreSection = ConfigFile.FindSection(TEXT("Ignore")))
	{
		IgnoreSection->MultiFind(TEXT("IgnoreRefsFrom"), IgnoreRefsFromAssets);
		IgnoreSection->MultiFind(TEXT("IgnoreRefsTo"), IgnoreRefsToAssets);
	}

	if (ValidEdges.Num() == 0)
	{
		UE_LOG(LogIoStore, Error, TEXT("No valid edges configured, nothing to validate"));
		return -1;
	}

	if (ValidEdges.FindPair(TEXT(""), TEXT("")))
	{
		UE_LOG(LogIoStore, Error, TEXT("Configuration contains all-to-all edge (empty string to empty string), nothing to validate"));
		return -1;
	}

	using namespace DescribeUtils;
	TOptional<FContainerPackageInfo> MaybeInfo = DescribeUtils::TryGetContainerPackageInfo(GlobalContainerPath, KeyChain, false);
	if (!MaybeInfo.IsSet())
	{
		return -1;
	}
	FContainerPackageInfo& Info = MaybeInfo.GetValue();

	const TArray<FContainerDesc*>& Containers = Info.Containers;
	TArray<FPackageDesc*>& Packages = Info.Packages;
	const TMap<FPackageObjectIndex, FScriptObjectDesc>& ScriptObjectByGlobalIdMap = Info.ScriptObjectByGlobalIdMap;
	const TMap<FPublicExportKey, FExportDesc*>& ExportByKeyMap = Info.ExportByKeyMap;

	// Expand container prefixes from config into full container names and produce transitive closure
	TMultiMap<const FContainerDesc*, const FContainerDesc*> FinalValidEdges;
	{
		TMultiMap<FString, const FContainerDesc* > ShortNameToContainer;
		for (auto It = ValidEdges.CreateIterator(); It; ++It)
		{
			for (const FContainerDesc* Desc : Containers)
			{
				// Empty strings mean 'all containers'
				if (It.Key().Len() == 0 || Desc->Name.ToString().StartsWith(It.Key()))
				{
					ShortNameToContainer.AddUnique(It.Key(), Desc);
				}
				if (It.Value().Len() == 0 || Desc->Name.ToString().StartsWith(It.Value()))
				{
					ShortNameToContainer.AddUnique(It.Value(), Desc);
				}
			}
		}
		TMultiMap<const FContainerDesc*, const FContainerDesc*> ValidDirectEdges;
		for (const TPair<FString, FString>& Pair : ValidEdges)
		{
			if (Pair.Key.Len() == 0) 
			{ 
				// Do 'all containers' later after handling explicit containers 
				continue;
			}
			for (auto FromIt = ShortNameToContainer.CreateKeyIterator(Pair.Key); FromIt; ++FromIt)
			{
				for (auto ToIt = ShortNameToContainer.CreateKeyIterator(Pair.Value); ToIt; ++ToIt)
				{
					ValidDirectEdges.AddUnique(FromIt.Value(), ToIt.Value());
				}
			}
		}
		
		TSet<const FContainerDesc*> UnassignedFromContainers;
		for (const FContainerDesc* Container : Containers) 
		{
			if (!ValidDirectEdges.Contains(Container)) 
			{
				UnassignedFromContainers.Add(Container);
			}
		}
		
		for (const TPair<FString, FString>& Pair : ValidEdges)
		{
			if (Pair.Key.Len() == 0) 
			{ 
				for (auto FromIt = ShortNameToContainer.CreateKeyIterator(Pair.Key); FromIt; ++FromIt)
				{
					if (!UnassignedFromContainers.Contains(FromIt.Value()))
					{
						continue;
					}

					for (auto ToIt = ShortNameToContainer.CreateKeyIterator(Pair.Value); ToIt; ++ToIt)
					{
						ValidDirectEdges.Add(FromIt.Value(), ToIt.Value());
					}
				}
			}
		}

		// Create a transitive closure (i.e. if it is valid for packages in container A to import packages in B, and B to import packages in C, then it is valid for packages in A to import packages in C)
		for (const FContainerDesc* StartContainer : Containers)
		{
			TSet<const FContainerDesc*> SeenContainers;
			TArray<const FContainerDesc*> Queue;
			Queue.Add(StartContainer);
			SeenContainers.Add(StartContainer);
			while (Queue.Num() > 0)
			{
				const FContainerDesc* ToContainer = Queue.Pop();
				FinalValidEdges.Add(StartContainer, ToContainer);
				for (auto ToIt = ValidDirectEdges.CreateKeyIterator(ToContainer); ToIt; ++ToIt)
				{
					if (!SeenContainers.Contains(ToIt.Value()))
					{
						SeenContainers.Add(ToIt.Value());
						Queue.Add(ToIt.Value());
					}
				}
			}
		}
	}

	TMap<const FContainerDesc*, TSet<TTuple<const FPackageDesc*, const FPackageDesc*>>> Errors;
	Algo::SortBy(Packages, [](FPackageDesc* Desc) { return Desc->PackageName; }, FNameLexicalLess());
	for (const FPackageDesc* Package : Packages)
	{
		bool bSkip = Algo::AnyOf(IgnoreRefsFromAssets, [Package](const FString& IgnoreString)
		{
			FString PackageNameString = Package->PackageName.ToString();
			if (PackageNameString == IgnoreString)
			{
				return true;
			}
			else if (FWildcardString::ContainsWildcards(*IgnoreString) && FWildcardString::IsMatch(*IgnoreString, *PackageNameString))
			{
				return true;
			}
			return false;
		});
		if (bSkip)
		{
			continue;
		}

		bool bNeedsHeader = true;
		for (const FImportDesc& Import : Package->Imports)
		{
			FPackageDesc* ImportPackage = Import.Export ? Import.Export->Package : nullptr;
			if (!ImportPackage) 
			{
				UE_CLOG(Import.Name != FName() && !FPackageName::IsScriptPackage(*Import.Name.ToString()),
					LogIoStore, Error, TEXT("Unresolved import of package %s by package %s"), *Import.Name.ToString(), *Package->PackageName.ToString());
				continue;
			}

			bSkip = Algo::AnyOf(IgnoreRefsFromAssets, [ImportPackage](const FString& IgnoreString)
			{
				FString PackageNameString = ImportPackage->PackageName.ToString();
				if (PackageNameString == IgnoreString)
				{
					return true;
				}
				else if (FWildcardString::ContainsWildcards(*IgnoreString) && FWildcardString::IsMatch(*IgnoreString, *PackageNameString))
				{
					return true;
				}
				return false;
			});
			if (bSkip)
			{
				continue;
			}

			// For each location of the importing paacka
			for (const DescribeUtils::FPackageLocation& Location : Package->Locations)
			{
				bool bValid = Algo::AnyOf(ImportPackage->Locations, [&](const DescribeUtils::FPackageLocation& ImportLocation) {
					return FinalValidEdges.FindPair(Location.Container, ImportLocation.Container) != nullptr;
				});
				if (!bValid)
				{
					Errors.FindOrAdd(Location.Container).Add({ Package, ImportPackage });
				}
			}
		}
	}
	FOutputDevice* OutputOverride = GWarn;
	FString OutputFilename;
	TUniquePtr<FOutputDeviceFile> OutputBuffer;
	if (!OutPath.IsEmpty())
	{
		OutputBuffer = MakeUnique<FOutputDeviceFile>(*OutPath, true);
		OutputBuffer->SetSuppressEventTag(true);
		OutputOverride = OutputBuffer.Get();
	}

	OutputOverride->Logf(ELogVerbosity::Display, TEXT("Invalid cross-container reference report"));
	OutputOverride->Logf(ELogVerbosity::Display, TEXT("Final valid edges: %d"), FinalValidEdges.Num());
	for (const TPair<const FContainerDesc*, const FContainerDesc*>& Pair : FinalValidEdges)
	{
		OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t%s -> %s"), *Pair.Key->Name.ToString(), *Pair.Value->Name.ToString());
	}

	if (Errors.Num() == 0)
	{
		OutputOverride->Logf(ELogVerbosity::Display, TEXT("No errors."));
		return 0;
	}

	for (const TPair<const FContainerDesc*, TSet<TTuple<const FPackageDesc*, const FPackageDesc*>>>& Pair : Errors)
	{
		OutputOverride->Logf(ELogVerbosity::Display, TEXT("%s"), *Pair.Key->Name.ToString());
		for (const TTuple<const FPackageDesc*, const FPackageDesc*>& Error : Pair.Value) 
		{
			FString LocationsString = FString::JoinBy(Error.Value->Locations, TEXT(","), [](const DescribeUtils::FPackageLocation& Location) { return Location.Container->Name.ToString(); });
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t%s -> %s (%s)"), *Error.Key->PackageName.ToString(), *Error.Value->PackageName.ToString(), *LocationsString);
		} 
	}
	return 0;
}

enum class EChunkTypeFilter
{
	None,
	PackageData,
	BulkData
};

FString LexToString(EChunkTypeFilter Filter)
{
	switch(Filter)
	{
		case EChunkTypeFilter::PackageData:
			return TEXT("PackageData");
		case EChunkTypeFilter::BulkData:
			return TEXT("BulkData");
		default:
			return TEXT("None");
	}
}

static int32 Diff(
	const FString& SourcePath,
	const FKeyChain& SourceKeyChain,
	const FString& TargetPath,
	const FKeyChain& TargetKeyChain,
	const FString& OutPath,
	EChunkTypeFilter ChunkTypeFilter)
{
	struct FContainerChunkInfo
	{
		FString ContainerName;
		TMap<FIoChunkId, FIoStoreTocChunkInfo> ChunkInfoById;
		int64 UncompressedContainerSize = 0;
		int64 CompressedContainerSize = 0;
	};

	struct FContainerDiff
	{
		TSet<FIoChunkId> Unmodified;
		TSet<FIoChunkId> Modified;
		TSet<FIoChunkId> Added;
		TSet<FIoChunkId> Removed;
		int64 UnmodifiedCompressedSize = 0;
		int64 ModifiedCompressedSize = 0;
		int64 AddedCompressedSize = 0;
		int64 RemovedCompressedSize = 0;
	};

	using FContainers = TMap<FString, FContainerChunkInfo>;

	auto ReadContainers = [ChunkTypeFilter](const FString& Directory, const FKeyChain& KeyChain, FContainers& OutContainers)
	{
		TArray<FString> ContainerFileNames;
		IFileManager::Get().FindFiles(ContainerFileNames, *(Directory / TEXT("*.utoc")), true, false);

		for (const FString& ContainerFileName : ContainerFileNames)
		{
			FString ContainerFilePath = Directory / ContainerFileName;
			UE_LOG(LogIoStore, Display, TEXT("Reading container '%s'"), *ContainerFilePath);

			TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, KeyChain);
			if (!Reader.IsValid())
			{
				UE_LOG(LogIoStore, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
				continue;
			}

			FString ContainerName = FPaths::GetBaseFilename(ContainerFileName);
			FContainerChunkInfo& ContainerChunkInfo = OutContainers.FindOrAdd(ContainerName);
			ContainerChunkInfo.ContainerName = MoveTemp(ContainerName);

			Reader->EnumerateChunks([&ContainerChunkInfo, ChunkTypeFilter](const FIoStoreTocChunkInfo& ChunkInfo)
			{
				const EIoChunkType ChunkType = ChunkInfo.Id.GetChunkType();

				const bool bCompareChunk =
					ChunkTypeFilter == EChunkTypeFilter::None
					|| (ChunkTypeFilter == EChunkTypeFilter::PackageData
							&& ChunkType == EIoChunkType::ExportBundleData)
					|| (ChunkTypeFilter == EChunkTypeFilter::BulkData
							&& (ChunkType == EIoChunkType::BulkData || ChunkType == EIoChunkType::OptionalBulkData || ChunkType == EIoChunkType::MemoryMappedBulkData));

				if (bCompareChunk)
				{
					ContainerChunkInfo.ChunkInfoById.Add(ChunkInfo.Id, ChunkInfo);
					ContainerChunkInfo.UncompressedContainerSize += ChunkInfo.Size;
					ContainerChunkInfo.CompressedContainerSize += ChunkInfo.CompressedSize;
				}

				return true;
			});
		}
	};

	auto ComputeDiff = [](const FContainerChunkInfo& SourceContainer, const FContainerChunkInfo& TargetContainer) -> FContainerDiff 
	{
		check(SourceContainer.ContainerName == TargetContainer.ContainerName);

		FContainerDiff ContainerDiff;

		for (const auto& TargetChunkInfo : TargetContainer.ChunkInfoById)
		{
			if (const FIoStoreTocChunkInfo* SourceChunkInfo = SourceContainer.ChunkInfoById.Find(TargetChunkInfo.Key))
			{
				if (SourceChunkInfo->Hash != TargetChunkInfo.Value.Hash)
				{
					ContainerDiff.Modified.Add(TargetChunkInfo.Key);
					ContainerDiff.ModifiedCompressedSize += TargetChunkInfo.Value.CompressedSize;
				}
				else
				{
					ContainerDiff.Unmodified.Add(TargetChunkInfo.Key);
					ContainerDiff.UnmodifiedCompressedSize += TargetChunkInfo.Value.CompressedSize;
				}
			}
			else
			{
				ContainerDiff.Added.Add(TargetChunkInfo.Key);
				ContainerDiff.AddedCompressedSize += TargetChunkInfo.Value.CompressedSize;
			}
		}

		for (const auto& SourceChunkInfo : SourceContainer.ChunkInfoById)
		{
			if (!TargetContainer.ChunkInfoById.Contains(SourceChunkInfo.Key))
			{
				ContainerDiff.Removed.Add(SourceChunkInfo.Key);
				ContainerDiff.RemovedCompressedSize += SourceChunkInfo.Value.CompressedSize;
			}
		}

		return MoveTemp(ContainerDiff);
	};

	FOutputDevice* OutputDevice = GWarn;
	TUniquePtr<FOutputDeviceFile> FileOutputDevice;

	if (!OutPath.IsEmpty())
	{
		UE_LOG(LogIoStore, Error, TEXT("Redirecting output to: '%s'"), *OutPath);

		FileOutputDevice = MakeUnique<FOutputDeviceFile>(*OutPath, true);
		FileOutputDevice->SetSuppressEventTag(true);
		OutputDevice = FileOutputDevice.Get();
	}

	FContainers SourceContainers, TargetContainers;
	TArray<FString> AddedContainers, ModifiedContainers, RemovedContainers;
	TArray<FContainerDiff> ContainerDiffs;

	UE_LOG(LogIoStore, Display, TEXT("Reading source container(s) from '%s':"), *SourcePath);
	ReadContainers(SourcePath, SourceKeyChain, SourceContainers);

	if (!SourceContainers.Num())
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to read source container(s) from '%s':"), *SourcePath);
		return -1;
	}

	UE_LOG(LogIoStore, Display, TEXT("Reading target container(s) from '%s':"), *TargetPath);
	ReadContainers(TargetPath, TargetKeyChain, TargetContainers);

	if (!TargetContainers.Num())
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to read target container(s) from '%s':"), *SourcePath);
		return -1;
	}

	for (const auto& TargetContainer : TargetContainers)
	{
		if (SourceContainers.Contains(TargetContainer.Key))
		{
			ModifiedContainers.Add(TargetContainer.Key);
		}
		else
		{
			AddedContainers.Add(TargetContainer.Key);
		}
	}

	for (const auto& SourceContainer : SourceContainers)
	{
		if (!TargetContainers.Contains(SourceContainer.Key))
		{
			RemovedContainers.Add(SourceContainer.Key);
		}
	}

	for (const FString& ModifiedContainer : ModifiedContainers)
	{
		ContainerDiffs.Emplace(ComputeDiff(*SourceContainers.Find(ModifiedContainer), *TargetContainers.Find(ModifiedContainer)));
	}

	OutputDevice->Logf(ELogVerbosity::Display, TEXT(""));
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("------------------------------ Container Diff Summary ------------------------------"));
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("Source path '%s'"), *SourcePath);
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("Target path '%s'"), *TargetPath);
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("Chunk type filter '%s'"), *LexToString(ChunkTypeFilter));

	OutputDevice->Logf(ELogVerbosity::Display, TEXT(""));
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("Source container file(s):"));
	OutputDevice->Logf(ELogVerbosity::Display, TEXT(""));
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("%-40s %15s %15s"), TEXT("Container"), TEXT("Size (MB)"), TEXT("Chunks"));
	OutputDevice->Logf(ELogVerbosity::Display, TEXT("-------------------------------------------------------------------------"));

	{
		uint64 TotalSourceBytes = 0;
		uint64 TotalSourceChunks = 0;

		for (const auto& NameContainerPair : SourceContainers)
		{
			const FContainerChunkInfo& SourceContainer = NameContainerPair.Value;
			OutputDevice->Logf(ELogVerbosity::Display, TEXT("%-40s %15.2lf %15d"), *SourceContainer.ContainerName, double(SourceContainer.CompressedContainerSize) / 1024.0 / 1024.0, SourceContainer.ChunkInfoById.Num());

			TotalSourceBytes += SourceContainer.CompressedContainerSize;
			TotalSourceChunks += SourceContainer.ChunkInfoById.Num();
		}

		OutputDevice->Logf(ELogVerbosity::Display, TEXT("-------------------------------------------------------------------------"));
		OutputDevice->Logf(ELogVerbosity::Display, TEXT("%-40s %15.2lf %15d"), *FString::Printf(TEXT("Total of %d container file(s)"), SourceContainers.Num()), double(TotalSourceBytes) / 1024.0 / 1024.0, TotalSourceChunks);
	}

	{
		uint64 TotalTargetBytes = 0;
		uint64 TotalTargetChunks = 0;
		uint64 TotalUnmodifiedChunks = 0;
		uint64 TotalUnmodifiedCompressedBytes = 0;
		uint64 TotalModifiedChunks = 0;
		uint64 TotalModifiedCompressedBytes = 0;
		uint64 TotalAddedChunks = 0;
		uint64 TotalAddedCompressedBytes = 0;
		uint64 TotalRemovedChunks = 0;
		uint64 TotalRemovedCompressedBytes = 0;

		if (ModifiedContainers.Num())
		{
			OutputDevice->Logf(ELogVerbosity::Display, TEXT(""));
			OutputDevice->Logf(ELogVerbosity::Display, TEXT("Target container file(s):"));
			OutputDevice->Logf(ELogVerbosity::Display, TEXT(""));
			OutputDevice->Logf(ELogVerbosity::Display, TEXT("%-40s %15s %15s %25s %25s %25s %25s %25s %25s %25s %25s"), TEXT("Container"), TEXT("Size (MB)"), TEXT("Chunks"), TEXT("Unmodified"), TEXT("Unmodified (MB)"), TEXT("Modified"), TEXT("Modified (MB)"), TEXT("Added"), TEXT("Added (MB)"), TEXT("Removed"), TEXT("Removed (MB)"));
			OutputDevice->Logf(ELogVerbosity::Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"));

			for (int32 Idx = 0; Idx < ModifiedContainers.Num(); Idx++)
			{
				const FContainerChunkInfo& SourceContainer = *SourceContainers.Find(ModifiedContainers[Idx]);
				const FContainerChunkInfo& TargetContainer = *TargetContainers.Find(ModifiedContainers[Idx]);
				const FContainerDiff& Diff = ContainerDiffs[Idx];

				const int32 NumChunks = TargetContainer.ChunkInfoById.Num();
				const int32 NumSourceChunks = SourceContainer.ChunkInfoById.Num();

				OutputDevice->Logf(ELogVerbosity::Display, TEXT("%-40s %15s %15d %25s %25s %25s %25s %25s %25s %25s %25s"),
					*TargetContainer.ContainerName,
					*FString::Printf(TEXT("%.2lf"),
						double(TargetContainer.CompressedContainerSize) / 1024.0 / 1024.0),
					NumChunks,
					*FString::Printf(TEXT("%d (%.2lf%%)"),
						Diff.Unmodified.Num(),
						100.0 * (double(Diff.Unmodified.Num()) / double(NumChunks))),
					*FString::Printf(TEXT("%.2lf (%.2lf%%)"),
						double(Diff.UnmodifiedCompressedSize) / 1024.0 / 1024.0,
						100.0 * (Diff.UnmodifiedCompressedSize) / double(TargetContainer.CompressedContainerSize)),
					*FString::Printf(TEXT("%d (%.2lf%%)"),
						Diff.Modified.Num(),
						100.0 * (double(Diff.Modified.Num()) / double(NumChunks))),
					*FString::Printf(TEXT("%.2lf (%.2lf%%)"),
						double(Diff.ModifiedCompressedSize) / 1024.0 / 1024.0,
						100.0 * (Diff.ModifiedCompressedSize) / double(TargetContainer.CompressedContainerSize)),
					*FString::Printf(TEXT("%d (%.2lf%%)"),
						Diff.Added.Num(),
						100.0 * (double(Diff.Added.Num()) / double(NumChunks))),
					*FString::Printf(TEXT("%.2lf (%.2lf%%)"),
						double(Diff.AddedCompressedSize) / 1024.0 / 1024.0,
						100.0 * (Diff.AddedCompressedSize) / double(TargetContainer.CompressedContainerSize)),
					*FString::Printf(TEXT("%d/%d (%.2lf%%)"),
						Diff.Removed.Num(),
						NumSourceChunks,
						100.0 * (double(Diff.Removed.Num()) / double(NumSourceChunks))),
					*FString::Printf(TEXT("%.2lf (%.2lf%%)"),
						double(Diff.RemovedCompressedSize) / 1024.0 / 1024.0,
						100.0 * (Diff.RemovedCompressedSize) / double(SourceContainer.CompressedContainerSize)));

				TotalTargetBytes += TargetContainer.CompressedContainerSize;
				TotalTargetChunks += NumChunks;
				TotalUnmodifiedChunks += Diff.Unmodified.Num();
				TotalUnmodifiedCompressedBytes += Diff.UnmodifiedCompressedSize;
				TotalModifiedChunks += Diff.Modified.Num();
				TotalModifiedCompressedBytes += Diff.ModifiedCompressedSize;
				TotalAddedChunks += Diff.Added.Num();
				TotalAddedCompressedBytes += Diff.AddedCompressedSize;
				TotalRemovedChunks += Diff.Removed.Num();
				TotalRemovedCompressedBytes += Diff.RemovedCompressedSize;
			}
		}

		if (AddedContainers.Num())
		{
			for (const FString& AddedContainer : AddedContainers)
			{
				const FContainerChunkInfo& TargetContainer = *TargetContainers.Find(AddedContainer);
				OutputDevice->Logf(ELogVerbosity::Display, TEXT("+%-39s %15.2lf %15d %25s %25s %25s %25s %25s %25s %25s %25s"),
					*TargetContainer.ContainerName,
					double(TargetContainer.CompressedContainerSize) / 1024.0 / 1024.0,
					TargetContainer.ChunkInfoById.Num(),
					TEXT("-"), TEXT("-"), TEXT("-"), TEXT("-"), TEXT("-"), TEXT("-"), TEXT("-"), TEXT("-"));

				TotalTargetBytes += TargetContainer.CompressedContainerSize;
				TotalTargetChunks += TargetContainer.ChunkInfoById.Num();
			}
		}

		OutputDevice->Logf(ELogVerbosity::Display, TEXT("----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------"));
		OutputDevice->Logf(ELogVerbosity::Display, TEXT("%-40s %15.2lf %15d %25d %25.2f %25d %25.2f %25d %25.2f %25d %25.2f"),
			*FString::Printf(TEXT("Total of %d container file(s)"), TargetContainers.Num()),
			double(TotalTargetBytes) / 1024.0 / 1024.0,
			TotalTargetChunks,
			TotalUnmodifiedChunks,
			double(TotalUnmodifiedCompressedBytes) / 1024.0 / 1024.0,
			TotalModifiedChunks,
			double(TotalModifiedCompressedBytes) / 1024.0 / 1024.0,
			TotalAddedChunks,
			double(TotalAddedCompressedBytes) / 1024.0 / 1024.0,
			TotalRemovedChunks,
			double(TotalRemovedCompressedBytes) / 1024.0 / 1024.0);
	}

	return 0;
}

bool LegacyDiffIoStoreContainers(const TCHAR* InContainerFilename1, const TCHAR* InContainerFilename2, bool bInLogUniques1, bool bInLogUniques2, const FKeyChain& InKeyChain1, const FKeyChain* InKeyChain2)
{
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
	UE_LOG(LogIoStore, Log, TEXT("FileEventType, FileName, Size1, Size2"));

	TUniquePtr<FIoStoreReader> Reader1 = CreateIoStoreReader(InContainerFilename1, InKeyChain1);
	if (!Reader1.IsValid())
	{
		return false;
	}

	if (!EnumHasAnyFlags(Reader1->GetContainerFlags(), EIoContainerFlags::Indexed))
	{
		UE_LOG(LogIoStore, Warning, TEXT("Missing directory index for container '%s'"), InContainerFilename1);
	}

	TUniquePtr<FIoStoreReader> Reader2 = CreateIoStoreReader(InContainerFilename2, InKeyChain2 ? *InKeyChain2 : InKeyChain1);
	if (!Reader2.IsValid())
	{
		return false;
	}

	if (!EnumHasAnyFlags(Reader2->GetContainerFlags(), EIoContainerFlags::Indexed))
	{
		UE_LOG(LogIoStore, Warning, TEXT("Missing directory index for container '%s'"), InContainerFilename2);
	}

	struct FEntry
	{
		FString FileName;
		FIoChunkHash Hash;
		uint64 Size;
	};

	TMap<FIoChunkId, FEntry> Container1Entries;
	Reader1->EnumerateChunks([&Container1Entries](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			FEntry& Entry = Container1Entries.Add(ChunkInfo.Id);
			Entry.FileName = ChunkInfo.FileName;
			Entry.Hash = ChunkInfo.Hash;
			Entry.Size = ChunkInfo.Size;
			return true;
		});

	int32 NumDifferentContents = 0;
	int32 NumEqualContents = 0;
	int32 NumUniqueContainer1 = 0;
	int32 NumUniqueContainer2 = 0;
	Reader2->EnumerateChunks([&Container1Entries, &NumDifferentContents, &NumEqualContents, bInLogUniques2, &NumUniqueContainer2](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			const FEntry* FindContainer1Entry = Container1Entries.Find(ChunkInfo.Id);
			if (FindContainer1Entry)
			{
				if (FindContainer1Entry->Size != ChunkInfo.Size)
				{
					UE_LOG(LogIoStore, Log, TEXT("FilesizeDifferent, %s, %llu, %llu"), *ChunkInfo.FileName, FindContainer1Entry->Size, ChunkInfo.Size);
					++NumDifferentContents;
				}
				else if (FindContainer1Entry->Hash != ChunkInfo.Hash)
				{
					UE_LOG(LogIoStore, Log, TEXT("ContentsDifferent, %s, %llu, %llu"), *ChunkInfo.FileName, FindContainer1Entry->Size, ChunkInfo.Size);
					++NumDifferentContents;
				}
				else
				{
					++NumEqualContents;
				}
				Container1Entries.Remove(ChunkInfo.Id);
			}
			else
			{
				++NumUniqueContainer2;
				if (bInLogUniques2)
				{
					UE_LOG(LogIoStore, Log, TEXT("UniqueToSecondContainer, %s, 0, %llu"), *ChunkInfo.FileName, ChunkInfo.Size);
				}
			}
			return true;
		});

	for (const auto& KV : Container1Entries)
	{
		const FEntry& Entry = KV.Value;
		++NumUniqueContainer1;
		if (bInLogUniques1)
		{
			UE_LOG(LogIoStore, Log, TEXT("UniqueToFirstContainer, %s, %llu, 0"), *Entry.FileName, Entry.Size);
		}
	}

	UE_LOG(LogIoStore, Log, TEXT("Comparison complete"));
	UE_LOG(LogIoStore, Log, TEXT("Unique to first container: %d, Unique to second container: %d, Num Different: %d, NumEqual: %d"), NumUniqueContainer1, NumUniqueContainer2, NumDifferentContents, NumEqualContents);
	return true;
}

int32 Staged2Zen(const FString& BuildPath, const FKeyChain& KeyChain, const FString& ProjectName, const ITargetPlatform* TargetPlatform)
{
	FString PlatformName = TargetPlatform->PlatformName();
	FString CookedOutputPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), PlatformName);
	if (IFileManager::Get().DirectoryExists(*CookedOutputPath))
	{
		UE_LOG(LogIoStore, Error, TEXT("'%s' already exists"), *CookedOutputPath);
		return -1;
	}

	TArray<FString> ContainerFiles;
	IFileManager::Get().FindFilesRecursive(ContainerFiles, *BuildPath, TEXT("*.utoc"), true, false);
	if (ContainerFiles.IsEmpty())
	{
		UE_LOG(LogIoStore, Error, TEXT("No container files found"));
		return -1;
	}

	TArray<FString> PakFiles;
	IFileManager::Get().FindFilesRecursive(PakFiles, *BuildPath, TEXT("*.pak"), true, false);
	if (PakFiles.IsEmpty())
	{
		UE_LOG(LogIoStore, Error, TEXT("No pak files found"));
		return -1;
	}

	UE_LOG(LogIoStore, Display, TEXT("Extracting files from paks..."));
	FPakPlatformFile PakPlatformFile;
	for (const auto& KV : KeyChain.GetEncryptionKeys())
	{
		FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(KV.Key, KV.Value.Key);
	}
	PakPlatformFile.Initialize(&FPlatformFileManager::Get().GetPlatformFile(), TEXT(""));
	FString CookedEngineContentPath = FPaths::Combine(CookedOutputPath, TEXT("Engine"), TEXT("Content"));
	IFileManager::Get().MakeDirectory(*CookedEngineContentPath, true);
	FString CookedProjectContentPath = FPaths::Combine(CookedOutputPath, ProjectName, TEXT("Content"));
	IFileManager::Get().MakeDirectory(*CookedProjectContentPath, true);
	FString EngineContentPakPath = TEXT("../../../Engine/Content/");
	FString ProjectContentPakPath = FPaths::Combine(TEXT("../../.."), ProjectName, TEXT("Content"));
	for (const FString& PakFilePath : PakFiles)
	{
		PakPlatformFile.Mount(*PakFilePath, 0);
		TArray<FString> FilesInPak;
		PakPlatformFile.GetPrunedFilenamesInPakFile(PakFilePath, FilesInPak);
		for (const FString& FileInPak : FilesInPak)
		{
			FString FileName = FPaths::GetCleanFilename(FileInPak);
			if (FileName == TEXT("AssetRegistry.bin"))
			{
				FString TargetPath = FPaths::Combine(CookedOutputPath, ProjectName, FileName);
				PakPlatformFile.CopyFile(*TargetPath, *FileInPak);
			}
			else if (FileName.EndsWith(TEXT(".ushaderbytecode")))
			{
				FString TargetPath = FPaths::Combine(CookedProjectContentPath, FileName);
				PakPlatformFile.CopyFile(*TargetPath, *FileInPak);
			}
			else if (FileName.StartsWith("GlobalShaderCache"))
			{
				FString TargetPath = FPaths::Combine(CookedOutputPath, TEXT("Engine"), FileName);
				PakPlatformFile.CopyFile(*TargetPath, *FileInPak);
			}
			else if (FileName.EndsWith(TEXT(".ufont")))
			{
				FString TargetPath;
				if (FileInPak.StartsWith(EngineContentPakPath))
				{
					TargetPath = FPaths::Combine(CookedEngineContentPath, *FileInPak + EngineContentPakPath.Len());
				}
				else if (FileInPak.StartsWith(ProjectContentPakPath))
				{
					TargetPath = FPaths::Combine(CookedProjectContentPath, *FileInPak + ProjectContentPakPath.Len());
				}
				else
				{
					UE_DEBUG_BREAK();
					continue;
				}
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(TargetPath), true);
				PakPlatformFile.CopyFile(*TargetPath, *FileInPak);
			}
		}
	}

	struct FBulkDataInfo
	{
		FString FileName;
		IPackageWriter::FBulkDataInfo::EType BulkDataType;
		TTuple<FIoStoreReader*, FIoChunkId> Chunk;
	};

	struct FPackageInfo
	{
		FName PackageName;
		FString FileName;
		TTuple<FIoStoreReader*, FIoChunkId> Chunk;
		TArray<FBulkDataInfo> BulkData;
		FPackageStoreEntryResource PackageStoreEntry;
	};

	struct FCollectedData
	{
		TSet<FIoChunkId> SeenChunks;
		TMap<FName, FPackageInfo> Packages;
		TMap<FPackageId, FName> PackageIdToName;
		TArray<TTuple<FIoStoreReader*, FIoChunkId>> ContainerHeaderChunks;
	} CollectedData;

	UE_LOG(LogIoStore, Display, TEXT("Collecting chunks..."));
	TArray<TUniquePtr<FIoStoreReader>> IoStoreReaders;
	IoStoreReaders.Reserve(ContainerFiles.Num());
	for (const FString& ContainerFilePath : ContainerFiles)
	{
		TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, KeyChain);
		if (!Reader.IsValid())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
			continue;
		}

		
		Reader->EnumerateChunks([&Reader, &CollectedData](const FIoStoreTocChunkInfo& ChunkInfo)
			{
				if (CollectedData.SeenChunks.Contains(ChunkInfo.Id))
				{
					return true;
				}
				CollectedData.SeenChunks.Add(ChunkInfo.Id);
				EIoChunkType ChunkType = static_cast<EIoChunkType>(ChunkInfo.Id.GetData()[11]);
				if (ChunkType == EIoChunkType::ExportBundleData ||
					ChunkType == EIoChunkType::BulkData ||
					ChunkType == EIoChunkType::OptionalBulkData ||
					ChunkType == EIoChunkType::MemoryMappedBulkData)
				{
					FString PackageNameStr;
					UE_CLOG(!ChunkInfo.bHasValidFileName, LogIoStore, Fatal, TEXT("Missing file name for package chunk"));
					if (FPackageName::TryConvertFilenameToLongPackageName(ChunkInfo.FileName, PackageNameStr, nullptr))
					{
						FName PackageName(PackageNameStr);
						CollectedData.PackageIdToName.Add(FPackageId::FromName(PackageName), PackageName);
						FPackageInfo& PackageInfo = CollectedData.Packages.FindOrAdd(PackageName);
						if (ChunkType == EIoChunkType::ExportBundleData)
						{
							PackageInfo.FileName = ChunkInfo.FileName;
							PackageInfo.PackageName = PackageName;
							PackageInfo.Chunk = MakeTuple(Reader.Get(), ChunkInfo.Id);
						}
						else
						{
							FBulkDataInfo& BulkDataInfo = PackageInfo.BulkData.AddDefaulted_GetRef();
							BulkDataInfo.FileName = ChunkInfo.FileName;
							BulkDataInfo.Chunk = MakeTuple(Reader.Get(), ChunkInfo.Id);
							if (ChunkType == EIoChunkType::OptionalBulkData)
							{
								BulkDataInfo.BulkDataType = IPackageWriter::FBulkDataInfo::Optional;
							}
							else if (ChunkType == EIoChunkType::MemoryMappedBulkData)
							{
								BulkDataInfo.BulkDataType = IPackageWriter::FBulkDataInfo::Mmap;
							}
							else
							{
								BulkDataInfo.BulkDataType = IPackageWriter::FBulkDataInfo::BulkSegment;
							}
						}
					}
					else
					{
						UE_LOG(LogIoStore, Warning, TEXT("Failed to convert file name '%s' to package name"), *ChunkInfo.FileName);
					}
				}
				else if (ChunkType == EIoChunkType::ContainerHeader)
				{
					CollectedData.ContainerHeaderChunks.Emplace(Reader.Get(), ChunkInfo.Id);
				}
				return true;
			});

		IoStoreReaders.Emplace(MoveTemp(Reader));
	}

	UE_LOG(LogIoStore, Display, TEXT("Reading container headers..."));
	for (const auto& ContainerHeaderChunk : CollectedData.ContainerHeaderChunks)
	{
		FIoBuffer ContainerHeaderBuffer = ContainerHeaderChunk.Key->Read(ContainerHeaderChunk.Value, FIoReadOptions()).ValueOrDie();
		FMemoryReaderView Ar(MakeArrayView(ContainerHeaderBuffer.Data(), ContainerHeaderBuffer.DataSize()));
		FIoContainerHeader ContainerHeader;
		Ar << ContainerHeader;
		
		const FFilePackageStoreEntry* StoreEntry = reinterpret_cast<const FFilePackageStoreEntry*>(ContainerHeader.StoreEntries.GetData());
		for (const FPackageId& PackageId : ContainerHeader.PackageIds)
		{
			const FName* FindPackageName = CollectedData.PackageIdToName.Find(PackageId);
			if (FindPackageName)
			{
				FPackageInfo* FindPackageInfo = CollectedData.Packages.Find(*FindPackageName);
				check(FindPackageInfo);
				
				FPackageStoreEntryResource& PackageStoreEntryResource = FindPackageInfo->PackageStoreEntry;
				PackageStoreEntryResource.PackageName = *FindPackageName;
				PackageStoreEntryResource.ImportedPackageIds.SetNum(StoreEntry->ImportedPackages.Num());
				FMemory::Memcpy(PackageStoreEntryResource.ImportedPackageIds.GetData(), StoreEntry->ImportedPackages.Data(), sizeof(FPackageId) * StoreEntry->ImportedPackages.Num()); //-V575
			}
			++StoreEntry;
		}
	}

	FString MetaDataOutputPath = FPaths::Combine(CookedOutputPath, ProjectName, TEXT("Metadata"));
	TUniquePtr<FZenStoreWriter> ZenStoreWriter = MakeUnique<FZenStoreWriter>(CookedOutputPath, MetaDataOutputPath, TargetPlatform);
	
	ICookedPackageWriter::FCookInfo CookInfo;
	CookInfo.bFullBuild = true;
	ZenStoreWriter->Initialize(CookInfo);
	ZenStoreWriter->BeginCook(CookInfo);
	int32 LocalPackageIndex = 0;
	TArray<FPackageInfo> PackagesArray;
	CollectedData.Packages.GenerateValueArray(PackagesArray);
	TAtomic<int32> UploadCount { 0 };
	ParallelFor(PackagesArray.Num(), [&UploadCount, &PackagesArray, &ZenStoreWriter](int32 Index)
	{
		const FPackageInfo& PackageInfo = PackagesArray[Index];

		IPackageWriter::FBeginPackageInfo BeginPackageInfo;
		BeginPackageInfo.PackageName = PackageInfo.PackageName;

		ZenStoreWriter->BeginPackage(BeginPackageInfo);

		IPackageWriter::FPackageInfo PackageStorePackageInfo;
		PackageStorePackageInfo.PackageName = PackageInfo.PackageName;
		PackageStorePackageInfo.LooseFilePath = PackageInfo.FileName;
		PackageStorePackageInfo.ChunkId = PackageInfo.Chunk.Value;

		FIoBuffer PackageDataBuffer = PackageInfo.Chunk.Key->Read(PackageInfo.Chunk.Value, FIoReadOptions()).ValueOrDie();
		ZenStoreWriter->WriteIoStorePackageData(PackageStorePackageInfo, PackageDataBuffer, PackageInfo.PackageStoreEntry, TArray<FFileRegion>());

		for (const FBulkDataInfo& BulkDataInfo : PackageInfo.BulkData)
		{
			IPackageWriter::FBulkDataInfo PackageStoreBulkDataInfo;
			PackageStoreBulkDataInfo.PackageName = PackageInfo.PackageName;
			PackageStoreBulkDataInfo.LooseFilePath = BulkDataInfo.FileName;
			PackageStoreBulkDataInfo.ChunkId = BulkDataInfo.Chunk.Value;
			PackageStoreBulkDataInfo.BulkDataType = BulkDataInfo.BulkDataType;
			FIoBuffer BulkDataBuffer = BulkDataInfo.Chunk.Key->Read(BulkDataInfo.Chunk.Value, FIoReadOptions()).ValueOrDie();
			ZenStoreWriter->WriteBulkData(PackageStoreBulkDataInfo, BulkDataBuffer, TArray<FFileRegion>());
		}
		
		IPackageWriter::FCommitPackageInfo CommitInfo;
		CommitInfo.PackageName = PackageInfo.PackageName;
		CommitInfo.WriteOptions = IPackageWriter::EWriteOptions::Write;
		CommitInfo.Status = IPackageWriter::ECommitStatus::Success;
		ZenStoreWriter->CommitPackage(MoveTemp(CommitInfo));

		int32 LocalUploadCount = UploadCount.IncrementExchange() + 1;
		UE_CLOG(LocalUploadCount % 1000 == 0, LogIoStore, Display, TEXT("Uploading package %d/%d"), LocalUploadCount, PackagesArray.Num());
	}, EParallelForFlags::ForceSingleThread); // Single threaded for now to limit memory usage

	UE_LOG(LogIoStore, Display, TEXT("Waiting for uploads to finish..."));
	ZenStoreWriter->EndCook(CookInfo);
	return 0;
}

int32 GenerateZenFileSystemManifest(ITargetPlatform* TargetPlatform)
{
	FString OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
	OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	FPaths::NormalizeDirectoryName(OutputDirectory);
	TUniquePtr<FSandboxPlatformFile> LocalSandboxFile = FSandboxPlatformFile::Create(false);
	LocalSandboxFile->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));
	const FString RootPathSandbox = LocalSandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*FPaths::RootDir());
	FString MetadataPathSandbox = LocalSandboxFile->ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::ProjectDir() / TEXT("Metadata")));
	const FString PlatformString = TargetPlatform->PlatformName();
	const FString ResolvedRootPath = RootPathSandbox.Replace(TEXT("[Platform]"), *PlatformString);
	const FString ResolvedMetadataPath = MetadataPathSandbox.Replace(TEXT("[Platform]"), *PlatformString);

	FZenFileSystemManifest ZenFileSystemManifest(*TargetPlatform, ResolvedRootPath);
	ZenFileSystemManifest.Generate();
	ZenFileSystemManifest.Save(*FPaths::Combine(ResolvedMetadataPath, TEXT("zenfs.manifest")));
	return 0;
}

bool ExtractFilesWriter(const FString& SrcFileName, const FString& DestFileName, const FIoChunkId& ChunkId, const uint8* Data, uint64 DataSize)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WriteFile);
	TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*DestFileName));
	if (FileHandle)
	{
		FileHandle->Serialize(const_cast<uint8*>(Data), DataSize);
		UE_CLOG(FileHandle->IsError(), LogIoStore, Error, TEXT("Failed writing to file \"%s\"."), *DestFileName);
		return !FileHandle->IsError();
	}
	else
	{
		UE_LOG(LogIoStore, Error, TEXT("Unable to create file \"%s\"."), *DestFileName);
		return false;
	}
};

bool ExtractFilesFromIoStoreContainer(
	const TCHAR* InContainerFilename,
	const TCHAR* InDestPath,
	const FKeyChain& InKeyChain,
	const FString* InFilter,
	TMap<FString, uint64>* OutOrderMap,
	TArray<FGuid>* OutUsedEncryptionKeys,
	bool* bOutIsSigned)
{
	return ProcessFilesFromIoStoreContainer(InContainerFilename, InDestPath, InKeyChain, InFilter, ExtractFilesWriter, OutOrderMap, OutUsedEncryptionKeys, bOutIsSigned, -1);
}

bool ProcessFilesFromIoStoreContainer(
	const TCHAR* InContainerFilename,
	const TCHAR* InDestPath,
	const FKeyChain& InKeyChain,
	const FString* InFilter,
	TFunction<bool(const FString&, const FString&, const FIoChunkId&, const uint8*, uint64)> FileProcessFunc,
	TMap<FString, uint64>* OutOrderMap,
	TArray<FGuid>* OutUsedEncryptionKeys,
	bool* bOutIsSigned,
	int32 MaxConcurrentReaders)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ExtractFilesFromIoStoreContainer);

	TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(InContainerFilename, InKeyChain);
	if (!Reader.IsValid())
	{
		return false;
	}

	if (!EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Indexed))
	{
		UE_LOG(LogIoStore, Error, TEXT("Missing directory index for container '%s'"), InContainerFilename);
		return false;
	}
	
	if (OutUsedEncryptionKeys)
	{
		OutUsedEncryptionKeys->Add(Reader->GetEncryptionKeyGuid());
	}

	if (bOutIsSigned)
	{
		*bOutIsSigned = EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Signed);
	}

	UE_LOG(LogIoStore, Display, TEXT("Extracting files from IoStore container '%s'..."), InContainerFilename);

	struct FEntry
	{
		FIoChunkId ChunkId;
		FString SourceFileName;
		FString DestFileName;
		uint64 Offset;
		bool bIsCompressed;

		FIoChunkHash Hash;
	};
	TArray<FEntry> Entries;
	const FIoDirectoryIndexReader& IndexReader = Reader->GetDirectoryIndexReader();
	FString DestPath(InDestPath);
	Reader->EnumerateChunks([&Entries, InFilter, &DestPath](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			if (!ChunkInfo.bHasValidFileName)
			{
				return true;
			}

			if (InFilter && (!ChunkInfo.FileName.MatchesWildcard(*InFilter)))
			{
				return true;
			}

			FEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.ChunkId = ChunkInfo.Id;
			Entry.SourceFileName = ChunkInfo.FileName;
			Entry.DestFileName = DestPath / ChunkInfo.FileName.Replace(TEXT("../../../"), TEXT(""));
			Entry.Offset = ChunkInfo.Offset;
			Entry.bIsCompressed = ChunkInfo.bIsCompressed;

			Entry.Hash = ChunkInfo.Hash;

			return true;
		});

	
	const bool bContainerIsEncrypted = EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Encrypted);
	const int32 MaxConcurrentTasks = (MaxConcurrentReaders <= 0) ? Entries.Num() : FMath::Min(MaxConcurrentReaders, Entries.Num());
	int32 ErrorCount = 0;

	for (int32 EntryStartIdx = 0; EntryStartIdx < Entries.Num(); )
	{
		TArray<UE::Tasks::TTask<bool>> ExtractTasks;
		ExtractTasks.Reserve(MaxConcurrentTasks);
		EAsyncExecution ThreadPool = EAsyncExecution::ThreadPool;

		const int32 NumTasks = FMath::Min(MaxConcurrentTasks, Entries.Num() - EntryStartIdx);
		for (int32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			const FEntry& Entry = Entries[EntryStartIdx + TaskIndex];

			UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> ReadTask = Reader->ReadAsync(Entry.ChunkId, FIoReadOptions());

			// Once the read is done, write out the file.
			ExtractTasks.Emplace(UE::Tasks::Launch(TEXT("IoStore_Extract"),
				[&Entry, &FileProcessFunc, ReadTask]() mutable
				{
					TIoStatusOr<FIoBuffer> ReadChunkResult = ReadTask.GetResult();
					if (!ReadChunkResult.IsOk())
					{
						UE_LOG(LogIoStore, Error, TEXT("Failed reading chunk for file \"%s\" (%s)."), *Entry.SourceFileName, *ReadChunkResult.Status().ToString());
						return false;
					}

					const uint8* Data = ReadChunkResult.ValueOrDie().Data();
					uint64 DataSize = ReadChunkResult.ValueOrDie().DataSize();
					if (Entry.ChunkId.GetChunkType() == EIoChunkType::ExportBundleData)
					{
						const FZenPackageSummary* PackageSummary = reinterpret_cast<const FZenPackageSummary*>(Data);
						uint64 HeaderDataSize = PackageSummary->HeaderSize;
						check(HeaderDataSize <= DataSize);
						FString DestFileName = FPaths::ChangeExtension(Entry.DestFileName, TEXT(".uheader"));
						if (!FileProcessFunc(Entry.SourceFileName, DestFileName, Entry.ChunkId, Data, HeaderDataSize))
						{
							return false;
						}
						DestFileName = FPaths::ChangeExtension(Entry.DestFileName, TEXT(".uexp"));
						if (!FileProcessFunc(Entry.SourceFileName, DestFileName, Entry.ChunkId, Data + HeaderDataSize, DataSize - HeaderDataSize))
						{
							return false;
						}
					}
					else if (!FileProcessFunc(Entry.SourceFileName, Entry.DestFileName, Entry.ChunkId, Data, DataSize))
					{
						return false;
					}
					return true;
				},
				Prerequisites(ReadTask)));
		}

		for (int32 TaskIndex = 0; TaskIndex < NumTasks; ++TaskIndex)
		{
			if (ExtractTasks[TaskIndex].GetResult())
			{
				const FEntry& Entry = Entries[EntryStartIdx + TaskIndex];
				if (OutOrderMap != nullptr)
				{
					OutOrderMap->Add(IndexReader.GetMountPoint() / Entry.SourceFileName, Entry.Offset);
				}
			}
			else
			{
				++ErrorCount;
			}
		}

		EntryStartIdx += NumTasks;
	}

	UE_LOG(LogIoStore, Log, TEXT("Finished extracting %d chunks (including %d errors)."), Entries.Num(), ErrorCount);
	return true;
}

bool SignIoStoreContainer(const TCHAR* InContainerFilename, const FRSAKeyHandle InSigningKey)
{
	FString TocFilePath = FPaths::ChangeExtension(InContainerFilename, TEXT(".utoc"));
	FString TempOutputPath = TocFilePath + ".tmp";
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	ON_SCOPE_EXIT
	{
		if (Ipf.FileExists(*TempOutputPath))
		{
			Ipf.DeleteFile(*TempOutputPath);
		}
	};

	FIoStoreTocResource TocResource;
	FIoStatus Status = FIoStoreTocResource::Read(*TocFilePath, EIoStoreTocReadOptions::ReadAll, TocResource);
	if (!Status.IsOk())
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed reading container file \"%s\"."), InContainerFilename);
		return false;
	}

	if (TocResource.ChunkBlockSignatures.Num() != TocResource.CompressionBlocks.Num())
	{
		UE_LOG(LogIoStore, Display, TEXT("Container is not signed, calculating block hashes..."));
		TocResource.ChunkBlockSignatures.Empty();
		TUniquePtr<FArchive> ContainerFileReader;
		int32 LastPartitionIndex = -1;
		TArray<uint8> BlockBuffer;
		BlockBuffer.SetNum(static_cast<int32>(TocResource.Header.CompressionBlockSize));
		const int32 BlockCount = TocResource.CompressionBlocks.Num();
		FString ContainerBasePath = FPaths::ChangeExtension(InContainerFilename, TEXT(""));
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
					UE_LOG(LogIoStore, Error, TEXT("Failed opening container file \"%s\"."), *UcasFilePath);
					return false;
				}
				ContainerFileReader.Reset(new FArchiveFileReaderGeneric(ContainerFileHandle, *UcasFilePath, ContainerFileHandle->Size(), 256 << 10));
				LastPartitionIndex = PartitionIndex;
			}
			ContainerFileReader->Seek(PartitionRawOffset);
			ContainerFileReader->Precache(PartitionRawOffset, 0); // Without this buffering won't work due to the first read after a seek always being uncached
			ContainerFileReader->Serialize(BlockBuffer.GetData(), BlockRawSize);
			FSHAHash& BlockHash = TocResource.ChunkBlockSignatures.AddDefaulted_GetRef();
			FSHA1::HashBuffer(BlockBuffer.GetData(), BlockRawSize, BlockHash.Hash);
		}
	}

	FIoContainerSettings ContainerSettings;
	ContainerSettings.ContainerId = TocResource.Header.ContainerId;
	ContainerSettings.ContainerFlags = TocResource.Header.ContainerFlags | EIoContainerFlags::Signed;
	ContainerSettings.EncryptionKeyGuid = TocResource.Header.EncryptionKeyGuid;
	ContainerSettings.SigningKey = InSigningKey;

	TIoStatusOr<uint64> WriteStatus = FIoStoreTocResource::Write(*TempOutputPath, TocResource, TocResource.Header.CompressionBlockSize, TocResource.Header.PartitionSize, ContainerSettings);
	if (!WriteStatus.IsOk())
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed writing new utoc file file \"%s\"."), *TocFilePath);
		return false;
	}

	Ipf.DeleteFile(*TocFilePath);
	Ipf.MoveFile(*TocFilePath, *TempOutputPath);

	return true;
}

static bool ParsePakResponseFile(const TCHAR* FilePath, TArray<FContainerSourceFile>& OutFiles)
{
	TArray<FString> ResponseFileContents;
	if (!FFileHelper::LoadFileToStringArray(ResponseFileContents, FilePath))
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to read response file '%s'."), FilePath);
		return false;
	}

	for (const FString& ResponseLine : ResponseFileContents)
	{
		TArray<FString> SourceAndDest;
		TArray<FString> Switches;

		FString NextToken;
		const TCHAR* ResponseLinePtr = *ResponseLine;
		while (FParse::Token(ResponseLinePtr, NextToken, false))
		{
			if ((**NextToken == TCHAR('-')))
			{
				new(Switches) FString(NextToken.Mid(1));
			}
			else
			{
				new(SourceAndDest) FString(NextToken);
			}
		}

		if (SourceAndDest.Num() == 0)
		{
			continue;
		}

		if (SourceAndDest.Num() != 2)
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid line in response file '%s'."), *ResponseLine);
			return false;
		}

		FPaths::NormalizeFilename(SourceAndDest[0]);

		FContainerSourceFile& FileEntry = OutFiles.AddDefaulted_GetRef();
		FileEntry.NormalizedPath = MoveTemp(SourceAndDest[0]);
		FileEntry.DestinationPath = MoveTemp(SourceAndDest[1]);

		for (int32 Index = 0; Index < Switches.Num(); ++Index)
		{
			if (Switches[Index] == TEXT("compress"))
			{
				FileEntry.bNeedsCompression = true;
			}
			if (Switches[Index] == TEXT("encrypt"))
			{
				FileEntry.bNeedsEncryption = true;
			}
		}
	}
	return true;
}

static bool ParsePakOrderFile(const TCHAR* FilePath, FFileOrderMap& Map, const FIoStoreArguments& Arguments)
{
	IOSTORE_CPU_SCOPE(ParsePakOrderFile);

	TArray<FString> OrderFileContents;
	if (!FFileHelper::LoadFileToStringArray(OrderFileContents, FilePath))
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to read order file '%s'."), FilePath);
		return false;
	}

	Map.Name = FPaths::GetCleanFilename(FilePath);
	UE_LOG(LogIoStore, Display, TEXT("Order file %s (short name %s) priority %d"), FilePath, *Map.Name, Map.Priority);
	int64 NextOrder = 0;
	for (const FString& OrderLine : OrderFileContents)
	{
		const TCHAR* OrderLinePtr = *OrderLine;
		FString PackageName;

		// Skip comments
		if (FCString::Strncmp(OrderLinePtr, TEXT("#"), 1) == 0 || FCString::Strncmp(OrderLinePtr, TEXT("//"), 2) == 0)
		{
			continue;
		}

		if (!FParse::Token(OrderLinePtr, PackageName, false))
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid line in order file '%s'."), *OrderLine);
			return false;
		}

		FName PackageFName;
		if (FPackageName::IsValidTextForLongPackageName(PackageName))
		{
			PackageFName = FName(PackageName);
		}
		else if (PackageName.StartsWith(TEXT("../../../")))
		{
			FString FullFileName = FPaths::Combine(Arguments.CookedDir, PackageName.RightChop(9));
			FPaths::NormalizeFilename(FullFileName);
			PackageFName = Arguments.PackageStore->GetPackageNameFromFileName(FullFileName);
		}

		if (!PackageFName.IsNone() && !Map.PackageNameToOrder.Contains(PackageFName))
		{
			Map.PackageNameToOrder.Emplace(PackageFName, NextOrder++);
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Order file %s (short name %s) contained %d valid entries"), FilePath, *Map.Name, Map.PackageNameToOrder.Num());
	return true;
}

class FCookedFileVisitor : public IPlatformFile::FDirectoryStatVisitor
{
	FCookedFileStatMap& CookedFileStatMap;

public:
	FCookedFileVisitor(FCookedFileStatMap& InCookedFileSizes)
		: CookedFileStatMap(InCookedFileSizes)
	{
		
	}

	virtual bool Visit(const TCHAR* FilenameOrDirectory, const FFileStatData& StatData)
	{
		if (StatData.bIsDirectory)
		{
			return true;
		}

		CookedFileStatMap.Add(FilenameOrDirectory, StatData.FileSize);

		return true;
	}
};

static bool ParseSizeArgument(const TCHAR* CmdLine, const TCHAR* Argument, uint64& OutSize, uint64 DefaultSize = 0)
{
	FString SizeString;
	if (FParse::Value(CmdLine, Argument, SizeString) && FParse::Value(CmdLine, Argument, OutSize))
	{
		if (SizeString.EndsWith(TEXT("MB")))
		{
			OutSize *= 1024*1024;
		}
		else if (SizeString.EndsWith(TEXT("KB")))
		{
			OutSize *= 1024;
		}
		return true;
	}
	else
	{
		OutSize = DefaultSize;
		return false;
	}
}

static bool ParseOrderFileArguments(FIoStoreArguments& Arguments)
{
	IOSTORE_CPU_SCOPE(ParseOrderFileArguments);

	uint64 OrderMapStartIndex = 0;
	FString OrderFileStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("Order="), OrderFileStr, false))
	{
		TArray<int32> OrderFilePriorities;
		TArray<FString> OrderFilePaths;
		OrderFileStr.ParseIntoArray(OrderFilePaths, TEXT(","), true);

		FString LegacyParam;
		if (FParse::Value(FCommandLine::Get(), TEXT("GameOrder="), LegacyParam, false))
		{
			UE_LOG(LogIoStore, Warning, TEXT("-GameOrder= and -CookerOrder= are deprecated in favor of -Order"));
			TArray<FString> LegacyPaths;
			LegacyParam.ParseIntoArray(LegacyPaths, TEXT(","), true);
			OrderFilePaths.Append(LegacyPaths);
		}
		if (FParse::Value(FCommandLine::Get(), TEXT("CookerOrder="), LegacyParam, false))
		{
			UE_LOG(LogIoStore, Warning, TEXT("-CookerOrder is ignored by IoStore. -GameOrder= and -CookerOrder= are deprecated in favor of -Order."));
		}

		FString OrderPriorityString;
		if (FParse::Value(FCommandLine::Get(), TEXT("OrderPriority="), OrderPriorityString, false))
		{
			TArray<FString> PriorityStrings;
			OrderPriorityString.ParseIntoArray(PriorityStrings, TEXT(","), true);
			if (PriorityStrings.Num() != OrderFilePaths.Num())
			{
				UE_LOG(LogIoStore, Error, TEXT("Number of parameters to -Order= and -OrderPriority= do not match"));
				return false;
			}

			for (const FString& PriorityString : PriorityStrings)
			{
				int32 Priority = FCString::Atoi(*PriorityString);
				OrderFilePriorities.Add(Priority);
			}
		}
		else
		{
			OrderFilePriorities.AddZeroed(OrderFilePaths.Num());
		}

		check(OrderFilePaths.Num() == OrderFilePriorities.Num());

		bool bMerge = false;
		for (int32 i = 0; i < OrderFilePaths.Num(); ++i)
		{
			FString& OrderFile = OrderFilePaths[i];
			int32 Priority = OrderFilePriorities[i];

			FFileOrderMap OrderMap(Priority, i);
			if (!ParsePakOrderFile(*OrderFile, OrderMap, Arguments))
			{
				return false;
			}
			Arguments.OrderMaps.Add(OrderMap);
		}
	}

	Arguments.bClusterByOrderFilePriority = !FParse::Param(FCommandLine::Get(), TEXT("DoNotClusterByOrderPriority"));
	
	return true;
}

bool ParseContainerGenerationArguments(FIoStoreArguments& Arguments, FIoStoreWriterSettings& WriterSettings)
{
	IOSTORE_CPU_SCOPE(ParseContainerGenerationArguments);
	if (FParse::Param(FCommandLine::Get(), TEXT("sign")))
	{
		Arguments.bSign = true;
	}

	UE_LOG(LogIoStore, Display, TEXT("Container signing - %s"), Arguments.bSign ? TEXT("ENABLED") : TEXT("DISABLED"));

	Arguments.bCreateDirectoryIndex = !FParse::Param(FCommandLine::Get(), TEXT("NoDirectoryIndex"));
	UE_LOG(LogIoStore, Display, TEXT("Directory index - %s"), Arguments.bCreateDirectoryIndex ? TEXT("ENABLED") : TEXT("DISABLED"));

	WriterSettings.CompressionMethod = DefaultCompressionMethod;
	WriterSettings.CompressionBlockSize = DefaultCompressionBlockSize;

	TArray<FName> CompressionFormats;
	FString DesiredCompressionFormats;
	if (FParse::Value(FCommandLine::Get(), TEXT("-compressionformats="), DesiredCompressionFormats) ||
		FParse::Value(FCommandLine::Get(), TEXT("-compressionformat="), DesiredCompressionFormats))
	{
		TArray<FString> Formats;
		DesiredCompressionFormats.ParseIntoArray(Formats, TEXT(","));
		for (FString& Format : Formats)
		{
			// look until we have a valid format
			FName FormatName = *Format;

			if (FCompression::IsFormatValid(FormatName))
			{
				WriterSettings.CompressionMethod = FormatName;
				break;
			}
		}

		if (WriterSettings.CompressionMethod == NAME_None)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to find desired compression format(s) '%s'. Using falling back to '%s'"),
				*DesiredCompressionFormats, *DefaultCompressionMethod.ToString());
		}
		else
		{
			UE_LOG(LogIoStore, Display, TEXT("Using compression format '%s'"), *WriterSettings.CompressionMethod.ToString());
		}
	}

	ParseSizeArgument(FCommandLine::Get(), TEXT("-alignformemorymapping="), WriterSettings.MemoryMappingAlignment, DefaultMemoryMappingAlignment);
	ParseSizeArgument(FCommandLine::Get(), TEXT("-compressionblocksize="), WriterSettings.CompressionBlockSize, DefaultCompressionBlockSize);

	WriterSettings.CompressionBlockAlignment = DefaultCompressionBlockAlignment;

	uint64 BlockAlignment = 0;
	if (ParseSizeArgument(FCommandLine::Get(), TEXT("-blocksize="), BlockAlignment))
	{
		WriterSettings.CompressionBlockAlignment = BlockAlignment;
	}

	//
	// If a filename to a global.utoc container is provided, all containers in that directory will have their compressed blocks be
	// made available for the new containers to reuse. This provides two benefits:
	//	1.	Saves compression time for the new blocks, as ssd/nvme io times are significantly faster.
	//	2.	Prevents trivial bit changes in the compressor from causing patch changes down the line, 
	//		allowing worry-free compressor upgrading.
	//
	// This should be a path to your last released containers. If those containers are encrypted, be sure to
	// provide keys via -ReferenceContainerCryptoKeys.
	//
	if (FParse::Value(FCommandLine::Get(), TEXT("-ReferenceContainerGlobalFileName="), Arguments.ReferenceChunkGlobalContainerFileName))
	{
		FString CryptoKeysCacheFilename;
		if (FParse::Value(FCommandLine::Get(), TEXT("-ReferenceContainerCryptoKeys="), CryptoKeysCacheFilename))
		{
			UE_LOG(LogIoStore, Display, TEXT("Parsing reference container crypto keys from a crypto key cache file '%s'"), *CryptoKeysCacheFilename);
			KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, Arguments.ReferenceChunkKeys);
		}
	}

	// By default, we use any hashes in the asset registry that exist in order to avoid reading and hashing
	// chunk unnecessarily. This flag causes us to read and hash anyway, and then ensure they match what is
	// in the asset registry. It is very bad if this fails!
	Arguments.bVerifyHashDatabase = FParse::Param(FCommandLine::Get(), TEXT("verifyhashdatabase"));


	uint64 PatchPaddingAlignment = 0;
	if (ParseSizeArgument(FCommandLine::Get(), TEXT("-patchpaddingalign="), PatchPaddingAlignment))
	{
		if (PatchPaddingAlignment < WriterSettings.CompressionBlockAlignment)
		{
			WriterSettings.CompressionBlockAlignment = PatchPaddingAlignment;
		}
	}

	// Temporary, this command-line allows us to explicitly override the value otherwise shared between pak building and iostore
	uint64 IOStorePatchPaddingAlignment = 0;
	if (ParseSizeArgument(FCommandLine::Get(), TEXT("-iostorepatchpaddingalign="), IOStorePatchPaddingAlignment))
	{
		WriterSettings.CompressionBlockAlignment = IOStorePatchPaddingAlignment;
	}

	uint64 MaxPartitionSize = 0;
	if (ParseSizeArgument(FCommandLine::Get(), TEXT("-maxPartitionSize="), MaxPartitionSize))
	{
		WriterSettings.MaxPartitionSize = MaxPartitionSize;
	}

	int32 CompressionMinBytesSaved = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-compressionMinBytesSaved="), CompressionMinBytesSaved))
	{
		WriterSettings.CompressionMinBytesSaved = CompressionMinBytesSaved;
	}

	int32 CompressionMinPercentSaved = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-compressionMinPercentSaved="), CompressionMinPercentSaved))
	{
		WriterSettings.CompressionMinPercentSaved = CompressionMinPercentSaved;
	}

	WriterSettings.bCompressionEnableDDC = FParse::Param(FCommandLine::Get(), TEXT("compressionEnableDDC"));

	int32 CompressionMinSizeToConsiderDDC = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-compressionMinSizeToConsiderDDC="), CompressionMinSizeToConsiderDDC))
	{
		WriterSettings.CompressionMinSizeToConsiderDDC = CompressionMinSizeToConsiderDDC;
	}

	UE_LOG(LogIoStore, Display, TEXT("Using memory mapping alignment '%ld'"), WriterSettings.MemoryMappingAlignment);
	UE_LOG(LogIoStore, Display, TEXT("Using compression block size '%ld'"), WriterSettings.CompressionBlockSize);
	UE_LOG(LogIoStore, Display, TEXT("Using compression block alignment '%ld'"), WriterSettings.CompressionBlockAlignment);
	UE_LOG(LogIoStore, Display, TEXT("Using compression min bytes saved '%d'"), WriterSettings.CompressionMinBytesSaved);
	UE_LOG(LogIoStore, Display, TEXT("Using compression min percent saved '%d'"), WriterSettings.CompressionMinPercentSaved);
	UE_LOG(LogIoStore, Display, TEXT("Using max partition size '%lld'"), WriterSettings.MaxPartitionSize);
	if (WriterSettings.bCompressionEnableDDC)
	{
		UE_LOG(LogIoStore, Display, TEXT("Using DDC for compression with min size '%d'"), WriterSettings.CompressionMinSizeToConsiderDDC);
	}
	else
	{
		UE_LOG(LogIoStore, Display, TEXT("Not using DDC for compression"));
	}

	FString CommandListFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("Commands="), CommandListFile))
	{
		UE_LOG(LogIoStore, Display, TEXT("Using command list file: '%s'"), *CommandListFile);
		TArray<FString> Commands;
		if (!FFileHelper::LoadFileToStringArray(Commands, *CommandListFile))
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed to read command list file '%s'."), *CommandListFile);
			return false;
		}

		Arguments.Containers.Reserve(Commands.Num());
		for (const FString& Command : Commands)
		{
			FContainerSourceSpec& ContainerSpec = Arguments.Containers.AddDefaulted_GetRef();

			if (FParse::Value(*Command, TEXT("Output="), ContainerSpec.OutputPath))
			{
				ContainerSpec.OutputPath = FPaths::ChangeExtension(ContainerSpec.OutputPath, TEXT(""));
			}
			ContainerSpec.bOnDemand = FParse::Param(*Command, TEXT("OnDemand"));
			FParse::Value(*Command, TEXT("OptionalOutput="), ContainerSpec.OptionalOutputPath);

			FParse::Value(*Command, TEXT("StageLooseFileRootPath="), ContainerSpec.StageLooseFileRootPath);

			FString ContainerName;
			if (FParse::Value(*Command, TEXT("ContainerName="), ContainerName))
			{
				ContainerSpec.Name = FName(ContainerName);
			}

			FString PatchSourceWildcard;
			if (FParse::Value(*Command, TEXT("PatchSource="), PatchSourceWildcard))
			{
				IFileManager::Get().FindFiles(ContainerSpec.PatchSourceContainerFiles, *PatchSourceWildcard, true, false);
				FString PatchSourceContainersDirectory = FPaths::GetPath(*PatchSourceWildcard);
				for (FString& PatchSourceContainerFile : ContainerSpec.PatchSourceContainerFiles)
				{
					PatchSourceContainerFile = PatchSourceContainersDirectory / PatchSourceContainerFile;
					FPaths::NormalizeFilename(PatchSourceContainerFile);
				}
			}

			ContainerSpec.bGenerateDiffPatch = FParse::Param(*Command, TEXT("GenerateDiffPatch"));

			FParse::Value(*Command, TEXT("PatchTarget="), ContainerSpec.PatchTargetFile);

			FString ResponseFilePath;
			if (FParse::Value(*Command, TEXT("ResponseFile="), ResponseFilePath))
			{
				if (!ParsePakResponseFile(*ResponseFilePath, ContainerSpec.SourceFiles))
				{
					UE_LOG(LogIoStore, Error, TEXT("Failed to parse Pak response file '%s'"), *ResponseFilePath);
					return false;
				}
				FParse::Value(*Command, TEXT("EncryptionKeyOverrideGuid="), ContainerSpec.EncryptionKeyOverrideGuid);
			}
		}
	}

	for (const FContainerSourceSpec& Container : Arguments.Containers)
	{
		if (Container.Name.IsNone())
		{
			UE_LOG(LogIoStore, Error, TEXT("ContainerName argument missing for container '%s'"), *Container.OutputPath);
			return false;
		}
	}

	Arguments.bFileRegions = FParse::Param(FCommandLine::Get(), TEXT("FileRegions"));
	WriterSettings.bEnableFileRegions = Arguments.bFileRegions;

	FString PatchReferenceCryptoKeysFilename;
	if (FParse::Value(FCommandLine::Get(), TEXT("PatchCryptoKeys="), PatchReferenceCryptoKeysFilename))
	{
		KeyChainUtilities::LoadKeyChainFromFile(PatchReferenceCryptoKeysFilename, Arguments.PatchKeyChain);
	}
	else
	{
		Arguments.PatchKeyChain = Arguments.KeyChain;
	}

	return true;
}

int32 CreateIoStoreContainerFiles(const TCHAR* CmdLine)
{
	IOSTORE_CPU_SCOPE(CreateIoStoreContainerFiles);

	UE_LOG(LogIoStore, Display, TEXT("==================== IoStore Utils ===================="));

	FIoStoreArguments Arguments;
	FIoStoreWriterSettings WriterSettings;

	LoadKeyChain(FCommandLine::Get(), Arguments.KeyChain);
	
	ITargetPlatform* TargetPlatform = nullptr;
	FString TargetPlatformName;
	if (FParse::Value(FCommandLine::Get(), TEXT("TargetPlatform="), TargetPlatformName))
	{
		ITargetPlatformManagerModule& TPM = GetTargetPlatformManagerRef();
		TargetPlatform = TPM.FindTargetPlatform(TargetPlatformName);
		if (!TargetPlatform)
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid TargetPlatform: '%s'"), *TargetPlatformName);
			return 1;
		}
	}

	FParse::Value(FCommandLine::Get(), TEXT("csv="), Arguments.CsvPath);

	FString ArgumentValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("List="), ArgumentValue))
	{
		FString ContainerPathOrWildcard = MoveTemp(ArgumentValue);
		if (Arguments.CsvPath.Len() == 0)
		{
			UE_LOG(LogIoStore, Error, TEXT("Incorrect arguments. Expected: -list=<ContainerFile> -csv=<path>"));
		}

		return ListContainer(Arguments.KeyChain, ContainerPathOrWildcard, Arguments.CsvPath);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("AssetRegistryWriteback="), ArgumentValue))
	{
		//
		// Opens a given directory of containers and a given asset registry, and adds chunk size information
		// for an asset's package to its asset tags in the asset registry. This can also be done during the staging
		// process with -WriteBackMetadataToAssetRegistry (below).
		//
		FString AssetRegistryFileName = MoveTemp(ArgumentValue);
		FString PathToContainers;
		if (!FParse::Value(FCommandLine::Get(), TEXT("ContainerDirectory="), PathToContainers))
		{
			UE_LOG(LogIoStore, Error, TEXT("Asset registry writeback requires -ContainerDirectory=Path/To/Containers"));
		}
		UE_LOG(LogIoStore, Warning, TEXT("AssetRegistryWriteback after stage is deprecated and will be removed in 5.5. Use writeback during stage via project packaging settings."));
		return DoAssetRegistryWritebackAfterStage(AssetRegistryFileName, MoveTemp(PathToContainers), Arguments.KeyChain);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("Describe="), ArgumentValue))
	{
		FString ContainerPathOrWildcard = ArgumentValue;
		FString PackageFilter;
		FParse::Value(FCommandLine::Get(), TEXT("PackageFilter="), PackageFilter);
		FString OutPath;
		FParse::Value(FCommandLine::Get(), TEXT("DumpToFile="), OutPath);
		bool bIncludeExportHashes = FParse::Param(FCommandLine::Get(), TEXT("IncludeExportHashes"));
		return Describe(ContainerPathOrWildcard, Arguments.KeyChain, PackageFilter, OutPath, bIncludeExportHashes);
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("ValidateCrossContainerRefs="), ArgumentValue))
	{
		FString ContainerPathOrWildcard = ArgumentValue;
		FString ConfigPath;
		FParse::Value(FCommandLine::Get(), TEXT("Config="), ConfigPath);
		FString OutPath;
		FParse::Value(FCommandLine::Get(), TEXT("DumpToFile="), OutPath);
		return ValidateCrossContainerRefs(ContainerPathOrWildcard, Arguments.KeyChain, ConfigPath, OutPath);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("ProfileReadSpeed")))
	{
		// Load the .UTOC file provided and read it in its entirety, cmdline parsed in function
		return ProfileReadSpeed(FCommandLine::Get(), Arguments.KeyChain);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("Diff")))
	{
		FString SourcePath, TargetPath, OutPath;
		FKeyChain SourceKeyChain, TargetKeyChain;

		if (!FParse::Value(FCommandLine::Get(), TEXT("Source="), SourcePath))
		{
			UE_LOG(LogIoStore, Error, TEXT("Incorrect arguments. Expected: -Diff -Source=<Path> -Target=<path>"));
			return -1;
		}

		if (!IFileManager::Get().DirectoryExists(*SourcePath))
		{
			UE_LOG(LogIoStore, Error, TEXT("Source directory '%s' doesn't exist"), *SourcePath);
			return -1;
		}

		if (!FParse::Value(FCommandLine::Get(), TEXT("Target="), TargetPath))
		{
			UE_LOG(LogIoStore, Error, TEXT("Incorrect arguments. Expected: -Diff -Source=<Path> -Target=<path>"));
		}

		if (!IFileManager::Get().DirectoryExists(*TargetPath))
		{
			UE_LOG(LogIoStore, Error, TEXT("Target directory '%s' doesn't exist"), *TargetPath);
			return -1;
		}

		FParse::Value(FCommandLine::Get(), TEXT("DumpToFile="), OutPath);

		FString CryptoKeysCacheFilename;
		if (FParse::Value(CmdLine, TEXT("CryptoKeys="), CryptoKeysCacheFilename) ||
			FParse::Value(CmdLine, TEXT("SourceCryptoKeys="), CryptoKeysCacheFilename))
		{
			UE_LOG(LogIoStore, Display, TEXT("Parsing source crypto keys from '%s'"), *CryptoKeysCacheFilename);
			KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, SourceKeyChain);
		}

		if (FParse::Value(CmdLine, TEXT("TargetCryptoKeys="), CryptoKeysCacheFilename))
		{
			UE_LOG(LogIoStore, Display, TEXT("Parsing target crypto keys from '%s'"), *CryptoKeysCacheFilename);
			KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, TargetKeyChain);
		}
		else
		{
			TargetKeyChain = SourceKeyChain;
		}

		EChunkTypeFilter ChunkTypeFilter = EChunkTypeFilter::None;
		if (FParse::Param(FCommandLine::Get(), TEXT("FilterBulkData")))
		{
			ChunkTypeFilter = EChunkTypeFilter::BulkData;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("FilterPackageData")))
		{
			ChunkTypeFilter = EChunkTypeFilter::PackageData;
		}

		return Diff(SourcePath, SourceKeyChain, TargetPath, TargetKeyChain, OutPath, ChunkTypeFilter);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("Staged2Zen")))
	{
		FString BuildPath;
		FString ProjectName;
		if (!FParse::Value(FCommandLine::Get(), TEXT("BuildPath="), BuildPath) ||
			!FParse::Value(FCommandLine::Get(), TEXT("ProjectName="), ProjectName) ||
			!TargetPlatform)
		{
			UE_LOG(LogIoStore, Error, TEXT("Incorrect arguments. Expected: -Staged2Zen -BuildPath=<Path> -ProjectName=<ProjectName> -TargetPlatform=<Platform>"));
			return -1;
		}
		return Staged2Zen(BuildPath, Arguments.KeyChain, ProjectName, TargetPlatform);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("CreateContentPatch")))
	{
		if (!ParseContainerGenerationArguments(Arguments, WriterSettings))
		{
			return -1;
		}

		for (const FContainerSourceSpec& Container : Arguments.Containers)
		{
			if (Container.PatchTargetFile.IsEmpty())
			{
				UE_LOG(LogIoStore, Error, TEXT("PatchTarget argument missing for container '%s'"), *Container.OutputPath);
				return -1;
			}
		}

		return CreateContentPatch(Arguments, WriterSettings);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("GenerateZenFileSystemManifest")))
	{
		if (!TargetPlatform)
		{
			UE_LOG(LogIoStore, Error, TEXT("Incorrect arguments. Expected: -GenerateZenFileSystemManifest -TargetPlatform=<Platform>"));
			return -11;
		}
		return GenerateZenFileSystemManifest(TargetPlatform);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("StartZenServerForStage")))
	{
		FString ManifestFilename;
		if (!FParse::Value(FCommandLine::Get(), TEXT("PackageStoreManifest="), ManifestFilename))
		{
			UE_LOG(LogIoStore, Error, TEXT("Expected -PackageStoreManifest=<path to package store manifest>"));
			return -1;
		}

		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*ManifestFilename));
		if (!Ar)
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed reading package store manifest"));
			return -1;
		}

		FCbObject ManifestObject = LoadCompactBinary(*Ar).AsObject();
		FCbObject OplogObject;
		FCbField ZenServerField = ManifestObject["zenserver"];
		if (ZenServerField)
		{
			UE::Zen::FServiceSettings ZenServiceSettings;
			ZenServiceSettings.ReadFromCompactBinary(ZenServerField["settings"]);
			FString ProjectId = FString(ZenServerField["projectid"].AsString());
			FString OplogId = FString(ZenServerField["oplogid"].AsString());

			// We just want the auto launch functionality
			UE::FZenStoreHttpClient ZenStoreClient(MoveTemp(ZenServiceSettings));
			ZenStoreClient.InitializeReadOnly(ProjectId, OplogId);
		}

		return 0;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("CreateDLCContainer="), Arguments.DLCPluginPath))
	{
		if (!ParseContainerGenerationArguments(Arguments, WriterSettings))
		{
			return -1;
		}
		
		Arguments.DLCName = FPaths::GetBaseFilename(*Arguments.DLCPluginPath);
		Arguments.bRemapPluginContentToGame = FParse::Param(FCommandLine::Get(), TEXT("RemapPluginContentToGame"));
		Arguments.bUpload = FParse::Param(FCommandLine::Get(), TEXT("Upload"));

		UE_LOG(LogIoStore, Display, TEXT("DLC: '%s'"), *Arguments.DLCPluginPath);
		UE_LOG(LogIoStore, Display, TEXT("Remapping plugin content to game: '%s'"), Arguments.bRemapPluginContentToGame ? TEXT("True") : TEXT("False"));

		bool bAssetRegistryLoaded = false;
		FString BasedOnReleaseVersionPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("BasedOnReleaseVersionPath="), BasedOnReleaseVersionPath))
		{
			UE_LOG(LogIoStore, Display, TEXT("Based on release version path: '%s'"), *BasedOnReleaseVersionPath);
			FString DevelopmentAssetRegistryPath = FPaths::Combine(BasedOnReleaseVersionPath, TEXT("Metadata"), GetDevelopmentAssetRegistryFilename());
			FArrayReader SerializedAssetData;
			if (FPaths::FileExists(*DevelopmentAssetRegistryPath) && FFileHelper::LoadFileToArray(SerializedAssetData, *DevelopmentAssetRegistryPath))
			{
				FAssetRegistryState ReleaseAssetRegistry;
				FAssetRegistrySerializationOptions Options;
				if (ReleaseAssetRegistry.Serialize(SerializedAssetData, Options))
				{
					UE_LOG(LogIoStore, Display, TEXT("Loaded asset registry '%s'"), *DevelopmentAssetRegistryPath);
					bAssetRegistryLoaded = true;

					TArray<FName> PackageNames;
					ReleaseAssetRegistry.GetPackageNames(PackageNames);
					Arguments.ReleasedPackages.PackageNames.Reserve(PackageNames.Num());
					Arguments.ReleasedPackages.PackageIdToName.Reserve(PackageNames.Num());

					for (FName PackageName : PackageNames)
					{
						// skip over packages that were not actually saved out, but were added to the AR - the DLC may now have those packages included,
						// and there will be a conflict later on if the package is in this list and the DLC list. PackageFlags of 0 means it was 
						// evaluated and skipped.
						TArrayView<FAssetData const* const> AssetsForPackage = ReleaseAssetRegistry.GetAssetsByPackageName(PackageName);
						checkf(AssetsForPackage.Num() > 0, TEXT("It is unexpected that no assets were found in DevelopmentAssetRegistry for the package %s. This indicates an invalid AR."), *PackageName.ToString());
						// just check the first one in the list, they will all have the same flags
						if (AssetsForPackage[0]->PackageFlags != 0)
						{
							Arguments.ReleasedPackages.PackageNames.Add(PackageName);
							Arguments.ReleasedPackages.PackageIdToName.Add(FPackageId::FromName(PackageName), PackageName);
						}
					}
				}
			}
		}
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("CreateGlobalContainer="), Arguments.GlobalContainerPath))
	{
		Arguments.GlobalContainerPath = FPaths::ChangeExtension(Arguments.GlobalContainerPath, TEXT(""));
		Arguments.bUpload = FParse::Param(FCommandLine::Get(), TEXT("Upload"));

		if (!ParseContainerGenerationArguments(Arguments, WriterSettings))
		{
			return -1;
		}
	}
	else
	{
		UE_LOG(LogIoStore, Display, TEXT("Usage:"));
		UE_LOG(LogIoStore, Display, TEXT(" -List=</path/to/[container.utoc|*.utoc]> -CSV=<list.csv> [-CryptoKeys=</path/to/crypto.json>]"));
		UE_LOG(LogIoStore, Display, TEXT(" -Describe=</path/to/global.utoc> [-PackageFilter=<PackageName>] [-DumpToFile=<describe.txt>] [-CryptoKeys=</path/to/crypto.json>]"));
		return -1;
	}

	// Common path for creating containers
	FParse::Value(FCommandLine::Get(), TEXT("CookedDirectory="), Arguments.CookedDir);
	if (Arguments.CookedDir.IsEmpty())
	{
		UE_LOG(LogIoStore, Error, TEXT("CookedDirectory must be specified"));
		return -1;
	}

	//
	// -compresslevel is technically consumed by OodleDataCompressionFormat, however the setting that it represents (PackageCompressionLevel_*)
	// is the intention of package compression, and so should also determine the compression we use for shaders. For Shaders we want fast decompression,
	// so we always used Mermaid. Note that this relies on compresslevel being passed even when the containers aren't compressed.
	//
	FString ShaderOodleLevel;
	FParse::Value(FCommandLine::Get(), TEXT("compresslevel="), ShaderOodleLevel);
	if (ShaderOodleLevel.Len())
	{
		if (FOodleDataCompression::ECompressionLevelFromString(*ShaderOodleLevel, Arguments.ShaderOodleLevel))
		{
			UE_LOG(LogIoStore, Display, TEXT("Selected Oodle level %d (%s) from command line for shaders"), (int)Arguments.ShaderOodleLevel, FOodleDataCompression::ECompressionLevelToString(Arguments.ShaderOodleLevel));
		}
		
	}

	// Whether or not to write compressed asset sizes back to the asset registry.

	FString WriteBackMetadataToAssetRegistry;
	if (FParse::Value(FCommandLine::Get(), TEXT("WriteBackMetadataToAssetRegistry="), WriteBackMetadataToAssetRegistry))
	{
		// StaticEnum not available in UnrealPak, so manual conversion:
		if (WriteBackMetadataToAssetRegistry.Equals(TEXT("AdjacentFile"), ESearchCase::IgnoreCase))
		{
			Arguments.WriteBackMetadataToAssetRegistry = EAssetRegistryWritebackMethod::AdjacentFile;
		}
		else if (WriteBackMetadataToAssetRegistry.Equals(TEXT("OriginalFile"), ESearchCase::IgnoreCase))
		{
			Arguments.WriteBackMetadataToAssetRegistry = EAssetRegistryWritebackMethod::OriginalFile;
		}
		else if (WriteBackMetadataToAssetRegistry.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase))
		{
			Arguments.WriteBackMetadataToAssetRegistry = EAssetRegistryWritebackMethod::Disabled;
		}
		else
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid WriteBackMetdataToAssetRegistry value: %s - check setting in ProjectSettings -> Packaging"), *WriteBackMetadataToAssetRegistry);
			UE_LOG(LogIoStore, Error, TEXT("Valid options are: AdjacentFile, OriginalFile, Disabled."), *WriteBackMetadataToAssetRegistry);
			return -1;
		}

		Arguments.bWritePluginSizeSummaryJsons = FParse::Param(FCommandLine::Get(), TEXT("WritePluginSizeSummaryJsons"));
	}

	FString PackageStoreManifestFilename;
	if (FParse::Value(FCommandLine::Get(), TEXT("PackageStoreManifest="), PackageStoreManifestFilename))
	{
		TUniquePtr<FCookedPackageStore> PackageStore = MakeUnique<FCookedPackageStore>(Arguments.CookedDir);
		FIoStatus Status = PackageStore->Load(*PackageStoreManifestFilename);
		if (Status.IsOk())
		{
			Arguments.PackageStore = MoveTemp(PackageStore);
		}
		else
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Failed loading package store manifest '%s'"), *PackageStoreManifestFilename);
		}
	}
	else
	{
		UE_LOG(LogIoStore, Error, TEXT("Expected -PackageStoreManifest=<path to package store manifest>"));
		return -1;
	}

	if (!ParseOrderFileArguments(Arguments))
	{
		return false;
	}

	FString ScriptObjectsFile;
	if (FParse::Value(FCommandLine::Get(), TEXT("ScriptObjects="), ScriptObjectsFile))
	{
		TArray<uint8> ScriptObjectsData;
		if (!FFileHelper::LoadFileToArray(ScriptObjectsData, *ScriptObjectsFile))
		{
			UE_LOG(LogIoStore, Fatal, TEXT("Failed reading script objects file '%s'"), *ScriptObjectsFile);
		}
		Arguments.ScriptObjects = MakeUnique<FIoBuffer>(FIoBuffer::Clone, ScriptObjectsData.GetData(), ScriptObjectsData.Num());
	}
	else
	{
		UE_CLOG(!Arguments.PackageStore->HasZenStoreClient(), LogIoStore, Fatal, TEXT("Expected -ScriptObjects=<path to script objects file> argument"));
		TIoStatusOr<FIoBuffer> Status = Arguments.PackageStore->ReadChunk(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects));
		UE_CLOG(!Status.IsOk(), LogIoStore, Fatal, TEXT("Failed reading script objects chunk '%s'"), *Status.Status().ToString());
		Arguments.ScriptObjects = MakeUnique<FIoBuffer>(Status.ConsumeValueOrDie());
	}

	{
		IOSTORE_CPU_SCOPE(FindCookedAssets);
		UE_LOG(LogIoStore, Display, TEXT("Searching for cooked assets in folder '%s'"), *Arguments.CookedDir);
		FCookedFileVisitor CookedFileVistor(Arguments.CookedFileStatMap);
		IFileManager::Get().IterateDirectoryStatRecursively(*Arguments.CookedDir, CookedFileVistor);
		UE_LOG(LogIoStore, Display, TEXT("Found '%d' files"), Arguments.CookedFileStatMap.Num());
	}

	return CreateTarget(Arguments, WriterSettings);
}

bool UploadIoStoreContainerFiles(const UE::IO::IAS::FIoStoreUploadParams& UploadParams, TConstArrayView<FString> ContainerFiles, const FKeyChain& KeyChain)
{
	TIoStatusOr<UE::IO::IAS::FIoStoreUploadResult> Result = UE::IO::IAS::UploadContainerFiles(UploadParams, ContainerFiles, KeyChain);
	if (Result.IsOk() == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to upload container file(s), reason '%s'"), *Result.Status().ToString());
		return false;
	}

	UE::IO::IAS::FIoStoreUploadResult UploadResult = Result.ConsumeValueOrDie();

	return Result.IsOk();
}

bool UploadIoStoreContainerFiles(const TCHAR* ContainerPathOrWildcard)
{
	check(ContainerPathOrWildcard);

	FKeyChain KeyChain;
	LoadKeyChain(FCommandLine::Get(), KeyChain);

	TIoStatusOr<UE::IO::IAS::FIoStoreUploadParams> UploadParams = UE::IO::IAS::FIoStoreUploadParams::Parse(FCommandLine::Get());
	if (UploadParams.IsOk() == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to upload container file(s), reason '%s'"), *UploadParams.Status().ToString());
		return false;
	}

	TArray<FString> ContainerFiles;
	{
		if (IFileManager::Get().FileExists(ContainerPathOrWildcard))
		{
			ContainerFiles.Add(ContainerPathOrWildcard);
		}
		else if (IFileManager::Get().DirectoryExists(ContainerPathOrWildcard))
		{
			FString Directory = ContainerPathOrWildcard;
			FPaths::NormalizeDirectoryName(Directory);

			TArray<FString> FoundContainerFiles;
			IFileManager::Get().FindFiles(FoundContainerFiles, *(Directory / TEXT("*.utoc")), true, false);

			for (const FString& Filename : FoundContainerFiles)
			{
				ContainerFiles.Emplace(Directory / Filename);
			}
		}
		else
		{
			FString Directory = FPaths::GetPath(ContainerPathOrWildcard);
			FPaths::NormalizeDirectoryName(Directory);

			TArray<FString> FoundContainerFiles;
			IFileManager::Get().FindFiles(FoundContainerFiles, ContainerPathOrWildcard, true, false);

			for (const FString& Filename : FoundContainerFiles)
			{
				ContainerFiles.Emplace(Directory / Filename);
			}
		}
	}

	if (ContainerFiles.IsEmpty())
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to find container file(s) '%s'"), ContainerPathOrWildcard);
		return false;
	}

	return UploadIoStoreContainerFiles(UploadParams.ConsumeValueOrDie(), ContainerFiles, KeyChain);
}

bool DownloadIoStoreContainerFiles(const TCHAR* TocPath)
{
	using namespace UE::IO::IAS;
	check(TocPath);

	TIoStatusOr<FIoStoreDownloadParams> Params = FIoStoreDownloadParams::Parse(FCommandLine::Get());
	if (Params.IsOk() == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to download container file(s), reason '%s'"), *Params.Status().ToString());
		return false;
	}

	FIoStatus Status = DownloadContainerFiles(Params.ConsumeValueOrDie(), TocPath);
	if (Status.IsOk() == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to download container file(s), reason '%s'"), *Status.ToString());
	}

	return Status.IsOk();
}

bool ListOnDemandTocs()
{
	using namespace UE::IO::IAS;

	TIoStatusOr<FIoStoreListTocsParams> Params = FIoStoreListTocsParams::Parse(FCommandLine::Get());
	if (Params.IsOk() == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to list TOC file(s), reason '%s'"), *Params.Status().ToString());
		return false;
	}

	FIoStatus Status = ListTocs(Params.ConsumeValueOrDie());
	if (Status.IsOk() == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to list TOC file(s), reason '%s'"), *Status.ToString());
	}

	return Status.IsOk();
}
