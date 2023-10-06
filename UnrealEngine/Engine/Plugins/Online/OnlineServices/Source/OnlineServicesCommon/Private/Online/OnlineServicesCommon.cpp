// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesCommon.h"

#include "Online/IOnlineComponent.h"
#include "Online/OnlineAsyncOpCache_Meta.inl"
#include "Online/OnlineExecHandler.h"

namespace UE::Online {

struct FOnlineServicesCommonConfig
{
	int32 MaxConcurrentOperations = 16;
};

namespace Meta {

BEGIN_ONLINE_STRUCT_META(FOnlineServicesCommonConfig)
	ONLINE_STRUCT_FIELD(FOnlineServicesCommonConfig, MaxConcurrentOperations)
END_ONLINE_STRUCT_META()

}

uint32 FOnlineServicesCommon::NextInstanceIndex = 0;

FOnlineServicesCommon::FOnlineServicesCommon(const FString& InConfigName, FName InInstanceName)
	: OpCache(InConfigName, *this)
	, InstanceIndex(NextInstanceIndex++)
	, InstanceName(InInstanceName)
	, ConfigProvider(MakeUnique<FOnlineConfigProviderGConfig>(GEngineIni))
	, ConfigName(InConfigName)
	, SerialQueue(ParallelQueue)
{
}

FOnlineServicesCommon::~FOnlineServicesCommon() = default;

void FOnlineServicesCommon::Init()
{
	OpCache.SetLoadConfigFn(
		[this](FOperationConfig& OperationConfig, const TArray<FString>& SectionHeirarchy)
		{
			return LoadConfig(OperationConfig, SectionHeirarchy);
		});

	RegisterComponents();
	Initialize();
	PostInitialize();
}

void FOnlineServicesCommon::Destroy()
{
	PreShutdown();
	Shutdown();
}

IAchievementsPtr FOnlineServicesCommon::GetAchievementsInterface()
{
	return IAchievementsPtr(AsShared(), Get<IAchievements>());
}

ICommercePtr FOnlineServicesCommon::GetCommerceInterface()
{
	return ICommercePtr(AsShared(), Get<ICommerce>());
}


IAuthPtr FOnlineServicesCommon::GetAuthInterface()
{
	return IAuthPtr(AsShared(), Get<IAuth>());
}

IUserInfoPtr FOnlineServicesCommon::GetUserInfoInterface()
{
	return IUserInfoPtr(AsShared(), Get<IUserInfo>());
}

ISocialPtr FOnlineServicesCommon::GetSocialInterface()
{
	return ISocialPtr(AsShared(), Get<ISocial>());
}

IPresencePtr FOnlineServicesCommon::GetPresenceInterface()
{
	return IPresencePtr(AsShared(), Get<IPresence>());
}

IExternalUIPtr FOnlineServicesCommon::GetExternalUIInterface()
{
	return IExternalUIPtr(AsShared(), Get<IExternalUI>());
}

ILeaderboardsPtr FOnlineServicesCommon::GetLeaderboardsInterface()
{
	return ILeaderboardsPtr(AsShared(), Get<ILeaderboards>());
}

ILobbiesPtr FOnlineServicesCommon::GetLobbiesInterface()
{
	return ILobbiesPtr(AsShared(), Get<ILobbies>());
}

ISessionsPtr FOnlineServicesCommon::GetSessionsInterface()
{
	return ISessionsPtr(AsShared(), Get<ISessions>());
}

IStatsPtr FOnlineServicesCommon::GetStatsInterface()
{
	return IStatsPtr(AsShared(), Get<IStats>());
}

IConnectivityPtr FOnlineServicesCommon::GetConnectivityInterface()
{
	return IConnectivityPtr(AsShared(), Get<IConnectivity>());
}

IPrivilegesPtr FOnlineServicesCommon::GetPrivilegesInterface()
{
	return IPrivilegesPtr(AsShared(), Get<IPrivileges>());
}

ITitleFilePtr FOnlineServicesCommon::GetTitleFileInterface()
{
	return ITitleFilePtr(AsShared(), Get<ITitleFile>());
}

IUserFilePtr FOnlineServicesCommon::GetUserFileInterface()
{
	return IUserFilePtr(AsShared(), Get<IUserFile>());
}

TOnlineResult<FGetResolvedConnectString> FOnlineServicesCommon::GetResolvedConnectString(FGetResolvedConnectString::Params&& Params)
{
	return TOnlineResult<FGetResolvedConnectString>(Errors::NotImplemented());
}

FName FOnlineServicesCommon::GetInstanceName() const
{
	return InstanceName;
}

void FOnlineServicesCommon::AssignBaseInterfaceSharedPtr(const FOnlineTypeName& TypeName, void* OutBaseInterfaceSP)
{
	Components.AssignBaseSharedPtr(TypeName, OutBaseInterfaceSP);
}

void FOnlineServicesCommon::RegisterComponents()
{
}

void FOnlineServicesCommon::Initialize()
{
	Components.Visit(&IOnlineComponent::Initialize);
}

void FOnlineServicesCommon::PostInitialize()
{
	Components.Visit(&IOnlineComponent::PostInitialize);

	LoadCommonConfig();
}

void FOnlineServicesCommon::UpdateConfig()
{
	Components.Visit(&IOnlineComponent::UpdateConfig);

	LoadCommonConfig();
}

bool FOnlineServicesCommon::Tick(float DeltaSeconds)
{
	Components.Visit(&IOnlineComponent::Tick, DeltaSeconds);

	ParallelQueue.Tick(DeltaSeconds);

	return true;
}

void FOnlineServicesCommon::PreShutdown()
{
	Components.Visit(&IOnlineComponent::PreShutdown);
}

void FOnlineServicesCommon::Shutdown()
{
	Components.Visit(&IOnlineComponent::Shutdown);
}

FOnlineAsyncOpQueueParallel& FOnlineServicesCommon::GetParallelQueue()
{
	return ParallelQueue;
}

FOnlineAsyncOpQueue& FOnlineServicesCommon::GetSerialQueue()
{
	return SerialQueue;
}

FOnlineAsyncOpQueue& FOnlineServicesCommon::GetSerialQueue(const FAccountId& AccountId)
{
	TUniquePtr<FOnlineAsyncOpQueueSerial>* Queue = PerUserSerialQueue.Find(AccountId);
	if (Queue == nullptr)
	{
		Queue = &PerUserSerialQueue.Emplace(AccountId, MakeUnique<FOnlineAsyncOpQueueSerial>(ParallelQueue));
	}

	return **Queue;
}

void FOnlineServicesCommon::RegisterExecHandler(const FString& Name, TUniquePtr<IOnlineExecHandler>&& Handler)
{
	ExecCommands.Emplace(Name, MoveTemp(Handler));
}

#if UE_ALLOW_EXEC_COMMANDS
bool FOnlineServicesCommon::Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("OnlineServices")))
	{
		int Index = 0;
		if (FParse::Value(Cmd, TEXT("Index="), Index) && Index == InstanceIndex)
		{
			FParse::Token(Cmd, false); // skip over Index=#

			FString Command;
			if (FParse::Token(Cmd, Command, false))
			{
				if (TUniquePtr<IOnlineExecHandler>* ExecHandler = ExecCommands.Find(Command))
				{
					return (*ExecHandler)->Exec(World, Cmd, Ar);
				}
			}
		}
		else if (FParse::Command(&Cmd, TEXT("List")))
		{
			Ar.Logf(TEXT("%u: %s"), InstanceIndex, *GetConfigName());
		}
	}
	return false;
}
#endif // UE_ALLOW_EXEC_COMMANDS

void FOnlineServicesCommon::LoadCommonConfig()
{
	FOnlineServicesCommonConfig Config;
	LoadConfig(Config);

	ParallelQueue.SetMaxConcurrentOperations(Config.MaxConcurrentOperations);
}

/* UE::Online */ }
