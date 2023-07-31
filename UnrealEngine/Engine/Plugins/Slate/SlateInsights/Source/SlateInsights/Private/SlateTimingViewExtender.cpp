// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateTimingViewExtender.h"
#include "Insights/ITimingViewSession.h"
#include "UObject/WeakObjectPtr.h"
#include "SlateTimingViewSession.h"

#define LOCTEXT_NAMESPACE "SlateTimingViewExtender"

namespace UE
{
namespace SlateInsights
{

void FSlateTimingViewExtender::OnBeginSession(Insights::ITimingViewSession& InSession)
{
	FPerSessionData* PerSessionData = PerSessionDataMap.Find(&InSession);
	if(PerSessionData == nullptr)
	{
		PerSessionData = &PerSessionDataMap.Add(&InSession);
		PerSessionData->SharedData = MakeUnique<FSlateTimingViewSession>();

		PerSessionData->SharedData->OnBeginSession(InSession);
	}
	else
	{
		PerSessionData->SharedData->OnBeginSession(InSession);
	}
}

void FSlateTimingViewExtender::OnEndSession(Insights::ITimingViewSession& InSession)
{
	FPerSessionData* PerSessionData = PerSessionDataMap.Find(&InSession);
	if(PerSessionData != nullptr)
	{
		PerSessionData->SharedData->OnEndSession(InSession);
	}

	PerSessionDataMap.Remove(&InSession);
}

void FSlateTimingViewExtender::Tick(Insights::ITimingViewSession& InSession, const TraceServices::IAnalysisSession& InAnalysisSession)
{
	FPerSessionData* PerSessionData = PerSessionDataMap.Find(&InSession);
	if(PerSessionData != nullptr)
	{
		PerSessionData->SharedData->Tick(InSession, InAnalysisSession);
	}
}

void FSlateTimingViewExtender::ExtendFilterMenu(Insights::ITimingViewSession& InSession, FMenuBuilder& InMenuBuilder)
{
	FPerSessionData* PerSessionData = PerSessionDataMap.Find(&InSession);
	if (PerSessionData != nullptr)
	{
		PerSessionData->SharedData->ExtendFilterMenu(InMenuBuilder);
	}
}

} //namespace SlateInsights
} //namespace UE

#undef LOCTEXT_NAMESPACE
