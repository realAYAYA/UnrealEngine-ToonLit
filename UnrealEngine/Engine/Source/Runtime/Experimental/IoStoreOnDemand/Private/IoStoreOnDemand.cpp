// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/IoStoreOnDemand.h"

#include "HAL/FileManager.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IO/IoChunkEncoding.h"
#include "IasCache.h"
#include "LatencyInjector.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/EncryptionKeyManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/PathViews.h"
#include "Modules/ModuleManager.h"
#include "OnDemandHttpClient.h"
#include "OnDemandIoDispatcherBackend.h"
#include "Serialization/Archive.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryWriter.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Statistics.h"
#include "String/LexFromString.h"

#if (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))
#include "Algo/Sort.h"
#include "Async/Async.h"
#include "Async/Mutex.h"
#include "Async/ParallelFor.h"
#include "Async/UniqueLock.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoStore.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/KeyChainUtilities.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "S3/S3Client.h"
#endif // (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))

DEFINE_LOG_CATEGORY(LogIas);

namespace UE::IO::IAS
{

FString GIasOnDemandTocExt = TEXT(".uondemandtoc");

bool GIasSuspendSystem = false;
static FAutoConsoleVariableRef CVar_SuspendSystemEnabled(
	TEXT("ias.SuspendSystem"),
	GIasSuspendSystem,
	TEXT("Suspends the use of the OnDemand system"),
	ECVF_ReadOnly
);

/** Temp cvar to allow the fallback url to be hotfixed in case of problems */
static FString GDistributedEndpointFallbackUrl;
static FAutoConsoleVariableRef CVar_DistributedEndpointFallbackUrl(
	TEXT("ias.DistributedEndpointFallbackUrl"),
	GDistributedEndpointFallbackUrl,
	TEXT("CDN url to be used if a distributed endpoint cannot be reached (overrides IoStoreOnDemand.ini)")
);

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
static bool ApplyEncryptionKeyFromString(const FString& GuidKeyPair)
{
	FGuid KeyGuid;
	FAES::FAESKey Key;

	if (ParseEncryptionKeyParam(GuidKeyPair, KeyGuid, Key))
	{
		// TODO: PAK and I/O store should share key manager
		FEncryptionKeyManager::Get().AddKey(KeyGuid, Key);
		FCoreDelegates::GetRegisterEncryptionKeyMulticastDelegate().Broadcast(KeyGuid, Key);

		return true;
	}
	else
	{
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
static bool TryParseConfigContent(const FString& ConfigContent, const FString& ConfigFileName, FOnDemandEndpoint& OutEndpoint)
{
	if (ConfigContent.IsEmpty())
	{
		return false;
	}

	FConfigFile Config;
	Config.ProcessInputFileContents(ConfigContent, ConfigFileName);

	Config.GetString(TEXT("Endpoint"), TEXT("DistributionUrl"), OutEndpoint.DistributionUrl);
	if (!OutEndpoint.DistributionUrl.IsEmpty())
	{
		Config.GetString(TEXT("Endpoint"), TEXT("FallbackUrl"), OutEndpoint.FallbackUrl);

		if (!GDistributedEndpointFallbackUrl.IsEmpty())
		{
			OutEndpoint.FallbackUrl = GDistributedEndpointFallbackUrl;
		}
	}
	
	Config.GetArray(TEXT("Endpoint"), TEXT("ServiceUrl"), OutEndpoint.ServiceUrls);
	Config.GetString(TEXT("Endpoint"), TEXT("TocPath"), OutEndpoint.TocPath);

	if (OutEndpoint.DistributionUrl.EndsWith(TEXT("/")))
	{
		OutEndpoint.DistributionUrl = OutEndpoint.DistributionUrl.Left(OutEndpoint.DistributionUrl.Len() - 1);
	}

	for (FString& ServiceUrl : OutEndpoint.ServiceUrls)
	{
		if (ServiceUrl.EndsWith(TEXT("/")))
		{
			ServiceUrl.LeftInline(ServiceUrl.Len() - 1);
		}
	}

	if (OutEndpoint.TocPath.StartsWith(TEXT("/")))
	{
		OutEndpoint.TocPath.RightChopInline(1);
	}

	FString ContentKey;
	if (Config.GetString(TEXT("Endpoint"), TEXT("ContentKey"), ContentKey))
	{
		ApplyEncryptionKeyFromString(ContentKey);
	}

	return OutEndpoint.IsValid();
}

////////////////////////////////////////////////////////////////////////////////
static bool TryParseConfigFileFromPlatformPackage(FOnDemandEndpoint& OutEndpoint)
{
	const FString ConfigFileName = TEXT("IoStoreOnDemand.ini");
	const FString ConfigPath = FPaths::Combine(TEXT("Cloud"), ConfigFileName);
	
	if (FPlatformMisc::FileExistsInPlatformPackage(ConfigPath))
	{
		const FString ConfigContent = FPlatformMisc::LoadTextFileFromPlatformPackage(ConfigPath);
		return TryParseConfigContent(ConfigContent, ConfigFileName, OutEndpoint);
	}
	else
	{
		return false;
	}
}

////////////////////////////////////////////////////////////////////////////////
bool TryParseConfigFile(const FString& ConfigPath, FOnDemandEndpoint& OutEndpoint)
{
	const FString ConfigFileName = TEXT("IoStoreOnDemand.ini");
	FString ConfigContent; 

	if (!FFileHelper::LoadFileToString(ConfigContent, &IPlatformFile::GetPlatformPhysical(), *ConfigPath))
	{
		return false;
	}

	return TryParseConfigContent(ConfigContent, ConfigFileName, OutEndpoint);
}

////////////////////////////////////////////////////////////////////////////////
static FIasCacheConfig GetIasCacheConfig(const TCHAR* CommandLine)
{
	FIasCacheConfig Ret;

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
	GetConfigInt(TEXT("JournalQuota"),			Ret.JournalQuota);
	GetConfigInt(TEXT("DemandThreshold"),		Ret.Demand.Threshold);
	GetConfigInt(TEXT("DemandBoost"),			Ret.Demand.Boost);
	GetConfigInt(TEXT("DemandSuperBoost"),		Ret.Demand.SuperBoost);
#undef GetConfigInt

#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("Ias.DropCache")))
	{
		Ret.DropCache = true;
	}
	if (FParse::Param(CommandLine, TEXT("Ias.NoCache")))
	{
		Ret.DiskQuota = 0;
	}
#endif

	return Ret;
}

////////////////////////////////////////////////////////////////////////////////

/** Utility for saving data to disk, similar to FFileHelper::SaveArrayToFile but supporting larger sizes */
static bool SaveArrayToFile(TArrayView64<uint8> Data, const TCHAR* Filename, uint32 WriteFlags = FILEWRITE_None)
{
	TUniquePtr<FArchive> Ar = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(Filename, WriteFlags));
	if (!Ar)
	{
		return false;
	}

	Ar->Serialize(static_cast<void*>(Data.GetData()), Data.Num());
	Ar->Close();

	return !Ar->IsError() && !Ar->IsCriticalError();
}

////////////////////////////////////////////////////////////////////////////////
FArchive& operator<<(FArchive& Ar, FTocMeta& Meta)
{
	Ar << Meta.EpochTimestamp;
	Ar << Meta.BuildVersion;
	Ar << Meta.TargetPlatform;
	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FTocMeta& Meta)
{
	Writer.BeginObject();
	Writer.AddInteger(UTF8TEXTVIEW("EpochTimestamp"), Meta.EpochTimestamp);
	Writer.AddString(UTF8TEXTVIEW("BuildVersion"), Meta.BuildVersion);
	Writer.AddString(UTF8TEXTVIEW("TargetPlatform"), Meta.TargetPlatform);
	Writer.EndObject();

	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, FTocMeta& OutMeta)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		OutMeta.EpochTimestamp = Obj["EpochTimestamp"].AsInt64();
		OutMeta.BuildVersion = FString(Obj["BuildVersion"].AsString());
		OutMeta.TargetPlatform = FString(Obj["TargetPlatform"].AsString());
		return true;
	}
	
	return false;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocHeader& Header)
{
	if (Ar.IsLoading() && Ar.TotalSize() < sizeof(FOnDemandTocHeader))
	{
		Ar.SetError();
		return Ar;
	}

	Ar << Header.Magic;
	if (Header.Magic != FOnDemandTocHeader::ExpectedMagic)
	{
		Ar.SetError();
		return Ar;
	}

	Ar << Header.Version;
	if (static_cast<EOnDemandTocVersion>(Header.Version) == EOnDemandTocVersion::Invalid)
	{
		Ar.SetError();
		return Ar;
	}

	if (uint32(Header.Version) > uint32(EOnDemandTocVersion::Latest))
	{
		Ar.SetError();
		return Ar;
	}

	Ar << Header.ChunkVersion;
	Ar << Header.BlockSize;
	Ar << Header.CompressionFormat;
	Ar << Header.ChunksDirectory;

	return Ar;
}

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

FArchive& operator<<(FArchive& Ar, FOnDemandTocEntry& Entry)
{
	Ar << Entry.Hash;
	Ar << Entry.ChunkId;
	Ar << Entry.RawSize;
	Ar << Entry.EncodedSize;
	Ar << Entry.BlockOffset;
	Ar << Entry.BlockCount;

	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocEntry& Entry)
{
	Writer.BeginObject();
	Writer.AddHash(UTF8TEXTVIEW("Hash"), Entry.Hash);
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

FArchive& operator<<(FArchive& Ar, FOnDemandTocContainerEntry& ContainerEntry)
{
	Ar << ContainerEntry.ContainerName;
	Ar << ContainerEntry.EncryptionKeyGuid;
	Ar << ContainerEntry.Entries;
	Ar << ContainerEntry.BlockSizes;
	Ar << ContainerEntry.BlockHashes;
	Ar << ContainerEntry.UTocHash;

	return Ar;
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
	for (uint32 BlockHash : ContainerEntry.BlockHashes)
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
			if (ArrayField.IsHash())
			{
				const FIoHash BlockHash = ArrayField.AsHash();
				OutContainer.BlockHashes.Add(*reinterpret_cast<const uint32*>(&BlockHash));
			}
			else
			{
				OutContainer.BlockHashes.Add(ArrayField.AsUInt32());
			}
		}

		OutContainer.UTocHash = Obj["UTocHash"].AsHash();

		return true;
	}

	return false;
}

