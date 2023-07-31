// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentAddressableStorage.h"

#include "Containers/SparseArray.h"
#include "CoreGlobals.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "HordeMessages.h"
#include "HttpModule.h"
#include "IO/IoHash.h"
#include "IRemoteMessage.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Memory/MemoryFwd.h"
#include "Memory/MemoryView.h"
#include "Misc/ScopeLock.h"
#include "Modules/ModuleManager.h"
#include "RemoteMessages.h"
#include "Serialization/CompactBinary.h"
#include "Templates/SharedPointer.h"
#include "Templates/UnrealTemplate.h"


namespace UE::RemoteExecution
{
	FContentAddressableStorage::FContentAddressableStorage(const FString& BaseURL, const TMap<FString, FString> AdditionalHeaders)
		: BaseURL(BaseURL)
		, AdditionalHeaders(AdditionalHeaders)
	{
	}

	bool FContentAddressableStorage::ToBlob(const IRemoteMessage& Message, TArray<uint8>& OutBlob, FIoHash& OutIoHash)
	{
		FCbObject CbObject = Message.Save();
		FMemoryView MemoryView;
		if (CbObject.TryGetView(MemoryView))
		{
			OutBlob.Empty();
			OutBlob.Append((const uint8*)MemoryView.GetData(), MemoryView.GetSize());
			OutIoHash = FIoHash(CbObject.GetHash());
			return true;
		}
		return false;
	}

	FIoHash FContentAddressableStorage::ToDigest(const TArray<uint8>& Blob)
	{
		return FIoHash::HashBuffer(Blob.GetData(), Blob.Num());
	}

