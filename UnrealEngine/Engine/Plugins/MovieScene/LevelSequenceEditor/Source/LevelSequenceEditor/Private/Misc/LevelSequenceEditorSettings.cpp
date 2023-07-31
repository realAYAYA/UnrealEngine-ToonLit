// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/LevelSequenceEditorSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequenceEditorSettings)

ULevelSequenceEditorSettings::ULevelSequenceEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoBindToSimulate = true;
	bAutoBindToPIE      = true;
}

ULevelSequenceMasterSequenceSettings::ULevelSequenceMasterSequenceSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MasterSequenceName(TEXT("Sequence"))
	, MasterSequenceSuffix(TEXT("Master"))
	, MasterSequenceNumShots(5)
{
	MasterSequenceBasePath.Path = TEXT("/Game/Cinematics/Sequences");
}

