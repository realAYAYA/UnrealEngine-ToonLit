// Copyright Epic Games, Inc. All Rights Reserved.

#include "MGameServerPrivate.h"

#include "GameTables.h"
#include "GameTablesModule.h"

DEFINE_LOG_CATEGORY(LogMGameServices);

#define LOCTEXT_NAMESPACE "FMGameServicesModule"

IMPLEMENT_MODULE(FMGameServicesModule, MGameServices);

#undef LOCTEXT_NAMESPACE

FMGameServicesModule* GGameServicesModule = nullptr;

FMGameServicesModule::FMGameServicesModule()
{
	GGameServicesModule = this;
}

FMGameServicesModule::~FMGameServicesModule()
{
	GGameServicesModule = nullptr;
}

void FMGameServicesModule::StartupModule()
{
	FCoreDelegates::OnPreExit.AddRaw(this, &FMGameServicesModule::HandleCorePreExit);
}

void FMGameServicesModule::ShutdownModule()
{
	// 注意，这里的代码能会有模块依赖问题（所依赖的模块已经被提前卸载）
	// 清理代码最好放在 FMGameServicesModule::HandleCorePreExit 中 
	
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}
}

void FMGameServicesModule::Start()
{
	LastTickTime = FMyTools::Now();
	if (!TickDelegateHandle.IsValid())
	{
		TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FMGameServicesModule::Tick), 0.030);
	}

	//FString ListenIp = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.listen_ip;
	const int32 ListenPort = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.port;

	NetServer = MakeShared<FPbTcpServer>();
	NetServer->SetPackageCallback([this](const FPbConnectionPtr& Conn, uint64 Code, const FMyDataBufferPtr& Message)
	{
		const uint64 ConnId = Conn->GetId();
		if (const auto Ptr = this->Sessions.Find(ConnId))
		{
			(*Ptr)->OnMessage(Code, Message);
		}
	});
	NetServer->SetConnectedCallback([this](const FPbConnectionPtr& Conn)
	{
		const uint64 ConnId = Conn->GetId();
		auto Ptr = MakeShared<FMGameSession>(Conn);
		this->Sessions.Emplace(ConnId, Ptr);

		Ptr->OnConnected();
	});
	NetServer->SetDisconnectedCallback([this](const FPbConnectionPtr& Conn)
	{
		const uint64 ConnId = Conn->GetId();
		if (const auto Ptr = this->Sessions.Find(ConnId))
		{
			(*Ptr)->OnDisconnected();
		}
		this->Sessions.Remove(ConnId);
	});

	bool Ret = NetServer->Start(ListenPort);
	if (!Ret)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Listen Error"));
		return;
	}

	const FString RedisIp = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.redis_ip;
	const int32 RedisPort = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.redis_port;
	const FString RedisPassword = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.redis_password;
	RedisClient = MakeUnique<FRedisClient>();
	Ret = RedisClient->ConnectToRedis(RedisIp, RedisPort, RedisPassword);
	if (!Ret)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString("Redis Error"));
		return;
	}
	
	// Todo 各个公共模块初始化

	OnFirstTick();
	bStarted = true;
}

void FMGameServicesModule::Shutdown()
{
	if (TickDelegateHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickDelegateHandle);
		TickDelegateHandle.Reset();
	}

	if (bStarted)
	{
		if (NetServer)
			NetServer->Stop();
		
		// Todo 各个公共模块Shutdown

		if (RedisClient)
			RedisClient->DisconnectRedis();

		bStarted = false;
	}
}

bool FMGameServicesModule::IsRunning() const
{
	if (NetServer)
		return NetServer->IsRunning();
	
	return false;
}

void FMGameServicesModule::OnFirstTick()
{
	
}

bool FMGameServicesModule::Tick(float)
{
	const FDateTime Now = FMyTools::Now();
	const float DeltaTime = (Now - LastTickTime).GetTotalSeconds();
	LastTickTime = Now;

	if (Now > NextRedisAliveCheckTime)
	{
		if (RedisClient)
		{
			if (!RedisClient->ExecCommand("PING"))
			{
				const FString RedisIp = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.redis_ip;
				const int32 RedisPort = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.redis_port;
				const FString RedisPassword = FGameTablesModule::Get().GetGameTables()->GameServicesConfig.redis_password;
				RedisClient->ConnectToRedis(RedisIp, RedisPort, RedisPassword);
			}
		}
		
		NextRedisAliveCheckTime = Now + FTimespan::FromSeconds(20);
	}

	if (NetServer)
		NetServer->Tick(DeltaTime);

	const FDateTime LocalNow = FMyTools::LocalNow();
	if (LocalNow.GetHour() == 8 && LocalNow.GetMinute() == 0 && LocalNow.GetSecond() < 10)
	{
		auto NowDT = LocalNow.GetDate();
		if (NowDT != LastDailyRefreshTime)
		{
			LastDailyRefreshTime = NowDT;
			OnDailyRefresh();

			if (LocalNow.GetDayOfWeek() == EDayOfWeek::Monday)
				OnWeeklyRefresh();
		}
	}

	// Todo 公共模块Tick

	if (Now > NextSessionAliveCheckTime)
		DoAliveCheck(Now);
	
	return true;
}

void FMGameServicesModule::DoAliveCheck(FDateTime Now)
{
	NextSessionAliveCheckTime = Now + FTimespan::FromSeconds(3);
	
#if WITH_EDITOR
	return;  // 编辑器模式运行不做超时检查
#endif
	for (const auto& Elem : Sessions)
	{
		const auto Session = Elem.Value;
		if (Session && Session->Connection)
		{
			const FDateTime LastTime = FMath::Min(Session->GetLastSentTime(), Session->GetLastReceivedTime());
			const int32 Seconds = (Now - LastTime).GetTotalSeconds();
			if (Seconds >= 60)
			{
				const int32 SentSeconds = (Now - Session->GetLastSentTime()).GetTotalSeconds();
				const int32 ReceivedSeconds = (Now - Session->GetLastReceivedTime()).GetTotalSeconds();
				UE_LOG(LogMGameServices, Warning, TEXT("[网络模块] 通讯超时 ConnId=%llu Last=%d SentSeconds=%d RecvSeconds=%d"), Session->Connection->GetId(), Seconds, SentSeconds, ReceivedSeconds);
				Session->Connection->Shutdown();
			}
		}
	}
}

void FMGameServicesModule::OnDailyRefresh()
{
	// Todo 日刷新
}

void FMGameServicesModule::OnWeeklyRefresh()
{
	// Todo 周刷新
}
