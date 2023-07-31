// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"
#include "PixelStreamingPlayerId.h"
#include "CanvasItem.h"
#include "UnrealEngine.h"
#include "ConsoleSettings.h"
#include "DebugGraph.h"

#include "PixelCaptureFrameMetadata.h"

namespace UE::PixelStreaming
{
	// An interface that allows us to collect webrtc stats from anything that implements it
	class IStatsSource
	{
	public:
		virtual ~IStatsSource() = default;
		virtual void PollWebRTCStats() const = 0;
	};

	struct FStatData
	{
	public:
		FStatData(FName InStatName, double InStatValue, int InNDecimalPlacesToPrint, bool bInSmooth = false)
			: StatName(InStatName)
			, StatValue(InStatValue)
			, NDecimalPlacesToPrint(InNDecimalPlacesToPrint)
			, bSmooth(bInSmooth)
		{
		}

		bool operator==(const FStatData& Other) const
		{
			return Equals(Other);
		}

		bool Equals(const FStatData& Other) const
		{
			return StatName == Other.StatName;
		}

		FName StatName;
		double StatValue;
		int NDecimalPlacesToPrint;
		bool bSmooth;
		double LastEMA = 0;
		int NumSamples = 0;
	};

	FORCEINLINE uint32 GetTypeHash(const FStatData& Obj)
	{
		// From UnrealString.h
		return GetTypeHash(Obj.StatName);
	}

	struct FRenderableStat
	{
		FStatData Stat;
		FCanvasTextItem CanvasItem;
	};

	// Pixel Streaming stats that are associated with a specific peer.
	class FPeerStats
	{

	public:
		FPeerStats(FPixelStreamingPlayerId InAssociatedPlayer)
			: AssociatedPlayer(InAssociatedPlayer)
			, PlayerIdCanvasItem(FVector2D(0, 0), FText::FromString(FString::Printf(TEXT("[Peer Stats(%s)]"), *AssociatedPlayer)), FSlateFontInfo(FSlateFontInfo(UEngine::GetSmallFont(), 12)), FLinearColor(0, 1, 0))
		{
			PlayerIdCanvasItem.EnableShadow(FLinearColor::Black);
		};

		bool StoreStat(FStatData StatToStore);

	private:
		int DisplayId = 0;
		FPixelStreamingPlayerId AssociatedPlayer;

	public:
		TMap<FName, FRenderableStat> StoredStats;
		FCanvasTextItem PlayerIdCanvasItem;
	};

	// Stats about Pixel Streaming that can displayed either in the in-application HUD, in the log, or simply reported to some subscriber.
	class FStats : FTickableGameObject
	{
	public:
		static constexpr double SmoothingPeriod = 3.0 * 60.0;
		static constexpr double SmoothingFactor = 10.0 / 100.0;
		static FStats* Get();

		void AddWebRTCStatsSource(IStatsSource* InSource);
		void RemoveWebRTCStatsSource(IStatsSource* InSource);

		FStats(const FStats&) = delete;
		bool QueryPeerStat(FPixelStreamingPlayerId PlayerId, FName StatToQuery, double& OutValue) const;
		void RemovePeerStats(FPixelStreamingPlayerId PlayerId);
		void StorePeerStat(FPixelStreamingPlayerId PlayerId, FStatData Stat);
		void StoreApplicationStat(FStatData PeerStat);
		void Tick(float DeltaTime);

		bool OnToggleStats(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream);
		int32 OnRenderStats(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation);

		bool OnToggleGraphs(UWorld* World, FCommonViewportClient* ViewportClient, const TCHAR* Stream);
		int32 OnRenderGraphs(UWorld* World, FViewport* Viewport, FCanvas* Canvas, int32 X, int32 Y, const FVector* ViewLocation, const FRotator* ViewRotation);

		FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(PixelStreamingStats, STATGROUP_Tickables); }

		void GraphValue(FName InName, float Value, int InSamples, float InMinRange, float InMaxRange, float InRefValue = 0.0f);

		double AddTimeStat(uint64 Cycles1, const FString& Label);
		double AddTimeDeltaStat(uint64 Cycles1, uint64 Cycles2, const FString& Label);
		void AddFrameTimingStats(const FPixelCaptureFrameMetadata& FrameMetadata);

		void AddCanvasTile(FName Name, const FCanvasTileItem& Tile);

	private:
		FStats();
		void RegisterEngineHooks();
		void PollPixelStreamingSettings();
		void RemovePeerStat(FPixelStreamingPlayerId PlayerId);
		void FireStatChanged(FPixelStreamingPlayerId PlayerId, FName StatName, float StatValue);
		void UpdateConsoleAutoComplete(TArray<FAutoCompleteCommand>& AutoCompleteList);

	private:
		static FStats* Instance;

		FCriticalSection WebRTCStatsSourceListCS;
		TArray<IStatsSource*> WebRTCStatsSourceList;

		bool bRegisterEngineStats = false;

		mutable FCriticalSection PeerStatsCS;
		TMap<FPixelStreamingPlayerId, FPeerStats> PeerStats;

		mutable FCriticalSection ApplicationStatsCS;
		TMap<FName, FRenderableStat> ApplicationStats;

		int64 LastTimeSettingsPolledCycles = 0;

		TMap<FName, FDebugGraph> Graphs;
		TMap<FName, FCanvasTileItem> Tiles;
	};
} // namespace UE::PixelStreaming