bool FOnDemandTocSentinel::IsValid()
{
	return FMemory::Memcmp(&Data, FOnDemandTocSentinel::SentinelImg, FOnDemandTocSentinel::SentinelSize) == 0;
}

FArchive& operator<<(FArchive& Ar, FOnDemandTocSentinel& Sentinel)
{
	if (Ar.IsSaving())
	{	
		// We could just cast FOnDemandTocSentinel::SentinelImg to a non-const pointer but we can't be 
		// 100% sure that the FArchive won't change the data, even if it is in Saving mode. Since this 
		// isn't performance critical we will play it safe.
		uint8 Output[FOnDemandTocSentinel::SentinelSize];
		FMemory::Memcpy(Output, FOnDemandTocSentinel::SentinelImg, FOnDemandTocSentinel::SentinelSize);

		Ar.Serialize(&Output, FOnDemandTocSentinel::SentinelSize);
	}
	else
	{
		Ar.Serialize(&Sentinel.Data, FOnDemandTocSentinel::SentinelSize);
	}

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FOnDemandToc& Toc)
{
	Ar << Toc.Header;
	if (Ar.IsError())
	{
		return Ar;
	}

	Ar.SetCustomVersion(Toc.VersionGuid, int32(Toc.Header.Version), TEXT("OnDemandToc"));

	if (uint32(Toc.Header.Version) >= uint32(EOnDemandTocVersion::Meta))
	{
		Ar << Toc.Meta;
	}
	Ar << Toc.Containers;

	return Ar;
}

FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandToc& Toc)
{
	Writer.BeginObject();
	Writer << UTF8TEXTVIEW("Header") << Toc.Header;

	Writer.BeginArray(UTF8TEXTVIEW("Containers"));
	for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
	{
		Writer << Container;
	}
	Writer.EndArray();
	Writer.EndObject();
	
	return Writer;
}

FGuid FOnDemandToc::VersionGuid = FGuid("C43DD98F353F499D9A0767F6EA0155EB");

bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandToc& OutToc)
{
	if (FCbObjectView Obj = Field.AsObjectView())
	{
		if (!LoadFromCompactBinary(Obj["Header"], OutToc.Header))
		{
			return false;
		}

		if (uint32(OutToc.Header.Version) >= uint32(EOnDemandTocVersion::Meta))
		{
			if (!LoadFromCompactBinary(Obj["Meta"], OutToc.Meta))
			{
				return false;
			}
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

TIoStatusOr<FOnDemandToc> LoadTocFromUrl(const FString& ServiceUrl, const FString& TocPath, int32 RetryCount)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LoadTocFromUrl);

	FString ErrorMsg;

	for (int32 Attempt = 0; Attempt <= RetryCount; ++Attempt)
	{
		TUniquePtr<FHttpClient> HttpClient = FHttpClient::Create(ServiceUrl);
		TAnsiStringBuilder<256> Url;

		Url << "/" << TocPath;

		UE_LOG(LogIas, Log, TEXT("Fetching TOC '%s/%s' (#%d/%d)"), *ServiceUrl, *TocPath, Attempt + 1, RetryCount);

		TIoStatusOr<FOnDemandToc> Toc;
		HttpClient->Get(Url.ToView(), [&Toc, &ErrorMsg](TIoStatusOr<FIoBuffer> Response, uint64 DurationMs)
			{
				if (Response.IsOk())
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LoadTocFromEndpoint);
					FIoBuffer Buffer = Response.ConsumeValueOrDie();
					FOnDemandToc NewToc;

					FMemoryReaderView Ar(Buffer.GetView());
					Ar << NewToc;
					if (!Ar.IsError())
					{
						Toc = TIoStatusOr<FOnDemandToc>(MoveTemp(NewToc));
					}
					else
					{
						ErrorMsg = TEXT("Failed loading on demand TOC from compact binary");
					}
				}
				else
				{
					ErrorMsg = FString::Printf(TEXT("Failed fetching TOC, reason '%s'"), *Response.Status().ToString());
				}
			});

		while (HttpClient->Tick());

		if (Toc.IsOk())
		{
			return Toc;
		}
	}

	UE_LOG(LogIas, Error, TEXT("%s"), *ErrorMsg);

	return TIoStatusOr<FOnDemandToc>(FIoStatus(EIoErrorCode::NotFound));
}

////////////////////////////////////////////////////////////////////////////////
#if (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))

using FJsonWriter = TSharedPtr<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>>; 
using FJsonWriterFactory = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>;

