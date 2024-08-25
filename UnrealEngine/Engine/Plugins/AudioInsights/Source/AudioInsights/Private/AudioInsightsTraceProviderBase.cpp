// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioInsightsTraceProviderBase.h"


namespace UE::Audio::Insights
{
	FTraceProviderBase::FTraceProviderBase(FName InName)
		: Name(InName)
	{
	}

	FName FTraceProviderBase::GetName() const
	{
		return Name;
	}

	FTraceProviderBase::FTraceAnalyzerBase::FTraceAnalyzerBase(TSharedRef<FTraceProviderBase> InProvider)
		: Provider(InProvider)
	{
	}

	void FTraceProviderBase::FTraceAnalyzerBase::OnAnalysisBegin(const FOnAnalysisContext& Context)
	{
		Provider->Reset();
	}

	bool FTraceProviderBase::FTraceAnalyzerBase::OnEventSuccess(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
	{
		++(Provider->LastMessageId);
		return true;
	}

	bool FTraceProviderBase::FTraceAnalyzerBase::OnEventFailure(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
	{
		++(Provider->LastMessageId);

		const FString ProviderName = Provider->GetName().ToString();
		constexpr bool bEventSucceeded = false;
		ensureAlwaysMsgf(bEventSucceeded, TEXT("'%s' TraceProvider's Analyzer message with RouteId '%u' event not handled"), *ProviderName, RouteId);
		return bEventSucceeded;
	}
} // namespace UE::Audio::Insights
