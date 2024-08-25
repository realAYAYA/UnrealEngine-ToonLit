// Copyright Epic Games, Inc. All Rights Reserved.

#include "Analytics/DMXEditorToolAnalyticsProvider.h"

#include "DMXEditorLog.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Timespan.h"


namespace UE::DMX
{
	const FString FDMXEditorToolAnalyticsProvider::DMXEventPrefix = TEXT("Usage.DMX.");

	FDMXEditorToolAnalyticsProvider::FDMXEditorToolAnalyticsProvider(const FName& InToolName)
		: ToolName(InToolName)
	{
		RecordToolStarted();

		FCoreDelegates::OnEnginePreExit.AddRaw(this, &FDMXEditorToolAnalyticsProvider::RecordToolEnded);
	}

	FDMXEditorToolAnalyticsProvider::~FDMXEditorToolAnalyticsProvider()
	{
		RecordToolEnded();

		FCoreDelegates::OnEnginePreExit.RemoveAll(this);
	}

	void FDMXEditorToolAnalyticsProvider::RecordEvent(const FName& Name, const TArray<FAnalyticsEventAttribute>& Attributes)
	{
		if (!FEngineAnalytics::IsAvailable())
		{
			return;
		}

		const FString EventName = DMXEventPrefix + ToolName.ToString() + TEXT(".") + Name.ToString();
		FEngineAnalytics::GetProvider().RecordEvent(EventName, Attributes);
	}

	void FDMXEditorToolAnalyticsProvider::RecordToolStarted()
	{
		if (!FEngineAnalytics::IsAvailable())
		{
			return;
		}

		ToolStartTimestamp = FDateTime::UtcNow();

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add(FAnalyticsEventAttribute(TEXT("ToolName"), ToolName));

		const FString EventName = DMXEventPrefix + ToolName.ToString() + TEXT(".ToolStarted");
		FEngineAnalytics::GetProvider().RecordEvent(EventName, Attributes);
	}

	void FDMXEditorToolAnalyticsProvider::RecordToolEnded()
	{
		if (!FEngineAnalytics::IsAvailable() || bEnded)
		{
			return;
		}

		const FDateTime Now = FDateTime::UtcNow();
		const FTimespan ToolUsageDuration = Now - ToolStartTimestamp;

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Add(FAnalyticsEventAttribute(TEXT("ToolName"), ToolName));
		Attributes.Add(FAnalyticsEventAttribute(TEXT("DurationSeconds"), static_cast<float>(ToolUsageDuration.GetTotalSeconds())));

		const FString EventName = DMXEventPrefix + ToolName.ToString() + TEXT(".ToolEnded");
		FEngineAnalytics::GetProvider().RecordEvent(EventName, Attributes);

		bEnded = true;
	}
}
