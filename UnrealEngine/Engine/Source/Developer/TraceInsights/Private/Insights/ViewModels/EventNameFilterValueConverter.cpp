// Copyright Epic Games, Inc. All Rights Reserved.

#include "EventNameFilterValueConverter.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "Insights/InsightsManager.h"

#define LOCTEXT_NAMESPACE "Insights::FEventNameFilterValueConverter"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

bool FEventNameFilterValueConverter::Convert(const FString& Input, int64& Output, FText& OutError) const
{
	TSharedPtr<const TraceServices::IAnalysisSession> Session = FInsightsManager::Get()->GetSession();
	if (Session.IsValid() && TraceServices::ReadTimingProfilerProvider(*Session.Get()))
	{
		TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session.Get());

		const TraceServices::ITimingProfilerProvider& TimingProfilerProvider = *TraceServices::ReadTimingProfilerProvider(*Session.Get());

		const TraceServices::ITimingProfilerTimerReader* TimerReader;
		TimingProfilerProvider.ReadTimers([&TimerReader](const TraceServices::ITimingProfilerTimerReader& Out) { TimerReader = &Out; });

		uint32 TimerCount = TimerReader->GetTimerCount();
		for (uint32 TimerIndex = 0; TimerIndex < TimerCount; ++TimerIndex)
		{
			const TraceServices::FTimingProfilerTimer* Timer = TimerReader->GetTimer(TimerIndex);
			if (Timer && Timer->Name)
			{
				if (FCString::Strcmp(Timer->Name, *Input) == 0)
				{
					Output = Timer->Id;
					return true;
				}
			}
		}
	}

	OutError = LOCTEXT("NoTimerFound", "No timer with this name was found!");
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FEventNameFilterValueConverter::GetTooltipText() const
{
	return LOCTEXT("FEventNameFilterValueConverterTooltip", "Enter the exact name of the timer.");
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FEventNameFilterValueConverter::GetHintText() const
{
	// Use the name of a well known event as the hint to show the user what kind of name we are expecting.
	return FText::FromString(TEXT("FEngineLoop"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE