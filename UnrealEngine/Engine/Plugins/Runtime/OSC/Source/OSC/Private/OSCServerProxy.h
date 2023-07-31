// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Common/UdpSocketReceiver.h"
#include "HAL/ThreadSafeBool.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Tickable.h"
#include "UObject/Object.h"

#include "OSCServer.h"


class OSC_API FOSCServerProxy : public IOSCServerProxy, public FTickableGameObject
{
public:
	FOSCServerProxy(UOSCServer& InServer);
	virtual ~FOSCServerProxy();

	// Begin IOSCServerProxy interface
	bool GetMulticastLoopback() const override;
	FString GetIpAddress() const override;
	int32 GetPort() const override;
	bool IsActive() const override;

	void Listen(const FString& InServerName) override;

	bool SetAddress(const FString& InReceiveIPAddress, int32 InPort) override;
	void SetMulticastLoopback(bool bInMulticastLoopback) override;

	void Stop() override;

	void AddClientToAllowList(const FString& InIPAddress) override;
	void RemoveClientFromAllowList(const FString& InIPAddress) override;
	void ClearClientAllowList() override;
	TSet<FString> GetClientAllowList() const override;

	void SetFilterClientsByAllowList(bool bInEnabled) override;

#if WITH_EDITOR
	void SetTickableInEditor(bool bInTickInEditor) override;
#endif // WITH_EDITOR
	// End IOSCServerProxy interface

	/** Callback that receives data from a socket. */
	void OnPacketReceived(const FArrayReaderPtr& InData, const FIPv4Endpoint& InEndpoint);

	// Begin FTickableGameObject interface
	virtual void Tick(float InDeltaTime) override;
	virtual bool IsTickable() const override { return IsActive(); }
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;

#if WITH_EDITOR
	virtual bool IsTickableInEditor() const override;
#endif // WITH_EDITOR
	// End FTickableGameObject interface

	/** Parent server UObject */
	UOSCServer* Server;

	/** Socket used to listen for OSC packets. */
	FSocket* Socket;

	/** UDP receiver. */
	FUdpSocketReceiver* SocketReceiver;

	/** Only packets from this list of client addresses will be processed if bFilterClientsByAllowList is true. */
	TSet<uint32> ClientAllowList;

	/** IPAddress to listen for OSC packets on.  If unset, defaults to LocalHost */
	FIPv4Address ReceiveIPAddress;

	/** Port to listen for OSC packets on. */
	int32 Port;

	/** Whether or not to loopback if address provided is multicast */
	bool bMulticastLoopback;

	/** Whether or not to use client allow list */
	bool bFilterClientsByAllowList;

#if WITH_EDITOR
	bool bTickInEditor;
#endif // WITH_EDITOR

};
