// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStoreOnDemand.h"
#include "OnDemandIoDispatcherBackend.h"
#include "EncryptionKeyManager.h"
#include "LatencyInjector.h"

#include "FileIoCache.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/LargeMemoryWriter.h"
#include "String/LexFromString.h"

#if (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))
#include "Async/Async.h"
#include "Misc/FileHelper.h"
#include "S3/S3Client.h"
#endif // (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))

DEFINE_LOG_CATEGORY(LogIas);

namespace UE::IO::Private
{

////////////////////////////////////////////////////////////////////////////////
static int64 ParseSizeParam(FStringView Value)
{
	Value = Value.TrimStartAndEnd();

	int64 Size = -1;
	LexFromString(Size, Value);
	if (Size >= 0)
	{
		if (Value.EndsWith(TEXT("GB"))) return Size << 30;
		if (Value.EndsWith(TEXT("MB"))) return Size << 20;
		if (Value.EndsWith(TEXT("KB"))) return Size << 10;
	}
	return Size;
}

////////////////////////////////////////////////////////////////////////////////
static int64 ParseSizeParam(const TCHAR* CommandLine, const TCHAR* Param)
{
	FString ParamValue;
	if (!FParse::Value(CommandLine, Param, ParamValue))
	{
		return -1;
	}

	return ParseSizeParam(ParamValue);
}

////////////////////////////////////////////////////////////////////////////////
static bool ParseEncryptionKeyParam(const FString& Param, FGuid& OutKeyGuid, FAES::FAESKey& OutKey)
{
	TArray<FString> Tokens;
	Param.ParseIntoArray(Tokens, TEXT(":"), true);

	if (Tokens.Num() == 2)
	{
		TArray<uint8> KeyBytes;
		if (FGuid::Parse(Tokens[0], OutKeyGuid) && FBase64::Decode(Tokens[1], KeyBytes))
		{
			if (OutKeyGuid != FGuid() && KeyBytes.Num() == FAES::FAESKey::KeySize)
			{
				FMemory::Memcpy(OutKey.Key, KeyBytes.GetData(), FAES::FAESKey::KeySize);
				return true;
			}
		}
	}
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////
static FFileIoCacheConfig GetFileIoCacheConfig(const TCHAR* CommandLine)
{
	FFileIoCacheConfig Ret;

	// Fetch values from .ini files
	auto GetConfigIntImpl = [CommandLine] (const TCHAR* ConfigKey, const TCHAR* ParamName, auto& Out)
	{
		int64 Value = -1;
		if (FString Temp; GConfig->GetString(TEXT("Ias"), ConfigKey, Temp, GEngineIni))
		{
			Value = ParseSizeParam(Temp);
		}
#if !UE_BUILD_SHIPPING
		if (int64 Override = ParseSizeParam(CommandLine, ParamName); Override >= 0)
		{
			Value = Override;
		}
#endif

		if (Value >= 0)
		{
			Out = decltype(Out)(Value);
		}

		return true;
	};

#define GetConfigInt(Name, Dest) \
	do { GetConfigIntImpl(TEXT("FileCache.") Name, TEXT("Ias.FileCache.") Name TEXT("="), Dest); } while (false)
	GetConfigInt(TEXT("WritePeriodSeconds"),	Ret.WriteRate.Seconds);
	GetConfigInt(TEXT("WriteOpsPerPeriod"),		Ret.WriteRate.Ops);
	GetConfigInt(TEXT("WriteBytesPerPeriod"),	Ret.WriteRate.Allowance);
	GetConfigInt(TEXT("DiskQuota"),				Ret.DiskQuota);
	GetConfigInt(TEXT("MemoryQuota"),			Ret.MemoryQuota);
#undef GetConfigInt

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("Ias.DropCache")))
	{
		Ret.DropCache = true;
	}
#endif

	return Ret;
}

} // namespace UE::IO::Private



