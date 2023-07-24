// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkClientReference.h"
#include "LiveLinkModule.h"

ILiveLinkClient* FLiveLinkClientReference::GetClient()const
{
	return FLiveLinkModule::LiveLinkClient_AnyThread;
}
