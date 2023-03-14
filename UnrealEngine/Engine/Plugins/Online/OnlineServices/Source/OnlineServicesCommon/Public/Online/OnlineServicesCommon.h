// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Online/OnlineServices.h"

#include "Online/OnlineComponentRegistry.h"
#include "Online/OnlineAsyncOpCache.h"
#include "Online/OnlineAsyncOpQueue.h"
#include "Online/OnlineConfig.h"
#include "Online/OnlineExecHandler.h"
#include "Online/OnlineServicesLog.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Misc/CoreMisc.h"
#include "Templates/SharedPointer.h"

namespace UE::Online {

class ONLINESERVICESCOMMON_API FOnlineServicesCommon
	: public IOnlineServices
	, public TSharedFromThis<FOnlineServicesCommon>
	, public FTSTickerObjectBase
	, public FSelfRegisteringExec
{
public:
	using Super = IOnlineServices;

	FOnlineServicesCommon(const FString& InConfigName, FName InInstanceName);
	FOnlineServicesCommon(const FOnlineServicesCommon&) = delete;
	FOnlineServicesCommon(FOnlineServicesCommon&&) = delete;
	virtual ~FOnlineServicesCommon() {}

	// IOnlineServices
	virtual void Init() override;
	virtual void Destroy() override;
	virtual IAchievementsPtr GetAchievementsInterface() override;
	virtual IAuthPtr GetAuthInterface() override;
	virtual IUserInfoPtr GetUserInfoInterface() override;
	virtual ICommercePtr GetCommerceInterface() override;
	virtual ISocialPtr GetSocialInterface() override;
	virtual IPresencePtr GetPresenceInterface() override;
	virtual IExternalUIPtr GetExternalUIInterface() override;
	virtual ILeaderboardsPtr GetLeaderboardsInterface() override;
	virtual ILobbiesPtr GetLobbiesInterface() override;
	virtual ISessionsPtr GetSessionsInterface() override;
	virtual IStatsPtr GetStatsInterface() override;
	virtual IConnectivityPtr GetConnectivityInterface() override;
	virtual IPrivilegesPtr GetPrivilegesInterface() override;
	virtual ITitleFilePtr GetTitleFileInterface() override;
	virtual IUserFilePtr GetUserFileInterface() override;
	virtual TOnlineResult<FGetResolvedConnectString> GetResolvedConnectString(FGetResolvedConnectString::Params&& Params) override;
	virtual FName GetInstanceName() const override;

	// FOnlineServicesCommon

	/**
	 * Retrieve any of the Interface IOnlineComponents
	 */
	template <typename ComponentType>
	ComponentType* Get()
	{
		return Components.Get<ComponentType>();
	}

	/**
	 * Called to register all the IOnlineComponents with the IOnlineService, called after this is constructed
	 */
	virtual void RegisterComponents();

	/**
	 * Calls Initialize on all the components, called after RegisterComponents
	 */
	virtual void Initialize();

	/**
	 * Calls PostInitialize on all the components, called after Initialize
	 */
	virtual void PostInitialize();

	/**
	 * Calls UpdateConfig on all the components
	 */
	virtual void UpdateConfig();

	/**
	 * Calls Tick on all the components
	 */
	virtual bool Tick(float DeltaSeconds) override;

	/**
	 * Calls PreShutdown on all the components, called prior to Shutdown
	 */
	virtual void PreShutdown();

	/**
	 * Calls Shutdown on all the components, called before this is destructed
	 */
	virtual void Shutdown();

	/**
	 * Call a callable according to a specified execution policy
	 */
	template <typename CallableType>
	void Execute(FOnlineAsyncExecutionPolicy ExecutionPolicy, CallableType&& Callable)
	{
		switch (ExecutionPolicy.GetExecutionPolicy())
		{
			case EOnlineAsyncExecutionPolicy::RunOnGameThread:
				ExecuteOnGameThread(MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunOnNextTick:
				Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunOnThreadPool:
				Async(EAsyncExecution::ThreadPool, MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunOnTaskGraph:
				Async(EAsyncExecution::TaskGraph, MoveTemp(Callable));
				break;

			case EOnlineAsyncExecutionPolicy::RunImmediately:
				Callable();
				break;
		}
	}

	/**
	 * Call a callable on the game thread
	 */
	template <typename CallableType>
	void ExecuteOnGameThread(CallableType&& Callable)
	{
		if (IsInGameThread())
		{
			Callable();
		}
		else
		{
			Async(EAsyncExecution::TaskGraphMainThread, MoveTemp(Callable));
		}
	}

	/**
	 * Override the default config provider (FOnlineConfigProviderGConfig(GEngineini))
	 */
	void SetConfigProvider(TUniquePtr<IOnlineConfigProvider>&& InConfigProvider)
	{
		ConfigProvider = MoveTemp(InConfigProvider);
	}

	/**
	 * Clear the list of config overrides
	 */
	void ResetConfigSectionOverrides()
	{
		ConfigSectionOverrides.Reset();
	}

	/**
	 * Add a config section override. These will be used in the order they are added
	 */
	void AddConfigSectionOverride(const FString& Override)
	{
		ConfigSectionOverrides.Add(Override);
	}

	/**
	 * Get the config name for the Subsystem
	 */
	const FString& GetConfigName() const { return ConfigName; }

	TArray<FString> GetConfigSectionHeiarchy(const FString& OperationName = FString()) const
	{
		TArray<FString> SectionHeiarchy;
		FString SectionName = TEXT("OnlineServices");
		SectionHeiarchy.Add(SectionName);
		SectionName += TEXT(".") + GetConfigName();
		SectionHeiarchy.Add(SectionName);
		if (!OperationName.IsEmpty())
		{
			SectionName += TEXT(".") + OperationName;
			SectionHeiarchy.Add(SectionName);
		}
		return SectionHeiarchy;
	}

	/**
	 * Load a config struct for an interface + operation
	 * Will load values from the following sections:
	 *   OnlineServices
	 *   OnlineServices.<InterfaceName> (if InterfaceName is set)
	 *   OnlineServices.<ServiceProvider>
	 *   OnlineServices.<ServiceProvider>.<InterfaceName> (if InterfaceName is set)
	 *   OnlineServices.<ServiceProvider>.<InterfaceName>.<OperationName> (if OperationName is set)
	 * 
	 * @param Struct Struct to populate with values from config
	 * @param InterfaceName Optional interface name to append to the config section name
	 * @param OperationName Optional operation name to append to the config section name
	 * 
	 * @return true if a value was loaded
	 */
	template <typename StructType>
	bool LoadConfig(StructType& Struct, const FString& InterfaceName = FString(), const FString& OperationName = FString()) const
	{
		TArray<FString> SectionHeiarchy;
		FString SectionName = TEXT("OnlineServices");
		SectionHeiarchy.Add(SectionName);
		if (!InterfaceName.IsEmpty())
		{
			SectionHeiarchy.Add(SectionName + TEXT(".") + InterfaceName);
		}
		SectionName += TEXT(".") + GetConfigName();
		SectionHeiarchy.Add(SectionName);
		if (!InterfaceName.IsEmpty())
		{
			SectionName += TEXT(".") + InterfaceName;
			SectionHeiarchy.Add(SectionName);
			if (!OperationName.IsEmpty())
			{
				SectionName += TEXT(".") + OperationName;
				SectionHeiarchy.Add(SectionName);
			}
		}
		return LoadConfig(Struct, SectionHeiarchy);
	}

	/**
	 * Get an array of a config section with the overrides added in
	 * 
	 * @param SectionHeiarchy Array of config sections to load values from
	 * 
	 * @return Array of the sections with overrides for values to be loaded from
	 */
	TArray<FString> GetConfigSectionHeirachWithOverrides(const TArray<FString>& SectionHeiarchy) const
	{
		TArray<FString> SectionHeiarchyWithOverrides;
		for (const FString& Section : SectionHeiarchy)
		{
			SectionHeiarchyWithOverrides.Add(Section);
			for (const FString& Override : ConfigSectionOverrides)
			{
				FString OverrideSection = Section + TEXT(" ") + Override;
				SectionHeiarchyWithOverrides.Add(OverrideSection);
			}
		}

		return SectionHeiarchyWithOverrides;
	}

	/**
	 * Load a config struct for a section heiarchy, also using the ConfigSectionOverrides
	 *
	 * @param Struct Struct to populate with values from config
	 * @param SectionHeiarchy Array of config sections to load values from
	 *
	 * @return true if a value was loaded
	 */
	template <typename StructType>
	bool LoadConfig(StructType& Struct, const TArray<FString>& SectionHeiarchy) const
	{
		return ::UE::Online::LoadConfig(*ConfigProvider, GetConfigSectionHeirachWithOverrides(SectionHeiarchy), Struct);
	}

	/* Get op (OnlineServices) */
	template <typename OpType>
	TOnlineAsyncOpRef<OpType> GetOp(typename OpType::Params&& Params)
	{
		return OpCache.GetOp<OpType>(MoveTemp(Params), GetConfigSectionHeiarchy());
	}

	template <typename OpType, typename ParamsFuncsType = TJoinableOpParamsFuncs<OpType>>
	TOnlineAsyncOpRef<OpType> GetJoinableOp(typename OpType::Params&& Params)
	{
		return OpCache.GetJoinableOp<OpType, ParamsFuncsType>(MoveTemp(Params), GetConfigSectionHeiarchy());
	}

	template <typename OpType, typename ParamsFuncsType = TMergeableOpParamsFuncs<OpType>>
	TOnlineAsyncOpRef<OpType> GetMergeableOp(typename OpType::Params&& Params)
	{
		return OpCache.GetMergeableOp<OpType, ParamsFuncsType>(MoveTemp(Params), GetConfigSectionHeiarchy());
	}

	/* Get op (Interface) */
	template <typename OpType>
	TOnlineAsyncOpRef<OpType> GetOp(typename OpType::Params&& Params, const TArray<FString> ConfigSectionHeiarchy)
	{
		return OpCache.GetOp<OpType>(MoveTemp(Params), ConfigSectionHeiarchy);
	}

	template <typename OpType, typename ParamsFuncsType /*= TJoinableOpParamsFuncs<OpType>*/>
	TOnlineAsyncOpRef<OpType> GetJoinableOp(typename OpType::Params&& Params, const TArray<FString> ConfigSectionHeiarchy)
	{
		return OpCache.GetJoinableOp<OpType, ParamsFuncsType>(MoveTemp(Params), ConfigSectionHeiarchy);
	}

	template <typename OpType, typename ParamsFuncsType /*= TMergeableOpParamsFuncs<OpType>*/>
	TOnlineAsyncOpRef<OpType> GetMergeableOp(typename OpType::Params&& Params, const TArray<FString> ConfigSectionHeiarchy)
	{
		return OpCache.GetMergeableOp<OpType, ParamsFuncsType>(MoveTemp(Params), ConfigSectionHeiarchy);
	}

	/* Queue for executing tasks in parallel. Serial queues feed into this */
	FOnlineAsyncOpQueueParallel& GetParallelQueue();

	/* Queue for executing tasks in serial */
	FOnlineAsyncOpQueue& GetSerialQueue();

	/* Queues for executing per-user tasks in serial */
	FOnlineAsyncOpQueue& GetSerialQueue(const FAccountId& AccountId);
	
	void RegisterExecHandler(const FString& Name, TUniquePtr<IOnlineExecHandler>&& Handler);

	virtual bool Exec(UWorld* World, const TCHAR* Cmd, FOutputDevice& Ar) override;

	FOnlineAsyncOpCache OpCache;

protected:
	TMap<FString, TUniquePtr<IOnlineExecHandler>> ExecCommands;

	static uint32 NextInstanceIndex;
	uint32 InstanceIndex;
	FName InstanceName;

	FOnlineComponentRegistry Components;
	TUniquePtr<IOnlineConfigProvider> ConfigProvider;

	/* Config section overrides */
	TArray<FString> ConfigSectionOverrides;
	FString ConfigName;

	FOnlineAsyncOpQueueParallel ParallelQueue;
	FOnlineAsyncOpQueueSerial SerialQueue;
	TMap<FAccountId, TUniquePtr<FOnlineAsyncOpQueueSerial>> PerUserSerialQueue;
};

/* UE::Online */ }

