// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareDisplayClusterModule.h"
#include "Module/TextureShareDisplayClusterLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareDisplayCluster
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareDisplayCluster::FTextureShareDisplayCluster()
{
	TextureShareDisplayClusterAPI = MakeUnique<FTextureShareDisplayClusterAPI>();
}

FTextureShareDisplayCluster::~FTextureShareDisplayCluster()
{
	TextureShareDisplayClusterAPI.Reset();
	UE_LOG(LogTextureShareDisplayCluster, Log, TEXT("TextureShareDisplayCluster module has been destroyed"));
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareDisplayCluster::StartupModule()
{
	UE_LOG(LogTextureShareDisplayCluster, Log, TEXT("TextureShareDisplayCluster module startup"));
	if (TextureShareDisplayClusterAPI->StartupModule())
	{
		UE_LOG(LogTextureShareDisplayCluster, Log, TEXT("TextureShareDisplayCluster module has started"));
	}
	else
	{
		UE_LOG(LogTextureShareDisplayCluster, Log, TEXT("TextureShareDisplayCluster module disabled"));
	}
}

void FTextureShareDisplayCluster::ShutdownModule()
{
	UE_LOG(LogTextureShareDisplayCluster, Log, TEXT("TextureShareDisplayCluster module shutdown"));
	TextureShareDisplayClusterAPI->ShutdownModule();
}

ITextureShareDisplayClusterAPI& FTextureShareDisplayCluster::GetTextureShareDisplayClusterAPI()
{
	return *TextureShareDisplayClusterAPI;
}

IMPLEMENT_MODULE(FTextureShareDisplayCluster, TextureShareDisplayCluster);
