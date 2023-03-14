// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterPlayerInput.h"
#include "Components/InputComponent.h"

#include "Cluster/IPDisplayClusterClusterManager.h"

#include "Misc/DisplayClusterGlobals.h"
#include "Misc/DisplayClusterHelpers.h"
#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterTypesConverter.h"

#include "Engine/World.h"


UDisplayClusterPlayerInput::UDisplayClusterPlayerInput()
	: Super()
{
}

void UDisplayClusterPlayerInput::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	UE_LOG(LogDisplayClusterGame, Verbose, TEXT("Processing input stack..."));

	if (IPDisplayClusterClusterManager* const ClusterMgr = GDisplayCluster->GetPrivateClusterMgr())
	{
		TMap<FString, FString> KeyStates;

		if (ClusterMgr->IsPrimary())
		{
			// Export key states data for the stack
			SerializeKeyStateMap(KeyStates);
			ClusterMgr->ImportNativeInputData(MoveTemp(KeyStates));
		}
		else
		{
			// Import key states data to the stack
			ClusterMgr->ExportNativeInputData(KeyStates);
			DeserializeKeyStateMap(KeyStates);
		}
	}

	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);
}

bool UDisplayClusterPlayerInput::SerializeKeyStateMap(TMap<FString, FString>& OutKeyStateMap)
{
	const TMap<FKey, FKeyState>& StateMap = GetKeyStateMap();
	for (auto it = StateMap.CreateConstIterator(); it; ++it)
	{
		const FString KeyName = it->Key.ToString();

		FString StrKeyState;
		StrKeyState.Reserve(2048);

		StrKeyState = FString::Printf(TEXT("%s;%s;%s;%s;%s;%s;%s;%s;%s;"),
			*DisplayClusterTypesConverter::template ToHexString(it->Value.RawValue),
			*DisplayClusterTypesConverter::template ToHexString(it->Value.Value),
			*DisplayClusterTypesConverter::template ToHexString(it->Value.LastUpDownTransitionTime),
			*DisplayClusterHelpers::str::BoolToStr(it->Value.bDown ? true : false, false),
			*DisplayClusterHelpers::str::BoolToStr(it->Value.bDownPrevious ? true : false, false),
			*DisplayClusterHelpers::str::BoolToStr(it->Value.bConsumed ? true : false, false),
			*DisplayClusterTypesConverter::template ToString(it->Value.PairSampledAxes),
			*DisplayClusterTypesConverter::template ToString(it->Value.SampleCountAccumulator),
			*DisplayClusterTypesConverter::template ToHexString(it->Value.RawValueAccumulator));

		for (int i = 0; i < IE_MAX; ++i)
		{
			StrKeyState += FString::Printf(TEXT("%s;"), *DisplayClusterHelpers::str::template ArrayToStr(it->Value.EventCounts[i], FString(","), false));
		}

		for (int i = 0; i < IE_MAX; ++i)
		{
			StrKeyState += FString::Printf(TEXT("%s;"), *DisplayClusterHelpers::str::template ArrayToStr(it->Value.EventAccumulator[i], FString(","), false));
		}

		OutKeyStateMap.Emplace(KeyName, StrKeyState);
	}

	return true;
}

bool UDisplayClusterPlayerInput::DeserializeKeyStateMap(const TMap<FString, FString>& InKeyStateMap)
{
	TMap<FKey, FKeyState>& StateMap = GetKeyStateMap();

	// Reset local key state map
	StateMap.Reset();

	int idx = 0;
	for (auto it = InKeyStateMap.CreateConstIterator(); it; ++it)
	{
		UE_LOG(LogDisplayClusterGame, VeryVerbose, TEXT("Input data [%d]: %s = %s"), idx, *it->Key, *it->Value);
		++idx;

		FKey Key(*it->Key);
		FKeyState KeyState;

		TArray<FString> Fields;
		DisplayClusterHelpers::str::template StrToArray<FString>(it->Value, FString(";"), Fields, false);

		KeyState.RawValue                 = DisplayClusterTypesConverter::template FromHexString<FVector>(Fields[0]);
		KeyState.Value                    = DisplayClusterTypesConverter::template FromHexString<FVector>(Fields[1]);
		KeyState.LastUpDownTransitionTime = DisplayClusterTypesConverter::template FromHexString<float>(Fields[2]);
		KeyState.bDown                    = DisplayClusterTypesConverter::template FromString<bool>(Fields[3]) ? 1 : 0;
		KeyState.bDownPrevious            = DisplayClusterTypesConverter::template FromString<bool>(Fields[4]) ? 1 : 0;
		KeyState.bConsumed                = DisplayClusterTypesConverter::template FromString<bool>(Fields[5]) ? 1 : 0;
		KeyState.PairSampledAxes          = DisplayClusterTypesConverter::template FromString<uint8>(Fields[6]);
		KeyState.SampleCountAccumulator   = DisplayClusterTypesConverter::template FromString<uint8>(Fields[7]);
		KeyState.RawValueAccumulator      = DisplayClusterTypesConverter::template FromHexString<FVector>(Fields[8]);

		for (int i = 0; i < IE_MAX; ++i)
		{
			const FString& EventCountsStr = Fields[9 + i];
			if (!EventCountsStr.IsEmpty())
			{
				DisplayClusterHelpers::str::template StrToArray<uint32>(EventCountsStr, FString(","), KeyState.EventCounts[i]);
			}

			const FString& EventAccumulatorStr = Fields[9 + IE_MAX + i];
			if (!EventAccumulatorStr.IsEmpty())
			{
				DisplayClusterHelpers::str::template StrToArray<uint32>(Fields[9 + IE_MAX + i], FString(","), KeyState.EventAccumulator[i]);
			}
		}

		// Add incoming data to the local map
		StateMap.Add(Key, KeyState);
	}

	return true;
}
