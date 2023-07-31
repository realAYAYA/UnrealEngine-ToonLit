// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaPlateModule.h"

#include "IMediaAssetsModule.h"
#include "MediaPlate.h"
#include "MediaPlateComponent.h"

DEFINE_LOG_CATEGORY(LogMediaPlate);

UMediaPlayer* FMediaPlateModule::GetMediaPlayer(UObject* Object, UObject*& PlayerProxy)
{
	UMediaPlayer* MediaPlayer = nullptr;

	// Is this a media plate?
	AMediaPlate* MediaPlate = Cast<AMediaPlate>(Object);
	if (MediaPlate != nullptr)
	{
		UMediaPlateComponent* MediaPlateComponent = MediaPlate->MediaPlateComponent;
		if (MediaPlateComponent != nullptr)
		{
			MediaPlayer = MediaPlateComponent->GetMediaPlayer();
			PlayerProxy = MediaPlateComponent;
		}
	}

	return MediaPlayer;
}

void FMediaPlateModule::StartupModule()
{
	IMediaAssetsModule* MediaAssetsModule = FModuleManager::LoadModulePtr<IMediaAssetsModule>("MediaAssets");
	if (MediaAssetsModule != nullptr)
	{
		GetPlayerFromObjectID = MediaAssetsModule->RegisterGetPlayerFromObject(IMediaAssetsModule::FOnGetPlayerFromObject::CreateRaw(this, &FMediaPlateModule::GetMediaPlayer));
	}
}

void FMediaPlateModule::ShutdownModule()
{
	if (GetPlayerFromObjectID != INDEX_NONE)
	{
		IMediaAssetsModule* MediaAssetsModule = FModuleManager::GetModulePtr<IMediaAssetsModule>("MediaAssets");
		if (MediaAssetsModule != nullptr)
		{
			MediaAssetsModule->UnregisterGetPlayerFromObject(GetPlayerFromObjectID);
			GetPlayerFromObjectID = INDEX_NONE;
		}
	}
}

IMPLEMENT_MODULE(FMediaPlateModule, MediaPlate)
