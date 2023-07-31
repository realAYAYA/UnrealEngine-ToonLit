// Copyright Epic Games, Inc. All Rights Reserved.

#include "Channels/RemoteSessionChannel.h"
#include "RemoteSession.h"
#include "Containers/StringFwd.h"


void FRemoteSessionChannelRegistry::RegisterChannelFactory(const TCHAR* InChannelName, ERemoteSessionChannelMode InHostMode, TWeakPtr<IRemoteSessionChannelFactoryWorker> InFactory)
{
	RegisteredFactories.Add(InChannelName, InFactory);
	KnownChannels.Emplace(InChannelName, InHostMode);
}

void FRemoteSessionChannelRegistry::RemoveChannelFactory(TWeakPtr<IRemoteSessionChannelFactoryWorker> InFactory)
{
	// #agrant todo: implement this
	//check(0);
	//RegisteredFactories.Add(InChannelName, InFactory);

	for (auto& It : RegisteredFactories)
	{
		if (It.Value == InFactory)
		{
			KnownChannels = KnownChannels.FilterByPredicate([It](const FRemoteSessionChannelInfo& Item) {
				return Item.Type == It.Key;
			});

			RegisteredFactories.Remove(It.Key);
			return;
		}
	}
}


TSharedPtr<IRemoteSessionChannel> FRemoteSessionChannelRegistry::CreateChannel(const FStringView InChannelName, ERemoteSessionChannelMode InMode, TBackChannelSharedPtr<IBackChannelConnection> InConnection)
{
	FString Name = FString(InChannelName.Len(), InChannelName.GetData());

	if (!RegisteredFactories.Contains(Name))
	{
		UE_LOG(LogRemoteSession, Error, TEXT("No factory registered for RemoteSession channel '%s'"), *Name);
		return TSharedPtr<IRemoteSessionChannel>();
	}

	TSharedPtr<IRemoteSessionChannelFactoryWorker> PinnedFactory = RegisteredFactories[Name].Pin();

	if (PinnedFactory.IsValid())
	{
		return PinnedFactory->Construct(InMode, InConnection);
	}

	return TSharedPtr<IRemoteSessionChannel>();
}

const TArray<FRemoteSessionChannelInfo>& FRemoteSessionChannelRegistry::GetRegisteredFactories() const
{
	return KnownChannels;
}
