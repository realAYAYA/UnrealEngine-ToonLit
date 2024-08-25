// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioInsightsTraceChannelHandle.h"
#include "AudioInsightsTraceProviderBase.h"
#include "Messages/MixerSourceTraceMessages.h"
#include "UObject/NameTypes.h"


namespace UE::Audio::Insights
{
	class FMixerSourceTraceProvider
		: public TDeviceDataMapTraceProvider<uint32, TSharedPtr<FMixerSourceDashboardEntry>>
		, public TSharedFromThis<FMixerSourceTraceProvider>
	{
	public:
		FMixerSourceTraceProvider(TSharedRef<FTraceChannelManager> InManager)
			: TDeviceDataMapTraceProvider<uint32, TSharedPtr<FMixerSourceDashboardEntry>>(GetName_Static())
		{
			Channels.Add(InManager->CreateHandle({ TEXT("AudioMixerChannel") }));
		}

		virtual ~FMixerSourceTraceProvider() = default;
		virtual UE::Trace::IAnalyzer* ConstructAnalyzer() override;

		virtual bool ProcessMessages() override;

		static FName GetName_Static();

	private:
		TMap<int32, float> MaxEnvsMap;

		FMixerSourceMessages TraceMessages;

		TSet<FTraceChannelHandle> Channels;
	};
} // namespace UE::Audio::Insights
