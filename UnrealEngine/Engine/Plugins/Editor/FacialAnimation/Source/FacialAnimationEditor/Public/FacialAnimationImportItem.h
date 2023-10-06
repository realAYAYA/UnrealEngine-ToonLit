// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FFacialAnimationImportItem
{
	bool Import();

private:
	class USoundWave* ImportSoundWave(const FString& InSoundWavePackageName, const FString& InSoundWaveAssetName, const FString& InWavFilename) const;
	bool ImportCurvesEmbeddedInSoundWave();

public:
	FString FbxFile;
	FString WaveFile;
	FString TargetPackageName;
	FString TargetAssetName;
};