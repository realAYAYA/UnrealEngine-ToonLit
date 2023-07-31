// Copyright Epic Games, Inc. All Rights Reserved.

#include "IoStoreUtilities.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Hash/CityHash.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "IO/IoDispatcher.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/Base64.h"
#include "Misc/AES.h"
#include "Misc/CoreDelegates.h"
#include "Misc/KeyChainUtilities.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Archive.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/BufferWriter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/AsyncLoading2.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Settings/ProjectPackagingSettings.h" // for EAssetRegistryWritebackMethod
#include "IO/PackageStore.h"
#include "UObject/Class.h"
#include "UObject/NameBatchSerialization.h"
#include "UObject/PackageFileSummary.h"
#include "UObject/ObjectResource.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
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

//PRAGMA_DISABLE_OPTIMIZATION

IMPLEMENT_MODULE(FDefaultModuleImpl, IoStoreUtilities);

#define IOSTORE_CPU_SCOPE(NAME) TRACE_CPUPROFILER_EVENT_SCOPE(IoStore##NAME);
#define IOSTORE_CPU_SCOPE_DATA(NAME, DATA) TRACE_CPUPROFILER_EVENT_SCOPE(IoStore##NAME);

TRACE_DECLARE_MEMORY_COUNTER(IoStoreUsedFileBufferMemory, TEXT("IoStore/UsedFileBufferMemory"));

#define OUTPUT_CHUNKID_DIRECTORY 0

static const FName DefaultCompressionMethod = NAME_Zlib;
static const uint64 DefaultCompressionBlockSize = 64 << 10;
static const uint64 DefaultCompressionBlockAlignment = 64 << 10;
static const uint64 DefaultMemoryMappingAlignment = 16 << 10;

static TUniquePtr<FIoStoreReader> CreateIoStoreReader(const TCHAR* Path, const FKeyChain& KeyChain);

class FIoStoreChunkDatabase : public IIoStoreWriterReferenceChunkDatabase
{
public:

	TArray<TUniquePtr<FIoStoreReader>> Readers;
	struct FReaderChunks
	{
		int32 ReaderIndex;
		TMap<FIoChunkHash, FIoChunkId> Chunks;
	};

	TMap<FIoContainerId, FReaderChunks> ChunkDatabase;
	int32 RequestCount = 0;
	int32 FulfillCount = 0;
	int32 ContainerNotFound = 0;
	int64 FulfillBytes = 0;
	

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

		int64 IoChunkCount = 0;		
		for (const FString& ContainerFilePath : ContainerFilePaths)
		{
			TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, InDecryptionKeychain);
			if (Reader.IsValid() == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to open reference chunk container %s"), *InGlobalContainerFileName);
				return false;
			}
			FReaderChunks& ReaderChunks = ChunkDatabase.FindOrAdd(Reader->GetContainerId());

			Reader->EnumerateChunks([&ReaderChunks](const FIoStoreTocChunkInfo& ChunkInfo)
			{
				ReaderChunks.Chunks.Add(TPair<FIoChunkHash, FIoChunkId>(ChunkInfo.Hash, ChunkInfo.Id));
				return true;
			});

