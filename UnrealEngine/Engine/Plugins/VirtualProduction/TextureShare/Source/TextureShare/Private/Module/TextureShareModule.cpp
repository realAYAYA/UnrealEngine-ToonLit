// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/TextureShareModule.h"
#include "Module/TextureShareAPI.h"
#include "Module/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// IModuleInterface
//////////////////////////////////////////////////////////////////////////////////////////////
void FTextureShareModule::StartupModule()
{
#if WITH_EDITOR
	RegisterSettings_Editor();
#endif

	TextureShareAPI = MakeUnique<FTextureShareAPI>();

	UE_LOG(LogTextureShare, Log, TEXT("TextureShare module has started"));
}

void FTextureShareModule::ShutdownModule()
{
#if WITH_EDITOR
	if (UObjectInitialized())
	{
		UnregisterSettings_Editor();
	}
#endif

	TextureShareAPI.Reset();

	UE_LOG(LogTextureShare, Log, TEXT("TextureShare module shutdown"));
}

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareModule
//////////////////////////////////////////////////////////////////////////////////////////////
FTextureShareModule::FTextureShareModule()
{
	UE_LOG(LogTextureShare, Log, TEXT("TextureShare module has been instantiated"));
}

FTextureShareModule::~FTextureShareModule()
{
	UE_LOG(LogTextureShare, Log, TEXT("TextureShare module has been destroyed"));
}

ITextureShareAPI& FTextureShareModule::GetTextureShareAPI()
{
	check(TextureShareAPI.IsValid());

	return *TextureShareAPI.Get();
}

IMPLEMENT_MODULE(FTextureShareModule, TextureShare);
