// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnalyticsProviderMulticast.h"
#include "StudioTelemetryLog.h"
#include "Analytics.h"
#include "Interfaces/IAnalyticsProviderModule.h"
#include "AnalyticsProviderConfigurationDelegate.h"
#include "Misc/ConfigCacheIni.h"
#include "HttpModule.h"

const FString TelemetrySection(TEXT("StudioTelemetry"));
static FString ProviderSection;

FString GetAnalyticsProviderConfiguration(const FString& Name, bool)
{
	FString Result;
	GConfig->GetString(*ProviderSection, *Name, Result, GEngineIni);
	return Result;
}

TSharedPtr<FAnalyticsProviderMulticast> FAnalyticsProviderMulticast::CreateAnalyticsProvider()
{
	return MakeShared<FAnalyticsProviderMulticast>();
}

TWeakPtr<IAnalyticsProvider> FAnalyticsProviderMulticast::GetAnalyticsProvider(const FString& Name)
{
	TSharedPtr<IAnalyticsProvider>* ProviderPtr = Providers.Find(Name);
	return ProviderPtr != nullptr ? *ProviderPtr : TSharedPtr<IAnalyticsProvider>();
}

FAnalyticsProviderMulticast::FAnalyticsProviderMulticast()
{
	TArray<FString> SectionNames;
	
	if (GConfig->GetSectionNames(GEngineIni, SectionNames))
	{
		for (const FString& SectionName : SectionNames)
		{
			if (SectionName.Find(TelemetrySection) != INDEX_NONE)
			{
				ProviderSection = SectionName;

				FString UsageType;

				// Validate the usage type is for this build type
				if (GConfig->GetString(*ProviderSection, TEXT("UsageType"), UsageType, GEngineIni))
				{
#if WITH_EDITOR
					// Must specify a Editor usage type for this type build
					if (UsageType.Find(TEXT("Editor")) == INDEX_NONE)
					{
						continue;
					}
#elif WITH_SERVER_CODE
					// Must specify a Server usage type for this type build
					if (UsageType.Find(TEXT("Server")) == INDEX_NONE)
					{
						continue;
					}
#else
					// Must specify a Client or Program usage type for this type build
					if (UsageType.Find(TEXT("Program")) == INDEX_NONE && UsageType.Find(TEXT("Client")) == INDEX_NONE)
					{
						continue;
					}
#endif
				}
				else
				{
					// Must always specify a usage type
					UE_LOG(LogStudioTelemetry, Error, TEXT("There must be a valid UsageType specified for analytics provider %s"), *ProviderSection);
					continue;
				}

				FString ProviderModuleName;

				if (GConfig->GetString(*ProviderSection, TEXT("ProviderModule"), ProviderModuleName, GEngineIni))
				{
					TSharedPtr<IAnalyticsProvider> AnalyticsProvider;

					FString Name = GetAnalyticsProviderConfiguration("Name", true);

					if ( Name.IsEmpty() )
					{ 
						UE_LOG(LogStudioTelemetry, Error, TEXT("There must be a valid Name specified for analytics provider %s."), *ProviderSection);
						continue;
					}
					else if (Providers.Find(Name) )
					{
						UE_LOG(LogStudioTelemetry, Warning, TEXT("An analytics provider with name %s already exists."), *Name);
						continue;
					}

					// Try to create the analytics provider
					AnalyticsProvider = FAnalytics::Get().CreateAnalyticsProvider(FName(ProviderModuleName), FAnalyticsProviderConfigurationDelegate::CreateStatic(&GetAnalyticsProviderConfiguration));
	
					if (AnalyticsProvider.IsValid())
					{
						UE_LOG(LogStudioTelemetry, Display, TEXT("Created an analytics provider %s from module %s configuration %s [%s]"), *Name, *ProviderModuleName, *GEngineIni, *ProviderSection);
						Providers.Add(Name, AnalyticsProvider);
					}
					else
					{
						UE_LOG(LogStudioTelemetry, Warning, TEXT("Unable to create an analytics provider %s from module %s configuration %s [%s]"), *Name, *ProviderModuleName, *GEngineIni, *ProviderSection);
					}
				}
				else
				{
					UE_LOG(LogStudioTelemetry, Error, TEXT("There must be a valid ProviderModule specified for analytics provider %s"), *ProviderSection);
				}
			}
		}
	}
}

bool FAnalyticsProviderMulticast::SetSessionID(const FString& InSessionID)
{
	SessionID = InSessionID;

	bool bResult = true;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		bResult &= (*it).Value->SetSessionID(InSessionID);
	}

	return bResult;
}

FString FAnalyticsProviderMulticast::GetSessionID() const
{
	return SessionID;
}

void FAnalyticsProviderMulticast::SetUserID(const FString& InUserID)
{
	UserID = InUserID;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetUserID(InUserID);
	}
}

FString FAnalyticsProviderMulticast::GetUserID() const
{
	return UserID;
}

void FAnalyticsProviderMulticast::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	DefaultEventAttributes = Attributes;

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->SetDefaultEventAttributes(CopyTemp(DefaultEventAttributes));
	}
}

TArray<FAnalyticsEventAttribute> FAnalyticsProviderMulticast::GetDefaultEventAttributesSafe() const
{
	return DefaultEventAttributes;
}

int32 FAnalyticsProviderMulticast::GetDefaultEventAttributeCount() const
{
	return DefaultEventAttributes.Num();
}

FAnalyticsEventAttribute FAnalyticsProviderMulticast::GetDefaultEventAttribute(int AttributeIndex) const
{
	return DefaultEventAttributes[AttributeIndex];
}

bool FAnalyticsProviderMulticast::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	bool bResult = true;

	for (TProviders::TConstIterator it(Providers);it;++it)
	{
		bResult &= (*it).Value->StartSession(Attributes);
	}
	return bResult;
}

void FAnalyticsProviderMulticast::EndSession()
{
	for (TProviders::TConstIterator it(Providers);it;++it)
	{
		TSharedPtr<IAnalyticsProvider> Provider = (*it).Value;

		Provider->EndSession();
		Provider.Reset();
	}

	Providers.Reset();
}

void FAnalyticsProviderMulticast::FlushEvents()
{
	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->FlushEvents();
	}
}

void FAnalyticsProviderMulticast::SetRecordEventCallback(OnRecordEvent Callback)
{
	RecordEventCallback = Callback;
}

void FAnalyticsProviderMulticast::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	// Expose events that have duplicate aatibute names. This is is not handled by the analytics backends in any reliable way.
	for (int32 index0 = 0; index0 < Attributes.Num(); ++index0)
	{
		for (int32 index1 = index0 + 1; index1 < Attributes.Num(); ++index1)
		{
			checkf(Attributes[index0].GetName() != Attributes[index1].GetName(), TEXT("Duplicate Attributes Found For Event %s %s==%s"), *EventName, *Attributes[index0].GetName(), *Attributes[index1].GetName());
		}
	}
#endif

	for (TProviders::TConstIterator it(Providers); it; ++it)
	{
		(*it).Value->RecordEvent(EventName, Attributes);
	}

	if (RecordEventCallback)
	{
		// Notify any callbacks
		RecordEventCallback(EventName, Attributes);
	}
}
