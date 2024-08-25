// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "IO/IoChunkId.h"
#include "IO/IoHash.h"
#include "IO/IoStatus.h"
#include "Misc/Guid.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

#if (IS_PROGRAM || WITH_EDITOR)
#include "Containers/Map.h"
#include "Misc/AES.h"
#endif // (IS_PROGRAM || WITH_EDITOR)

#define UE_API IOSTOREONDEMAND_API

class FArchive;
class FCbFieldView;
class FCbWriter;
struct FKeyChain;
class IIoStoreWriter;
struct FAnalyticsEventAttribute;
struct FIoContainerSettings;
struct FIoStoreWriterSettings;
namespace UE::IO::IAS { struct FOnDemandEndpoint; }
using FIoBlockHash = uint32;

// Custom initialization allows users to control when
// the system should be initialized.
#if !defined(UE_IAS_CUSTOM_INITIALIZATION)
	#define UE_IAS_CUSTOM_INITIALIZATION 0
#endif

UE_API DECLARE_LOG_CATEGORY_EXTERN(LogIas, Log, All);

namespace UE::IO::IAS
{

////////////////////////////////////////////////////////////////////////////////

bool TryParseConfigFile(const FString& ConfigPath, FOnDemandEndpoint& OutEndpoint);

////////////////////////////////////////////////////////////////////////////////
enum class EOnDemandTocVersion : uint32
{
	Invalid			= 0,
	Initial			= 1,
	UTocHash		= 2,
	BlockHash32		= 3,
	NoRawHash		= 4,
	Meta			= 5,

	LatestPlusOne,
	Latest			= (LatestPlusOne - 1)
};

enum class EOnDemandChunkVersion : uint32
{
	Invalid			= 0,
	Initial			= 1,

	LatestPlusOne,
	Latest			= (LatestPlusOne - 1)
};

struct FTocMeta
{
	int64 EpochTimestamp = 0;
	FString BuildVersion;
	FString TargetPlatform;

	UE_API friend FArchive& operator<<(FArchive& Ar, FTocMeta& Meta);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FTocMeta& Meta);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FTocMeta& OutMeta);

struct FOnDemandTocHeader
{
	static constexpr uint64 ExpectedMagic = 0x6f6e64656d616e64; // ondemand

	uint64 Magic = ExpectedMagic;
	uint32 Version = uint32(EOnDemandTocVersion::Latest);
	uint32 ChunkVersion = uint32(EOnDemandChunkVersion::Latest);
	uint32 BlockSize = 0;
	FString CompressionFormat;
	FString ChunksDirectory;
	
	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocHeader& Header);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocHeader& Header);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocHeader& OutTocHeader);

struct FOnDemandTocEntry
{
	FIoHash Hash = FIoHash::Zero;
	FIoChunkId ChunkId = FIoChunkId::InvalidChunkId;
	uint64 RawSize = 0;
	uint64 EncodedSize = 0;
	uint32 BlockOffset = ~uint32(0);
	uint32 BlockCount = 0; 
	
	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocEntry& Entry);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocEntry& Entry);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocEntry& OutTocEntry);

struct FOnDemandTocContainerEntry
{
	FString ContainerName;
	FString EncryptionKeyGuid;
	TArray<FOnDemandTocEntry> Entries;
	TArray<uint32> BlockSizes;
	TArray<FIoBlockHash> BlockHashes;

	/** Hash of the .utoc file (on disk) used to generate this data */
	FIoHash UTocHash;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocContainerEntry& ContainerEntry);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandTocContainerEntry& ContainerEntry);
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandTocContainerEntry& OutContainer);

struct FOnDemandTocSentinel
{
public:
	static constexpr inline char SentinelImg[] = "-[]--[]--[]--[]-";
	static constexpr uint32 SentinelSize = 16;

	bool IsValid();

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandTocSentinel& Sentinel);

private:
	uint8 Data[SentinelSize] = { 0 };
};

