// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Misc/SecureHash.h"
#include "Templates/TypeHash.h"

enum class ESwitchboardTaskType : uint8
{
	Start,
	Kill,
	Restart,
	ReceiveFileFromClient,
	SendFileToClient,
	Disconnect,
	KeepAlive,
	GetSyncStatus,
	GetFlipMode,
	FixExeFlags,
	RedeployListener,
	RefreshMosaics,
	MinimizeWindows,
	SetInactiveTimeout,
};


struct FSwitchboardTask
{
	ESwitchboardTaskType Type;
	FString Name;
	FGuid TaskID;
	FIPv4Endpoint Recipient;

	FSwitchboardTask(ESwitchboardTaskType InType, FString InName, FGuid InTaskID, FIPv4Endpoint InRecipient)
		: Type(InType)
		, Name(InName)
		, TaskID(InTaskID)
		, Recipient(InRecipient)
	{}

	/** Calculates a hash that should be the same for equivalent Tasks, even if their TaskID is different */
	virtual uint32 GetEquivalenceHash() const
	{
		return HashCombine(GetTypeHash(Type), GetTypeHash(Recipient));
	}
	
	virtual ~FSwitchboardTask()
	{}
};

struct FSwitchboardGetSyncStatusTask : public FSwitchboardTask
{
	FSwitchboardGetSyncStatusTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint, const FGuid& InProgramID)
		: FSwitchboardTask{ ESwitchboardTaskType::GetSyncStatus, TEXT("get sync status"), InTaskId, InEndpoint }
		, ProgramID(InProgramID)
	{}

	/** ID of the program that we wish to get the FlipMode of */
	FGuid ProgramID;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		return HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(ProgramID));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardRefreshMosaicsTask : public FSwitchboardTask
{
	FSwitchboardRefreshMosaicsTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint)
		: FSwitchboardTask{ ESwitchboardTaskType::RefreshMosaics, TEXT("refresh mosaics"), InTaskId, InEndpoint }
	{}

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		return HashCombine(GetTypeHash(Type), GetTypeHash(Name));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardStartTask : public FSwitchboardTask
{
	FSwitchboardStartTask(
		const FGuid& InTaskId, 
		const FIPv4Endpoint& InEndpoint, 
		const FString& InCommand, 
		const FString& InArgs, 
		const FString& InName, 
		const FString& InCaller, 
		const FString& InWorkingDir
	)
		: FSwitchboardTask{ ESwitchboardTaskType::Start, TEXT("start"), InTaskId, InEndpoint }
		, Command(InCommand)
		, Arguments(InArgs)
		, Name(InName)
		, Caller(InCaller)
		, WorkingDir(InWorkingDir)
	{
	}

	FString Command;
	FString Arguments;
	FString Name;
	FString Caller;
	FString WorkingDir;
	bool bUpdateClientsWithStdout = false;
	int32 PriorityModifier = 0;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		uint32 Hash = HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(Command));
		Hash = HashCombine(Hash, GetTypeHash(Caller));
		return HashCombine(Hash, GetTypeHash(Arguments));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardKillTask : public FSwitchboardTask
{
	FSwitchboardKillTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint, const FGuid& InProgramID)
		: FSwitchboardTask{ ESwitchboardTaskType::Kill, TEXT("kill"), InTaskId, InEndpoint}
		, ProgramID(InProgramID)
	{}

	FGuid ProgramID; // unique ID of process to kill

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		return HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(ProgramID));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardReceiveFileFromClientTask : public FSwitchboardTask
{
	FSwitchboardReceiveFileFromClientTask(const FGuid& InTaskID, const FIPv4Endpoint& InEndpoint, const FString& InDestination, const FString& InContent)
		: FSwitchboardTask{ ESwitchboardTaskType::ReceiveFileFromClient, TEXT("receive file from client"), InTaskID, InEndpoint }
		, Destination(InDestination)
		, FileContent(InContent)
	{}

	FString Destination;
	FString FileContent;
	bool bForceOverwrite = false;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		uint32 Hash = HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(Destination));
		return HashCombine(Hash, GetTypeHash(FileContent));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardRedeployListenerTask : public FSwitchboardTask
{
	FSwitchboardRedeployListenerTask(const FGuid& InTaskID, const FIPv4Endpoint& InEndpoint, const FString& InExpectedHashHexDigest, const FString& InContent)
		: FSwitchboardTask{ ESwitchboardTaskType::RedeployListener, TEXT("deploy new listener executable from client"), InTaskID, InEndpoint }
		, FileContent(InContent)
	{
		ExpectedHash.FromString(InExpectedHashHexDigest);
	}

	FSHAHash ExpectedHash;
	FString FileContent;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		uint32 Hash = HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(ExpectedHash));
		return HashCombine(Hash, GetTypeHash(FileContent));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardSendFileToClientTask : public FSwitchboardTask
{
	FSwitchboardSendFileToClientTask(const FGuid& InTaskID, const FIPv4Endpoint& InEndpoint, const FString& InSource)
		: FSwitchboardTask{ ESwitchboardTaskType::SendFileToClient, TEXT("send file to client"), InTaskID, InEndpoint }
		, Source(InSource)
	{}

	FString Source;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		return HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(Source));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardFixExeFlagsTask : public FSwitchboardTask
{
	FSwitchboardFixExeFlagsTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint, const FGuid& InProgramID)
		: FSwitchboardTask{ ESwitchboardTaskType::FixExeFlags, TEXT("fixExeFlags"), InTaskId, InEndpoint }
		, ProgramID(InProgramID)
	{}

	FGuid ProgramID;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		return HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(ProgramID));
	}
	//~ End FSwitchboardTask interface
};

struct FSwitchboardDisconnectTask : public FSwitchboardTask
{
	FSwitchboardDisconnectTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint)
		: FSwitchboardTask{ESwitchboardTaskType::Disconnect, TEXT("disconnect"), InTaskId, InEndpoint}
	{}
};

struct FSwitchboardKeepAliveTask : public FSwitchboardTask
{
	FSwitchboardKeepAliveTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint)
		: FSwitchboardTask{ESwitchboardTaskType::KeepAlive, TEXT("keep alive"), InTaskId, InEndpoint}
	{}
};

struct FSwitchboardMinimizeWindowsTask : public FSwitchboardTask
{
	FSwitchboardMinimizeWindowsTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint)
		: FSwitchboardTask{ ESwitchboardTaskType::MinimizeWindows, TEXT("minimize windows"), InTaskId, InEndpoint }
	{}
};

struct FSwitchboardSetInactiveTimeoutTask : public FSwitchboardTask
{
	FSwitchboardSetInactiveTimeoutTask(const FGuid& InTaskId, const FIPv4Endpoint& InEndpoint, double InTimeoutSeconds)
		: FSwitchboardTask{ ESwitchboardTaskType::SetInactiveTimeout, TEXT("set inactive timeout"), InTaskId, InEndpoint }
		, TimeoutSeconds(InTimeoutSeconds)
	{}

	double TimeoutSeconds;

	//~ Begin FSwitchboardTask interface
	virtual uint32 GetEquivalenceHash() const override
	{
		return HashCombine(FSwitchboardTask::GetEquivalenceHash(), GetTypeHash(TimeoutSeconds));
	}
	//~ End FSwitchboardTask interface
};

