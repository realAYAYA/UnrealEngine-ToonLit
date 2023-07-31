// Copyright Epic Games, Inc. All Rights Reserved.

#include "IConcertSharedSlateModule.h"

class FConcertSharedSlateModule : public IConcertSharedSlateModule
{
public:
	FConcertSharedSlateModule() = default;
	virtual ~FConcertSharedSlateModule() {}

	virtual void StartupModule() override
	{
	}

	virtual void ShutdownModule() override
	{
	}
};


IMPLEMENT_MODULE(FConcertSharedSlateModule, ConcertSharedSlate);

