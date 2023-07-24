// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"


#define OSS_PLATFORM_NAME_PS4		TEXT("PSN")
#define OSS_PLATFORM_NAME_XBOX		TEXT("XBL")
#define OSS_PLATFORM_NAME_WINDOWS	TEXT("WIN")
#define OSS_PLATFORM_NAME_MAC		TEXT("MAC")
#define OSS_PLATFORM_NAME_LINUX		TEXT("LNX")
#define OSS_PLATFORM_NAME_IOS		TEXT("IOS")
#define OSS_PLATFORM_NAME_ANDROID	TEXT("AND")
#define OSS_PLATFORM_NAME_SWITCH	TEXT("SWT")
#define OSS_PLATFORM_NAME_OTHER		TEXT("OTHER")



#ifndef NULL_SUBSYSTEM
#define NULL_SUBSYSTEM FName(TEXT("NULL"))
#endif

#ifndef GOOGLEPLAY_SUBSYSTEM
#define GOOGLEPLAY_SUBSYSTEM FName(TEXT("GOOGLEPLAY"))
#endif

#ifndef IOS_SUBSYSTEM
#define IOS_SUBSYSTEM FName(TEXT("IOS"))
#endif

#ifndef APPLE_SUBSYSTEM
#define APPLE_SUBSYSTEM FName(TEXT("APPLE"))
#endif

#ifndef AMAZON_SUBSYSTEM
#define AMAZON_SUBSYSTEM FName(TEXT("AMAZON"))
#endif

#ifndef GOOGLE_SUBSYSTEM
#define GOOGLE_SUBSYSTEM FName(TEXT("GOOGLE"))
#endif

#ifndef FACEBOOK_SUBSYSTEM
#define FACEBOOK_SUBSYSTEM FName(TEXT("FACEBOOK"))
#endif

#ifndef GAMECIRCLE_SUBSYSTEM
#define GAMECIRCLE_SUBSYSTEM FName(TEXT("GAMECIRCLE"))
#endif

#ifndef STEAM_SUBSYSTEM
#define STEAM_SUBSYSTEM FName(TEXT("STEAM"))
#endif

#ifndef PS4_SUBSYSTEM
#define PS4_SUBSYSTEM FName(TEXT("PS4"))
#endif

#ifndef PS4SERVER_SUBSYSTEM
#define PS4SERVER_SUBSYSTEM FName(TEXT("PS4SERVER"))
#endif

#ifndef THUNDERHEAD_SUBSYSTEM
#define THUNDERHEAD_SUBSYSTEM FName(TEXT("THUNDERHEAD"))
#endif

#ifndef MCP_SUBSYSTEM
#define MCP_SUBSYSTEM FName(TEXT("MCP"))
#endif

#ifndef MCP_SUBSYSTEM_EMBEDDED
#define MCP_SUBSYSTEM_EMBEDDED FName(TEXT("MCP:EMBEDDED"))
#endif

#ifndef TENCENT_SUBSYSTEM
#define TENCENT_SUBSYSTEM FName(TEXT("TENCENT"))
#endif

UE_DEPRECATED(4.27, "OnlineSubsystemWeChat has been deprecated and will be removed") ONLINESUBSYSTEM_API extern FName WECHAT_SUBSYSTEM;
UE_DEPRECATED(4.27, "OnlineSubsystemLive has been deprecated and will be removed") ONLINESUBSYSTEM_API extern FName LIVE_SUBSYSTEM;
UE_DEPRECATED(4.27, "OnlineSubsystemLiveServer has been deprecated and will be removed") ONLINESUBSYSTEM_API extern FName LIVESERVER_SUBSYSTEM;

#ifndef SWITCH_SUBSYSTEM
#define SWITCH_SUBSYSTEM FName(TEXT("SWITCH"))
#endif

UE_DEPRECATED(5.2, "OnlineSubsystemOculus has been deprecated and will be removed") ONLINESUBSYSTEM_API extern FName OCULUS_SUBSYSTEM;

#ifndef SAMSUNG_SUBSYSTEM
#define SAMSUNG_SUBSYSTEM FName(TEXT("SAMSUNG"))
#endif

#ifndef QUAIL_SUBSYSTEM
#define QUAIL_SUBSYSTEM FName(TEXT("Quail"))
#endif

#ifndef EOS_SUBSYSTEM
#define EOS_SUBSYSTEM FName(TEXT("EOS"))
#endif

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