struct FOnDemandToc
{
	FOnDemandToc() = default;
	~FOnDemandToc() = default;

	FOnDemandToc(FOnDemandToc&&) = default;
	FOnDemandToc& operator= (FOnDemandToc&&) = default;

	// Copying this structure would be quite expensive so we want to make sure that it doesn't happen.

	FOnDemandToc(const FOnDemandToc&) = delete;
	FOnDemandToc&  operator= (const FOnDemandToc&) = delete;

	FOnDemandTocHeader Header;
	FTocMeta Meta;
	TArray<FOnDemandTocContainerEntry> Containers;

	UE_API friend FArchive& operator<<(FArchive& Ar, FOnDemandToc& Toc);
	UE_API friend FCbWriter& operator<<(FCbWriter& Writer, const FOnDemandToc& Toc);

	static FGuid VersionGuid;
};

UE_API bool LoadFromCompactBinary(FCbFieldView Field, FOnDemandToc& OutToc);

TIoStatusOr<FOnDemandToc> LoadTocFromUrl(const FString& ServiceURL, const FString& TocPath, int32 RetryCount);

#if (IS_PROGRAM || WITH_EDITOR)

////////////////////////////////////////////////////////////////////////////////
struct FIoStoreUploadParams
{
	FString ServiceUrl;
	FString DistributionUrl;
	FString FallbackUrl;
	FString Bucket;
	FString BucketPrefix;
	FString Region; 
	FString AccessKey;
	FString SecretKey;
	FString SessionToken;
	FString CredentialsFile;
	FString CredentialsFileKeyName;
	FString BuildVersion;
	FString TargetPlatform;
	FString EncryptionKeyName;
	int32 MaxConcurrentUploads = 16;
	bool bDeleteContainerFiles = true;
	bool bDeletePakFiles = true;

	/** If we should write out the .iochunktoc to disk as well as uploading it. */
	bool bWriteTocToDisk = false;
	/** Where the .iochunktoc file should be written out. */
	FString TocOutputDir;

	UE_API static TIoStatusOr<FIoStoreUploadParams> Parse(const TCHAR* CommandLine);
	FIoStatus Validate() const;
};

/** Results from uploading a FOnDemandToc */
struct FIoStoreUploadResult
{
	/** Hash of the toc when written as a binary blob */
	FIoHash TocHash;
	
	/** Url of the service that the toc was uploaded too */
	FString ServiceUrl;
	/** Path of the toc on the service */
	FString TocPath;

	/** Size (in bytes) of the toc when written as a binary blob */
	uint64 TocSize = 0;
};

UE_API TIoStatusOr<FIoStoreUploadResult> UploadContainerFiles(
	const FIoStoreUploadParams& UploadParams,
	TConstArrayView<FString> ContainerFiles,
	const FKeyChain& KeyChain);

////////////////////////////////////////////////////////////////////////////////
struct FIoStoreDownloadParams
{
	FString Directory;
	FString ServiceUrl;
	FString Bucket;
	FString Region; 
	FString AccessKey;
	FString SecretKey;
	FString SessionToken;
	FString CredentialsFile;
	FString CredentialsFileKeyName;
	int32 MaxConcurrentDownloads = 16;
	
	UE_API static TIoStatusOr<FIoStoreDownloadParams> Parse(const TCHAR* CommandLine);
	FIoStatus Validate() const;
};

UE_API FIoStatus DownloadContainerFiles(const FIoStoreDownloadParams& DownloadParams, const FString& TocPath);

