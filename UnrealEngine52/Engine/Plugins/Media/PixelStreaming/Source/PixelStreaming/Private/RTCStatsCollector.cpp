// Copyright Epic Games, Inc. All Rights Reserved.

#include "RTCStatsCollector.h"
#include "PixelStreamingPrivate.h"
#include "PixelStreamingStatNames.h"
#include "Settings.h"
#include "ToStringExtensions.h"

namespace UE::PixelStreaming
{
	//------------------------ FStreamStatsSink------------------------

	FRTCStatsCollector::FStreamStatsSink::FStreamStatsSink(FString InRid)
		: Rid(InRid)
	{
		// basic values to display
		Add(PixelStreamingStatNames::FirCount, 0);
		Add(PixelStreamingStatNames::PliCount, 0);
		Add(PixelStreamingStatNames::NackCount, 0);
		Add(PixelStreamingStatNames::SliCount, 0);
		Add(PixelStreamingStatNames::RetransmittedBytesSent, 0);
		Add(PixelStreamingStatNames::TargetBitrate, 0);
		Add(PixelStreamingStatNames::TotalEncodeBytesTarget, 0);
		Add(PixelStreamingStatNames::KeyFramesEncoded, 0);
		Add(PixelStreamingStatNames::FrameWidth, 0);
		Add(PixelStreamingStatNames::FrameHeight, 0);
		Add(PixelStreamingStatNames::HugeFramesSent, 0);
		Add(PixelStreamingStatNames::AvgSendDelay, 0);

		// these are values used to calculate extra values (stores time deltas etc)
		AddMonitored(PixelStreamingStatNames::FramesSent);
		AddMonitored(PixelStreamingStatNames::FramesReceived);
		AddMonitored(PixelStreamingStatNames::BytesSent);
		AddMonitored(PixelStreamingStatNames::BytesReceived);
		AddMonitored(PixelStreamingStatNames::QPSum);
		AddMonitored(PixelStreamingStatNames::TotalEncodeTime);
		AddMonitored(PixelStreamingStatNames::FramesEncoded);
		AddMonitored(PixelStreamingStatNames::FramesDecoded);

		// below is the logic for calculating each of the stats we want to calculate

		// FrameSent Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* FramesSentStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::FramesSent);
			if (FramesSentStat && FramesSentStat->LatestStat.StatValue > 0)
			{
				const double FramesSentPerSecond = FramesSentStat->CalculateDelta(Period);
				return FStatData(PixelStreamingStatNames::FramesSentPerSecond, FramesSentPerSecond, 0);
			}
			return {};
		});

		// FramesReceived Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* FramesReceivedStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::FramesReceived);
			if (FramesReceivedStat && FramesReceivedStat->LatestStat.StatValue > 0)
			{
				const double FramesReceivedPerSecond = FramesReceivedStat->CalculateDelta(Period);
				return FStatData(PixelStreamingStatNames::FramesReceivedPerSecond, FramesReceivedPerSecond, 0);
			}
			return {};
		});

		// BytesSent Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* BytesSentStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::BytesSent);
			if (BytesSentStat && BytesSentStat->LatestStat.StatValue > 0)
			{
				const double BytesSentPerSecond = BytesSentStat->CalculateDelta(Period);
				const double MegabitsPerSecond = BytesSentPerSecond / 1000.0 * 8.0;
				return FStatData(PixelStreamingStatNames::Bitrate, MegabitsPerSecond, 0);
			}
			return {};
		});

		// BytesReceived Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* BytesReceivedStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::BytesReceived);
			if (BytesReceivedStat && BytesReceivedStat->LatestStat.StatValue > 0)
			{
				const double BytesReceivedPerSecond = BytesReceivedStat->CalculateDelta(Period);
				const double MegabitsPerSecond = BytesReceivedPerSecond / 1000.0 * 8.0;
				return FStatData(PixelStreamingStatNames::Bitrate, MegabitsPerSecond, 0);
			}
			return {};
		});

		// Encoded fps
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* EncodedFramesStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::FramesEncoded);
			if (EncodedFramesStat && EncodedFramesStat->LatestStat.StatValue > 0)
			{
				const double EncodedFramesPerSecond = EncodedFramesStat->CalculateDelta(Period);
				return FStatData(PixelStreamingStatNames::EncodedFramesPerSecond, EncodedFramesPerSecond, 0);
			}
			return {};
		});

		// Decoded fps
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* DecodedFramesStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::FramesDecoded);
			if (DecodedFramesStat && DecodedFramesStat->LatestStat.StatValue > 0)
			{
				const double DecodedFramesPerSecond = DecodedFramesStat->CalculateDelta(Period);
				return FStatData(PixelStreamingStatNames::DecodedFramesPerSecond, DecodedFramesPerSecond, 0);
			}
			return {};
		});

		// Avg QP Per Second
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* QPSumStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::QPSum);
			FStatData* EncodedFramesPerSecond = StatSource.GetCalculatedStat(PixelStreamingStatNames::EncodedFramesPerSecond);
			if (QPSumStat && QPSumStat->LatestStat.StatValue > 0
				&& EncodedFramesPerSecond && EncodedFramesPerSecond->StatValue > 0.0)
			{
				const double QPSumDeltaPerSecond = QPSumStat->CalculateDelta(Period);
				const double MeanQPPerFrame = QPSumDeltaPerSecond / EncodedFramesPerSecond->StatValue;
				FName StatName = PixelStreamingStatNames::MeanQPPerSecond;
				return FStatData(StatName, MeanQPPerFrame, 0);
			}
			return {};
		});

		// Mean EncodeTime (ms) Per Frame
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* TotalEncodeTimeStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::TotalEncodeTime);
			FStatData* EncodedFramesPerSecond = StatSource.GetCalculatedStat(PixelStreamingStatNames::EncodedFramesPerSecond);
			if (TotalEncodeTimeStat && TotalEncodeTimeStat->LatestStat.StatValue > 0
				&& EncodedFramesPerSecond && EncodedFramesPerSecond->StatValue > 0.0)
			{
				const double TotalEncodeTimePerSecond = TotalEncodeTimeStat->CalculateDelta(Period);
				const double MeanEncodeTimePerFrameMs = TotalEncodeTimePerSecond / EncodedFramesPerSecond->StatValue * 1000.0;
				return FStatData(PixelStreamingStatNames::MeanEncodeTime, MeanEncodeTimePerFrameMs, 2);
			}
			return {};
		});

		// Mean SendDelay (ms) Per Frame
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* TotalSendDelayStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::TotalPacketSendDelay);
			FStatData* FramesSentPerSecond = StatSource.GetCalculatedStat(PixelStreamingStatNames::FramesSentPerSecond);
			if (TotalSendDelayStat && TotalSendDelayStat->LatestStat.StatValue > 0
				&& FramesSentPerSecond && FramesSentPerSecond->StatValue > 0.0)
			{
				const double TotalSendDelayPerSecond = TotalSendDelayStat->CalculateDelta(Period);
				const double MeanSendDelayPerFrameMs = TotalSendDelayPerSecond / FramesSentPerSecond->StatValue * 1000.0;
				return FStatData(PixelStreamingStatNames::MeanSendDelay, MeanSendDelayPerFrameMs, 2);
			}
			return {};
		});

		// JitterBufferDelay (ms)
		AddStatCalculator([](FStatsSink& StatSource, double Period) -> TOptional<FStatData> {
			FMonitoredStat* JitterBufferDelayStat = StatSource.GetMonitoredStat(PixelStreamingStatNames::JitterBufferDelay);
			FStatData* FramesReceivedPerSecond = StatSource.GetCalculatedStat(PixelStreamingStatNames::FramesReceivedPerSecond);
			if (JitterBufferDelayStat && JitterBufferDelayStat->LatestStat.StatValue > 0
				&& FramesReceivedPerSecond && FramesReceivedPerSecond->StatValue > 0.0)
			{
				const double TotalJitterBufferDelayPerSecond = JitterBufferDelayStat->CalculateDelta(Period);
				const double MeanJitterBufferDelayMs = TotalJitterBufferDelayPerSecond / FramesReceivedPerSecond->StatValue * 1000.0;
				return FStatData(PixelStreamingStatNames::JitterBufferDelay, MeanJitterBufferDelayMs, 2);
			}
			return {};
		});
	}

	// ------------- FRTCStatsCollector-------------------

	FRTCStatsCollector::FRTCStatsCollector()
		: FRTCStatsCollector(INVALID_PLAYER_ID)
	{
	}

	FRTCStatsCollector::FRTCStatsCollector(FPixelStreamingPlayerId PlayerId)
		: AssociatedPlayerId(PlayerId)
		, LastCalculationCycles(FPlatformTime::Cycles64())
		, bIsEnabled(!Settings::CVarPixelStreamingWebRTCDisableStats.GetValueOnAnyThread())
	{
		FStatsSink TrackStatsSink;
		TrackStatsSink.Add(PixelStreamingStatNames::JitterBufferDelay, 2);
		TrackStatsSink.Add(PixelStreamingStatNames::FramesPerSecond, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::FramesDecoded, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::FramesDropped, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::FramesCorrupted, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::PartialFramesLost, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::FullFramesLost, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::JitterBufferTargetDelay, 2);
		TrackStatsSink.Add(PixelStreamingStatNames::InterruptionCount, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::TotalInterruptionDuration, 2);
		TrackStatsSink.Add(PixelStreamingStatNames::FreezeCount, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::PauseCount, 0);
		TrackStatsSink.Add(PixelStreamingStatNames::TotalFreezesDuration, 2);
		TrackStatsSink.Add(PixelStreamingStatNames::TotalPausesDuration, 2);
		StatSinks.Add("track", TrackStatsSink);

		FStatsSink SourceStatsSink;
		SourceStatsSink.Add(PixelStreamingStatNames::SourceFps, 0);
		StatSinks.Add("media-source", SourceStatsSink);
	}

	void FRTCStatsCollector::AddRef() const
	{
		FPlatformAtomics::InterlockedIncrement(&RefCount);
	}

	rtc::RefCountReleaseStatus FRTCStatsCollector::Release() const
	{
		if (FPlatformAtomics::InterlockedDecrement(&RefCount) == 0)
		{
			return rtc::RefCountReleaseStatus::kDroppedLastRef;
		}

		return rtc::RefCountReleaseStatus::kOtherRefsRemained;
	}

	// returns true if the value is worth storing (false if it started and remains zero)
	bool FRTCStatsCollector::ExtractValueAndSet(const webrtc::RTCStatsMemberInterface* ExtractFrom, FStatData* SetValueHere)
	{
		const bool bZeroInitially = SetValueHere->StatValue == 0.0;
		FString StatValueStr = ExtractFrom->is_defined() ? ToString(ExtractFrom->ValueToString()) : TEXT("0.0");
		double StatValueDouble = FCString::Atod(*StatValueStr);
		SetValueHere->StatValue = StatValueDouble;
		const bool bZeroStill = SetValueHere->StatValue == 0.0;
		return !(bZeroInitially && bZeroStill);
	}

	FRTCStatsCollector::FStatsSink* FRTCStatsCollector::FindSink(const FString& StatsType, const std::vector<const webrtc::RTCStatsMemberInterface*>& StatMembers, FString& OutSsrc)
	{
		FStatsSink* Sink = nullptr;

		if (StatsType == "outbound-rtp" || StatsType == "inbound-rtp")
		{
			// Extract the `ssrc` to uniquely id the stream
			for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
			{
				const FString StatName = FString(StatMember->name());
				if (StatName == "ssrc")
				{
					OutSsrc = StatMember->is_defined() ? ToString(StatMember->ValueToString()) : TEXT("");
					if (!StreamStatsSinks.Contains(OutSsrc))
					{
						Sink = &StreamStatsSinks.Add(OutSsrc, FStreamStatsSink(OutSsrc));
					}
					else
					{
						Sink = StreamStatsSinks.Find(OutSsrc);
					}
					break;
				}
			}
		}
		else
		{
			Sink = StatSinks.Find(StatsType);
		}

		return Sink;
	}

	void FRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& Report)
	{
		FStats* PSStats = FStats::Get();

		if (!bIsEnabled || !PSStats || !Report)
		{
			return;
		}

		for (auto&& Stats : *Report)
		{
			const FString StatsType = FString(Stats.type());
			std::vector<const webrtc::RTCStatsMemberInterface*> StatMembers = Stats.Members();

			// For debugging
			// UE_LOG(LogPixelStreaming, Log, TEXT("------------%s---%s------------"), *FString(Stats.id().c_str()), *FString(Stats.type()));
			// for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
			// {
			// 	UE_LOG(LogPixelStreaming, Log, TEXT("%s"), *FString(StatMember->name()));
			// }

			// Find relevant FStatsSink
			FString Ssrc;

			if (FStatsSink* StatsSink = FindSink(StatsType, StatMembers, Ssrc))
			{
				const bool bSimulcastStat = Ssrc.Contains(TEXT("Simulcast"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
				const FString Id = bSimulcastStat ? Ssrc : AssociatedPlayerId;

				for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
				{
					const FName StatName = FName(StatMember->name());

					// Check if the stat is a stat we use for calculation, if so, extract the value and store it
					if (FMonitoredStat* MonitoredStat = StatsSink->GetMonitoredStat(StatName))
					{
						ExtractValueAndSet(StatMember, &MonitoredStat->LatestStat);
					}
					else if (FStatData* StatToEmit = StatsSink->Get(StatName))
					{
						if (ExtractValueAndSet(StatMember, StatToEmit))
						{
							PSStats->StorePeerStat(Id, *StatToEmit);
						}
					}
				}

				PostDeliverCalculateStats(PSStats);
			}
		}
	}

	void FRTCStatsCollector::PostDeliverCalculateStats(FStats* PSStats)
	{
		uint64 CyclesNow = FPlatformTime::Cycles64();
		double SecondsDelta = FGenericPlatformTime::ToSeconds64(CyclesNow - LastCalculationCycles);

		// Don't calculate stats if 1 second window has not yet elapsed
		if (SecondsDelta < 1.0)
		{
			return;
		}

		// Calculate stats based on each of the stream stats sinks
		for (auto& Entry : StreamStatsSinks)
		{
			const FString& Ssrc = Entry.Key;
			const bool bSimulcastStat = Ssrc.Contains(TEXT("Simulcast"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
			const FString Id = bSimulcastStat ? Ssrc : AssociatedPlayerId;
			FStatsSink& StreamSink = Entry.Value;

			for (auto& Calculator : StreamSink.Calculators)
			{
				TOptional<FStatData> OptStatData = Calculator(StreamSink, SecondsDelta);
				if (OptStatData.IsSet())
				{
					FStatData& StatData = *OptStatData;
					StreamSink.CalculatedStats.Add(StatData.StatName, StatData);
					PSStats->StorePeerStat(Id, StatData);
				}
			}
		}

		LastCalculationCycles = FPlatformTime::Cycles64();
	}
} // namespace UE::PixelStreaming
