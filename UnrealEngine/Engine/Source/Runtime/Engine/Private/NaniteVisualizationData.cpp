// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteVisualizationData.h"
#include "NaniteDefinitions.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITORONLY_DATA
#include "LevelEditorViewport.h"
#else
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "NaniteDefinitions.h"
#include "SceneManagement.h"
#endif

#define LOCTEXT_NAMESPACE "FNaniteVisualizationData"

static FNaniteVisualizationData GNaniteVisualizationData;

// Force center crosshair for debug picking, even if mouse control is available
int32 GNanitePickingCrosshair = 0;
static FAutoConsoleVariableRef CVarNanitePickingCrosshair(
	TEXT("r.Nanite.Picking.Crosshair"),
	GNanitePickingCrosshair,
	TEXT("")
);

void FNaniteVisualizationData::Initialize()
{
	if (!bIsInitialized)
	{
		AddVisualizationMode(TEXT("Overview"), LOCTEXT("Overview", "Overview"), FModeType::Overview, NANITE_VISUALIZE_OVERVIEW, true);

		AddVisualizationMode(TEXT("Mask"), LOCTEXT("Mask", "Mask"), FModeType::Standard, NANITE_VISUALIZE_NANITE_MASK, true);
		AddVisualizationMode(TEXT("Triangles"), LOCTEXT("Triangles", "Triangles"), FModeType::Standard, NANITE_VISUALIZE_TRIANGLES, true);
		AddVisualizationMode(TEXT("Patches"), LOCTEXT("Patches", "Patches"), FModeType::Standard, NANITE_VISUALIZE_PATCHES, true);
		AddVisualizationMode(TEXT("Clusters"), LOCTEXT("Clusters", "Clusters"), FModeType::Standard, NANITE_VISUALIZE_CLUSTERS, true);
		AddVisualizationMode(TEXT("Primitives"), LOCTEXT("Primitives", "Primitives"), FModeType::Standard, NANITE_VISUALIZE_PRIMITIVES, true);
		AddVisualizationMode(TEXT("Instances"), LOCTEXT("Instances", "Instances"), FModeType::Standard, NANITE_VISUALIZE_INSTANCES, true);
		AddVisualizationMode(TEXT("Overdraw"), LOCTEXT("Overdraw", "Overdraw"), FModeType::Standard, NANITE_VISUALIZE_OVERDRAW, false);
		AddVisualizationMode(TEXT("MaterialID"), LOCTEXT("MaterialID", "Material ID"), FModeType::Standard, NANITE_VISUALIZE_MATERIAL_DEPTH, true);
		AddVisualizationMode(TEXT("LightmapUV"), LOCTEXT("LightmapUV", "Lightmap UV"), FModeType::Standard, NANITE_VISUALIZE_LIGHTMAP_UVS, true);
		AddVisualizationMode(TEXT("EvaluateWPO"), LOCTEXT("EvaluateWPO", "Evaluate WPO"), FModeType::Standard, NANITE_VISUALIZE_EVALUATE_WORLD_POSITION_OFFSET, true);
		AddVisualizationMode(TEXT("PixelProgrammable"), LOCTEXT("PixelProgrammable", "Pixel Programmable"), FModeType::Standard, NANITE_VISUALIZE_PIXEL_PROGRAMMABLE_RASTER, true);
		
		AddVisualizationMode(TEXT("Picking"), LOCTEXT("Picking", "Picking"), FModeType::Advanced, NANITE_VISUALIZE_PICKING, true);
		AddVisualizationMode(TEXT("Groups"), LOCTEXT("Groups", "Groups"), FModeType::Advanced, NANITE_VISUALIZE_GROUPS, true);
		AddVisualizationMode(TEXT("Pages"), LOCTEXT("Pages", "Pages"), FModeType::Advanced, NANITE_VISUALIZE_PAGES, true);
		AddVisualizationMode(TEXT("Hierarchy"), LOCTEXT("Hierarchy", "Hierarchy"), FModeType::Advanced, NANITE_VISUALIZE_HIERARCHY_OFFSET, true);
		AddVisualizationMode(TEXT("RasterMode"), LOCTEXT("RasterMode", "Raster Mode"), FModeType::Advanced, NANITE_VISUALIZE_RASTER_MODE, true);
		AddVisualizationMode(TEXT("RasterBins"), LOCTEXT("RasterBins", "Raster Bins"), FModeType::Advanced, NANITE_VISUALIZE_RASTER_BINS, true);
		AddVisualizationMode(TEXT("ShadingBins"), LOCTEXT("ShadingBins", "Shading Bins"), FModeType::Advanced, NANITE_VISUALIZE_SHADING_BINS, true);
		AddVisualizationMode(TEXT("SceneZMin"), LOCTEXT("SceneZMin", "Scene Z Min"), FModeType::Advanced, NANITE_VISUALIZE_SCENE_Z_MIN, true);
		AddVisualizationMode(TEXT("SceneZMax"), LOCTEXT("SceneZMax", "Scene Z Max"), FModeType::Advanced, NANITE_VISUALIZE_SCENE_Z_MAX, true);
		AddVisualizationMode(TEXT("SceneZDelta"), LOCTEXT("SceneZDelta", "Scene Z Delta"), FModeType::Advanced, NANITE_VISUALIZE_SCENE_Z_DELTA, true);
		AddVisualizationMode(TEXT("SceneZDecoded"), LOCTEXT("SceneZDecoded", "Scene Z Decoded"), FModeType::Advanced, NANITE_VISUALIZE_SCENE_Z_DECODED, true);
		AddVisualizationMode(TEXT("MaterialZMin"), LOCTEXT("MaterialZMin", "Material Z Min"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_Z_MIN, true);
		AddVisualizationMode(TEXT("MaterialZMax"), LOCTEXT("MaterialZMax", "Material Z Max"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_Z_MAX, true);
		AddVisualizationMode(TEXT("MaterialZDelta"), LOCTEXT("MaterialZDelta", "Material Z Delta"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_Z_DELTA, true);
		AddVisualizationMode(TEXT("MaterialZDecoded"), LOCTEXT("MaterialZDecoded", "Material Z Decoded"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_Z_DECODED, true);
		AddVisualizationMode(TEXT("MaterialCount"), LOCTEXT("MaterialCount", "Material Count"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_COUNT, true);
		AddVisualizationMode(TEXT("MaterialMode"), LOCTEXT("MaterialMode", "Material Mode"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_MODE, true);
		AddVisualizationMode(TEXT("MaterialIndex"), LOCTEXT("MaterialIndex", "Material Index"), FModeType::Advanced, NANITE_VISUALIZE_MATERIAL_INDEX, true);
		AddVisualizationMode(TEXT("HitProxyID"), LOCTEXT("HitProxyID", "Hit Proxy ID"), FModeType::Advanced, NANITE_VISUALIZE_HIT_PROXY_DEPTH, true);
		AddVisualizationMode(TEXT("LightmapUVIndex"), LOCTEXT("LightmapUVIndex", "Lightmap UV Index"), FModeType::Advanced, NANITE_VISUALIZE_LIGHTMAP_UV_INDEX, true);
		AddVisualizationMode(TEXT("LightmapDataIndex"), LOCTEXT("LightmapDataIndex", "Lightmap Data Index"), FModeType::Advanced, NANITE_VISUALIZE_LIGHTMAP_DATA_INDEX, true);
		AddVisualizationMode(TEXT("PositionBits"), LOCTEXT("PositionBits", "Position Bits"), FModeType::Advanced, NANITE_VISUALIZE_POSITION_BITS, true);
		AddVisualizationMode(TEXT("VSMStatic"), LOCTEXT("VSMStatic", "Virtual Shadow Map Static"), FModeType::Advanced, NANITE_VISUALIZE_VSM_STATIC_CACHING, true);
		AddVisualizationMode(TEXT("ShadingWriteMask"), LOCTEXT("ShadingWriteMask", "Shading Write Mask"), FModeType::Advanced, NANITE_VISUALIZE_SHADING_WRITE_MASK, true);
		AddVisualizationMode(TEXT("NoDerivativeOps"), LOCTEXT("NoDerivativeOps", "No Derivative Ops"), FModeType::Advanced, NANITE_VISUALIZE_NO_DERIVATIVE_OPS, true);
		AddVisualizationMode(TEXT("FastClearTiles"), LOCTEXT("FastClearTiles", "Fast Clear Tiles"), FModeType::Advanced, NANITE_VISUALIZE_FAST_CLEAR_TILES, true);
		AddVisualizationMode(TEXT("Tessellation"), LOCTEXT("Tessellation", "Tessellation"), FModeType::Advanced, NANITE_VISUALIZE_TESSELLATION, true);
		AddVisualizationMode(TEXT("DisplacementScale"), LOCTEXT("DisplacementScale", "DisplacementScale"), FModeType::Advanced, NANITE_VISUALIZE_DISPLACEMENT_SCALE, true);

		ConfigureConsoleCommand();

		bIsInitialized = true;
	}
}

void FNaniteVisualizationData::ConfigureConsoleCommand()
{
	FString AvailableVisualizationModes;
	for (TModeMap::TConstIterator It = ModeMap.CreateConstIterator(); It; ++It)
	{
		const FModeRecord& Record = It.Value();
		AvailableVisualizationModes += FString(TEXT("\n  "));
		AvailableVisualizationModes += Record.ModeString;
	}

	ConsoleDocumentationVisualizationMode = TEXT("When the viewport view-mode is set to 'Nanite Visualization', this command specifies which of the various channels to display. Values entered other than the allowed values shown below will be ignored.");
	ConsoleDocumentationVisualizationMode += AvailableVisualizationModes;

	IConsoleManager::Get().RegisterConsoleVariable(
		GetVisualizeConsoleCommandName(),
		TEXT(""),
		*ConsoleDocumentationVisualizationMode,
		ECVF_Cheat);

	ConsoleDocumentationOverviewTargets = TEXT("Specify the list of modes that can be used in the Nanite visualization overview. Put nothing between the commas to leave a gap.\n\n\tChoose from:\n");
	ConsoleDocumentationOverviewTargets += AvailableVisualizationModes;

	IConsoleManager::Get().RegisterConsoleVariable(
		GetOverviewConsoleCommandName(),
		TEXT("Triangles,Clusters,Instances,Primitives,,,,,,,,,Overdraw,MaterialID,RasterBins,EvaluateWPO"),
		//TEXT("Triangles,Clusters,Instances,Primitives"),
		*ConsoleDocumentationOverviewTargets,
		ECVF_Default
	);
}

void FNaniteVisualizationData::AddVisualizationMode(
	const TCHAR* ModeString,
	const FText& ModeText,
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
	Record.ModeDesc				= FText::GetEmpty();
	Record.ModeType				= ModeType;
	Record.ModeID				= ModeID;
	Record.DefaultComposited	= DefaultComposited;
}

void FNaniteVisualizationData::SetActiveMode(int32 ModeID, const FName& ModeName, bool bDefaultComposited)
{
	ActiveVisualizationModeID = ModeID;
	ActiveVisualizationModeName = ModeName;
	bActiveVisualizationModeComposited = bDefaultComposited;
}

bool FNaniteVisualizationData::IsActive() const
{
	if (!IsInitialized())
	{
		return false;
	}

	if (GetActiveModeID() == INDEX_NONE)
	{
		return false;
	}
	
	if (GetActiveModeID() == NANITE_VISUALIZE_OVERVIEW && bOverviewListEmpty)
	{
		return false;
	}
	
	return true;
}

bool FNaniteVisualizationData::Update(const FName& InViewMode)
{
	bool bForceShowFlag = false;

	if (IsInitialized())
	{
		SetActiveMode(INDEX_NONE, NAME_None, true);

		// Check if overview has a configured mode list so it can be parsed and cached.
		static IConsoleVariable* ICVarOverview = IConsoleManager::Get().FindConsoleVariable(GetOverviewConsoleCommandName());
		if (ICVarOverview)
		{
			FString OverviewModeList = ICVarOverview->GetString();
			if (IsDifferentToCurrentOverviewModeList(OverviewModeList))
			{
				FString Left, Right;

				// Update our record of the list of modes we've been asked to display
				SetCurrentOverviewModeList(OverviewModeList);
				CurrentOverviewModeNames.Reset();
				CurrentOverviewModeIDs.Reset();
				bOverviewListEmpty = true;

				// Extract each mode name from the comma separated string
				while (OverviewModeList.Len())
				{
					// Detect last entry in the list
					if (!OverviewModeList.Split(TEXT(","), &Left, &Right))
					{
						Left = OverviewModeList;
						Right = FString();
					}

					// Look up the mode ID for this name
					Left.TrimStartInline();

					const FName ModeName = FName(*Left);
					const int32 ModeID = GetModeID(ModeName);

					if (!Left.IsEmpty() && ModeID == INDEX_NONE)
					{
						UE_LOG(LogNaniteVisualization, Warning, TEXT("Unknown Nanite visualization mode '%s'"), *Left);
					}
					else
					{
						if (ModeID == INDEX_NONE)
						{
							// Placeholder entry to keep indices static for tile layout
							CurrentOverviewModeIDs.Emplace(0xFFFFFFFF);
						}
						else
						{
							CurrentOverviewModeIDs.Emplace(ModeID);
							bOverviewListEmpty = false;
						}

						CurrentOverviewModeNames.Emplace(ModeName);
					}

					OverviewModeList = Right;
				}
			}
		}

		// Check if the console command is set (overrides the editor)
		if (ActiveVisualizationModeID == INDEX_NONE)
		{
			static IConsoleVariable* ICVarVisualize = IConsoleManager::Get().FindConsoleVariable(GetVisualizeConsoleCommandName());
			if (ICVarVisualize)
			{
				const FString ConsoleVisualizationMode = ICVarVisualize->GetString();
				const bool bDisable = ConsoleVisualizationMode == TEXT("off") || ConsoleVisualizationMode == TEXT("none");

				if (!ConsoleVisualizationMode.IsEmpty() && !bDisable)
				{
					const FName  ModeName = FName(*ConsoleVisualizationMode);
					const int32  ModeID   = GetModeID(ModeName);
					if (ModeID == INDEX_NONE)
					{
						UE_LOG(LogNaniteVisualization, Warning, TEXT("Unknown Nanite visualization mode '%s'"), *ConsoleVisualizationMode);
					}
					else
					{
						SetActiveMode(ModeID, ModeName, GetModeDefaultComposited(ModeName));
						bForceShowFlag = true;
					}
				}
			}
		}

		// Check the view mode state (set by editor).
		if (ActiveVisualizationModeID == INDEX_NONE && InViewMode != NAME_None)
		{
			const int32 ModeID = GetModeID(InViewMode);
			if (ensure(ModeID != INDEX_NONE))
			{
				SetActiveMode(ModeID, InViewMode, GetModeDefaultComposited(InViewMode));
			}
		}
	}

	return bForceShowFlag;
}

FText FNaniteVisualizationData::GetModeDisplayName(const FName& InModeName) const
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

int32 FNaniteVisualizationData::GetModeID(const FName& InModeName) const
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

bool FNaniteVisualizationData::GetModeDefaultComposited(const FName& InModeName) const
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

void FNaniteVisualizationData::SetCurrentOverviewModeList(const FString& InNameList)
{
	CurrentOverviewModeList = InNameList;
}

bool FNaniteVisualizationData::IsDifferentToCurrentOverviewModeList(const FString& InNameList)
{
	return InNameList != CurrentOverviewModeList;
}

FNaniteVisualizationData& GetNaniteVisualizationData()
{
	if (!GNaniteVisualizationData.IsInitialized())
	{
		GNaniteVisualizationData.Initialize();
	}

	return GNaniteVisualizationData;
}

void FNaniteVisualizationData::Pick(UWorld* World)
{
	if (!IsActive() || GetActiveModeID() != NANITE_VISUALIZE_PICKING)
	{
		return;
	}

	FVector2f NaniteDebugMousePos = FVector2f::ZeroVector;
	FIntPoint NaniteDebugScreenSize = FIntPoint(0, 0);

	bool bValidPosition = false;

	if (World && World->GetNumPlayerControllers() > 0)
	{
		APlayerController* Controller = World->GetFirstPlayerController();
		Controller->GetMousePosition(NaniteDebugMousePos.X, NaniteDebugMousePos.Y);
		Controller->GetViewportSize(NaniteDebugScreenSize.X, NaniteDebugScreenSize.Y);
		bValidPosition = true;
	}
#if WITH_EDITORONLY_DATA
	// While in the editor we don't necessarily have a player controller, so we query the viewport object instead
	else if (GCurrentLevelEditingViewportClient)
	{
		if (GCurrentLevelEditingViewportClient->Viewport->IsCursorVisible())
		{
			NaniteDebugMousePos.X = GCurrentLevelEditingViewportClient->Viewport->GetMouseX();
			NaniteDebugMousePos.Y = GCurrentLevelEditingViewportClient->Viewport->GetMouseY();
			bValidPosition = true;
		}
		
		NaniteDebugScreenSize = GCurrentLevelEditingViewportClient->Viewport->GetSizeXY();
	}
#endif

	if (!bValidPosition || GNanitePickingCrosshair != 0)
	{
		NaniteDebugMousePos.X = NaniteDebugScreenSize.X >> 1u;
		NaniteDebugMousePos.Y = NaniteDebugScreenSize.Y >> 1u;
	}

	ENQUEUE_RENDER_COMMAND(UpdateNaniteDebugPicking)(
	[this, NaniteDebugMousePos, NaniteDebugScreenSize](FRHICommandList& RHICmdList)
	{
		MousePos = NaniteDebugMousePos;
		ScreenSize = NaniteDebugScreenSize;
	});
}

#undef LOCTEXT_NAMESPACE
