// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDisplaySlaveVirtualSubject.h"

#include "LiveLinkOverNDisplayPrivate.h"
#include "LiveLinkSubjectSettings.h"



void UNDisplaySlaveVirtualSubject::Update()
{
	UpdateTranslatorsForThisFrame();
}

void UNDisplaySlaveVirtualSubject::UpdateFrameData(FLiveLinkFrameDataStruct&& NewFrameData)
{
	UpdateFrameDataSnapshot(MoveTemp(NewFrameData));
}

void UNDisplaySlaveVirtualSubject::SetTrackedSubjectInfo(const FLiveLinkSubjectKey& InSubjectKey, TSubclassOf<ULiveLinkRole> InRole)
{
	AssociatedSubject = InSubjectKey;
	Role = InRole;
}

void UNDisplaySlaveVirtualSubject::UpdateTranslators(const TArray<ULiveLinkFrameTranslator*>& SourceTranslators)
{
	for (const ULiveLinkFrameTranslator* SourceTranslator : SourceTranslators)
	{
		ULiveLinkFrameTranslator* NewTranslator = DuplicateObject(SourceTranslator, this);
		FrameTranslators.Add(NewTranslator);
	}
}
