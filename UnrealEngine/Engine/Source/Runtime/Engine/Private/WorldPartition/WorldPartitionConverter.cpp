// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionConverter.h"

#if WITH_EDITOR

#include "Engine/World.h"
#include "WorldPartition/WorldPartition.h"
#include "Engine/LODActor.h"
#include "Engine/LevelBounds.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "UObject/MetaData.h"
#include "GameFramework/WorldSettings.h"
#include "LevelUtils.h"
#include "EditorLevelUtils.h"
#include "LandscapeGizmoActor.h"
#include "EdGraph/EdGraph.h"

bool FWorldPartitionConverter::Convert(UWorld* InWorld, const FWorldPartitionConverter::FParameters& InParameters)
{
	if (!InWorld)
	{
		return false;
	}

	FWorldPartitionConverter Converter(InWorld, InParameters);
	return Converter.Convert();
}

FWorldPartitionConverter::FWorldPartitionConverter(UWorld* InWorld, const FWorldPartitionConverter::FParameters& InParameters)
	: World(InWorld)
	, Parameters(InParameters)
{
	check(World);
}

bool FWorldPartitionConverter::Convert()
{
	AWorldSettings* WorldSettings = World->GetWorldSettings();
	if (!WorldSettings)
	{
		return false;
	}

	bool bCreatedWorldPartition = false;
	UWorldPartition* WorldPartition = World->GetWorldPartition();
	if (!WorldPartition)
	{
		World->PersistentLevel->ConvertAllActorsToPackaging(true);
		World->PersistentLevel->bUseExternalActors = true;
		
		WorldPartition = UWorldPartition::CreateOrRepairWorldPartition(WorldSettings, Parameters.EditorHashClass, Parameters.RuntimeHashClass);
		if (!WorldPartition)
		{
			return false;
		}

		FLevelActorFoldersHelper::SetUseActorFolders(World->PersistentLevel, Parameters.bUseActorFolders);
		WorldPartition->bEnableStreaming = Parameters.bEnableStreaming;
		WorldPartition->bStreamingWasEnabled = Parameters.bEnableStreaming;
		bCreatedWorldPartition = true;
	}

	UWorld* MainWorld = World;
	ULevel* MainLevel = MainWorld->PersistentLevel;
	UPackage* MainPackage = MainLevel->GetPackage();

	TArray<AActor*> Actors = ObjectPtrDecay(MainLevel->Actors);
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor) && ShouldDeleteActor(Actor, /*bIsMainLevel*/ true))
		{
			MainWorld->EditorDestroyActor(Actor, /*bShouldModifyLevel*/ false);
		}
	}

	if (Parameters.bConvertSubLevels)
	{
		TArray<ULevel*> SubLevelsToConvert;
		GatherAndPrepareSubLevelsToConvert(World->PersistentLevel, SubLevelsToConvert);

		for (ULevel* SubLevel : SubLevelsToConvert)
		{
			UPackage* SourceLevelPackage = SubLevel->GetPackage();
			UWorld* SourcePackageWorld = UWorld::FindWorldInPackage(SourceLevelPackage);
			// Avoid modifying original sub-level
			UPackage* DuplicatedLevelPackage = CreatePackage(*FString::Printf(TEXT("%s_DuplicateTemp"), *SourceLevelPackage->GetName()));
			FObjectDuplicationParameters DuplicationParameters(SourcePackageWorld, DuplicatedLevelPackage);
			TMap<UObject*, UObject*> DuplicatedObjectPtrs;
			DuplicationParameters.CreatedObjects = &DuplicatedObjectPtrs;
			DuplicationParameters.bAssignExternalPackages = false;
			DuplicationParameters.DuplicateMode = EDuplicateMode::World;
			UWorld* DuplicatedWorld = Cast<UWorld>(StaticDuplicateObjectEx(DuplicationParameters));
			check(DuplicatedWorld);
			ULevel* DuplicatedLevel = DuplicatedWorld->PersistentLevel;

			UWorld* SubWorld = SubLevel->GetTypedOuter<UWorld>();
			UPackage* SubPackage = SubLevel->GetPackage();

			RemapSoftObjectPaths.Add(FSoftObjectPath(SubWorld).ToString(), FSoftObjectPath(MainWorld).ToString());
			RemapSoftObjectPaths.Add(FSoftObjectPath(SubLevel).ToString(), FSoftObjectPath(MainLevel).ToString());
			RemapSoftObjectPaths.Add(FSoftObjectPath(SubPackage).ToString(), FSoftObjectPath(MainPackage).ToString());

			DuplicatedLevel->ConvertAllActorsToPackaging(true);
			DuplicatedLevel->bUseExternalActors = true;

			TArray<AActor*> ActorsToConvert = ObjectPtrDecay(DuplicatedLevel->Actors);
			for (AActor* Actor : ActorsToConvert)
			{
				if (IsValid(Actor) && !ShouldDeleteActor(Actor, /*bIsMainLevel*/ false))
				{
					// Before changing outer, bIsCookedForEditor flag to actor package 
					if (SourceLevelPackage->bIsCookedForEditor)
					{
						if (UPackage* ActorPackage = Actor->GetExternalPackage())
						{
							ActorPackage->bIsCookedForEditor = true;
							ActorPackage->SetPackageFlags(PKG_FilterEditorOnly);
						}
					}

					ChangeObjectOuter(Actor, MainLevel);

					// Migrate blueprint classes
					UClass* ActorClass = Actor->GetClass();
					if (!ActorClass->IsNative() && (ActorClass->GetPackage() == SubPackage))
					{
						ChangeObjectOuter(ActorClass, MainPackage);
					}
				}
			}

			TArray<UObject*> ObjectsToRename;
			ForEachObjectWithPackage(SubPackage, [&](UObject* Object)
			{
				if (!Object->IsA<AActor>() && !Object->IsA<ULevel>() && !Object->IsA<UWorld>() && !Object->IsA<UMetaData>())
				{
					ObjectsToRename.Add(Object);
				}
				return true;
			}, /*bIncludeNestedObjects*/false);

			for (UObject* ObjectToRename : ObjectsToRename)
			{
				ChangeObjectOuter(ObjectToRename, MainPackage);
			}
		}

		// Remove converted sublevels
		TArray<ULevelStreaming*> StreamingLevels = World->GetStreamingLevels();
		for (ULevelStreaming* LevelStreaming : StreamingLevels)
		{
			ULevel* LoadedLevel = LevelStreaming->GetLoadedLevel();
			if (SubLevelsToConvert.Contains(LoadedLevel))
			{
				if (FLevelUtils::IsLevelLocked(LoadedLevel))
				{
					FLevelUtils::ToggleLevelLock(LoadedLevel);
				}
				EditorLevelUtils::RemoveLevelFromWorld(LoadedLevel);
			}
		}
	}

	// Fixup SoftObjectPaths
	FixupSoftObjectPaths(MainPackage);

	if (bCreatedWorldPartition)
	{
		WorldPartition->Initialize(World, FTransform::Identity);
		UWorldPartition::WorldPartitionChangedEvent.Broadcast(World);
	}

	return true;
}

