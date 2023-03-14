// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCIncludes.h"
#include "PixelStreamingPlayerId.h"
#include "Stats.h"

namespace UE::PixelStreaming
{
	class FRTCStatsCollector : public webrtc::RTCStatsCollectorCallback
	{
	public:
		FRTCStatsCollector();
		FRTCStatsCollector(FPixelStreamingPlayerId PlayerId);

		// Begin RTCStatsCollectorCallback interface
		void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
		void AddRef() const override;
		rtc::RefCountReleaseStatus Release() const override;
		// End RTCStatsCollectorCallback interface

	private:
		void ExtractValueAndSet(const webrtc::RTCStatsMemberInterface* ExtractFrom, FStatData* SetValueHere);
		void CalculateStats(FStats* PSStats);

	private:
		// Some stats have a latest value that is update each time that stats comes and
		// then also a calculated value that might get updates every 1 second.
		// An example is FPS, where total frames sent is the stat that is tracked but
		// the calculated stat is frames sent per second.
		struct FCalculatedStat
		{
			FStatData LatestStat;
			double PrevValue;

			double CalculateDelta(double Period)
			{
				double Delta = (LatestStat.StatValue - PrevValue) * Period;
				PrevValue = LatestStat.StatValue;
				return Delta;
			}
		};

		class FStatsSink
		{
		public:
			virtual ~FStatsSink() = default;
			// Sink only interested in a single type of stat from WebRTC (e.g. track, outbound-rtp).
			virtual bool Contains(const FName& StatName) const { return Stats.Contains(StatName); };
			virtual bool ContainsCalcStat(const FName& StatName) const { return CalcStats.Contains(StatName); }
			virtual void Add(FName StatName, int NDecimalPlaces) { Stats.Add(StatName, FStatData(StatName, 0.0, NDecimalPlaces)); };
			virtual void AddForCalculation(FName StatName) { CalcStats.Add(StatName, { FStatData(StatName, 0.0, 2), 0.0 }); };
			virtual FStatData* Get(const FName& StatName) { return Stats.Find(StatName); };
			virtual FCalculatedStat* GetCalcStat(const FName& StatName) { return CalcStats.Find(StatName); }

		protected:
			TMap<FName, FStatData> Stats;
			TMap<FName, FCalculatedStat> CalcStats;
		};

		class FStreamStatsSink : public FStatsSink
		{
		public:
			FStreamStatsSink(FString InRid);
			virtual ~FStreamStatsSink() = default;

		private:
			// Rid is the unique stream identifier.
			FString Rid;
		};

	private:
		FPixelStreamingPlayerId AssociatedPlayerId;

		mutable int32 RefCount;

		uint64 LastCalculationCycles;

		TMap<FString, FStatsSink> StatSinks;

		bool bIsEnabled;

		// Stream stats are further broken down and stored per RID, as some peers (e.g. an SFU) can have multiple streams.
		TMap<FString, FStatsSink> StreamStatsSinks;
	};
} // namespace UE::PixelStreaming
