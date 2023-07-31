// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/NetDriver.h"
#include "SteamSocketsPackage.h"
#include "SteamSocketsTypes.h"
#include "SteamSocketsNetDriver.generated.h"

class FNetworkNotify;

UCLASS(transient, config=Engine)
class STEAMSOCKETS_API USteamSocketsNetDriver : public UNetDriver
{
	GENERATED_BODY()

public:

	USteamSocketsNetDriver() :
		Socket(nullptr),
		bIsDelayedNetworkAccess(false)
	{
	}

	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UNetDriver Interface.
	virtual void Shutdown() override;
	virtual bool IsAvailable() const override;
	virtual bool InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error) override;
	virtual bool InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error) override;
	virtual bool InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error) override;
	virtual void TickDispatch(float DeltaTime) override;
	virtual void LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits) override;
	virtual void LowLevelDestroy() override;
	virtual class ISocketSubsystem* GetSocketSubsystem() override;
	virtual bool IsNetResourceValid(void) override;
	//~ End UNetDriver Interface

	bool ArePacketHandlersDisabled() const;

protected:
	class FSteamSocket* Socket;
	bool bIsDelayedNetworkAccess;

	void ResetSocketInfo(const class FSteamSocket* RemovedSocket);

	UNetConnection* FindClientConnectionForHandle(SteamSocketHandles SocketHandle);

	void OnConnectionCreated(SteamSocketHandles ListenParentHandle, SteamSocketHandles SocketHandle);
	void OnConnectionUpdated(SteamSocketHandles SocketHandle, int32 NewState);
	void OnConnectionDisconnected(SteamSocketHandles SocketHandle);

	friend class FSteamSocketsSubsystem;
};
