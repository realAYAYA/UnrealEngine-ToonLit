// Copyright Epic Games, Inc. All Rights Reserved.

#include "SessionEditorFilterService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "Templates/SharedPointer.h"
#include "Misc/CoreDelegates.h"
#include "Trace/Trace.h"

FSessionEditorFilterService::FSessionEditorFilterService(TraceServices::FSessionHandle InHandle, TSharedPtr<const TraceServices::IAnalysisSession> InSession) : FBaseSessionFilterService(InHandle, InSession)
{	
}

void FSessionEditorFilterService::OnApplyChannelChanges()
{
	if (FrameEnabledChannels.Num())
	{
		for (const FString& ChannelName : FrameEnabledChannels)
		{
			UE::Trace::ToggleChannel(*ChannelName, true);
		}
		FrameEnabledChannels.Empty();
	}

	if (FrameDisabledChannels.Num())
	{
		for (const FString& ChannelName : FrameDisabledChannels)
		{
			UE::Trace::ToggleChannel(*ChannelName, false);
		}

		FrameDisabledChannels.Empty();
	}
}
