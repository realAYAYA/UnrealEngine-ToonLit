// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 WorldPartitionConvertCommandlet.cpp: Commandlet used to convert levels to partition
=============================================================================*/

#include "Commandlets/WorldPartitionConvertCommandlet.h"
#include "Algo/ForEach.h"
#include "AssetToolsModule.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/LevelBounds.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/LODActor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/MapBuildDataRegistry.h"
#include "ActorReferencesUtils.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionSettings.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/WorldPartitionMiniMap.h"
#include "WorldPartition/WorldPartitionMiniMapHelper.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "DataLayer/DataLayerFactory.h"
#include "GameFramework/WorldSettings.h"
#include "UObject/UObjectHash.h"
#include "PackageHelperFunctions.h"
#include "UObject/MetaData.h"
#include "UObject/SavePackage.h"
#include "Editor.h"
#include "HLOD/HLODEngineSubsystem.h"
#include "HierarchicalLOD.h"
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "InstancedFoliageActor.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor/GroupActor.h"
#include "EdGraph/EdGraph.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FoliageEditUtility.h"
#include "FoliageHelper.h"
#include "Engine/WorldComposition.h"
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "InstancedFoliage.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeConfigHelper.h"
#include "LandscapeGizmoActor.h"
#include "WorldPartition/DataLayer/DataLayerInstanceWithAsset.h"
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "ActorFolder.h"

DEFINE_LOG_CATEGORY(LogWorldPartitionConvertCommandlet);

class FArchiveGatherPrivateImports : public FArchiveUObject
{
	AActor* Root;
	UPackage* RootPackage;
	UObject* CurrentObject;
	TMap<UObject*, AActor*>& PrivateRefsMap;
	TSet<FString>& ActorsReferencesToActors;

	void HandleObjectReference(UObject* Obj)
	{
		if(!Obj->HasAnyMarks(OBJECTMARK_TagImp))
		{
			UObject* OldCurrentObject = CurrentObject;
			CurrentObject = Obj;
			Obj->Mark(OBJECTMARK_TagImp);
			Obj->Serialize(*this);
			CurrentObject = OldCurrentObject;
		}
	}

public:
	FArchiveGatherPrivateImports(AActor* InRoot, TMap<UObject*, AActor*>& InPrivateRefsMap, TSet<FString>& InActorsReferencesToActors)
		: Root(InRoot)
		, RootPackage(InRoot->GetPackage())
		, CurrentObject(nullptr)
		, PrivateRefsMap(InPrivateRefsMap)
		, ActorsReferencesToActors(InActorsReferencesToActors)
	{
		SetIsSaving(true);
		SetIsPersistent(true);
		ArIsObjectReferenceCollector = true;
		ArShouldSkipBulkData = true;
		UnMarkAllObjects();
	}

	~FArchiveGatherPrivateImports()
	{
		UnMarkAllObjects();
	}

	virtual FArchive& operator<<(UObject*& Obj) override
	{
		if(Obj)
		{
			if(Obj->IsIn(Root) || (CurrentObject && Obj->IsIn(CurrentObject)))
			{
				HandleObjectReference(Obj);
			}
			else if(Obj->IsInPackage(RootPackage) && !Obj->HasAnyFlags(RF_Standalone))
			{
				if(!Obj->GetTypedOuter<AActor>())
				{
					AActor** OriginalRoot = PrivateRefsMap.Find(Obj);
					if(OriginalRoot && (*OriginalRoot != Root))
					{
						SET_WARN_COLOR(COLOR_RED);
						UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Duplicate reference %s.%s(%s) (first referenced by %s)"), *Root->GetName(), *Obj->GetName(), *Obj->GetClass()->GetName(), *(*OriginalRoot)->GetName());
						CLEAR_WARN_COLOR();
					}
					else if(!OriginalRoot)
					{
						// Actor references will be extracted by the caller, ignore them
						if(Obj->IsA<AActor>() && !Obj->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && Obj->GetTypedOuter<ULevel>())
						{
							AActor* ActorRef = (AActor*)Obj;
							ActorsReferencesToActors.Add(
								FString::Printf(
									TEXT("%s, %s, %s, %s, %.2f"), 
									*RootPackage->GetName(), 
									CurrentObject ? *CurrentObject->GetName() : *Root->GetName(), 
									CurrentObject ? *Root->GetName() : TEXT("null"),
									*Obj->GetName(), 
									(ActorRef->GetActorLocation() - Root->GetActorLocation()).Size())
							);
						}
						else if(!Obj->IsA<ULevel>())
						{
							if(!CurrentObject || !Obj->IsIn(CurrentObject))
							{
								PrivateRefsMap.Add(Obj, Root);

								SET_WARN_COLOR(COLOR_WHITE);
								UE_LOG(LogWorldPartitionConvertCommandlet, Warning, TEXT("Encountered actor %s referencing %s (%s)"), *Root->GetName(), *Obj->GetPathName(), *Obj->GetClass()->GetName());
								CLEAR_WARN_COLOR();
							}

							HandleObjectReference(Obj);
						}
					}
				}
			}
		}
		return *this;
	}
};

UWorldPartitionConvertCommandlet::UWorldPartitionConvertCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bConversionSuffix(false)
	, ConversionSuffix(TEXT("_WP"))
	, bConvertActorsNotReferencedByLevelScript(true)
	, WorldOrigin(FVector::ZeroVector)
	, WorldExtent(HALF_WORLD_MAX)
	, LandscapeGridSize(4)
	, DataLayerFactory(NewObject<UDataLayerFactory>())
{
	if (FParse::Param(FCommandLine::Get(), TEXT("RunningFromUnrealEd")))
	{
		ShowErrorCount = false;	// This has the side effect of making the process return code match the return code of the commandlet
		FastExit = true;		// Faster exit which avoids crash during shutdown. The engine isn't shutdown cleanly.
	}
}

UWorld* UWorldPartitionConvertCommandlet::LoadWorld(const FString& LevelToLoad)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::LoadWorld);

	SET_WARN_COLOR(COLOR_WHITE);
	UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Loading level %s."), *LevelToLoad);
	CLEAR_WARN_COLOR();

	UPackage* MapPackage = LoadPackage(nullptr, *LevelToLoad, LOAD_None);
	if (!MapPackage)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Error loading %s."), *LevelToLoad);
		return nullptr;
	}

	return UWorld::FindWorldInPackage(MapPackage);
}

ULevel* UWorldPartitionConvertCommandlet::InitWorld(UWorld* World)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::InitWorld);

	SET_WARN_COLOR(COLOR_WHITE);
	UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Initializing level %s."), *World->GetName());
	CLEAR_WARN_COLOR();

	// Setup the world.
	World->WorldType = EWorldType::Editor;
	World->AddToRoot();
	if (!World->bIsWorldInitialized)
	{
		UWorld::InitializationValues IVS;
		IVS.RequiresHitProxies(false);
		IVS.ShouldSimulatePhysics(false);
		IVS.EnableTraceCollision(false);
		IVS.CreateNavigation(false);
		IVS.CreateAISystem(false);
		IVS.AllowAudioPlayback(false);
		IVS.CreatePhysicsScene(true);

		World->InitWorld(IVS);
		World->PersistentLevel->UpdateModelComponents();
		World->UpdateWorldComponents(true, false);

		World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	}

	return World->PersistentLevel;
}

