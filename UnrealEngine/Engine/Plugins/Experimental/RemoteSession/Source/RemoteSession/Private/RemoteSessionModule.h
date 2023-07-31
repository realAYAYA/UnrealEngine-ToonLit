// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSession.h"
#include "RemoteSessionRole.h"
#include "Tickable.h"

class FRemoteSessionHost;
class FRemoteSessionClient;
class IRemoteSessionChannelFactoryWorker;



class FRemoteSessionModule : public IRemoteSessionModule, public FTickableGameObject
{
private:

	TSharedPtr<FRemoteSessionHost>							Host;
	TSharedPtr<FRemoteSessionClient>						Client;

	//TArray<TWeakPtr<IRemoteSessionChannelFactoryWorker>>	FactoryWorkers;
	//TArray<TSharedPtr<IRemoteSessionChannelFactoryWorker>>	BuiltInFactory;

	int32													DefaultPort = IRemoteSessionModule::kDefaultPort;

	bool													bAutoHostWithPIE = true;
	bool													bAutoHostWithGame = true;

	//TArray<FRemoteSessionChannelInfo>						ProgramaticallySupportedChannels;

	FDelegateHandle PostPieDelegate;
	FDelegateHandle EndPieDelegate;
	FDelegateHandle GameStartDelegate;

public:

	//~ Begin IRemoteSessionModule interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void AddChannelFactory(const FStringView InChannelName, ERemoteSessionChannelMode InHostMode, TWeakPtr<IRemoteSessionChannelFactoryWorker> Worker) override;
	void RemoveChannelFactory(TWeakPtr<IRemoteSessionChannelFactoryWorker> Worker) override;

	virtual TSharedPtr<IRemoteSessionRole>	CreateClient(const TCHAR* RemoteAddress) override;
	virtual void StopClient(TSharedPtr<IRemoteSessionRole> InClient, const FString& InReason) override;

	virtual void InitHost(const int16 Port = 0) override;
	virtual bool IsHostRunning() const override { return Host.IsValid(); }
	virtual bool IsHostConnected() const override;
	virtual void StopHost(const FString& InReason) override;
	virtual TSharedPtr<IRemoteSessionRole> GetHost() const override;
	virtual TSharedPtr<IRemoteSessionUnmanagedRole> CreateHost(TArray<FRemoteSessionChannelInfo> SupportedChannels, int32 Port) const override;
	//~ End IRemoteSessionModule interface

	//~ Begin FTickableGameObject interface
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(FRemoteSession, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return true; }
	virtual void Tick(float DeltaTime) override;
	//~ End FTickableGameObject interface

	void SetAutoStartWithPIE(bool bEnable);

private:
	void OnPostInit();
	void OnPreExit();
	bool HandleSettingsSaved();

	TSharedPtr<FRemoteSessionHost> CreateHostInternal(TArray<FRemoteSessionChannelInfo> SupportedChannels, int32 Port) const;

	void OnPIEStarted(bool bSimulating);
	void OnPIEEnded(bool bSimulating);
};
