// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingStreamerInputFactory.h"

#include <AssetTypeCategories.h>
#include "PixelStreamingStreamerInputBackBuffer.h"

#define LOCTEXT_NAMESPACE "PixelStreaming"

UPixelStreamingStreamerInputBackBufferFactory::UPixelStreamingStreamerInputBackBufferFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;

	SupportedClass = UPixelStreamingStreamerInputBackBuffer::StaticClass();
}

FText UPixelStreamingStreamerInputBackBufferFactory::GetDisplayName() const
{
	return LOCTEXT("InputBackBufferFactoryDisplayName", "Back Buffer Streamer Input");
}

uint32 UPixelStreamingStreamerInputBackBufferFactory::GetMenuCategories() const
{
	return EAssetTypeCategories::Misc;
}

UObject* UPixelStreamingStreamerInputBackBufferFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	if (UPixelStreamingStreamerInputBackBuffer* Resource = NewObject<UPixelStreamingStreamerInputBackBuffer>(InParent, InName, Flags | RF_Transactional))
	{
		return Resource;
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
