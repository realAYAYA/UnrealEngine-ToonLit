// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stats.h"
#include "Async/Async.h"
#include "CanvasTypes.h"
#include "PixelStreamingDelegates.h"
#include "Settings.h"
#include "Engine/Console.h"
#include "ConsoleSettings.h"
#include "PixelStreamingPeerConnection.h"

namespace UE::PixelStreaming
{
	FStats* FStats::Instance = nullptr;

	FStats* FStats::Get()
	{
		if (Instance == nullptr)
		{
			Instance = new FStats();
		}
		return Instance;
	}

	void FStats::AddWebRTCStatsSource(IStatsSource* InStatsSource)
	{
		FScopeLock Lock(&WebRTCStatsSourceListCS);
		WebRTCStatsSourceList.Add(InStatsSource);
	}

	void FStats::RemoveWebRTCStatsSource(IStatsSource* InStatsSource)
	{
		FScopeLock Lock(&WebRTCStatsSourceListCS);
		WebRTCStatsSourceList.Remove(InStatsSource);
	}

	FStats::FStats()
	{
		checkf(Instance == nullptr, TEXT("There should only ever been one PixelStreaming stats object."));
		UConsole::RegisterConsoleAutoCompleteEntries.AddRaw(this, &FStats::UpdateConsoleAutoComplete);
	}

	void FStats::StorePeerStat(FPixelStreamingPlayerId PlayerId, FStatData Stat)
	{
		bool Updated = false;
		{
			FScopeLock Lock(&PeerStatsCS);

			if (!PeerStats.Contains(PlayerId))
			{
				PeerStats.Add(PlayerId, FPeerStats(PlayerId));
				Updated = true;
			}
			else
			{
				Updated = PeerStats[PlayerId].StoreStat(Stat);
			}
		}

		if (Updated)
		{
			FireStatChanged(PlayerId, Stat.StatName, Stat.StatValue);
		}
	}

	bool FStats::QueryPeerStat(FPixelStreamingPlayerId PlayerId, FName StatToQuery, double& OutValue) const
	{
		FScopeLock Lock(&PeerStatsCS);

		if (const FPeerStats* SinglePeerStats = PeerStats.Find(PlayerId))
		{
			if (const FRenderableStat* StoredStat = SinglePeerStats->StoredStats.Find(StatToQuery))
			{
				OutValue = StoredStat->Stat.StatValue;
				return true;
			}
		}

		return false;
	}

	void FStats::RemovePeerStats(FPixelStreamingPlayerId PlayerId)
	{
		FScopeLock Lock(&PeerStatsCS);

		PeerStats.Remove(PlayerId);

		if (PlayerId == SFU_PLAYER_ID)
		{
			TArray<FPixelStreamingPlayerId> ToRemove;

			for (auto& Entry : PeerStats)
			{
				FPixelStreamingPlayerId PeerId = Entry.Key;
				if (PeerId.Contains(TEXT("Simulcast"), ESearchCase::IgnoreCase, ESearchDir::FromStart))
				{
					ToRemove.Add(PeerId);
				}
			}

			for (FPixelStreamingPlayerId SimulcastLayerId : ToRemove)
			{
				PeerStats.Remove(SimulcastLayerId);
			}
		}
	}

	double CalcMA(double PrevAvg, int NumSamples, double Value)
	{
		const double Result = NumSamples * PrevAvg + Value;
		return Result / (PrevAvg + 1.0);
	}

	double CalcEMA(double PrevAvg, int NumSamples, double Value)
	{
		const double Mult = 2.0 / (NumSamples + 1.0);
		const double Result = (Value - PrevAvg) * Mult + PrevAvg;
		return Result;
	}