////////////////////////////////////////////////////////////////////////////////
static void GetChunkObjectKeys(const FOnDemandToc& Toc, FString Prefix, TArray<FString>& OutKeys)
{
	if (Prefix.EndsWith(TEXT("/")))
	{
		Prefix = Prefix.Left(Prefix.Len() - 1);
	}

	TStringBuilder<256> Sb;
	for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
	{
		for (const FOnDemandTocEntry& Entry : Container.Entries)
		{
			Sb.Reset();
			const FString Hash = LexToString(Entry.Hash);
			Sb << Prefix << TEXT("/") << Toc.Header.ChunksDirectory << TEXT("/") << Hash.Left(2) << TEXT("/") << Hash << TEXT(".iochunk");
			OutKeys.Add(FString(Sb.ToString()).ToLower());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
enum class EOnDemandTocJsonOptions
{
	Header			= (1 << 0),
	TocEntries		= (1 << 1),
	BlockSizes		= (1 << 2),
	BlockHashes		= (1 << 3),
	All				= Header | TocEntries | BlockSizes | BlockHashes
};
ENUM_CLASS_FLAGS(EOnDemandTocJsonOptions);

static void ToJson(FJsonWriter JsonWriter, const FOnDemandToc& Toc, EOnDemandTocJsonOptions Options)
{
	JsonWriter->WriteObjectStart(TEXT("Header"));
	{
		JsonWriter->WriteValue(TEXT("Magic"), Toc.Header.Magic);
		JsonWriter->WriteValue(TEXT("Version"), uint64(Toc.Header.Version));
		JsonWriter->WriteValue(TEXT("BlockSize"), uint64(Toc.Header.BlockSize));
		JsonWriter->WriteValue(TEXT("CompressonFormat"), Toc.Header.CompressionFormat);
		JsonWriter->WriteValue(TEXT("ChunksDirectory"), Toc.Header.ChunksDirectory);
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteObjectStart(TEXT("Meta"));
	{
		const FDateTime Dt = FDateTime::FromUnixTimestamp(Toc.Meta.EpochTimestamp);
		JsonWriter->WriteValue(TEXT("DateTime"), Dt.ToString()), 
		JsonWriter->WriteValue(TEXT("BuildVersion"), Toc.Meta.BuildVersion);
		JsonWriter->WriteValue(TEXT("TargetPlatform"), Toc.Meta.TargetPlatform);
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->WriteArrayStart(TEXT("Containers"));
	{
		for (const FOnDemandTocContainerEntry& Container : Toc.Containers)
		{
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("Name"), Container.ContainerName);
			JsonWriter->WriteValue(TEXT("EncryptionKeyGuid"), Container.EncryptionKeyGuid);

			if (EnumHasAnyFlags(Options, EOnDemandTocJsonOptions::TocEntries))
			{
				JsonWriter->WriteArrayStart("Entries");
				for (const FOnDemandTocEntry& Entry : Container.Entries)
				{
					JsonWriter->WriteObjectStart();
					JsonWriter->WriteValue(TEXT("Hash"), LexToString(Entry.Hash));
					JsonWriter->WriteValue(TEXT("ChunkId"), LexToString(Entry.ChunkId));
					JsonWriter->WriteValue(TEXT("RawSize"), Entry.RawSize);
					JsonWriter->WriteValue(TEXT("EncodedSize"), Entry.EncodedSize);
					JsonWriter->WriteValue(TEXT("BlockOffset"), int32(Entry.BlockOffset));
					JsonWriter->WriteValue(TEXT("BlockCount"), int32(Entry.BlockCount));
					JsonWriter->WriteObjectEnd();
				}
				JsonWriter->WriteArrayEnd();
			}

			if (EnumHasAnyFlags(Options, EOnDemandTocJsonOptions::BlockSizes))
			{
				JsonWriter->WriteArrayStart("Blocks");
				{
					for (uint32 BlockSize : Container.BlockSizes)
					{
						JsonWriter->WriteValue(int32(BlockSize));
					}
				}
				JsonWriter->WriteArrayEnd();
			}

			if (EnumHasAnyFlags(Options, EOnDemandTocJsonOptions::BlockHashes))
			{
				JsonWriter->WriteArrayStart("BlockHashes");
				{
					for (const FIoBlockHash& BlockHash : Container.BlockHashes)
					{
						JsonWriter->WriteValue(int32(BlockHash));
					}
				}
				JsonWriter->WriteArrayEnd();
			}
			JsonWriter->WriteObjectEnd();
		}
	}
	JsonWriter->WriteArrayEnd();
}

////////////////////////////////////////////////////////////////////////////////
class FS3UploadQueue
{
public:
	FS3UploadQueue(FS3Client& Client, const FString& Bucket, int32 ThreadCount);
	bool Enqueue(FStringView Key, FIoBuffer Payload);
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

bool FS3UploadQueue::Enqueue(FStringView Key, FIoBuffer Payload)
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
				bEnqueued = Queue.Enqueue(FQueueEntry {FString(Key), Payload});
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
			UE_LOG(LogIas, Log, TEXT("Uploaded chunk '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Bucket, *Entry.Key);
		}
		else
		{
			TStringBuilder<256> ErrorResponse;
			Response.GetErrorMsg(ErrorResponse);

			UE_LOG(LogIas, Warning, TEXT("Failed to upload chunk '%s/%s/%s', StatusCode: %u, Error: %s"), *Client.GetConfig().ServiceUrl, *Bucket, *Entry.Key, Response.StatusCode, ErrorResponse.ToString());
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

struct FResolvedPaths
{
	FString ServiceUrl;
	FString TocPath;
	FString ChunkPrefix;
};

[[nodiscard]] static FResolvedPaths ResolvePaths(const FIoStoreUploadParams& UploadParams, FStringView TocFilename)
{
	FResolvedPaths Result;
	if (!UploadParams.BucketPrefix.IsEmpty())
	{
		Result.ServiceUrl = UploadParams.ServiceUrl;
		Result.TocPath = TocFilename;
		Result.ChunkPrefix = UploadParams.BucketPrefix;
	}
	else
	{
		// The configuration file should specify a service URL without any trailing
		// host path, i.e. http://{host:port}/{host-path}. Add the trailing path
		// to the TOC path to form the complete path the TOC from the host, i.e
		// TocPath={host-path}/{bucket}/{bucket-prefix}/{toc-hash}.uchunktoc

		FStringView ServiceUrl = UploadParams.ServiceUrl;
		FStringView TocPrefx;
		{
			// Find the first '//' then find the first '/' after that
			int32 Sep = INDEX_NONE;

			const int32 DoubleSlash = ServiceUrl.Find(TEXT("//"));
			if (DoubleSlash != INDEX_NONE)
			{
				Sep = ServiceUrl.Find(TEXT("/"), DoubleSlash + 2);
			}

			if (Sep != INDEX_NONE)
			{
				TocPrefx = ServiceUrl.RightChop(Sep + 1);
				ServiceUrl.LeftInline(Sep);
			}
		}

		Result.ServiceUrl = ServiceUrl;

		TStringBuilder<256> Prefix;
		FPathViews::Append(Prefix, TocPrefx, UploadParams.Bucket);

		Result.ChunkPrefix = Prefix;
		
		if (!TocFilename.IsEmpty())
		{
			TStringBuilder<256> TocPath;
			FPathViews::Append(TocPath, Prefix, TocFilename);

			Result.TocPath = TocPath;
		}
	}

	return Result;
}

[[nodiscard]] static FString ResolveChunksDirectory(const FIoStoreUploadParams& UploadParams)
{
	FResolvedPaths Paths = ResolvePaths(UploadParams, FStringView());
	return Paths.ChunkPrefix;
}

static FIoStatus WriteContainerFiles(FOnDemandToc& OnDemandToc, const TMap<FIoHash, FString>& UTocPaths)
{
	UE_LOG(LogIas, Display, TEXT("Attempting to write out %d '%s' files"), OnDemandToc.Containers.Num(), *GIasOnDemandTocExt);

	for (FOnDemandTocContainerEntry& Container : OnDemandToc.Containers)
	{
		const FString* UTocPath = UTocPaths.Find(Container.UTocHash);
		if (UTocPath == nullptr)
		{
			return FIoStatus(EIoErrorCode::Unknown, FString::Printf(TEXT("Could not find the original path for ondemand container '%s'"), *Container.ContainerName));
		}

		// Create a new FOnDemandToc to be written out and give it ownership of the FOnDemandTocContainerEntry that we want
		// associated with it.This way we don't need to write any specialized serialization code to split up the input FOnDemandToc
		// into one per container.
		FOnDemandToc ContainerToc;
		ContainerToc.Header = OnDemandToc.Header;
		ContainerToc.Meta = OnDemandToc.Meta;
		ContainerToc.Containers.Emplace(MoveTemp(Container));

		const FString OutputPath = FPathViews::ChangeExtension(*UTocPath, GIasOnDemandTocExt);

		TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*OutputPath));

		if (!Ar.IsValid())
		{
			return FIoStatus(EIoErrorCode::FileOpenFailed, FString::Printf(TEXT("Failed to open '%s' for write"), *OutputPath));
		}

		// TODO: We should consider adding a hash of the FOnDemandToc that can be computed at runtime on the loaded structure
		// (to avoid running over the file twice) to verify that nothing was corrupted.

		(*Ar) << ContainerToc;

		FOnDemandTocSentinel Sentinel;
		(*Ar) << Sentinel;

		if (Ar->IsError() || Ar->IsCriticalError())
		{
			return FIoStatus(EIoErrorCode::WriteError, FString::Printf(TEXT("Failed to write to '%s'"), *OutputPath));
		}

		// TODO: Should write an end of file sentinel here for a simple/easy file corruption check

		UE_LOG(LogIas, Display, TEXT("Wrote ondemand container file '%s' to disk"), *OutputPath);

		// Move the container entry back to OnDemandToc in case we want to use the data structure
		// beyond this point in the future.
		Container = MoveTemp(ContainerToc.Containers[0]);
	}

	return FIoStatus::Ok;
}

TIoStatusOr<FIoStoreUploadParams> FIoStoreUploadParams::Parse(const TCHAR* CommandLine)
{
	FIoStoreUploadParams Params;

	if (!FParse::Value(CommandLine, TEXT("Bucket="), Params.Bucket))
	{
		FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
	}

	FParse::Value(CommandLine, TEXT("BucketPrefix="), Params.BucketPrefix);
	FParse::Value(CommandLine, TEXT("ServiceUrl="), Params.ServiceUrl);
	FParse::Value(CommandLine, TEXT("DistributionUrl="), Params.DistributionUrl);
	FParse::Value(CommandLine, TEXT("FallbackUrl="), Params.FallbackUrl);
	FParse::Value(CommandLine, TEXT("Region="), Params.Region);
	FParse::Value(CommandLine, TEXT("AccessKey="), Params.AccessKey);
	FParse::Value(CommandLine, TEXT("SecretKey="), Params.SecretKey);
	FParse::Value(CommandLine, TEXT("SessionToken="), Params.SessionToken);
	FParse::Value(CommandLine, TEXT("CredentialsFile="), Params.CredentialsFile);
	FParse::Value(CommandLine, TEXT("CredentialsFileKeyName="), Params.CredentialsFileKeyName);
	FParse::Value(CommandLine, TEXT("BuildVersion="), Params.BuildVersion);
	FParse::Value(CommandLine, TEXT("TargetPlatform="), Params.TargetPlatform);
	FParse::Value(CommandLine, TEXT("EncryptionKeyName="), Params.EncryptionKeyName);
	Params.bDeleteContainerFiles = !FParse::Param(CommandLine, TEXT("KeepContainerFiles"));
	Params.bDeletePakFiles = !FParse::Param(CommandLine, TEXT("KeepPakFiles"));
	Params.bWriteTocToDisk = FParse::Param(CommandLine, TEXT("WriteTocToDisk"));

	// If we keep this feature we should allow the caller to set this path
	// themselves rather than rely on the config file location
	FString ConfigFilePath;
	FParse::Value(FCommandLine::Get(), TEXT("ConfigFilePath="), ConfigFilePath);
	Params.TocOutputDir = FPaths::GetPath(ConfigFilePath);

	if (FIoStatus Validation = Params.Validate(); !Validation.IsOk())
	{
		return Validation;
	}

	return Params;
}

FIoStatus FIoStoreUploadParams::Validate() const
{
	if (bWriteTocToDisk && TocOutputDir.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Cmdline param 'WriteToDisk' requires a valid 'ConfigFilePath' param as well"));
	}

	if (!AccessKey.IsEmpty() && SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid secret key"));
	}
	else if (AccessKey.IsEmpty() && !SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid access key"));
	}

	if (!CredentialsFile.IsEmpty() && CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credential file key name"));
	}

	if (ServiceUrl.IsEmpty() && Region.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Service URL or AWS region needs to be specified"));
	}

	return FIoStatus::Ok;
}

static void WriteConfigFile(
	const FIoStoreUploadParams& UploadParams,
	const FIoStoreUploadResult& UploadResult,
	const FKeyChain& KeyChain)
{
	FStringBuilderBase Sb;
	Sb << TEXT("[Endpoint]") << TEXT("\r\n");

	if (!UploadParams.DistributionUrl.IsEmpty())
	{
		Sb << TEXT("DistributionUrl=\"") << UploadParams.DistributionUrl << TEXT("\"\r\n");

		if (!UploadParams.FallbackUrl.IsEmpty())
		{
			Sb << TEXT("FallbackUrl=\"") << UploadParams.FallbackUrl << TEXT("\"\r\n");
		}
	}
	else
	{
		Sb << TEXT("ServiceUrl=\"") << UploadResult.ServiceUrl << TEXT("\"\r\n");
	}

	Sb << TEXT("TocPath=\"") << UploadResult.TocPath << TEXT("\"\r\n");

	// Temporary solution to get replays working with encrypted on demand content
	{
		if (!UploadParams.EncryptionKeyName.IsEmpty())
		{
			const TCHAR* EncryptionKeyName = *(UploadParams.EncryptionKeyName);

			TOptional<FNamedAESKey> EncryptionKey;
			for (const TPair<FGuid, FNamedAESKey>& KeyPair: KeyChain.GetEncryptionKeys())
			{
				if (KeyPair.Value.Name.Compare(EncryptionKeyName, ESearchCase::IgnoreCase) == 0)
				{
					EncryptionKey.Emplace(KeyPair.Value);
				}
			}

			if (EncryptionKey)
			{
				FString KeyString = FBase64::Encode(EncryptionKey.GetValue().Key.Key, FAES::FAESKey::KeySize);
				Sb << TEXT("ContentKey=\"") << EncryptionKey.GetValue().Guid.ToString() << TEXT(":") << KeyString << TEXT("\"\r\n");
			}
			else
			{
				UE_LOG(LogIoStore, Warning, TEXT("Failed to encryption key '%s' in key chain"), EncryptionKeyName);
			}
		}
	}

	FString ConfigFilePath = UploadParams.TocOutputDir / TEXT("IoStoreOnDemand.ini");
	UE_LOG(LogIoStore, Display, TEXT("Saving on demand config file '%s'"), *ConfigFilePath);
	if (FFileHelper::SaveStringToFile(Sb.ToString(), *ConfigFilePath) == false)
	{
		UE_LOG(LogIoStore, Error, TEXT("Failed to save on demand config file '%s'"), *ConfigFilePath);
	}
}

TIoStatusOr<FIoStoreUploadResult> UploadContainerFiles(
	const FIoStoreUploadParams& UploadParams,
	TConstArrayView<FString> ContainerFiles,
	const FKeyChain& KeyChain)
{
	TMap<FGuid, FAES::FAESKey> EncryptionKeys;
	for (const TPair<FGuid, FNamedAESKey>& KeyPair: KeyChain.GetEncryptionKeys())
	{
		EncryptionKeys.Add(KeyPair.Key, KeyPair.Value.Key);
	}

	struct FContainerStats
	{
		FString ContainerName;
		uint64 ChunkCount = 0;
		uint64 TotalBytes = 0;
		uint64 UploadedChunkCount = 0;
		uint64 UploadedBytes = 0;
	};
	TMap<FString, FContainerStats> ContainerSummary;

	const double StartTime = FPlatformTime::Seconds();

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
	uint64 TotalExistingTocs = 0;
	uint64 TotalExistingBytes = 0;
	{
		TStringBuilder<256> TocsKey;
		TocsKey << UploadParams.BucketPrefix << "/";

		UE_LOG(LogIas, Display, TEXT("	 '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, TocsKey.ToString());
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
			FMemoryReaderView Ar(TocResponse.Body.GetView());
			Ar << Toc;

			if (Ar.IsError()) 
			{
				Toc = FOnDemandToc{};
				if (LoadFromCompactBinary(FCbFieldView(TocResponse.Body.GetData()), Toc) == false)
				{
					UE_LOG(LogIas, Display, TEXT("Failed to load TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *UploadParams.Bucket, *TocInfo.Key);
					continue;
				}
			}

			for (const FOnDemandTocContainerEntry& ContainerEntry : Toc.Containers)
			{
				for (const FOnDemandTocEntry& TocEntry : ContainerEntry.Entries)
				{
					bool bIsAlreadyInSet;
					ExistingChunks.Add(TocEntry.Hash, &bIsAlreadyInSet);
					if (!bIsAlreadyInSet)
					{
						TotalExistingBytes += TocEntry.EncodedSize;
					}
				}
			}

			TotalExistingTocs++;
		}
	}

	FString ChunksRelativePath = UploadParams.BucketPrefix.IsEmpty() ? TEXT("Chunks") : FString::Printf(TEXT("%s/Chunks"), *UploadParams.BucketPrefix);
	ChunksRelativePath.ToLowerInline();

	bool bWritePerContainerToc = false;
	GConfig->GetBool(TEXT("Ias"), TEXT("ForceTocFromMountedPaks"), bWritePerContainerToc, GEngineIni);

	uint64 TotalUploadedChunks = 0;
	uint64 TotalUploadedBytes = 0;

	FOnDemandToc OnDemandToc;
	OnDemandToc.Header.ChunksDirectory = ResolveChunksDirectory(UploadParams);

	OnDemandToc.Containers.Reserve(ContainerFiles.Num());

	// Map of the .utoc paths that we have created ondemand containers for, indexed by their hash so that the paths can
	// be looked up later. We do not rely on the filename as we cannot be sure that there won't be duplicate file names
	// stored in different directories.
	TMap<FIoHash, FString> UTocPaths;

	TArray<FString> FilesToDelete;
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
			continue;
		}
		UE_LOG(LogIas, Display, TEXT("Uploading ondemand container '%s'"), *Path);

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
		if (EnumHasAnyFlags(ContainerFileReader.GetContainerFlags(), EIoContainerFlags::Encrypted))
		{
			ContainerEntry.EncryptionKeyGuid = LexToString(ContainerFileReader.GetEncryptionKeyGuid());
		}
		FContainerStats& ContainerStats = ContainerSummary.FindOrAdd(ContainerEntry.ContainerName);
		
		ContainerEntry.Entries.Reserve(ChunkInfos.Num());

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
				check(Align(BlockInfo.CompressedSize, FAES::AESBlockSize) == BlockInfo.AlignedSize);
				const uint64 EncodedBlockSize = BlockInfo.AlignedSize;
				ContainerEntry.BlockSizes.Add(uint32(BlockInfo.CompressedSize));

				FMemoryView EncodedBlock = EncodedBlocks.Left(EncodedBlockSize);
				EncodedBlocks += EncodedBlock.GetSize();
				ContainerEntry.BlockHashes.Add(FIoChunkEncoding::HashBlock(EncodedBlock));

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
			TocEntry.RawSize = RawChunkSize;
			TocEntry.EncodedSize = EncodedChunkSize;
			TocEntry.BlockOffset = BlockOffset;
			TocEntry.BlockCount = BlockCount;

			ContainerStats.ChunkCount++;
			ContainerStats.TotalBytes += EncodedChunkSize;

			if (ExistingChunks.Contains(TocEntry.Hash) == false)
			{
				const FString HashString = LexToString(ChunkHash);

				TStringBuilder<256> Key;
				Key << ChunksRelativePath
					<< TEXT("/") << HashString.Left(2)
					<< TEXT("/") << HashString
					<< TEXT(".iochunk");

				if (UploadQueue.Enqueue(Key, ReadResult.IoBuffer) == false)
				{
					return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to upload chunk"));
				}

				TotalUploadedChunks++;
				TotalUploadedBytes += EncodedChunkSize;
				ContainerStats.UploadedChunkCount++;
				ContainerStats.UploadedBytes += EncodedChunkSize;
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

			if (!Response.IsOk())
			{
				return FIoStatus(EIoErrorCode::WriteError, FString::Printf(TEXT("Failed to upload '%s', StatusCode: %u"), *UTocFilePath, Response.StatusCode));
			}
			
			UTocPaths.Add(ContainerEntry.UTocHash, UTocFilePath);

			UE_LOG(LogIas, Display, TEXT("Uploaded '%s'"), *UTocFilePath);	
		}

		if (UploadParams.bDeleteContainerFiles)
		{
			FilesToDelete.Add(Path);
			ContainerFileReader.GetContainerFilePaths(FilesToDelete);

			// We need the pak files in order to mount OnDemand toc files!
			if (UploadParams.bDeletePakFiles && !bWritePerContainerToc)
			{
				FilesToDelete.Add(FPaths::ChangeExtension(Path, TEXT(".pak")));
				FilesToDelete.Add(FPaths::ChangeExtension(Path, TEXT(".sig")));
			}
		}
	}

	if (OnDemandToc.Containers.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("No container file(s) marked as on demand"));
	}

	if (!UploadQueue.Flush())
	{
		return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to upload chunk(s)"));
	}

	OnDemandToc.Meta.EpochTimestamp = FDateTime::Now().ToUnixTimestamp();
	OnDemandToc.Meta.BuildVersion = UploadParams.BuildVersion;
	OnDemandToc.Meta.TargetPlatform = UploadParams.TargetPlatform;

	FIoStoreUploadResult UploadResult;
	{
		FLargeMemoryWriter Ar;
		Ar << OnDemandToc;

		UploadResult.TocHash = FIoHash::HashBuffer(Ar.GetView());
		TStringBuilder<256> Key;
		if (!UploadParams.BucketPrefix.IsEmpty())
		{
			Key << UploadParams.BucketPrefix.ToLower() << TEXT("/");
		}
		Key << LexToString(UploadResult.TocHash) << TEXT(".iochunktoc");
		
		FResolvedPaths ResolvedPaths = ResolvePaths(UploadParams, Key);

		UploadResult.ServiceUrl = MoveTemp(ResolvedPaths.ServiceUrl);
		UploadResult.TocPath = MoveTemp(ResolvedPaths.TocPath);

		UploadResult.TocSize = Ar.TotalSize();

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

		if (UploadParams.bWriteTocToDisk)
		{
			// Write a single .iochunktoc containing all on demand data for the current build
			TStringBuilder<512> OnDemandTocFilePath;
			FPathViews::Append(OnDemandTocFilePath, UploadParams.TocOutputDir, UploadResult.TocHash);
			OnDemandTocFilePath << TEXT(".iochunktoc");

			if (SaveArrayToFile(TArrayView64<uint8>(Ar.GetData(), Ar.TotalSize()), OnDemandTocFilePath.ToString()) == false)
			{
				UE_LOG(LogIoStore, Error, TEXT("Failed to save on demand toc file '%s'"), OnDemandTocFilePath.ToString());
			}
		}
	}

	if (bWritePerContainerToc)
	{
		// Write out separate .uondemandtoc files, one per.utoc containing ondemand data.
		FIoStatus Result = WriteContainerFiles(OnDemandToc, UTocPaths);
		if (!Result.IsOk())
		{
			return Result;
		}
	}

	for (const FString& Path : FilesToDelete)
	{
		if (IFileManager::Get().FileExists(*Path))
		{
			UE_LOG(LogIas, Display, TEXT("Deleting '%s'"), *Path); 
			IFileManager::Get().Delete(*Path);
		}
	}

	if (!UploadParams.TocOutputDir.IsEmpty())
	{
		WriteConfigFile(UploadParams, UploadResult, KeyChain);
	}

	{
		const double Duration = FPlatformTime::Seconds() - StartTime;
		
		UE_LOG(LogIas, Display, TEXT(""));
		UE_LOG(LogIas, Display, TEXT("------------------------------------------------- Upload Summary -------------------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-15s: %s"), TEXT("Service URL"), *UploadParams.ServiceUrl);
		UE_LOG(LogIas, Display, TEXT("%-15s: %s"), TEXT("Bucket"), *UploadParams.Bucket);
		UE_LOG(LogIas, Display, TEXT("%-15s: %s"), TEXT("TargetPlatform"), *UploadParams.TargetPlatform);
		UE_LOG(LogIas, Display, TEXT("%-15s: %s"), TEXT("BuildVersion"), *UploadParams.BuildVersion);
		UE_LOG(LogIas, Display, TEXT("%-15s: %s"), TEXT("TOC path"), *UploadResult.TocPath);
		UE_LOG(LogIas, Display, TEXT("%-15s: %.2lf KiB"), TEXT("TOC size"), double(UploadResult.TocSize) / 1024.0);
		UE_LOG(LogIas, Display, TEXT("%-15s: %.2lf second(s)"), TEXT("Duration"), Duration);
		UE_LOG(LogIas, Display, TEXT(""));
		
		UE_LOG(LogIas, Display, TEXT("%-50s %15s %15s %15s %15s"),
			TEXT("Container"), TEXT("Chunk(s)"), TEXT("Size (MiB)"), TEXT("Uploaded"), TEXT("Uploaded (MiB)"));
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));

		FContainerStats TotalStats;
		for (const TTuple<FString, FContainerStats>& Kv : ContainerSummary)
		{
			UE_LOG(LogIas, Display, TEXT("%-50s %15llu %15.2lf %15llu %15.2lf"),
				*Kv.Key, Kv.Value.ChunkCount, double(Kv.Value.TotalBytes) / 1024.0 / 1024.0, Kv.Value.UploadedChunkCount, double(Kv.Value.UploadedBytes) / 1024.0 / 1024.0);
			
			TotalStats.ChunkCount += Kv.Value.ChunkCount;
			TotalStats.TotalBytes += Kv.Value.TotalBytes;
			TotalStats.UploadedChunkCount += Kv.Value.UploadedChunkCount;
			TotalStats.UploadedBytes += Kv.Value.UploadedBytes;
		}
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-50s %15llu %15.2lf %15llu %15.2lf"),
			TEXT("Total"),TotalStats.ChunkCount, double(TotalStats.TotalBytes) / 1024.0 / 1024.0, TotalStats.UploadedChunkCount, double(TotalStats.UploadedBytes) / 1024.0 / 1024.0);
		UE_LOG(LogIas, Display, TEXT(""));
		
		UE_LOG(LogIas, Display, TEXT("%-50s %15s %15s %15s"), TEXT("Bucket"), TEXT("TOC(s)"), TEXT("Chunk(s)"), TEXT("MiB"));
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-50s %15llu %15d %15.2lf"), TEXT("Existing"), TotalExistingTocs, ExistingChunks.Num(), double(TotalExistingBytes) / 1024.0 / 1024.0);
		UE_LOG(LogIas, Display, TEXT("%-50s %15llu %15d %15.2lf"), TEXT("Uploaded"), 1, TotalStats.UploadedChunkCount, double(TotalStats.UploadedBytes) / 1024.0 / 1024.0);
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-50s %15llu %15d %15.2lf"), TEXT("Total"),
			TotalExistingTocs + 1, ExistingChunks.Num() + TotalStats.UploadedChunkCount, double(TotalExistingBytes + TotalStats.UploadedBytes) / 1024.0 / 1024.0);
		UE_LOG(LogIas, Display, TEXT(""));
	}
	
	return UploadResult;
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FIoStoreDownloadParams> FIoStoreDownloadParams::Parse(const TCHAR* CommandLine)
{
	FIoStoreDownloadParams Params;

	if (!FParse::Value(CommandLine, TEXT("Directory="), Params.Directory))
	{
		FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid output directory"));
	}

	if (!FParse::Value(CommandLine, TEXT("Bucket="), Params.Bucket))
	{
		FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
	}

	FParse::Value(CommandLine, TEXT("ServiceUrl="), Params.ServiceUrl);
	FParse::Value(CommandLine, TEXT("Region="), Params.Region);
	FParse::Value(CommandLine, TEXT("AccessKey="), Params.AccessKey);
	FParse::Value(CommandLine, TEXT("SecretKey="), Params.SecretKey);
	FParse::Value(CommandLine, TEXT("SessionToken="), Params.SessionToken);
	FParse::Value(CommandLine, TEXT("CredentialsFile="), Params.CredentialsFile);
	FParse::Value(CommandLine, TEXT("CredentialsFileKeyName="), Params.CredentialsFileKeyName);

	if (FIoStatus Validation = Params.Validate(); !Validation.IsOk())
	{
		return Validation;
	}

	return Params;
}

FIoStatus FIoStoreDownloadParams::Validate() const
{
	if (!AccessKey.IsEmpty() && SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid secret key"));
	}
	else if (AccessKey.IsEmpty() && !SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid access key"));
	}

	if (!CredentialsFile.IsEmpty() && CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credential file key name"));
	}

	if (ServiceUrl.IsEmpty() && Region.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Service URL or AWS region needs to be specified"));
	}

	return FIoStatus::Ok;
}

FIoStatus DownloadContainerFiles(const FIoStoreDownloadParams& DownloadParams, const FString& TocPath)
{
	struct FContainerStats
	{
		uint64 TocEntryCount = 0;
		uint64 TocRawSize = 0;
		uint64 RawSize = 0;
		uint64 CompressedSize = 0;
		EIoContainerFlags ContainerFlags = EIoContainerFlags::None;
		FName CompressionMethod;
	};
	TMap<FString, FContainerStats> ContainerSummary;

	const double StartTime = FPlatformTime::Seconds();

	FS3ClientConfig Config;
	Config.ServiceUrl = DownloadParams.ServiceUrl;
	Config.Region = DownloadParams.Region;
	
	FS3ClientCredentials Credentials;
	if (DownloadParams.CredentialsFile.IsEmpty() == false)
	{
		UE_LOG(LogIas, Display, TEXT("Loading credentials file '%s'"), *DownloadParams.CredentialsFile);
		FS3CredentialsProfileStore CredentialsStore = FS3CredentialsProfileStore::FromFile(DownloadParams.CredentialsFile);
		if (CredentialsStore.TryGetCredentials(DownloadParams.CredentialsFileKeyName, Credentials) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Failed to find valid credentials in credentials file"));
		}
		else
		{
			UE_LOG(LogIas, Display, TEXT("Found credentials for '%s'"), *DownloadParams.CredentialsFileKeyName);
		}
	}
	else
	{
		Credentials = FS3ClientCredentials(DownloadParams.AccessKey, DownloadParams.SecretKey, DownloadParams.SessionToken);
	}

	FS3Client Client(Config, Credentials);

	UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *DownloadParams.Bucket, *TocPath);
	FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
	{
		DownloadParams.Bucket,
		TocPath
	});

	if (TocResponse.IsOk() == false)
	{
		return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to fetch TOC"));
	}

	FOnDemandToc OnDemandToc;
	if (LoadFromCompactBinary(FCbFieldView(TocResponse.Body.GetData()), OnDemandToc) == false)
	{
		return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to load on demand TOC"));
	}

	FStringView BucketPrefix;
	{
		FStringView TocPathView = TocPath;
		int32 Index = INDEX_NONE;
		if (TocPathView.FindLastChar(TEXT('/'), Index))
		{
			BucketPrefix = TocPathView.Left(Index);
		}
	}

	for (const FOnDemandTocContainerEntry& ContainerEntry : OnDemandToc.Containers)
	{
		TStringBuilder<256> FileTocKey;
		if (BucketPrefix.IsEmpty() == false)
		{
			FileTocKey << BucketPrefix << TEXT("/");
		}
		FileTocKey << ContainerEntry.UTocHash << TEXT(".utoc");

		UE_LOG(LogIas, Display, TEXT("Fetching '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *DownloadParams.Bucket, FileTocKey.ToString());
		FS3GetObjectResponse Response = Client.GetObject(FS3GetObjectRequest
		{
			DownloadParams.Bucket,
			FString(FileTocKey).ToLower()
		});

		if (Response.IsOk() == false)
		{
			return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to load container .utoc file"));
		}

		const FString UTocPath = FString::Printf(TEXT("%s/%s.utoc"), *DownloadParams.Directory, *ContainerEntry.ContainerName);
		const FString UCasPath = FPaths::ChangeExtension(UTocPath, TEXT(".ucas")); 
		FContainerStats& ContainerStats = ContainerSummary.FindOrAdd(ContainerEntry.ContainerName);
		ContainerStats.TocRawSize = Response.Body.GetSize();

		if (TUniquePtr<FArchive> TocFile(IFileManager::Get().CreateFileWriter(*UTocPath)); TocFile.IsValid())
		{
			UE_LOG(LogIas, Display, TEXT("Writing '%s'"), *UTocPath);
			TocFile->Serialize((void*)Response.Body.GetData(), Response.Body.GetSize());
		}
		else
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXT("Failed to write container .utoc file"));
		}

		FIoStoreTocResource FileToc;
		if (FIoStatus Status = FIoStoreTocResource::Read(*UTocPath, EIoStoreTocReadOptions::ReadAll, FileToc); Status.IsOk() == false)
		{
			return Status;
		}

		const int32 TocEntryCount = int32(FileToc.Header.TocEntryCount);
		const uint64 CompressionBlockSize = FileToc.Header.CompressionBlockSize;
		ContainerStats.TocEntryCount = TocEntryCount;
		ContainerStats.ContainerFlags = FileToc.Header.ContainerFlags; 

		TMap<FIoChunkId, FIoHash> ChunkHashes;
		for (const FOnDemandTocEntry& Entry : ContainerEntry.Entries)
		{
			ChunkHashes.Add(Entry.ChunkId, Entry.Hash);
			ContainerStats.RawSize += Entry.RawSize;
		}

		TArray<int32> SortedIndices;
		for (int32 Idx = 0; Idx < TocEntryCount; ++Idx)
		{
			SortedIndices.Add(Idx);
		}

		Algo::Sort(SortedIndices, [&FileToc](int32 Lhs, int32 Rhs)
		{
			return FileToc.ChunkOffsetLengths[Lhs].GetOffset() < FileToc.ChunkOffsetLengths[Rhs].GetOffset();
		});

		FString ChunksRelativePath = BucketPrefix.IsEmpty()
			? FString::Printf(TEXT("IoChunksV%u"), OnDemandToc.Header.ChunkVersion)
			: FString::Printf(TEXT("%s/IoChunksV%u"), *FString(BucketPrefix), OnDemandToc.Header.ChunkVersion);

		TUniquePtr<FArchive> CasFile(IFileManager::Get().CreateFileWriter(*UCasPath));
		TArray<uint8> PaddingBuffer;

		for (int32 Idx = 0; Idx < TocEntryCount; ++Idx)
		{
			const int32 SortedIdx = SortedIndices[Idx];
			const FIoChunkId& ChunkId = FileToc.ChunkIds[SortedIdx];
			const FIoOffsetAndLength& OffsetLength = FileToc.ChunkOffsetLengths[SortedIdx];
			const FIoHash ChunkHash = ChunkHashes.FindChecked(ChunkId);
			const FString HashString = LexToString(ChunkHash);
			const int32 FirstBlockIndex = int32(OffsetLength.GetOffset() / CompressionBlockSize);
			const int32 LastBlockIndex = int32((Align(OffsetLength.GetOffset() + OffsetLength.GetLength(), CompressionBlockSize) - 1) / CompressionBlockSize);
			const FIoStoreTocCompressedBlockEntry& FirstBlock = FileToc.CompressionBlocks[FirstBlockIndex];
			const FName CompressionMethod = FileToc.CompressionMethods[FirstBlock.GetCompressionMethodIndex()];

			if (CompressionMethod.IsNone() == false && ContainerStats.CompressionMethod.IsNone())
			{
				ContainerStats.CompressionMethod = CompressionMethod;
			}

			TStringBuilder<256> ChunkKey;
			ChunkKey << ChunksRelativePath
				<< TEXT("/") << HashString.Left(2)
				<< TEXT("/") << HashString
				<< TEXT(".iochunk");

			UE_LOG(LogIas, Display, TEXT("Fetching '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *DownloadParams.Bucket, ChunkKey.ToString());
			FS3GetObjectResponse ChunkResponse = Client.GetObject(FS3GetObjectRequest
			{
				DownloadParams.Bucket,
				FString(ChunkKey).ToLower()
			});

			if (ChunkResponse.IsOk() == false)
			{
				return FIoStatus(EIoErrorCode::ReadError, TEXT("Failed to fetch chunk"));
			}

			const uint64 Padding = FirstBlock.GetOffset() - CasFile->Tell();
			if (Padding > PaddingBuffer.Num())
			{
				PaddingBuffer.SetNumZeroed(int32(Padding));
			}

			if (Padding > 0)
			{
				CasFile->Serialize(PaddingBuffer.GetData(), Padding);
			}

			UE_LOG(LogIas, Display, TEXT("Serializing chunk %d/%d '%s' -> '%s' (%llu B)"),
				Idx + 1, TocEntryCount, *HashString, *LexToString(ChunkId), ChunkResponse.Body.GetSize());

			check(CasFile->Tell() == FirstBlock.GetOffset());
			CasFile->Serialize((void*)ChunkResponse.Body.GetData(), ChunkResponse.Body.GetSize());
		}

		ContainerStats.CompressedSize = CasFile->Tell();
	}

	{
		const double Duration = FPlatformTime::Seconds() - StartTime;

		UE_LOG(LogIas, Display, TEXT(""));
		UE_LOG(LogIas, Display, TEXT("---------------------------------------- Download Summary --------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-40s: %s"), TEXT("Service URL"), *DownloadParams.ServiceUrl);
		UE_LOG(LogIas, Display, TEXT("%-40s: %s"), TEXT("Bucket"), *DownloadParams.Bucket);
		UE_LOG(LogIas, Display, TEXT("%-40s: %s"), TEXT("TOC"), *TocPath);
		UE_LOG(LogIas, Display, TEXT("%-40s: %.2lf second(s)"), TEXT("Duration"), Duration);
		UE_LOG(LogIas, Display, TEXT(""));

		UE_LOG(LogIas, Display, TEXT("%-30s %10s %15s %15s %15s %25s"),
			TEXT("Container"), TEXT("Flags"), TEXT("TOC Size (KB)"), TEXT("TOC Entries"), TEXT("Size (MB)"), TEXT("Compressed (MB)"));
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));
		
		FContainerStats TotalStats;
		for (const TTuple<FString, FContainerStats>& Kv : ContainerSummary)
		{
			const FString& ContainerName = Kv.Key;
			const FContainerStats& Stats = Kv.Value;

			FString CompressionInfo = TEXT("-");

			if (Stats.CompressionMethod != NAME_None)
			{
				double Procentage = (double(Stats.RawSize - Stats.CompressedSize) / double(Stats.RawSize)) * 100.0;
				CompressionInfo = FString::Printf(TEXT("%.2lf (%.2lf%% %s)"),
					(double)Stats.CompressedSize / 1024.0 / 1024.0,
					Procentage,
					*Stats.CompressionMethod.ToString());
			}

			FString ContainerSettings = FString::Printf(TEXT("%s/%s/%s/%s/%s"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Compressed) ? TEXT("C") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Encrypted) ? TEXT("E") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Signed) ? TEXT("S") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::Indexed) ? TEXT("I") : TEXT("-"),
				EnumHasAnyFlags(Stats.ContainerFlags, EIoContainerFlags::OnDemand) ? TEXT("O") : TEXT("-"));

			UE_LOG(LogIas, Display, TEXT("%-30s %10s %15.2lf %15llu %15.2lf %25s"),
				*ContainerName,
				*ContainerSettings,
				(double)Stats.TocRawSize / 1024.0,
				Stats.TocEntryCount,
				(double)Stats.RawSize / 1024.0 / 1024.0,
				*CompressionInfo);

			TotalStats.TocEntryCount += Stats.TocEntryCount;
			TotalStats.TocRawSize += Stats.TocRawSize;
			TotalStats.RawSize += Stats.RawSize;
			TotalStats.CompressedSize += Stats.CompressedSize;
		}
		UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------------"));
		UE_LOG(LogIas, Display, TEXT("%-30s %10s %15.2lf %15llu %15.2lf %25.2lf "),
			TEXT("Total"),
			TEXT(""),
			(double)TotalStats.TocRawSize / 1024.0,
			TotalStats.TocEntryCount,
			(double)TotalStats.RawSize / 1024.0 / 1024.0,
			(double)TotalStats.CompressedSize / 1024.0 / 1024.0);

		UE_LOG(LogIas, Display, TEXT(""));
		UE_LOG(LogIas, Display, TEXT("** Flags: (C)ompressed / (E)ncrypted / (S)igned) / (I)ndexed) / (O)nDemand **"));
		UE_LOG(LogIas, Display, TEXT(""));
	}

	return FIoStatus::Ok;
}

