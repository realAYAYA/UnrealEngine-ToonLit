// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithImportedSequencesActor.h"
#include "CoreMinimal.h"
#include "LevelSequence.h"

ADatasmithImportedSequencesActor::ADatasmithImportedSequencesActor(const FObjectInitializer& Init)
	: Super(Init)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;
}

void ADatasmithImportedSequencesActor::PlayLevelSequence(ULevelSequence* SequenceToPlay)
{
    if(SequenceToPlay)
    {
        UE_LOG(LogTemp, Log, TEXT("Play %s"), *SequenceToPlay->GetName());
    }
}