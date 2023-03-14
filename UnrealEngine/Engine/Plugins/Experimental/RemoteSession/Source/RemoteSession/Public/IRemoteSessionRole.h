// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RemoteSessionTypes.h"

class IRemoteSessionChannel;


class REMOTESESSION_API IRemoteSessionRole
{
public:
	virtual~IRemoteSessionRole() {}

	virtual bool IsConnected() const = 0;
	
	virtual bool HasError() const = 0;
	
	virtual FString GetErrorMessage() const = 0;

	/* Future versions will support querying the version type, at the moment we just have old & new */
	virtual bool IsLegacyConnection() const = 0;


	/* Registers a delegate for notifications of connection changes*/
	virtual FDelegateHandle RegisterConnectionChangeDelegate(FOnRemoteSessionConnectionChange::FDelegate InDelegate) = 0;

	/* Register for notifications when the host sends a list of available channels */
	virtual FDelegateHandle RegisterChannelListDelegate(FOnRemoteSessionReceiveChannelList::FDelegate InDelegate) = 0;

	/* Register for notifications whenever a change in the state of a channel occurs */
	virtual FDelegateHandle RegisterChannelChangeDelegate(FOnRemoteSessionChannelChange::FDelegate InDelegate) = 0;

	/* Unregister all delegates for the specified object */
	virtual void RemoveAllDelegates(void* UserObject) = 0;

	virtual TSharedPtr<IRemoteSessionChannel> GetChannel(const TCHAR* Type) = 0;

	template<class T>
	TSharedPtr<T> GetChannel()
	{
		TSharedPtr<IRemoteSessionChannel> Channel = GetChannel(T::StaticType());

		if (Channel.IsValid())
		{
			return StaticCastSharedPtr<T>(Channel);
		}

		return TSharedPtr<T>();
	}

	virtual bool OpenChannel(const FRemoteSessionChannelInfo& Info) = 0;

};

class REMOTESESSION_API IRemoteSessionUnmanagedRole : public IRemoteSessionRole
{
public:
	virtual void Tick(float DeltaTime) = 0;
	virtual void Close(const FString& InReason) = 0;
};
