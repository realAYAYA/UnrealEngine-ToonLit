// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleManager.h"

#include "MGameSession.h"

#include "RedisClient.h"
#include "MyTcpServer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMGameServices, Log, All);

class MGAMESERVICES_API FMGameServicesModule : public IModuleInterface
{
	
public:

	FMGameServicesModule();
	virtual ~FMGameServicesModule() override;

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	void Start();
	void Shutdown();
	bool IsRunning() const;

	void HandleCorePreExit() { Shutdown(); }

	TUniquePtr<FRedisClient> RedisClient;

private:

	void OnFirstTick();
	bool Tick(float);

	void DoAliveCheck(FDateTime Now);

	void OnDailyRefresh();
	void OnWeeklyRefresh();

	TMap<uint64, FMGameSessionPtr> Sessions;

	TSharedPtr<FPbTcpServer> NetServer;
	
	FTSTicker::FDelegateHandle TickDelegateHandle;
	
	FDateTime LastTickTime{0};
	FDateTime NextSessionAliveCheckTime {0};
	FDateTime NextRedisAliveCheckTime{0};
	FDateTime LastDailyRefreshTime{0};

	bool bStarted = false;
};

extern FMGameServicesModule* GGameServicesModule;