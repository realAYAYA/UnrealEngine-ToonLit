// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditorWorldUtils.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryHelpers.h"

FScopedEditorWorld::FScopedEditorWorld()
	: World(nullptr)
	, PrevGWorld(nullptr)
{
}

FScopedEditorWorld::FScopedEditorWorld(UWorld* InWorld, const UWorld::InitializationValues& InInitializationValues, EWorldType::Type InWorldType)
	: FScopedEditorWorld()
{
	Init(InWorld, InInitializationValues, InWorldType);
}

FScopedEditorWorld::FScopedEditorWorld(const FStringView InLongPackageName, const UWorld::InitializationValues& InInitializationValues, EWorldType::Type InWorldType)
	: FScopedEditorWorld(TSoftObjectPtr<UWorld>(FSoftObjectPath(InLongPackageName)), InInitializationValues, InWorldType)
{
}

FScopedEditorWorld::FScopedEditorWorld(const TSoftObjectPtr<UWorld>& InSoftWorld, const UWorld::InitializationValues& InInitializationValues, EWorldType::Type InWorldType)
	: FScopedEditorWorld()
{
	FSoftObjectPath WorldObjectPath(InSoftWorld.ToSoftObjectPath());
	// Handle cases where the SoftObjectPath doesn't have a valid AssetName
	if (WorldObjectPath.GetAssetFName().IsNone())
	{
		FString WorldObjectPathStr = WorldObjectPath.GetLongPackageName() + TEXT(".") + FPackageName::GetShortName(WorldObjectPath.GetLongPackageName());
		WorldObjectPath = FSoftObjectPath(WorldObjectPathStr);
	}
	UAssetRegistryHelpers::FixupRedirectedAssetPath(WorldObjectPath);
	
	if (UPackage* WorldPackage = LoadWorldPackageForEditor(WorldObjectPath.GetLongPackageName()))
	{
		if (UWorld* RuntimeWorld = UWorld::FindWorldInPackage(WorldPackage))
		{
			Init(RuntimeWorld, InInitializationValues, InWorldType);
		}
	}
}

void FScopedEditorWorld::Init(UWorld* InWorld, const UWorld::InitializationValues& InInitializationValues, EWorldType::Type InWorldType)
{
	check(InWorld);
	check(!InWorld->bIsWorldInitialized);

	World = InWorld;

	// Add to root
	World->AddToRoot();

	// Set current GWorld / WorldContext
	FWorldContext& WorldContext = GEditor->GetEditorWorldContext(true);
	WorldContext.SetCurrentWorld(World);
	PrevGWorld = GWorld;
	GWorld = World;

	// Initialize the world
	World->WorldType = InWorldType;
	World->InitWorld(InInitializationValues);
	World->PersistentLevel->UpdateModelComponents();
	World->UpdateWorldComponents(true /*bRerunConstructionScripts*/, false /*bCurrentLevelOnly*/);
	World->UpdateLevelStreaming();
}

UWorld* FScopedEditorWorld::GetWorld() const
{
	return World;
}

FScopedEditorWorld::~FScopedEditorWorld()
{
	if (World)
	{
		// Destroy world
		World->DestroyWorld(false /*bBroadcastWorldDestroyedEvent*/);

		// Unroot world
		World->RemoveFromRoot();

		// Restore previous GWorld / WorldContext
		FWorldContext& WorldContext = GEditor->GetEditorWorldContext(true);
		WorldContext.SetCurrentWorld(PrevGWorld);
		GWorld = PrevGWorld;
	}
}


UPackage* LoadWorldPackageForEditor(const FStringView InLongPackageName, EWorldType::Type InWorldType, uint32 InLoadFlags)
{
	FName WorldPackageFName(InLongPackageName);
	UWorld::WorldTypePreLoadMap.FindOrAdd(WorldPackageFName) = InWorldType;
	UPackage* WorldPackage = LoadPackage(nullptr, InLongPackageName.GetData(), InLoadFlags);
	UWorld::WorldTypePreLoadMap.Remove(WorldPackageFName);
	return WorldPackage;
}