////////////////////////////////////////////////////////////////////////////////
TIoStatusOr<FIoStoreListTocsParams> FIoStoreListTocsParams::Parse(const TCHAR* CommandLine)
{
	FIoStoreListTocsParams Params;

	// Convenience argument to specify both bucket and bucket prefix
	// -BucketPath="mybucket/some/data/path" is equal to -Bucket="mybucket" -BucketPrefix="some/data/path
	FString BucketPath;
	if (FParse::Value(CommandLine, TEXT("-BucketPath="), BucketPath))
	{
		FStringView PathView = BucketPath;
		int32 Idx = INDEX_NONE;
		if (PathView.FindChar(TCHAR('/'), Idx))
		{
			Params.Bucket = PathView.Left(Idx);
			FStringView BucketPrefix = PathView.RightChop(Idx + 1);
			if (BucketPrefix.EndsWith(TEXT("/")))
			{
				BucketPrefix.LeftChopInline(1);
			}
			Params.BucketPrefix = BucketPrefix;
		}
		else
		{
			Params.Bucket = MoveTemp(BucketPath);
		}
	}

	if (Params.Bucket.IsEmpty())
	{
		if (!FParse::Value(CommandLine, TEXT("Bucket="), Params.Bucket))
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
		}
	}

	FParse::Value(CommandLine, TEXT("BucketPrefix="), Params.BucketPrefix);
	FParse::Value(CommandLine, TEXT("ServiceUrl="), Params.ServiceUrl);
	FParse::Value(CommandLine, TEXT("Region="), Params.Region);
	FParse::Value(CommandLine, TEXT("AccessKey="), Params.AccessKey);
	FParse::Value(CommandLine, TEXT("SecretKey="), Params.SecretKey);
	FParse::Value(CommandLine, TEXT("SessionToken="), Params.SessionToken);
	FParse::Value(CommandLine, TEXT("CredentialsFile="), Params.CredentialsFile);
	FParse::Value(CommandLine, TEXT("TocKey="), Params.TocKey);
	FParse::Value(CommandLine, TEXT("BuildVersion="), Params.BuildVersion);
	FParse::Value(CommandLine, TEXT("TargetPlatform="), Params.TargetPlatform);
	FParse::Value(CommandLine, TEXT("ChunkKeys="), Params.ChunkKeys);
	// JSON serialization options
	Params.bTocEntries = FParse::Param(CommandLine, TEXT("TocEntries"));
	Params.bBlockSizes = FParse::Param(CommandLine, TEXT("BlockSizes"));
	Params.bBlockHashes = FParse::Param(CommandLine, TEXT("BlockHashes"));

	if (!FParse::Value(CommandLine, TEXT("CredentialsFileKeyName="), Params.CredentialsFileKeyName))
	{
		Params.CredentialsFileKeyName = TEXT("default");
	}

	FParse::Value(CommandLine, TEXT("Json="), Params.OutFile);

	if (FParse::Value(CommandLine, TEXT("TocUrl="), Params.TocUrl))
	{
		FStringView UrlView(Params.TocUrl);
		if (UrlView.StartsWith(TEXTVIEW("http://")) && UrlView.EndsWith(TEXTVIEW(".iochunktoc")))
		{
			int32 Delim = INDEX_NONE;
			if (UrlView.RightChop(7).FindChar(TEXT('/'), Delim))
			{
				Params.ServiceUrl = FString(UrlView.Left(7 +  Delim));
				Params.TocKey = UrlView.RightChop(Params.ServiceUrl.Len() + 1);
			}
		}
	}

	if (FIoStatus Status = Params.Validate(); !Status.IsOk())
	{
		return Status;
	}

	return Params;
}

