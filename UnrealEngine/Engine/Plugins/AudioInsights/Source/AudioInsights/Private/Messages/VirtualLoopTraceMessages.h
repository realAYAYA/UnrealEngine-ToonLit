// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "Math/NumericLimits.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Trace/Analyzer.h"
#include "Views/TableDashboardViewFactory.h"


namespace UE::Audio::Insights
{
	struct FVirtualLoopMessageBase
	{
		FVirtualLoopMessageBase() = default;
		FVirtualLoopMessageBase(const Trace::IAnalyzer::FOnEventContext& InContext)
		{
			const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;
			DeviceId = static_cast<::Audio::FDeviceId>(EventData.GetValue<uint32>("DeviceId"));
			Timestamp = InContext.EventTime.AsSeconds(EventData.GetValue<uint64>("Timestamp"));
			PlayOrder = EventData.GetValue<uint32>("PlayOrder");
		}

		::Audio::FDeviceId DeviceId = INDEX_NONE;
		uint32 PlayOrder = INDEX_NONE;
		double Timestamp = 0.0;
	};

	using FVirtualLoopRealizeMessage = FVirtualLoopMessageBase;
	using FVirtualLoopStopMessage = FVirtualLoopMessageBase;

	struct FVirtualLoopVirtualizeMessage : public FVirtualLoopMessageBase
	{
		FVirtualLoopVirtualizeMessage() = default;
		FVirtualLoopVirtualizeMessage(const Trace::IAnalyzer::FOnEventContext& InContext)
			: FVirtualLoopMessageBase(InContext)
		{
			const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;
			EventData.GetString("Name", Name);
			ComponentId = EventData.GetValue<uint64>("ComponentId");
		}

		FString Name;
		uint64 ComponentId = TNumericLimits<uint64>::Max();
	};

	struct FVirtualLoopUpdateMessage : public FVirtualLoopMessageBase
	{
		FVirtualLoopUpdateMessage() = default;
		FVirtualLoopUpdateMessage(const Trace::IAnalyzer::FOnEventContext& InContext)
			: FVirtualLoopMessageBase(InContext)
		{
			const Trace::IAnalyzer::FEventData& EventData = InContext.EventData;
			TimeVirtualized = EventData.GetValue<float>("TimeVirtualized");
			PlaybackTime = EventData.GetValue<float>("PlaybackTime");
			UpdateInterval = EventData.GetValue<float>("UpdateInterval");

			LocationX = EventData.GetValue<double>("LocationX");
			LocationY = EventData.GetValue<double>("LocationY");
			LocationZ = EventData.GetValue<double>("LocationZ");

			RotatorPitch = EventData.GetValue<double>("RotatorPitch");
			RotatorYaw = EventData.GetValue<double>("RotatorYaw");
			RotatorRoll = EventData.GetValue<double>("RotatorRoll");
		}

		float TimeVirtualized = 0.0f;
		float PlaybackTime = 0.0f;
		float UpdateInterval = 0.0f;

		double LocationX = 0.0;
		double LocationY = 0.0;
		double LocationZ = 0.0;

		double RotatorPitch = 0.0;
		double RotatorYaw = 0.0;
		double RotatorRoll = 0.0;
	};

	class FVirtualLoopDashboardEntry : public FSoundAssetDashboardEntry
	{
	public:
		FVirtualLoopDashboardEntry() = default;
		virtual ~FVirtualLoopDashboardEntry() = default;

		virtual bool IsValid() const override
		{
			return PlayOrder != static_cast<uint32>(INDEX_NONE);
		}

		uint32 PlayOrder = static_cast<uint32>(INDEX_NONE);
		uint64 ComponentId = TNumericLimits<uint64>::Max();

		float TimeVirtualized = 0.0f;
		float PlaybackTime = 0.0f;
		float UpdateInterval = 0.0f;

		FVector Location = FVector::ZeroVector;
		FRotator Rotator = FRotator::ZeroRotator;
	};

	class FVirtualLoopMessages
	{
		TAnalyzerMessageQueue<FVirtualLoopVirtualizeMessage> VirtualizeMessages { 5.0 };
		TAnalyzerMessageQueue<FVirtualLoopRealizeMessage> RealizeMessages { 5.0 };
		TAnalyzerMessageQueue<FVirtualLoopStopMessage> StopMessages { 5.0 };
		TAnalyzerMessageQueue<FVirtualLoopUpdateMessage> UpdateMessages { 0.1 };

		friend class FVirtualLoopTraceProvider;
	};
} // namespace UE::Audio::Insights
