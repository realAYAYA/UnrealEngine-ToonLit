// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "IContentAddressableStorage.h"
#include "IO/IoHash.h"
#include "RemoteMessages.h"


namespace UE::RemoteExecution
{
class IRemoteMessage;

	class FContentAddressableStorage : public IContentAddressableStorage
	{
	private:
		FString BaseURL;
		TMap<FString, FString> AdditionalHeaders;

	public:
		FContentAddressableStorage(const FString& BaseURL, const TMap<FString, FString> AdditionalHeaders);

		virtual bool ToBlob(const IRemoteMessage& Message, TArray<uint8>& OutBlob, FIoHash& OutIoHash) override;
		virtual FIoHash ToDigest(const TArray<uint8>& Blob) override;

		virtual TFuture<EStatusCode> DoesBlobExistAsync(const FString& NameSpaceId, const FIoHash& Hash) override;
		virtual TFuture<TMap<FIoHash, EStatusCode>> DoBlobsExistAsync(const FString& NameSpaceId, const TSet<FIoHash>& Hashes) override;

		virtual TFuture<EStatusCode> PutBlobAsync(const FString& NameSpaceId, const FIoHash& Hash, const TArray<uint8>& Data) override;
		virtual TFuture<TMap<FIoHash, EStatusCode>> PutBlobsAsync(const FString& NameSpaceId, const TMap<FIoHash, TArray<uint8>>& Blobs) override;

		virtual TFuture<EStatusCode> PutMissingBlobAsync(const FString& NameSpaceId, const FIoHash& Hash, const TArray<uint8>& Data) override;
		virtual TFuture<TMap<FIoHash, EStatusCode>> PutMissingBlobsAsync(const FString& NameSpaceId, const TMap<FIoHash, TArray<uint8>>& Blobs) override;

		virtual TFuture<TPair<EStatusCode, TArray<uint8>>> GetBlobAsync(const FString& NameSpaceId, const FIoHash& Hash) override;
		virtual TFuture<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>> GetBlobsAsync(const FString& NameSpaceId, const TSet<FIoHash>& Hashes) override;

		virtual TFuture<TPair<EStatusCode, FGetObjectTreeResponse>> GetObjectTreeAsync(const FString& NameSpaceId, const FIoHash& Hash, const TSet<FIoHash>& HaveHashes) override;
		virtual TFuture<TPair<EStatusCode, FGetObjectTreeResponse>> GetObjectTreeAsync(const FString& NameSpaceId, const FIoHash& Hash) override;
	};
}