namespace UE
{

////////////////////////////////////////////////////////////////////////////////
FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocHeader& Header)
{
	Writer.BeginObject();
	Writer.AddInteger(UTF8TEXTVIEW("Magic"), Header.Magic);
	Writer.AddInteger(UTF8TEXTVIEW("Version"), Header.Version);
	Writer.AddInteger(UTF8TEXTVIEW("ChunkVersion"), Header.ChunkVersion);
	Writer.AddInteger(UTF8TEXTVIEW("BlockSize"), Header.BlockSize);
	Writer.AddString(UTF8TEXTVIEW("CompressionFormat"), Header.CompressionFormat);
	Writer.AddString(UTF8TEXTVIEW("ChunksDirectory"), Header.ChunksDirectory);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocHeader& OutTocHeader)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		OutTocHeader.Magic = Obj["Magic"].AsUInt64();
		OutTocHeader.Version = Obj["Version"].AsUInt32();
		OutTocHeader.ChunkVersion = Obj["ChunkVersion"].AsUInt32();
		OutTocHeader.BlockSize = Obj["BlockSize"].AsUInt32();
		OutTocHeader.CompressionFormat = FString(Obj["CompressionFormat"].AsString());
		OutTocHeader.ChunksDirectory = FString(Obj["ChunksDirectory"].AsString());

		return OutTocHeader.Magic == FOnDemandTocHeader::ExpectedMagic &&
			static_cast<EOnDemandTocVersion>(OutTocHeader.Version) != EOnDemandTocVersion::Invalid;
	}

	return false;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocEntry& Entry)
{
	Writer.BeginObject();
	Writer.AddHash(UTF8TEXTVIEW("Hash"), Entry.Hash);
	Writer.AddHash(UTF8TEXTVIEW("RawHash"), Entry.RawHash);
	Writer << UTF8TEXTVIEW("ChunkId") << Entry.ChunkId;
	Writer.AddInteger(UTF8TEXTVIEW("RawSize"), Entry.RawSize);
	Writer.AddInteger(UTF8TEXTVIEW("EncodedSize"), Entry.EncodedSize);
	Writer.AddInteger(UTF8TEXTVIEW("BlockOffset"), Entry.BlockOffset);
	Writer.AddInteger(UTF8TEXTVIEW("BlockCount"), Entry.BlockCount);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocEntry& OutTocEntry)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		if (!LoadFromCompactBinary(Obj["ChunkId"], OutTocEntry.ChunkId))
		{
			return false;
		}

		OutTocEntry.Hash = Obj["Hash"].AsHash();
		OutTocEntry.RawHash = Obj["RawHash"].AsHash();
		OutTocEntry.RawSize = Obj["RawSize"].AsUInt64(~uint64(0));
		OutTocEntry.EncodedSize = Obj["EncodedSize"].AsUInt64(~uint64(0));
		OutTocEntry.BlockOffset = Obj["BlockOffset"].AsUInt32(~uint32(0));
		OutTocEntry.BlockCount = Obj["BlockCount"].AsUInt32();

		return OutTocEntry.Hash != FIoHash::Zero &&
			OutTocEntry.RawSize != ~uint64(0) &&
			OutTocEntry.EncodedSize != ~uint64(0) &&
			OutTocEntry.BlockOffset != ~uint32(0);
	}

	return false;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocContainerEntry& ContainerEntry)
{
	Writer.BeginObject();
	Writer.AddString(UTF8TEXTVIEW("Name"), ContainerEntry.ContainerName);
	Writer.AddString(UTF8TEXTVIEW("EncryptionKeyGuid"), ContainerEntry.EncryptionKeyGuid);

	Writer.BeginArray(UTF8TEXTVIEW("Entries"));
	for (const FOnDemandTocEntry& Entry : ContainerEntry.Entries)
	{
		Writer << Entry;
	}
	Writer.EndArray();
	
	Writer.BeginArray(UTF8TEXTVIEW("BlockSizes"));
	for (uint32 BlockSize : ContainerEntry.BlockSizes)
	{
		Writer << BlockSize;
	}
	Writer.EndArray();

	Writer.BeginArray(UTF8TEXTVIEW("BlockHashes"));
	for (const FIoHash& BlockHash : ContainerEntry.BlockHashes)
	{
		Writer << BlockHash;
	}
	Writer.EndArray();

	Writer.AddHash(UTF8TEXTVIEW("UTocHash"), ContainerEntry.UTocHash);

	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocContainerEntry& OutContainer)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		OutContainer.ContainerName = FString(Obj["Name"].AsString());
		OutContainer.EncryptionKeyGuid = FString(Obj["EncryptionKeyGuid"].AsString());

		FCbArrayView Entries = Obj["Entries"].AsArrayView();
		OutContainer.Entries.Reserve(int32(Entries.Num()));
		for (FCbFieldView ArrayField : Entries)
		{
			if (!LoadFromCompactBinary(ArrayField, OutContainer.Entries.AddDefaulted_GetRef()))
			{
				return false;
			}
		}

		FCbArrayView BlockSizes = Obj["BlockSizes"].AsArrayView();
		OutContainer.BlockSizes.Reserve(int32(BlockSizes.Num()));
		for (FCbFieldView ArrayField : BlockSizes)
		{
			OutContainer.BlockSizes.Add(ArrayField.AsUInt32());
		}

		FCbArrayView BlockHashes = Obj["BlockHashes"].AsArrayView();
		OutContainer.BlockHashes.Reserve(int32(BlockHashes.Num()));
		for (FCbFieldView ArrayField : BlockHashes)
		{
			OutContainer.BlockHashes.Add(ArrayField.AsHash());
		}

		OutContainer.UTocHash = Obj["UTocHash"].AsHash();

		return true;
	}

	return false;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandToc& TocResource)
{
	Writer.BeginObject();
	Writer << UTF8TEXTVIEW("Header") << TocResource.Header;

	Writer.BeginArray(UTF8TEXTVIEW("Containers"));
	for (const FOnDemandTocContainerEntry& Container : TocResource.Containers)
	{
		Writer << Container;
	}
	Writer.EndArray();
	Writer.EndObject();
	
	return Writer;
}

