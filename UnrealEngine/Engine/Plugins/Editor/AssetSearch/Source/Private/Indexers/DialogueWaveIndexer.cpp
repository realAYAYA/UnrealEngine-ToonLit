// Copyright Epic Games, Inc. All Rights Reserved.

#include "DialogueWaveIndexer.h"
#include "Utility/IndexerUtilities.h"
#include "Engine/DataAsset.h"
#include "Sound/DialogueWave.h"
#include "SearchSerializer.h"

enum class EDialogueWaveIndexerVersion
{
	Empty,
	Initial,

	// -----<new versions can be added above this line>-------------------------------------------------
	VersionPlusOne,
	LatestVersion = VersionPlusOne - 1
};

int32 FDialogueWaveIndexer::GetVersion() const
{
	return (int32)EDialogueWaveIndexerVersion::LatestVersion;
}

void FDialogueWaveIndexer::IndexAsset(const UObject* InAssetObject, FSearchSerializer& Serializer) const
{
	const UDialogueWave* DialogueWave = CastChecked<UDialogueWave>(InAssetObject);

	Serializer.BeginIndexingObject(DialogueWave, TEXT("$self"));

	Serializer.IndexProperty(TEXT("SpokenText"), DialogueWave->SpokenText);

	Serializer.EndIndexingObject();
}