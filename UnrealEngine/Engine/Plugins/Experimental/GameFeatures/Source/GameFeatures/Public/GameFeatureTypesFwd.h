// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
