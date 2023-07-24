// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "IExecution.h"
#include "RemoteMessages.h"
#include "Templates/SharedPointer.h"


namespace UE::RemoteExecution
{
	class FExecution : public IExecution
	{
	private:
		struct FWaitTask;

		FString BaseURL;
		TMap<FString, FString> AdditionalHeaders;

	public:
		FExecution(const FString& BaseURL, const TMap<FString, FString> AdditionalHeaders);

		// Inherited via IExecution
		virtual TFuture<EStatusCode> AddTasksAsync(const FString& ChannelId, const FAddTasksRequest& AddTaskRequest) override;
		virtual TFuture<TPair<EStatusCode, FGetTaskUpdatesResponse>> GetUpdatesAsync(const FString& ChannelId, const int32 WaitSeconds = 0) override;

		virtual TFuture<TPair<EStatusCode, FGetTaskUpdatesResponse>> RunTasksAsync(const FAddTasksRequest& AddTaskRequest, const int32 TimeoutSeconds = 0) override;

	private:
		void WaitForTasksAsync(TSharedPtr<FWaitTask> WaitTask);
	};
}
