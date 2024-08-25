// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SAudioCurveView.h"
#include "Views/TableDashboardViewFactory.h"
#include "Widgets/Input/SCheckBox.h"


// Removing temporarily due to redesign of solo and mute functionality
#define AUDIO_INSIGHTS_SHOW_SOURCE_CONTEXT_MENU 0

namespace UE::Audio::Insights
{
	class FMixerSourceDashboardViewFactory : public FTraceObjectTableDashboardViewFactory
	{
	public:
		FMixerSourceDashboardViewFactory();
		struct FPlotColumnInfo
		{
			const TFunctionRef<float(const IDashboardDataViewEntry& InData)> DataFunc;
			const FNumberFormattingOptions* FormatOptions;
		};

		virtual ~FMixerSourceDashboardViewFactory() = default;

		virtual FName GetName() const override;
		virtual FText GetDisplayName() const override;
		virtual FSlateIcon GetIcon() const override;
		virtual EDefaultDashboardTabStack GetDefaultTabStack() const override;
		virtual void ProcessEntries(FTraceTableDashboardViewFactory::EProcessReason Reason) override;
		virtual TSharedRef<SWidget> MakeWidget() override;

		// Maximum amount of data history kept for plots (in seconds)
		static const double MaxPlotHistorySeconds;
		// Maximum number of sources to plot at once 
		static const int32 MaxPlotSources; 

	protected:
		virtual const TMap<FName, FTraceTableDashboardViewFactory::FColumnData>& GetColumns() const override;
		virtual void SortTable() override;

#if WITH_EDITOR
		virtual bool IsDebugDrawEnabled() const override;
		virtual void DebugDraw(float InElapsed, const IDashboardDataViewEntry& InEntry, ::Audio::FDeviceId DeviceId) const override;
#endif // WITH_EDITOR
		TSharedRef<SWidget> MakePlotsWidget();

	private:
		void ResetPlots();
		void OnPIEStopped(bool bSimulating);
		void UpdatePlotsWidgetsData();
		void UpdateSoloMuteState();

		// Column information used by plot widgets, keyed by column name. These keys should be a subset of the keys in GetColumns(). 
		const TMap<FName, FPlotColumnInfo>& GetPlotColumnInfo();
		const TFunctionRef<float(const IDashboardDataViewEntry& InData)> GetPlotColumnDataFunc(const FName& ColumnName);
		const FNumberFormattingOptions* GetPlotColumnNumberFormat(const FName& ColumnName);
		const FText GetPlotColumnDisplayName(const FName& ColumnName);

#if AUDIO_INSIGHTS_SHOW_SOURCE_CONTEXT_MENU
		virtual TSharedPtr<SWidget> OnConstructContextMenu() override;
#endif
		virtual FSlateColor GetRowColor(const TSharedPtr<IDashboardDataViewEntry>& InRowDataPtr) override;

		TSharedRef<SWidget> MakeMuteSoloWidget();

		void ToggleMuteForAllItems(ECheckBoxState NewState);
		void ToggleSoloForAllItems(ECheckBoxState NewState);

		void MuteSound();
		void SoloSound();
		void ClearMutesAndSolos();

		using FPlotCurvePoint = SAudioCurveView::FCurvePoint;
		// Map of source id to data point array 
		using FPointDataPerCurveMap = TMap<int32, TArray<FPlotCurvePoint>>;
		using FPlotCurveMetadata = SAudioCurveView::FCurveMetadata;

		FCheckBoxStyle MuteToggleButtonStyle;
		FCheckBoxStyle SoloToggleButtonStyle;

		TSharedPtr<SCheckBox> MuteToggleButton;
		TSharedPtr<SCheckBox> SoloToggleButton;

		// Curve points per timestamp per source id per column name 
		TMap<FName, TSharedPtr<FPointDataPerCurveMap>> PlotWidgetCurveIdToPointDataMapPerColumn;
		// SourceId to metadata for the corresponding curve
		TSharedPtr<TMap<int32, FPlotCurveMetadata>> PlotWidgetMetadataPerCurve;
		
		// Column names for plot selector widget 
		TArray<FName> ColumnNames;
		
		double BeginTimestamp = TNumericLimits<double>::Max();
		double CurrentTimestamp = 0;

		const static int32 NumPlotWidgets = 2;
		TArray<FName> SelectedPlotColumnNames;
		TArray<TSharedPtr<SAudioCurveView>> PlotWidgets;

		// State of the mute and solo buttons
#if ENABLE_AUDIO_DEBUG
		ECheckBoxState MuteState = ECheckBoxState::Unchecked;
		ECheckBoxState SoloState = ECheckBoxState::Unchecked;
		FString CurrentFilterString;
#endif
	};
} // namespace UE::Audio::Insights
