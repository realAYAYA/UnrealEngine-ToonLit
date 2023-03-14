// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WaveformEditorToolMenuContext.generated.h"

class FWaveformEditor;

UCLASS()
class WAVEFORMEDITOR_API UWaveformEditorToolMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<FWaveformEditor> WaveformEditor;
};