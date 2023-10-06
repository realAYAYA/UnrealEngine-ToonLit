// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class FLocalizableMessageProcessor;

class LOCALIZABLEMESSAGE_API ILocalizableMessageModule : public IModuleInterface
{
public:
	static ILocalizableMessageModule& Get();

	virtual FLocalizableMessageProcessor& GetLocalizableMessageProcessor() = 0;
};