bool FWorldPartitionConverter::ShouldDeleteActor(AActor* InActor, bool bIsMainLevel) const
{
	// We need to migrate transient actors as Fortnite uses a transient actor(AFortTimeOfDayManager) to handle lighting in maps and is required during the generation of MiniMap. 
	if (InActor->IsA<ALODActor>() ||
		InActor->IsA<ALandscapeGizmoActor>())
	{
		return true;
	}

	if (!bIsMainLevel)
	{
		// Only delete these actors if they aren't in the main level
		if (InActor->IsA<ALevelScriptActor>() ||
			InActor->IsA<AWorldSettings>() ||
			InActor->IsA<ALevelBounds>() ||
			InActor == (AActor*)InActor->GetLevel()->GetDefaultBrush())
		{
			return true;
		}
	}

	return false;
}

void FWorldPartitionConverter::ChangeObjectOuter(UObject* InObject, UObject* InNewOuter)
{
	FString OldPath = FSoftObjectPath(InObject).ToString();
	InObject->Rename(nullptr, InNewOuter, REN_NonTransactional | REN_DontCreateRedirectors | REN_ForceNoResetLoaders | REN_DoNotDirty);
	FString NewPath = FSoftObjectPath(InObject).ToString();
	RemapSoftObjectPaths.Add(OldPath, NewPath);
}

