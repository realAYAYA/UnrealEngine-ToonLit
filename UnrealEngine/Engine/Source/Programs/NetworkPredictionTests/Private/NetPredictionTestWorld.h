// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"

class UNetworkPredictionWorldManager;
class UNetworkPredictionSettingsObject;

namespace UE::Net::Private
{

struct FNetPredictionTestWorld
{
public:
	FNetPredictionTestWorld(UNetworkPredictionSettingsObject* Settings, ENetMode InNetMode);

	void PreTick(int32 FixedFrameRate);
	void Tick(int32 FixedFrameRate);

	UNetworkPredictionWorldManager* WorldManager = nullptr;

private:
	ENetMode NetMode;
};

}