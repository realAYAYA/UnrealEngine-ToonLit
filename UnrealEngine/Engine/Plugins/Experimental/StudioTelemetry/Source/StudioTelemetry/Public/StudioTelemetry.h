// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IAnalyticsProvider.h"
#include "Interfaces/IAnalyticsTracer.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class FAnalyticsProviderMulticast;
/**
 * Studio Telemetry Plugin API
 * 
 * Notes:
 * Telemetry for Common Editor and Core Engine is collected automatically.
 * Telemetry Sessions are started and ended automatically with the plugin initialization and shutdown. As such telemetry will not be captured prior to the plugin initialization.
 * Developers are encouraged to add their own telemetry via this API or to intercept the event recording via the supplied callback on the SetRecordEventCallback API below.
 * It is strongly recommended that developers implement their own IAnalyticsProviderModule where custom recording of telemetry events is desired.
 * Custom AnalyticsProviders can be added to the plugin via the .ini. See FAnalyticsProviderLog or FAnalyticsProviderET for example.
 * Telemetry events are recored to all registered IAnalyticsProviders supplied in the .ini file using the FAnalyticsProviderMulticast provider, except where specifically recorded with the RecordEvent(ProviderName,.. ) API below
 */
class FStudioTelemetry : public IModuleInterface
{
public:

	typedef TFunction<void(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attrs)> OnRecordEvent;

	/** Check whether the module is available*/
	static STUDIOTELEMETRY_API bool IsAvailable() { return FModuleManager::Get().IsModuleLoaded("StudioTelemetry"); }

	/** Access to the module singleton*/
	static STUDIOTELEMETRY_API FStudioTelemetry& Get();

	/** Access to the a specific named analytics provider within the system*/
	STUDIOTELEMETRY_API TWeakPtr<IAnalyticsProvider> GetProvider(const FString& ProviderName);

	/** Access to the broadcast analytics provider for the system*/
	STUDIOTELEMETRY_API TWeakPtr<IAnalyticsProvider> GetProvider();

	/** Access to the tracer for the system*/
	STUDIOTELEMETRY_API TWeakPtr<IAnalyticsTracer> GetTracer();
	
	/** Thread safe method to record an event to all registered analytics providers*/
	STUDIOTELEMETRY_API void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes = {});

	/** Thread safe method to record an event to all registered analytics providers*/
	STUDIOTELEMETRY_API void RecordEvent(const FName CategoryName, const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes = {});

	/** Thread safe method to record an event to the specifically named analytics provider */
	STUDIOTELEMETRY_API void RecordEventToProvider(const FString& ProviderName, const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes = {});

	/** Start a new span specifying the parent*/
	STUDIOTELEMETRY_API TSharedPtr<IAnalyticsSpan> StartSpan(const FName Name, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {});

	/** Start a new span specifying the parent*/
	STUDIOTELEMETRY_API TSharedPtr<IAnalyticsSpan> StartSpan(const FName Name, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {});

	/** Start an existing span*/
	STUDIOTELEMETRY_API bool StartSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {});

	/** End an existing span*/
	STUDIOTELEMETRY_API bool EndSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {});

	/** End an existing span by name*/
	STUDIOTELEMETRY_API bool EndSpan(const FName Name, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {});

	/** Get an active span by name, non active spans will not be available*/
	STUDIOTELEMETRY_API TSharedPtr<IAnalyticsSpan> GetSpan(const FName Name);

	/** Callback for interception of telemetry events recording that can be used by Developers to send telemetry events to their own back end, though it is recommended that Developers implement their own IAnalyticsProvider via their own IAnalyticsProviderModule*/
	STUDIOTELEMETRY_API void SetRecordEventCallback(OnRecordEvent);

	class ScopedSpan
	{
	public:
		ScopedSpan(const FName Name, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes = {} )
		{
			if (FStudioTelemetry::Get().IsAvailable())
			{
				Span = FStudioTelemetry::Get().StartSpan(Name, AdditionalAttributes);
			}
		}

		~ScopedSpan()
		{
			if (FStudioTelemetry::Get().IsAvailable())
			{
				FStudioTelemetry::Get().EndSpan(Span);
			}
		}
	private:

		TSharedPtr<IAnalyticsSpan> Span;
	};

private:

	/** IModuleInterface implementation */
	STUDIOTELEMETRY_API virtual void StartupModule()  final;
	STUDIOTELEMETRY_API virtual void ShutdownModule()  final;

	/** Starts a new analytics session*/
	void StartSession();

	/** Ends an existing analytics session*/
	void EndSession();

	FCriticalSection						CriticalSection;
	TSharedPtr<FAnalyticsProviderMulticast>	AnalyticsProvider;
	TSharedPtr<IAnalyticsTracer>			AnalyticsTracer;
	OnRecordEvent							RecordEventCallback;
	FGuid									SessionGUID;
};

#define STUDIO_TELEMETRY_SPAN_SCOPE(Name) FStudioTelemetry::ScopedSpan PREPROCESSOR_JOIN(ScopedSpan, __LINE__)(TEXT(#Name));
#define STUDIO_TELEMETRY_START_SPAN(Name) if (FStudioTelemetry::Get().IsAvailable()) { FStudioTelemetry::Get().StartSpan(TEXT(#Name));}
#define STUDIO_TELEMETRY_END_SPAN(Name) if (FStudioTelemetry::Get().IsAvailable()) { FStudioTelemetry::Get().EndSpan(TEXT(#Name));}
