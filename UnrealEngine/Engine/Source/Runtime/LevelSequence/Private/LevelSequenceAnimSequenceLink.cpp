// Copyright Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceAnimSequenceLink.h"
#include "Animation/AnimSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LevelSequenceAnimSequenceLink)


void FLevelSequenceAnimSequenceLinkItem::SetAnimSequence(UAnimSequence* InAnimSequence)
{
	PathToAnimSequence = FSoftObjectPath(InAnimSequence);
}

UAnimSequence* FLevelSequenceAnimSequenceLinkItem::ResolveAnimSequence()
{
	UObject *Object = PathToAnimSequence.TryLoad();
	return Cast<UAnimSequence>(Object);
}


ULevelSequenceAnimSequenceLink::ULevelSequenceAnimSequenceLink(class FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{

}