TIoStatusOr<FString> FOnDemandToc::Save(const TCHAR* Directory, const FOnDemandToc& TocResource)
{
	if (TocResource.Header.Magic != FOnDemandTocHeader::ExpectedMagic)
	{
		return FIoStatus(EIoErrorCode::CorruptToc);
	}

	if (TocResource.Header.CompressionFormat.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::CorruptToc);
	}

	FCbWriter Writer;
	Writer << TocResource;

	FLargeMemoryWriter Ar;
	SaveCompactBinary(Ar, Writer.Save());

	const FIoHash TocHash = FIoHash::HashBuffer(Ar.GetView());
	const FString FilePath = FString(Directory) / LexToString(TocHash) + TEXT(".iochunktoc");

	IFileManager& FileMgr = IFileManager::Get();
	if (TUniquePtr<FArchive> FileAr(FileMgr.CreateFileWriter(*FilePath)); FileAr.IsValid())
	{
		FileAr->Serialize(Ar.GetData(), Ar.TotalSize());
		FileAr->Flush();
		FileAr->Close();

		return FilePath;
	}

	return FIoStatus(EIoErrorCode::WriteError);
}

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandToc& OutToc)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		if (!LoadFromCompactBinary(Obj["Header"], OutToc.Header))
		{
			return false;
		}

		FCbArrayView Containers = Obj["Containers"].AsArrayView();
		OutToc.Containers.Reserve(int32(Containers.Num()));
		for (FCbFieldView ArrayField : Containers)
		{
			if (!LoadFromCompactBinary(ArrayField, OutToc.Containers.AddDefaulted_GetRef()))
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////
#if (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))
class FS3UploadQueue
{
public:
	FS3UploadQueue(FS3Client& Client, const FString& Bucket, int32 ThreadCount);
	bool Enqueue(const FString& Key, FIoBuffer Payload);
	bool Flush();

private:
	void ThreadEntry();

	struct FQueueEntry
	{
		FString Key;
		FIoBuffer Payload;
	};

	FS3Client& Client;
	TArray<TFuture<void>> Threads;
	FString Bucket;
	FCriticalSection CriticalSection;
	TQueue<FQueueEntry> Queue;
	FEventRef WakeUpEvent;
	FEventRef UploadCompleteEvent;
	std::atomic_int32_t ConcurrentUploads{0};
	std::atomic_int32_t ActiveThreadCount{0};
	std::atomic_int32_t ErrorCount{0};
	std::atomic_bool bCompleteAdding{false};
};

FS3UploadQueue::FS3UploadQueue(FS3Client& InClient, const FString& InBucket, int32 ThreadCount)
	: Client(InClient)
	, Bucket(InBucket)
{
	ActiveThreadCount = ThreadCount;
	for (int32 Idx = 0; Idx < ThreadCount; ++Idx)
	{
		Threads.Add(AsyncThread([this]()
		{
			ThreadEntry();
		}));
	}
}

bool FS3UploadQueue::Enqueue(const FString& Key, FIoBuffer Payload)
{
	if (ActiveThreadCount == 0)
	{
		return false;
	}

	for(;;)
	{
		bool bEnqueued = false;
		{
			FScopeLock _(&CriticalSection);
			if (ConcurrentUploads < Threads.Num())
			{
				bEnqueued = Queue.Enqueue(FQueueEntry {Key, Payload});
			}
		}

		if (bEnqueued)
		{
			WakeUpEvent->Trigger();
			break;
		}

		UploadCompleteEvent->Wait();
	}

	return true;
}

void FS3UploadQueue::ThreadEntry()
{
	for(;;)
	{
		FQueueEntry Entry;
		bool bDequeued = false;
		{
			FScopeLock _(&CriticalSection);
			bDequeued = Queue.Dequeue(Entry);
		}

		if (!bDequeued)
		{
			if (bCompleteAdding)
			{
				break;
			}
			WakeUpEvent->Wait();
			continue;
		}

		ConcurrentUploads++;
		const FS3PutObjectResponse Response = Client.TryPutObject(FS3PutObjectRequest{Bucket, Entry.Key, Entry.Payload.GetView()});
		ConcurrentUploads--;
		UploadCompleteEvent->Trigger();

		if (Response.IsOk())
		{
			UE_LOG(LogIas, Display, TEXT("Uploaded chunk '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Bucket, *Entry.Key);
		}
		else
		{
			UE_LOG(LogIas, Warning, TEXT("Failed to upload chunk '%s/%s/%s', StatusCode: %u"), *Client.GetConfig().ServiceUrl, *Bucket, *Entry.Key, Response.StatusCode);
			ErrorCount++;
			break;
		}
	}

	ActiveThreadCount--;
}

bool FS3UploadQueue::Flush()
{
	bCompleteAdding = true;
	for (int32 Idx = 0; Idx < Threads.Num(); ++Idx)
	{
		WakeUpEvent->Trigger();
	}

	for (TFuture<void>& Thread : Threads)
	{
		Thread.Wait();
	}

	return ErrorCount == 0;
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FIoStoreUploadParams> FIoStoreUploadParams::Parse(const TCHAR* CommandLine)
{
	FIoStoreUploadParams Params;

	if (!FParse::Value(CommandLine, TEXT("Bucket="), Params.Bucket))
	{
		FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
	}

	FParse::Value(CommandLine, TEXT("BucketPrefix="), Params.BucketPrefix);
	FParse::Value(CommandLine, TEXT("ServiceUrl="), Params.ServiceUrl);
	FParse::Value(CommandLine, TEXT("Region="), Params.Region);
	FParse::Value(CommandLine, TEXT("AccessKey="), Params.AccessKey);
	FParse::Value(CommandLine, TEXT("SecretKey="), Params.SecretKey);
	FParse::Value(CommandLine, TEXT("SessionToken="), Params.SessionToken);
	FParse::Value(CommandLine, TEXT("CredentialsFile="), Params.CredentialsFile);
	FParse::Value(CommandLine, TEXT("CredentialsFileKeyName="), Params.CredentialsFileKeyName);
	Params.bDeleteContainerFiles = FParse::Param(CommandLine, TEXT("KeepUploadedContainers")) == false;

	if (Params.AccessKey.IsEmpty() &&
		Params.SecretKey.IsEmpty() &&
		Params.CredentialsFile.IsEmpty() &&
		Params.CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credentials"));
	}
	
	if (!Params.AccessKey.IsEmpty() && Params.SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid secret key"));
	}
	else if (Params.AccessKey.IsEmpty() && !Params.SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid access key"));
	}

	if (!Params.CredentialsFile.IsEmpty() && Params.CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credential file key name"));
	}

	if (Params.ServiceUrl.IsEmpty() && Params.Region.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Service URL or AWS region needs to be specified"));
	}

	return Params;
}

TIoStatusOr<FIoStoreUploadResult> UploadContainerFiles(
	const FIoStoreUploadParams& UploadParams,
	TConstArrayView<FString> ContainerFiles,
	const TMap<FGuid, FAES::FAESKey>& EncryptionKeys)
{
	FS3ClientConfig Config;
	Config.ServiceUrl = UploadParams.ServiceUrl;
	Config.Region = UploadParams.Region;
	
	FS3ClientCredentials Credentials;
	if (UploadParams.CredentialsFile.IsEmpty() == false)
	{
		UE_LOG(LogIas, Display, TEXT("Loading credentials file '%s'"), *UploadParams.CredentialsFile);
		FS3CredentialsProfileStore CredentialsStore = FS3CredentialsProfileStore::FromFile(UploadParams.CredentialsFile);
		if (CredentialsStore.TryGetCredentials(UploadParams.CredentialsFileKeyName, Credentials) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Failed to find valid credentials in credentials file"));
		}
		else
		{
			UE_LOG(LogIas, Display, TEXT("Found credentials for '%s'"), *UploadParams.CredentialsFileKeyName);
		}
	}
	else
	{
		Credentials = FS3ClientCredentials(UploadParams.AccessKey, UploadParams.SecretKey, UploadParams.SessionToken);
	}

	FS3Client Client(Config, Credentials);
	FS3UploadQueue UploadQueue(Client, UploadParams.Bucket, UploadParams.MaxConcurrentUploads);

	if (ContainerFiles.Num() == 0)
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("No container file(s) specified"));
	}

	TSet<FIoHash> ExistingChunks;
	{
		TStringBuilder<256> TocsKey;
		TocsKey << UploadParams.BucketPrefix << "/";

		UE_LOG(LogIas, Display, TEXT("Fetching TOC's '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, TocsKey.ToString());
		FS3ListObjectResponse Response = Client.ListObjects(FS3ListObjectsRequest
		{
			UploadParams.Bucket,
			TocsKey.ToString(),
			TEXT('/')
		});

		for (const FS3Object& TocInfo : Response.Objects)
		{
			if (TocInfo.Key.EndsWith(TEXT("iochunktoc")) == false)
			{
				continue;
			}

			UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, *TocInfo.Key);
			FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
			{
				UploadParams.Bucket,
				TocInfo.Key
			});
			
			if (TocResponse.IsOk() == false)
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to fetch TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, *TocInfo.Key);
				continue;
			}

			FOnDemandToc Toc;
			if (UE::LoadFromCompactBinary(FCbFieldView(TocResponse.Body.GetData()), Toc) == false)
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to load TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, *TocInfo.Key);
				continue;
			}

			for (const FOnDemandTocContainerEntry& ContainerEntry : Toc.Containers)
			{
				for (const FOnDemandTocEntry& TocEntry : ContainerEntry.Entries)
				{
					ExistingChunks.Add(TocEntry.Hash);
				}
			}
		}
	}
	UE_LOG(LogIas, Display, TEXT("Found %d existing chunks"), ExistingChunks.Num());
	
	FString ChunksRelativePath = UploadParams.BucketPrefix.IsEmpty()
		? FString::Printf(TEXT("IoChunksV%u"), EOnDemandChunkVersion::Latest)
		: FString::Printf(TEXT("%s/IoChunksV%u"), *UploadParams.BucketPrefix, EOnDemandChunkVersion::Latest);

	ChunksRelativePath.ToLowerInline();

	uint64 TotalUploadedChunks = 0;
	uint64 TotalUploadedBytes = 0;

	FOnDemandToc OnDemandToc;
	OnDemandToc.Header.ChunksDirectory = FString::Printf(TEXT("IoChunksV%u"), EOnDemandChunkVersion::Latest).ToLower();

	TArray<FString> UploadedFiles;
	for (const FString& Path : ContainerFiles)
	{
		FIoStoreReader ContainerFileReader;
		{
			FIoStatus Status = ContainerFileReader.Initialize(*FPaths::ChangeExtension(Path, TEXT("")), EncryptionKeys);
			if (!Status.IsOk())
			{
				UE_LOG(LogIas, Error, TEXT("Failed to open container '%s' for reading"), *Path);
				continue;
			}
		}

		if (EnumHasAnyFlags(ContainerFileReader.GetContainerFlags(), EIoContainerFlags::OnDemand) == false)
		{
			UE_LOG(LogIas, Display, TEXT("Skipping non ondemand container '%s'"), *Path);
			continue;
		}
		
		UE_LOG(LogIas, Display, TEXT("Uploading container '%s/.ucas'"), *Path);

		const uint32 BlockSize = ContainerFileReader.GetCompressionBlockSize();
		if (OnDemandToc.Header.BlockSize == 0)
		{
			OnDemandToc.Header.BlockSize = ContainerFileReader.GetCompressionBlockSize();
		}
		check(OnDemandToc.Header.BlockSize == ContainerFileReader.GetCompressionBlockSize());

		TArray<FIoStoreTocChunkInfo> ChunkInfos;
		ContainerFileReader.EnumerateChunks([&ChunkInfos](FIoStoreTocChunkInfo&& Info)
		{ 
			ChunkInfos.Emplace(MoveTemp(Info));
			return true;
		});
		
		FOnDemandTocContainerEntry& ContainerEntry = OnDemandToc.Containers.AddDefaulted_GetRef();
		ContainerEntry.ContainerName = FPaths::GetBaseFilename(Path);
		ContainerEntry.EncryptionKeyGuid = LexToString(ContainerFileReader.GetEncryptionKeyGuid());
		
		for (const FIoStoreTocChunkInfo& ChunkInfo : ChunkInfos)
		{
			const bool bDecrypt = false;
			TIoStatusOr<FIoStoreCompressedReadResult> Status = ContainerFileReader.ReadCompressed(ChunkInfo.Id, FIoReadOptions(), bDecrypt);
			if (!Status.IsOk())
			{
				return Status.Status();
			}

			FIoStoreCompressedReadResult ReadResult = Status.ConsumeValueOrDie();

			const uint32 BlockOffset = ContainerEntry.BlockSizes.Num();
			const uint32 BlockCount = ReadResult.Blocks.Num();
			const FIoHash ChunkHash = FIoHash::HashBuffer(ReadResult.IoBuffer.GetView());

			FMemoryView EncodedBlocks = ReadResult.IoBuffer.GetView();
			uint64 RawChunkSize = 0;
			uint64 EncodedChunkSize = 0;
			for (const FIoStoreCompressedBlockInfo& BlockInfo : ReadResult.Blocks)
			{
				const uint64 EncodedBlockSize = Align(BlockInfo.CompressedSize, FAES::AESBlockSize);
				ContainerEntry.BlockSizes.Add(EncodedBlockSize);

				FMemoryView EncodedBlock = EncodedBlocks.Left(EncodedBlockSize);
				EncodedBlocks += EncodedBlock.GetSize();
				ContainerEntry.BlockHashes.Add(FIoHash::HashBuffer(EncodedBlock));

				EncodedChunkSize += EncodedBlockSize;
				RawChunkSize += BlockInfo.UncompressedSize;

				if (OnDemandToc.Header.CompressionFormat.IsEmpty() && BlockInfo.CompressionMethod != NAME_None)
				{
					OnDemandToc.Header.CompressionFormat = BlockInfo.CompressionMethod.ToString();
				}
			}

			if (EncodedChunkSize != ReadResult.IoBuffer.GetSize())
			{
				return FIoStatus(EIoErrorCode::ReadError, TEXT("Encoded chunk size does not match buffer"));
			}

			FOnDemandTocEntry& TocEntry = ContainerEntry.Entries.AddDefaulted_GetRef();
			TocEntry.ChunkId = ChunkInfo.Id;
			TocEntry.Hash = ChunkHash;
			TocEntry.RawHash = ChunkInfo.Hash.ToIoHash();
			TocEntry.RawSize = RawChunkSize;
			TocEntry.EncodedSize = EncodedChunkSize;
			TocEntry.BlockOffset = BlockOffset;
			TocEntry.BlockCount = BlockCount;

			if (ExistingChunks.Contains(TocEntry.Hash) == false)
			{
				const FString HashString = LexToString(ChunkHash);

				TStringBuilder<256> Key;
				Key << ChunksRelativePath
					<< TEXT("/") << HashString.Left(2)
					<< TEXT("/") << HashString
					<< TEXT(".iochunk");

				const bool bEnqueued = UploadQueue.Enqueue(Key.ToString(), ReadResult.IoBuffer);

				if (bEnqueued)
				{
					UE_LOG(LogIas, Display, TEXT("Uploaded chunk '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, Key.ToString());
				}
				else
				{
					return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to upload chunk"));
				}

				TotalUploadedChunks++;
				TotalUploadedBytes += ReadResult.IoBuffer.GetSize();
			}
		}

		{
			const FString UTocFilePath = FPaths::ChangeExtension(Path, TEXT(".utoc"));
			TArray<uint8> Buffer;
			if (!FFileHelper::LoadFileToArray(Buffer, *UTocFilePath))
			{
				return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to upload .utoc file"));
			}

			ContainerEntry.UTocHash = FIoHash::HashBuffer(Buffer.GetData(), Buffer.Num());
			TStringBuilder<256> Key;
			if (UploadParams.BucketPrefix.IsEmpty() == false)
			{
				Key << UploadParams.BucketPrefix.ToLower() << TEXT("/");
			}
			Key << LexToString(ContainerEntry.UTocHash) << TEXT(".utoc");

			const FS3PutObjectResponse Response = Client.TryPutObject(
				FS3PutObjectRequest{UploadParams.Bucket, Key.ToString(), MakeMemoryView(Buffer.GetData(), Buffer.Num())});

			if (Response.IsOk())
			{
				UE_LOG(LogIas, Display, TEXT("Uploaded '%s'"), *UTocFilePath);
			}
			else
			{
				return FIoStatus(EIoErrorCode::WriteError, FString::Printf(TEXT("Failed to upload '%s', StatusCode: %u"), *UTocFilePath, Response.StatusCode));
			}
		}
		
		UploadedFiles.Add(Path);
	}

	if (OnDemandToc.Containers.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("No container file(s) marked as on demand"));
	}

	if (!UploadQueue.Flush())
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to upload chunk(s)"));
	}

	FIoStoreUploadResult UploadResult;
	{
		FCbWriter Writer;
		Writer << OnDemandToc;

		FLargeMemoryWriter Ar;
		SaveCompactBinary(Ar, Writer.Save());

		UploadResult.TocHash = FIoHash::HashBuffer(Ar.GetView());
		TStringBuilder<256> Key;
		if (UploadParams.BucketPrefix.IsEmpty() == false)
		{
			Key << UploadParams.BucketPrefix.ToLower() << TEXT("/");
		}
		Key << LexToString(UploadResult.TocHash) << TEXT(".iochunktoc");

		UploadResult.TocPath = Key.ToString();

		const FS3PutObjectResponse Response = Client.TryPutObject(FS3PutObjectRequest{UploadParams.Bucket, Key.ToString(), Ar.GetView()});
		if (Response.IsOk())
		{
			UE_LOG(LogIas, Display, TEXT("Uploaded on demand TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, Key.ToString());
		}
		else
		{
			UE_LOG(LogIas, Warning, TEXT("Failed to upload TOC '%s/%s/%s', StatusCode: %u"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, Key.ToString(), Response.StatusCode);
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to upload TOC"));
		}
	}

	if (UploadParams.bDeleteContainerFiles)
	{
		for (const FString& TocPath : UploadedFiles)
		{
			const FString CasPath = FPaths::ChangeExtension(TocPath, TEXT(".ucas"));
			const TCHAR* FilePaths[] {*TocPath, *CasPath};
			for (const TCHAR* FilePath : FilePaths)
			{
				if (IFileManager::Get().FileExists(FilePath))
				{
					UE_LOG(LogIas, Display, TEXT("Deleting '%s'"), FilePath); 
					IFileManager::Get().Delete(FilePath);
				}
			}
		}
	}
	
	UE_LOG(LogIas, Display, TEXT("Done uploading %d container file(s), %d chunks of total %.2lf MiB"),
		UploadedFiles.Num(), TotalUploadedChunks, (double(TotalUploadedBytes) / 1024 / 1024));
	
	return UploadResult;
}
#endif // (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))

} // namespace UE

////////////////////////////////////////////////////////////////////////////////
class FIoStoreOnDemandModule
	: public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
};

