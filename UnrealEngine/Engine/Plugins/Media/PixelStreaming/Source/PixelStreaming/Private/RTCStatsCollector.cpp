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

		AddForCalculation(PixelStreamingStatNames::FramesSent);
		AddForCalculation(PixelStreamingStatNames::BytesSent);
		AddForCalculation(PixelStreamingStatNames::QPSum);
		AddForCalculation(PixelStreamingStatNames::TotalEncodeTime);
		AddForCalculation(PixelStreamingStatNames::FramesEncoded);
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
		TrackStatsSink.Add(PixelStreamingStatNames::FramesReceived, 0);
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

	void FRTCStatsCollector::ExtractValueAndSet(const webrtc::RTCStatsMemberInterface* ExtractFrom, FStatData* SetValueHere)
	{
		FString StatValueStr = ExtractFrom->is_defined() ? ToString(ExtractFrom->ValueToString()) : TEXT("0.0");
		double StatValueDouble = FCString::Atod(*StatValueStr);
		SetValueHere->StatValue = StatValueDouble;
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
			FString StatsType = FString(Stats.type());
			std::vector<const webrtc::RTCStatsMemberInterface*> StatMembers = Stats.Members();

			// For debugging
			// UE_LOG(LogPixelStreaming, Log, TEXT("------------%s---%s------------"), *FString(Stats.id().c_str()), *FString(Stats.type()));
			// for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
			// {
			// 	UE_LOG(LogPixelStreaming, Log, TEXT("%s"), *FString(StatMember->name()));
			// }

			// Find relevant FStatsSink
			FStatsSink* StatsSink = nullptr;
			FString Rid;

			if (StatsType == "outbound-rtp")
			{
				// Extract the `rid`
				for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
				{
					FString StatName = FString(StatMember->name());
					if (StatName == "rid")
					{

						Rid = StatMember->is_defined() ? ToString(StatMember->ValueToString()) : TEXT("");
						if (!StreamStatsSinks.Contains(Rid))
						{
							StreamStatsSinks.Add(Rid, FStreamStatsSink(Rid));
						}
						StatsSink = StreamStatsSinks.Find(Rid);
						break;
					}
				}
			}
			else
			{
				StatsSink = StatSinks.Find(StatsType);
			}

			if (!StatsSink)
			{
				continue;
			}

			bool bSimulcastStat = Rid.Contains(TEXT("Simulcast"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
			FString Id = bSimulcastStat ? Rid : AssociatedPlayerId;

			for (const webrtc::RTCStatsMemberInterface* StatMember : StatMembers)
			{
				FName StatName = FName(StatMember->name());

				// Check if the stat is a stat we use for calculation, if so, extract the value and store it
				FCalculatedStat* CalcStat = StatsSink->GetCalcStat(StatName);
				if (CalcStat)
				{
					ExtractValueAndSet(StatMember, &CalcStat->LatestStat);
					continue;
				}

				// Check if stat is a stat we emit immediately, if so, extract the value and emit it
				FStatData* StatToEmit = StatsSink->Get(StatName);
				if (StatToEmit)
				{
					const bool bZeroInitially = StatToEmit->StatValue == 0.0;
					ExtractValueAndSet(StatMember, StatToEmit);
					const bool bZeroStill = StatToEmit->StatValue == 0.0;

					// Don't bother emitting a zero value
					if (!bZeroInitially && !bZeroStill)
					{
						PSStats->StorePeerStat(Id, *StatToEmit);
					}

					continue;
				}
			}

			CalculateStats(PSStats);
		}
	}

	void FRTCStatsCollector::CalculateStats(FStats* PSStats)
	{
		uint64 CyclesNow = FPlatformTime::Cycles64();
		double SecondsDelta = FGenericPlatformTime::ToSeconds64(CyclesNow - LastCalculationCycles);

		// Don't calculate stats if 1 second window has not yet elapsed
		if (SecondsDelta < 1.0)
		{
			return;
		}

		// This will bring the calculations into a 1 second window
		double PerSecondRatio = 1.0 / SecondsDelta;
		double FramesSentPerSecond = 0.0;
		double EncodedFramesPerSecond = 0.0;

		// Calculate stats based on each of the stream stats sinks

		for (auto& Entry : StreamStatsSinks)
		{
			FString& Rid = Entry.Key;
			FStatsSink& StreamSink = Entry.Value;
			bool bSimulcastStat = Rid.Contains(TEXT("Simulcast"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
			FString Id = bSimulcastStat ? Rid : AssociatedPlayerId;

			// --------- FrameSent Per Second -----------
			FCalculatedStat* FramesSentStat = StreamSink.GetCalcStat(PixelStreamingStatNames::FramesSent);
			if (FramesSentStat && FramesSentStat->LatestStat.StatValue > 0)
			{
				FramesSentPerSecond = FramesSentStat->CalculateDelta(PerSecondRatio);
				PSStats->StorePeerStat(Id, FStatData(PixelStreamingStatNames::FramesSentPerSecond, FramesSentPerSecond, 0));
			}

			// --------- BytesSent Per Second -----------
			FCalculatedStat* BytesSentStat = StreamSink.GetCalcStat(PixelStreamingStatNames::BytesSent);
			if (BytesSentStat && BytesSentStat->LatestStat.StatValue > 0)
			{
				double BytesSentPerSecond = BytesSentStat->CalculateDelta(PerSecondRatio);
				double MegabitsPerSecond = BytesSentPerSecond / 1000.0 * 8.0;
				FName StatName = PixelStreamingStatNames::Bitrate;
				PSStats->StorePeerStat(Id, FStatData(StatName, MegabitsPerSecond, 0));
			}

			// ------------- Encoded fps -------------
			FCalculatedStat* EncodedFramesStat = StreamSink.GetCalcStat(PixelStreamingStatNames::FramesEncoded);
			if (EncodedFramesStat && EncodedFramesStat->LatestStat.StatValue > 0)
			{
				EncodedFramesPerSecond = EncodedFramesStat->CalculateDelta(PerSecondRatio);
				FName StatName = PixelStreamingStatNames::EncodedFramesPerSecond;
				PSStats->StorePeerStat(Id, FStatData(StatName, EncodedFramesPerSecond, 0));
			}

			// ------------- Avg QP Per Second -------------
			FCalculatedStat* QPSumStat = StreamSink.GetCalcStat(PixelStreamingStatNames::QPSum);
			if (QPSumStat && QPSumStat->LatestStat.StatValue > 0 && EncodedFramesPerSecond > 0.0)
			{
				double QPSumDeltaPerSecond = QPSumStat->CalculateDelta(PerSecondRatio);
				double MeanQPPerFrame = QPSumDeltaPerSecond / EncodedFramesPerSecond;
				FName StatName = PixelStreamingStatNames::MeanQPPerSecond;
				PSStats->StorePeerStat(Id, FStatData(StatName, MeanQPPerFrame, 0));
			}

			// ------------- Mean EncodeTime (ms) Per Frame -------------
			FCalculatedStat* TotalEncodeTimeStat = StreamSink.GetCalcStat(PixelStreamingStatNames::TotalEncodeTime);
			if (TotalEncodeTimeStat && TotalEncodeTimeStat->LatestStat.StatValue > 0 && EncodedFramesPerSecond > 0.0)
			{
				double TotalEncodeTimePerSecond = TotalEncodeTimeStat->CalculateDelta(PerSecondRatio);
				double MeanEncodeTimePerFrameMs = TotalEncodeTimePerSecond / EncodedFramesPerSecond * 1000.0;
				FName StatName = PixelStreamingStatNames::MeanEncodeTime;
				PSStats->StorePeerStat(Id, FStatData(StatName, MeanEncodeTimePerFrameMs, 2));
			}

			// ------------- Mean SendDelay (ms) Per Frame -------------
			FCalculatedStat* TotalSendDelayStat = StreamSink.GetCalcStat(PixelStreamingStatNames::TotalPacketSendDelay);
			if (TotalSendDelayStat && TotalSendDelayStat->LatestStat.StatValue > 0 && FramesSentPerSecond > 0)
			{
				double TotalSendDelayPerSecond = TotalSendDelayStat->CalculateDelta(PerSecondRatio);
				double MeanSendDelayPerFrameMs = TotalSendDelayPerSecond / FramesSentPerSecond * 1000.0;
				FName StatName = PixelStreamingStatNames::MeanSendDelay;
				PSStats->StorePeerStat(Id, FStatData(StatName, MeanSendDelayPerFrameMs, 2));
			}
		}

		LastCalculationCycles = FPlatformTime::Cycles64();
	}
} // namespace UE::PixelStreaming
