// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnsyncCore.h"
#include "UnsyncError.h"
#include "UnsyncRemote.h"
#include "UnsyncSocket.h"
#include "UnsyncThread.h"
#include "UnsyncUtil.h"

#include <functional>
#include <mutex>
#include <unordered_map>
#include <optional>

namespace unsync {

struct FDirectoryManifest;
struct FAuthDesc;
struct FHttpConnection;

enum class EDownloadRetryMode
{
	Retry,	// potentially recoverable error (caller can retry)
	Abort,	// unrecoverable error
};

struct FDownloadError : FError
{
	FDownloadError() = default;
	FDownloadError(EDownloadRetryMode InRetryMode) : RetryMode(InRetryMode) {}

	EDownloadRetryMode RetryMode = EDownloadRetryMode::Abort;

	bool CanRetry() const { return RetryMode == EDownloadRetryMode ::Retry; }
};

using FDownloadResult = TResult<FEmpty, FDownloadError>;

struct FDownloadedBlock
{
	uint64		 DecompressedSize = 0;
	uint64		 CompressedSize	  = 0;
	const uint8* Data			  = nullptr;

	bool IsCompressed() const { return CompressedSize != 0; }
};

using FBlockDownloadCallback = std::function<void(const FDownloadedBlock& Block, FHash128 BlockHash)>;

struct FMacroBlockRequest
{
	FGenericHash Hash	= {};
	uint64		 Offset = 0;
	uint64		 Size	= 0;

	uint64 MacroBlockBaseOffset = 0;
	uint64 MacroBlockTotalSize	= 0;

	bool IsValid() const { return Size != 0; }
};

class FBlockRequestMap
{
public:
	void Init(EStrongHashAlgorithmID InStrongHasher)
	{
		UNSYNC_ASSERTF(StrongHasher == EStrongHashAlgorithmID::Invalid, L"Request map is already initialized");
		StrongHasher = InStrongHasher;
	}

	void AddFileBlocks(const FPath& OriginalFilePath, const FPath& ResolvedFilePath, const FFileManifest& Manifest);

	const std::vector<std::string>& GetFileList() const { return FileListUtf8; }
	const FBlockRequest*			FindRequest(const FGenericHash& BlockHash) const;
	const std::string*				FindFile(const FHash128& Hash) const;
	EStrongHashAlgorithmID			GetStrongHasher() const { return StrongHasher; }
	FMacroBlockRequest				GetMacroBlockRequest(const FGenericHash& BlockHash) const;

private:
	EStrongHashAlgorithmID							 StrongHasher = EStrongHashAlgorithmID::Invalid;
	std::vector<std::string>						 FileListUtf8;
	std::unordered_map<FHash128, uint32>			 HashToFile;
	std::unordered_map<FHash128, FBlockRequest>		 BlockRequests;
	std::unordered_map<FHash128, FMacroBlockRequest> MacroBlockRequests;
};

struct FRemoteProtocolFeatures
{
	bool bTelemetry		   = false;
	bool bMirrors		   = false;
	bool bAuthentication   = false;
	bool bDirectoryListing = false;
	bool bFileDownload	   = false;
	bool bDownloadByHash   = false;
};

struct FTelemetryEventSyncComplete
{
	std::string ClientVersion;
	std::string Session;
	std::string Source;
	std::string ClientHostNameHash;
	uint64 TotalBytes = 0;
	uint64 SourceBytes = 0;
	uint64 BaseBytes = 0;
	uint32 SkippedFiles = 0;
	uint32 FullCopyFiles = 0;
	uint32 PartialCopyFiles = 0;
	double Elapsed = 0;
	bool bSuccess = false;
};

struct FRemoteProtocolBase
{
	FRemoteProtocolBase(const FRemoteDesc& InRemoteDesc, const FBlockRequestMap* InRequestMap)
	: RequestMap(InRequestMap)
	, RemoteDesc(InRemoteDesc)
	{
	}

	virtual ~FRemoteProtocolBase(){};

	virtual bool Contains(const FDirectoryManifest& Manifest) = 0;
	virtual bool IsValid() const							  = 0;
	virtual void Invalidate()								  = 0;

