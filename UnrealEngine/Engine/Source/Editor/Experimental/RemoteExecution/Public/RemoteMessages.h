// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "IO/IoHash.h"
#include "IRemoteMessage.h"
#include "Misc/DateTime.h"
#include "Serialization/CompactBinary.h"


namespace UE::RemoteExecution
{
	// Subset of http response codes
	enum class EStatusCode : int32
	{
		Unknown = 0,
		Ok = 200,
		BadRequest = 400,
		Denied = 401,
		Forbidden = 403,
		NotFound = 404,
		BadMethod = 405,
		RequestTimeout = 408,
		RequestTooLarge = 413,
		UriTooLong = 414,
		UnsupportedMedia = 415,
		TooManyRequests = 429,
		ServerError = 500,
		NotSupported = 501,
		BadGateway = 502,
		ServiceUnavail = 503,
		GatewayTimeout = 504,
	};

	enum class EComputeTaskState : int32
	{
		Queued = 0,
		Executing = 1,
		Complete = 2,
	};

	enum class EComputeTaskOutcome : int32
	{
		Success = 0,
		Failed = 1,
		Cancelled = 2,
		NoResult = 3,
		Expired = 4,
		BlobNotFound = 5,
		Exception = 6,
	};

	static FString ComputeTaskStateString(const EComputeTaskState ComputeTaskState)
	{
		switch (ComputeTaskState)
		{
		case EComputeTaskState::Queued: return TEXT("Queued");
		case EComputeTaskState::Executing: return TEXT("Executing");
		case EComputeTaskState::Complete: return TEXT("Complete");
		}
		return TEXT("Unknown");
	}

	static FString ComputeTaskOutcomeString(const EComputeTaskOutcome ComputeTaskOutcome)
	{
		switch (ComputeTaskOutcome)
		{
		case EComputeTaskOutcome::Success: return TEXT("Success");
		case EComputeTaskOutcome::Failed: return TEXT("Failed");
		case EComputeTaskOutcome::Cancelled: return TEXT("Cancelled");
		case EComputeTaskOutcome::NoResult: return TEXT("NoResult");
		case EComputeTaskOutcome::Expired: return TEXT("Expired");
		case EComputeTaskOutcome::BlobNotFound: return TEXT("BlobNotFound");
		case EComputeTaskOutcome::Exception: return TEXT("Exception");
		}
		return TEXT("Unknown");
	}

	class FAddTasksRequest : public IRemoteMessage
	{
	public:
		FIoHash RequirementsHash;
		TArray<FIoHash> TaskHashes;
		bool DoNotCache;

		REMOTEEXECUTION_API FAddTasksRequest();

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FGetTaskUpdateResponse : public IRemoteMessage
	{
	public:
		FIoHash TaskHash;
		FDateTime Time;
		EComputeTaskState State;
		EComputeTaskOutcome Outcome;
		FString Detail;
		FIoHash ResultHash;
		FString AgentId;
		FString LeaseId;

		REMOTEEXECUTION_API FGetTaskUpdateResponse();

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FGetTaskUpdatesResponse : public IRemoteMessage
	{
	public:
		TArray<FGetTaskUpdateResponse> Updates;

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	struct FGetObjectTreeResponse
	{
		TMap<FIoHash, TArray<uint8>> Objects;
		TSet<FIoHash> BinaryAttachments;
	};

	class FFileNode : public IRemoteMessage
	{
	public:
		FString Name;
		FIoHash Hash;
		int64 Size;
		int32 Attributes;

		REMOTEEXECUTION_API FFileNode();

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FDirectoryNode : public IRemoteMessage
	{
	public:
		FString Name;
		FIoHash Hash;

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FDirectoryTree : public IRemoteMessage
	{
	public:
		TArray<FFileNode> Files;
		TArray<FDirectoryNode> Directories;

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FRequirements : public IRemoteMessage
	{
	public:
		FString Condition;
		TMap<FString, int32> Resources;
		bool Exclusive;

		REMOTEEXECUTION_API FRequirements();

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FTask : public IRemoteMessage
	{
	public:
		FString Executable;
		TArray<FString> Arguments;
		TMap<FString, FString> EnvVars;
		FString WorkingDirectory;
		FIoHash SandboxHash;
		FIoHash RequirementsHash;
		TArray<FString> OutputPaths;

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FTaskResult : public IRemoteMessage
	{
	public:
		int32 ExitCode;
		FIoHash StdOutHash;
		FIoHash StdErrHash;
		FIoHash OutputHash;

		REMOTEEXECUTION_API FTaskResult();

		// Inherited via IMessage
		REMOTEEXECUTION_API virtual FCbObject Save() const override;
		REMOTEEXECUTION_API virtual void Load(const FCbObjectView& CbObjectView) override;
	};
}
