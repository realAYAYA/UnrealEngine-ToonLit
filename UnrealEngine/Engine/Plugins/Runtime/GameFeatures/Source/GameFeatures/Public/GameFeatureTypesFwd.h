// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FString;
enum class EGameFeaturePluginProtocol : uint8;

namespace GameFeaturePluginStatePrivate
{
	enum EGameFeaturePluginState : uint8;
}
using EGameFeaturePluginState = GameFeaturePluginStatePrivate::EGameFeaturePluginState;

namespace UE::GameFeatures
{
	GAMEFEATURES_API FString ToString(EGameFeaturePluginState InType);
}

enum class EGameFeatureURLOptions : uint8;
