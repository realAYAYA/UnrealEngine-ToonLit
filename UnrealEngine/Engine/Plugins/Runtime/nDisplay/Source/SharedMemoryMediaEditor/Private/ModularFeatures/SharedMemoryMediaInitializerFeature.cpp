// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularFeatures/SharedMemoryMediaInitializerFeature.h"

#include "SharedMemoryMediaOutput.h"
#include "SharedMemoryMediaSource.h"


bool FSharedMemoryMediaInitializerFeature::IsMediaSubjectSupported(const UObject* MediaSubject)
{
	if (MediaSubject)
	{
		return MediaSubject->IsA<USharedMemoryMediaSource>() || MediaSubject->IsA<USharedMemoryMediaOutput>();
	}

	return false;
}

static constexpr const TCHAR* GetMediaPrefix(const IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo::EMediaSubjectOwnerType OwnerType)
{
	switch (OwnerType)
	{
	case IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo::EMediaSubjectOwnerType::ICVFXCamera:
		return TEXT("icam");

	case IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo::EMediaSubjectOwnerType::Viewport:
		return TEXT("vp");

	case IDisplayClusterModularFeatureMediaInitializer::FMediaSubjectOwnerInfo::EMediaSubjectOwnerType::Backbuffer:
		return TEXT("node");

	default:
		return TEXT("unknown");
	}
}

void FSharedMemoryMediaInitializerFeature::InitializeMediaSubjectForTile(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo, const FIntPoint& TilePos)
{
	const FString UniqueName = FString::Printf(TEXT("%s@%s_tile_%d:%d"), GetMediaPrefix(OnwerInfo.OwnerType), *OnwerInfo.OwnerName, TilePos.X, TilePos.Y);

	if (USharedMemoryMediaSource* SMMediaSource = Cast<USharedMemoryMediaSource>(MediaSubject))
	{
		SMMediaSource->UniqueName   = UniqueName;
		SMMediaSource->bZeroLatency = true;
		SMMediaSource->Mode         = ESharedMemoryMediaSourceMode::Framelocked;
	}
	else if (USharedMemoryMediaOutput* SMMediaOutput = Cast<USharedMemoryMediaOutput>(MediaSubject))
	{
		SMMediaOutput->UniqueName   = UniqueName;
		SMMediaOutput->bInvertAlpha = true;
		SMMediaOutput->bCrossGpu    = true;
		SMMediaOutput->NumberOfTextureBuffers = 4;
	}
}

void FSharedMemoryMediaInitializerFeature::InitializeMediaSubjectForFullFrame(UObject* MediaSubject, const FMediaSubjectOwnerInfo& OnwerInfo)
{
	const FString UniqueName = FString::Printf(TEXT("%s@%s"), GetMediaPrefix(OnwerInfo.OwnerType), *OnwerInfo.OwnerName);

	if (USharedMemoryMediaSource* SMMediaSource = Cast<USharedMemoryMediaSource>(MediaSubject))
	{
		SMMediaSource->UniqueName   = UniqueName;
		SMMediaSource->bZeroLatency = true;
		SMMediaSource->Mode         = ESharedMemoryMediaSourceMode::Framelocked;
	}
	else if (USharedMemoryMediaOutput* SMMediaOutput = Cast<USharedMemoryMediaOutput>(MediaSubject))
	{
		SMMediaOutput->UniqueName   = UniqueName;
		SMMediaOutput->bInvertAlpha = true;
		SMMediaOutput->bCrossGpu    = true;
		SMMediaOutput->NumberOfTextureBuffers = 4;
	}
}