	virtual FDownloadResult	 Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback) = 0;
	virtual TResult<FBuffer> DownloadManifest(std::string_view ManifestName)													 = 0;

	const FBlockRequestMap* RequestMap;
	FRemoteDesc				RemoteDesc;
};

// TODO:
// - transparently go through proxy when reading blocks during patching
//   i.e. read blocks from disk / network share or from proxy automatically
class FProxy
{
public:
	FProxy(const FRemoteDesc&			  InRemoteDesc,
		   const FRemoteProtocolFeatures& InFeatures,
		   const FAuthDesc*				  InAuthDesc,
		   const FBlockRequestMap*		  InRequestMap);
	~FProxy();

	bool Contains(const FDirectoryManifest& Manifest);
	bool IsValid() const;

	FDownloadResult	 Download(const TArrayView<FNeedBlock> NeedBlocks, const FBlockDownloadCallback& CompletionCallback);
	TResult<FBuffer> DownloadManifest(std::string_view ManifestName);

private:
	std::unique_ptr<FRemoteProtocolBase> ProtocolImpl;
};

class FProxyPool
{
public:
	FProxyPool();
	FProxyPool(const FRemoteDesc& InRemoteDesc, const FAuthDesc* InAuthDesc);

	std::unique_ptr<FProxy> Alloc();
	void					Dealloc(std::unique_ptr<FProxy>&& Proxy);

	void Invalidate();
	bool IsValid() const;

	FSemaphore ParallelDownloadSemaphore;

	const FRemoteDesc RemoteDesc;
	const FAuthDesc* AuthDesc = nullptr; // optional reference to externally-owned auth parameters

	void InitRequestMap(EStrongHashAlgorithmID InStrongHasher);
	void BuildFileBlockRequests(const FPath& OriginalFilePath, const FPath& ResolvedFilePath, const FFileManifest& FileManifest);

	const FRemoteProtocolFeatures& GetFeatures() const { return Features; }
	const std::string& GetSessionId() const { return SessionId; }

	void SendTelemetryEvent(const FTelemetryEventSyncComplete& Event);

private:
	std::vector<std::unique_ptr<FProxy>> Pool;
	bool								 bValid = true;

	FRemoteProtocolFeatures Features;
	std::string SessionId;

	FBlockRequestMap RequestMap;

	std::mutex Mutex;
};

namespace ProxyQuery {

struct FHelloResponse
{
	std::string Name;
	std::string VersionNumber;
	std::string VersionGit;
	std::string SessionId;

	std::string AuthServerUri;
	std::string AuthClientId;
	std::string AuthAudience;
	std::string CallbackUri;

	std::vector<std::string> FeatureNames;
	FRemoteProtocolFeatures	 Features;

	std::optional<FHostAddressAndPort> PrimaryHost;

	bool SupportsAuthentication() const { return Features.bAuthentication && !AuthServerUri.empty() && !AuthClientId.empty(); }
};
TResult<FHelloResponse> Hello(const FRemoteDesc& RemoteDesc, const FAuthDesc* OptAuthDesc = nullptr);
TResult<FHelloResponse> Hello(FHttpConnection& Connection, const FAuthDesc* OptAuthDesc = nullptr);

struct FDirectoryListingEntry
{
	std::string Name;
	uint64		Mtime	   = 0;
	uint64		Size	   = 0;
	bool		bDirectory = false;
};

struct FDirectoryListing
{
	std::vector<FDirectoryListingEntry> Entries;

	static TResult<FDirectoryListing> FromJson(const char* JsonString);
};

// TODO: add overloads with HTTP connection
TResult<FDirectoryListing> ListDirectory(const FRemoteDesc& Remote, const FAuthDesc* AuthDesc, const std::string& Path);
TResult<FBuffer>		   DownloadFile(const FRemoteDesc& Remote, const FAuthDesc* AuthDesc, const std::string& Path);

using FDownloadOutputCallback = std::function<FIOWriter&(uint64 Size)>;
TResult<> DownloadFile(const FRemoteDesc& Remote, const FAuthDesc* AuthDesc, const std::string& Path, FDownloadOutputCallback OutputCallback);

} 

}  // namespace unsync
