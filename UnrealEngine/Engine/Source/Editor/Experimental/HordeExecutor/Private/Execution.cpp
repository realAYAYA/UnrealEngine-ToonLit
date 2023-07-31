// Copyright Epic Games, Inc. All Rights Reserved.

#include "Execution.h"

#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "HordeExecutorModule.h"
#include "HttpModule.h"
#include "IO/IoHash.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Memory/MemoryFwd.h"
#include "Memory/MemoryView.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/Timespan.h"
#include "Modules/ModuleManager.h"
#include "RemoteMessages.h"
#include "Serialization/CompactBinary.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"


namespace UE::RemoteExecution
{
	struct FExecution::FWaitTask
	{
		TPromise<TPair<EStatusCode, FGetTaskUpdatesResponse>> Promise;
		FString ChannelId;
		TArray<FIoHash> TaskHashes;
		TMap<FIoHash, FGetTaskUpdateResponse> Complete;
		FDateTime Timeout;
	};

	FExecution::FExecution(const FString& BaseURL, const TMap<FString, FString> AdditionalHeaders)
		: BaseURL(BaseURL)
		, AdditionalHeaders(AdditionalHeaders)
	{
	}

	TFuture<EStatusCode> FExecution::AddTasksAsync(const FString& ChannelId, const FAddTasksRequest& AddTaskRequest)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<EStatusCode> UnloadedPromise;
			UnloadedPromise.SetValue(EStatusCode::BadRequest);
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(ChannelId);
		const FString Route = FString::Format(TEXT("/api/v1/compute/{0}"), Args);

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb(TEXT("POST"));
		Request->SetURL(BaseURL + Route);
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-ue-cb"));
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}

		{
			FCbObject CbObject = AddTaskRequest.Save();
			FMemoryView MemoryView;
			if (!CbObject.TryGetView(MemoryView))
			{
				TPromise<EStatusCode> ReturnPromise;
				ReturnPromise.SetValue(EStatusCode::BadRequest);
				return ReturnPromise.GetFuture();
			}

			TArray<uint8> Content = TArray<uint8>((const uint8*)MemoryView.GetData(), MemoryView.GetSize());
			Request->SetContent(MoveTemp(Content));
		}

		TSharedPtr<TPromise<EStatusCode>> ReturnPromise = MakeShared<TPromise<EStatusCode>>();
		Request->OnProcessRequestComplete().BindLambda([ReturnPromise](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			ReturnPromise->EmplaceValue((EStatusCode)HttpResponse->GetResponseCode());
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<TPair<EStatusCode, FGetTaskUpdatesResponse>> FExecution::GetUpdatesAsync(const FString& ChannelId, const int32 WaitSeconds)
	{
		FHttpModule* HttpModule = static_cast<FHttpModule*>(FModuleManager::Get().GetModule("HTTP"));
		if (!HttpModule)
		{
			TPromise<TPair<EStatusCode, FGetTaskUpdatesResponse>> UnloadedPromise;
			UnloadedPromise.EmplaceValue(TPair<EStatusCode, FGetTaskUpdatesResponse>(EStatusCode::BadRequest, FGetTaskUpdatesResponse()));
			return UnloadedPromise.GetFuture();
		}

		FStringFormatOrderedArguments Args;
		Args.Add(ChannelId);
		Args.Add(WaitSeconds);
		const FString Route = FString::Format(TEXT("/api/v1/compute/{0}/updates?wait={1}"), Args);

		TSharedRef<IHttpRequest> Request = HttpModule->CreateRequest();
		Request->SetVerb(TEXT("POST"));
		Request->SetURL(BaseURL + Route);
		Request->SetHeader(TEXT("Accept"), TEXT("application/x-ue-cb"));
		for (const TPair<FString, FString>& Header : AdditionalHeaders)
		{
			Request->SetHeader(Header.Key, Header.Value);
		}

		TSharedPtr<TPromise<TPair<EStatusCode, FGetTaskUpdatesResponse>>> ReturnPromise = MakeShared<TPromise<TPair<EStatusCode, FGetTaskUpdatesResponse>>>();
		Request->OnProcessRequestComplete().BindLambda([ReturnPromise](FHttpRequestPtr Req, FHttpResponsePtr HttpResponse, bool bSucceeded) {
			FGetTaskUpdatesResponse GetTaskUpdatesResponse;
			if (bSucceeded && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
			{
				const FCbObjectView View = FCbObjectView(HttpResponse->GetContent().GetData());
				GetTaskUpdatesResponse.Load(View);
			}
			ReturnPromise->EmplaceValue(TPair<EStatusCode, FGetTaskUpdatesResponse>((EStatusCode)HttpResponse->GetResponseCode(), MoveTemp(GetTaskUpdatesResponse)));
			});
		Request->ProcessRequest();
		return ReturnPromise->GetFuture();
	}

	TFuture<TPair<EStatusCode, FGetTaskUpdatesResponse>> FExecution::RunTasksAsync(const FAddTasksRequest& AddTaskRequest, const int32 TimeoutSeconds)
	{
		TSharedPtr<FWaitTask> WaitTask = MakeShared<FWaitTask>();
		WaitTask->ChannelId = FGuid::NewGuid().ToString();
		WaitTask->TaskHashes.Append(AddTaskRequest.TaskHashes);
		WaitTask->Timeout = TimeoutSeconds > 0 ? FDateTime::Now() + FTimespan::FromSeconds(TimeoutSeconds) : FDateTime::MaxValue();

		AddTasksAsync(WaitTask->ChannelId, AddTaskRequest).Next([this, WaitTask](EStatusCode&& Response) {
			if (Response != EStatusCode::Ok)
			{
				WaitTask->Promise.EmplaceValue(TPair<EStatusCode, FGetTaskUpdatesResponse>(MoveTemp(Response), FGetTaskUpdatesResponse()));
				return;
			}
			WaitForTasksAsync(WaitTask);
			});

		return WaitTask->Promise.GetFuture();

	}

	void FExecution::WaitForTasksAsync(TSharedPtr<FWaitTask> WaitTask)
	{
		GetUpdatesAsync(WaitTask->ChannelId, 5).Next([this, WaitTask](TPair<EStatusCode, FGetTaskUpdatesResponse>&& Response) {
			for (FGetTaskUpdateResponse& Update : Response.Value.Updates)
			{
				UE_LOG(LogHordeExecutor, Verbose, TEXT("WaitForTasksAsync: %s %s"),
					*ComputeTaskStateString(Update.State),
					*FString::FromHexBlob(Update.TaskHash.GetBytes(), sizeof(FIoHash::ByteArray)));
				if (Update.State == EComputeTaskState::Complete)
				{
					WaitTask->Complete.Add(Update.TaskHash, MoveTemp(Update));
				}
			}

			if (WaitTask->Complete.Num() != WaitTask->TaskHashes.Num() && Response.Key == EStatusCode::Ok && FDateTime::Now() > WaitTask->Timeout)
			{
				Response.Key = EStatusCode::RequestTimeout;
			}

			if (WaitTask->Complete.Num() == WaitTask->TaskHashes.Num() || Response.Key != EStatusCode::Ok)
			{
				for (const FIoHash& TaskHash : WaitTask->TaskHashes)
				{
					if (!WaitTask->Complete.Contains(TaskHash))
					{
						FGetTaskUpdateResponse Update;
						Update.TaskHash = TaskHash;
						Update.Outcome = EComputeTaskOutcome::NoResult;
						WaitTask->Complete.Add(TaskHash, MoveTemp(Update));
					}
				}

				FGetTaskUpdatesResponse Result;
				WaitTask->Complete.GenerateValueArray(Result.Updates);
				WaitTask->Promise.EmplaceValue(TPair<EStatusCode, FGetTaskUpdatesResponse>(MoveTemp(Response.Key), MoveTemp(Result)));
				return;
			}
			WaitForTasksAsync(WaitTask);
			});
	}
}