	void FStats::StoreApplicationStat(FStatData Stat)
	{
		bool bUpdated = false;

		{
			FScopeLock Lock(&ApplicationStatsCS);

			if (ApplicationStats.Contains(Stat.StatName))
			{
				FRenderableStat* RenderableStat = ApplicationStats.Find(Stat.StatName);

				if (Stat.bSmooth && RenderableStat->Stat.StatValue != 0)
				{
					const int MaxSamples = 60;
					RenderableStat->Stat.NumSamples = FGenericPlatformMath::Min(MaxSamples, RenderableStat->Stat.NumSamples + 1);
					if (RenderableStat->Stat.NumSamples < MaxSamples)
					{
						RenderableStat->Stat.LastEMA = CalcMA(RenderableStat->Stat.LastEMA, RenderableStat->Stat.NumSamples - 1, Stat.StatValue);
					}
					else
					{
						RenderableStat->Stat.LastEMA = CalcEMA(RenderableStat->Stat.LastEMA, RenderableStat->Stat.NumSamples - 1, Stat.StatValue);
					}
					RenderableStat->Stat.StatValue = RenderableStat->Stat.LastEMA;
					bUpdated = true;
				}
				else
				{
					bUpdated = RenderableStat->Stat.StatValue != Stat.StatValue;
					RenderableStat->Stat.StatValue = Stat.StatValue;
				}

				if (bUpdated)
				{
					FText TextToDisplay = FText::FromString(FString::Printf(TEXT("%s: %.*f"), *Stat.StatName.ToString(), Stat.NDecimalPlacesToPrint, RenderableStat->Stat.StatValue));
					RenderableStat->CanvasItem.Text = TextToDisplay;
				}
			}
			else
			{
				FText TextToDisplay = FText::FromString(FString::Printf(TEXT("%s: %.*f"), *Stat.StatName.ToString(), Stat.NDecimalPlacesToPrint, Stat.StatValue));

				FRenderableStat RenderableStat{
					Stat,
					FCanvasTextItem(FVector2D(0, 0), TextToDisplay, FSlateFontInfo(FSlateFontInfo(UEngine::GetSmallFont(), 12)), FLinearColor(0, 1, 0))
				};

				RenderableStat.CanvasItem.EnableShadow(FLinearColor::Black);

				ApplicationStats.Add(RenderableStat.Stat.StatName, RenderableStat);
				bUpdated = true;
			}
		}

		if (bUpdated)
		{
			FireStatChanged(FPixelStreamingPlayerId(TEXT("Application")), Stat.StatName, Stat.StatValue);
		}
	}

	void FStats::FireStatChanged(FPixelStreamingPlayerId PlayerId, FName StatName, float StatValue)
	{
		if (UPixelStreamingDelegates* Delegates = UPixelStreamingDelegates::GetPixelStreamingDelegates())
		{
			Delegates->OnStatChangedNative.Broadcast(PlayerId, StatName, StatValue);
			Delegates->OnStatChanged.Broadcast(PlayerId, StatName, StatValue);
		}
	}

	void FStats::UpdateConsoleAutoComplete(TArray<FAutoCompleteCommand>& AutoCompleteList)
	{
		// This *might* need to be on the game thread? I haven't seen issues not explicitly putting it on the game thread though.

		const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();

		AutoCompleteList.AddDefaulted();
		FAutoCompleteCommand& AutoCompleteCommand = AutoCompleteList.Last();
		AutoCompleteCommand.Command = TEXT("Stat PixelStreaming");
		AutoCompleteCommand.Desc = TEXT("Displays stats about Pixel Streaming on screen.");
		AutoCompleteCommand.Color = ConsoleSettings->AutoCompleteCommandColor;

		AutoCompleteList.AddDefaulted();
		FAutoCompleteCommand& AutoCompleteGraphCommand = AutoCompleteList.Last();
		AutoCompleteGraphCommand.Command = TEXT("Stat PixelStreamingGraphs");
		AutoCompleteGraphCommand.Desc = TEXT("Displays graphs about Pixel Streaming on screen.");
		AutoCompleteGraphCommand.Color = ConsoleSettings->AutoCompleteCommandColor;
	}

	int32 FStats::OnRenderStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
	{
		if (GAreScreenMessagesEnabled)
		{
			Y += 250;
			// Draw each peer's stats in a column, so we must recall where Y starts for each column
			int32 YStart = Y;

			// --------- Draw stats for this Pixel Streaming instance ----------

			{
				FScopeLock Lock(&ApplicationStatsCS);

				for (auto& ApplicationStatEntry : ApplicationStats)
				{
					FRenderableStat& StatToDraw = ApplicationStatEntry.Value;
					StatToDraw.CanvasItem.Position.X = X;
					StatToDraw.CanvasItem.Position.Y = Y;
					Canvas->DrawItem(StatToDraw.CanvasItem);
					Y += StatToDraw.CanvasItem.DrawnSize.Y;
				}
			}

			// --------- Draw stats for each peer ----------

			// increment X now we are done drawing application stats
			X += 435;

			// TMap<FPixelStreamingPlayerId, FPeerStats> PeerStats;
			{
				FScopeLock Lock(&PeerStatsCS);

				for (auto& EachPeerEntry : PeerStats)
				{
					FPeerStats& SinglePeerStats = EachPeerEntry.Value;
					if (SinglePeerStats.StoredStats.Num() == 0)
					{
						continue;
					}

					// Reset Y for each peer as each peer gets it own column
					Y = YStart;

					SinglePeerStats.PlayerIdCanvasItem.Position.X = X;
					SinglePeerStats.PlayerIdCanvasItem.Position.Y = Y;
					Canvas->DrawItem(SinglePeerStats.PlayerIdCanvasItem);
					Y += SinglePeerStats.PlayerIdCanvasItem.DrawnSize.Y;

					for (auto& NameStatKeyVal : SinglePeerStats.StoredStats)
					{
						FRenderableStat& Stat = NameStatKeyVal.Value;
						Stat.CanvasItem.Position.X = X;
						Stat.CanvasItem.Position.Y = Y;
						Canvas->DrawItem(Stat.CanvasItem);
						Y += Stat.CanvasItem.DrawnSize.Y;
					}

					// Each peer's stats gets its own column
					X += 250;
				}
			}
		}
		return Y;
	}

