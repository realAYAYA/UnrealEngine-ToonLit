// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IMessageContext.h"
#include "LiveLinkHubMessages.h"

#include "LiveLinkHubUEClientInfo.generated.h"

/**
 * Identifier for a UE client receiving data from the hub.
 */
USTRUCT()
struct FLiveLinkHubClientId
{
	GENERATED_BODY()

	/** Default constructor, should only be used by the reflection code. */
	FLiveLinkHubClientId() = default;

	static FLiveLinkHubClientId NewId()
	{
		FLiveLinkHubClientId Id{};
		Id.Guid = FGuid::NewGuid();
		return Id;
	}

	bool IsValid() const
	{
		return Guid.IsValid();
	}

	/** Get the hash of this ID. */
	friend uint32 GetTypeHash(FLiveLinkHubClientId Id)
	{
		return GetTypeHash(Id.Guid);
	}

	bool operator==(const FLiveLinkHubClientId& Other) const
	{
		return Guid == Other.Guid;
	}

private:
	/** Unique identifier for this client. */
	UPROPERTY()
	FGuid Guid;
};

/** Wrapper around FLiveLinkClientInfoMessage that adds additional info. Used mainly to display information about a client in the UI. */
USTRUCT()
struct FLiveLinkHubUEClientInfo
{
	GENERATED_BODY();

	FLiveLinkHubUEClientInfo() = default;

	explicit FLiveLinkHubUEClientInfo(const FLiveLinkClientInfoMessage& InClientInfo)
		: Id(FLiveLinkHubClientId::NewId())
		, LongName(InClientInfo.LongName)
		, Status(InClientInfo.Status)
		, IPAddress(TEXT("192.168.0.1 (Placeholder)"))
		, Hostname(InClientInfo.Hostname)
		, ProjectName(InClientInfo.ProjectName)
		, CurrentLevel(InClientInfo.CurrentLevel)
	{
	}

	void UpdateFromInfoMessage(const FLiveLinkClientInfoMessage& InClientInfo)
	{
		LongName = InClientInfo.LongName;
		Status = InClientInfo.Status;
		IPAddress = TEXT("192.168.0.1 (Placeholder)");
		Hostname = InClientInfo.Hostname;
		ProjectName = InClientInfo.ProjectName;
		CurrentLevel = InClientInfo.CurrentLevel;
	}

	/** Identifier for this client. */
	UPROPERTY()
	FLiveLinkHubClientId Id;

	/** Full name used to identify this client. (ie.UEFN_sessionID_LDN_WSYS_9999) */
	UPROPERTY(VisibleAnywhere, Category = "Client Details")
   	FString LongName;
	
	/** Status of the client, ie. is it actively doing a take record at the moment? */
	UPROPERTY(transient)
	ELiveLinkClientStatus Status = ELiveLinkClientStatus::Disconnected;
	
	UPROPERTY()
	FString IPAddress;
	
	/** Name of the host of the UE client */
	UPROPERTY(VisibleAnywhere, Category = "Client Details")
	FString Hostname;

	/** Name of the current project. */
	UPROPERTY(VisibleAnywhere, Category = "Client Details")
	FString ProjectName;
	
	/** Name of the current level opened. */
	UPROPERTY(VisibleAnywhere, Category = "Client Details")
	FString CurrentLevel;
	
	/** Subjects that should not be transmitted to this client. */
	UPROPERTY()
	TSet<FName> DisabledSubjects;
	
	/** Whether this client should receive messages. */
	UPROPERTY()
	bool bEnabled = true;
};