			IoChunkCount += ReaderChunks.Chunks.Num();
			ReaderChunks.ReaderIndex = Readers.Num();
			Readers.Add(MoveTemp(Reader));
		}

		UE_LOG(LogIoStore, Display, TEXT("Block reference loaded %d containers and %s chunks, in %.1f seconds"), Readers.Num(), *FText::AsNumber(IoChunkCount).ToString(), FPlatformTime::Seconds() - StartTime);
		return true;
	}

	// Not thread safe, called from the BeginCompress dispatch thread.
	virtual bool RetrieveChunk(const TPair<FIoContainerId, FIoChunkHash>& InChunkKey, const FName& InCompressionMethod, uint64 InUncompressedSize, uint64 InNumChunkBlocks, TUniqueFunction<void(TIoStatusOr<FIoStoreCompressedReadResult>)> InCompleteCallback)
	{
		RequestCount++;

		FReaderChunks* ReaderChunks = ChunkDatabase.Find(InChunkKey.Key);
		if (ReaderChunks == nullptr)
		{
			// Container doesn't exist - likely provided the path to a different project. Mark this
			// error as happening once so we can log at the end.
			ContainerNotFound++;
			return false;
		}

		FIoChunkId* ChunkId = ReaderChunks->Chunks.Find(InChunkKey.Value);
		if (ChunkId == nullptr)
		{
			// No exact chunk data match - this is a normal exit condition for a changed block.
			return false;
		}

		uint64 TotalCompressedSize = 0;
		uint64 TotalUncompressedSize = 0;
		uint32 CompressedBlockCount = 0;
		Readers[ReaderChunks->ReaderIndex]->EnumerateCompressedBlocksForChunk(*ChunkId, [&TotalUncompressedSize, &CompressedBlockCount, &TotalCompressedSize](const FIoStoreTocCompressedBlockInfo& BlockInfo)
		{			
			TotalCompressedSize += BlockInfo.CompressedSize;
			TotalUncompressedSize += BlockInfo.UncompressedSize;
			CompressedBlockCount ++;
			return true;
		});

		if (TotalUncompressedSize != InUncompressedSize)
		{
			// Shocked if this happens - hash match with different data!
			return false;
		}

		if (CompressedBlockCount != InNumChunkBlocks)
		{
			// Different CompressionBlockSize between the builds
			return false;
		}

		FulfillBytes += TotalCompressedSize;
		FulfillCount++;

		//
		// At this point we know we can use the block so we can go async.
		//
		FFunctionGraphTask::CreateAndDispatchWhenReady([this, ChunkId, ReaderIndex = ReaderChunks->ReaderIndex, CompleteCallback = MoveTemp(InCompleteCallback)]()
		{
			TIoStatusOr<FIoStoreCompressedReadResult> Result = Readers[ReaderIndex]->ReadCompressed(*ChunkId, FIoReadOptions());
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
	TArray<FContainerSourceFile> SourceFiles;
	FString PatchTargetFile;
	TArray<FString> PatchSourceContainerFiles;
	FGuid EncryptionKeyOverrideGuid;
	bool bGenerateDiffPatch = false;
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
	uint32 LoadOrderFactor;
	uint64 DiskLayoutOrder = MAX_uint64;
	TSet<struct FLegacyCookedPackage*> ReferencedByPackages;
	TMap<FContainerTargetSpec*, EShaderType> TypeInContainer;

	// This is used to ensure build determinism and such must be stable
	// across builds.
	static bool Sort(const FShaderInfo* A, const FShaderInfo* B)
	{
		if (A->DiskLayoutOrder == B->DiskLayoutOrder)
		{
			if (A->LoadOrderFactor == B->LoadOrderFactor)
			{
				// Shader chunk IDs are the hash of the shader so this is consistent across builds.
				uint64 AHash = *(uint64*)A->ChunkId.GetData();
				uint64 BHash = *(uint64*)B->ChunkId.GetData();
				return AHash < BHash;
			}
			return A->LoadOrderFactor < B->LoadOrderFactor;
		}
		return A->DiskLayoutOrder < B->DiskLayoutOrder;
	}
};

struct FLegacyCookedPackage
{
	FPackageId GlobalPackageId;
	FName PackageName;
	FName RedirectFromPackageName;
	FString FileName;
	uint64 UAssetSize = 0;
	uint64 UExpSize = 0;
	FString OptionalSegmentFileName;
	uint64 OptionalSegmentUAssetSize = 0;
	uint64 OptionalSegmentUExpSize = 0;
	uint64 TotalBulkDataSize = 0;
	uint64 DiskLayoutOrder = MAX_uint64;
	FPackageStorePackage* OptimizedPackage = nullptr;
	FPackageStorePackage* OptimizedOptionalSegmentPackage = nullptr;
	TArray<FShaderInfo*> Shaders;
	const FPackageStoreEntryResource* PackageStoreEntry = nullptr;
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
	FLegacyCookedPackage* Package = nullptr;
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
	FCookedPackageStore(const FString& CookedDir)
		: PackageStoreManifest(CookedDir)
	{
	}

	FIoStatus Load(const TCHAR* ManifestFilename)
	{
		FIoStatus Status = PackageStoreManifest.Load(ManifestFilename);
		if (!Status.IsOk())
		{
			return Status;
		}

		if (const FPackageStoreManifest::FZenServerInfo* ZenServerInfo = PackageStoreManifest.ReadZenServerInfo())
		{
			DataSource = MakeUnique<FZenStoreDataSource>(*ZenServerInfo);
		}
		
		if (DataSource)
		{
			FIoContainerId ContainerId = FIoContainerId::FromName(TEXT("global"));

			TIoStatusOr<FIoBuffer> HeaderBuffer = DataSource->ReadChunk(CreateIoChunkId(ContainerId.Value(), 0, EIoChunkType::ContainerHeader), FIoReadOptions());
			if (!HeaderBuffer.IsOk())
			{
				return FIoStatusBuilder(EIoErrorCode::ReadError) << TEXT("Failed to read container header");
			}

			FMemoryReaderView Ar(MakeArrayView(HeaderBuffer.ValueOrDie().Data(), HeaderBuffer.ValueOrDie().DataSize()));
			FIoContainerHeader ContainerHeader;
			Ar << ContainerHeader;

			PackageEntries.Reserve(ContainerHeader.PackageIds.Num());
			TArrayView<const FFilePackageStoreEntry> FilePackageEntries = MakeArrayView<const FFilePackageStoreEntry>(reinterpret_cast<const FFilePackageStoreEntry*>(ContainerHeader.StoreEntries.GetData()), ContainerHeader.PackageIds.Num());

			int32 Idx = 0;
			for (const FFilePackageStoreEntry& FilePackageEntry : FilePackageEntries)
			{
				FPackageStoreEntryResource& Entry = PackageEntries.AddDefaulted_GetRef();

				Entry.ExportInfo = FPackageStoreExportInfo
				{
					FilePackageEntry.ExportCount,
					FilePackageEntry.ExportBundleCount
				};
				Entry.ImportedPackageIds = MakeArrayView<const FPackageId>(FilePackageEntry.ImportedPackages.Data(), FilePackageEntry.ImportedPackages.Num());

				PackageIdToEntry.Add(ContainerHeader.PackageIds[Idx++], &Entry);
			}
		}
		
		ManifestPackageInfos = PackageStoreManifest.GetPackages();
		for (const FPackageStoreManifest::FPackageInfo& PackageInfo : ManifestPackageInfos)
		{
			FName PackageName = FName(PackageInfo.PackageName);
			PackageNameToPackageInfoMap.Add(PackageName, &PackageInfo);
			for (const FIoChunkId& ChunkId : PackageInfo.ExportBundleChunkIds)
			{
				ChunkIdToPackageNameMap.Add(ChunkId, PackageName);
			}
			for (const FIoChunkId& ChunkId : PackageInfo.BulkDataChunkIds)
			{
				ChunkIdToPackageNameMap.Add(ChunkId, PackageName);
			}
		}

		for (const FPackageStoreManifest::FFileInfo& FileInfo : PackageStoreManifest.GetFiles())
		{
			FilenameToChunkIdMap.Add(FileInfo.FileName, FileInfo.ChunkId);
			FName* FindPackageName = ChunkIdToPackageNameMap.Find(FileInfo.ChunkId);
			if (FindPackageName)
			{
				FString FilenameWithoutExtension = FPaths::ChangeExtension(FileInfo.FileName, FString());
				FilenameToPackageNameMap.Add(MoveTemp(FilenameWithoutExtension), *FindPackageName);
			}
		}

		return FIoStatus::Ok;
	}

	FIoChunkId GetChunkIdFromFileName(const FString& Filename) const
	{
		return FilenameToChunkIdMap.FindRef(Filename);
	}

	FName GetPackageNameFromChunkId(const FIoChunkId& ChunkId) const
	{
		return ChunkIdToPackageNameMap.FindRef(ChunkId);
	}

	FName GetPackageNameFromFileName(const FString& Filename) const
	{
		FString FilenameWithoutExtension = FPaths::ChangeExtension(Filename, FString());
		return FilenameToPackageNameMap.FindRef(FilenameWithoutExtension);
	}

	const FPackageStoreManifest::FPackageInfo* GetPackageInfoFromPackageName(FName PackageName)
	{
		return PackageNameToPackageInfoMap.FindRef(PackageName);
	}

	const FPackageStoreEntryResource* GetPackageStoreEntry(FPackageId PackageId) const
	{
		return PackageIdToEntry.FindRef(PackageId);
	}

	bool HasDataSource() const
	{
		return DataSource.IsValid();
	}

	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId)
	{
		return DataSource->GetChunkSize(ChunkId);
	}

	TIoStatusOr<FIoBuffer> ReadChunk(const FIoChunkId& ChunkId)
	{
		return DataSource->ReadChunk(ChunkId, FIoReadOptions());
	}

	void ReadChunkAsync(const FIoChunkId& ChunkId, TFunction<void(TIoStatusOr<FIoBuffer>)>&& Callback)
	{
		class FReadChunkTask
			: public FNonAbandonableTask
		{
		public:
			FReadChunkTask(IDataSource* InDataSource, const FIoChunkId& InChunkId, TFunction<void(TIoStatusOr<FIoBuffer>)>&& InCallback)
				: DataSource(InDataSource)
				, ChunkId(InChunkId)
				, Callback(MoveTemp(InCallback))
			{
			}

			void DoWork()
			{
				Callback(DataSource->ReadChunk(ChunkId, FIoReadOptions()));
			}

			TStatId GetStatId() const
			{
				return TStatId();
			}

		private:
			IDataSource* DataSource;
			FIoChunkId ChunkId;
			TFunction<void(TIoStatusOr<FIoBuffer>)> Callback;
		};

		(new FAutoDeleteAsyncTask<FReadChunkTask>(DataSource.Get(), ChunkId, MoveTemp(Callback)))->StartBackgroundTask();
	}
	
	TIoStatusOr<FIoBuffer> ReadPackageHeader(FPackageId PackageId)
	{
		if (const FPackageStoreEntryResource* Entry = PackageIdToEntry.FindRef(PackageId))
		{
			FIoReadOptions ReadOptions;
			ReadOptions.SetRange(0, 64 << 10);
			
			TIoStatusOr<FIoBuffer> Status = DataSource->ReadChunk(CreateIoChunkId(PackageId.Value(), 0, EIoChunkType::ExportBundleData), ReadOptions);
			if (!Status.IsOk())
			{
				return Status;
			}

			FIoBuffer Buffer = Status.ConsumeValueOrDie();
			uint32 HeaderSize = reinterpret_cast<const FZenPackageSummary*>(Buffer.Data())->HeaderSize;
			if (HeaderSize > Buffer.DataSize())
			{
				ReadOptions.SetRange(0, HeaderSize);

				Status = DataSource->ReadChunk(CreateIoChunkId(PackageId.Value(), 0, EIoChunkType::ExportBundleData), ReadOptions);
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
	class IDataSource
	{
	public:
		virtual ~IDataSource() = default;
		virtual TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId) = 0;
		virtual TIoStatusOr<FIoBuffer> ReadChunk(const FIoChunkId& ChunkId, const FIoReadOptions& ReadOptions) = 0;
	};

	class FZenStoreDataSource
		: public IDataSource
	{
	public:
		FZenStoreDataSource(const FPackageStoreManifest::FZenServerInfo& ZenServerInfo)
		{
			ZenStoreClient = MakeUnique<UE::FZenStoreHttpClient>(UE::Zen::FServiceSettings(ZenServerInfo.Settings));

			ZenStoreClient->InitializeReadOnly(ZenServerInfo.ProjectId, ZenServerInfo.OplogId);
		}

		virtual TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& ChunkId) override
		{
			return ZenStoreClient->GetChunkSize(ChunkId);
		}

		virtual TIoStatusOr<FIoBuffer> ReadChunk(const FIoChunkId& ChunkId, const FIoReadOptions& ReadOptions) override
		{
			return ZenStoreClient->ReadChunk(ChunkId, ReadOptions.GetOffset(), ReadOptions.GetSize());
		}

	private:
		TUniquePtr<UE::FZenStoreHttpClient> ZenStoreClient;
	};
	
	TUniquePtr<IDataSource> DataSource;
	FPackageStoreManifest PackageStoreManifest;
	TArray<FPackageStoreManifest::FPackageInfo> ManifestPackageInfos;
	TArray<FPackageStoreEntryResource> PackageEntries;
	TMap<FPackageId, const FPackageStoreEntryResource*> PackageIdToEntry;
	TMap<FString, FIoChunkId> FilenameToChunkIdMap;
	TMap<FIoChunkId, FName> ChunkIdToPackageNameMap;
	TMap<FString, FName> FilenameToPackageNameMap;
	TMap<FName, const FPackageStoreManifest::FPackageInfo*> PackageNameToPackageInfoMap;
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
	FKeyChain ReferenceChunkKeys;
	FReleasedPackages ReleasedPackages;
	TUniquePtr<FCookedPackageStore> PackageStore;
	TUniquePtr<FIoBuffer> ScriptObjects;
	bool bSign = false;
	bool bRemapPluginContentToGame = false;
	bool bCreateDirectoryIndex = true;
	bool bClusterByOrderFilePriority = false;
	bool bFileRegions = false;
	EAssetRegistryWritebackMethod WriteBackMetadataToAssetRegistry = EAssetRegistryWritebackMethod::Disabled;

	bool IsDLC() const
	{
		return DLCPluginPath.Len() > 0;
	}
};

struct FContainerTargetSpec
{
	FIoContainerId ContainerId;
	FIoContainerHeader Header;
	FIoContainerHeader OptionalSegmentHeader;
	FName Name;
	FGuid EncryptionKeyGuid;
	FString OutputPath;
	FString OptionalSegmentOutputPath;
	TSharedPtr<IIoStoreWriter> IoStoreWriter;
	TSharedPtr<IIoStoreWriter> OptionalSegmentIoStoreWriter;
	TArray<FContainerTargetFile> TargetFiles;
	TArray<TUniquePtr<FIoStoreReader>> PatchSourceReaders;
	EIoContainerFlags ContainerFlags = EIoContainerFlags::None;
	TArray<FLegacyCookedPackage*> Packages;
	TSet<FShaderInfo*> GlobalShaders;
	TSet<FShaderInfo*> SharedShaders;
	TSet<FShaderInfo*> UniqueShaders;
	TSet<FShaderInfo*> InlineShaders;
	bool bGenerateDiffPatch = false;
};

using FPackageNameMap = TMap<FName, FLegacyCookedPackage*>;
using FPackageIdMap = TMap<FPackageId, FLegacyCookedPackage*>;

#if OUTPUT_CHUNKID_DIRECTORY
class FChunkIdCsv
{
public:

	~FChunkIdCsv()
	{
		if (OutputArchive)
		{
			OutputArchive->Flush();
		}
	}

	void CreateOutputFile(const FString& RootPath)
	{
		const FString OutputFilename = RootPath / TEXT("chunkid_directory.csv");
		OutputArchive.Reset(IFileManager::Get().CreateFileWriter(*OutputFilename));
		if (OutputArchive)
		{
			const ANSICHAR* Output = "NameIndex,NameNumber,ChunkIndex,ChunkType,ChunkIdHash,DebugString\n";
			OutputArchive->Serialize((void*)Output, FPlatformString::Strlen(Output));
		}
	}

	void AddChunk(uint32 NameIndex, uint32 NameNumber, uint16 ChunkIndex, uint8 ChunkType, uint32 ChunkIdHash, const TCHAR* DebugString)
	{
		ANSICHAR Buffer[MAX_SPRINTF + 1] = { 0 };
		int32 NumOfCharacters = FCStringAnsi::Sprintf(Buffer, "%u,%u,%u,%u,%u,%s\n", NameIndex, NameNumber, ChunkIndex, ChunkType, ChunkIdHash, TCHAR_TO_ANSI(DebugString));
		OutputArchive->Serialize(Buffer, NumOfCharacters);
	}	

private:
	TUniquePtr<FArchive> OutputArchive;
};
FChunkIdCsv ChunkIdCsv;

#endif



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
	const TArray<FLegacyCookedPackage*>& Packages,
	const TArray<FFileOrderMap>& OrderMaps,
	const FPackageIdMap& PackageIdMap,
	bool bClusterByOrderFilePriority
	)
{
	IOSTORE_CPU_SCOPE(AssignPackagesDiskOrder);

	struct FCluster
	{
		TArray<FLegacyCookedPackage*> Packages;
		int32 OrderFileIndex; // Index in OrderMaps of the FFileOrderMap which contained Packages.Last()
		int32 ClusterSequence;

		FCluster(int32 InOrderFileIndex, int32 InClusterSequence)
			: OrderFileIndex(InOrderFileIndex)
			, ClusterSequence(InClusterSequence)
		{
		}
	};

	TArray<FCluster*> Clusters;
	TSet<FLegacyCookedPackage*> AssignedPackages;
	TArray<FLegacyCookedPackage*> ProcessStack;

	struct FPackageAndOrder
	{
		FLegacyCookedPackage* Package = nullptr;
		int64 LocalOrder;
		const FFileOrderMap* OrderMap;

		FPackageAndOrder(FLegacyCookedPackage* InPackage, int64 InLocalOrder, const FFileOrderMap* InOrderMap)
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
	for (FLegacyCookedPackage* Package : Packages)
	{
		// Default to the fallback order map 
		// Reverse the bundle load order for the fallback map (so that packages are considered before their imports)
		const FFileOrderMap* UsedOrderMap = &FallbackOrderMap;
		int64 LocalOrder = -int64(Package->OptimizedPackage->GetLoadOrder());

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

	int32 ClusterSequence = 0;
	TMap<FLegacyCookedPackage*, FCluster*> PackageToCluster;
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
				FLegacyCookedPackage* PackageToProcess = ProcessStack.Pop(false);
				if (!AssignedPackages.Contains(PackageToProcess))
				{
					AssignedPackages.Add(PackageToProcess);
					Cluster->Packages.Add(PackageToProcess);
					PackageToCluster.Add(PackageToProcess, Cluster);
					ClusterBytes += PackageToProcess->UExpSize;
					AssignedUExpSize += PackageToProcess->UExpSize;
					AssignedBulkSize += PackageToProcess->TotalBulkDataSize;
					
					TArray<FPackageId> AllReferencedPackageIds;
					AllReferencedPackageIds.Append(PackageToProcess->OptimizedPackage->GetImportedPackageIds());
					for (const FPackageId& ReferencedPackageId : AllReferencedPackageIds)
					{
						FLegacyCookedPackage* FindReferencedPackage = PackageIdMap.FindRef(ReferencedPackageId);
						if (FindReferencedPackage)
						{
							ProcessStack.Push(FindReferencedPackage);
						}
					}
				}
			}

			for (FLegacyCookedPackage* Package : Cluster->Packages)
			{
				int64 BytesToRead = 0;
				TSet<FCluster*> ClustersToRead;

				TSet<FLegacyCookedPackage*> VisitedDeps;
				TArray<FLegacyCookedPackage*> DepQueue;
				DepQueue.Push(Package);
				while (DepQueue.Num() > 0)
				{
					FLegacyCookedPackage* Cursor = DepQueue.Pop();
					if( VisitedDeps.Contains(Cursor) == false)
					{
						VisitedDeps.Add(Cursor);
						BytesToRead += Cursor->UExpSize;
						if (FCluster* ReadCluster = PackageToCluster.FindRef(Cursor))
						{
							ClustersToRead.Add(ReadCluster);
						}
						
						for (const FPackageId& ImportedPackageId : Cursor->OptimizedPackage->GetImportedPackageIds())
						{
							FLegacyCookedPackage* FindReferencedPackage = PackageIdMap.FindRef(ImportedPackageId);
							if (FindReferencedPackage)
							{
								DepQueue.Push(FindReferencedPackage);
							}
		}
	}
				}

				TArray<FCluster*> OrderedClustersToRead = ClustersToRead.Array();
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
		Algo::Sort(Cluster->Packages, [](const FLegacyCookedPackage* A, const FLegacyCookedPackage* B)
		{
			return A->OptimizedPackage->GetLoadOrder() < B->OptimizedPackage->GetLoadOrder();
		});
	}

	uint64 LayoutIndex = 0;
	for (FCluster* Cluster : Clusters)
	{
		for (FLegacyCookedPackage* Package : Cluster->Packages)
		{
			Package->DiskLayoutOrder = LayoutIndex++;
		}
		delete Cluster;
	}
	
	ClusterStatsCsv.Close();
}

static void CreateDiskLayout(
	const TArray<FContainerTargetSpec*>& ContainerTargets,
	const TArray<FLegacyCookedPackage*>& Packages,
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

FLegacyCookedPackage& FindOrAddPackage(
	const FIoStoreArguments& Arguments,
	const FName& PackageName,
	TArray<FLegacyCookedPackage*>& Packages,
	FPackageNameMap& PackageNameMap,
	FPackageIdMap& PackageIdMap)
{
	FLegacyCookedPackage* Package = PackageNameMap.FindRef(PackageName);
	if (!Package)
	{
		FPackageId PackageId = FPackageId::FromName(PackageName);
		if (FLegacyCookedPackage* FindById = PackageIdMap.FindRef(PackageId))
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

	return *Package;
}

FLegacyCookedPackage* FindOrAddPackageFromFileName(
	const FIoStoreArguments& Arguments,
	const TCHAR* FileName,
	TArray<FLegacyCookedPackage*>& Packages,
	FPackageNameMap& PackageNameMap,
	FPackageIdMap& PackageIdMap)
{
	FName PackageName = Arguments.PackageStore->GetPackageNameFromFileName(FileName);
	if (PackageName.IsNone())
	{
		return nullptr;
	}

	return &FindOrAddPackage(Arguments, PackageName, Packages, PackageNameMap, PackageIdMap);
}

static void ParsePackageAssetsFromFiles(TArray<FLegacyCookedPackage*>& Packages, const FPackageStoreOptimizer& PackageStoreOptimizer)
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
		for (const FLegacyCookedPackage* Package : Packages)
		{
			TotalUAssetSize += Package->UAssetSize;
			TotalOptionalSegmentUAssetSize += Package->OptionalSegmentUAssetSize;
		}
		UAssetMemory = reinterpret_cast<uint8*>(FMemory::Malloc(TotalUAssetSize));
		uint8* UAssetMemoryPtr = UAssetMemory;
		OptionalSegmentUAssetMemory = reinterpret_cast<uint8*>(FMemory::Malloc(TotalOptionalSegmentUAssetSize));
		uint8* OptionalSegmentUAssetMemoryPtr = OptionalSegmentUAssetMemory;

		for (int32 Index = 0; Index < TotalPackageCount; ++Index)
		{
			PackageAssetBuffers[Index] = UAssetMemoryPtr;
			UAssetMemoryPtr += Packages[Index]->UAssetSize;
			OptionalSegmentPackageAssetBuffers[Index] = OptionalSegmentUAssetMemoryPtr;
			OptionalSegmentUAssetMemoryPtr += Packages[Index]->OptionalSegmentUAssetSize;
		}

		TAtomic<uint64> CurrentFileIndex{ 0 };
		ParallelFor(TotalPackageCount, [&ReadCount, &PackageAssetBuffers, &OptionalSegmentPackageAssetBuffers, &Packages, &CurrentFileIndex](int32 Index)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ReadUAssetFile);
			FLegacyCookedPackage* Package = Packages[Index];
			if (Package->UAssetSize)
			{
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
			FLegacyCookedPackage* Package = Packages[Index];

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
		}, EParallelForFlags::Unbalanced);
	}

	FMemory::Free(UAssetMemory);
	FMemory::Free(OptionalSegmentUAssetMemory);
}

static void ParsePackageAssetsFromPackageStore(FCookedPackageStore& PackageStore, TArray<FLegacyCookedPackage*>& Packages, const FPackageStoreOptimizer& PackageStoreOptimizer)
{
	IOSTORE_CPU_SCOPE(ParsePackageAssetsFromPackageStore);
	UE_LOG(LogIoStore, Display, TEXT("Parsing packages..."));

	const int32 TotalPackageCount = Packages.Num();
	
	ParallelFor(TotalPackageCount, [
		&PackageStore,
		&PackageStoreOptimizer,
		&Packages](int32 Index)
	{
		FLegacyCookedPackage* Package = Packages[Index];
		TIoStatusOr<FIoBuffer> HeaderBuffer = PackageStore.ReadPackageHeader(Package->GlobalPackageId);
		Package->UExpSize = Package->UAssetSize - HeaderBuffer.ValueOrDie().DataSize();
		Package->UAssetSize = HeaderBuffer.ValueOrDie().DataSize();
		Package->OptimizedPackage = PackageStoreOptimizer.CreatePackageFromPackageStoreHeader(Package->PackageName, HeaderBuffer.ValueOrDie(), *Package->PackageStoreEntry);
		check(Package->OptimizedPackage->GetId() == Package->GlobalPackageId);
	}, EParallelForFlags::Unbalanced);
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

bool ConvertToIoStoreShaderLibrary(
	const TCHAR* FileName,
	TTuple<FIoChunkId, FIoBuffer>& OutLibraryIoChunk,
	TArray<TTuple<FIoChunkId, FIoBuffer, uint32>>& OutCodeIoChunks,
	TArray<TTuple<FSHAHash, TArray<FIoChunkId>>>& OutShaderMaps,
	TArray<TTuple<FSHAHash, TSet<FName>>>& OutShaderMapAssetAssociations)
{
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
		[&OutCodeIoChunks, &IoStoreLibraryHeader, &DiskArchiveAccess, &LibraryAr, &SerializedShaders, &OffsetToShaderCode](int32 GroupIndex)
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
						ShaderCodeArchive::DecompressShader(ShaderStart, IndividuallyCompressedShader.UncompressedSize, CompressedShaderMemory, IndividuallyCompressedShader.Size);

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
			ShaderCodeArchive::CompressShaderUsingCurrentSettings(nullptr, CompressedGroupSize, UncompressedGroupMemory, Group.UncompressedSize);
			checkf(CompressedGroupSize > 0, TEXT("CompressedGroupSize estimate seems wrong (%lld)"), CompressedGroupSize);

			uint8* CompressedShaderGroupMemory = reinterpret_cast<uint8*>(FMemory::Malloc(CompressedGroupSize));
			bool bCompressed = ShaderCodeArchive::CompressShaderUsingCurrentSettings(CompressedShaderGroupMemory, CompressedGroupSize, UncompressedGroupMemory, Group.UncompressedSize);
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

void ProcessShaderLibraries(const FIoStoreArguments& Arguments, TArray<FContainerTargetSpec*>& ContainerTargets, TArray<FShaderInfo*> OutShaders)
{
	IOSTORE_CPU_SCOPE(ProcessShaderLibraries);

	TMap<FIoChunkId, FShaderInfo*> ChunkIdToShaderInfoMap;
	TMap<FSHAHash, TArray<FIoChunkId>> ShaderChunkIdsByShaderMapHash;
	TMap<FName, TSet<FSHAHash>> PackageNameToShaderMaps;
	TMap<FContainerTargetSpec*, TSet<FShaderInfo*>> AllContainerShaderLibraryShadersMap;

	for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		TArray<FShaderInfo> Shaders;
		for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
		{
			if (TargetFile.ChunkType == EContainerChunkType::ShaderCodeLibrary)
			{
				TSet<FShaderInfo*>& ContainerShaderLibraryShaders = AllContainerShaderLibraryShadersMap.FindOrAdd(ContainerTarget);

				TArray<TTuple<FSHAHash, TArray<FIoChunkId>>> ShaderMaps;
				TTuple<FIoChunkId, FIoBuffer> LibraryChunk;
				TArray<TTuple<FIoChunkId, FIoBuffer, uint32>> CodeChunks;
				TArray<TTuple<FSHAHash, TSet<FName>>> ShaderMapAssetAssociations;
				if (!ConvertToIoStoreShaderLibrary(*TargetFile.NormalizedSourcePath, LibraryChunk, CodeChunks, ShaderMaps, ShaderMapAssetAssociations))
				{
					UE_LOG(LogIoStore, Warning, TEXT("Failed converting shader library '%s'"), *TargetFile.NormalizedSourcePath);
					continue;
				}
				TargetFile.ChunkId = LibraryChunk.Key;
				TargetFile.SourceBuffer.Emplace(LibraryChunk.Value);
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
					ShaderChunkIdsByShaderMapHash.Add(ShaderMap.Key, MoveTemp(ShaderMap.Value));
				}
			} // end if containerchunktype shadercodelibrary
		} // end foreach targetfile
	} // end foreach container

	// 1. Update ShaderInfos with which packages we reference.
	// 2. Add to packages which shaders we use.
	// 3. Add to PackageStore what shaders we use.
	for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		for (FLegacyCookedPackage* Package : ContainerTarget->Packages)
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
					Package->OptimizedPackage->AddShaderMapHash(ShaderMapHash);
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
		}
	}

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
			for (FLegacyCookedPackage* Package : ContainerTarget->Packages)
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
				FShaderInfo::EShaderType* ShaderType = ShaderInfo->TypeInContainer.Find(ContainerTarget);
				check(ShaderType);
				if (*ShaderType == FShaderInfo::Global)
				{
					ContainerTarget->GlobalShaders.Add(ShaderInfo);
				}
				else if (*ShaderType == FShaderInfo::Inline)
				{
					ContainerTarget->InlineShaders.Add(ShaderInfo);
				}
				else if (ShaderInfo->ReferencedByPackages.Num() > 1)
				{
					ContainerTarget->SharedShaders.Add(ShaderInfo);
				}
				else
				{
					// If there are unreferenced shaders they will go in here and be sorted last
					ContainerTarget->UniqueShaders.Add(ShaderInfo);
				}
				AddShaderTargetFile(ShaderInfo);
			}
		}
	}
}