	bool FStats::OnToggleStats(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
	{
		// todo: all about the toggle func
		return true;
	}

	bool FStats::OnToggleGraphs(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream)
	{
		return true;
	}

	int32 FStats::OnRenderGraphs(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation)
	{
		FVector2D GraphPos{ 0, 0 };
		FVector2D GraphSize{ 200, 200 };
		float GraphSpacing = 5;

		for (auto& [GraphName, Graph] : Graphs)
		{
			Graph.Draw(Canvas, GraphPos, GraphSize);
			GraphPos.X += GraphSize.X + GraphSpacing;
			if (GraphPos.X > Canvas->GetParentCanvasSize().X)
			{
				GraphPos.Y += GraphSize.Y + GraphSpacing;
				GraphPos.X = 0;
			}
		}

		for (auto& [TileName, Tile] : Tiles)
		{
			Tile.Position.X = GraphPos.X;
			Tile.Position.Y = GraphPos.Y;
			Tile.Size = GraphSize;
			Tile.Draw(Canvas);
			GraphPos.X += GraphSize.X + GraphSpacing;
			if (GraphPos.X > Canvas->GetParentCanvasSize().X)
			{
				GraphPos.Y += GraphSize.Y + GraphSpacing;
				GraphPos.X = 0;
			}
		}

		return Y;
	}

	void FStats::PollPixelStreamingSettings()
	{
		double DeltaSeconds = FPlatformTime::ToSeconds64(FPlatformTime::Cycles64() - LastTimeSettingsPolledCycles);
		if (DeltaSeconds > 1)
		{
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.Encoder.MinQP")), Settings::CVarPixelStreamingEncoderMinQP.GetValueOnAnyThread(), 0));
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.Encoder.MaxQP")), Settings::CVarPixelStreamingEncoderMaxQP.GetValueOnAnyThread(), 0));
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.Encoder.KeyframeInterval (frames)")), Settings::CVarPixelStreamingEncoderKeyframeInterval.GetValueOnAnyThread(), 0));
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.WebRTC.Fps")), Settings::CVarPixelStreamingWebRTCFps.GetValueOnAnyThread(), 0));
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.WebRTC.StartBitrate")), Settings::CVarPixelStreamingWebRTCStartBitrate.GetValueOnAnyThread(), 0));
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.WebRTC.MinBitrate")), Settings::CVarPixelStreamingWebRTCMinBitrate.GetValueOnAnyThread(), 0));
			StoreApplicationStat(FStatData(FName(TEXT("PixelStreaming.WebRTC.MaxBitrate")), Settings::CVarPixelStreamingWebRTCMaxBitrate.GetValueOnAnyThread(), 0));

			LastTimeSettingsPolledCycles = FPlatformTime::Cycles64();
		}
	}

	void FStats::Tick(float DeltaTime)
	{
		// Note (Luke): If we want more metrics from WebRTC there is also the histogram counts.
		// For example:
		// RTC_HISTOGRAM_COUNTS("WebRTC.Video.NacksSent", nacks_sent, 1, 100000, 100);
		// webrtc::metrics::Histogram* Hist1 = webrtc::metrics::HistogramFactoryGetCounts("WebRTC.Video.NacksSent", 0, 100000, 100);
		// Will require calling webrtc::metrics::Enable();

		// we dispatch this to the signalling thread because the poll call gets executed on the signalling thread
		// but the signalling thread might be waiting on this mutex lock because its deleting a peer and removing
		// the stats source. That can result in a deadlock. Ideally this should probably be handled better.
		FPixelStreamingPeerConnection::PostSignalingTask([this]() {
			FScopeLock Lock(&WebRTCStatsSourceListCS);
			for (auto& Source : WebRTCStatsSourceList)
			{
				Source->PollWebRTCStats();
			}
		});

		PollPixelStreamingSettings();

		if (!GEngine)
		{
			return;
		}

		if (!bRegisterEngineStats)
		{
			RegisterEngineHooks();
		}
	}

	void FStats::RegisterEngineHooks()
	{
		GAreScreenMessagesEnabled = true;

		const FName StatName("STAT_PixelStreaming");
		const FName StatCategory("STATCAT_PixelStreaming");
		const FText StatDescription(FText::FromString("Pixel Streaming stats for all connected peers."));
		UEngine::FEngineStatRender RenderStatFunc = UEngine::FEngineStatRender::CreateRaw(this, &FStats::OnRenderStats);
		UEngine::FEngineStatToggle ToggleStatFunc = UEngine::FEngineStatToggle::CreateRaw(this, &FStats::OnToggleStats);
		GEngine->AddEngineStat(StatName, StatCategory, StatDescription, RenderStatFunc, ToggleStatFunc, false);

		const FName GraphName("STAT_PixelStreamingGraphs");
		const FText GraphDescription(FText::FromString("Pixel Streaming graphs showing frame pipeline timings."));
		UEngine::FEngineStatRender RenderGraphFunc = UEngine::FEngineStatRender::CreateRaw(this, &FStats::OnRenderGraphs);
		UEngine::FEngineStatToggle ToggleGraphFunc = UEngine::FEngineStatToggle::CreateRaw(this, &FStats::OnToggleGraphs);
		GEngine->AddEngineStat(GraphName, StatCategory, GraphDescription, RenderGraphFunc, ToggleGraphFunc, false);

		bool StatsEnabled = Settings::CVarPixelStreamingOnScreenStats.GetValueOnAnyThread();
		if (StatsEnabled)
		{
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE)
				{
					UWorld* World = WorldContext.World();
					UGameViewportClient* ViewportClient = World->GetGameViewport();
					GEngine->SetEngineStat(World, ViewportClient, TEXT("PixelStreaming"), StatsEnabled);
				}
			}
		}

