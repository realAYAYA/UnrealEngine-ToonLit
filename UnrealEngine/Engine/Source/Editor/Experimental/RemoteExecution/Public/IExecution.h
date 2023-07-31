// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"


namespace UE::RemoteExecution
{
	enum class EStatusCode;
	class FAddTasksRequest;
	class FGetTaskUpdatesResponse;

	class IExecution
	{
	public:
		/** Virtual destructor */
		virtual ~IExecution() {}

		virtual TFuture<EStatusCode> AddTasksAsync(const FString& ChannelId, const FAddTasksRequest& Request) = 0;
		virtual TFuture<TPair<EStatusCode, FGetTaskUpdatesResponse>> GetUpdatesAsync(const FString& ChannelId, const int32 WaitSeconds = 0) = 0;

		virtual TFuture<TPair<EStatusCode, FGetTaskUpdatesResponse>> RunTasksAsync(const FAddTasksRequest& AddTaskRequest, const int32 TimeoutSeconds = 0) = 0;
	};
}
