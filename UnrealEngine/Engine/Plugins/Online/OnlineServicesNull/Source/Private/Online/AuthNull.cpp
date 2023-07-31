// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/AuthNull.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Misc/CoreDelegates.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeRWLock.h"
#include "Online/OnlineServicesNull.h"
#include "Online/OnlineServicesNullTypes.h"
#include "Online/AuthErrors.h"
#include "Online/OnlineErrorDefinitions.h"
#include "SocketSubsystem.h"

#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

namespace UE::Online {
// Copied from OSS Null

struct FAuthNullConfig
{
	bool bAddUserNumToNullId = false;
	bool bForceStableNullId = false;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FAuthNullConfig)
	ONLINE_STRUCT_FIELD(FAuthNullConfig, bAddUserNumToNullId),
	ONLINE_STRUCT_FIELD(FAuthNullConfig, bForceStableNullId)
END_ONLINE_STRUCT_META()

/* Meta*/ }

namespace {

FString GenerateRandomUserId(const FAuthNullConfig& Config, FPlatformUserId PlatformUserId)
{
	FString HostName;
	if(ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
	{
		if (!SocketSubsystem->GetHostName(HostName))
		{
			// could not get hostname, use address
			bool bCanBindAll;
			TSharedPtr<class FInternetAddr> Addr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
			HostName = Addr->ToString(false);
		}
	}

	bool bUseStableNullId = Config.bForceStableNullId;
	FString UserSuffix;

	if (Config.bAddUserNumToNullId)
	{
		UserSuffix = FString::Printf(TEXT("-%d"), PlatformUserId.GetInternalId());
	}
	 
	if (FPlatformProcess::IsFirstInstance() && !GIsEditor)
	{
		// If we're outside the editor and know this is the first instance, use the system login id
		bUseStableNullId = true;
	}

	if (bUseStableNullId)
	{
		// Use a stable id possibly with a user num suffix
		return FString::Printf(TEXT("OSSV2-%s-%s%s"), *HostName, *FPlatformMisc::GetLoginId().ToUpper(), *UserSuffix);
	}

	// If we're not the first instance (or in the editor), return truly random id
	return FString::Printf(TEXT("OSSV2-%s-%s%s"), *HostName, *FGuid::NewGuid().ToString(), *UserSuffix);
}

TSharedRef<FAccountInfoNull> CreateAccountInfo(const FAuthNullConfig& Config, FPlatformUserId PlatformUserId)
{
	const FString DisplayId = GenerateRandomUserId(Config, PlatformUserId);
	return MakeShared<FAccountInfoNull>(FAccountInfoNull{ {
		FOnlineAccountIdRegistryNull::Get().FindOrAddAccountId(DisplayId),
		PlatformUserId,
		ELoginStatus::LoggedIn,
		{ { AccountAttributeData::DisplayName, DisplayId } }
		} });
}

/* anonymous*/ }

TSharedPtr<FAccountInfoNull> FAccountInfoRegistryNULL::Find(FPlatformUserId PlatformUserId) const
{
	return StaticCastSharedPtr<FAccountInfoNull>(Super::Find(PlatformUserId));
}

TSharedPtr<FAccountInfoNull> FAccountInfoRegistryNULL::Find(FAccountId AccountId) const
{
	return StaticCastSharedPtr<FAccountInfoNull>(Super::Find(AccountId));
}

void FAccountInfoRegistryNULL::Register(const TSharedRef<FAccountInfoNull>& AccountInfoNULL)
{
	FWriteScopeLock Lock(IndexLock);
	DoRegister(AccountInfoNULL);
}

void FAccountInfoRegistryNULL::Unregister(FAccountId AccountId)
{
	if (TSharedPtr<FAccountInfoNull> AccountInfoNULL = Find(AccountId))
	{
		FWriteScopeLock Lock(IndexLock);
		DoUnregister(AccountInfoNULL.ToSharedRef());
	}
	else
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("[FAccountInfoRegistryNULL::Unregister] Failed to find account [%s]."), *ToLogString(AccountId));
	}
}

FAuthNull::FAuthNull(FOnlineServicesNull& InServices)
	: FAuthCommon(InServices)
{
}

void FAuthNull::Initialize()
{
	FAuthCommon::Initialize();
	InitializeUsers();
}

void FAuthNull::PreShutdown()
{
	FAuthCommon::PreShutdown();
	UninitializeUsers();
}

const FAccountInfoRegistry& FAuthNull::GetAccountInfoRegistry() const
{
	return AccountInfoRegistryNULL;
}

void FAuthNull::InitializeUsers()
{
	FAuthNullConfig AuthNullConfig;
	LoadConfig(AuthNullConfig);

	// There is no "login" for Null - all local users are initialized as "logged in".
	TArray<FPlatformUserId> Users;
	IPlatformInputDeviceMapper::Get().GetAllActiveUsers(Users);
	Algo::ForEach(Users, [&](FPlatformUserId PlatformUserId)
	{
		AccountInfoRegistryNULL.Register(CreateAccountInfo(AuthNullConfig, PlatformUserId));
	});

	// Setup hook to add new users when they become available.
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().AddRaw(this, &FAuthNull::OnInputDeviceConnectionChange);
}