FIoStatus FIoStoreListTocsParams::Validate() const
{
	if (Bucket.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid bucket name"));
	}
	if (!AccessKey.IsEmpty() && SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid secret key"));
	}
	else if (AccessKey.IsEmpty() && !SecretKey.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid access key"));
	}

	if (!CredentialsFile.IsEmpty() && CredentialsFileKeyName.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Invalid credential file key name"));
	}

	if (TocUrl.IsEmpty() && ServiceUrl.IsEmpty() && Region.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Service URL or AWS region needs to be specified"));
	}

	return FIoStatus::Ok;
}

FIoStatus ListTocs(const FIoStoreListTocsParams& Params)
{
	struct FChunkStats
	{
		TSet<FIoHash> Chunks;
		uint64 TotalChunkSize = 0;

		void Add(const FIoHash& Hash, uint64 ChunkSize)
		{
			bool bExist = false;
			Chunks.Add(Hash, &bExist);
			if (!bExist)
			{
				TotalChunkSize += ChunkSize;
			}
		}
	};
	TMap<FString, FChunkStats> Stats;

	struct FTocDescription
	{
		FOnDemandToc Toc;
		FDateTime DateTime;
		FString Key;
		uint64 Size = 0;
		uint64 ChunkCount = 0;
		uint64 TotalChunkSize = 0;
	};

	const bool bFilteredQuery = !Params.TocKey.IsEmpty() || !Params.BuildVersion.IsEmpty();

	FS3ClientConfig Config;
	Config.ServiceUrl = Params.ServiceUrl;
	Config.Region = Params.Region;
	
	FS3ClientCredentials Credentials;
	if (Params.CredentialsFile.IsEmpty() == false)
	{
		UE_LOG(LogIas, Display, TEXT("Loading credentials file '%s'"), *Params.CredentialsFile);
		FS3CredentialsProfileStore CredentialsStore = FS3CredentialsProfileStore::FromFile(Params.CredentialsFile);
		if (CredentialsStore.TryGetCredentials(Params.CredentialsFileKeyName, Credentials) == false)
		{
			return FIoStatus(EIoErrorCode::InvalidParameter, TEXT("Failed to find valid credentials in credentials file"));
		}
		else
		{
			UE_LOG(LogIas, Display, TEXT("Found credentials for '%s'"), *Params.CredentialsFileKeyName);
		}
	}
	else
	{
		Credentials = FS3ClientCredentials(Params.AccessKey, Params.SecretKey, Params.SessionToken);
	}

	FS3Client Client(Config, Credentials);
	FMutex TocMutex;
	TArray<FTocDescription> Tocs;

	if (Params.TocUrl.IsEmpty())
	{
		FStringView PrefixView = Params.BucketPrefix;
		if (PrefixView.StartsWith(TEXT("/")))
		{
			PrefixView.RightChopInline(1);
		}

		TStringBuilder<256> Path;
		Path << PrefixView << TEXT("/");

		UE_LOG(LogIas, Display, TEXT("Fetching TOC's from '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.Bucket, Path.ToString());
		FS3ListObjectResponse Response = Client.ListObjects(FS3ListObjectsRequest
		{
			Params.Bucket,
			Path.ToString(),
			TEXT('/')
		});

		if (Response.Objects.IsEmpty())
		{
			UE_LOG(LogIas, Display, TEXT("Not TOC's found at '%s/%s/%s' (StatusCode: %u)"), *Client.GetConfig().ServiceUrl, *Params.Bucket, Path.ToString(), Response.StatusCode);
			return FIoStatus(EIoErrorCode::NotFound);
		}

		ParallelFor(Response.Objects.Num(), [&Params, &Client, &Response, &Tocs, &TocMutex](int32 Index)
		{
			const FS3Object& Obj = Response.Objects[Index];
			if (Obj.Key.EndsWith(TEXT("iochunktoc")) == false)
			{
				return;
			}

			if (!Params.TocKey.IsEmpty() && !Params.TocKey.Equals(FPaths::GetBaseFilename(Obj.Key), ESearchCase::IgnoreCase))
			{
				return;
			}

			UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.Bucket, *Obj.Key);
			FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
			{
				Params.Bucket,
				Obj.Key
			});

			if (TocResponse.IsOk() == false)
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to fetch TOC '%s/%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.Bucket, *Obj.Key);
				return;
			}

			FOnDemandToc Toc;
			FMemoryReaderView Ar(TocResponse.Body.GetView());
			Ar << Toc;

			if (Ar.IsError() || Toc.Header.Magic != FOnDemandTocHeader::ExpectedMagic)
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to serialize TOC '%s/%s/%s'. Header version/magic mimsatch"), *Client.GetConfig().ServiceUrl, *Params.Bucket, *Obj.Key);
				return;
			}

			if (!Params.BuildVersion.IsEmpty())
			{
				if (!Params.BuildVersion.Equals(Toc.Meta.BuildVersion, ESearchCase::IgnoreCase))
				{
					return;
				}
				if (!Params.TargetPlatform.IsEmpty() && !Params.TargetPlatform.Equals(Toc.Meta.TargetPlatform, ESearchCase::IgnoreCase))
				{
					return;
				}
			}

			FDateTime DateTime = FDateTime::FromUnixTimestamp(Toc.Meta.EpochTimestamp);
			FTocDescription Desc = FTocDescription 
			{
				.Toc = MoveTemp(Toc),
				.DateTime = DateTime,
				.Key = Obj.Key,
				.Size = Obj.Size
			};

			{
				TUniqueLock Lock(TocMutex);
				Tocs.Add(MoveTemp(Desc));
			}
		});
	}
	else
	{
		UE_LOG(LogIas, Display, TEXT("Fetching TOC '%s/%s'"), *Client.GetConfig().ServiceUrl, *Params.TocKey);
		FS3GetObjectResponse TocResponse = Client.GetObject(FS3GetObjectRequest
		{
			TEXT("/"),
			Params.TocKey
		});

		if (TocResponse.IsOk())
		{
			FOnDemandToc Toc;
			FMemoryReaderView Ar(TocResponse.Body.GetView());
			Ar << Toc;

			if (Toc.Header.Magic == FOnDemandTocHeader::ExpectedMagic)
			{
				FDateTime DateTime = FDateTime::FromUnixTimestamp(Toc.Meta.EpochTimestamp);
				Tocs.Add(FTocDescription 
				{
					.Toc = MoveTemp(Toc),
					.DateTime = DateTime,
					.Key = Params.TocKey,
					.Size = TocResponse.Body.GetSize()
				});
			}
			else
			{
				UE_LOG(LogIas, Warning, TEXT("Failed to serialize TOC '%s/%s'. Header magic mimsatch"), *Client.GetConfig().ServiceUrl, *Params.TocKey);
			}
		}
	}

	if (Tocs.IsEmpty())
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}

	for (FTocDescription& Desc : Tocs)
	{
		FChunkStats& BuildStats = Stats.FindOrAdd(Desc.Toc.Meta.BuildVersion);
		FChunkStats& BucketStats = Stats.FindOrAdd(Params.Bucket);
		for (const FOnDemandTocContainerEntry& Container : Desc.Toc.Containers)
		{
			for (const FOnDemandTocEntry& Entry : Container.Entries)
			{
				Desc.ChunkCount++;
				Desc.TotalChunkSize += Entry.EncodedSize;
				BucketStats.Add(Entry.Hash, Entry.EncodedSize);
				BuildStats.Add(Entry.Hash, Entry.EncodedSize);
			}
		}
	}

	Tocs.Sort([](const auto& LHS, const auto& RHS) { return LHS.DateTime > RHS.DateTime; });

	if (Params.OutFile.IsEmpty())
	{
		int32 Counter = 1;
		TStringBuilder<256> Url;

		for (const FTocDescription& Desc : Tocs)
		{
			Url.Reset();
			Url << Client.GetConfig().ServiceUrl << TEXT("/") << Params.Bucket << TEXT("/") << Desc.Key;

			UE_LOG(LogIas, Display, TEXT(""));
			UE_LOG(LogIas, Display, TEXT("%d) %s"), Counter++, *Desc.Key);
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"),TEXT("Date"), *Desc.DateTime.ToString());
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"), TEXT("BuildVersion"), *Desc.Toc.Meta.BuildVersion);
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"), TEXT("TargetPlatform"), *Desc.Toc.Meta.TargetPlatform);
			UE_LOG(LogIas, Display, TEXT("%-20s: %.2lf KiB"), TEXT("TocSize"), double(Desc.Size) / 1024.0);
			UE_LOG(LogIas, Display, TEXT("%-20s: %llu"), TEXT("ChunkCount"), Desc.ChunkCount);
			UE_LOG(LogIas, Display, TEXT("%-20s: %.2lf MiB"), TEXT("TotalChunkSize"), double(Desc.TotalChunkSize) / 1024.0 / 1024.0);
			UE_LOG(LogIas, Display, TEXT("%-20s: %s"), TEXT("Url"), Url.ToString());
		}
		UE_LOG(LogIas, Display, TEXT(""));
		
		if (bFilteredQuery == false)
		{
			TArray<FString> Keys;
			Stats.GetKeys(Keys);
			Keys.Sort();

			UE_LOG(LogIas, Display, TEXT("%-80s %15s %15s"), TEXT("BuildVersion"), TEXT("Chunk(s)"), TEXT("MiB"));
			UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
			for (const FString& Key : Keys)
			{
				if (Key != Params.Bucket)
				{
					const FChunkStats& ChunkStats = Stats.FindChecked(Key);
					UE_LOG(LogIas, Display, TEXT("%-80s %15llu %15.2f"), *Key, ChunkStats.Chunks.Num(), double(ChunkStats.TotalChunkSize) / 1024.0 / 1024.0);
				}
			}
			UE_LOG(LogIas, Display, TEXT("-------------------------------------------------------------------------------------------------------------------"));
			FChunkStats& BucketStats = Stats.FindOrAdd(Params.Bucket);
			UE_LOG(LogIas, Display, TEXT("%-80s %15llu %15.2f"), *Params.Bucket, BucketStats.Chunks.Num(), double(BucketStats.TotalChunkSize) / 1024.0 / 1024.0);
			UE_LOG(LogIas, Display, TEXT(""));
		}
	}
	else
	{
		FString Json;
		FJsonWriter JsonWriter = FJsonWriterFactory::Create(&Json);
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("ServiceUrl"), Client.GetConfig().ServiceUrl);
		JsonWriter->WriteValue(TEXT("Bucket"), Params.Bucket);
		JsonWriter->WriteValue(TEXT("BucketPrefix"), Params.BucketPrefix);

		JsonWriter->WriteArrayStart(TEXT("Tocs"));
		for (const FTocDescription& Desc : Tocs)
		{
			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("Key"), Desc.Key);
			JsonWriter->WriteValue(TEXT("ChunkCount"), Desc.ChunkCount);
			JsonWriter->WriteValue(TEXT("TotalChunkSize"), Desc.TotalChunkSize);

			EOnDemandTocJsonOptions JsonOptions = EOnDemandTocJsonOptions::Header;
			if (Params.bTocEntries)
			{
				JsonOptions |= EOnDemandTocJsonOptions::TocEntries;
			}
			if (Params.bBlockSizes)
			{
				JsonOptions |= EOnDemandTocJsonOptions::BlockSizes;
			}
			if (Params.bBlockHashes)
			{
				JsonOptions |= EOnDemandTocJsonOptions::BlockHashes;
			}
			ToJson(JsonWriter, Desc.Toc, JsonOptions);
			JsonWriter->WriteObjectEnd();
		}
		JsonWriter->WriteArrayEnd();

		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		UE_LOG(LogIas, Display, TEXT("Saving file '%s'"), *Params.OutFile);
		if (!FFileHelper::SaveStringToFile(Json, *Params.OutFile))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXTVIEW("Failed writing JSON file")); 
		}
	}

	if (Params.ChunkKeys.IsEmpty() == false)
	{
		FString Json;
		FJsonWriter JsonWriter = FJsonWriterFactory::Create(&Json);
		
		JsonWriter->WriteObjectStart();
		JsonWriter->WriteValue(TEXT("ServiceUrl"), Client.GetConfig().ServiceUrl);
		JsonWriter->WriteValue(TEXT("Bucket"), Params.Bucket);
		JsonWriter->WriteValue(TEXT("BucketPrefix"), Params.BucketPrefix);

		JsonWriter->WriteArrayStart(TEXT("Tocs"));
		for (const FTocDescription& Desc : Tocs)
		{
			TArray<FString> ObjKeys;
			GetChunkObjectKeys(Desc.Toc, Params.BucketPrefix, ObjKeys);

			JsonWriter->WriteObjectStart();
			JsonWriter->WriteValue(TEXT("Key"), Desc.Key);
			JsonWriter->WriteValue(TEXT("BuildVersion"), Desc.Toc.Meta.BuildVersion);
			JsonWriter->WriteValue(TEXT("TargetPlatform"), Desc.Toc.Meta.TargetPlatform);
			JsonWriter->WriteArrayStart(TEXT("ChunkKeys"));
			for (const FString& Key : ObjKeys)
			{
				JsonWriter->WriteValue(Key);
			}
			JsonWriter->WriteArrayEnd();
			JsonWriter->WriteObjectEnd();
		}
		JsonWriter->WriteArrayEnd();

		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();

		UE_LOG(LogIas, Display, TEXT("Saving chunk key(s) '%s'"), *Params.ChunkKeys);
		if (!FFileHelper::SaveStringToFile(Json, *Params.ChunkKeys))
		{
			return FIoStatus(EIoErrorCode::WriteError, TEXTVIEW("Failed writing JSON file")); 
		}
	}

	return FIoStatus::Ok;
}

