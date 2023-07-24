// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"

struct FIoHash;


namespace UE::RemoteExecution
{
	enum class EStatusCode;
	class IRemoteMessage;
	struct FGetObjectTreeResponse;

	class IContentAddressableStorage
	{
	public:
		/** Virtual destructor */
		virtual ~IContentAddressableStorage() {}

		virtual bool ToBlob(const IRemoteMessage& Message, TArray<uint8>& OutBlob, FIoHash& OutIoHash) = 0;
		virtual FIoHash ToDigest(const TArray<uint8>& Blob) = 0;

		virtual TFuture<EStatusCode> DoesBlobExistAsync(const FString& NameSpaceId, const FIoHash& Hash) = 0;
		virtual TFuture<TMap<FIoHash, EStatusCode>> DoBlobsExistAsync(const FString& NameSpaceId, const TSet<FIoHash>& Hashes) = 0;

		virtual TFuture<EStatusCode> PutBlobAsync(const FString& NameSpaceId, const FIoHash& Hash, const TArray<uint8>& Data) = 0;
		virtual TFuture<TMap<FIoHash, EStatusCode>> PutBlobsAsync(const FString& NameSpaceId, const TMap<FIoHash, TArray<uint8>>& Blobs) = 0;

		virtual TFuture<EStatusCode> PutMissingBlobAsync(const FString& NameSpaceId, const FIoHash& Hash, const TArray<uint8>& Data) = 0;
		virtual TFuture<TMap<FIoHash, EStatusCode>> PutMissingBlobsAsync(const FString& NameSpaceId, const TMap<FIoHash, TArray<uint8>>& Blobs) = 0;

		virtual TFuture<TPair<EStatusCode, TArray<uint8>>> GetBlobAsync(const FString& NameSpaceId, const FIoHash& Hash) = 0;
		virtual TFuture<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>> GetBlobsAsync(const FString& NameSpaceId, const TSet<FIoHash>& Hashes) = 0;

		virtual TFuture<TPair<EStatusCode, FGetObjectTreeResponse>> GetObjectTreeAsync(const FString& NameSpaceId, const FIoHash& Hash, const TSet<FIoHash>& HaveHashes) = 0;
		virtual TFuture<TPair<EStatusCode, FGetObjectTreeResponse>> GetObjectTreeAsync(const FString& NameSpaceId, const FIoHash& Hash) = 0;
	};
}
