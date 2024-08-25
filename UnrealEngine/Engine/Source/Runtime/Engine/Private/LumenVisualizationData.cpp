// Copyright Epic Games, Inc. All Rights Reserved.

#include "LumenVisualizationData.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "FLumenVisualizationData"

static FLumenVisualizationData GLumenVisualizationData;

// Must match appropriate values in r.Lumen.Visualize
#define LUMEN_VISUALIZE_OVERVIEW					1
#define LUMEN_VISUALIZE_PERFORMANCE_OVERVIEW		2
#define LUMEN_VISUALIZE_LUMEN_SCENE					3
#define LUMEN_VISUALIZE_REFLECTION_VIEW				4
#define LUMEN_VISUALIZE_SURFACE_CACHE				5
#define LUMEN_VISUALIZE_MODE_GEOMETRY_NORMALS		6
#define LUMEN_VISUALIZE_DEDICATED_REFLECTION_RAYS	7

void FLumenVisualizationData::Initialize()
{
	if (!bIsInitialized)
	{
		AddVisualizationMode(
			TEXT("Overview"),
			LOCTEXT("Overview", "Overview"),
			LOCTEXT("OverviewDesc", "All Lumen view mode tiles overlayed on top"),
			FModeType::Overview,
			LUMEN_VISUALIZE_OVERVIEW,
			true);

		AddVisualizationMode(
			TEXT("PerformanceOverview"),
			LOCTEXT("PerformanceOverview", "Performance Overview"),
			LOCTEXT("PerformanceOverviewDesc", "All Lumen performance view mode tiles overlayed on top"),
			FModeType::Overview,
			LUMEN_VISUALIZE_PERFORMANCE_OVERVIEW,
			true);

		AddVisualizationMode(
			TEXT("LumenScene"),
			LOCTEXT("LumenScene", "Lumen Scene"),
			LOCTEXT("LumenSceneDesc", "Visualizes Lumen scene representation in a highest possible quality and view distance"),
			FModeType::Standard,
			LUMEN_VISUALIZE_LUMEN_SCENE,
			true);

		AddVisualizationMode(
			TEXT("GeometryNormals"),
			LOCTEXT("GeometryNormals", "Geometry Normals"),
			LOCTEXT("GeometryNormalsDesc", "Visualizes Geometry Normals with current Lumen settings."),
			FModeType::Standard,
			LUMEN_VISUALIZE_MODE_GEOMETRY_NORMALS,
			true);

		AddVisualizationMode(
			TEXT("ReflectionView"),
			LOCTEXT("ReflectionView", "Reflection View"),
			LOCTEXT("ReflectionViewDesc", "Visualizes Lumen scene representation with current reflection settings"),
			FModeType::Standard,
			LUMEN_VISUALIZE_REFLECTION_VIEW, 
			true);

		AddVisualizationMode(
			TEXT("SurfaceCache"),
			LOCTEXT("SurfaceCache", "Surface Cache"),
			LOCTEXT("SurfaceCacheDesc", "Visualizes Lumen Surface Cache. Pink - missing surface cache coverage. Yellow - culled meshes."),
			FModeType::Standard,
			LUMEN_VISUALIZE_SURFACE_CACHE,
			true);

		AddVisualizationMode(
			TEXT("DedicatedReflectionRays"),
			LOCTEXT("DedicatedReflectionRays", "Dedicated Reflection Rays"),
			LOCTEXT("DedicatedReflectionRaysDesc", "Visualizes pixels which require dedicated Lumen reflection rays (pass MaxRoughnessToTrace or MaxRoughnessToTraceForFoliage treshold). Green - foliage. Red - other."),
			FModeType::Standard,
			LUMEN_VISUALIZE_DEDICATED_REFLECTION_RAYS,
			true);

		ConfigureConsoleCommand();

		bIsInitialized = true;
	}
}

void FLumenVisualizationData::ConfigureConsoleCommand()
{
	FString AvailableVisualizationModes;
	for (TModeMap::TConstIterator It = ModeMap.CreateConstIterator(); It; ++It)
	{
		const FModeRecord& Record = It.Value();
		AvailableVisualizationModes += FString(TEXT("\n  "));
		AvailableVisualizationModes += Record.ModeString;
	}

	ConsoleDocumentationVisualizationMode = TEXT("When the viewport view-mode is set to 'Lumen Visualization', this command specifies which of the various channels to display. Values entered other than the allowed values shown below will be ignored.");
	ConsoleDocumentationVisualizationMode += AvailableVisualizationModes;

	IConsoleManager::Get().RegisterConsoleVariable(
		GetVisualizeConsoleCommandName(),
		TEXT(""),
		*ConsoleDocumentationVisualizationMode,
		ECVF_Cheat);
}

void FLumenVisualizationData::AddVisualizationMode(
	const TCHAR* ModeString,
	const FText& ModeText,
	const FText& ModeDesc,
	const FModeType ModeType,
	int32 ModeID,
	bool DefaultComposited
)
{
	const FName ModeName = FName(ModeString);

	FModeRecord& Record	= ModeMap.Emplace(ModeName);
	Record.ModeString			= FString(ModeString);
	Record.ModeName				= ModeName;
	Record.ModeText				= ModeText;
	Record.ModeDesc				= ModeDesc;
	Record.ModeType				= ModeType;
	Record.ModeID				= ModeID;
	Record.DefaultComposited	= DefaultComposited;
}

FText FLumenVisualizationData::GetModeDisplayName(const FName& InModeName) const
{
	if (const FModeRecord* Record = ModeMap.Find(InModeName))
	{
		return Record->ModeText;
	}
	else
	{
		return FText::GetEmpty();
	}
}

int32 FLumenVisualizationData::GetModeID(const FName& InModeName) const
{
	if (const FModeRecord* Record = ModeMap.Find(InModeName))
	{
		return Record->ModeID;
	}
	else
	{
		return INDEX_NONE;
	}
}

bool FLumenVisualizationData::GetModeDefaultComposited(const FName& InModeName) const
{
	if (const FModeRecord* Record = ModeMap.Find(InModeName))
	{
		return Record->DefaultComposited;
	}
	else
	{
		return false;
	}
}

FLumenVisualizationData& GetLumenVisualizationData()
{
	if (!GLumenVisualizationData.IsInitialized())
	{
		GLumenVisualizationData.Initialize();
	}

	return GLumenVisualizationData;
}

#undef LOCTEXT_NAMESPACE