#endif // (PLATFORM_DESKTOP && (IS_PROGRAM || WITH_EDITOR))



////////////////////////////////////////////////////////////////////////////////
void FIoStoreOnDemandModule::SetBulkOptionalEnabled(bool bInEnabled)
{
	if (Backend.IsValid())
	{
		Backend->SetBulkOptionalEnabled(bInEnabled);
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Deferring call to FIoStoreOnDemandModule::SetBulkOptionalEnabled(%s)"), bInEnabled ? TEXT("true") : TEXT("false"));
		DeferredBulkOptionalEnabled = bInEnabled;
	}
}

void FIoStoreOnDemandModule::SetEnabled(bool bInEnabled)
{
	if (Backend.IsValid())
	{
		Backend->SetEnabled(bInEnabled);
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Deferring call to FIoStoreOnDemandModule::SetEnabled(%s)"), bInEnabled ? TEXT("true") : TEXT("false"));
		DeferredEnabled = bInEnabled;
	}
}

void FIoStoreOnDemandModule::AbandonCache()
{
	if (Backend.IsValid())
	{
		Backend->AbandonCache();
	}
	else
	{
		UE_LOG(LogIas, Log, TEXT("Deferring call to FIoStoreOnDemandModule::AbandonCache"));
		DeferredAbandonCache = true;
	}
}