void InitializeContainerTargetsAndPackages(
	const FIoStoreArguments& Arguments,
	TArray<FLegacyCookedPackage*>& Packages,
	FPackageNameMap& PackageNameMap,
	FPackageIdMap& PackageIdMap,
	TArray<FContainerTargetSpec*>& ContainerTargets,
	FPackageStoreOptimizer& PackageStoreOptimizer)
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
			OutTargetFile.Package = FindOrAddPackageFromFileName(Arguments, *SourceFile.NormalizedPath, Packages, PackageNameMap, PackageIdMap);
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
				OutTargetFile.Package->FileName = SourceFile.NormalizedPath; // .uasset path
				OutTargetFile.Package->UAssetSize = OriginalCookedFileStatData->FileSize;
				OutTargetFile.Package->UExpSize = CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::BulkData:
				OutTargetFile.ChunkType = EContainerChunkType::BulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::BulkData);
				OutTargetFile.Package->TotalBulkDataSize += CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::OptionalBulkData:
				OutTargetFile.ChunkType = EContainerChunkType::OptionalBulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::OptionalBulkData);
				OutTargetFile.Package->TotalBulkDataSize += CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::MemoryMappedBulkData:
				OutTargetFile.ChunkType = EContainerChunkType::MemoryMappedBulkData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::MemoryMappedBulkData);
				OutTargetFile.Package->TotalBulkDataSize += CookedFileStatData->FileSize;
				break;
			case FCookedFileStatData::OptionalSegmentPackageData:
				OutTargetFile.ChunkType = EContainerChunkType::OptionalSegmentPackageData;
				OutTargetFile.ChunkId = CreateIoChunkId(OutTargetFile.Package->GlobalPackageId.Value(), 1, EIoChunkType::ExportBundleData);
				OutTargetFile.Package->OptionalSegmentFileName = SourceFile.NormalizedPath; // .o.uasset path
				OutTargetFile.Package->OptionalSegmentUAssetSize = OriginalCookedFileStatData->FileSize;
				OutTargetFile.Package->OptionalSegmentUExpSize = CookedFileStatData->FileSize;
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

	auto CreateTargetFileFromPackageStore = [
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

		OutTargetFile.ChunkId = PackageStore.GetChunkIdFromFileName(SourceFile.NormalizedPath);
		if (!OutTargetFile.ChunkId.IsValid())
		{
			UE_LOG(LogIoStore, Warning, TEXT("File not found in manifest: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}

		TIoStatusOr<uint64> ChunkSize = PackageStore.GetChunkSize(OutTargetFile.ChunkId);
		if (!ChunkSize.IsOk())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Chunk size not found for: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}
		OutTargetFile.SourceSize = ChunkSize.ValueOrDie();

		FName PackageName = PackageStore.GetPackageNameFromChunkId(OutTargetFile.ChunkId);
		if (PackageName.IsNone())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Package name not found for: '%s'"), *SourceFile.NormalizedPath);
			return false;
		}

		OutTargetFile.Package = &FindOrAddPackage(Arguments, PackageName, Packages, PackageNameMap, PackageIdMap);
		OutTargetFile.Package->PackageStoreEntry = PackageStore.GetPackageStoreEntry(OutTargetFile.Package->GlobalPackageId);
		if (!OutTargetFile.Package->PackageStoreEntry)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to find package store entry for package: '%s'"), *PackageName.ToString());
			return false;
		}

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
			OutTargetFile.Package->FileName = SourceFile.NormalizedPath;
			OutTargetFile.Package->UAssetSize = OutTargetFile.SourceSize;
		}
		else if (Extension == TEXT(".o.uasset") || Extension == TEXT(".o.umap"))
		{
			OutTargetFile.ChunkType = EContainerChunkType::OptionalSegmentPackageData;
			OutTargetFile.Package->OptionalSegmentFileName = SourceFile.NormalizedPath;
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
		// TODO: There are no region files when cooking for Zen
		//if (Arguments.bFileRegions && !SourceFile.bNeedsCompression && !SourceFile.bNeedsEncryption)
		//{
		//	FString RegionsFilename = OutTargetFile.ChunkType == EContainerChunkType::PackageData
		//		? FPaths::ChangeExtension(SourceFile.NormalizedPath, FString(TEXT(".uexp")) + FFileRegion::RegionsFileExtension)
		//		: SourceFile.NormalizedPath + FFileRegion::RegionsFileExtension;
		//	TUniquePtr<FArchive> RegionsFile(IFileManager::Get().CreateFileReader(*RegionsFilename));
		//	if (RegionsFile.IsValid())
		//	{
		//		FFileRegion::SerializeFileRegions(*RegionsFile.Get(), OutTargetFile.FileRegions);
		//	}
		//}
		
		return true;
	};
	
	for (const FContainerSourceSpec& ContainerSource : Arguments.Containers)
	{
		FContainerTargetSpec* ContainerTarget = AddContainer(ContainerSource.Name, ContainerTargets);
		ContainerTarget->OutputPath = ContainerSource.OutputPath;
		ContainerTarget->bGenerateDiffPatch = ContainerSource.bGenerateDiffPatch;
		if (Arguments.bSign)
		{
			ContainerTarget->ContainerFlags |= EIoContainerFlags::Signed;
		}

		if (!ContainerTarget->EncryptionKeyGuid.IsValid())
		{
			ContainerTarget->EncryptionKeyGuid = ContainerSource.EncryptionKeyOverrideGuid;
		}

		ContainerTarget->PatchSourceReaders = CreatePatchSourceReaders(ContainerSource.PatchSourceContainerFiles, Arguments);

		{
			IOSTORE_CPU_SCOPE(ProcessSourceFiles);
			bool bHasOptionalSegmentPackages = false;
			for (const FContainerSourceFile& SourceFile : ContainerSource.SourceFiles)
			{
				FContainerTargetFile TargetFile;
				bool bIsValidTargetFile = Arguments.PackageStore->HasDataSource()
					? CreateTargetFileFromPackageStore(SourceFile, TargetFile)
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

				UE_LOG(LogIoStore, Display, TEXT("Saving optional container to: '%s'"), *ContainerTarget->OptionalSegmentOutputPath);
			}
		}
	}

	Algo::Sort(Packages, [](const FLegacyCookedPackage* A, const FLegacyCookedPackage* B)
	{
		return A->GlobalPackageId < B->GlobalPackageId;
	});
};

