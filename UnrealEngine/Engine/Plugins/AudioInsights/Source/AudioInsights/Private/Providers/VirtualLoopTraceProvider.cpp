// Copyright Epic Games, Inc. All Rights Reserved.
#include "Providers/VirtualLoopTraceProvider.h"

#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/ModuleService.h"


namespace UE::Audio::Insights
{
	FName FVirtualLoopTraceProvider::GetName_Static()
	{
		return "AudioVirtualLoopProvider";
	}

	bool FVirtualLoopTraceProvider::ProcessMessages()
	{
		auto RemoveEntryFunc = [this](const FVirtualLoopRealizeMessage& Msg, TSharedPtr<FVirtualLoopDashboardEntry>* OutEntry)
		{
			if (OutEntry && (*OutEntry)->Timestamp < Msg.Timestamp)
			{
				RemoveDeviceEntry(Msg.DeviceId, Msg.PlayOrder);
			}
		};

		auto GetEntryFunc = [this](const FVirtualLoopMessageBase& Msg)
		{
			return FindDeviceEntry(Msg.DeviceId, Msg.PlayOrder);
		};

		ProcessMessageQueue<FVirtualLoopStopMessage>(TraceMessages.StopMessages, GetEntryFunc, RemoveEntryFunc);

		ProcessMessageQueue<FVirtualLoopVirtualizeMessage>(TraceMessages.VirtualizeMessages,
		[this](const FVirtualLoopMessageBase& Msg)
		{
			TSharedPtr<FVirtualLoopDashboardEntry>* ToReturn = nullptr;
			UpdateDeviceEntry(Msg.DeviceId, Msg.PlayOrder, [&ToReturn, &Msg](TSharedPtr<FVirtualLoopDashboardEntry>& Entry)
			{
				if (!Entry.IsValid())
				{
					Entry = MakeShared<FVirtualLoopDashboardEntry>();
				}
				Entry->DeviceId = Msg.DeviceId;
				Entry->PlayOrder = Msg.PlayOrder;
				Entry->Timestamp = Msg.Timestamp;

				ToReturn = &Entry;
			});
			return ToReturn;
		},
		[](const FVirtualLoopVirtualizeMessage& Msg, TSharedPtr<FVirtualLoopDashboardEntry>* OutEntry)
		{
			FVirtualLoopDashboardEntry& EntryRef = *OutEntry->Get();
			EntryRef.Name = Msg.Name;
			EntryRef.ComponentId = Msg.ComponentId;
		});

		ProcessMessageQueue<FVirtualLoopRealizeMessage>(TraceMessages.RealizeMessages, 
		[this](const FVirtualLoopMessageBase& Msg)
		{
			TSharedPtr<FVirtualLoopDashboardEntry>* ToReturn = nullptr;

			UpdateDeviceEntry(Msg.DeviceId, Msg.PlayOrder, [&ToReturn, &Msg](TSharedPtr<FVirtualLoopDashboardEntry>& Entry)
			{
				if (!Entry.IsValid())
				{
					Entry = MakeShared<FVirtualLoopDashboardEntry>();
					Entry->DeviceId = Msg.DeviceId;
					Entry->PlayOrder = Msg.PlayOrder;
				}
				Entry->Timestamp = Msg.Timestamp;
				ToReturn = &Entry;
			});

			return ToReturn;
		}, 
		RemoveEntryFunc);

		ProcessMessageQueue<FVirtualLoopUpdateMessage>(TraceMessages.UpdateMessages, 
		GetEntryFunc,
		[](const FVirtualLoopUpdateMessage& Msg, TSharedPtr<FVirtualLoopDashboardEntry>* OutEntry)
		{
			if (OutEntry)
			{
				FVirtualLoopDashboardEntry& EntryRef = *OutEntry->Get();
				EntryRef.PlaybackTime = Msg.PlaybackTime;
				EntryRef.TimeVirtualized = Msg.TimeVirtualized;
				EntryRef.UpdateInterval = Msg.UpdateInterval;
				EntryRef.Location = FVector{ Msg.LocationX, Msg.LocationY, Msg.LocationZ };
				EntryRef.Rotator = FRotator{ Msg.RotatorPitch, Msg.RotatorYaw, Msg.RotatorRoll };
			}
		});

		return true;
	}

	UE::Trace::IAnalyzer* FVirtualLoopTraceProvider::ConstructAnalyzer()
	{
		class FVirtualLoopTraceAnalyzer : public FTraceAnalyzerBase
		{
		public:
			FVirtualLoopTraceAnalyzer(TSharedRef<FVirtualLoopTraceProvider> InProvider)
				: FTraceAnalyzerBase(InProvider)
			{
			}

			virtual void OnAnalysisBegin(const FOnAnalysisContext& Context) override
			{
				FTraceAnalyzerBase::OnAnalysisBegin(Context);

				UE::Trace::IAnalyzer::FInterfaceBuilder& Builder = Context.InterfaceBuilder;
				Builder.RouteEvent(RouteId_Realize, "Audio", "VirtualLoopRealize");
				Builder.RouteEvent(RouteId_Stop, "Audio", "VirtualLoopStop");
				Builder.RouteEvent(RouteId_Update, "Audio", "VirtualLoopUpdate");
				Builder.RouteEvent(RouteId_Virtualize, "Audio", "VirtualLoopVirtualize");
			}

			virtual bool OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context) override
			{
				LLM_SCOPE_BYNAME(TEXT("Insights/FVirtualLoopTraceAnalyzer"));

				FVirtualLoopMessages& Messages = GetProvider<FVirtualLoopTraceProvider>().TraceMessages;
				switch (RouteId)
				{
					case RouteId_Realize:
					{
						Messages.RealizeMessages.Enqueue(FVirtualLoopRealizeMessage { Context });
						break;
					}

					case RouteId_Stop:
					{
						Messages.StopMessages.Enqueue(FVirtualLoopStopMessage { Context });
						break;
					}

					case RouteId_Update:
					{
						Messages.UpdateMessages.Enqueue(FVirtualLoopUpdateMessage { Context });
						break;
					}

					case RouteId_Virtualize:
					{
						Messages.VirtualizeMessages.Enqueue(FVirtualLoopVirtualizeMessage { Context });
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
				RouteId_Virtualize,
				RouteId_Update,
				RouteId_Realize,
				RouteId_Stop
			};
		};

		return new FVirtualLoopTraceAnalyzer(AsShared());
	}
} // namespace UE::Audio::Insights
