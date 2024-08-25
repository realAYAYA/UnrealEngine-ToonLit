// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Compression/CompressedBuffer.h"
#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"
#include "Memory/MemoryFwd.h"
#include "Templates/SharedPointer.h"

#if !UE_BUILD_SHIPPING

class FIoBuffer;
class FIoChunkId;
class FInternetAddr;
class FSocket;
class FStorageServerChunkBatchRequest;
class FStorageServerConnection;
class ISocketSubsystem;
struct FPackageStoreEntryResource;

enum class EStorageServerContentType : uint8
{
	Unknown = 0,
	CbObject,
	Binary,
	CompressedBinary,
};

inline FAnsiStringView GetMimeTypeString(EStorageServerContentType ContentType)
{
	switch (ContentType)
	{
		case EStorageServerContentType::CbObject:
			return ANSITEXTVIEW("application/ue-x-cb");
		case EStorageServerContentType::Binary:
			return ANSITEXTVIEW("application/octet-stream");
		case EStorageServerContentType::CompressedBinary:
			return ANSITEXTVIEW("application/x-ue-comp");
		default:
			return ANSITEXTVIEW("unknown");
	};
};

inline EStorageServerContentType GetMimeType(const FAnsiStringView& ContentType)
{
	if (ContentType == ANSITEXTVIEW("application/octet-stream"))
	{
		return EStorageServerContentType::Binary;
	}
	else if (ContentType == ANSITEXTVIEW("application/x-ue-comp"))
	{
		return EStorageServerContentType::CompressedBinary;
	}
	else if (ContentType == ANSITEXTVIEW("application/x-ue-cb"))
	{
		return EStorageServerContentType::CbObject;
	}
	else
	{
		return EStorageServerContentType::Unknown;
	}
};

struct FStorageServerSerializationContext
{
	TArray64<uint8> CompressedBuffer;
	FCompressedBufferReader Decoder;
};

class FStorageServerRequest
	: public FArchive
{
	EStorageServerContentType AcceptContentType() const;
protected:
	friend FStorageServerConnection;

	FStorageServerRequest(
		FAnsiStringView Verb,
		FAnsiStringView Resource,
		FAnsiStringView Hostname,
		EStorageServerContentType Accept = EStorageServerContentType::Binary);

	FSocket* Send(FStorageServerConnection& Owner, bool bLogOnError = true);
	virtual void Serialize(void* V, int64 Length) override;

	EStorageServerContentType AcceptType;
	TAnsiStringBuilder<512> HeaderBuffer;
	TArray<uint8, TInlineAllocator<1024>> BodyBuffer;
};

class FStorageServerResponse
	: public FArchive
{
public:
	~FStorageServerResponse()
	{
		if (Socket)
		{
			ReleaseSocket(false);
		}
	}

	bool IsOk() const
	{
		return bIsOk;
	}

	const int32 GetErrorCode() const
	{
		return ErrorCode;
	}

	const FString& GetErrorMessage() const
	{
		return ErrorMessage;
	}

	int64 TotalSize() override
	{
		return ContentLength;
	}

	int64 Tell() override
	{
		return Position;
	}

	void Serialize(void* V, int64 Length) override;

	int64 SerializeChunk(FStorageServerSerializationContext& Context, FIoBuffer& OutChunk, void* TargetVa = nullptr, uint64 RawOffset = 0, uint64 RawSize = MAX_uint64);
	
	inline int64 SerializeChunk(FIoBuffer& OutChunk, void* TargetVa = nullptr, uint64 RawOffset = 0, uint64 RawSize = MAX_uint64)
	{
		FStorageServerSerializationContext SerializationContext;
		return SerializeChunk(SerializationContext, OutChunk, TargetVa, RawOffset, RawSize);
	}

	int64 SerializeChunkTo(FMutableMemoryView Memory, uint64 RawOffset = 0);

	FCbObject GetResponseObject();

private:
	friend FStorageServerConnection;
	friend FStorageServerChunkBatchRequest;

	FStorageServerResponse(FStorageServerConnection& Owner, FSocket& Socket);
	void ReleaseSocket(bool bKeepAlive);

	FStorageServerConnection& Owner;
	FSocket* Socket = nullptr;
	int64 ContentLength = 0;
	int64 Position = 0;
	int32 ErrorCode;
	FString ErrorMessage;
	bool bIsOk = false;
	EStorageServerContentType ContentType = EStorageServerContentType::Unknown;
};

class FStorageServerChunkBatchRequest
	: private FStorageServerRequest
{
public:
	STORAGESERVERCLIENT_API FStorageServerChunkBatchRequest& AddChunk(const FIoChunkId& ChunkId, int64 Offset, int64 Size);
	STORAGESERVERCLIENT_API bool Issue(TFunctionRef<void(uint32 ChunkCount, uint32* ChunkIndices, uint64* ChunkSizes, FStorageServerResponse& ChunkDataStream)> OnResponse);

private:
	friend FStorageServerConnection;

	FStorageServerChunkBatchRequest(FStorageServerConnection& Owner, FAnsiStringView Resource, FAnsiStringView Hostname);

	FStorageServerConnection& Owner;
	int32 ChunkCountOffset = 0;
};

class FStorageServerConnection
{
public:
	STORAGESERVERCLIENT_API FStorageServerConnection();
	STORAGESERVERCLIENT_API ~FStorageServerConnection();

	STORAGESERVERCLIENT_API bool Initialize(TArrayView<const FString> HostAddresses, int32 Port, const TCHAR* ProjectNameOverride = nullptr, const TCHAR* PlatformNameOverride = nullptr);

	STORAGESERVERCLIENT_API void PackageStoreRequest(TFunctionRef<void(FPackageStoreEntryResource&&)> Callback);
	STORAGESERVERCLIENT_API void FileManifestRequest(TFunctionRef<void(FIoChunkId Id, FStringView Path)> Callback);
	STORAGESERVERCLIENT_API int64 ChunkSizeRequest(const FIoChunkId& ChunkId);
	STORAGESERVERCLIENT_API bool ReadChunkRequest(const FIoChunkId& ChunkId, uint64 Offset, uint64 Size, TFunctionRef<void(FStorageServerResponse&)> OnResponse);
	STORAGESERVERCLIENT_API FStorageServerChunkBatchRequest NewChunkBatchRequest();

	STORAGESERVERCLIENT_API FString GetHostAddr() const;

private:
	friend FStorageServerRequest;
	friend FStorageServerResponse;
	friend FStorageServerChunkBatchRequest;

	int32 HandshakeRequest(TArrayView<const TSharedPtr<FInternetAddr>> HostAddresses);
	FSocket* AcquireSocketFromPool();
	FSocket* AcquireNewSocket(float TimeoutSeconds = -1.0f);
	void ReleaseSocket(FSocket* Socket, bool bKeepAlive);

	ISocketSubsystem& SocketSubsystem;
	TAnsiStringBuilder<1024> OplogPath;
	TSharedPtr<FInternetAddr> ServerAddr;
	TAnsiStringBuilder<1024> Hostname;
	FCriticalSection SocketPoolCritical;
	TArray<FSocket*> SocketPool;
};

#endif