		bRegisterEngineStats = true;
	}

	//
	// ---------------- PixelStreamingPeerStats ---------------------------
	// Stats specific to a particular peer, as opposed to the entire app.
	//

	bool FPeerStats::StoreStat(FStatData StatToStore)
	{
		bool bUpdated = false;

		if (!StoredStats.Contains(StatToStore.StatName))
		{
			FText TextToDisplay = FText::FromString(FString::Printf(TEXT("%s: %.*f"), *StatToStore.StatName.ToString(), StatToStore.NDecimalPlacesToPrint, StatToStore.StatValue));

			FRenderableStat RenderableStat{
				StatToStore,
				FCanvasTextItem(FVector2D(0, 0), TextToDisplay, FSlateFontInfo(FSlateFontInfo(UEngine::GetSmallFont(), 12)), FLinearColor(0, 1, 0))
			};

			RenderableStat.CanvasItem.EnableShadow(FLinearColor::Black);

			StoredStats.Add(StatToStore.StatName, RenderableStat);

			// first time this stat has been stored, so we also need to sort our stats so they render in consistent order
			StoredStats.KeySort([](const FName& A, const FName& B) {
				return A.FastLess(B);
			});

			return true;
		}
		else
		{
			// We already have this stat, so just update it

			FRenderableStat* RenderableStat = StoredStats.Find(StatToStore.StatName);
			if (!RenderableStat)
			{
				return false;
			}

			if (RenderableStat->Stat.bSmooth && RenderableStat->Stat.StatValue != 0)
			{
				const int MaxSamples = 60;
				RenderableStat->Stat.NumSamples = FGenericPlatformMath::Min(MaxSamples, RenderableStat->Stat.NumSamples + 1);
				if (RenderableStat->Stat.NumSamples < MaxSamples)
					RenderableStat->Stat.LastEMA = CalcMA(RenderableStat->Stat.LastEMA, RenderableStat->Stat.NumSamples - 1, StatToStore.StatValue);
				else
					RenderableStat->Stat.LastEMA = CalcEMA(RenderableStat->Stat.LastEMA, RenderableStat->Stat.NumSamples - 1, StatToStore.StatValue);
				RenderableStat->Stat.StatValue = RenderableStat->Stat.LastEMA;
				bUpdated = true;
			}
			else
			{
				bUpdated = RenderableStat->Stat.StatValue != StatToStore.StatValue;
				RenderableStat->Stat.StatValue = StatToStore.StatValue;
			}

			if (!bUpdated)
			{
				return false;
			}
			else
			{
				FText TextToDisplay = FText::FromString(FString::Printf(TEXT("%s: %.*f"), *StatToStore.StatName.ToString(), StatToStore.NDecimalPlacesToPrint, RenderableStat->Stat.StatValue));
				RenderableStat->CanvasItem.Text = TextToDisplay;
			}
			return bUpdated;
		}
	}

	void FStats::GraphValue(FName InName, float Value, int InSamples, float InMinRange, float InMaxRange, float InRefValue)
	{
		if (!Graphs.Contains(InName))
		{
			auto& Graph = Graphs.Add(InName, FDebugGraph(InName, InSamples, InMinRange, InMaxRange, InRefValue));
			Graph.AddValue(Value);
		}
		else
		{
			Graphs[InName].AddValue(Value);
		}
	}

	double FStats::AddTimeStat(uint64 Cycles1, const FString& Label)
	{
		const double DeltaMs = FPlatformTime::ToMilliseconds64(Cycles1);
		const FStatData TimeData{ FName(*Label), DeltaMs, 2, true };
		StoreApplicationStat(TimeData);
		return DeltaMs;
	}

	double FStats::AddTimeDeltaStat(uint64 Cycles1, uint64 Cycles2, const FString& Label)
	{
		const uint64 MaxCycles = FGenericPlatformMath::Max(Cycles1, Cycles2);
		const uint64 MinCycles = FGenericPlatformMath::Min(Cycles1, Cycles2);
		const double DeltaMs = FPlatformTime::ToMilliseconds64(MaxCycles - MinCycles) * ((Cycles1 > Cycles2) ? 1.0 : -1.0);
		const FStatData TimeData{ FName(*Label), DeltaMs, 2, true };
		StoreApplicationStat(TimeData);
		return DeltaMs;
	}

	void FStats::AddFrameTimingStats(const FPixelCaptureFrameMetadata& FrameMetadata)
	{
		const double TimeCapture = AddTimeStat(FrameMetadata.CaptureTime, FString::Printf(TEXT("%s Layer %d Frame Capture Time"), *FrameMetadata.ProcessName, FrameMetadata.Layer));
		const double TimeGPU = AddTimeStat(FrameMetadata.CaptureProcessGPUTime, FString::Printf(TEXT("%s Layer %d Frame Capture GPU Time"), *FrameMetadata.ProcessName, FrameMetadata.Layer));
		const double TimeCPU = AddTimeStat(FrameMetadata.CaptureProcessCPUTime, FString::Printf(TEXT("%s Layer %d Frame Capture CPU Time"), *FrameMetadata.ProcessName, FrameMetadata.Layer));
		const double TimeEncode = AddTimeDeltaStat(FrameMetadata.LastEncodeEndTime, FrameMetadata.LastEncodeStartTime, FString::Printf(TEXT("%s Layer %d Frame Encode Time"), *FrameMetadata.ProcessName, FrameMetadata.Layer));

		const FStatData UseData{ FName(*FString::Printf(TEXT("%s Layer %d Frame Uses"), *FrameMetadata.ProcessName, FrameMetadata.Layer)), static_cast<double>(FrameMetadata.UseCount), 0, false };
		StoreApplicationStat(UseData);

		const int Samples = 100;
		GraphValue(*FString::Printf(TEXT("%d Capture Time"), FrameMetadata.Layer), StaticCast<float>(TimeCapture), Samples, 0.0f, 30.0f);
		GraphValue(*FString::Printf(TEXT("%d GPU Time"), FrameMetadata.Layer), StaticCast<float>(TimeGPU), Samples, 0.0f, 6.0f);
		GraphValue(*FString::Printf(TEXT("%d CPU Time"), FrameMetadata.Layer), StaticCast<float>(TimeCPU), Samples, 0.0f, 6.0f);
		GraphValue(*FString::Printf(TEXT("%d Encode Time"), FrameMetadata.Layer), StaticCast<float>(TimeEncode), Samples, 0.0f, 10.0f);
		GraphValue(*FString::Printf(TEXT("%d Frame Uses"), FrameMetadata.Layer), StaticCast<float>(FrameMetadata.UseCount), Samples, 0.0f, 10.0f);
	}

	void FStats::AddCanvasTile(FName Name, const FCanvasTileItem& Tile)
	{
		Tiles.FindOrAdd(Name, Tile);
	}
} // namespace UE::PixelStreaming