void LogWriterResults(const TArray<FIoStoreWriterResult>& Results)
{
	UE_LOG(LogIoStore, Display, TEXT("--------------------------------------------------- IoDispatcher --------------------------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-30s %10s %15s %15s %15s %25s"),
		TEXT("Container"), TEXT("Flags"), TEXT("TOC Size (KB)"), TEXT("TOC Entries"), TEXT("Size (MB)"), TEXT("Compressed (MB)"));
	UE_LOG(LogIoStore, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));
	uint64 TotalTocSize = 0;
	uint64 TotalTocEntryCount = 0;
	uint64 TotalUncompressedContainerSize = 0;
	uint64 TotalPaddingSize = 0;
	for (const FIoStoreWriterResult& Result : Results)
	{
		FString CompressionInfo = TEXT("-");

		if (Result.CompressionMethod != NAME_None)
		{
			double Procentage = (double(Result.UncompressedContainerSize - Result.CompressedContainerSize) / double(Result.UncompressedContainerSize)) * 100.0;
			CompressionInfo = FString::Printf(TEXT("%.2lf (%.2lf%% %s)"),
				(double)Result.CompressedContainerSize / 1024.0 / 1024.0,
				Procentage,
				*Result.CompressionMethod.ToString());
		}

		FString ContainerSettings = FString::Printf(TEXT("%s/%s/%s/%s"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Compressed) ? TEXT("C") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Encrypted) ? TEXT("E") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Signed) ? TEXT("S") : TEXT("-"),
			EnumHasAnyFlags(Result.ContainerFlags, EIoContainerFlags::Indexed) ? TEXT("I") : TEXT("-"));

		UE_LOG(LogIoStore, Display, TEXT("%-30s %10s %15.2lf %15llu %15.2lf %25s"),
			*Result.ContainerName,
			*ContainerSettings,
			(double)Result.TocSize / 1024.0,
			Result.TocEntryCount,
			(double)Result.UncompressedContainerSize / 1024.0 / 1024.0,
			*CompressionInfo);


		TotalTocSize += Result.TocSize;
		TotalTocEntryCount += Result.TocEntryCount;
		TotalUncompressedContainerSize += Result.UncompressedContainerSize;
		TotalPaddingSize += Result.PaddingSize;
	}

	UE_LOG(LogIoStore, Display, TEXT("%-30s %10s %15.2lf %15llu %15.2lf %25s"),
		TEXT("TOTAL"),
		TEXT(""),
		(double)TotalTocSize / 1024.0,
		TotalTocEntryCount,
		(double)TotalUncompressedContainerSize / 1024.0 / 1024.0,
		TEXT("-"));

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("** Flags: (C)ompressed / (E)ncrypted / (S)igned) / (I)ndexed) **"));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("Compression block padding: %8.2lf MB"), (double)TotalPaddingSize / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT(""));

	UE_LOG(LogIoStore, Display, TEXT("-------------------------------------------- Container Directory Index --------------------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT("%-30s %15s"), TEXT("Container"), TEXT("Size (KB)"));
	for (const FIoStoreWriterResult& Result : Results)
	{
		UE_LOG(LogIoStore, Display, TEXT("%-30s %15.2lf"), *Result.ContainerName, double(Result.DirectoryIndexSize) / 1024.0);
	}

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("---------------------------------------------- Container Patch Report ---------------------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT("%-30s %16s %16s %16s %16s %16s"), TEXT("Container"), TEXT("Total (count)"), TEXT("Modified (count)"), TEXT("Added (count)"), TEXT("Modified (MB)"), TEXT("Added (MB)"));
	for (const FIoStoreWriterResult& Result : Results)
	{
		UE_LOG(LogIoStore, Display, TEXT("%-30s %16d %16d %16d %16.2lf %16.2lf"), *Result.ContainerName, Result.TocEntryCount, Result.ModifiedChunksCount, Result.AddedChunksCount, Result.ModifiedChunksSize / 1024.0 / 1024.0, Result.AddedChunksSize / 1024.0 / 1024.0);
	}
}

void LogContainerPackageInfo(const TArray<FContainerTargetSpec*>& ContainerTargets)
{
	uint64 TotalStoreSize = 0;
	uint64 TotalPackageCount = 0;
	uint64 TotalLocalizedPackageCount = 0;

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("--------------------------------------------------- PackageStore (KB) ---------------------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-30s %20s %20s %20s"),
		TEXT("Container"),
		TEXT("Store Size"),
		TEXT("Packages"),
		TEXT("Localized"));
	UE_LOG(LogIoStore, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));

	for (const FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		uint64 StoreSize = ContainerTarget->Header.StoreEntries.Num();
		uint64 PackageCount = ContainerTarget->Packages.Num();
		uint64 LocalizedPackageCount = ContainerTarget->Header.LocalizedPackages.Num();

		UE_LOG(LogIoStore, Display, TEXT("%-30s %20.0lf %20llu %20llu"),
			*ContainerTarget->Name.ToString(),
			(double)StoreSize / 1024.0,
			PackageCount,
			LocalizedPackageCount);

		TotalStoreSize += StoreSize;
		TotalPackageCount += PackageCount;
		TotalLocalizedPackageCount += LocalizedPackageCount;
	}
	UE_LOG(LogIoStore, Display, TEXT("%-30s %20.0lf %20llu %20llu"),
		TEXT("TOTAL"),
		(double)TotalStoreSize / 1024.0,
		TotalPackageCount,
		TotalLocalizedPackageCount);


	uint64 TotalHeaderSize = 0;
	uint64 TotalGraphSize = 0;
	uint64 TotalExportBundleEntriesSize = 0;
	uint64 TotalImportMapSize = 0;
	uint64 TotalExportMapSize = 0;
	uint64 TotalNameMapSize = 0;

	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("--------------------------------------------------- PackageHeader (KB) --------------------------------------------------"));
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("%-30s %13s %13s %13s %13s %13s %13s"),
		TEXT("Container"),
		TEXT("Header"),
		TEXT("Graph"),
		TEXT("ExportBundleEntries"),
		TEXT("ImportMap"),
		TEXT("ExportMap"),
		TEXT("NameMap"));
	UE_LOG(LogIoStore, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));
	for (const FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		uint64 HeaderSize = 0;
		uint64 GraphSize = 0;
		uint64 ExportBundleEntriesSize = 0;
		uint64 ImportMapSize = 0;
		uint64 ExportMapSize = 0;
		uint64 NameMapSize = 0;

		for (const FLegacyCookedPackage* Package : ContainerTarget->Packages)
		{
			HeaderSize += Package->OptimizedPackage->GetHeaderSize();
			GraphSize += Package->OptimizedPackage->GetGraphDataSize();
			ExportBundleEntriesSize += Package->OptimizedPackage->GetExportBundleEntriesSize();
			ImportMapSize += Package->OptimizedPackage->GetImportMapSize();
			ExportMapSize += Package->OptimizedPackage->GetExportMapSize();
			NameMapSize += Package->OptimizedPackage->GetNameMapSize();
		}

		UE_LOG(LogIoStore, Display, TEXT("%-30s %13.0lf %13.0lf %13.0lf %13.0lf %13.0lf %13.0lf"),
			*ContainerTarget->Name.ToString(),
			(double)HeaderSize / 1024.0,
			(double)GraphSize / 1024.0,
			(double)ExportBundleEntriesSize / 1024.0,
			(double)ImportMapSize / 1024.0,
			(double)ExportMapSize / 1024.0,
			(double)NameMapSize / 1024.0);

		TotalHeaderSize += HeaderSize;
		TotalGraphSize += GraphSize;
		TotalExportBundleEntriesSize += ExportBundleEntriesSize;
		TotalImportMapSize += ImportMapSize;
		TotalExportMapSize += ExportMapSize;
		TotalNameMapSize += NameMapSize;
	}

	UE_LOG(LogIoStore, Display, TEXT("%-30s %13.0lf %13.0lf %13.0lf %13.0lf %13.0lf %13.0lf"),
		TEXT("TOTAL"),
		(double)TotalHeaderSize / 1024.0,
		(double)TotalGraphSize / 1024.0,
		(double)TotalExportBundleEntriesSize / 1024.0,
		(double)TotalImportMapSize / 1024.0,
		(double)TotalExportMapSize / 1024.0,
		(double)TotalNameMapSize / 1024.0);

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
		else if (PackageStore->HasDataSource())
		{
			return new FCookedPackageStoreWriteRequest(*this, InTargetFile);
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
		uint64 SourceBufferSize;
		FGraphEventRef CompletionEvent;
		FIoBuffer SourceBuffer;
		bool bHasUpdatedExportBundleRegions = false;
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
			SourceBuffer = TargetFile.SourceBuffer.GetValue();
			OnSourceBufferLoaded();
		}
	};

	// Used when staging from cooked files
	class FLooseFileWriteRequest
		: public FWriteContainerTargetFileRequest
	{
	public:
		FLooseFileWriteRequest(FIoStoreWriteRequestManager& InManager,const FContainerTargetFile& InTargetFile)
			: FWriteContainerTargetFileRequest(InManager, InTargetFile) { }

		virtual void LoadSourceBufferAsync() override
		{
			SourceBuffer = FIoBuffer(GetSourceBufferSize());

			QueueEntry->FileHandle.Reset(
				FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*TargetFile.NormalizedSourcePath));
			
			QueueEntry->AddRef(); // Must keep it around until we've assigned the ReadRequest pointer
			FAsyncFileCallBack Callback = [this](bool, IAsyncReadRequest* ReadRequest)
			{
				if (TargetFile.ChunkType == EContainerChunkType::PackageData)
				{
					SourceBuffer = Manager.PackageStoreOptimizer.CreatePackageBuffer(TargetFile.Package->OptimizedPackage, SourceBuffer, bHasUpdatedExportBundleRegions ? nullptr : &FileRegions);
					bHasUpdatedExportBundleRegions = true;
				}
				else if (TargetFile.ChunkType == EContainerChunkType::OptionalSegmentPackageData)
				{
					check(TargetFile.Package->OptimizedOptionalSegmentPackage);
					SourceBuffer = Manager.PackageStoreOptimizer.CreatePackageBuffer(TargetFile.Package->OptimizedOptionalSegmentPackage, SourceBuffer, bHasUpdatedExportBundleRegions ? nullptr : &FileRegions);
					bHasUpdatedExportBundleRegions = true;
				}
				OnSourceBufferLoaded();
			};

			QueueEntry->ReadRequest.Reset(
				QueueEntry->FileHandle->ReadRequest(0, SourceBuffer.DataSize(), AIOP_Normal, &Callback, SourceBuffer.Data()));
			QueueEntry->ReleaseRef(Manager);
		}
	};

	// Used when cooking directly to I/O store container file
	class FCookedPackageStoreWriteRequest
		: public FWriteContainerTargetFileRequest
	{
	public:
		FCookedPackageStoreWriteRequest(FIoStoreWriteRequestManager& InManager,const FContainerTargetFile& InTargetFile)
			: FWriteContainerTargetFileRequest(InManager, InTargetFile) {}

		virtual void LoadSourceBufferAsync() override
		{
			Manager.PackageStore->ReadChunkAsync(
				TargetFile.ChunkId,
				[this](TIoStatusOr<FIoBuffer> Status)
				{
					SourceBuffer = Status.ConsumeValueOrDie();
					if (TargetFile.ChunkType == EContainerChunkType::PackageData)
					{
						check(TargetFile.Package->UAssetSize > 0);
						const uint64 HeaderSize = TargetFile.Package->UAssetSize;
						FIoBuffer ExportsBuffer(SourceBuffer.Data() + HeaderSize, SourceBuffer.DataSize() - HeaderSize, SourceBuffer);
						SourceBuffer = Manager.PackageStoreOptimizer.CreatePackageBuffer(TargetFile.Package->OptimizedPackage, ExportsBuffer, bHasUpdatedExportBundleRegions ? nullptr : &FileRegions);
						bHasUpdatedExportBundleRegions = true;
					}
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

	static constexpr uint64 BufferMemoryLimit = 2ull << 30;
};

static void AddChunkInfoToAssetRegistry(TMap<FPackageId, TArray<FIoStoreTocChunkInfo, TInlineAllocator<2>>>&& PackageToChunks, FAssetRegistryState& AssetRegistry, uint64 TotalCompressedSize)
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
		if (AssetPackage.Value->DiskSize == -1)
		{
			// No data on disk!
			continue;
		}

		const FAssetData* AssetData = UE::AssetRegistry::GetMostImportantAsset(AssetRegistry.GetAssetsByPackageName(AssetPackage.Key), false);
		if (AssetData == nullptr)
		{
			// e.g. /Script packages.
			continue;
		}

		const TArray<FIoStoreTocChunkInfo, TInlineAllocator<2>>* PackageChunks = PackageToChunks.Find(FPackageId::FromName(AssetPackage.Key));
		if (PackageChunks == nullptr)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Package data for %s had no chunks %d -- %lld "), *AssetPackage.Key.ToString(), AssetPackage.Value->ChunkHashes.Num(), AssetPackage.Value->DiskSize);
			continue;
		}

		int32 ChunkCount = 0;
		int64 Size = 0;
		int64 CompressedSize = 0;
		for (const FIoStoreTocChunkInfo& ChunkInfo : *PackageChunks)
		{
			ChunkCount++;
			Size += ChunkInfo.Size;
			CompressedSize += ChunkInfo.CompressedSize;
		}

		FAssetDataTagMap TagsAndValues;
		TagsAndValues.Add("Stage_ChunkCount", LexToString(ChunkCount));
		TagsAndValues.Add("Stage_ChunkSize", LexToString(Size));
		TagsAndValues.Add("Stage_ChunkCompressedSize", LexToString(CompressedSize));
		AssetRegistry.AddTagsToAssetData(AssetData->GetSoftObjectPath(), MoveTemp(TagsAndValues));

		// We assign a package's chunks to a single asset, remove it from the list so that
		// at the end we can track how many chunks don't get assigned.
		PackageToChunks.Remove(FPackageId::FromName(AssetPackage.Key));

		UpdatedAssetCount++;
		AssetsCompressedSize += CompressedSize;
	}
	
	// PackageToChunks now has chunks that we never assigned to an asset, and so aren't accounted for.
	uint64 RemainingByType[(uint8)EIoChunkType::MAX] = {};
	for (auto PackageChunks : PackageToChunks)
	{
		for (FIoStoreTocChunkInfo& Info : PackageChunks.Value)
		{
			RemainingByType[(uint8)Info.ChunkType] += Info.CompressedSize;
		}
	}

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

static bool LoadAssetRegistry(const FString& InAssetRegistryFileName, FAssetRegistryState& OutAssetRegistry)
{
	FAssetRegistryVersion::Type Version;
	FAssetRegistryLoadOptions Options(UE::AssetRegistry::ESerializationTarget::ForDevelopment);
	bool bSucceeded = FAssetRegistryState::LoadFromDisk(*InAssetRegistryFileName, Options, OutAssetRegistry, &Version);
	return bSucceeded;
}

static bool SaveAssetRegistry(const FString& InAssetRegistryFileName, FAssetRegistryState& InAssetRegistry, bool InSaveTempAndRename)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SavingAssetRegistry);
	FLargeMemoryWriter SerializedAssetRegistry;
	if (InAssetRegistry.Save(SerializedAssetRegistry, FAssetRegistrySerializationOptions(UE::AssetRegistry::ESerializationTarget::ForDevelopment)) == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to serialize asset registry to memory."));
		return false;
	}

	FString OutputFileName = InSaveTempAndRename ? (InAssetRegistryFileName + TEXT(".temp")) : InAssetRegistryFileName;

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

	if (InSaveTempAndRename)
	{
		// Move our temp file over the original asset registry.
		if (IFileManager::Get().Move(*InAssetRegistryFileName, *OutputFileName) == false)
		{
			// Error already logged by FileManager
			return false;
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Saved asset registry to disk. (%s)"), *InAssetRegistryFileName);

	return true;
}

int32 DoAssetRegistryWritebackAfterStage(const FString& InAssetRegistryFileName, FString&& InContainerDirectory, const FKeyChain& InKeyChain)
{
	// This version called after the containers are already created, when you
	// have a bunch of containers on disk and you want to add chunk info back to
	// an asset registry.

	FAssetRegistryState AssetRegistry;
	if (LoadAssetRegistry(InAssetRegistryFileName, AssetRegistry) == false)
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
	TMap<FPackageId, TArray<FIoStoreTocChunkInfo, TInlineAllocator<2>>> PackageToChunks;
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
			PackageToChunks.FindOrAdd(PackageId).Add(ChunkInfo);
			TotalCompressedSize += ChunkInfo.CompressedSize;
			return true;
		});
	}

	AddChunkInfoToAssetRegistry(MoveTemp(PackageToChunks), AssetRegistry, TotalCompressedSize);

	return SaveAssetRegistry(InAssetRegistryFileName, AssetRegistry, true) ? 0 : 1;
}

