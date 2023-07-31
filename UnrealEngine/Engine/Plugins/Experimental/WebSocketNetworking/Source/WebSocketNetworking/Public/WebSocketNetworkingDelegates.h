// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class INetworkingWebSocket;

DECLARE_DELEGATE(FWebSocketInfoCallBack);
DECLARE_DELEGATE_TwoParams(FWebSocketPacketReceivedCallBack, void* /*Data*/, int32 /*Data Size*/);
DECLARE_DELEGATE_OneParam(FWebSocketClientConnectedCallBack, INetworkingWebSocket* /*Socket*/);
						  
