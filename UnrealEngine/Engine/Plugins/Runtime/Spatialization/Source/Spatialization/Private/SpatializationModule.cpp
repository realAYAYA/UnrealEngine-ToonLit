// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatializationModule.h"
#include "Features/IModularFeatures.h"


void FSpatializationModule::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(FITDSpatializationPluginFactory::GetModularFeatureName(), &PluginFactory);
}

void FSpatializationModule::ShutdownModule()
{
	// Nothing done here for now.
}

IMPLEMENT_MODULE(FSpatializationModule, Spatialization)