bool DoAssetRegistryWritebackDuringStage(EAssetRegistryWritebackMethod InMethod, const FString& InCookedDir, TArray<TSharedPtr<IIoStoreWriter>>& InIoStoreWriters)
{
	// This version called during container creation.

	TRACE_CPUPROFILER_EVENT_SCOPE(UpdateAssetRegistryWithSizeInfo);
	UE_LOG(LogIoStore, Display, TEXT("Adding staging metadata to asset registry..."));

	// The overwhelming majority of time for the asset registry writeback is loading and saving.
	FString AssetRegistryFileName;
	FAssetRegistryState AssetRegistry;
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
			UE_LOG(LogIoStore, Error, TEXT("No development asset registry file found!"));
			return false;
		}
		else
		{
			AssetRegistryFileName = MoveTemp(PossibleAssetRegistryFiles[0]);
			UE_LOG(LogIoStore, Display, TEXT("Using input asset registry: %s"), *AssetRegistryFileName);

			if (LoadAssetRegistry(AssetRegistryFileName, AssetRegistry) == false)
			{
				return false; // already logged
			}
		}
	}

	// Create a map off the package id to all of its chunks. 2 inline allocation
	// is for the export data and the bulk data. For a major test project, 2 covers
	// 89% of packages, 1 covers 72%.
	uint64 TotalCompressedSize = 0;
	TMap<FPackageId, TArray<FIoStoreTocChunkInfo, TInlineAllocator<2>>> PackageToChunks;
	for (TSharedPtr<IIoStoreWriter> IoStoreWriter : InIoStoreWriters)
	{
		IoStoreWriter->EnumerateChunks([&](const FIoStoreTocChunkInfo& ChunkInfo)
		{
			FPackageId PackageId = FPackageId::FromValue(*(int64*)(ChunkInfo.Id.GetData()));
			PackageToChunks.FindOrAdd(PackageId).Add(ChunkInfo);
			TotalCompressedSize += ChunkInfo.CompressedSize;
			return true;
		});
	}

	AddChunkInfoToAssetRegistry(MoveTemp(PackageToChunks), AssetRegistry, TotalCompressedSize);

	FString OutputFileName;
	switch (InMethod)
	{
	case EAssetRegistryWritebackMethod::OriginalFile:
		{
			// Write to an adjacent file and move after
			if (SaveAssetRegistry(AssetRegistryFileName, AssetRegistry, true) == false)
			{
				return false;
			}
			break;
		}
	case EAssetRegistryWritebackMethod::AdjacentFile:
		{
			if (SaveAssetRegistry(AssetRegistryFileName.Replace(TEXT(".bin"), TEXT("Staged.bin")), AssetRegistry, true) == false)
			{
				return false;
			}
			break;
		}
	default:
		{
			UE_LOG(LogIoStore, Error, TEXT("Invalid asset registry writeback method (should already be handled!) (%d)"), InMethod);
			return false;
		}
	}
	
	return true;
}

