// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DirectLinkStreamCommunicationInterface.h"

#include "CoreTypes.h"
#include "IMessageContext.h"



namespace DirectLink
{

struct FStreamDescription
{
	bool bThisIsSource = false;

	FGuid SourcePoint;
	FGuid DestinationPoint;
	FStreamPort LocalStreamPort = 0; // works like an Id within that endpoint
	FMessageAddress RemoteAddress;
	FStreamPort RemoteStreamPort = 0;
	EStreamConnectionState Status = EStreamConnectionState::Uninitialized;

	TUniquePtr<IStreamReceiver> Receiver;
	TSharedPtr<IStreamSender> Sender;
};

} // namespace DirectLink
