// Copyright Epic Games, Inc. All Rights Reserved.
#include "Providers/MixerSourceTraceProvider.h"

#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ModuleService.h"


namespace UE::Audio::Insights
{
	FName FMixerSourceTraceProvider::GetName_Static()
	{
		return "MixerSourceProvider";
	}

	bool FMixerSourceTraceProvider::ProcessMessages()
	{
		auto BumpEntryFunc = [this](const FMixerSourceMessageBase& Msg)
		{
			TSharedPtr<FMixerSourceDashboardEntry>* ToReturn = nullptr;
			UpdateDeviceEntry(Msg.DeviceId, Msg.PlayOrder, [&ToReturn, &Msg](TSharedPtr<FMixerSourceDashboardEntry>& Entry)
			{
				if (!Entry.IsValid())
				{
					Entry = MakeShared<FMixerSourceDashboardEntry>();
					Entry->DeviceId = Msg.DeviceId;
					Entry->PlayOrder = Msg.PlayOrder;
				}
				Entry->Timestamp = Msg.Timestamp;
				ToReturn = &Entry;
			});

			return ToReturn;
		};

		ProcessMessageQueue<FMixerSourceStartMessage>(TraceMessages.StartMessages, BumpEntryFunc,
		[](const FMixerSourceStartMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			FMixerSourceDashboardEntry& EntryRef = *OutEntry->Get();
			EntryRef.Name = *Msg.Name;
			EntryRef.ComponentId = Msg.ComponentId;
			EntryRef.SourceId = Msg.SourceId;
		});

		ProcessMessageQueue<FMixerSourceVolumeMessage>(TraceMessages.VolumeMessages, BumpEntryFunc,
		[](const FMixerSourceVolumeMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			(*OutEntry)->Volume = Msg.Volume;
		});

		ProcessMessageQueue<FMixerSourcePitchMessage>(TraceMessages.PitchMessages, BumpEntryFunc,
		[](const FMixerSourcePitchMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			(*OutEntry)->Pitch = Msg.Pitch;
		});

		ProcessMessageQueue<FMixerSourceLPFFreqMessage>(TraceMessages.LPFFreqMessages, BumpEntryFunc,
		[](const FMixerSourceLPFFreqMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			(*OutEntry)->LPFFreq = Msg.LPFFrequency;
		});

		ProcessMessageQueue<FMixerSourceHPFFreqMessage>(TraceMessages.HPFFreqMessages, BumpEntryFunc,
		[](const FMixerSourceHPFFreqMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			(*OutEntry)->HPFFreq = Msg.HPFFrequency;
		});

		MaxEnvsMap.Reset();
		ProcessMessageQueue<FMixerSourceEnvelopeMessage>(TraceMessages.EnvelopeMessages, BumpEntryFunc,
		[this](const FMixerSourceEnvelopeMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			float& MaxEnv = MaxEnvsMap.FindOrAdd(Msg.PlayOrder);
			MaxEnv = FMath::Max(MaxEnv, Msg.Envelope);
			(*OutEntry)->Envelope = MaxEnv;
		});

		ProcessMessageQueue<FMixerSourceDistanceAttenuationMessage>(TraceMessages.DistanceAttenuationMessages, BumpEntryFunc,
		[](const FMixerSourceDistanceAttenuationMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			(*OutEntry)->DistanceAttenuation = Msg.DistanceAttenuation;
		});

		auto GetEntry = [this](const FMixerSourceMessageBase& Msg)
		{
			return FindDeviceEntry(Msg.DeviceId, Msg.PlayOrder);
		};

		ProcessMessageQueue<FMixerSourceStopMessage>(TraceMessages.StopMessages, GetEntry,
		[this](const FMixerSourceStopMessage& Msg, TSharedPtr<FMixerSourceDashboardEntry>* OutEntry)
		{
			if (OutEntry && (*OutEntry)->Timestamp < Msg.Timestamp)
			{
				RemoveDeviceEntry(Msg.DeviceId, Msg.PlayOrder);
			}
		});

		return true;
	}

	UE::Trace::IAnalyzer* FMixerSourceTraceProvider::ConstructAnalyzer()
	{
		class FMixerSourceTraceAnalyzer : public FTraceAnalyzerBase
		{
		public:
			FMixerSourceTraceAnalyzer(TSharedRef<FMixerSourceTraceProvider> InProvider)
				: FTraceAnalyzerBase(InProvider)
			{
			}

			virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
			{
				UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;
				Builder.RouteEvent(RouteId_DistanceAttenuation, "Audio", "MixerSourceDistanceAttenuation");
				Builder.RouteEvent(RouteId_Envelope, "Audio", "MixerSourceEnvelope");
				Builder.RouteEvent(RouteId_Filters, "Audio", "MixerSourceFilters");
				Builder.RouteEvent(RouteId_Pitch, "Audio", "MixerSourcePitch");
				Builder.RouteEvent(RouteId_Start, "Audio", "MixerSourceStart");
				Builder.RouteEvent(RouteId_Stop, "Audio", "MixerSourceStop");
				Builder.RouteEvent(RouteId_Volume, "Audio", "MixerSourceVolume");
			}

			virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override
			{
				LLM_SCOPE_BYNAME(TEXT("Insights/FMixerSourceTraceAnalyzer"));

				FMixerSourceMessages& Messages = GetProvider<FMixerSourceTraceProvider>().TraceMessages;
				switch (RouteId)
				{
					case RouteId_Start:
					{
						Messages.StartMessages.Enqueue(FMixerSourceStartMessage { Context });
						break;
					}

					case RouteId_Stop:
					{
						Messages.StopMessages.Enqueue(FMixerSourceStopMessage { Context });
						break;
					}

					case RouteId_Volume:
					{
						Messages.VolumeMessages.Enqueue(FMixerSourceVolumeMessage { Context });
						break;
					}

					case RouteId_Pitch:
					{
						Messages.PitchMessages.Enqueue(FMixerSourcePitchMessage { Context });
						break;
					}

					case RouteId_Envelope:
					{
						Messages.EnvelopeMessages.Enqueue(FMixerSourceEnvelopeMessage { Context });
						break;
					}

					case RouteId_Filters:
					{
						Messages.LPFFreqMessages.Enqueue(FMixerSourceLPFFreqMessage { Context });
						Messages.HPFFreqMessages.Enqueue(FMixerSourceHPFFreqMessage { Context });
						break;
					}

					case RouteId_DistanceAttenuation:
					{
						Messages.DistanceAttenuationMessages.Enqueue(FMixerSourceDistanceAttenuationMessage { Context });
						break;
					}

					default:
					{
						return OnEventFailure(RouteId, Style, Context);
					}
				}

				return OnEventSuccess(RouteId, Style, Context);
			}

		private:
			enum : uint16
			{
				RouteId_DistanceAttenuation,
				RouteId_Envelope,
				RouteId_Filters,
				RouteId_Pitch,
				RouteId_Start,
				RouteId_Stop,
				RouteId_Volume,
			};
		};

		return new FMixerSourceTraceAnalyzer(AsShared());
	}
} // namespace UE::Audio::Insights