int32 CreateTarget(const FIoStoreArguments& Arguments, const FIoStoreWriterSettings& GeneralIoWriterSettings)
{
	TGuardValue<int32> GuardAllowUnversionedContentInEditor(GAllowUnversionedContentInEditor, 1);

#if OUTPUT_CHUNKID_DIRECTORY
	ChunkIdCsv.CreateOutputFile(CookedDir);
#endif

	TSharedPtr<IIoStoreWriterReferenceChunkDatabase> ChunkDatabase;
	if (Arguments.ReferenceChunkGlobalContainerFileName.Len())
	{
		ChunkDatabase = MakeShared<FIoStoreChunkDatabase>();
		if (((FIoStoreChunkDatabase&)*ChunkDatabase).Init(Arguments.ReferenceChunkGlobalContainerFileName, Arguments.ReferenceChunkKeys) == false)
		{
			UE_LOG(LogIoStore, Error, TEXT("Failed to initialize reference chunk store."));
			return 1;
		}
	}

	TArray<FLegacyCookedPackage*> Packages;
	FPackageNameMap PackageNameMap;
	FPackageIdMap PackageIdMap;

	FPackageStoreOptimizer PackageStoreOptimizer;
	PackageStoreOptimizer.Initialize(*Arguments.ScriptObjects);
	FIoStoreWriteRequestManager WriteRequestManager(PackageStoreOptimizer, Arguments.PackageStore.Get());

	TArray<FContainerTargetSpec*> ContainerTargets;
	UE_LOG(LogIoStore, Display, TEXT("Creating container targets..."));
	{
		IOSTORE_CPU_SCOPE(CreateContainerTargets);
		InitializeContainerTargetsAndPackages(Arguments, Packages, PackageNameMap, PackageIdMap, ContainerTargets, PackageStoreOptimizer);
	}

	TUniquePtr<FIoStoreWriterContext> IoStoreWriterContext(new FIoStoreWriterContext());
	FIoStatus IoStatus = IoStoreWriterContext->Initialize(GeneralIoWriterSettings);
	check(IoStatus.IsOk());
	TArray<TSharedPtr<IIoStoreWriter>> IoStoreWriters;
	TSharedPtr<IIoStoreWriter> GlobalIoStoreWriter;
	{
		IOSTORE_CPU_SCOPE(InitializeIoStoreWriters);
		if (!Arguments.IsDLC())
		{
			FIoContainerSettings GlobalContainerSettings;
			if (Arguments.bSign)
			{
				GlobalContainerSettings.SigningKey = Arguments.KeyChain.GetSigningKey();
				GlobalContainerSettings.ContainerFlags |= EIoContainerFlags::Signed;
			}
			GlobalIoStoreWriter = IoStoreWriterContext->CreateContainer(*Arguments.GlobalContainerPath, GlobalContainerSettings);
			IoStoreWriters.Add(GlobalIoStoreWriter);
		}
		for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
		{
			check(ContainerTarget->ContainerId.IsValid());
			if (!ContainerTarget->OutputPath.IsEmpty())
			{
				FIoContainerSettings ContainerSettings;
				ContainerSettings.ContainerId = ContainerTarget->ContainerId;
				if (Arguments.bCreateDirectoryIndex)
				{
					ContainerSettings.ContainerFlags = ContainerTarget->ContainerFlags | EIoContainerFlags::Indexed;
				}
				if (EnumHasAnyFlags(ContainerTarget->ContainerFlags, EIoContainerFlags::Encrypted))
				{
					const FNamedAESKey* Key = Arguments.KeyChain.GetEncryptionKeys().Find(ContainerTarget->EncryptionKeyGuid);
					check(Key);
					ContainerSettings.EncryptionKeyGuid = ContainerTarget->EncryptionKeyGuid;
					ContainerSettings.EncryptionKey = Key->Key;
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
				IoStoreWriters.Add(ContainerTarget->IoStoreWriter);
				if (!ContainerTarget->OptionalSegmentOutputPath.IsEmpty())
				{
					ContainerTarget->OptionalSegmentIoStoreWriter = IoStoreWriterContext->CreateContainer(*ContainerTarget->OptionalSegmentOutputPath, ContainerSettings);
					IoStoreWriters.Add(ContainerTarget->OptionalSegmentIoStoreWriter);
				}
			}
		}
	}

	if (Arguments.PackageStore->HasDataSource())
	{
		ParsePackageAssetsFromPackageStore(*Arguments.PackageStore, Packages, PackageStoreOptimizer);
	}
	else
	{
		ParsePackageAssetsFromFiles(Packages, PackageStoreOptimizer);
	}

	UE_LOG(LogIoStore, Display, TEXT("Processing shader libraries..."));
	TArray<FShaderInfo*> Shaders;
	ProcessShaderLibraries(Arguments, ContainerTargets, Shaders);

	if (Arguments.IsDLC() && Arguments.bRemapPluginContentToGame)
	{
		for (FLegacyCookedPackage* Package : Packages)
		{
			const int32 DLCNameLen = Arguments.DLCName.Len() + 1;
			FString PackageNameStr = Package->PackageName.ToString();
			FString RedirectedPackageNameStr = TEXT("/Game");
			RedirectedPackageNameStr.AppendChars(*PackageNameStr + DLCNameLen, PackageNameStr.Len() - DLCNameLen);
			FName RedirectedPackageName = FName(*RedirectedPackageNameStr);
			Package->OptimizedPackage->RedirectFrom(RedirectedPackageName);
		}
	}

	for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		if (ContainerTarget->IoStoreWriter)
		{
			for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
			{
				if (TargetFile.ChunkType != EContainerChunkType::PackageData && TargetFile.ChunkType != EContainerChunkType::OptionalSegmentPackageData)
				{
					FIoWriteOptions WriteOptions;
					WriteOptions.DebugName = *TargetFile.DestinationPath;
					WriteOptions.bForceUncompressed = TargetFile.bForceUncompressed;
					WriteOptions.bIsMemoryMapped = TargetFile.ChunkType == EContainerChunkType::MemoryMappedBulkData;
					WriteOptions.FileName = TargetFile.DestinationPath;
					if (TargetFile.ChunkType == EContainerChunkType::OptionalSegmentBulkData)
					{
						FIoChunkId ChunkId = TargetFile.ChunkId;
						if (TargetFile.Package->OptimizedOptionalSegmentPackage && TargetFile.Package->OptimizedOptionalSegmentPackage->HasEditorData())
						{
							// Auto optional packages replace the non-optional part when the container is mounted
							ChunkId = CreateIoChunkId(TargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::BulkData);
						}
						ContainerTarget->OptionalSegmentIoStoreWriter->Append(ChunkId, WriteRequestManager.Read(TargetFile), WriteOptions);
					}
					else
					{
						ContainerTarget->IoStoreWriter->Append(TargetFile.ChunkId, WriteRequestManager.Read(TargetFile), WriteOptions);
					}
				}
			}
		}
	}

	TMap<FPackageId, FPackageStorePackage*> OptimizedPackagesMap;
	for (FLegacyCookedPackage* Package : Packages)
	{
		check(Package->OptimizedPackage);
		OptimizedPackagesMap.Add(Package->OptimizedPackage->GetId(), Package->OptimizedPackage);
	}

	UE_LOG(LogIoStore, Display, TEXT("Processing redirects..."));
	PackageStoreOptimizer.ProcessRedirects(OptimizedPackagesMap, Arguments.IsDLC());

	UE_LOG(LogIoStore, Display, TEXT("Optimizing packages..."));
	PackageStoreOptimizer.OptimizeExportBundles(OptimizedPackagesMap);

	UE_LOG(LogIoStore, Display, TEXT("Finalizing packages..."));
	for (FLegacyCookedPackage* Package : Packages)
	{
		check(Package->OptimizedPackage);
		PackageStoreOptimizer.FinalizePackage(Package->OptimizedPackage);
		if (Package->OptimizedOptionalSegmentPackage)
		{
			PackageStoreOptimizer.FinalizePackage(Package->OptimizedOptionalSegmentPackage);
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Creating disk layout..."));
	FString ClusterCSVPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("ClusterCSV="), ClusterCSVPath))
	{
		ClusterStatsCsv.CreateOutputFile(ClusterCSVPath);
	}
	CreateDiskLayout(ContainerTargets, Packages, Arguments.OrderMaps, PackageIdMap, Arguments.bClusterByOrderFilePriority);

	for (FContainerTargetSpec* ContainerTarget : ContainerTargets)
	{
		if (ContainerTarget->IoStoreWriter)
		{
			TArray<FPackageStoreEntryResource> PackageStoreEntries;
			for (FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
			{
				if (TargetFile.ChunkType == EContainerChunkType::PackageData || TargetFile.ChunkType == EContainerChunkType::OptionalSegmentPackageData)
				{
					check(TargetFile.Package);
					FIoWriteOptions WriteOptions;
					WriteOptions.DebugName = *TargetFile.DestinationPath;
					WriteOptions.bForceUncompressed = TargetFile.bForceUncompressed;
					WriteOptions.FileName = TargetFile.DestinationPath;
					if (TargetFile.ChunkType == EContainerChunkType::OptionalSegmentPackageData)
					{
						check(ContainerTarget->OptionalSegmentIoStoreWriter);
						check(TargetFile.Package->OptimizedOptionalSegmentPackage);
						FIoChunkId ChunkId = TargetFile.ChunkId;
						if (TargetFile.Package->OptimizedOptionalSegmentPackage->HasEditorData())
						{
							// Auto optional packages replace the non-optional part when the container is mounted
							ChunkId = CreateIoChunkId(TargetFile.Package->GlobalPackageId.Value(), 0, EIoChunkType::ExportBundleData);
						}
						ContainerTarget->OptionalSegmentIoStoreWriter->Append(ChunkId, WriteRequestManager.Read(TargetFile), WriteOptions);
					}
					else
					{
						ContainerTarget->IoStoreWriter->Append(TargetFile.ChunkId, WriteRequestManager.Read(TargetFile), WriteOptions);
						PackageStoreEntries.Add(PackageStoreOptimizer.CreatePackageStoreEntry(TargetFile.Package->OptimizedPackage, TargetFile.Package->OptimizedOptionalSegmentPackage));
					}
				}
			}

			auto WriteContainerHeaderChunk = [](FIoContainerHeader& Header, IIoStoreWriter* IoStoreWriter)
			{
				FLargeMemoryWriter HeaderAr(0, true);
				HeaderAr << Header;
				int64 DataSize = HeaderAr.TotalSize();
				FIoBuffer ContainerHeaderBuffer(FIoBuffer::AssumeOwnership, HeaderAr.ReleaseOwnership(), DataSize);

				FIoWriteOptions WriteOptions;
				WriteOptions.DebugName = TEXT("ContainerHeader");
				WriteOptions.bForceUncompressed = true;
				IoStoreWriter->Append(
					CreateIoChunkId(Header.ContainerId.Value(), 0, EIoChunkType::ContainerHeader),
					ContainerHeaderBuffer,
					WriteOptions);
			};

			ContainerTarget->Header = PackageStoreOptimizer.CreateContainerHeader(ContainerTarget->ContainerId, PackageStoreEntries);
			WriteContainerHeaderChunk(ContainerTarget->Header, ContainerTarget->IoStoreWriter.Get());

			if (ContainerTarget->OptionalSegmentIoStoreWriter)
			{
				ContainerTarget->OptionalSegmentHeader = PackageStoreOptimizer.CreateOptionalContainerHeader(ContainerTarget->ContainerId, PackageStoreEntries);
				WriteContainerHeaderChunk(ContainerTarget->OptionalSegmentHeader, ContainerTarget->OptionalSegmentIoStoreWriter.Get());
			}
		}

		// Check if we need to dump the final order of the packages. Useful, to debug packing.
		if (FParse::Param(FCommandLine::Get(), TEXT("writefinalorder")))
		{
			FString FinalContainerOrderFile = FPaths::GetPath(ContainerTarget->OutputPath) + FPaths::GetBaseFilename(ContainerTarget->OutputPath) + TEXT("-order.txt");
			TUniquePtr<FArchive> IoOrderListArchive(IFileManager::Get().CreateFileWriter(*FinalContainerOrderFile));
			if (IoOrderListArchive)
			{
				IoOrderListArchive->SetIsTextFormat(true);

				// The TargetFiles list is not the order written to disk - FIoStoreWriter sorts
				// on the IdealOrder prior to writing.
				TArray<const FContainerTargetFile*> SortedTargetFiles;
				SortedTargetFiles.Reserve(ContainerTarget->TargetFiles.Num());
				for (const FContainerTargetFile& TargetFile : ContainerTarget->TargetFiles)
				{
					SortedTargetFiles.Add(&TargetFile);
				}

				Algo::SortBy(SortedTargetFiles, [](const FContainerTargetFile* TargetFile) { return TargetFile->IdealOrder; });

				for (const FContainerTargetFile* TargetFile : SortedTargetFiles)
				{
					TStringBuilder<256> Line;
					Line.Append(LexToString(TargetFile->ChunkId));

					if (TargetFile->Package)
					{
						Line.Append(TEXT(" "));
						Line.Append(TargetFile->Package->FileName);
					}

					IoOrderListArchive->Logf(TEXT("%s"), Line.ToString());
				}

				IoOrderListArchive->Close();
			}
		}

	}

	uint64 InitialLoadSize = 0;
	if (GlobalIoStoreWriter)
	{
		FIoBuffer ScriptObjectsBuffer = PackageStoreOptimizer.CreateScriptObjectsBuffer();
		InitialLoadSize = ScriptObjectsBuffer.DataSize();
		FIoWriteOptions WriteOptions;
		WriteOptions.DebugName = TEXT("ScriptObjects");
		GlobalIoStoreWriter->Append(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects), ScriptObjectsBuffer, WriteOptions);
	}

	UE_LOG(LogIoStore, Display, TEXT("Serializing container(s)..."));

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
	if (GeneralIoWriterSettings.bCompressionEnableDDC)
	{
		FIoStoreWriterContext::FProgress Progress = IoStoreWriterContext->GetProgress();
		uint64 TotalDDCAttempts = Progress.CompressionDDCHitCount + Progress.CompressionDDCMissCount;
		double DDCHitRate = double(Progress.CompressionDDCHitCount) / TotalDDCAttempts * 100.0;
		UE_LOG(LogIoStore, Display, TEXT("Compression DDC hits: %llu/%llu (%.2f%%)"), Progress.CompressionDDCHitCount, TotalDDCAttempts, DDCHitRate);
	}

	if (Arguments.WriteBackMetadataToAssetRegistry != EAssetRegistryWritebackMethod::Disabled)
	{
		DoAssetRegistryWritebackDuringStage(Arguments.WriteBackMetadataToAssetRegistry, Arguments.CookedDir, IoStoreWriters);
	}

	TArray<FIoStoreWriterResult> IoStoreWriterResults;
	IoStoreWriterResults.Reserve(IoStoreWriters.Num());
	for (TSharedPtr<IIoStoreWriter> IoStoreWriter : IoStoreWriters)
	{
		IoStoreWriterResults.Emplace(IoStoreWriter->GetResult().ConsumeValueOrDie());
	}
	IoStoreWriters.Empty();

	IOSTORE_CPU_SCOPE(CalculateStats);

	UE_LOG(LogIoStore, Display, TEXT("Calculating stats..."));
	uint64 UExpSize = 0;
	uint64 UAssetSize = 0;
	uint64 ImportedPackagesCount = 0;
	uint64 NoImportedPackagesCount = 0;
	uint64 NameMapCount = 0;
	
	for (const FLegacyCookedPackage* Package : Packages)
	{
		UExpSize += Package->UExpSize;
		UAssetSize += Package->UAssetSize;
		NameMapCount += Package->OptimizedPackage->GetNameCount();
		int32 PackageImportedPackagesCount = Package->OptimizedPackage->GetImportedPackageIds().Num();
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
	
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2lf MB UExp"), (double)UExpSize / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2lf MB UAsset"), (double)UAssetSize / 1024.0 / 1024.0);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8d Packages"), Packages.Num());
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Global shaders"), (double)GlobalShaderSize / 1024.0 / 1024.0, GlobalShaderCount);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Shared shaders"), (double)SharedShaderSize / 1024.0 / 1024.0, SharedShaderCount);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Unique shaders"), (double)UniqueShaderSize / 1024.0 / 1024.0, UniqueShaderCount);
	UE_LOG(LogIoStore, Display, TEXT("Input:  %8.2f MB for %d Inline shaders"), (double)InlineShaderSize / 1024.0 / 1024.0, InlineShaderCount);
	UE_LOG(LogIoStore, Display, TEXT(""));
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Export bundle entries"), PackageStoreOptimizer.GetTotalExportBundleEntryCount());
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Export bundles"), PackageStoreOptimizer.GetTotalExportBundleCount());
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Internal export bundle arcs"), PackageStoreOptimizer.GetTotalInternalBundleArcsCount());
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu External export bundle arcs"), PackageStoreOptimizer.GetTotalExternalBundleArcsCount());
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Name map entries"), NameMapCount);
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Imported package entries"), ImportedPackagesCount);
	UE_LOG(LogIoStore, Display, TEXT("Output: %8llu Packages without imports"), NoImportedPackagesCount);
	UE_LOG(LogIoStore, Display, TEXT("Output: %8d Public runtime script objects"), PackageStoreOptimizer.GetTotalScriptObjectCount());
	UE_LOG(LogIoStore, Display, TEXT("Output: %8.2lf MB InitialLoadData"), (double)InitialLoadSize / 1024.0 / 1024.0);

	if (ChunkDatabase.IsValid())
	{
		uint64 TotalCompressedBytes = 0;
		for (const FIoStoreWriterResult& Result : IoStoreWriterResults)
		{
			TotalCompressedBytes += Result.CompressedContainerSize;
		}

		FIoStoreChunkDatabase& ChunkDatabaseRef = (FIoStoreChunkDatabase&)*ChunkDatabase;
		UE_LOG(LogIoStore, Display, TEXT("Reference Chunk: %s reused bytes out of %s possible: %.1f%%"), *FText::AsNumber(ChunkDatabaseRef.FulfillBytes).ToString(), *FText::AsNumber(TotalCompressedBytes).ToString(), 100.0 * ChunkDatabaseRef.FulfillBytes / TotalCompressedBytes);
		UE_LOG(LogIoStore, Display, TEXT("Reference Chunk: %s chunks found / %s requests"), *FText::AsNumber(ChunkDatabaseRef.FulfillCount).ToString(), *FText::AsNumber(ChunkDatabaseRef.RequestCount).ToString());
		if (ChunkDatabaseRef.ContainerNotFound)
		{
			UE_LOG(LogIoStore, Warning, TEXT("Reference Chunk had %s requests for a container that wasn't loaded. This means the "), *FText::AsNumber(ChunkDatabaseRef.ContainerNotFound).ToString());
			UE_LOG(LogIoStore, Warning, TEXT("new output has a container that wasn't deployed before. If that doesn't sound right"));
			UE_LOG(LogIoStore, Warning, TEXT("verify that you used reference containers from the same project."));
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
		[&ChunkFileNamesMap, &TargetReader](FString Filename, uint32 TocEntryIndex) -> bool
		{
			TIoStatusOr<FIoStoreTocChunkInfo> ChunkInfo = TargetReader->GetChunkInfo(TocEntryIndex);
			if (ChunkInfo.IsOk())
			{
				ChunkFileNamesMap.Add(ChunkInfo.ValueOrDie().Id, Filename);
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
	struct FComputedChunkInfo
	{
		FIoChunkId Id;
		FString FileName;
		FIoChunkHash Hash;
		EIoChunkType ChunkType;
		uint64 Size;
		uint64 CompressedSize;
		uint64 Offset;
		uint64 OffsetOnDisk;
		int32 NumCompressedBlocks;
		bool bIsCompressed;
		bool bHasValidFileName;
	};

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

	TUniquePtr<FOutputDeviceFile> Out = MakeUnique<FOutputDeviceFile>(*CsvPath, true);
	Out->SetSuppressEventTag(true);

	Out->Log(TEXT("OrderInContainer, ChunkId, PackageId, PackageName, Filename, ContainerName, Offset, OffsetOnDisk, Size, CompressedSize, Hash, ChunkType"));

	for (const FString& ContainerFilePath : ContainerFilePaths)
	{
		TUniquePtr<FIoStoreReader> Reader = CreateIoStoreReader(*ContainerFilePath, KeyChain);
		if (!Reader.IsValid())
		{
			UE_LOG(LogIoStore, Warning, TEXT("Failed to read container '%s'"), *ContainerFilePath);
			continue;
		}

		if (!EnumHasAnyFlags(Reader->GetContainerFlags(), EIoContainerFlags::Indexed))
		{
			UE_LOG(LogIoStore, Warning, TEXT("Missing directory index for container '%s'"), *ContainerFilePath);
		}

		UE_LOG(LogIoStore, Display, TEXT("Listing container '%s'"), *ContainerFilePath);

		FString ContainerName = FPaths::GetBaseFilename(ContainerFilePath);
		uint64 CompressionBlockSize = Reader->GetCompressionBlockSize();
		TArray<FIoStoreTocCompressedBlockInfo> CompressedBlocks;
		Reader->EnumerateCompressedBlocks([&CompressedBlocks](const FIoStoreTocCompressedBlockInfo& Block) {
			CompressedBlocks.Add(Block);
			return true;
			});


		TArray<FComputedChunkInfo> Chunks;
		Reader->EnumerateChunks([&Chunks, CompressionBlockSize, &CompressedBlocks](const FIoStoreTocChunkInfo& ChunkInfo) {

			int32 FirstBlockIndex = int32(ChunkInfo.Offset / CompressionBlockSize);
			int32 LastBlockIndex = int32((Align(ChunkInfo.Offset + ChunkInfo.Size, CompressionBlockSize) - 1) / CompressionBlockSize);
			int32 NumCompressedBlocks = LastBlockIndex - FirstBlockIndex + 1;
			uint64 OffsetOnDisk = CompressedBlocks[FirstBlockIndex].Offset;

			FComputedChunkInfo ComputedInfo{
				ChunkInfo.Id,
				ChunkInfo.FileName,
				ChunkInfo.Hash,
				ChunkInfo.ChunkType,
				ChunkInfo.Size,
				ChunkInfo.CompressedSize,
				ChunkInfo.Offset,
				OffsetOnDisk,
				NumCompressedBlocks,
				ChunkInfo.bIsCompressed,
				ChunkInfo.bHasValidFileName
			};

			Chunks.Add(ComputedInfo);
			return true;
			});

		auto SortKey = [](const FComputedChunkInfo& ChunkInfo) { return ChunkInfo.OffsetOnDisk; };
		Algo::SortBy(Chunks, SortKey);

		for(int32 Index=0; Index < Chunks.Num(); ++Index)
		{
			const FComputedChunkInfo& ChunkInfo = Chunks[Index];
			FString PackageName;
			FPackageId PackageId;
			if (ChunkInfo.bHasValidFileName && FPackageName::TryConvertFilenameToLongPackageName(ChunkInfo.FileName, PackageName, nullptr))
			{
				PackageId = FPackageId::FromName(FName(*PackageName));
			}

			Out->Logf(TEXT("%d, %s, 0x%llX, %s, %s, %s, %lld, %lld, %lld, %lld, 0x%s, %s"),
					Index,
					*BytesToHex(ChunkInfo.Id.GetData(), ChunkInfo.Id.GetSize()),
					PackageId.ValueForDebugging(),
					*PackageName,
					*ChunkInfo.FileName,
					*ContainerName,
					ChunkInfo.Offset,
					ChunkInfo.OffsetOnDisk,
					ChunkInfo.Size,
					ChunkInfo.CompressedSize,
					*ChunkInfo.Hash.ToString(),
					*LexToString(ChunkInfo.ChunkType)
					);
		}
	}

	return 0;
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

int32 Describe(
	const FString& GlobalContainerPath,
	const FKeyChain& KeyChain,
	const FString& PackageFilter,
	const FString& OutPath,
	bool bIncludeExportHashes)
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
		int32 ExportBundleCount = -1;
		TArray<FPackageLocation, TInlineAllocator<1>> Locations;
		TArray<FPackageId> ImportedPackageIds;
		TArray<uint64> ImportedPublicExportHashes;
		TArray<FImportDesc> Imports;
		TArray<FExportDesc> Exports;
		TArray<TArray<FExportBundleEntryDesc>, TInlineAllocator<1>> ExportBundles;
	};

	if (!IFileManager::Get().FileExists(*GlobalContainerPath))
	{
		UE_LOG(LogIoStore, Error, TEXT("Global container '%s' doesn't exist."), *GlobalContainerPath);
		return -1;
	}

	TUniquePtr<FIoStoreReader> GlobalReader = CreateIoStoreReader(*GlobalContainerPath, KeyChain);
	if (!GlobalReader.IsValid())
	{
		UE_LOG(LogIoStore, Warning, TEXT("Failed reading global container '%s'"), *GlobalContainerPath);
		return -1;
	}

	UE_LOG(LogIoStore, Display, TEXT("Loading script imports..."));

	TIoStatusOr<FIoBuffer> ScriptObjectsBuffer = GlobalReader->Read(CreateIoChunkId(0, 0, EIoChunkType::ScriptObjects), FIoReadOptions());
	if (!ScriptObjectsBuffer.IsOk())
	{
		UE_LOG(LogIoStore, Warning, TEXT("Failed reading initial load meta chunk from global container '%s'"), *GlobalContainerPath);
		return -1;
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
				PackageDesc->Exports.SetNum(ContainerEntry.ExportCount);
				PackageDesc->ExportBundleCount = ContainerEntry.ExportBundleCount;
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
			ExportDesc.SerialSize = ExportMapEntry.CookedSerialSize;
		}

		const FExportBundleHeader* ExportBundleHeaders = reinterpret_cast<const FExportBundleHeader*>(PackageSummaryData + PackageSummary->GraphDataOffset);
		const FExportBundleEntry* ExportBundleEntries = reinterpret_cast<const FExportBundleEntry*>(PackageSummaryData + PackageSummary->ExportBundleEntriesOffset);
		uint64 CurrentExportOffset = PackageSummary->HeaderSize;
		for (int32 ExportBundleIndex = 0; ExportBundleIndex < Job.PackageDesc->ExportBundleCount; ++ExportBundleIndex)
		{
			TArray<FExportBundleEntryDesc>& ExportBundleDesc = Job.PackageDesc->ExportBundles.AddDefaulted_GetRef();
			const FExportBundleHeader* ExportBundle = ExportBundleHeaders + ExportBundleIndex;
			const FExportBundleEntry* BundleEntry = ExportBundleEntries + ExportBundle->FirstEntryIndex;
			const FExportBundleEntry* BundleEntryEnd = BundleEntry + ExportBundle->EntryCount;
			check(BundleEntry <= BundleEntryEnd);
			while (BundleEntry < BundleEntryEnd)
			{
				FExportBundleEntryDesc& EntryDesc = ExportBundleDesc.AddDefaulted_GetRef();
				EntryDesc.CommandType = FExportBundleEntry::EExportCommandType(BundleEntry->CommandType);
				EntryDesc.LocalExportIndex = BundleEntry->LocalExportIndex;
				EntryDesc.Export = &Job.PackageDesc->Exports[BundleEntry->LocalExportIndex];
				if (BundleEntry->CommandType == FExportBundleEntry::ExportCommandType_Serialize)
				{
					EntryDesc.Export->SerialOffset = CurrentExportOffset;
					CurrentExportOffset += EntryDesc.Export->SerialSize;

					if (bIncludeExportHashes)
					{
						check(EntryDesc.Export->SerialOffset + EntryDesc.Export->SerialSize <= IoBuffer.ValueOrDie().DataSize());
						FSHA1::HashBuffer(IoBuffer.ValueOrDie().Data() + EntryDesc.Export->SerialOffset, EntryDesc.Export->SerialSize, EntryDesc.Export->ExportHash.Hash);
					}
				}
				++BundleEntry;
			}
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
						Current = ExportStack.Pop(false);
						FullNameBuilder.Append(TEXT("/"));
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
						check(ScriptObjectDesc);
						Import.Name = ScriptObjectDesc->FullName;
					}
				}
			}
		}
	}

	UE_LOG(LogIoStore, Display, TEXT("Collecting output packages..."));
	TArray<const FPackageDesc*> OutputPackages;
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
		TGuardValue<bool> GuardPrintLogCategory(GPrintLogCategory, false);
		TGuardValue<bool> GuardPrintLogVerbosity(GPrintLogVerbosity, false);

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
				FScriptObjectDesc* ScriptObjectDesc = ScriptObjectByGlobalIdMap.Find(PackageObjectIndex);
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
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("Localized Packages"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
				for (const FPackageDesc* LocalizedPackage : ContainerDesc->LocalizedPackages)
				{
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
					OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\t           Source: 0x%llX '%s'"), LocalizedPackage->PackageId.ValueForDebugging(), *LocalizedPackage->PackageName.ToString());
				}
			}

			if (ContainerDesc->PackageRedirects.Num())
			{
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("--------------------------------------------"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("Package Redirects"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
				for (const FPackageRedirect& Redirect : ContainerDesc->PackageRedirects)
				{
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
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t\tExportBundleCount: %d"), PackageDesc->ExportBundleCount);

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
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("Export Bundles"));
			OutputOverride->Logf(ELogVerbosity::Display, TEXT("=========="));
			Index = 0;
			for (const TArray<FExportBundleEntryDesc>& ExportBundle : PackageDesc->ExportBundles)
			{
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\t*************************"));
				OutputOverride->Logf(ELogVerbosity::Display, TEXT("\tExport Bundle %d"), Index++);
				for (const FExportBundleEntryDesc& ExportBundleEntry : ExportBundle)
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
	}

	for (FPackageDesc* PackageDesc : Packages)
	{
		delete PackageDesc;
	}
	for (FContainerDesc* ContainerDesc : Containers)
	{
		delete ContainerDesc;
	}

	return 0;
}

static int32 Diff(
	const FString& SourcePath,
	const FKeyChain& SourceKeyChain,
	const FString& TargetPath,
	const FKeyChain& TargetKeyChain,
	const FString& OutPath)
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

	auto ReadContainers = [](const FString& Directory, const FKeyChain& KeyChain, FContainers& OutContainers)
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

			Reader->EnumerateChunks([&ContainerChunkInfo](const FIoStoreTocChunkInfo& ChunkInfo)
			{
				ContainerChunkInfo.ChunkInfoById.Add(ChunkInfo.Id, ChunkInfo);
				ContainerChunkInfo.UncompressedContainerSize += ChunkInfo.Size;
				ContainerChunkInfo.CompressedContainerSize += ChunkInfo.CompressedSize;
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

bool LegacyDiffIoStoreContainers(const TCHAR* InContainerFilename1, const TCHAR* InContainerFilename2, bool bInLogUniques1, bool bInLogUniques2, const FKeyChain& InKeyChain)
{
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);
	UE_LOG(LogIoStore, Log, TEXT("FileEventType, FileName, Size1, Size2"));

	TUniquePtr<FIoStoreReader> Reader1 = CreateIoStoreReader(InContainerFilename1, InKeyChain);
	if (!Reader1.IsValid())
	{
		return false;
	}

	if (!EnumHasAnyFlags(Reader1->GetContainerFlags(), EIoContainerFlags::Indexed))
	{
		UE_LOG(LogIoStore, Warning, TEXT("Missing directory index for container '%s'"), InContainerFilename1);
	}

	TUniquePtr<FIoStoreReader> Reader2 = CreateIoStoreReader(InContainerFilename2, InKeyChain);
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
				PackageStoreEntryResource.ExportInfo.ExportBundleCount = StoreEntry->ExportBundleCount;
				PackageStoreEntryResource.ExportInfo.ExportCount = StoreEntry->ExportCount;
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
	ZenStoreWriter->BeginCook();
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
	ZenStoreWriter->EndCook();
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

bool ExtractFilesFromIoStoreContainer(
	const TCHAR* InContainerFilename,
	const TCHAR* InDestPath,
	const FKeyChain& InKeyChain,
	const FString* InFilter,
	TMap<FString, uint64>* OutOrderMap,
	TArray<FGuid>* OutUsedEncryptionKeys,
	bool* bOutIsSigned)
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

	auto WriteFile = [](const FString& FileName, const uint8* Data, uint64 DataSize)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(WriteFile);
		TUniquePtr<FArchive> FileHandle(IFileManager::Get().CreateFileWriter(*FileName));
		if (FileHandle)
		{
			FileHandle->Serialize(const_cast<uint8*>(Data), DataSize);
			UE_CLOG(FileHandle->IsError(), LogIoStore, Error, TEXT("Failed writing to file \"%s\"."), *FileName);
			return !FileHandle->IsError();
		}
		else
		{
			UE_LOG(LogIoStore, Error, TEXT("Unable to create file \"%s\"."), *FileName);
			return false;
		}
	};

	TArray<UE::Tasks::TTask<bool>> ExtractTasks;
	ExtractTasks.Reserve(Entries.Num());
	EAsyncExecution ThreadPool = EAsyncExecution::ThreadPool;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		const FEntry& Entry = Entries[EntryIndex];

		UE::Tasks::TTask<TIoStatusOr<FIoBuffer>> ReadTask = Reader->ReadAsync(Entry.ChunkId, FIoReadOptions());

		// Once the read is done, write out the file.
		ExtractTasks.Emplace(UE::Tasks::Launch(TEXT("IoStore_Extract"), 
			[&Entry, &WriteFile, ReadTask]() mutable
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
					if (!WriteFile(DestFileName, Data, HeaderDataSize))
					{
						return false;
					}
					DestFileName = FPaths::ChangeExtension(Entry.DestFileName, TEXT(".uexp"));
					if (!WriteFile(DestFileName, Data + HeaderDataSize, DataSize - HeaderDataSize))
					{
						return false;
					}
				}
				else if (!WriteFile(Entry.DestFileName, Data, DataSize))
				{
					return false;
				}
				return true;
			},
			Prerequisites(ReadTask)));
	}

	int32 ErrorCount = 0;
	for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
	{
		if (ExtractTasks[EntryIndex].GetResult())
		{
			const FEntry& Entry = Entries[EntryIndex];
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

	UE_LOG(LogIoStore, Log, TEXT("Finished extracting %d chunks (including %d errors)."), Entries.Num(), ErrorCount);
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
		UE_LOG(LogIoStore, Error, TEXT("Failed to read order file '%s'."), *FilePath);
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
	if (FParse::Param(FCommandLine::Get(), TEXT("sign")))
	{
		Arguments.bSign = true;
	}

	UE_LOG(LogIoStore, Display, TEXT("Container signing - %s"), Arguments.bSign ? TEXT("ENABLED") : TEXT("DISABLED"));

	Arguments.bCreateDirectoryIndex = !FParse::Param(FCommandLine::Get(), TEXT("NoDirectoryIndex"));
	UE_LOG(LogIoStore, Display, TEXT("Directory index - %s"), Arguments.bCreateDirectoryIndex ? TEXT("ENABLED") : TEXT("DISABLED"));

	WriterSettings.CompressionMethod = DefaultCompressionMethod;
	WriterSettings.CompressionBlockSize = DefaultCompressionBlockSize;

	WriterSettings.bEnableCsvOutput = FParse::Param(FCommandLine::Get(), TEXT("csvoutput"));

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
	FParse::Value(FCommandLine::Get(), TEXT("-ReferenceContainerGlobalFileName="), Arguments.ReferenceChunkGlobalContainerFileName);

	FString CryptoKeysCacheFilename;
	if (FParse::Value(FCommandLine::Get(), TEXT("-ReferenceContainerCryptoKeys="), CryptoKeysCacheFilename))
	{
		UE_LOG(LogIoStore, Display, TEXT("Parsing reference container crypto keys from a crypto key cache file '%s'"), *CryptoKeysCacheFilename);
		KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, Arguments.ReferenceChunkKeys);
	}


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

			if (!FParse::Value(*Command, TEXT("Output="), ContainerSpec.OutputPath))
			{
				UE_LOG(LogIoStore, Error, TEXT("Output argument missing from command '%s'"), *Command);
				return false;
			}
			ContainerSpec.OutputPath = FPaths::ChangeExtension(ContainerSpec.OutputPath, TEXT(""));

			FParse::Value(*Command, TEXT("OptionalOutput="), ContainerSpec.OptionalOutputPath);

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

				FString EncryptionKeyOverrideGuidString;
				if (FParse::Value(*Command, TEXT("EncryptionKeyOverrideGuid="), EncryptionKeyOverrideGuidString))
				{
					FGuid::Parse(EncryptionKeyOverrideGuidString, ContainerSpec.EncryptionKeyOverrideGuid);
				}
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

	FString ArgumentValue;
	if (FParse::Value(FCommandLine::Get(), TEXT("List="), ArgumentValue))
	{
		FString ContainerPathOrWildcard = MoveTemp(ArgumentValue);
		FString CsvPath;
		if (!FParse::Value(FCommandLine::Get(), TEXT("csv="), CsvPath))
		{
			UE_LOG(LogIoStore, Error, TEXT("Incorrect arguments. Expected: -list=<ContainerFile> -csv=<path>"));
		}

		return ListContainer(Arguments.KeyChain, ContainerPathOrWildcard, CsvPath);
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
		if (FParse::Value(CmdLine, TEXT("SourceCryptoKeys="), CryptoKeysCacheFilename))
		{
			UE_LOG(LogIoStore, Display, TEXT("Parsing source crypto keys from '%s'"), *CryptoKeysCacheFilename);
			KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, SourceKeyChain);
		}

		if (FParse::Value(CmdLine, TEXT("TargetCryptoKeys="), CryptoKeysCacheFilename))
		{
			UE_LOG(LogIoStore, Display, TEXT("Parsing target crypto keys from '%s'"), *CryptoKeysCacheFilename);
			KeyChainUtilities::LoadKeyChainFromFile(CryptoKeysCacheFilename, TargetKeyChain);
		}

		return Diff(SourcePath, SourceKeyChain, TargetPath, TargetKeyChain, OutPath);
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
	else if (FParse::Value(FCommandLine::Get(), TEXT("CreateDLCContainer="), Arguments.DLCPluginPath))
	{
		if (!ParseContainerGenerationArguments(Arguments, WriterSettings))
		{
			return -1;
		}
		
		Arguments.DLCName = FPaths::GetBaseFilename(*Arguments.DLCPluginPath);
		Arguments.bRemapPluginContentToGame = FParse::Param(FCommandLine::Get(), TEXT("RemapPluginContentToGame"));

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
			return -1;
		}
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
		UE_CLOG(!Arguments.PackageStore->HasDataSource(), LogIoStore, Fatal, TEXT("Expected -ScriptObjects=<path to script objects file> argument"));
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