IMPLEMENT_MODULE(FIoStoreOnDemandModule, IoStoreOnDemand);

void FIoStoreOnDemandModule::StartupModule()
{
	using namespace UE::IO::Private;

#if !WITH_EDITOR
	const TCHAR* CommandLine = FCommandLine::Get();

	UE::FOnDemandEndpoint Endpoint;
	
	FString UrlParam;
	if (FParse::Value(CommandLine, TEXT("Ias.TocUrl="), UrlParam))
	{
		FStringView UrlView(UrlParam);
		if (UrlView.StartsWith(TEXTVIEW("http://")) && UrlView.EndsWith(TEXTVIEW(".iochunktoc")))
		{
			int32 Delim = INDEX_NONE;
			if (UrlView.RightChop(7).FindChar(TEXT('/'), Delim))
			{
				Endpoint.ServiceUrl = UrlView.Left(7 +  Delim);
				Endpoint.TocPath = UrlView.RightChop(Endpoint.ServiceUrl.Len() + 1);
			}
		}
	}

	{
		FString EncryptionKey;
		if (FParse::Value(CommandLine, TEXT("IasEncryptionKey="), EncryptionKey))
		{
			FGuid KeyGuid;
			FAES::FAESKey Key;
			if (ParseEncryptionKeyParam(EncryptionKey, KeyGuid, Key))
			{
				UE::FEncryptionKeyManager::Get().AddKey(KeyGuid, Key);
			}
		}
	}

	if (!Endpoint.IsValid())
	{
		Endpoint = UE::FOnDemandEndpoint();
		FString ConfigFileName = TEXT("IoStoreOnDemand.ini");
		FString ConfigPath = FPaths::Combine(TEXT("Cloud"), ConfigFileName);
		FString ConfigContent = FPlatformMisc::LoadTextFileFromPlatformPackage(ConfigPath);

		if (ConfigContent.Len())
		{
			FConfigFile Config;
			Config.ProcessInputFileContents(ConfigContent, ConfigFileName);

			Config.GetString(TEXT("Endpoint"), TEXT("DistributionUrl"), Endpoint.DistributionUrl);
			Config.GetString(TEXT("Endpoint"), TEXT("ServiceUrl"), Endpoint.ServiceUrl);
			Config.GetString(TEXT("Endpoint"), TEXT("TocPath"), Endpoint.TocPath);
			
			if (Endpoint.DistributionUrl.EndsWith(TEXT("/")))
			{
				Endpoint.DistributionUrl = Endpoint.DistributionUrl.Left(Endpoint.DistributionUrl.Len() - 1);
			}
			
			if (Endpoint.ServiceUrl.EndsWith(TEXT("/")))
			{
				Endpoint.ServiceUrl = Endpoint.DistributionUrl.Left(Endpoint.ServiceUrl.Len() - 1);
			}

			if (Endpoint.TocPath.StartsWith(TEXT("/")))
			{
				Endpoint.TocPath.RightChopInline(1);
			}

			FString ContentKey;
			if (Config.GetString(TEXT("Endpoint"), TEXT("ContentKey"), ContentKey))
			{
				FGuid KeyGuid;
				FAES::FAESKey Key;
				if (ParseEncryptionKeyParam(ContentKey, KeyGuid, Key))
				{
					UE::FEncryptionKeyManager::Get().AddKey(KeyGuid, Key);
				}
			}
		}
	}

	if (!Endpoint.IsValid())
	{
		return;
	}

	FLatencyInjector::Initialize(CommandLine);

	Endpoint.EndpointType = UE::EOnDemandEndpointType::CDN;

	TSharedPtr<IIoCache> Cache;
	if (FFileIoCacheConfig Config = GetFileIoCacheConfig(CommandLine); Config.DiskQuota > 0)
	{
		Cache = MakeShareable(MakeFileIoCache(Config).Release());
	}

	TSharedPtr<UE::IOnDemandIoDispatcherBackend> Backend = UE::MakeOnDemandIoDispatcherBackend(Cache);
	Backend->Mount(Endpoint);
	FIoDispatcher::Get().Mount(Backend.ToSharedRef(), -10);
#endif // !WITH_EDITOR
}

void FIoStoreOnDemandModule::ShutdownModule()
{
}