/**
 * Parameters for listing uploaded TOC file(s) from an S3 compatible endpoint.
 *
 * Example usage:
 *
 * 1) Print available TOC's from a local server to standard out.
 * UnrealPak.exe -ListTocs -ServiceUrl="http://10.24.101.92:9000" -Bucket=<bucketname> -BucketPrefix=<some/path/to/data> -AccessKey=<accesskey> -SecretKey=<secretkey>
 *
 * 2) Print available TOC's from AWS S3.
 * UnrealPak.exe -Region="us-east-1" -BucketPath=<mybucket/some/data/path/> -CredentialsFile=<path/to/credentials.txt> -BuildVersion=<version> -Json=<path/to/file.json>
 * 
 * 3) Serialize all TOC's matching a specific build version to JSON:
 * UnrealPak.exe -Region="us-east-1" -BucketPath=<mybucket/some/data/path/> -CredentialsFile=<path/to/credentials.txt> -BuildVersion=<version> -Json=<path/to/file.json>
 *
 * 4) Serialize all chunk object key(s) to JSON.
 * UnrealPak.exe -Region="us-east-1" -BucketPath=<mybucket/some/data/path/> -CredentialsFile=<path/to/credentials.txt> -BuildVersion=<version> -ChunkKeys=<path/to/file.json>
 *
 * 5) Fetch a TOC from a public CDN.
 * UnrealPak.exe -ListTocs -TocUrl=<http://some.public.endpoint.net/path/to/1a32076ca12bfc6feb982ffb064d18f28156606c.iochunktoc>
 *
 * Parameters: -TocEntries, -BlockSizes and -BlockHashes controls what to include when serializing TOC's to JSON.
 *
 * Credentials file example:
 *
 * [default]
 * aws_access_key_id="<key>"
 * aws_secret_access_key="<key>
 * aws_session_token="<token>"
 *
 * Note: All values must be surounded with "".
 */
struct FIoStoreListTocsParams
{
	FString OutFile;
	FString ServiceUrl;
	FString Bucket;
	FString BucketPrefix;
	FString Region; 
	FString AccessKey;
	FString SecretKey;
	FString SessionToken;
	FString CredentialsFile;
	FString CredentialsFileKeyName;
	FString TocUrl;
	FString TocKey;
	FString BuildVersion;
	FString TargetPlatform;
	FString ChunkKeys;
	bool bTocEntries = false;
	bool bBlockSizes = false;
	bool bBlockHashes = false;

	UE_API static TIoStatusOr<FIoStoreListTocsParams> Parse(const TCHAR* CommandLine);
	FIoStatus Validate() const;
};

UE_API FIoStatus ListTocs(const FIoStoreListTocsParams& Params);

#endif // (IS_PROGRAM || WITH_EDITOR)

class IOnDemandIoDispatcherBackend;

#if UE_IAS_CUSTOM_INITIALIZATION

/** Result of calling FIoStoreOnDemandModule::Initialize */
enum class EOnDemandInitResult
{
	/** The module initialized correctly and can be used */
	Success = 0,
	/** The module is disabled as OnDemand data is not required for the current process*/
	Disabled,
	/** The module was unable to start up correctly due to an unexpected error */
	Error,

	/**
	 * The use of the module has been suspended, if possible calling systems should activate alternative ways
	 * to access the OnDemand data. This option is temporary and not intended for general use.
	 */
	Suspended
};

#endif // UE_IAS_CUSTOM_INITIALIZATION

class FIoStoreOnDemandModule
	: public IModuleInterface
{
private:
	void InitializeInternal();
	TSharedPtr<IOnDemandIoDispatcherBackend> Backend;
	// Deferred state requests if called before backend
	// is initialized
	TOptional<bool> DeferredEnabled;
	TOptional<bool> DeferredAbandonCache;
	TOptional<bool> DeferredBulkOptionalEnabled;

public:
	UE_API void SetBulkOptionalEnabled(bool bInEnabled);
	UE_API void SetEnabled(bool bInEnabled);
	UE_API bool IsEnabled() const;
	UE_API void AbandonCache();

	UE_API void ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
#if UE_IAS_CUSTOM_INITIALIZATION
	UE_API EOnDemandInitResult Initialize();
#endif //UE_IAS_CUSTOM_INITIALIZATION
};

} // namespace UE::IO::IAS

#undef UE_API
