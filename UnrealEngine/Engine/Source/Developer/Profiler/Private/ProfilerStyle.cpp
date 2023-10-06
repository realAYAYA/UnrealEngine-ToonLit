// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfilerStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"
#include "Styling/StarshipCoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/CoreStyle.h"

FName FProfilerStyle::StyleName("ProfilerStyle");
TUniquePtr<FProfilerStyle> FProfilerStyle::Inst(nullptr);

const FName& FProfilerStyle::GetStyleSetName() const
{
	return StyleName;
}

const FProfilerStyle& FProfilerStyle::Get()
{
	if (!Inst.IsValid())
	{
		Inst = TUniquePtr<FProfilerStyle>(new FProfilerStyle);
	}
	return *(Inst.Get());
}

void FProfilerStyle::Shutdown()
{
	Inst.Reset();
}

FProfilerStyle::FProfilerStyle()
	: FSlateStyleSet(StyleName)
{
	const FVector2D Icon12x12(12.0f, 12.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon32x32(32.0f, 32.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	SetParentStyleName(FAppStyle::GetAppStyleSetName());

	SetContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Profiler
	{
		FTextBlockStyle NormalText = FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText");
		// Profiler group brushes
		Set("Profiler.Group.16", new BOX_BRUSH("Icons/Profiler/GroupBorder-16Gray", FMargin(4.0f / 16.0f)));

		// Profiler toolbar icons
		Set("Profiler.Tab", new IMAGE_BRUSH_SVG("Starship/Common/Visualizer", Icon16x16));
		Set("Profiler.Tab.GraphView", new IMAGE_BRUSH("Icons/Profiler/Profiler_Graph_View_Tab_16x", Icon16x16));
		Set("Profiler.Tab.EventGraph", new IMAGE_BRUSH("Icons/Profiler/profiler_OpenEventGraph_32x", Icon16x16));
		Set("Profiler.Tab.FiltersAndPresets", new IMAGE_BRUSH("Icons/Profiler/Profiler_Filter_Presets_Tab_16x", Icon16x16));

		Set("ProfilerCommand.ProfilerManager_Load", new IMAGE_BRUSH("Icons/Profiler/Profiler_Load_Profiler_40x", Icon40x40));
		Set("ProfilerCommand.ProfilerManager_Load.Small", new IMAGE_BRUSH("Icons/Profiler/Profiler_Load_Profiler_40x", Icon20x20));

		Set("ProfilerCommand.ProfilerManager_LoadMultiple", new IMAGE_BRUSH("Icons/Profiler/Profiler_LoadMultiple_Profiler_40x", Icon40x40));
		Set("ProfilerCommand.ProfilerManager_LoadMultiple.Small", new IMAGE_BRUSH("Icons/Profiler/Profiler_LoadMultiple_Profiler_40x", Icon20x20));

		Set("ProfilerCommand.ProfilerManager_Save", new IMAGE_BRUSH_SVG("Starship/Common/save", Icon16x16));
		Set("ProfilerCommand.ProfilerManager_Save.Small", new IMAGE_BRUSH_SVG("Starship/Common/save", Icon16x16));

		Set("ProfilerCommand.ProfilerManager_ToggleLivePreview", new IMAGE_BRUSH("Automation/RefreshTests", Icon40x40));
		Set("ProfilerCommand.ProfilerManager_ToggleLivePreview.Small", new IMAGE_BRUSH("Automation/RefreshTests", Icon20x20));

		Set("ProfilerCommand.StatsProfiler", new IMAGE_BRUSH("Icons/Profiler/profiler_stats_40x", Icon40x40));
		Set("ProfilerCommand.StatsProfiler.Small", new IMAGE_BRUSH("Icons/Profiler/profiler_stats_40x", Icon20x20));

		Set("ProfilerCommand.MemoryProfiler", new IMAGE_BRUSH("Icons/Profiler/profiler_mem_40x", Icon40x40));
		Set("ProfilerCommand.MemoryProfiler.Small", new IMAGE_BRUSH("Icons/Profiler/profiler_mem_40x", Icon20x20));

		Set("ProfilerCommand.FPSChart", new IMAGE_BRUSH("Icons/Profiler/Profiler_FPS_Chart_40x", Icon40x40));
		Set("ProfilerCommand.FPSChart.Small", new IMAGE_BRUSH("Icons/Profiler/Profiler_FPS_Chart_40x", Icon20x20));

		Set("ProfilerCommand.OpenSettings", new IMAGE_BRUSH("Icons/Profiler/Profiler_Settings_40x", Icon40x40));
		Set("ProfilerCommand.OpenSettings.Small", new IMAGE_BRUSH("Icons/Profiler/Profiler_Settings_40x", Icon20x20));

		Set("ProfilerCommand.ToggleDataPreview", new IMAGE_BRUSH("Icons/Profiler/profiler_sync_40x", Icon40x40));
		Set("ProfilerCommand.ToggleDataPreview.Small", new IMAGE_BRUSH("Icons/Profiler/profiler_sync_40x", Icon20x20));

		Set("ProfilerCommand.ToggleDataCapture", new IMAGE_BRUSH("Icons/Profiler/Profiler_Data_Capture_40x", Icon40x40));
		Set("ProfilerCommand.ToggleDataCapture.Small", new IMAGE_BRUSH("Icons/Profiler/Profiler_Data_Capture_40x", Icon20x20));

		Set("ProfilerCommand.ToggleDataCapture.Checked", new IMAGE_BRUSH_SVG("Starship/Common/stop", Icon40x40));
		Set("ProfilerCommand.ToggleDataCapture.Checked.Small", new IMAGE_BRUSH_SVG("Starship/Common/stop", Icon20x20));

		Set("ProfilerCommand.ToggleShowDataGraph", new IMAGE_BRUSH("Icons/Profiler/profiler_ShowGraphData_32x", Icon32x32));
		Set("ProfilerCommand.OpenEventGraph", new IMAGE_BRUSH("Icons/Profiler/profiler_OpenEventGraph_32x", Icon16x16));

		// Tooltip hint icon
		Set("Profiler.Tooltip.HintIcon10", new IMAGE_BRUSH("Icons/Profiler/Profiler_Custom_Tooltip_12x", Icon12x12));

		// Text styles
		Set("Profiler.CaptionBold", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 10))
			.SetColorAndOpacity(FLinearColor::White)
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		);

		Set("Profiler.TooltipBold", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Bold", 8))
			.SetColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		);

		Set("Profiler.Tooltip", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 8))
			.SetColorAndOpacity(FLinearColor::White)
			.SetShadowOffset(FVector2D(1.0f, 1.0f))
			.SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.8f))
		);

		// Event graph icons
		Set("Profiler.EventGraph.SetRoot", new IMAGE_BRUSH("Icons/Profiler/profiler_SetRoot_32x", Icon32x32));
		Set("Profiler.EventGraph.CullEvents", new IMAGE_BRUSH("Icons/Profiler/Profiler_Cull_Events_16x", Icon16x16));
		Set("Profiler.EventGraph.FilterEvents", new IMAGE_BRUSH("Icons/Profiler/Profiler_Filter_Events_16x", Icon16x16));

		Set("Profiler.EventGraph.SelectStack", new IMAGE_BRUSH("Icons/Profiler/profiler_SelectStack_32x", Icon32x32));

		Set("Profiler.EventGraph.ExpandAll", new IMAGE_BRUSH("Icons/Profiler/profiler_ExpandAll_32x", Icon32x32));
		Set("Profiler.EventGraph.CollapseAll", new IMAGE_BRUSH("Icons/Profiler/profiler_CollapseAll_32x", Icon32x32));

		Set("Profiler.EventGraph.ExpandSelection", new IMAGE_BRUSH("Icons/Profiler/profiler_ExpandSelection_32x", Icon32x32));
		Set("Profiler.EventGraph.CollapseSelection", new IMAGE_BRUSH("Icons/Profiler/profiler_CollapseSelection_32x", Icon32x32));

		Set("Profiler.EventGraph.ExpandThread", new IMAGE_BRUSH("Icons/Profiler/profiler_ExpandThread_32x", Icon32x32));
		Set("Profiler.EventGraph.CollapseThread", new IMAGE_BRUSH("Icons/Profiler/profiler_CollapseThread_32x", Icon32x32));

		Set("Profiler.EventGraph.ExpandHotPath", new IMAGE_BRUSH("Icons/Profiler/profiler_ExpandHotPath_32x", Icon32x32));
		Set("Profiler.EventGraph.HotPathSmall", new IMAGE_BRUSH("Icons/Profiler/profiler_HotPath_32x", Icon12x12));

		Set("Profiler.EventGraph.ExpandHotPath16", new IMAGE_BRUSH("Icons/Profiler/profiler_HotPath_32x", Icon16x16));

		Set("Profiler.EventGraph.GameThread", new IMAGE_BRUSH("Icons/Profiler/profiler_GameThread_32x", Icon32x32));
		Set("Profiler.EventGraph.RenderThread", new IMAGE_BRUSH("Icons/Profiler/profiler_RenderThread_32x", Icon32x32));

		Set("Profiler.EventGraph.ViewColumn", new IMAGE_BRUSH("Icons/Profiler/profiler_ViewColumn_32x", Icon32x32));
		Set("Profiler.EventGraph.ResetColumn", new IMAGE_BRUSH("Icons/Profiler/profiler_ResetColumn_32x", Icon32x32));

		Set("Profiler.EventGraph.HistoryBack", new IMAGE_BRUSH("Icons/Profiler/Profiler_History_Back_16x", Icon16x16));
		Set("Profiler.EventGraph.HistoryForward", new IMAGE_BRUSH("Icons/Profiler/Profiler_History_Fwd_16x", Icon16x16));

		Set("Profiler.EventGraph.MaximumIcon", new IMAGE_BRUSH("Icons/Profiler/Profiler_Max_Event_Graph_16x", Icon16x16));
		Set("Profiler.EventGraph.AverageIcon", new IMAGE_BRUSH("Icons/Profiler/Profiler_Average_Event_Graph_16x", Icon16x16));

		Set("Profiler.EventGraph.FlatIcon", new IMAGE_BRUSH("Icons/Profiler/Profiler_Events_Flat_16x", Icon16x16));
		Set("Profiler.EventGraph.FlatCoalescedIcon", new IMAGE_BRUSH("Icons/Profiler/Profiler_Events_Flat_Coalesced_16x", Icon16x16));
		Set("Profiler.EventGraph.HierarchicalIcon", new IMAGE_BRUSH("Icons/Profiler/Profiler_Events_Hierarchial_16x", Icon16x16));

		Set("Profiler.EventGraph.HasCulledEventsSmall", new IMAGE_BRUSH("Icons/Profiler/Profiler_Has_Culled_Children_12x", Icon12x12));
		Set("Profiler.EventGraph.CulledEvent", new IMAGE_BRUSH("Icons/Profiler/Profiler_Culled_12x", Icon12x12));
		Set("Profiler.EventGraph.FilteredEvent", new IMAGE_BRUSH("Icons/Profiler/Profiler_Filtered_12x", Icon12x12));

		Set("Profiler.EventGraph.DarkText", FTextBlockStyle(NormalText)
			.SetFont(DEFAULT_FONT("Regular", 8))
			.SetColorAndOpacity(FLinearColor::Black)
			.SetShadowOffset(FVector2D(0.0f, 0.0f))
		);

		// Thread-view
		Set("Profiler.ThreadView.SampleBorder", new BOX_BRUSH("Icons/Profiler/Profiler_ThreadView_SampleBorder_16x", FMargin(2.0f / 16.0f)));

		// Event graph selected event border
		Set("Profiler.EventGraph.Border.TB", new BOX_BRUSH("Icons/Profiler/Profiler_Border_TB_16x", FMargin(4.0f / 16.0f)));
		Set("Profiler.EventGraph.Border.L", new BOX_BRUSH("Icons/Profiler/Profiler_Border_L_16x", FMargin(4.0f / 16.0f)));
		Set("Profiler.EventGraph.Border.R", new BOX_BRUSH("Icons/Profiler/Profiler_Border_R_16x", FMargin(4.0f / 16.0f)));

		// Misc

		Set("Profiler.Misc.SortBy", new IMAGE_BRUSH("Icons/Profiler/profiler_SortBy_32x", Icon32x32));
		Set("Profiler.Misc.SortAscending", new IMAGE_BRUSH("Icons/Profiler/profiler_SortAscending_32x", Icon32x32));
		Set("Profiler.Misc.SortDescending", new IMAGE_BRUSH("Icons/Profiler/profiler_SortDescending_32x", Icon32x32));

		Set("Profiler.Misc.ResetToDefault", new IMAGE_BRUSH("Icons/Profiler/profiler_ResetToDefault_32x", Icon32x32));

		Set("Profiler.Misc.Reset16", new IMAGE_BRUSH("Icons/Profiler/profiler_ResetToDefault_32x", Icon16x16));

		Set("Profiler.Type.Calls", new IMAGE_BRUSH("Icons/Profiler/profiler_Calls_32x", Icon16x16));
		Set("Profiler.Type.Event", new IMAGE_BRUSH("Icons/Profiler/profiler_Event_32x", Icon16x16));
		Set("Profiler.Type.Memory", new IMAGE_BRUSH("Icons/Profiler/profiler_Memory_32x", Icon16x16));
		Set("Profiler.Type.Number", new IMAGE_BRUSH("Icons/Profiler/profiler_Number_32x", Icon16x16));

		// NumberInt, NumberFloat, Memory, Hierarchical
		Set("Profiler.Type.NumberInt", new IMAGE_BRUSH("Icons/Profiler/profiler_Number_32x", Icon16x16));
		Set("Profiler.Type.NumberFloat", new IMAGE_BRUSH("Icons/Profiler/profiler_Number_32x", Icon16x16));
		Set("Profiler.Type.Memory", new IMAGE_BRUSH("Icons/Profiler/profiler_Memory_32x", Icon16x16));
		Set("Profiler.Type.Hierarchical", new IMAGE_BRUSH("Icons/Profiler/profiler_Event_32x", Icon16x16));

		Set("Profiler.Misc.GenericFilter", new IMAGE_BRUSH("Icons/Profiler/profiler_GenericFilter_32x", Icon16x16));
		Set("Profiler.Misc.GenericGroup", new IMAGE_BRUSH("Icons/Profiler/profiler_GenericGroup_32x", Icon16x16));
		Set("Profiler.Misc.CopyToClipboard", new IMAGE_BRUSH("Icons/Profiler/profiler_CopyToClipboard_32x", Icon32x32));

		Set("Profiler.Misc.Disconnect", new IMAGE_BRUSH("Icons/Profiler/profiler_Disconnect_32x", Icon32x32));

		Set("ProfileVisualizer.Mono", new BOX_BRUSH("Common/ProfileVisualizer_Mono", FMargin(5.f / 12.f)));
	}


	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FProfilerStyle::~FProfilerStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}
