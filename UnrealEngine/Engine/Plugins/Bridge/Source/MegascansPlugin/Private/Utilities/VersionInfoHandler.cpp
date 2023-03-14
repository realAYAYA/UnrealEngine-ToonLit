// Copyright Epic Games, Inc. All Rights Reserved.
#include "VersionInfoHandler.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"

UVersionInfoHandler* UVersionInfoHandler::Get()
{
	return GetMutableDefault <UVersionInfoHandler>();
};
