// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsHorde.h"
#include "AnalyticsET.h"
#include "IAnalyticsProviderET.h"
#include "Horde.h"

IMPLEMENT_MODULE( FAnalyticsHorde, AnalyticsHorde );

void FAnalyticsHorde::StartupModule()
{
}

void FAnalyticsHorde::ShutdownModule()
{
}

TSharedPtr<IAnalyticsProvider> FAnalyticsHorde::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	if (GetConfigValue.IsBound())
	{
		TSharedPtr<IAnalyticsProviderET> AnalyicsProviderET = FAnalyticsET::Get().CreateAnalyticsProviderET(GetConfigValue);

		// Check if we have specifcied a Horde URL in the environment. This allows jobs running on Horde to send telemetry directly to the server they were run on
		const FString HordeServerURL = FHorde::GetServerURL();

		if (AnalyicsProviderET.IsValid() && !HordeServerURL.IsEmpty())
		{
			TArray<FString> AltDomains;
			AnalyicsProviderET->SetURLEndpoint(HordeServerURL, AltDomains);
		}
		
		return AnalyicsProviderET;
	}

	return nullptr;
}
