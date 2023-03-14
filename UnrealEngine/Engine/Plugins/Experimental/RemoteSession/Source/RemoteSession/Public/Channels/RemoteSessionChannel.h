// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/StringView.h"
#include "IRemoteSessionRole.h"
#include "BackChannel/Types.h"
#include "RemoteSessionTypes.h"

class REMOTESESSION_API IRemoteSessionChannel
{
public:

	IRemoteSessionChannel(ERemoteSessionChannelMode InRole, TBackChannelSharedPtr<IBackChannelConnection> InConnection) {}

	virtual ~IRemoteSessionChannel() {}

	virtual void Tick(const float InDeltaTime) = 0;

	virtual const TCHAR* GetType() const = 0;
};

class REMOTESESSION_API IRemoteSessionChannelFactoryWorker
{
public:
	virtual ~IRemoteSessionChannelFactoryWorker() {}
	virtual TSharedPtr<IRemoteSessionChannel> Construct(ERemoteSessionChannelMode InMode, TBackChannelSharedPtr<IBackChannelConnection> InConnection) const = 0;
};


class REMOTESESSION_API FRemoteSessionChannelRegistry
{
public:
	static FRemoteSessionChannelRegistry& Get()
	{
		static FRemoteSessionChannelRegistry Instance;
		return Instance;
	}

	void RegisterChannelFactory(const TCHAR* InChannelName, ERemoteSessionChannelMode InHostMode, TWeakPtr<IRemoteSessionChannelFactoryWorker> InFactory);
	void RemoveChannelFactory(TWeakPtr<IRemoteSessionChannelFactoryWorker> InFactory);

	TSharedPtr<IRemoteSessionChannel> CreateChannel(const FStringView InChannelName, ERemoteSessionChannelMode InMode, TBackChannelSharedPtr<IBackChannelConnection> InConnection);
	const TArray<FRemoteSessionChannelInfo>& GetRegisteredFactories() const;

protected:

	FRemoteSessionChannelRegistry() = default;
	TMap<FString, TWeakPtr<IRemoteSessionChannelFactoryWorker>> RegisteredFactories;
	TArray<FRemoteSessionChannelInfo> KnownChannels;
};


#define REGISTER_CHANNEL_FACTORY(ChannelName, FactoryClass, HostMode ) \
	class AutoRegister_##ChannelName { \
	public: \
		AutoRegister_##ChannelName() \
		{ \
			static auto Factory =  MakeShared<FactoryClass>(); \
			FRemoteSessionChannelRegistry::Get().RegisterChannelFactory(TEXT(#ChannelName), HostMode, Factory); \
		} \
	}; \
	AutoRegister_##ChannelName G##ChannelName