bool FIoStoreOnDemandModule::IsEnabled() const
{
	return Backend.IsValid()? Backend->IsEnabled():DeferredAbandonCache.IsSet();
}

void FIoStoreOnDemandModule::ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const
{
	if (Backend.IsValid())
	{
		Backend->ReportAnalytics(OutAnalyticsArray);
	}
}

void FIoStoreOnDemandModule::InitializeInternal()
{
	LLM_SCOPE_BYTAG(Ias);

#if WITH_EDITOR
	bool bEnabledInEditor = false;
	GConfig->GetBool(TEXT("Ias"), TEXT("EnableInEditor"), bEnabledInEditor, GEngineIni);

	if (!bEnabledInEditor)
	{
		return;
	}
#endif //WITH_EDITOR

	const TCHAR* CommandLine = FCommandLine::Get();
	
#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("NoIas")))
	{
		return;
	}
#endif

	// Make sure we haven't called initialize before
	check(!Backend.IsValid());

	FOnDemandEndpoint Endpoint;
	
	FString UrlParam;
	if (FParse::Value(CommandLine, TEXT("Ias.TocUrl="), UrlParam))
	{
		FStringView UrlView(UrlParam);
		if (UrlView.StartsWith(TEXTVIEW("http://")) && UrlView.EndsWith(TEXTVIEW(".iochunktoc")))
		{
			int32 Delim = INDEX_NONE;
			if (UrlView.RightChop(7).FindChar(TEXT('/'), Delim))
			{
				Endpoint.ServiceUrls.Add(FString(UrlView.Left(7 +  Delim)));
				Endpoint.TocPath = UrlView.RightChop(Endpoint.ServiceUrls[0].Len() + 1);

				// Since the user has provided a url to download the .iochunktoc from we should
				// assume that they want to download it rather than use anything on disk.
				Endpoint.bForceTocDownload = true;
			}
		}
	}

	{
		FString EncryptionKey;
		if (FParse::Value(CommandLine, TEXT("Ias.EncryptionKey="), EncryptionKey))
		{
			ApplyEncryptionKeyFromString(EncryptionKey);
		}
	}

	if (!Endpoint.IsValid())
	{
		Endpoint = FOnDemandEndpoint();
		if (!TryParseConfigFileFromPlatformPackage(Endpoint))
		{
			return;
		}
	}

	FLatencyInjector::Initialize(CommandLine);

	TUniquePtr<IIasCache> Cache;
	FIasCacheConfig CacheConfig = GetIasCacheConfig(CommandLine);
	CacheConfig.DropCache = DeferredAbandonCache.Get(CacheConfig.DropCache);
	if (CacheConfig.DiskQuota > 0)
	{
		if (FPaths::HasProjectPersistentDownloadDir())
		{
			FString CacheDir = FPaths::ProjectPersistentDownloadDir();
			Cache = MakeIasCache(*CacheDir, CacheConfig);
		}
	}
	if (!Cache.IsValid())
	{
		UE_LOG(LogIas, Log, TEXT("File cache disabled - streaming only (%s)"),
			(CacheConfig.DiskQuota > 0) ? TEXT("init-fail") : TEXT("zero-quota"));
	}

	Backend = MakeOnDemandIoDispatcherBackend(MoveTemp(Cache));
	Backend->Mount(Endpoint);
	int32 BackendPriority = -10;
#if !UE_BUILD_SHIPPING
	if (FParse::Param(CommandLine, TEXT("Ias")))
	{
		// Bump the priority to be higher then the file system backend
		BackendPriority = 10;
	}
#endif

	// Setup any states changes issued before initialization
	if (DeferredEnabled.IsSet())
	{
		Backend->SetEnabled(*DeferredEnabled);
	}
	if (DeferredBulkOptionalEnabled.IsSet())
	{
		Backend->SetBulkOptionalEnabled(*DeferredBulkOptionalEnabled);
	}
	
	FIoDispatcher::Get().Mount(Backend.ToSharedRef(), BackendPriority);
}
	
