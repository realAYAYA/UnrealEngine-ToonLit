// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Math/Box.h"

class FNetworkGUID;

// non ModelDef specific debug helpers
namespace NetworkPredictionDebug
{
	NETWORKPREDICTION_API void DrawDebugOutline(FTransform Transform, FBox BoundingBox, FColor Color, float Lifetime);
	NETWORKPREDICTION_API void DrawDebugText3D(const TCHAR* Str, FTransform Transform, FColor, float Lifetime);
	NETWORKPREDICTION_API UObject* FindReplicatedObjectOnPIEServer(UObject* ClientObject);
	NETWORKPREDICTION_API FNetworkGUID FindObjectNetGUID(UObject* Obj);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Misc/NetworkGuid.h"
#endif
