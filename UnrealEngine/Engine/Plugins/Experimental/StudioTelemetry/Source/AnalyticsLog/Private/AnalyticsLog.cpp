// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsLog.h"
#include "AnalyticsProviderLog.h"

IMPLEMENT_MODULE( FAnalyticsLog, AnalyticsLog );

void FAnalyticsLog::StartupModule()
{
}

void FAnalyticsLog::ShutdownModule()
{
}

TSharedPtr<IAnalyticsProvider> FAnalyticsLog::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		return MakeShared<FAnalyticsProviderLog>(GetConfigValue);
	}

	return nullptr;
}

