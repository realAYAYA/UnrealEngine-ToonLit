// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSceneSubsystem.h"
#include "AvaScene.h"
#include "Containers/Set.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "IAvaSceneInterface.h"

namespace UE::Ava::Private
{
	static const TSet<EWorldType::Type> GUnsupportedWorlds
		{
			EWorldType::Type::None,
			EWorldType::Type::Inactive,
			EWorldType::Type::EditorPreview,
		};
}

void UAvaSceneSubsystem::RegisterSceneInterface(ULevel* InLevel, IAvaSceneInterface* InSceneInterface)
{
	// Replace the existing Interface with new one for the given Level
	SceneInterfaces.Add(InLevel, InSceneInterface);
}

IAvaSceneInterface* UAvaSceneSubsystem::GetSceneInterface() const
{
	if (UWorld* const World = GetWorld())
	{
		return GetSceneInterface(World->PersistentLevel);
	}
	return nullptr;
}

IAvaSceneInterface* UAvaSceneSubsystem::GetSceneInterface(ULevel* InLevel) const
{
	return SceneInterfaces.FindRef(InLevel).Get();
}

IAvaSceneInterface* UAvaSceneSubsystem::FindSceneInterface(ULevel* InLevel)
{
	AAvaScene* SceneActor = nullptr;
	if (InLevel)
	{
		InLevel->Actors.FindItemByClass(&SceneActor);
	}
	return SceneActor;
}

void UAvaSceneSubsystem::PostInitialize()
{
	Super::PostInitialize();

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	// Register Default Scene Interfaces
	for (FConstLevelIterator LevelIterator = World->GetLevelIterator(); LevelIterator; ++LevelIterator)
	{
		ULevel* const Level = *LevelIterator;

		AAvaScene* SceneActor = nullptr;
		if (Level && Level->Actors.FindItemByClass(&SceneActor))
		{
			RegisterSceneInterface(Level, SceneActor);
		}
	}
}

bool UAvaSceneSubsystem::DoesSupportWorldType(const EWorldType::Type InWorldType) const
{
	const bool bDisallowedWorld = UE::Ava::Private::GUnsupportedWorlds.Contains(InWorldType);
	return !bDisallowedWorld;
}
