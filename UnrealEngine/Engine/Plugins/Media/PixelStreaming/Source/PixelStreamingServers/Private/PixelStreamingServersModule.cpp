// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingServersModule.h"

FPixelStreamingServersModule& FPixelStreamingServersModule::Get()
{
	return FModuleManager::LoadModuleChecked<FPixelStreamingServersModule>("PixelStreamingServers");
}

void FPixelStreamingServersModule::StartupModule()
{
	// Any startup logic require goes here.
}

int FPixelStreamingServersModule::NextPort()
{
	// Todo: Checking for in-use ports.
	return (4000 + NextGeneratedPort.Increment()) % 65535;
}

IMPLEMENT_MODULE(FPixelStreamingServersModule, PixelStreamingServers)
