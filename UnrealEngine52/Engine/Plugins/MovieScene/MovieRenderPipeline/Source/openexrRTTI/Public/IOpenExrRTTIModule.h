// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Modules/ModuleInterface.h"
#include "Containers/Map.h"
#include "Misc/StringFormatArg.h"
#include "Containers/UnrealString.h"

THIRD_PARTY_INCLUDES_START
#include "OpenEXR/ImfHeader.h"
THIRD_PARTY_INCLUDES_END


class UEOPENEXRRTTI_API IOpenExrRTTIModule : public IModuleInterface
{
public:
	virtual void AddFileMetadata(const TMap<FString, FStringFormatArg>& InMetadata, Imf::Header& InHeader) = 0;
};
