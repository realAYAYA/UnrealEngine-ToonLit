// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Thread.h"

class IAnalyticsProvider;
class IAnalyticsProviderET;
struct FAnalyticsEventAttribute;

/**
 * The public interface for the game studio to gather information about internal development metrics.
 */
class FStudioAnalytics : FNoncopyable
{
public:
	static ENGINE_API void SetProvider(TSharedRef<IAnalyticsProviderET> InAnalytics);

	/** Add a list of attributes to the defaults */
	static ENGINE_API void AddDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes);

	/** Add a single attribute to the defaults */
	static ENGINE_API void AddDefaultEventAttribute(const FAnalyticsEventAttribute& Attribute);

	/** Applies the default attributes */
	static ENGINE_API void ApplyDefaultEventAttributes();

	/**
	 * Return the provider instance. Not valid outside of Initialize/Shutdown calls.
	 * Note: must check IsAvailable() first else this code will assert if the provider is not valid.
	 */
	static ENGINE_API IAnalyticsProviderET& GetProvider();

	/** Helper function to determine if the provider is valid. */
	static ENGINE_API bool IsAvailable() { return Analytics.IsValid(); }

	static ENGINE_API double GetAnalyticSeconds();

	static ENGINE_API void Tick(float DeltaSeconds);

	static ENGINE_API void Shutdown();

	/** General report event function. */
	static ENGINE_API void RecordEvent(const FString& EventName);
	static ENGINE_API void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes);

	/** An event for reporting load time that blocks the editor. */
	static ENGINE_API void FireEvent_Loading(const FString& LoadingName, double SecondsSpentLoading, const TArray<FAnalyticsEventAttribute>& Attributes = TArray<FAnalyticsEventAttribute>());

private:
	static void RunTimer_Concurrent();

private:
	static bool bInitialized;
	static ENGINE_API TSharedPtr<IAnalyticsProviderET> Analytics;
	static TArray<FAnalyticsEventAttribute> DefaultAttributes;
	static FThread TimerThread;
	static volatile double TimeEstimation;
};