UWorldPartition* UWorldPartitionConvertCommandlet::CreateWorldPartition(AWorldSettings* MainWorldSettings)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::CreateWorldPartition);

	UWorldPartition* WorldPartition = UWorldPartition::CreateOrRepairWorldPartition(MainWorldSettings, EditorHashClass, RuntimeHashClass);

	if (bDisableStreaming)
	{
		WorldPartition->bEnableStreaming = false;
	}
		
	// Read the conversion config file
	if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*LevelConfigFilename))
	{
		WorldPartition->EditorHash->LoadConfig(*EditorHashClass, *LevelConfigFilename);
		WorldPartition->RuntimeHash->LoadConfig(*RuntimeHashClass, *LevelConfigFilename);

		// Use specified existing default HLOD layer if valid 
		if (UHLODLayer* ExistingHLODLayer = LoadObject<UHLODLayer>(NULL, *DefaultHLODLayerAsset.ToString(), nullptr, LOAD_NoWarn))
		{
			WorldPartition->DefaultHLODLayer = ExistingHLODLayer;
		}
	}

	// Duplicate the default HLOD setup
	if ((WorldPartition->DefaultHLODLayer == UHLODLayer::GetEngineDefaultHLODLayersSetup()) && !bDisableStreaming)
	{
		UHLODLayer* CurHLODLayer = WorldPartition->GetDefaultHLODLayer();
		UHLODLayer* NewHLODLayer = UHLODLayer::DuplicateHLODLayersSetup(CurHLODLayer, WorldPartition->GetPackage()->GetName(), WorldPartition->GetWorld()->GetName());
		
		WorldPartition->SetDefaultHLODLayer(NewHLODLayer);

		TMap<UHLODLayer*, UHLODLayer*> ReplacementMap;
		while (NewHLODLayer)
		{
			ReplacementMap.Add(CurHLODLayer, NewHLODLayer);
			PackagesToSave.Add(NewHLODLayer->GetPackage());
			CurHLODLayer = CurHLODLayer->GetParentLayer();
			NewHLODLayer = NewHLODLayer->GetParentLayer();
		}
							
		FArchiveReplaceObjectRef<UHLODLayer> ReplaceObjectRefAr(WorldPartition->RuntimeHash, ReplacementMap, EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);							
	}
	
	return WorldPartition;
}

void UWorldPartitionConvertCommandlet::GatherAndPrepareSubLevelsToConvert(ULevel* Level, TArray<ULevel*>& SubLevels)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::GatherAndPrepareSubLevelsToConvert);

	UWorld* World = Level->GetTypedOuter<UWorld>();	

	// Set all streaming levels to be loaded/visible for next Flush
	TArray<ULevelStreaming*> StreamingLevels;
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (ShouldConvertStreamingLevel(StreamingLevel))
		{
			StreamingLevels.Add(StreamingLevel);
			StreamingLevel->SetShouldBeLoaded(true);
			StreamingLevel->SetShouldBeVisible(true);
			StreamingLevel->SetShouldBeVisibleInEditor(true);
		}
		else
		{
			UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Skipping conversion of streaming Level %s"), *StreamingLevel->GetWorldAssetPackageName());
		}
	}

	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	
	for(ULevelStreaming* StreamingLevel: StreamingLevels)
	{
		if (PrepareStreamingLevelForConversion(StreamingLevel))
		{
			ULevel* SubLevel = StreamingLevel->GetLoadedLevel();
			check(SubLevel);

			SubLevels.Add(SubLevel);

			// Recursively obtain sub levels to convert
			GatherAndPrepareSubLevelsToConvert(SubLevel, SubLevels);
		}
	}
}

bool UWorldPartitionConvertCommandlet::PrepareStreamingLevelForConversion(ULevelStreaming* StreamingLevel)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::PrepareStreamingLevelForConversion);

	ULevel* SubLevel = StreamingLevel->GetLoadedLevel();
	check(SubLevel);

	if (bOnlyMergeSubLevels || StreamingLevel->ShouldBeAlwaysLoaded() || StreamingLevel->bDisableDistanceStreaming)
	{
		FString WorldPath = SubLevel->GetPackage()->GetName();
		UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Converting %s streaming level %s"), StreamingLevel->bDisableDistanceStreaming ? TEXT("non distance-based") : TEXT("always loaded"), *StreamingLevel->GetWorldAssetPackageName());

		for (AActor* Actor: SubLevel->Actors)
		{
			if (Actor && Actor->CanChangeIsSpatiallyLoadedFlag())
			{
				Actor->SetIsSpatiallyLoaded(false);
			}
		}
	}
	
	return true;
}

bool UWorldPartitionConvertCommandlet::GetAdditionalLevelsToConvert(ULevel* Level, TArray<ULevel*>& SubLevels)
{
	return true;
}

bool UWorldPartitionConvertCommandlet::ShouldDeleteActor(AActor* Actor, bool bMainLevel) const
{
	// We need to migrate transient actors as Fortnite uses a transient actor(AFortTimeOfDayManager) to handle lighting in maps and is required during the generation of MiniMap. 
	if (Actor->IsA<ALODActor>() ||
		Actor->IsA<ALevelBounds>() ||
		Actor->IsA<ALandscapeGizmoActor>())
	{
		return true;
	}

	if (!bMainLevel)
	{
		// Only delete these actors if they aren't in the main level
		if (Actor->IsA<ALevelScriptActor>() ||
			Actor->IsA<AWorldSettings>() ||
			Actor == (AActor*)Actor->GetLevel()->GetDefaultBrush())
		{
			return true;
		}
	}

	return false;
}

void UWorldPartitionConvertCommandlet::PerformAdditionalWorldCleanup(UWorld* World) const
{
}

void UWorldPartitionConvertCommandlet::OutputConversionReport() const
{
	UE_LOG(LogWorldPartitionConvertCommandlet, Display, TEXT("WorldPartitionConvertCommandlet report:"));

	auto OutputReport = [](const TCHAR* Msg, const TSet<FString>& Values)
	{
		if (Values.Num() != 0)
		{
			UE_LOG(LogWorldPartitionConvertCommandlet, Display, TEXT("- Found %s:"), Msg);
			TArray<FString> Array = Values.Array();
			Array.Sort();
			for (const FString& Name : Array)
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Display, TEXT("  * %s"), *Name);
			}
			UE_LOG(LogWorldPartitionConvertCommandlet, Display, TEXT(""));
		}	
	};

	OutputReport(TEXT("sublevels containing LevelScriptBPs"), MapsWithLevelScriptsBPs);
	OutputReport(TEXT("sublevels containing MapBuildData"), MapsWithMapBuildData);
	OutputReport(TEXT("actors with child actors"), ActorsWithChildActors);
	OutputReport(TEXT("group actors"), GroupActors);
	OutputReport(TEXT("actors in actor groups"), ActorsInGroupActors);
	OutputReport(TEXT("actor referencing other actors"), ActorsReferencesToActors);
}

