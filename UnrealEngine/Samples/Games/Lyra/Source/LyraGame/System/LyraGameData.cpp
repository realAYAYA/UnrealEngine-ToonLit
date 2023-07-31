// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraGameData.h"
#include "LyraAssetManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraGameData)

ULyraGameData::ULyraGameData()
{
}

const ULyraGameData& ULyraGameData::ULyraGameData::Get()
{
	return ULyraAssetManager::Get().GetGameData();
}
