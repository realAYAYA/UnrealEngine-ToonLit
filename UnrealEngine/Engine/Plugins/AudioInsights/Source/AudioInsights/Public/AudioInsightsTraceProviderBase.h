// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <atomic>
#include "AudioDefines.h"
#include "Containers/Ticker.h"
#include "IAudioInsightsModule.h"
#include "Messages/AnalyzerMessageQueue.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"


namespace UE::Audio::Insights
{
	class AUDIOINSIGHTS_API FTraceProviderBase
		: public TraceServices::IEditableProvider
	{
	public:
		FTraceProviderBase(FName InName);
		virtual ~FTraceProviderBase() = default;

		virtual Trace::IAnalyzer* ConstructAnalyzer() = 0;
		FName GetName() const;

		virtual void Reset()
		{
			LastUpdateId = 0;
			LastMessageId = 0;
		}

		virtual bool ProcessMessages()
		{
			LastUpdateId = LastMessageId;
			return true;
		}

		uint64 GetLastUpdateId() const
		{
			return LastUpdateId;
		}

		bool IsUpdated() const
		{
			return GetLastMessageId() == LastUpdateId;
		}

	protected:
		class AUDIOINSIGHTS_API FTraceAnalyzerBase : public Trace::IAnalyzer
		{
		public:
			FTraceAnalyzerBase(TSharedRef<FTraceProviderBase> InProvider);
			virtual ~FTraceAnalyzerBase() = default;

			virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override;

		protected:
			virtual bool OnEventSuccess(uint16 RouteId, EStyle Style, const FOnEventContext& Context);
			virtual bool OnEventFailure(uint16 RouteId, EStyle Style, const FOnEventContext& Context);

			template <typename TProviderType>
			TProviderType& GetProvider()
			{
				return *StaticCastSharedRef<TProviderType>(Provider);
			}

		private:
			TSharedRef<FTraceProviderBase> Provider;
		};

		uint64 GetLastMessageId() const
		{
			return LastMessageId;
		}

		uint64 LastUpdateId = 0;

	private:
		std::atomic<uint64> LastMessageId { 0 };
		FName Name;

		friend class FTraceAnalyzerBase;
	};

	template<typename EntryKey, typename EntryType /* = IDashboardDataViewEntry */>
	class TDeviceDataMapTraceProvider : public FTraceProviderBase
	{
	public:
		using KeyType = EntryKey;
		using ValueType = EntryType;

		using FDeviceData = TSortedMap<EntryKey, EntryType>;
		using FEntryPair = TPair<EntryKey, EntryType>;

		TDeviceDataMapTraceProvider(FName InName)
			: FTraceProviderBase(InName)
		{
			TickerHandle = FTSTicker::GetCoreTicker().AddTicker(*InName.ToString(), 0.0f, [this](float DeltaTime)
			{
				if (LastUpdateId != GetLastMessageId())
				{
					ProcessMessages();
				}
				LastUpdateId = GetLastMessageId();
				return true;
			});
		}

		virtual ~TDeviceDataMapTraceProvider()
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		}

		const TMap<::Audio::FDeviceId, FDeviceData>& GetDeviceDataMap() const
		{
			return DeviceDataMap;
		}

		const FDeviceData* FindFilteredDeviceData() const
		{
			const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
			const ::Audio::FDeviceId DeviceId = InsightsModule.GetDeviceId();

			return DeviceDataMap.Find(DeviceId);
		}

	protected:
		using Super = TDeviceDataMapTraceProvider<EntryKey, EntryType>;

		TMap<::Audio::FDeviceId, FDeviceData> DeviceDataMap;

		virtual void Reset() override
		{
			DeviceDataMap.Empty();
			FTraceProviderBase::Reset();
		}

		template <typename TMsgType>
		void ProcessMessageQueue(
			TAnalyzerMessageQueue<TMsgType>& InQueue,
			TFunctionRef<EntryType*(const TMsgType&)> GetEntry,
			TFunctionRef<void(const TMsgType&, EntryType*)> ProcessEntry
		)
		{
			TArray<TMsgType> Messages = InQueue.DequeueAll();
			for (const TMsgType& Msg : Messages)
			{
				EntryType* Entry = GetEntry(Msg);
				ProcessEntry(Msg, Entry);
			};
		}

		EntryType* FindDeviceEntry(::Audio::FDeviceId InDeviceId, const EntryKey& InKey)
		{
			if (FDeviceData* Entry = DeviceDataMap.Find(InDeviceId))
			{
				return Entry->Find(InKey);
			}

			return nullptr;
		}

		const EntryType* FindDeviceEntry(::Audio::FDeviceId InDeviceId, const EntryKey& InKey) const
		{
			if (const FDeviceData* Entry = DeviceDataMap.Find(InDeviceId))
			{
				return Entry->Find(InKey);
			}

			return nullptr;
		}

		FDeviceData* FindFilteredDeviceData()
		{
			const IAudioInsightsModule& InsightsModule = FModuleManager::GetModuleChecked<IAudioInsightsModule>(IAudioInsightsModule::GetName());
			const ::Audio::FDeviceId DeviceId = InsightsModule.GetDeviceId();

			return DeviceDataMap.Find(DeviceId);
		}

		bool RemoveDeviceEntry(::Audio::FDeviceId InDeviceId, const EntryKey& InKey)
		{
			if (FDeviceData* DeviceData = DeviceDataMap.Find(InDeviceId))
			{
				if (DeviceData->Remove(InKey) > 0)
				{
					if (DeviceData->IsEmpty())
					{
						DeviceDataMap.Remove(InDeviceId);
					}

					return true;
				}
			}

			return false;
		}

		void UpdateDeviceEntry(::Audio::FDeviceId InDeviceId, const EntryKey& InKey, TFunctionRef<void(EntryType&)> InEntryMutator)
		{
			FDeviceData& DeviceData = DeviceDataMap.FindOrAdd(InDeviceId);
			EntryType& Entry = DeviceData.FindOrAdd(InKey);
			InEntryMutator(Entry);
		}

	private:
		FTSTicker::FDelegateHandle TickerHandle;
	};
} // namespace UE::Audio::Insights