bool LevelHasLevelScriptBlueprint(ULevel* Level)
{
	if (ULevelScriptBlueprint* LevelScriptBP = Level->GetLevelScriptBlueprint(true))
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

bool LevelHasMapBuildData(ULevel* Level)
{
	return Level->MapBuildData != nullptr;
}

void UWorldPartitionConvertCommandlet::ChangeObjectOuter(UObject* Object, UObject* NewOuter)
{
	FString OldPath = FSoftObjectPath(Object).ToString();
	Object->Rename(nullptr, NewOuter, REN_DontCreateRedirectors);
	FString NewPath = FSoftObjectPath(Object).ToString();
	RemapSoftObjectPaths.Add(OldPath, NewPath);
}

void UWorldPartitionConvertCommandlet::FixupSoftObjectPaths(UPackage* OuterPackage)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::FixupSoftObjectPaths);

	UE_SCOPED_TIMER(TEXT("FixupSoftObjectPaths"), LogWorldPartitionConvertCommandlet, Display);

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

			const FString OriginalValue = Value.ToString();

			auto GetSourceString = [this]()
			{
				FString DebugStackString;
				for (const FName& DebugData: DebugDataStack)
				{
					DebugStackString += DebugData.ToString();
					DebugStackString += TEXT(".");
				}
				DebugStackString.RemoveFromEnd(TEXT("."));
				return DebugStackString;
			};

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
			}

			if (!Value.IsNull())
			{
				FString NewValue = Value.ToString();
				if (NewValue != OriginalValue)
				{
					UE_LOG(LogWorldPartitionConvertCommandlet, Verbose, TEXT("Remapped SoftObjectPath %s to %s"), *OriginalValue, *NewValue);
					UE_LOG(LogWorldPartitionConvertCommandlet, Verbose, TEXT("  Source: %s"), *GetSourceString());
				}
			}

			return *this;
		}

	private:
		virtual void PushDebugDataString(const FName& DebugData) override
		{
			DebugDataStack.Add(DebugData);
		}

		virtual void PopDebugDataString() override
		{
			DebugDataStack.Pop();
		}

		TArray<FName> DebugDataStack;
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

bool UWorldPartitionConvertCommandlet::DetachDependantLevelPackages(ULevel* Level)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::DetachDependantLevelPackages);

	if (Level->MapBuildData && (Level->MapBuildData->GetPackage() != Level->GetPackage()))
	{
		PackagesToDelete.Add(Level->MapBuildData->GetPackage());
		Level->MapBuildData = nullptr;
	}

	// Try to delete matching HLOD package
	FHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<FHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
	IHierarchicalLODUtilities* Utilities = Module.GetUtilities();

	const int32 NumHLODLevels = Level->GetWorldSettings()->GetNumHierarchicalLODLevels();

	for (int32 HLODIndex=0; HLODIndex<NumHLODLevels; HLODIndex++)
	{
		if (UPackage* HLODPackage = Utilities->RetrieveLevelHLODPackage(Level, HLODIndex))
		{
			PackagesToDelete.Add(HLODPackage);
		}
	}

	for (AActor* Actor: Level->Actors)
	{
		if (IsValid(Actor) && Actor->IsA<ALODActor>())
		{
			Level->GetWorld()->DestroyActor(Actor);
		}
	}

	Level->GetWorldSettings()->ResetHierarchicalLODSetup();

	return true;
}

bool UWorldPartitionConvertCommandlet::RenameWorldPackageWithSuffix(UWorld* World)
{
	bool bRenamedSuccess = false;
	UPackage* Package = World->GetPackage();

	FString OldWorldName = World->GetName();
	FString NewWorldName = OldWorldName + ConversionSuffix;
	bRenamedSuccess = World->Rename(*NewWorldName, nullptr, REN_NonTransactional | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	if (!bRenamedSuccess)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Unable to rename world to %s"), *NewWorldName);
		return false;
	}

	FString OldPackageName = Package->GetName();
	FString NewPackageName = OldPackageName + ConversionSuffix;
	FString NewPackageResourceName = Package->GetLoadedPath().GetPackageName().Replace(*OldPackageName, *NewPackageName);
	bRenamedSuccess = Package->Rename(*NewPackageName, nullptr, REN_NonTransactional | REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	if (!bRenamedSuccess)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Unable to rename package to %s"), *NewPackageName);
		return false;
	}
	Package->SetLoadedPath(FPackagePath::FromPackageNameChecked(NewPackageResourceName));

	return true;
}

void UWorldPartitionConvertCommandlet::SetupHLOD()
{
	// No need to spawn HLOD actors during the conversion
	GEngine->GetEngineSubsystem<UHLODEngineSubsystem>()->DisableHLODSpawningOnLoad(true);

	SetupHLODLayerAssets();
}

void UWorldPartitionConvertCommandlet::SetupHLODLayerAssets()
{
	// Assign HLOD layers to the classes listed in the level config
	for (const FHLODLayerActorMapping& Entry : HLODLayersForActorClasses)
	{
		UHLODLayer* HLODLayer = LoadObject<UHLODLayer>(NULL, *Entry.HLODLayer.ToString(), nullptr, LOAD_NoWarn);
		if (!HLODLayer)
		{
			UE_LOG(LogWorldPartitionConvertCommandlet, Warning, TEXT("Unable to load HLOD Layer %s, skipping assignment to class %s"), *Entry.HLODLayer.ToString(), *Entry.ActorClass.ToString());
			continue;
		}

		// Load the BP class & assign 
		if (UClass* LoadedObject = Entry.ActorClass.LoadSynchronous())
		{
			if (AActor* CDO = CastChecked<AActor>(LoadedObject->GetDefaultObject()))
			{
				if (CDO->GetHLODLayer() != HLODLayer)
				{
					CDO->SetHLODLayer(HLODLayer);
					CDO->MarkPackageDirty();
					PackagesToSave.Add(CDO->GetPackage());
				}
			}
		}
	}
}

void UWorldPartitionConvertCommandlet::SetActorGuid(AActor* Actor, const FGuid& NewGuid)
{
	FSetActorGuid SetActorGuid(Actor, NewGuid);
}

void UWorldPartitionConvertCommandlet::OnWorldLoaded(UWorld* World)
{
	if (UWorldComposition* WorldComposition = World->WorldComposition)
	{
		// Add tiles streaming levels to world
		World->SetStreamingLevels(WorldComposition->TilesStreaming);

		// Make sure to force bDisableDistanceStreaming on streaming levels of World Composition non distance dependent tiles (for the rest of the process to handle streaming level as always loaded)
		UWorldComposition::FTilesList& Tiles = WorldComposition->GetTilesList();
		for (int32 TileIdx = 0; TileIdx < Tiles.Num(); TileIdx++)
		{
			FWorldCompositionTile& Tile = Tiles[TileIdx];
			ULevelStreaming* StreamingLevel = WorldComposition->TilesStreaming[TileIdx];
			if (StreamingLevel && !WorldComposition->IsDistanceDependentLevel(Tile.PackageName))
			{
				StreamingLevel->bDisableDistanceStreaming = true;
			}
		}
	}
}

