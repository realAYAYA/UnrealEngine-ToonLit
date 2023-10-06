// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCacheStreamerSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCacheStreamerSettings)

	 
UGeometryCacheStreamerSettings::UGeometryCacheStreamerSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("Geometry Cache");

	LookAheadBuffer = 4.0f;
	MaxMemoryAllowed = 4096.0f;
}

