// Copyright Epic Games, Inc. All Rights Reserved.

#include "StrataVisualizationData.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "Misc/ConfigCacheIni.h"
#include "RenderUtils.h"

#define LOCTEXT_NAMESPACE "FStrataVisualizationData"

static FStrataVisualizationData GStrataVisualizationData;

static FString ConfigureConsoleCommand(FStrataVisualizationData::TModeMap& ModeMap)
{
	FString AvailableVisualizationModes;
	for (FStrataVisualizationData::TModeMap::TConstIterator It = ModeMap.CreateConstIterator(); It; ++It)
	{
		const FStrataVisualizationData::FModeRecord& Record = It.Value();
		AvailableVisualizationModes += FString(TEXT("\n  Value="));
		AvailableVisualizationModes += FString::Printf(TEXT("%d: "), uint8(Record.Mode));
		AvailableVisualizationModes += Record.ModeString;
		if (!Record.bAvailableCommand)
		{
			AvailableVisualizationModes += FString::Printf(TEXT(" --- Unavailable, reason: %s"), *Record.UnavailableReason.ToString());
		}
	}

	FString Out;
	Out = TEXT("When the viewport view-mode is set to 'Substrate Visualization', this command specifies which of the various channels to display. Values entered other than the allowed values shown below will be ignored.");
	Out += AvailableVisualizationModes;

	IConsoleManager::Get().RegisterConsoleVariable(
		FStrataVisualizationData::GetVisualizeConsoleCommandName(),
		0,
		*Out,
		ECVF_Cheat);

	return Out;
}

static void AddVisualizationMode(
	FStrataVisualizationData::TModeMap& ModeMap,
	const TCHAR* ModeString,
	const FText& ModeText,
	const FText& ModeDesc,
	const FStrataVisualizationData::FViewMode Mode,
	bool bDefaultComposited,
	bool bAvailableCommand,
	const FText& UnavailableReason
)
{
	const FName ModeName = FName(ModeString);

	FStrataVisualizationData::FModeRecord& Record = ModeMap.Emplace(ModeName);
	Record.ModeString			= FString(ModeString);
	Record.ModeName				= ModeName;
	Record.ModeText				= ModeText;
	Record.ModeDesc				= ModeDesc;
	Record.Mode					= Mode;
	Record.bDefaultComposited	= bDefaultComposited;
	Record.bAvailableCommand	= bAvailableCommand;
	Record.UnavailableReason	= UnavailableReason;
}

void FStrataVisualizationData::Initialize()
{
	if (!bIsInitialized && Strata::IsStrataEnabled())
	{
		TModeMap AllModeMap;

		AddVisualizationMode(
			AllModeMap,
			TEXT("MaterialProperties"),
			LOCTEXT("MaterialProperties", "Material Properties"),
			LOCTEXT("MaterialPropertiesDesc", "Visualizes Substrate material properties under mouse cursor"),
			FViewMode::MaterialProperties,
			true,
			true,
			LOCTEXT("None", "None"));

		AddVisualizationMode(
			AllModeMap,
			TEXT("MaterialCount"),
			LOCTEXT("MaterialCount", "Material Count"),
			LOCTEXT("MaterialCountDesc", "Visualizes Substrate material count per pixel"),
			FViewMode::MaterialCount,
			true,
			true,
			LOCTEXT("None", "None"));

		AddVisualizationMode(
			AllModeMap,
			TEXT("AdvancedMaterialProperties"),
			LOCTEXT("AdvancedMaterialProperties", "Advanced Material Properties"),
			LOCTEXT("AdvancedMaterialPropertiesDesc", "Visualizes Substrate advanced material properties"),
			FViewMode::AdvancedMaterialProperties,
			true,
			Strata::IsAdvancedVisualizationEnabled(),
			LOCTEXT("IsSubstrateAdvancedDebugShaderEnabled", "Substrate advanced debugging r.Substrate.Debug.AdvancedVisualizationShaders is disabled"));

		AddVisualizationMode(
			AllModeMap,
			TEXT("MaterialClassification"),
			LOCTEXT("MaterialClassification", "Material Classification"),
			LOCTEXT("MaterialClassificationDesc", "Visualizes Substrate material classification"),
			FViewMode::MaterialClassification,
			true,
			true,
			LOCTEXT("None", "None"));

		AddVisualizationMode(
			AllModeMap,
			TEXT("DecalClassification"),
			LOCTEXT("DecalClassification", "Decal classification"),
			LOCTEXT("DecalClassificationDesc", "Visualizes Substrate decal classification"),
			FViewMode::DecalClassification,
			true,
			false, // Disable for now, as it is not important, and is mainly used for debugging
			LOCTEXT("IsSubstrateBufferPassEnabled", "Substrate tiled DBuffer pass (r.Substrate.DBufferPass and r.Substrate.DBufferPass.DedicatedTiles) is disabled"));

		AddVisualizationMode(
			AllModeMap,
			TEXT("RoughRefractionClassification"),
			LOCTEXT("RoughRefractionClassification", "Rough Refraction Classification"),
			LOCTEXT("RoughRefractionClassificationDesc", "Visualizes Substrate rough refraction classification"),
			FViewMode::RoughRefractionClassification,
			true,
			Strata::IsOpaqueRoughRefractionEnabled(),
			LOCTEXT("IsSubstrateRoughRefractionEnabled", "Substrate rough refraction r.Substrate.OpaqueMaterialRoughRefraction is disabled"));

		AddVisualizationMode(
			AllModeMap,
			TEXT("SubstrateInfo"),
			LOCTEXT("SubstrateInfo", "Substrate Info"),
			LOCTEXT("SubstrateInfoDesc", "Visualizes Substrate info"),
			FViewMode::StrataInfo,
			true,
			true,
			LOCTEXT("None", "None"));

		ConsoleDocumentationVisualizationMode = ConfigureConsoleCommand(AllModeMap);

		// Now only copy the available modes for the menu to not overload it with useless entries.
		for (auto& Mode : AllModeMap)
		{
			if(Mode.Value.bAvailableCommand)
			{
				ModeMap.Emplace(Mode.Key) = Mode.Value;
			}
		}
	}
	bIsInitialized = true;
}

FText FStrataVisualizationData::GetModeDisplayName(const FName& InModeName) const
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

FStrataVisualizationData::FViewMode FStrataVisualizationData::GetViewMode(const FName& InModeName) const
{
	if (const FModeRecord* Record = ModeMap.Find(InModeName))
	{
		return Record->Mode;
	}
	else
	{
		return FStrataVisualizationData::FViewMode::None;
	}
}

bool FStrataVisualizationData::GetModeDefaultComposited(const FName& InModeName) const
{
	if (const FModeRecord* Record = ModeMap.Find(InModeName))
	{
		return Record->bDefaultComposited;
	}
	else
	{
		return false;
	}
}

FStrataVisualizationData& GetStrataVisualizationData()
{
	if (!GStrataVisualizationData.IsInitialized())
	{
		GStrataVisualizationData.Initialize();
	}

	return GStrataVisualizationData;
}

#undef LOCTEXT_NAMESPACE
