// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Trace/Analyzer.h"
#include "Views/TableDashboardViewFactory.h"


namespace UE::Audio::Insights
{
	struct FMixerSourceMessageBase
	{
		FMixerSourceMessageBase() = default;
		FMixerSourceMessageBase(const Trace::IAnalyzer::FOnEventContext& InContext)
		{
			const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;
			DeviceId = static_cast<::Audio::FDeviceId>(EventData.GetValue<uint32>("DeviceId"));
			Timestamp = InContext.EventTime.AsSeconds(EventData.GetValue<uint64>("Timestamp"));
			PlayOrder = EventData.GetValue<uint32>("PlayOrder");
		}

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 PlayOrder = INDEX_NONE;
		uint64 ComponentId = TNumericLimits<uint64>::Max();
		double Timestamp = 0.0;
	};

	using FMixerSourceStopMessage = FMixerSourceMessageBase;

	struct FMixerSourceStartMessage : public FMixerSourceMessageBase
	{
		FMixerSourceStartMessage() = default;
		FMixerSourceStartMessage(const Trace::IAnalyzer::FOnEventContext& InContext)
			: FMixerSourceMessageBase(InContext)
		{
			const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;
			EventData.GetString("Name", Name);
			ComponentId = EventData.GetValue<uint64>("ComponentId");
			SourceId = EventData.GetValue<int32>("SourceId");
		}

		FString Name;
		int32 SourceId = INDEX_NONE;
		uint64 ComponentId = TNumericLimits<uint64>::Max();
	};

#define DEFINE_MIXERSOURCE_PARAM_MESSAGE(ClassName, ParamName, Type, Default)			\
	struct ClassName : public FMixerSourceMessageBase									\
	{																					\
		ClassName() = default;															\
		ClassName(const Trace::IAnalyzer::FOnEventContext& InContext)					\
			: FMixerSourceMessageBase(InContext)										\
		{																				\
			const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;		\
			ParamName = EventData.GetValue<Type>(#ParamName);							\
		}																				\
		Type ParamName = Default;														\
	};

	DEFINE_MIXERSOURCE_PARAM_MESSAGE(FMixerSourceDistanceAttenuationMessage, DistanceAttenuation, float, 0.0f)
	DEFINE_MIXERSOURCE_PARAM_MESSAGE(FMixerSourceEnvelopeMessage, Envelope, float, 0.0f)
	DEFINE_MIXERSOURCE_PARAM_MESSAGE(FMixerSourceHPFFreqMessage, HPFFrequency, float, MIN_FILTER_FREQUENCY)
	DEFINE_MIXERSOURCE_PARAM_MESSAGE(FMixerSourceLPFFreqMessage, LPFFrequency, float, MAX_FILTER_FREQUENCY)
	DEFINE_MIXERSOURCE_PARAM_MESSAGE(FMixerSourcePitchMessage, Pitch, float, 1.0f)
	DEFINE_MIXERSOURCE_PARAM_MESSAGE(FMixerSourceVolumeMessage, Volume, float, 1.0f)
#undef DEFINE_MIXERSOURCE_PARAM_MESSAGE

	class FMixerSourceDashboardEntry : public FSoundAssetDashboardEntry
	{
	public:
		FMixerSourceDashboardEntry() = default;
		virtual ~FMixerSourceDashboardEntry() = default;

		int32 SourceId = INDEX_NONE;

		float Volume = 1.0f;
		float Pitch = 1.0f;
		float LPFFreq = MIN_FILTER_FREQUENCY;
		float HPFFreq = MAX_FILTER_FREQUENCY;
		float Envelope = 0.0f;
		float DistanceAttenuation = 0.0f;
	};

	class FMixerSourceMessages
	{
	public:
		TAnalyzerMessageQueue<FMixerSourceDistanceAttenuationMessage> DistanceAttenuationMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceEnvelopeMessage> EnvelopeMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceHPFFreqMessage> HPFFreqMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceLPFFreqMessage> LPFFreqMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourcePitchMessage> PitchMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceStartMessage> StartMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceStopMessage> StopMessages { 0.1 };
		TAnalyzerMessageQueue<FMixerSourceVolumeMessage> VolumeMessages { 0.1 };
	};
} // namespace UE::Audio::Insights