int32 UWorldPartitionConvertCommandlet::Main(const FString& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWorldPartitionConvertCommandlet::Main);

	UE_SCOPED_TIMER(TEXT("Conversion"), LogWorldPartitionConvertCommandlet, Display);

	FPackageSourceControlHelper PackageHelper;

	TArray<FString> Tokens, Switches;
	TMap<FString, FString> Arguments;
	ParseCommandLine(*Params, Tokens, Switches, Arguments);

	if (!Tokens.Num())
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("missing map name"));
		return 1;
	}
	else if (Tokens.Num() > 1)
	{
		FString BadParams;
		Algo::ForEach(Tokens, [&BadParams](const FString& Token) { BadParams += Token; BadParams += TEXT(" "); });
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("extra parameters %s"), *BadParams);
		return 1;
	}

	// This will convert incomplete package name to a fully qualified path, avoiding calling it several times (takes ~50s)
	if (!FPackageName::SearchForPackageOnDisk(Tokens[0], &Tokens[0]))
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Unknown level '%s'"), *Tokens[0]);
		return 1;
	}

	bOnlyMergeSubLevels = Switches.Contains(TEXT("OnlyMergeSubLevels"));
	bDeleteSourceLevels = Switches.Contains(TEXT("DeleteSourceLevels"));
	bGenerateIni = Switches.Contains(TEXT("GenerateIni"));
	bReportOnly = bGenerateIni || Switches.Contains(TEXT("ReportOnly"));
	bVerbose = Switches.Contains(TEXT("Verbose"));
	bDisableStreaming = Switches.Contains(TEXT("DisableStreaming"));
	ConversionSuffix = GetConversionSuffix(bOnlyMergeSubLevels);

	FString* FoliageTypePathValue = Arguments.Find(TEXT("FoliageTypePath"));
	
	if (FoliageTypePathValue != nullptr)
	{
		FoliageTypePath = *FoliageTypePathValue;
	}

	if (!Switches.Contains(TEXT("AllowCommandletRendering")))
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("The option \"-AllowCommandletRendering\" is required."));
		return 1;
	}

	ReadAdditionalTokensAndSwitches(Tokens, Switches);

	if (bVerbose)
	{
		LogWorldPartitionConvertCommandlet.SetVerbosity(ELogVerbosity::Verbose);
	}

	if (Switches.Contains(TEXT("RunningFromUnrealEd")))
	{
		UseCommandletResultAsExitCode = true;	// The process return code will match the return code of the commandlet
		FastExit = true;						// Faster exit which avoids crash during shutdown. The engine isn't shutdown cleanly.
	}

	bConversionSuffix = Switches.Contains(TEXT("ConversionSuffix"));

	// Load configuration file
	FString LevelLongPackageName;
	if (FPackageName::SearchForPackageOnDisk(Tokens[0], nullptr, &LevelLongPackageName))
	{
		LevelConfigFilename = FConfigCacheIni::NormalizeConfigIniPath(FPaths::ChangeExtension(LevelLongPackageName, TEXT("ini")));

		if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*LevelConfigFilename))
		{
			LoadConfig(GetClass(), *LevelConfigFilename);
		}
		else
		{
			EditorHashClass = UWorldPartitionSettings::Get()->GetEditorHashDefaultClass();
			RuntimeHashClass = UWorldPartitionSettings::Get()->GetRuntimeHashDefaultClass();
		}
	}

	if (!EditorHashClass)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Missing or invalid editor hash class"));
		return 1;
	}

	if (!RuntimeHashClass)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Missing or invalid runtime hash class"));
		return 1;
	}

	SetupHLOD();

	// Load world
	UWorld* MainWorld = LoadWorld(Tokens[0]);
	if (!MainWorld)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Unknown world '%s'"), *Tokens[0]);
		return 1;
	}

	// Setup Folder for DataLayer assets
	if (FString* DataLayerAssetFolderValue = Arguments.Find(TEXT("DataLayerAssetFolder")))
	{
		DataLayerAssetFolder = *DataLayerAssetFolderValue;
	}
	else
	{
		UPackage* Package = MainWorld->GetPackage();
		FName PackageMountPoint = FPackageName::GetPackageMountPoint(Package->GetLoadedPath().GetPackageName());
		if (PackageMountPoint.IsNone())
		{
			PackageMountPoint = TEXT("Game");
		}
		DataLayerAssetFolder = FPaths::RemoveDuplicateSlashes(FString::Printf(TEXT("/%s/DataLayers/%s%s/"), *PackageMountPoint.ToString(), *MainWorld->GetName(), *ConversionSuffix));
	}

	// Delete existing result from running the commandlet, even if not using the suffix mode to cleanup previous conversion
	if (!bReportOnly)
	{
		UE_SCOPED_TIMER(TEXT("Deleting existing conversion results"), LogWorldPartitionConvertCommandlet, Display);

		FString OldLevelName = Tokens[0] + ConversionSuffix;
		TArray<FString> CleanupPaths = ULevel::GetExternalObjectsPaths(OldLevelName);
		// Append DataLayer assets folder
		CleanupPaths.Add(DataLayerAssetFolder);

		TArray<FString> FilesToDelete;

		for (const FString& CleanupPath : CleanupPaths)
		{
			FString Directory = FPackageName::LongPackageNameToFilename(CleanupPath);
			if (IFileManager::Get().DirectoryExists(*Directory))
			{
				IFileManager::Get().IterateDirectoryRecursively(*Directory, [this, &FilesToDelete](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
				{
					if (!bIsDirectory)
					{
						FString Filename(FilenameOrDirectory);
						if (Filename.EndsWith(FPackageName::GetAssetPackageExtension()))
						{
							FilesToDelete.Emplace(MoveTemp(Filename));
						}
					}
					return true;
				});
			}
		}

		bool bResult = PackageHelper.Delete(FilesToDelete);
		if (!bResult)
		{
			UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Failed to delete previous conversion package(s)"));
			return 1;
		}

		if (FPackageName::SearchForPackageOnDisk(OldLevelName, &OldLevelName))
		{
			if (!PackageHelper.Delete(OldLevelName))
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Failed to delete previously converted level '%s'"), *Tokens[0]);
				return 1;
			}
		}
	}

	// Make sure the world isn't already partitioned
	AWorldSettings* MainWorldSettings = MainWorld->GetWorldSettings();
	if (MainWorldSettings->IsPartitionedWorld())
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Level '%s' is already partitioned"), *Tokens[0]);
		return 1;
	}

	// Setup the world partition object, do not create world partition object if only merging sublevels
	UWorldPartition* WorldPartition = bOnlyMergeSubLevels ? nullptr : CreateWorldPartition(MainWorldSettings);
	
	if (!bOnlyMergeSubLevels && !WorldPartition)
	{
		return 1;
	}

	// Initialize the world, create subsystems, etc.
	ULevel* MainLevel = InitWorld(MainWorld);
	if (!MainLevel)
	{
		UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Unknown level '%s'"), *Tokens[0]);
		return 1;
	}

	ON_SCOPE_EXIT
	{
		const bool bBroadcastWorldDestroyedEvent = false;
		MainWorld->DestroyWorld(bBroadcastWorldDestroyedEvent);
	};

	UPackage* MainPackage = MainLevel->GetPackage();
	AWorldDataLayers* MainWorldDataLayers = MainWorld->GetWorldDataLayers();
	// DataLayers are only needed if converting to WorldPartition
	check(bOnlyMergeSubLevels || MainWorldDataLayers);

	OnWorldLoaded(MainWorld);

	auto PartitionFoliage = [this, MainWorld](AInstancedFoliageActor* IFA) -> bool
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PartitionFoliage);

		TMap<UFoliageType*, TArray<FFoliageInstance>> FoliageToAdd;
		int32 NumInstances = 0;
		int32 NumInstancesProcessed = 0;

		bool bAddFoliageSucceeded = IFA->ForEachFoliageInfo([IFA, &FoliageToAdd, &NumInstances, this](UFoliageType* FoliageType, FFoliageInfo& FoliageInfo) -> bool
		{
			if (FoliageInfo.Type == EFoliageImplType::Actor)
			{
				// We don't support Actor Foliage in WP
				FoliageInfo.ExcludeActors();
				return true;
			}

			UFoliageType* FoliageTypeToAdd = FoliageType;

			if (FoliageType->GetTypedOuter<AInstancedFoliageActor>() != nullptr)
			{
				UFoliageType* NewFoliageType = nullptr;
				
				if (!FoliageTypePath.IsEmpty())
				{
					UObject* FoliageSource = FoliageType->GetSource();
					const FString BaseAssetName = (FoliageSource != nullptr) ? FoliageSource->GetName() : FoliageType->GetName();
					FString PackageName = FoliageTypePath / BaseAssetName + TEXT("_FoliageType");

					NewFoliageType = FFoliageEditUtility::DuplicateFoliageTypeToNewPackage(PackageName, FoliageType);
				}

				if (NewFoliageType == nullptr)
				{
					UE_LOG(LogWorldPartitionConvertCommandlet, Error,
						   TEXT("Level contains embedded FoliageType settings: please save the FoliageType setting assets, ")
						   TEXT("use the SaveFoliageTypeToContentFolder switch, ")
						   TEXT("specify FoliageTypePath in configuration file or the commandline."));
					return false;
				}

				FoliageTypeToAdd = NewFoliageType;
				PackagesToSave.Add(NewFoliageType->GetOutermost());
			}

			if (FoliageInfo.Instances.Num() > 0)
			{
				check(FoliageTypeToAdd->GetTypedOuter<AInstancedFoliageActor>() == nullptr);

				FoliageToAdd.FindOrAdd(FoliageTypeToAdd).Append(FoliageInfo.Instances);
				NumInstances += FoliageInfo.Instances.Num();
				UE_LOG(LogWorldPartitionConvertCommandlet, Display, TEXT("FoliageType: %s Count: %d"), *FoliageTypeToAdd->GetName(), FoliageInfo.Instances.Num());
			}

			return true;
		});

		if (!bAddFoliageSucceeded)
		{
			return false;
		}

		IFA->GetLevel()->GetWorld()->DestroyActor(IFA);

		// Add Foliage to those actors
		for (auto& InstancesPerFoliageType : FoliageToAdd)
		{
			for (const FFoliageInstance& Instance : InstancesPerFoliageType.Value)
			{
				AInstancedFoliageActor* GridIFA = AInstancedFoliageActor::Get(MainWorld, /*bCreateIfNone=*/true, MainWorld->PersistentLevel, Instance.Location);
				FFoliageInfo* NewFoliageInfo = nullptr;
				UFoliageType* NewFoliageType = GridIFA->AddFoliageType(InstancesPerFoliageType.Key, &NewFoliageInfo);
				NewFoliageInfo->AddInstance(NewFoliageType, Instance);
				NumInstancesProcessed++;
			}
		}

		check(NumInstances == NumInstancesProcessed);

		return true;
	};

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	auto ConvertActorLayersToDataLayers = [this, MainWorldDataLayers, &AssetTools](AActor* Actor)
	{
		// Convert Layers into DataLayers with DynamicallyLoaded flag disabled
		if (Actor->SupportsDataLayerType(UDataLayerInstance::StaticClass()))
		{
			for (FName Layer : Actor->Layers)
			{
				FString DataLayerAssetName = SlugStringForValidName(Layer.ToString());
				FName DataLayerAssetPathName(DataLayerAssetFolder + DataLayerAssetName + TEXT(".") + DataLayerAssetName);
				UDataLayerInstance* DataLayerInstance = const_cast<UDataLayerInstance*>(MainWorldDataLayers->GetDataLayerInstanceFromAssetName(DataLayerAssetPathName));
				if (!DataLayerInstance)
				{
					if (UObject* Asset = AssetTools.CreateAsset(DataLayerAssetName, DataLayerAssetFolder, UDataLayerAsset::StaticClass(), DataLayerFactory))
					{
						PackagesToSave.Add(Asset->GetPackage());
						UDataLayerAsset* DataLayerAsset = CastChecked<UDataLayerAsset>(Asset);
						DataLayerAsset->SetType(EDataLayerType::Editor);
						DataLayerInstance = MainWorldDataLayers->CreateDataLayer<UDataLayerInstanceWithAsset>(DataLayerAsset);
					}
				}
				if (DataLayerInstance)
				{
					Actor->AddDataLayer(DataLayerInstance);
				}
			}
		}
		// Clear actor layers as they are replaced by data layers, keep them if only merging
		if (!bOnlyMergeSubLevels)
		{
			Actor->Layers.Empty();
		}
	};

	auto PrepareLevelActors = [this, PartitionFoliage, MainWorld, ConvertActorLayersToDataLayers](ULevel* Level, TArray<AActor*>& Actors, bool bMainLevel) -> bool
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PrepareLevelActors);

		const FBox WorldBounds(WorldOrigin - WorldExtent, WorldOrigin + WorldExtent);

		TArray<AInstancedFoliageActor*> IFAs;
		TSet<ULandscapeInfo*> LandscapeInfos;

		for (auto Iter = Actors.CreateIterator(); Iter; ++Iter)
		{
			AActor* Actor = *Iter;

			if (IsValid(Actor))
			{
				check(Actor->GetLevel() == Level);

				if (ShouldDeleteActor(Actor, bMainLevel))
				{
					// Delete actor if processing main level, otherwise just ignore them
					if (bMainLevel)
					{
						Level->GetWorld()->DestroyActor(Actor);
					}
					else
					{
						Iter.RemoveCurrent();
					}
				}
				else 
				{
					if (AInstancedFoliageActor* IFA = Cast<AInstancedFoliageActor>(Actor))
					{
						IFAs.Add(IFA);
					}
					else if (ALandscapeProxy* LandscapeProxy = Cast<ALandscapeProxy>(Actor))
					{
						ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
						check(LandscapeInfo);
						LandscapeInfos.Add(LandscapeInfo);
					}
					// Only override default grid placement on actors that are spatially loaded
					else if (Actor->GetIsSpatiallyLoaded() && Actor->CanChangeIsSpatiallyLoadedFlag())
					{
						const FBox ActorBounds = Actor->GetStreamingBounds();

						if (!WorldBounds.IsInside(ActorBounds))
						{
							Actor->SetIsSpatiallyLoaded(false);
						}
					}

					if (bMainLevel)
					{
						ConvertActorLayersToDataLayers(Actor);
					}
				}
			}
		}

		if (bMainLevel)
		{
			if (LevelHasLevelScriptBlueprint(Level))
			{
				ULevelScriptBlueprint* LevelScriptBlueprint = Level->GetLevelScriptBlueprint(true);
				const ActorsReferencesUtils::FGetActorReferencesParams Params(LevelScriptBlueprint);
				TArray<AActor*> LevelScriptActorReferences;
				Algo::Transform(ActorsReferencesUtils::GetActorReferences(Params), LevelScriptActorReferences, [](const ActorsReferencesUtils::FActorReference& ActorReference) { return ActorReference.Actor; });

				for (AActor* LevelScriptActorReference : LevelScriptActorReferences)
				{
					if (LevelScriptActorReference->GetIsSpatiallyLoaded() && LevelScriptActorReference->CanChangeIsSpatiallyLoadedFlag())
					{
						LevelScriptActorReference->SetIsSpatiallyLoaded(false);
					}
				}
			}
		}

		if (!bOnlyMergeSubLevels)
		{
			// do loop after as it may modify Level->Actors
			if (IFAs.Num())
			{
				UE_SCOPED_TIMER(TEXT("PartitionFoliage"), LogWorldPartitionConvertCommandlet, Display);

				for (AInstancedFoliageActor* IFA : IFAs)
				{
					if (!PartitionFoliage(IFA))
					{
						return false;
					}
				}
			}

			if (LandscapeInfos.Num())
			{
				UE_SCOPED_TIMER(TEXT("PartitionLandscape"), LogWorldPartitionConvertCommandlet, Display);

				for (ULandscapeInfo* LandscapeInfo : LandscapeInfos)
				{
					FLandscapeConfigHelper::PartitionLandscape(MainWorld, LandscapeInfo, LandscapeGridSize);
				}
			}
		}

		return true;
	};

	// Gather and load sublevels
	TArray<ULevel*> SubLevelsToConvert;
	GatherAndPrepareSubLevelsToConvert(MainLevel, SubLevelsToConvert);

	if (!GetAdditionalLevelsToConvert(MainLevel, SubLevelsToConvert))
	{
		return 1;
	}

	// Validate levels for conversion
	bool bSkipStableGUIDValidation = Switches.Contains(TEXT("SkipStableGUIDValidation"));
	if (!bSkipStableGUIDValidation)
	{
		bool bNeedsResaveSubLevels = false;

		for (ULevel* Level: SubLevelsToConvert)
		{
			if (!Level->bContainsStableActorGUIDs)
			{
				bNeedsResaveSubLevels |= true;
				UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Unable to convert level '%s' with non-stable actor GUIDs. Resave the level before converting."), *Level->GetPackage()->GetName());
			}
		}

		if (bNeedsResaveSubLevels)
		{
			return 1;
		}
	}

	// Prepare levels for conversion
	DetachDependantLevelPackages(MainLevel);

	if (!PrepareLevelActors(MainLevel, MutableView(MainLevel->Actors), true)) 
	{
		return 1;
	}
	
	PackagesToSave.Add(MainLevel->GetPackage());

	if (bConversionSuffix)
	{
		FString OldMainWorldPath = FSoftObjectPath(MainWorld).ToString();
		FString OldMainLevelPath = FSoftObjectPath(MainLevel).ToString();
		FString OldPackagePath = FSoftObjectPath(MainPackage).ToString();

		if (!RenameWorldPackageWithSuffix(MainWorld))
		{
			return 1;
		}

		RemapSoftObjectPaths.Add(OldMainWorldPath, FSoftObjectPath(MainWorld).ToString());
		RemapSoftObjectPaths.Add(OldMainLevelPath, FSoftObjectPath(MainLevel).ToString());
		RemapSoftObjectPaths.Add(OldPackagePath, FSoftObjectPath(MainPackage).ToString());
	}

	TMap<UObject*, AActor*> PrivateRefsMap;
	for(ULevel* SubLevel : SubLevelsToConvert)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(ConvertSubLevel);

		UWorld* SubWorld = SubLevel->GetTypedOuter<UWorld>();
		UPackage* SubPackage = SubLevel->GetPackage();

		RemapSoftObjectPaths.Add(FSoftObjectPath(SubWorld).ToString(), FSoftObjectPath(MainWorld).ToString());
		RemapSoftObjectPaths.Add(FSoftObjectPath(SubLevel).ToString(), FSoftObjectPath(MainLevel).ToString());
		RemapSoftObjectPaths.Add(FSoftObjectPath(SubPackage).ToString(), FSoftObjectPath(MainPackage).ToString());

		TArray<AActor*> ActorsToConvert;
		if (LevelHasLevelScriptBlueprint(SubLevel))
		{
			MapsWithLevelScriptsBPs.Add(SubPackage->GetLoadedPath().GetPackageName());

			if (bConvertActorsNotReferencedByLevelScript)
			{
				// Gather the list of actors referenced by the level script blueprint
				TSet<AActor*> LevelScriptActorReferences;

				ALevelScriptActor* LevelScriptActor = SubLevel->GetLevelScriptActor();
				LevelScriptActorReferences.Add(LevelScriptActor);

				ULevelScriptBlueprint* LevelScriptBlueprint = SubLevel->GetLevelScriptBlueprint(true);
				const ActorsReferencesUtils::FGetActorReferencesParams LevelScriptBlueprintReferencesParams = ActorsReferencesUtils::FGetActorReferencesParams(LevelScriptBlueprint)
					.SetRecursive(true);
				Algo::Transform(ActorsReferencesUtils::GetActorReferences(LevelScriptBlueprintReferencesParams), LevelScriptActorReferences, [](const ActorsReferencesUtils::FActorReference& ActorReference) { return ActorReference.Actor; });

				for(AActor* Actor: SubLevel->Actors)
				{
					if(IsValid(Actor))
					{
						// Since we'll keep this level around, pass bMainLevel true here to ensure that we 
						// delete only unwanted actors, and keep level specific actors (world settings, brush and level script)
						if (ShouldDeleteActor(Actor, /*bMainLevel=*/true))
						{
							SubLevel->GetWorld()->DestroyActor(Actor);
						}
						else
						{
							TSet<AActor*> ActorReferences;
							const ActorsReferencesUtils::FGetActorReferencesParams ActorReferencesParams = ActorsReferencesUtils::FGetActorReferencesParams(Actor)
								.SetRecursive(true);
							Algo::Transform(ActorsReferencesUtils::GetActorReferences(ActorReferencesParams), ActorReferences, [](const ActorsReferencesUtils::FActorReference& ActorReference) { return ActorReference.Actor; });

							for (AActor* ActorReference : ActorReferences)
							{
								if (LevelScriptActorReferences.Find(ActorReference))
								{
									LevelScriptActorReferences.Add(Actor);
									LevelScriptActorReferences.Append(ActorReferences);
									break;
								}
							}
						}
					}
				}

				for(AActor* Actor: SubLevel->Actors)
				{
					if(IsValid(Actor))
					{
						if (!LevelScriptActorReferences.Find(Actor))
						{
							ActorsToConvert.Add(Actor);
						}
						else
						{
							RemapSoftObjectPaths.Add(FSoftObjectPath(Actor).ToString(), FSoftObjectPath(Actor).ToString());
						}
					}
				}
			}

			// Rename the world if requested
			UWorld* SubLevelWorld = SubLevel->GetTypedOuter<UWorld>();
			UPackage* SubLevelPackage = SubLevelWorld->GetPackage();

			if (bConversionSuffix)
			{
				FString OldMainWorldPath = FSoftObjectPath(SubLevelWorld).ToString();
				FString OldMainLevelPath = FSoftObjectPath(SubLevel).ToString();
				FString OldPackagePath = FSoftObjectPath(SubLevelPackage).ToString();

				if (!RenameWorldPackageWithSuffix(SubLevelWorld))
				{
					return 1;
				}

				RemapSoftObjectPaths.Add(OldMainWorldPath, FSoftObjectPath(SubLevelWorld).ToString());
				RemapSoftObjectPaths.Add(OldMainLevelPath, FSoftObjectPath(SubLevel).ToString());
				RemapSoftObjectPaths.Add(OldPackagePath, FSoftObjectPath(SubLevelPackage).ToString());
			}
			
			PackagesToSave.Add(SubLevelPackage);

			// Spawn the level instance actor
			ULevelStreaming* SubLevelStreaming = nullptr;
			for (ULevelStreaming* LevelStreaming : MainWorld->GetStreamingLevels())
			{
				if (LevelStreaming->GetLoadedLevel() == SubLevel)
				{
					SubLevelStreaming = LevelStreaming;
					break;
				}
			}
			check(SubLevelStreaming);

			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = MainLevel;
			ALevelInstance* LevelInstanceActor = MainWorld->SpawnActor<ALevelInstance>(SpawnParams);
			
			FTransform LevelTransform;
			if (SubLevelPackage->GetWorldTileInfo())
			{
				LevelTransform = FTransform(FVector(SubLevelPackage->GetWorldTileInfo()->Position));
			}
			else
			{
				LevelTransform = SubLevelStreaming->LevelTransform;
			}

			LevelInstanceActor->DesiredRuntimeBehavior = ELevelInstanceRuntimeBehavior::LevelStreaming;
			LevelInstanceActor->SetActorTransform(LevelTransform);
			LevelInstanceActor->SetWorldAsset(SubLevelWorld);
			LevelInstanceActor->SetActorLabel(SubLevelWorld->GetName());
		}
		else
		{
			if (LevelHasMapBuildData(SubLevel))
			{
				MapsWithMapBuildData.Add(SubPackage->GetLoadedPath().GetPackageName());
			}

			DetachDependantLevelPackages(SubLevel);

			ActorsToConvert = ObjectPtrDecay(SubLevel->Actors);
		}

		UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Converting %s"), *SubWorld->GetName());

		if (!PrepareLevelActors(SubLevel, ActorsToConvert, false))
		{
			return 1;
		}

		for(AActor* Actor: ActorsToConvert)
		{
			if(IsValid(Actor))
			{
				check(Actor->GetOuter() == SubLevel);
				check(!ShouldDeleteActor(Actor, false));
				
				if (Actor->IsA(AGroupActor::StaticClass()))
				{
					GroupActors.Add(*Actor->GetFullName());
				}

				if (Actor->GroupActor)
				{
					ActorsInGroupActors.Add(*Actor->GetFullName());
				}

				TArray<AActor*> ChildActors;
				Actor->GetAllChildActors(ChildActors, false);

				if (ChildActors.Num())
				{
					ActorsWithChildActors.Add(*Actor->GetFullName());
				}

				FArchiveGatherPrivateImports Ar(Actor, PrivateRefsMap, ActorsReferencesToActors);
				Actor->Serialize(Ar);

				// Even after Foliage Partitioning it is possible some Actors still have a FoliageTag. Make sure it is removed.
				if (FFoliageHelper::IsOwnedByFoliage(Actor))
				{
					FFoliageHelper::SetIsOwnedByFoliage(Actor, false);
				}

				ChangeObjectOuter(Actor, MainLevel);

				// Migrate blueprint classes
				UClass* ActorClass = Actor->GetClass();
				if (!ActorClass->IsNative() && (ActorClass->GetPackage() == SubPackage))
				{
					ChangeObjectOuter(ActorClass, MainPackage);
					UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Extracted non-native class %s"), *ActorClass->GetName());
				}

				// Actor is now in main level, we can create data layers for it
				ConvertActorLayersToDataLayers(Actor);
			}
		}

		if (!LevelHasLevelScriptBlueprint(SubLevel))
		{
			if (!bReportOnly)
			{
				TArray<UObject*> ObjectsToRename;
				ForEachObjectWithPackage(SubPackage, [&](UObject* Object)
				{
					if(!Object->IsA<AActor>() && !Object->IsA<ULevel>() && !Object->IsA<UWorld>() && !Object->IsA<UMetaData>())
					{
						ObjectsToRename.Add(Object);
					}
					return true;
				}, /*bIncludeNestedObjects*/false);

				for(UObject* ObjectToRename: ObjectsToRename)
				{
					ChangeObjectOuter(ObjectToRename, MainPackage);
					UE_LOG(LogWorldPartitionConvertCommandlet, Warning, TEXT("Renamed orphan object %s"), *ObjectToRename->GetName());
				}

				PackagesToDelete.Add(SubLevel->GetPackage());
			}
		}
		else
		{
			// Rebuild Model to clear refs to actors that were converted
			GEditor->RebuildLevel(*SubLevel);
		}
	}

	// Clear streaming levels
	for (ULevelStreaming* LevelStreaming: MainWorld->GetStreamingLevels())
	{
		LevelStreaming->MarkAsGarbage();
		ULevelStreaming::RemoveLevelAnnotation(LevelStreaming->GetLoadedLevel());
		MainWorld->RemoveLevel(LevelStreaming->GetLoadedLevel());
	}
	MainWorld->ClearStreamingLevels();

	// Fixup SoftObjectPaths
	FixupSoftObjectPaths(MainPackage);

	PerformAdditionalWorldCleanup(MainWorld);

	bool bForceInitializeWorld = false;
	bool bInitializedPhysicsSceneForSave = GEditor->InitializePhysicsSceneForSaveIfNecessary(MainWorld, bForceInitializeWorld);

	// After conversion, convert actors to external actors
	UPackage* LevelPackage = MainLevel->GetPackage();

	TArray<AActor*> ActorList;
	TArray<AActor*> ChildActorList;
	ActorList.Reserve(MainLevel->Actors.Num());

	// Move child actors at the end of the list
	for (AActor* Actor: MainLevel->Actors)
	{
		if (IsValid(Actor))
		{
			check(Actor->GetLevel() == MainLevel);
			check(Actor->GetActorGuid().IsValid());

			if (Actor->GetParentActor())
			{
				ChildActorList.Add(Actor);
			}
			else
			{
				ActorList.Add(Actor);
			}
		}
	}

	ActorList.Append(ChildActorList);
	ChildActorList.Empty();

	if (!bOnlyMergeSubLevels)
	{
		WorldPartition->AddToRoot();
	}

	if (!bReportOnly)
	{
		FLevelActorFoldersHelper::SetUseActorFolders(MainLevel, true);
		MainLevel->SetUseExternalActors(true);

		MainLevel->ForEachActorFolder([this](UActorFolder* ActorFolder)
		{
			if (ActorFolder->IsPackageExternal())
			{
				PackagesToDelete.Add(ActorFolder->GetExternalPackage());
				ActorFolder->SetPackageExternal(false);
			}
			ActorFolder->SetPackageExternal(true);
			return true;
		});

		TSet<FGuid> ActorGuids;
		for(AActor* Actor: ActorList)
		{
			if (!IsValid(Actor) || !Actor->SupportsExternalPackaging())
			{
				continue;
			}

			bool bAlreadySet = false;
			ActorGuids.Add(Actor->GetActorGuid(), &bAlreadySet);
			if (bAlreadySet)
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Duplicated guid actor %s(guid:%s) can't extract actor"), *Actor->GetName(), *Actor->GetActorGuid().ToString(EGuidFormats::Digits));
				return 1;
			}

			if (Actor->IsPackageExternal())
			{
				PackagesToDelete.Add(Actor->GetPackage());
				Actor->SetPackageExternal(false);
			}
						
			Actor->SetPackageExternal(true);

			if (!Actor->CreateOrUpdateActorFolder())
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Failed to convert actor %s folder to persistent folder."), *Actor->GetName());
			}
			
			UPackage* ActorPackage = Actor->GetExternalPackage();
			PackagesToSave.Add(ActorPackage);

			UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Extracted actor %s(guid:%s) in %s"), *Actor->GetName(), *Actor->GetActorGuid().ToString(EGuidFormats::Digits), *ActorPackage->GetName());
		}

		// Required to clear any deleted actors from the level
		CollectGarbage(RF_Standalone);

		for (AActor* Actor : ActorList)
		{
			if (!IsValid(Actor))
			{
				continue;
			}

			PerformAdditionalActorChanges(Actor);
		}

		MainLevel->ForEachActorFolder([this](UActorFolder* ActorFolder)
		{
			UPackage* ActorFolderPackage = ActorFolder->GetExternalPackage();
			check(ActorFolderPackage);
			PackagesToSave.Add(ActorFolderPackage);
			return true;
		});

		MainWorld->WorldComposition = nullptr;
		MainLevel->bIsPartitioned = !bOnlyMergeSubLevels;
		GEditor->RebuildLevel(*MainLevel);

		if (bDeleteSourceLevels)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(DeleteSourceLevels);

			if (!PackageHelper.Delete(PackagesToDelete))
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Failed to delete source level package(s)"));
				return 1;
			}
		}

		// Checkout packages
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(CheckoutPackages);

			UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Checking out %d packages."), PackagesToSave.Num());

			TArray<FString> FilesToCheckout;
			FilesToCheckout.Reserve(PackagesToSave.Num());

			for(UPackage* Package: PackagesToSave)
			{
				FString PackageFileName = SourceControlHelpers::PackageFilename(Package);
				if (FPlatformFileManager::Get().GetPlatformFile().FileExists(*PackageFileName))
				{
					FilesToCheckout.Emplace(MoveTemp(PackageFileName));
				}
			}

			if (!PackageHelper.Checkout(FilesToCheckout))
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Failed to checkout package(s)"));
				return 1;
			}
		}

		SET_WARN_COLOR(COLOR_YELLOW);
		bool bRemapError = false;
		for (TMap<UObject*, AActor*>::TConstIterator It(PrivateRefsMap); It; ++It)
		{
			check(It->Value->IsPackageExternal() == It->Value->SupportsExternalPackaging());
			if (It->Value->SupportsExternalPackaging())
			{
				UE_LOG(LogWorldPartitionConvertCommandlet, Warning, TEXT("Changing object %s package from %s to actor external package %s"), *It->Key->GetName(), *It->Key->GetPackage()->GetName(), *It->Value->GetPackage()->GetName());
				check(It->Value->GetExternalPackage());
				It->Key->SetExternalPackage(It->Value->GetExternalPackage());
			}
			else
			{
				// Remap obj's outer
				//
				// Before remapping, validate that object is still in a different package than the actor's package :
				// Calling Rename on an object can also affect other objects of PrivateRefsMap (UModel::Rename is one example).
				UPackage* ActorPackage = It->Value->GetPackage();
				if (!It->Key->IsInPackage(ActorPackage))
				{
					UObject* ObjectOuter = It->Key->GetOuter();
					FString* RemappedOuterPath = RemapSoftObjectPaths.Find(FSoftObjectPath(ObjectOuter).ToString());
					if (UObject* RemappedOuterObject = RemappedOuterPath ? FSoftObjectPath(*RemappedOuterPath).ResolveObject() : nullptr)
					{
						FString OldPathName = It->Key->GetPathName();
						It->Key->Rename(nullptr, RemappedOuterObject, REN_DontCreateRedirectors);
						UE_LOG(LogWorldPartitionConvertCommandlet, Warning, TEXT("Renamed object from %s to %s"), *OldPathName, *It->Key->GetPathName());
					}
					else
					{
						UE_LOG(LogWorldPartitionConvertCommandlet, Error, TEXT("Failed to find a corresponding outer for object %s."), *It->Key->GetPathName());
						bRemapError = true;
					}
				}
			}
		}
		if (bRemapError)
		{
			return 1;
		}
		CLEAR_WARN_COLOR();
	
		// Save packages
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SavePackages);

			UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("Saving %d packages."), PackagesToSave.Num());
			for (UPackage* PackageToSave : PackagesToSave)
			{
				FString PackageFileName = SourceControlHelpers::PackageFilename(PackageToSave);
				FSavePackageArgs SaveArgs;
				SaveArgs.TopLevelFlags = RF_Standalone;
				SaveArgs.SaveFlags = SAVE_Async;
				if (!UPackage::SavePackage(PackageToSave, nullptr, *PackageFileName, SaveArgs))
				{
					return 1;
				}
			}
			
			UPackage::WaitForAsyncFileWrites();
		}

		// Add packages
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(AddPackagesToSourceControl);

			// Add new packages to source control
			if(!PackageHelper.AddToSourceControl(PackagesToSave))
			{
				return 1;
			}
		}

		if(bInitializedPhysicsSceneForSave)
		{
			GEditor->CleanupPhysicsSceneThatWasInitializedForSave(MainWorld, bForceInitializeWorld);
		}

		UE_LOG(LogWorldPartitionConvertCommandlet, Log, TEXT("######## CONVERSION COMPLETED SUCCESSFULLY ########"));
	}

	if (bGenerateIni || !bReportOnly)
	{
		if (bGenerateIni || !FPlatformFileManager::Get().GetPlatformFile().FileExists(*LevelConfigFilename))
		{
			SaveConfig(CPF_Config, *LevelConfigFilename);

			if (!bOnlyMergeSubLevels)
			{
				WorldPartition->EditorHash->SaveConfig(CPF_Config, *LevelConfigFilename);
				WorldPartition->RuntimeHash->SaveConfig(CPF_Config, *LevelConfigFilename);
			}

			UE_LOG(LogWorldPartitionConvertCommandlet, Display, TEXT("Generated ini file: %s"), *LevelConfigFilename);
		}
	}

	UPackage::WaitForAsyncFileWrites();

	OutputConversionReport();

	return 0;
}

const FString UWorldPartitionConvertCommandlet::GetConversionSuffix(const bool bInOnlyMergeSubLevels)
{
	return bInOnlyMergeSubLevels ? TEXT("_OFPA") : TEXT("_WP");
}

bool UWorldPartitionConvertCommandlet::ShouldConvertStreamingLevel(ULevelStreaming* StreamingLevel)
{
	return StreamingLevel && !ExcludedLevels.Contains(StreamingLevel->GetWorldAssetPackageName());
}

