// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

using FPixelStreamingPlayerId = FString;

PIXELSTREAMING_API inline FPixelStreamingPlayerId ToPlayerId(FString PlayerIdString)
{
	return FPixelStreamingPlayerId(PlayerIdString);
}

PIXELSTREAMING_API inline FPixelStreamingPlayerId ToPlayerId(int32 PlayerIdInteger)
{
	return FString::FromInt(PlayerIdInteger);
}

PIXELSTREAMING_API inline int32 PlayerIdToInt(FPixelStreamingPlayerId PlayerId)
{
	return FCString::Atoi(*PlayerId);
}

static const FPixelStreamingPlayerId INVALID_PLAYER_ID = ToPlayerId(FString(TEXT("Invalid Player Id")));
static const FPixelStreamingPlayerId SFU_PLAYER_ID = FString(TEXT("1"));