void FWorldPartitionConverter::GatherAndPrepareSubLevelsToConvert(ULevel* InLevel, TArray<ULevel*>& OutSubLevels)
{
	TArray<ULevelStreaming*> StreamingLevels;
	UWorld* OuterWorld = InLevel->GetTypedOuter<UWorld>();

	for (ULevelStreaming* StreamingLevel : OuterWorld->GetStreamingLevels())
	{
		if (StreamingLevel)
		{
			StreamingLevels.Add(StreamingLevel);
			StreamingLevel->SetShouldBeLoaded(true);
			StreamingLevel->SetShouldBeVisible(true);
			StreamingLevel->SetShouldBeVisibleInEditor(true);
		}
	}
	OuterWorld->FlushLevelStreaming();

	for (ULevelStreaming* StreamingLevel : StreamingLevels)
	{
		if (PrepareStreamingLevelForConversion(StreamingLevel))
		{
			ULevel* Level = StreamingLevel->GetLoadedLevel();
			OutSubLevels.Add(Level);
			// Recursively obtain sub levels to convert
			GatherAndPrepareSubLevelsToConvert(Level, OutSubLevels);
		}
	}
}

bool FWorldPartitionConverter::PrepareStreamingLevelForConversion(ULevelStreaming* InStreamingLevel)
{
	ULevel* Level = InStreamingLevel->GetLoadedLevel();
	if (!Level || LevelHasLevelScriptBlueprint(Level))
	{
		return false;
	}

	if (InStreamingLevel->ShouldBeAlwaysLoaded() || InStreamingLevel->bDisableDistanceStreaming)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->CanChangeIsSpatiallyLoadedFlag())
			{
				Actor->SetIsSpatiallyLoaded(false);
			}
		}
	}

	return true;
}

bool FWorldPartitionConverter::LevelHasLevelScriptBlueprint(ULevel* InLevel)
{
	if (ULevelScriptBlueprint* LevelScriptBP = InLevel->GetLevelScriptBlueprint(/*bDontCreate*/ true))
	{
		TArray<UEdGraph*> AllGraphs;
		LevelScriptBP->GetAllGraphs(AllGraphs);
		for (UEdGraph* CurrentGraph : AllGraphs)
		{
			for (UEdGraphNode* Node : CurrentGraph->Nodes)
			{
				if (!Node->IsAutomaticallyPlacedGhostNode())
				{
					return true;
				}
			}
		}
	}
	return false;
}

void FWorldPartitionConverter::FixupSoftObjectPaths(UPackage* OuterPackage)
{
	struct FSoftPathFixupSerializer : public FArchiveUObject
	{
		FSoftPathFixupSerializer(TMap<FString, FString>& InRemapSoftObjectPaths)
			: RemapSoftObjectPaths(InRemapSoftObjectPaths)
		{
			this->SetIsSaving(true);
		}

		FArchive& operator<<(FSoftObjectPath& Value)
		{
			if (Value.IsNull())
			{
				return *this;
			}

			FString OriginalValue = Value.ToString();

			if (FString* RemappedValue = RemapSoftObjectPaths.Find(OriginalValue))
			{
				Value.SetPath(*RemappedValue);
			}
			else if (Value.GetSubPathString().StartsWith(TEXT("PersistentLevel.")))
			{
				int32 DotPos = Value.GetSubPathString().Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromStart);
				if (DotPos != INDEX_NONE)
				{
					RemappedValue = RemapSoftObjectPaths.Find(Value.GetWithoutSubPath().ToString());
					if (RemappedValue)
					{
						FString NewPath = *RemappedValue + ':' + Value.GetSubPathString();
						Value.SetPath(NewPath);
					}
				}

				FString NewValue = Value.ToString();
				if (NewValue == OriginalValue)
				{
					Value.Reset();
				}
			}

			return *this;
		}

	private:
		TMap<FString, FString>& RemapSoftObjectPaths;
	};

	FSoftPathFixupSerializer FixupSerializer(RemapSoftObjectPaths);

	ForEachObjectWithPackage(OuterPackage, [&](UObject* Object)
	{
		if (Object->HasAllFlags(RF_WasLoaded))
		{
			Object->Serialize(FixupSerializer);
		}
		return true;
	}, true, RF_NoFlags, EInternalObjectFlags::Garbage);
}

#endif
