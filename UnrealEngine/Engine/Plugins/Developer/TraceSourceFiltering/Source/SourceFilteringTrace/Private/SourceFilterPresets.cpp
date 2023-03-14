// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceFilterPresets.h"

#include "HAL/IConsoleManager.h"
#include "Internationalization/Text.h"
#include "Logging/LogMacros.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"

#include "TraceSourceFilteringProjectSettings.h"
#include "TraceSourceFiltering.h"
#include "SourceFilterTrace.h"

#if SOURCE_FILTER_TRACE_ENABLED
static FAutoConsoleCommand GListAvailablePresetsCommand(
	TEXT("Trace.ListPresets"),
	TEXT("Lists all the available filtering presets"),
	FConsoleCommandDelegate::CreateStatic(&FSourceFilterPresets::ListAvailablePresets)
);

static FAutoConsoleCommand GLoadPresetCommand(
	TEXT("Trace.LoadPreset"),
	TEXT("Loads a specific filtering preset\n")
	TEXT("Argument:\n")
	TEXT("Integer: index of preset within Trace.ListPresets output\n")
	TEXT("or\n")
	TEXT("String: path to UTraceFilterCollection object\n"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FSourceFilterPresets::LoadPresetCommand)
);
#endif // SOURCE_FILTER_TRACE_ENABLED

DEFINE_LOG_CATEGORY_STATIC(SourceFilterPresets, Display, Display);

void FSourceFilterPresets::ListAvailablePresets()
{
	TArray<FAssetData> PresetAssetData;
	GetPresets(PresetAssetData);

	for (int32 AssetDataIndex = 0; AssetDataIndex < PresetAssetData.Num(); ++AssetDataIndex)
	{
		const FAssetData& AssetData = PresetAssetData[AssetDataIndex];
		UE_LOG(SourceFilterPresets, Display, TEXT("[%i]: %s\n"), AssetDataIndex, *AssetData.GetExportTextName());
	}
}

void FSourceFilterPresets::GetPresets(TArray<FAssetData>& InOutPresetAssetData)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TMultiMap<FName, FString> TagValues = { { FBlueprintTags::NativeParentClassPath, FObjectPropertyBase::GetExportPath(USourceFilterCollection::StaticClass()) } };
	AssetRegistryModule.Get().GetAssetsByClass(USourceFilterCollection::StaticClass()->GetClassPathName(), InOutPresetAssetData);
}

void FSourceFilterPresets::LoadPreset(const FSoftObjectPath& PresetPath)
{
	if (USourceFilterCollection* Preset = Cast<USourceFilterCollection>(PresetPath.TryLoad()))
	{
		FTraceSourceFiltering::Get().GetFilterCollection()->CopyData(Preset);

		UE_LOG(SourceFilterPresets, Display, TEXT("Loaded Trace Source Filter preset: %s\n"), *PresetPath.ToString());
	}
	else
	{
		// Unable to load
		UE_LOG(SourceFilterPresets, Display, TEXT("Failed to find preset: %s\n"), *PresetPath.ToString());
	}
}

void FSourceFilterPresets::LoadPresetCommand(const TArray<FString>& Arguments)
{
	// Only expecting a single argument
	if (Arguments.Num() == 1)
	{
		int32 Index = 0;
		if (LexTryParseString(Index, *Arguments[0]))
		{
			TArray<FAssetData> PresetAssetData;
			GetPresets(PresetAssetData);

			// Retrieve specific asset data at user index
			if (PresetAssetData.IsValidIndex(Index))
			{
				LoadPreset(PresetAssetData[Index].GetObjectPathString());
			}
			else
			{
				UE_LOG(SourceFilterPresets, Display, TEXT("Invalid preset index\n"));
				ListAvailablePresets();
			}
		}
		else
		{
			FSoftObjectPath Path(Arguments[0]);
			if (Path.IsValid())
			{
				LoadPreset(Path);
			}
		}
	}
	else
	{
		UE_LOG(SourceFilterPresets, Display, TEXT("Expecting a single argument:\n")
		TEXT("Integer: index of preset within Trace.ListPresets output\n")
		TEXT("or\n"),
		TEXT("String: path to UTraceFilterCollection object\n"));
	}
}
