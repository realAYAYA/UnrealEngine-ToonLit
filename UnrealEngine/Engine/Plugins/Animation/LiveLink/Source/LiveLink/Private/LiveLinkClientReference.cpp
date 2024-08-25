// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientReference.h"
#include "ILiveLinkClient.h"
#include "LiveLinkModule.h"
#include "Features/IModularFeatures.h"

#ifndef WITH_LIVELINK_HUB
#define WITH_LIVELINK_HUB 0
#endif

ILiveLinkClient* FLiveLinkClientReference::GetClient()const
{
#if WITH_LIVELINK_HUB
	// Compiling for LiveLinkHub should return the livelink client we registered as a modular feature.
	return &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
#else
	return FLiveLinkModule::LiveLinkClient_AnyThread;
#endif
}