	TFuture<EStatusCode> FContentAddressableStorage::DoesBlobExistAsync(const FString& NameSpaceId, const FIoHash& Hash)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<EStatusCode> UnloadedPromise;
			UnloadedPromise.SetValue(EStatusCode::BadRequest);
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(NameSpaceId);
		Args.Add(FString::FromHexBlob(Hash.GetBytes(), sizeof(FIoHash::ByteArray)));
		const FString Route = FString::Format(TEXT("/api/v1/blobs/{0}/{1}"), Args);

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb(TEXT("HEAD"));
		Request->SetURL(BaseURL + Route);
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}

		TSharedPtr<TPromise<EStatusCode>> ReturnPromise = MakeShared<TPromise<EStatusCode>>();
		Request->OnProcessRequestComplete().BindLambda([ReturnPromise](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			ReturnPromise->EmplaceValue((EStatusCode)HttpResponse->GetResponseCode());
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<TMap<FIoHash, EStatusCode>> FContentAddressableStorage::DoBlobsExistAsync(const FString& NameSpaceId, const TSet<FIoHash>& Hashes)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<TMap<FIoHash, EStatusCode>> UnloadedPromise;
			TMap<FIoHash, EStatusCode> UnloadedResults;
			for (const FIoHash& Hash : Hashes)
			{
				UnloadedResults.Add(Hash, EStatusCode::BadRequest);
			}
			UnloadedPromise.EmplaceValue(UnloadedResults);
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(NameSpaceId);
		const FString Route = FString::Format(TEXT("/api/v1/blobs/{0}/exists?"), Args);
		FString Query;
		for (const FIoHash& Hash : Hashes)
		{
			FStringFormatOrderedArguments QueryArgs;
			QueryArgs.Add(Query);
			QueryArgs.Add(FString::FromHexBlob(Hash.GetBytes(), sizeof(FIoHash::ByteArray)));
			if (Query.IsEmpty())
			{
				Query = FString::Format(TEXT("id={1}"), QueryArgs);
			}
			else
			{
				Query = FString::Format(TEXT("{0}&id={1}"), QueryArgs);
			}
		}

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb(TEXT("POST"));
		Request->SetURL(BaseURL + Route + Query);
		Request->SetHeader(TEXT("Accept"), TEXT("application/x-ue-cb"));
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}

		TSharedPtr<TPromise<TMap<FIoHash, EStatusCode>>> ReturnPromise = MakeShared<TPromise<TMap<FIoHash, EStatusCode>>>();
		Request->OnProcessRequestComplete().BindLambda([ReturnPromise, Hashes](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			TMap<FIoHash, EStatusCode> Results;
			if (bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
			{
				FExistsResponse ExistsResponse;
				ExistsResponse.Load(FCbObjectView(HttpResponse->GetContent().GetData()));
				for (const FIoHash& Hash : Hashes)
				{
					Results.Add(Hash, ExistsResponse.Id.Contains(Hash) ? EStatusCode::Ok : EStatusCode::NotFound);
				}
			}
			else
			{
				for (const FIoHash& Hash : Hashes)
				{
					Results.Add(Hash, (EStatusCode)HttpResponse->GetResponseCode());
				}
			}
			ReturnPromise->EmplaceValue(TMap<FIoHash, EStatusCode>(MoveTemp(Results)));
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<EStatusCode> FContentAddressableStorage::PutBlobAsync(const FString& NameSpaceId, const FIoHash& Hash, const TArray<uint8>& Data)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<EStatusCode> UnloadedPromise;
			UnloadedPromise.SetValue(EStatusCode::BadRequest);
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(NameSpaceId);
		Args.Add(FString::FromHexBlob(Hash.GetBytes(), sizeof(FIoHash::ByteArray)));
		const FString Route = FString::Format(TEXT("/api/v1/blobs/{0}/{1}"), Args);

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb(TEXT("PUT"));
		Request->SetURL(BaseURL + Route);
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
		Request->SetHeader(TEXT("Accept"), TEXT("application/x-ue-cb"));
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}
		Request->SetContent(Data);

		TSharedPtr<TPromise<EStatusCode>> ReturnPromise = MakeShared<TPromise<EStatusCode>>();
		Request->OnProcessRequestComplete().BindLambda([ReturnPromise](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			ReturnPromise->EmplaceValue((EStatusCode)HttpResponse->GetResponseCode());
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<TMap<FIoHash, EStatusCode>> FContentAddressableStorage::PutBlobsAsync(const FString& NameSpaceId, const TMap<FIoHash, TArray<uint8>>& Blobs)
	{
		TSharedPtr<TPromise<TMap<FIoHash, EStatusCode>>> ReturnPromise = MakeShared<TPromise<TMap<FIoHash, EStatusCode>>>();
		TSharedPtr<TMap<FIoHash, EStatusCode>> Results = MakeShared<TMap<FIoHash, EStatusCode>>();
		TSharedPtr<FCriticalSection> CriticalSection = MakeShared<FCriticalSection>();
		for (const TPair<FIoHash, TArray<uint8>>& Blob : Blobs)
		{
			PutBlobAsync(NameSpaceId, Blob.Key, Blob.Value).Next([ReturnPromise, Results, CriticalSection, Hash = Blob.Key, Total = Blobs.Num()](EStatusCode&& Response) {
				FScopeLock Lock(CriticalSection.Get());
				Results->Add(Hash, MoveTemp(Response));
				if (Results->Num() == Total) {
					ReturnPromise->EmplaceValue(TMap<FIoHash, EStatusCode>(*Results.Get()));
				}
			});
		}
		return ReturnPromise->GetFuture();
	}

	TFuture<EStatusCode> FContentAddressableStorage::PutMissingBlobAsync(const FString& NameSpaceId, const FIoHash& Hash, const TArray<uint8>& Data)
	{
		TSharedPtr<TPromise<EStatusCode>> ReturnPromise = MakeShared<TPromise<EStatusCode>>();

		DoesBlobExistAsync(NameSpaceId, Hash).Next([this, NameSpaceId, ReturnPromise, Hash, Data](EStatusCode&& ExistResponse) {
			if (ExistResponse == EStatusCode::Ok)
			{
				ReturnPromise->EmplaceValue(ExistResponse);
				return;
			}

			if (!GIsRunning)
			{
				ReturnPromise->SetValue(EStatusCode::Denied);
				return;
			}

			PutBlobAsync(NameSpaceId, Hash, Data).Next([ReturnPromise](EStatusCode&& PutResponse) {
				ReturnPromise->EmplaceValue(PutResponse);
				});
			});
		return ReturnPromise->GetFuture();
	}

	TFuture<TMap<FIoHash, EStatusCode>> FContentAddressableStorage::PutMissingBlobsAsync(const FString& NameSpaceId, const TMap<FIoHash, TArray<uint8>>& Blobs)
	{
		TSharedPtr<TPromise<TMap<FIoHash, EStatusCode>>> ReturnPromise = MakeShared<TPromise<TMap<FIoHash, EStatusCode>>>();

		TSet<FIoHash> Hashes;
		Blobs.GetKeys(Hashes);
		DoBlobsExistAsync(NameSpaceId, Hashes).Next([this, NameSpaceId, ReturnPromise, Blobs](TMap<FIoHash, EStatusCode>&& ExistsResponse) {
			const TMap<FIoHash, TArray<uint8>>& Remaining = Blobs.FilterByPredicate([ExistsResponse](const TPair<FIoHash, TArray<uint8>>& Pair) {
				return ExistsResponse[Pair.Key] != EStatusCode::Ok;
				});

			if (Remaining.IsEmpty())
			{
				ReturnPromise->EmplaceValue(ExistsResponse);
				return;
			}

			if (!GIsRunning)
			{
				TMap<FIoHash, EStatusCode> PutResponse;
				for (const TPair<FIoHash, TArray<uint8>>& Blob : Blobs)
				{
					PutResponse.Add(Blob.Key, EStatusCode::Denied);
				}
				ReturnPromise->EmplaceValue(PutResponse);
				return;
			}

			PutBlobsAsync(NameSpaceId, Remaining).Next([ReturnPromise, ExistsResponse](TMap<FIoHash, EStatusCode>&& PutResponse) {
				for (const TPair<FIoHash, EStatusCode>& Exists : ExistsResponse)
				{
					if (!PutResponse.Contains(Exists.Key))
					{
						PutResponse.Add(Exists.Key, Exists.Value);
					}
				}
				ReturnPromise->EmplaceValue(PutResponse);
				});
			});
		return ReturnPromise->GetFuture();
	}

	TFuture<TPair<EStatusCode, TArray<uint8>>> FContentAddressableStorage::GetBlobAsync(const FString& NameSpaceId, const FIoHash& Hash)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<TPair<EStatusCode, TArray<uint8>>> UnloadedPromise;
			UnloadedPromise.EmplaceValue(TPair<EStatusCode, TArray<uint8>>(EStatusCode::BadRequest, TArray<uint8>()));
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(NameSpaceId);
		Args.Add(FString::FromHexBlob(Hash.GetBytes(), sizeof(FIoHash::ByteArray)));
		const FString Route = FString::Format(TEXT("/api/v1/blobs/{0}/{1}"), Args);

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb("GET");
		Request->SetURL(BaseURL + Route);
		Request->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}

		TSharedPtr<TPromise<TPair<EStatusCode, TArray<uint8>>>> ReturnPromise = MakeShared<TPromise<TPair<EStatusCode, TArray<uint8>>>>();
		Request->OnProcessRequestComplete().BindLambda([ReturnPromise](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			ReturnPromise->EmplaceValue(TPair<EStatusCode, TArray<uint8>>((EStatusCode)HttpResponse->GetResponseCode(), HttpResponse->GetContent()));
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>> FContentAddressableStorage::GetBlobsAsync(const FString& NameSpaceId, const TSet<FIoHash>& Hashes)
	{
		TSharedPtr<TPromise<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>>> ReturnPromise = MakeShared<TPromise<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>>>();
		TSharedPtr<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>> Results = MakeShared<TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>>();
		TSharedPtr<FCriticalSection> CriticalSection = MakeShared<FCriticalSection>();
		for (const FIoHash& Hash : Hashes)
		{
			GetBlobAsync(NameSpaceId, Hash).Next([ReturnPromise, Results, CriticalSection, Hash, Total = Hashes.Num()](TPair<EStatusCode, TArray<uint8>>&& Response) {
				FScopeLock Lock(CriticalSection.Get());
				Results->Add(Hash, Response);
				if (Results->Num() == Total) {
					ReturnPromise->EmplaceValue(TMap<FIoHash, TPair<EStatusCode, TArray<uint8>>>(*Results.Get()));
				}
			});
		}
		return ReturnPromise->GetFuture();
	}

	TFuture<TPair<EStatusCode, FGetObjectTreeResponse>> FContentAddressableStorage::GetObjectTreeAsync(const FString& NameSpaceId, const FIoHash& Hash, const TSet<FIoHash>& HaveHashes)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<TPair<EStatusCode, FGetObjectTreeResponse>> UnloadedPromise;
			UnloadedPromise.EmplaceValue(TPair<EStatusCode, FGetObjectTreeResponse>(EStatusCode::BadRequest, FGetObjectTreeResponse()));
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(NameSpaceId);
		Args.Add(FString::FromHexBlob(Hash.GetBytes(), sizeof(FIoHash::ByteArray)));
		const FString Route = FString::Format(TEXT("/api/v1/objects/{0}/{1}/tree"), Args);

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb("POST");
		Request->SetURL(BaseURL + Route);
		Request->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-ue-cb"));
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}
		{
			FGetObjectTreeRequest ObjectTreeRequest;
			ObjectTreeRequest.Have = HaveHashes;
			FCbObject CbObject = ObjectTreeRequest.Save();
			FMemoryView MemoryView;
			if (!CbObject.TryGetView(MemoryView))
			{
				TPromise<TPair<EStatusCode, FGetObjectTreeResponse>> ReturnPromise;
				ReturnPromise.SetValue(TPair<EStatusCode, FGetObjectTreeResponse>());
				return ReturnPromise.GetFuture();
			}
			Request->SetContent(TArray<uint8>((const uint8*)MemoryView.GetData(), MemoryView.GetSize()));
		}

		TSharedPtr<TPromise<TPair<EStatusCode, FGetObjectTreeResponse>>> ReturnPromise = MakeShared<TPromise<TPair<EStatusCode, FGetObjectTreeResponse>>>();
		Request->OnProcessRequestComplete().BindLambda([this, NameSpaceId, ReturnPromise](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			EStatusCode ResponseCode = (EStatusCode)HttpResponse->GetResponseCode();
			FGetObjectTreeResponse Response;
			if (ResponseCode == EStatusCode::Ok)
			{
				const TArray<uint8>& Content = HttpResponse->GetContent();
				int Index = 0;
				// Repeating array of Hash then object
				while (Index < Content.Num())
				{
					FCbFieldView Field = FCbFieldView(&Content[Index]);
					Index += Field.GetSize();
					if (Field.IsObjectAttachment())
					{
						FIoHash Hash = Field.AsObjectAttachment();
						Field = FCbFieldView(&Content[Index]);
						Index += Field.GetSize();
						if (!Field.IsObject())
						{
							Response.Objects.Add(MoveTemp(Hash), TArray<uint8>());
							continue;
						}
						FCbObjectView ObjectView = Field.AsObjectView();
						FMemoryView MemoryView;
						if (ObjectView.TryGetView(MemoryView))
						{
							Response.Objects.Add(MoveTemp(Hash), TArray<uint8>((const uint8*)MemoryView.GetData(), MemoryView.GetSize()));
						}
					}
					else if (Field.IsBinaryAttachment())
					{
						Response.BinaryAttachments.Add(Field.AsBinaryAttachment());
					}
					else // Unknown type
					{
					}
				}
			}
			ReturnPromise->EmplaceValue(TPair<EStatusCode, FGetObjectTreeResponse>(MoveTemp(ResponseCode), MoveTemp(Response)));
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<TPair<EStatusCode, FGetObjectTreeResponse>> FContentAddressableStorage::GetObjectTreeAsync(const FString& NameSpaceId, const FIoHash& Hash)
	{
		return GetObjectTreeAsync(NameSpaceId, Hash, TSet<FIoHash>());
	}
}
