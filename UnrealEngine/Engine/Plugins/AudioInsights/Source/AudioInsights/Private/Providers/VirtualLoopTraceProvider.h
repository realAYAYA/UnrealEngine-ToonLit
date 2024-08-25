// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceChannelHandle.h"
#include "AudioInsightsTraceProviderBase.h"
#include "Messages/VirtualLoopTraceMessages.h"


namespace UE::Audio::Insights
{
	class FVirtualLoopTraceProvider
		: public TDeviceDataMapTraceProvider<uint32, TSharedPtr<FVirtualLoopDashboardEntry>>
		, public TSharedFromThis<FVirtualLoopTraceProvider>
	{
	public:
		FVirtualLoopTraceProvider(TSharedRef<FTraceChannelManager> InChannelManager)
			: TDeviceDataMapTraceProvider<uint32, TSharedPtr<FVirtualLoopDashboardEntry>>(GetName_Static())
		{
			Channels.Add(InChannelManager->CreateHandle(TEXT("AudioChannel")));
		}

		virtual ~FVirtualLoopTraceProvider() = default;
		virtual UE::Trace::IAnalyzer* ConstructAnalyzer() override;

		static FName GetName_Static();

	private:
		virtual bool ProcessMessages() override;

		FVirtualLoopMessages TraceMessages;
		TSet<FTraceChannelHandle> Channels;
	};
} // namespace UE::Audio::Insights
