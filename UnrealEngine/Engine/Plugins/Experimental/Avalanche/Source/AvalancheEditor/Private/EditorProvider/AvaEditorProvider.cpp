// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaEditorProvider.h"
#include "AvaEditorActorUtils.h"
#include "AvaScene.h"
#include "Engine/World.h"
#include "IAvaEditor.h"

UObject* FAvaEditorProvider::GetSceneObject(UWorld* InWorld, EAvaEditorObjectQueryType InQueryType)
{
	if (InWorld)
	{
		const bool bCreateSceneIfNotFound = InQueryType == EAvaEditorObjectQueryType::CreateIfNotFound;
		return AAvaScene::GetScene(InWorld->PersistentLevel, bCreateSceneIfNotFound);
	}
	return nullptr;
}

void FAvaEditorProvider::GetActorsToEdit(TArray<AActor*>& InOutActorsToEdit) const
{
	FAvaEditorActorUtils::GetActorsToEdit(InOutActorsToEdit);
}
