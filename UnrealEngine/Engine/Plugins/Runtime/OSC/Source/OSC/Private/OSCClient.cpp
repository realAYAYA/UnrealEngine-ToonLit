// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCClient.h"

#include "Common/UdpSocketBuilder.h"
#include "Sockets.h"

#include "OSCBundle.h"
#include "OSCClientProxy.h"
#include "OSCLog.h"
#include "OSCMessage.h"
#include "OSCPacket.h"
#include "OSCStream.h"


UOSCClient::UOSCClient(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ClientProxy(nullptr)
{
}

void UOSCClient::Connect()
{
	check(!ClientProxy.IsValid());
	ClientProxy.Reset(new FOSCClientProxy(GetName()));
}

bool UOSCClient::IsActive() const
{
	return ClientProxy.IsValid() && ClientProxy->IsActive();
}

void UOSCClient::GetSendIPAddress(FString& InIPAddress, int32& Port)
{
	check(ClientProxy.IsValid());
	ClientProxy->GetSendIPAddress(InIPAddress, Port);
}

bool UOSCClient::SetSendIPAddress(const FString& InIPAddress, const int32 Port)
{
	check(ClientProxy.IsValid());
	return ClientProxy->SetSendIPAddress(InIPAddress, Port);
}

void UOSCClient::Stop()
{
	if (ClientProxy.IsValid())
	{
		ClientProxy->Stop();
	}
}

void UOSCClient::BeginDestroy()
{
	Stop();
	Super::BeginDestroy();
}

void UOSCClient::SendOSCMessage(FOSCMessage& Message)
{
	check(ClientProxy.IsValid());
	ClientProxy->SendMessage(Message);
}

void UOSCClient::SendOSCBundle(FOSCBundle& Bundle)
{
	check(ClientProxy.IsValid());
	ClientProxy->SendBundle(Bundle);
}