void FIoStoreOnDemandModule::StartupModule()
{
#if !UE_IAS_CUSTOM_INITIALIZATION

	if (!GIasSuspendSystem)
	{
		InitializeInternal();
	}
	else
	{
		UE_LOG(LogIas, Display, TEXT("The IoStoreOnDemand module has been remotely disabled by the 'ias.SuspendSystemEnabled' cvar"));
	}

#endif // !UE_IAS_CUSTOM_INITIALIZATION
}

void FIoStoreOnDemandModule::ShutdownModule()
{
}

#if UE_IAS_CUSTOM_INITIALIZATION

EOnDemandInitResult FIoStoreOnDemandModule::Initialize()
{
	if (GIasSuspendSystem)
	{
		UE_LOG(LogIas, Display, TEXT("The IoStoreOnDemand module has been remotely disabled by the 'ias.SuspendSystemEnabled' cvar"));
		return EOnDemandInitResult::Suspended;
	}

	InitializeInternal();

	return Backend.IsValid() ? EOnDemandInitResult::Success : EOnDemandInitResult::Disabled;
};

#endif // UE_IAS_CUSTOM_INITIALIZATION

} // namespace UE::IO::IAS

////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_MODULE(UE::IO::IAS::FIoStoreOnDemandModule, IoStoreOnDemand);
