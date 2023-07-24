// Copyright Epic Games, Inc. All Rights Reserved.
#include "VersionInfoHandler.h"

UVersionInfoHandler* UVersionInfoHandler::Get()
{
	return GetMutableDefault <UVersionInfoHandler>();
};