void FAuthNull::UninitializeUsers()
{
	IPlatformInputDeviceMapper::Get().GetOnInputDeviceConnectionChange().RemoveAll(this);
}

void FAuthNull::OnInputDeviceConnectionChange(EInputDeviceConnectionState NewConnectionState, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId)
{
	// If this is a new platform user then register an entry for them so they will be seen as "logged-in".
	if (!AccountInfoRegistryNULL.Find(PlatformUserId))
	{
		FAuthNullConfig AuthNullConfig;
		LoadConfig(AuthNullConfig);

		TSharedRef<FAccountInfoNull> AccountInfo = CreateAccountInfo(AuthNullConfig, PlatformUserId);
		AccountInfoRegistryNULL.Register(AccountInfo);
		OnAuthLoginStatusChangedEvent.Broadcast(FAuthLoginStatusChanged{ AccountInfo, ELoginStatus::LoggedIn });
	}
}

// FOnlineAccountIdRegistryNull
FOnlineAccountIdRegistryNull& FOnlineAccountIdRegistryNull::Get()
{
	static FOnlineAccountIdRegistryNull Instance;
	return Instance;
}

FAccountId FOnlineAccountIdRegistryNull::Find(const FString& AccountId) const
{
	const FReadScopeLock ReadLock(Lock);
	const FOnlineAccountIdString* Entry = FindNoLock(AccountId);
	return Entry ? Entry->AccountId : FAccountId();
}

const FOnlineAccountIdString* FOnlineAccountIdRegistryNull::FindNoLock(const FAccountId& AccountId) const
{
	if(AccountId.IsValid() && AccountId.GetOnlineServicesType() == EOnlineServices::Null && AccountId.GetHandle() <= (uint32)Ids.Num())
	{
		return &Ids[AccountId.GetHandle()-1];
	}
	return nullptr;
}

const FOnlineAccountIdString* FOnlineAccountIdRegistryNull::FindNoLock(const FString& AccountId) const
{
	FOnlineAccountIdString* const* Entry = StringToIdIndex.Find(AccountId);
	return Entry ? *Entry : nullptr;
}

FAccountId FOnlineAccountIdRegistryNull::FindOrAddAccountId(const FString& AccountId)
{
	// Check for existing entry under read lock.
	{
		const FReadScopeLock ReadLock(Lock);
		if (const FOnlineAccountIdString* ExistingAccountId = FindNoLock(AccountId))
		{
			return ExistingAccountId->AccountId;
		}
	}

	// Check for existing entry again under write lock before adding entry.
	{
		const FWriteScopeLock WriteLock(Lock);
		if (const FOnlineAccountIdString* ExistingAccountId = FindNoLock(AccountId))
		{
			return ExistingAccountId->AccountId;
		}

		FOnlineAccountIdString& Id = Ids.Emplace_GetRef();
		Id.AccountIndex = Ids.Num();
		Id.Data = AccountId;
		Id.AccountId = FAccountId(EOnlineServices::Null, Id.AccountIndex);

		StringToIdIndex.Add(AccountId, &Id);
		return Id.AccountId;
	}
}

FString FOnlineAccountIdRegistryNull::ToLogString(const FAccountId& AccountId) const
{
	const FReadScopeLock ReadLock(Lock);
	if(const FOnlineAccountIdString* Id = FindNoLock(AccountId))
	{
		return Id->Data;
	}

	return FString(TEXT("[InvalidNetID]"));
}


TArray<uint8> FOnlineAccountIdRegistryNull::ToReplicationData(const FAccountId& AccountId) const
{
	const FReadScopeLock ReadLock(Lock);
	if (const FOnlineAccountIdString* Id = FindNoLock(AccountId))
	{
		TArray<uint8> ReplicationData;
		ReplicationData.SetNumUninitialized(Id->Data.Len());
		StringToBytes(Id->Data, ReplicationData.GetData(), Id->Data.Len());
		UE_LOG(LogOnlineServices, VeryVerbose, TEXT("[FOnlineAccountIdRegistryNull::ToReplicationData] StringToBytes on %s returned %d len"), *Id->Data, ReplicationData.Num())
		return ReplicationData;
	}

	return TArray<uint8>();
}

FAccountId FOnlineAccountIdRegistryNull::FromReplicationData(const TArray<uint8>& ReplicationData)
{
	FString Result = BytesToString(ReplicationData.GetData(), ReplicationData.Num());
	if(Result.Len() > 0)
	{
		return FindOrAddAccountId(Result);
	}
	return FAccountId();
}

/* UE::Online */ }
