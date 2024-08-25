// Copyright Epic Games, Inc. All Rights Reserved.

/**
*	
*	===================== LyraReplicationGraph Replication =====================
*
*	Overview
*	
*		This changes the way actor relevancy works. AActor::IsNetRelevantFor is NOT used in this system!
*		
*		Instead, The ULyraReplicationGraph contains UReplicationGraphNodes. These nodes are responsible for generating lists of actors to replicate for each connection.
*		Most of these lists are persistent across frames. This enables most of the gathering work ("which actors should be considered for replication) to be shared/reused.
*		Nodes may be global (used by all connections), connection specific (each connection gets its own node), or shared (e.g, teams: all connections on the same team share).
*		Actors can be in multiple nodes! For example a pawn may be in the spatialization node but also in the always-relevant-for-team node. It will be returned twice for 
*		teammates. This is ok though should be minimized when possible.
*		
*		ULyraReplicationGraph is intended to not be directly used by the game code. That is, you should not have to include LyraReplicationGraph.h anywhere else.
*		Rather, ULyraReplicationGraph depends on the game code and registers for events that the game code broadcasts (e.g., events for players joining/leaving teams).
*		This choice was made because it gives ULyraReplicationGraph a complete holistic view of actor replication. Rather than exposing generic public functions that any
*		place in game code can invoke, all notifications are explicitly registered in ULyraReplicationGraph::InitGlobalActorClassSettings.
*		
*	Lyra Nodes
*	
*		These are the top level nodes currently used:
*		
*		UReplicationGraphNode_GridSpatialization2D: 
*		This is the spatialization node. All "distance based relevant" actors will be routed here. This node divides the map into a 2D grid. Each cell in the grid contains 
*		children nodes that hold lists of actors based on how they update/go dormant. Actors are put in multiple cells. Connections pull from the single cell they are in.
*		
*		UReplicationGraphNode_ActorList
*		This is an actor list node that contains the always relevant actors. These actors are always relevant to every connection.
*		
*		ULyraReplicationGraphNode_AlwaysRelevant_ForConnection
*		This is the node for connection specific always relevant actors. This node does not maintain a persistent list but builds it each frame. This is possible because (currently)
*		these actors are all easily accessed from the PlayerController. A persistent list would require notifications to be broadcast when these actors change, which would be possible
*		but currently not necessary.
*		
*		ULyraReplicationGraphNode_PlayerStateFrequencyLimiter
*		A custom node for handling player state replication. This replicates a small rolling set of player states (currently 2/frame). This is so player states replicate
*		to simulated connections at a low, steady frequency, and to take advantage of serialization sharing. Auto proxy player states are replicated at higher frequency (to the
*		owning connection only) via ULyraReplicationGraphNode_AlwaysRelevant_ForConnection.
*		
*		UReplicationGraphNode_TearOff_ForConnection
*		Connection specific node for handling tear off actors. This is created and managed in the base implementation of Replication Graph.
*	
*	How To Use
*	
*		Making something always relevant: Please avoid if you can :) If you must, just setting AActor::bAlwaysRelevant = true in the class defaults will do it.
*		
*		Making something always relevant to connection: You will need to modify ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection. You will also want 
*		to make sure the actor does not get put in one of the other nodes. The safest way to do this is by setting its EClassRepNodeMapping to NotRouted in ULyraReplicationGraph::InitGlobalActorClassSettings.
*
*	How To Debug
*	
*		Its a good idea to just disable rep graph to see if your problem is specific to this system or just general replication/game play problem.
*		
*		If it is replication graph related, there are several useful commands that can be used: see ReplicationGraph_Debugging.cpp. The most useful are below. Use the 'cheat' command to run these on the server from a client.
*	
*		"Net.RepGraph.PrintGraph" - this will print the graph to the log: each node and actor. 
*		"Net.RepGraph.PrintGraph class" - same as above but will group by class.
*		"Net.RepGraph.PrintGraph nclass" - same as above but will group by native classes (hides blueprint noise)
*		
*		Net.RepGraph.PrintAll <Frames> <ConnectionIdx> <"Class"/"Nclass"> -  will print the entire graph, the gathered actors, and how they were prioritized for a given connection for X amount of frames.
*		
*		Net.RepGraph.PrintAllActorInfo <ActorMatchString> - will print the class, global, and connection replication info associated with an actor/class. If MatchString is empty will print everything. Call directly from client.
*		
*		Lyra.RepGraph.PrintRouting - will print the EClassRepNodeMapping for each class. That is, how a given actor class is routed (or not) in the Replication Graph.
*	
*/

#include "LyraReplicationGraph.h"

#include "Net/UnrealNetwork.h"
#include "Engine/LevelStreaming.h"
#include "EngineUtils.h"
#include "CoreGlobals.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebuggerCategoryReplicator.h"
#endif

#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameState.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/NetConnection.h"
#include "UObject/UObjectIterator.h"

#include "LyraReplicationGraphSettings.h"
#include "Character/LyraCharacter.h"
#include "Player/LyraPlayerController.h"

DEFINE_LOG_CATEGORY( LogLyraRepGraph );

namespace Lyra::RepGraph
{
	float DestructionInfoMaxDist = 30000.f;
	static FAutoConsoleVariableRef CVarLyraRepGraphDestructMaxDist(TEXT("Lyra.RepGraph.DestructInfo.MaxDist"), DestructionInfoMaxDist, TEXT("Max distance (not squared) to rep destruct infos at"), ECVF_Default);

	int32 DisplayClientLevelStreaming = 0;
	static FAutoConsoleVariableRef CVarLyraRepGraphDisplayClientLevelStreaming(TEXT("Lyra.RepGraph.DisplayClientLevelStreaming"), DisplayClientLevelStreaming, TEXT(""), ECVF_Default);

	float CellSize = 10000.f;
	static FAutoConsoleVariableRef CVarLyraRepGraphCellSize(TEXT("Lyra.RepGraph.CellSize"), CellSize, TEXT(""), ECVF_Default);

	// Essentially "Min X" for replication. This is just an initial value. The system will reset itself if actors appears outside of this.
	float SpatialBiasX = -150000.f;
	static FAutoConsoleVariableRef CVarLyraRepGraphSpatialBiasX(TEXT("Lyra.RepGraph.SpatialBiasX"), SpatialBiasX, TEXT(""), ECVF_Default);

	// Essentially "Min Y" for replication. This is just an initial value. The system will reset itself if actors appears outside of this.
	float SpatialBiasY = -200000.f;
	static FAutoConsoleVariableRef CVarLyraRepSpatialBiasY(TEXT("Lyra.RepGraph.SpatialBiasY"), SpatialBiasY, TEXT(""), ECVF_Default);

	// How many buckets to spread dynamic, spatialized actors across. High number = more buckets = smaller effective replication frequency. This happens before individual actors do their own NetUpdateFrequency check.
	int32 DynamicActorFrequencyBuckets = 3;
	static FAutoConsoleVariableRef CVarLyraRepDynamicActorFrequencyBuckets(TEXT("Lyra.RepGraph.DynamicActorFrequencyBuckets"), DynamicActorFrequencyBuckets, TEXT(""), ECVF_Default);

	int32 DisableSpatialRebuilds = 1;
	static FAutoConsoleVariableRef CVarLyraRepDisableSpatialRebuilds(TEXT("Lyra.RepGraph.DisableSpatialRebuilds"), DisableSpatialRebuilds, TEXT(""), ECVF_Default);

	int32 LogLazyInitClasses = 0;
	static FAutoConsoleVariableRef CVarLyraRepLogLazyInitClasses(TEXT("Lyra.RepGraph.LogLazyInitClasses"), LogLazyInitClasses, TEXT(""), ECVF_Default);

	// How much bandwidth to use for FastShared movement updates. This is counted independently of the NetDriver's target bandwidth.
	int32 TargetKBytesSecFastSharedPath = 10;
	static FAutoConsoleVariableRef CVarLyraRepTargetKBytesSecFastSharedPath(TEXT("Lyra.RepGraph.TargetKBytesSecFastSharedPath"), TargetKBytesSecFastSharedPath, TEXT(""), ECVF_Default);

	float FastSharedPathCullDistPct = 0.80f;
	static FAutoConsoleVariableRef CVarLyraRepFastSharedPathCullDistPct(TEXT("Lyra.RepGraph.FastSharedPathCullDistPct"), FastSharedPathCullDistPct, TEXT(""), ECVF_Default);

	int32 EnableFastSharedPath = 1;
	static FAutoConsoleVariableRef CVarLyraRepEnableFastSharedPath(TEXT("Lyra.RepGraph.EnableFastSharedPath"), EnableFastSharedPath, TEXT(""), ECVF_Default);

	UReplicationDriver* ConditionalCreateReplicationDriver(UNetDriver* ForNetDriver, UWorld* World)
	{
		// Only create for GameNetDriver
		if (World && ForNetDriver && ForNetDriver->NetDriverName == NAME_GameNetDriver)
		{
			const ULyraReplicationGraphSettings* LyraRepGraphSettings = GetDefault<ULyraReplicationGraphSettings>();

			// Enable/Disable via developer settings
			if (LyraRepGraphSettings && LyraRepGraphSettings->bDisableReplicationGraph)
			{
				UE_LOG(LogLyraRepGraph, Display, TEXT("Replication graph is disabled via LyraReplicationGraphSettings."));
				return nullptr;
			}

			UE_LOG(LogLyraRepGraph, Display, TEXT("Replication graph is enabled for %s in world %s."), *GetNameSafe(ForNetDriver), *GetPathNameSafe(World));

			TSubclassOf<ULyraReplicationGraph> GraphClass = LyraRepGraphSettings->DefaultReplicationGraphClass.TryLoadClass<ULyraReplicationGraph>();
			if (GraphClass.Get() == nullptr)
			{
				GraphClass = ULyraReplicationGraph::StaticClass();
			}

			ULyraReplicationGraph* LyraReplicationGraph = NewObject<ULyraReplicationGraph>(GetTransientPackage(), GraphClass.Get());
			return LyraReplicationGraph;
		}

		return nullptr;
	}
};

// ----------------------------------------------------------------------------------------------------------

ULyraReplicationGraph::ULyraReplicationGraph()
{
	if (!UReplicationDriver::CreateReplicationDriverDelegate().IsBound())
	{
		UReplicationDriver::CreateReplicationDriverDelegate().BindLambda(
			[](UNetDriver* ForNetDriver, const FURL& URL, UWorld* World) -> UReplicationDriver*
			{
				return Lyra::RepGraph::ConditionalCreateReplicationDriver(ForNetDriver, World);
			});
	}
}

void ULyraReplicationGraph::ResetGameWorldState()
{
	Super::ResetGameWorldState();

	AlwaysRelevantStreamingLevelActors.Empty();

	for (UNetReplicationGraphConnection* ConnManager : Connections)
	{
		for (UReplicationGraphNode* ConnectionNode : ConnManager->GetConnectionGraphNodes())
		{
			if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
			{
				AlwaysRelevantConnectionNode->ResetGameWorldState();
			}
		}
	}

	for (UNetReplicationGraphConnection* ConnManager : PendingConnections)
	{
		for (UReplicationGraphNode* ConnectionNode : ConnManager->GetConnectionGraphNodes())
		{
			if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
			{
				AlwaysRelevantConnectionNode->ResetGameWorldState();
			}
		}
	}
}

EClassRepNodeMapping ULyraReplicationGraph::GetClassNodeMapping(UClass* Class) const
{
	if (!Class)
	{
		return EClassRepNodeMapping::NotRouted;
	}
	
	if (const EClassRepNodeMapping* Ptr = ClassRepNodePolicies.FindWithoutClassRecursion(Class))
	{
		return *Ptr;
	}
	
	AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
	if (!ActorCDO || !ActorCDO->GetIsReplicated())
	{
		return EClassRepNodeMapping::NotRouted;
	}
		
	auto ShouldSpatialize = [](const AActor* CDO)
	{
		return CDO->GetIsReplicated() && (!(CDO->bAlwaysRelevant || CDO->bOnlyRelevantToOwner || CDO->bNetUseOwnerRelevancy));
	};

	auto GetLegacyDebugStr = [](const AActor* CDO)
	{
		return FString::Printf(TEXT("%s [%d/%d/%d]"), *CDO->GetClass()->GetName(), CDO->bAlwaysRelevant, CDO->bOnlyRelevantToOwner, CDO->bNetUseOwnerRelevancy);
	};

	// Only handle this class if it differs from its super. There is no need to put every child class explicitly in the graph class mapping
	UClass* SuperClass = Class->GetSuperClass();
	if (AActor* SuperCDO = Cast<AActor>(SuperClass->GetDefaultObject()))
	{
		if (SuperCDO->GetIsReplicated() == ActorCDO->GetIsReplicated()
			&& SuperCDO->bAlwaysRelevant == ActorCDO->bAlwaysRelevant
			&& SuperCDO->bOnlyRelevantToOwner == ActorCDO->bOnlyRelevantToOwner
			&& SuperCDO->bNetUseOwnerRelevancy == ActorCDO->bNetUseOwnerRelevancy
			)
		{
			return GetClassNodeMapping(SuperClass);
		}
	}

	if (ShouldSpatialize(ActorCDO))
	{
		return EClassRepNodeMapping::Spatialize_Dynamic;
	}
	else if (ActorCDO->bAlwaysRelevant && !ActorCDO->bOnlyRelevantToOwner)
	{
		return EClassRepNodeMapping::RelevantAllConnections;
	}

	return EClassRepNodeMapping::NotRouted;
}

void ULyraReplicationGraph::RegisterClassRepNodeMapping(UClass* Class)
{
	EClassRepNodeMapping Mapping = GetClassNodeMapping(Class);
	ClassRepNodePolicies.Set(Class, Mapping);
}

void ULyraReplicationGraph::InitClassReplicationInfo(FClassReplicationInfo& Info, UClass* Class, bool Spatialize) const
{
	AActor* CDO = Class->GetDefaultObject<AActor>();
	if (Spatialize)
	{
		Info.SetCullDistanceSquared(CDO->NetCullDistanceSquared);
		UE_LOG(LogLyraRepGraph, Log, TEXT("Setting cull distance for %s to %f (%f)"), *Class->GetName(), Info.GetCullDistanceSquared(), Info.GetCullDistance());
	}

	Info.ReplicationPeriodFrame = GetReplicationPeriodFrameForFrequency(CDO->NetUpdateFrequency);

	UClass* NativeClass = Class;
	while (!NativeClass->IsNative() && NativeClass->GetSuperClass() && NativeClass->GetSuperClass() != AActor::StaticClass())
	{
		NativeClass = NativeClass->GetSuperClass();
	}

	UE_LOG(LogLyraRepGraph, Log, TEXT("Setting replication period for %s (%s) to %d frames (%.2f)"), *Class->GetName(), *NativeClass->GetName(), Info.ReplicationPeriodFrame, CDO->NetUpdateFrequency);
}

bool ULyraReplicationGraph::ConditionalInitClassReplicationInfo(UClass* ReplicatedClass, FClassReplicationInfo& ClassInfo)
{
	if (ExplicitlySetClasses.FindByPredicate([&](const UClass* SetClass) { return ReplicatedClass->IsChildOf(SetClass); }) != nullptr)
	{
		return false;
	}

	bool ClassIsSpatialized = IsSpatialized(ClassRepNodePolicies.GetChecked(ReplicatedClass));
	InitClassReplicationInfo(ClassInfo, ReplicatedClass, ClassIsSpatialized);
	return true;
}

void ULyraReplicationGraph::AddClassRepInfo(UClass* Class, EClassRepNodeMapping Mapping)
{
	if (IsSpatialized(Mapping))
	{
		if (Class->GetDefaultObject<AActor>()->bAlwaysRelevant)
		{
			UE_LOG(LogLyraRepGraph, Warning, TEXT("Replicated Class %s is AlwaysRelevant but is initialized into a spatialized node (%s)"), *Class->GetName(), *StaticEnum<EClassRepNodeMapping>()->GetNameStringByValue((int64)Mapping));
		}
	}

	ClassRepNodePolicies.Set(Class, Mapping);
}

void ULyraReplicationGraph::RegisterClassReplicationInfo(UClass* ReplicatedClass)
{
	FClassReplicationInfo ClassInfo;
	if (ConditionalInitClassReplicationInfo(ReplicatedClass, ClassInfo))
	{
		GlobalActorReplicationInfoMap.SetClassInfo(ReplicatedClass, ClassInfo);
		UE_LOG(LogLyraRepGraph, Log, TEXT("Setting %s - %.2f"), *GetNameSafe(ReplicatedClass), ClassInfo.GetCullDistance());
	}
}

void ULyraReplicationGraph::InitGlobalActorClassSettings()
{
	// Setup our lazy init function for classes that are not currently loaded.
	GlobalActorReplicationInfoMap.SetInitClassInfoFunc(
		[this](UClass* Class, FClassReplicationInfo& ClassInfo)
		{
			RegisterClassRepNodeMapping(Class); // This needs to run before RegisterClassReplicationInfo.

			const bool bHandled = ConditionalInitClassReplicationInfo(Class, ClassInfo);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (Lyra::RepGraph::LogLazyInitClasses != 0)
			{
				if (bHandled)
				{
					EClassRepNodeMapping Mapping = ClassRepNodePolicies.GetChecked(Class);
					UE_LOG(LogLyraRepGraph, Warning, TEXT("%s was Lazy Initialized. (Parent: %s) %d."), *GetNameSafe(Class), *GetNameSafe(Class->GetSuperClass()), (int32)Mapping);

					FClassReplicationInfo& ParentRepInfo = GlobalActorReplicationInfoMap.GetClassInfo(Class->GetSuperClass());
					if (ClassInfo.BuildDebugStringDelta() != ParentRepInfo.BuildDebugStringDelta())
					{
						UE_LOG(LogLyraRepGraph, Warning, TEXT("Differences Found!"));
						FString DebugStr = ParentRepInfo.BuildDebugStringDelta();
						UE_LOG(LogLyraRepGraph, Warning, TEXT("  Parent: %s"), *DebugStr);

						DebugStr = ClassInfo.BuildDebugStringDelta();
						UE_LOG(LogLyraRepGraph, Warning, TEXT("  Class : %s"), *DebugStr);
					}
				}
				else
				{
					UE_LOG(LogLyraRepGraph, Warning, TEXT("%s skipped Lazy Initialization because it does not differ from its parent. (Parent: %s)"), *GetNameSafe(Class), *GetNameSafe(Class->GetSuperClass()));

				}
			}
#endif

			return bHandled;
		});

	ClassRepNodePolicies.InitNewElement = [this](UClass* Class, EClassRepNodeMapping& NodeMapping)
	{
		NodeMapping = GetClassNodeMapping(Class);
		return true;
	};

	const ULyraReplicationGraphSettings* LyraRepGraphSettings = GetDefault<ULyraReplicationGraphSettings>();
	check(LyraRepGraphSettings);

	// Set Classes Node Mappings
	for (const FRepGraphActorClassSettings& ActorClassSettings : LyraRepGraphSettings->ClassSettings)
	{
		if (ActorClassSettings.bAddClassRepInfoToMap)
		{
			if (UClass* StaticActorClass = ActorClassSettings.GetStaticActorClass())
			{
				UE_LOG(LogLyraRepGraph, Log, TEXT("ActorClassSettings -- AddClassRepInfo - %s :: %i"), *StaticActorClass->GetName(), int(ActorClassSettings.ClassNodeMapping));
				AddClassRepInfo(StaticActorClass, ActorClassSettings.ClassNodeMapping);
			}
		}
	}

#if WITH_GAMEPLAY_DEBUGGER
	AddClassRepInfo(AGameplayDebuggerCategoryReplicator::StaticClass(), EClassRepNodeMapping::NotRouted);				// Replicated via ULyraReplicationGraphNode_AlwaysRelevant_ForConnection
#endif

	TArray<UClass*> AllReplicatedClasses;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		AActor* ActorCDO = Cast<AActor>(Class->GetDefaultObject());
		if (!ActorCDO || !ActorCDO->GetIsReplicated())
		{
			continue;
		}

		// Skip SKEL and REINST classes. I don't know a better way to do this.
		if (Class->GetName().StartsWith(TEXT("SKEL_")) || Class->GetName().StartsWith(TEXT("REINST_")))
		{
			continue;
		}

		// --------------------------------------------------------------------
		// This is a replicated class. Save this off for the second pass below
		// --------------------------------------------------------------------

		AllReplicatedClasses.Add(Class);

		RegisterClassRepNodeMapping(Class);
	}

	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// Setup FClassReplicationInfo. This is essentially the per class replication settings. Some we set explicitly, the rest we are setting via looking at the legacy settings on AActor.
	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	auto SetClassInfo = [&](UClass* Class, const FClassReplicationInfo& Info) { GlobalActorReplicationInfoMap.SetClassInfo(Class, Info); ExplicitlySetClasses.Add(Class); };
	ExplicitlySetClasses.Reset();

	FClassReplicationInfo CharacterClassRepInfo;
	CharacterClassRepInfo.DistancePriorityScale = 1.f;
	CharacterClassRepInfo.StarvationPriorityScale = 1.f;
	CharacterClassRepInfo.ActorChannelFrameTimeout = 4;
	CharacterClassRepInfo.SetCullDistanceSquared(ALyraCharacter::StaticClass()->GetDefaultObject<ALyraCharacter>()->NetCullDistanceSquared);

	SetClassInfo(ACharacter::StaticClass(), CharacterClassRepInfo);

	{
		// Sanity check our FSharedRepMovement type has the same quantization settings as the default character.
		FRepMovement DefaultRepMovement = ALyraCharacter::StaticClass()->GetDefaultObject<ALyraCharacter>()->GetReplicatedMovement(); // Use the same quantization settings as our default replicatedmovement
		FSharedRepMovement SharedRepMovement;
		ensureMsgf(SharedRepMovement.RepMovement.LocationQuantizationLevel == DefaultRepMovement.LocationQuantizationLevel, TEXT("LocationQuantizationLevel mismatch. %d != %d"), (uint8)SharedRepMovement.RepMovement.LocationQuantizationLevel, (uint8)DefaultRepMovement.LocationQuantizationLevel);
		ensureMsgf(SharedRepMovement.RepMovement.VelocityQuantizationLevel == DefaultRepMovement.VelocityQuantizationLevel, TEXT("VelocityQuantizationLevel mismatch. %d != %d"), (uint8)SharedRepMovement.RepMovement.VelocityQuantizationLevel, (uint8)DefaultRepMovement.VelocityQuantizationLevel);
		ensureMsgf(SharedRepMovement.RepMovement.RotationQuantizationLevel == DefaultRepMovement.RotationQuantizationLevel, TEXT("RotationQuantizationLevel mismatch. %d != %d"), (uint8)SharedRepMovement.RepMovement.RotationQuantizationLevel, (uint8)DefaultRepMovement.RotationQuantizationLevel);
	}

	// ------------------------------------------------------------------------------------------------------
	//	Setup FastShared replication for pawns. This is called up to once per frame per pawn to see if it wants
	//	to send a FastShared update to all relevant connections.
	// ------------------------------------------------------------------------------------------------------
	CharacterClassRepInfo.FastSharedReplicationFunc = [](AActor* Actor)
	{
		bool bSuccess = false;
		if (ALyraCharacter* Character = Cast<ALyraCharacter>(Actor))
		{
			bSuccess = Character->UpdateSharedReplication();
		}
		return bSuccess;
	};

	CharacterClassRepInfo.FastSharedReplicationFuncName = FName(TEXT("FastSharedReplication"));

	FastSharedPathConstants.MaxBitsPerFrame = (int32)((float)(Lyra::RepGraph::TargetKBytesSecFastSharedPath * 1024 * 8) / NetDriver->GetNetServerMaxTickRate());
	FastSharedPathConstants.DistanceRequirementPct = Lyra::RepGraph::FastSharedPathCullDistPct;

	SetClassInfo(ALyraCharacter::StaticClass(), CharacterClassRepInfo);

	// ---------------------------------------------------------------------
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.ListSize = 12;
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.NumBuckets = Lyra::RepGraph::DynamicActorFrequencyBuckets;
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.BucketThresholds.Reset();
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.EnableFastPath = (Lyra::RepGraph::EnableFastSharedPath > 0);
	UReplicationGraphNode_ActorListFrequencyBuckets::DefaultSettings.FastPathFrameModulo = 1;

	RPCSendPolicyMap.Reset();

	// Set FClassReplicationInfo based on legacy settings from all replicated classes
	for (UClass* ReplicatedClass : AllReplicatedClasses)
	{
		RegisterClassReplicationInfo(ReplicatedClass);
	}

	// Print out what we came up with
	UE_LOG(LogLyraRepGraph, Log, TEXT(""));
	UE_LOG(LogLyraRepGraph, Log, TEXT("Class Routing Map: "));
	for (auto ClassMapIt = ClassRepNodePolicies.CreateIterator(); ClassMapIt; ++ClassMapIt)
	{
		UClass* Class = CastChecked<UClass>(ClassMapIt.Key().ResolveObjectPtr());
		EClassRepNodeMapping Mapping = ClassMapIt.Value();

		// Only print if different than native class
		UClass* ParentNativeClass = GetParentNativeClass(Class);

		EClassRepNodeMapping* ParentMapping = ClassRepNodePolicies.Get(ParentNativeClass);
		if (ParentMapping && Class != ParentNativeClass && Mapping == *ParentMapping)
		{
			continue;
		}

		UE_LOG(LogLyraRepGraph, Log, TEXT("  %s (%s) -> %s"), *Class->GetName(), *GetNameSafe(ParentNativeClass), *StaticEnum<EClassRepNodeMapping>()->GetNameStringByValue((int64)Mapping));
	}

	UE_LOG(LogLyraRepGraph, Log, TEXT(""));
	UE_LOG(LogLyraRepGraph, Log, TEXT("Class Settings Map: "));
	FClassReplicationInfo DefaultValues;
	for (auto ClassRepInfoIt = GlobalActorReplicationInfoMap.CreateClassMapIterator(); ClassRepInfoIt; ++ClassRepInfoIt)
	{
		UClass* Class = CastChecked<UClass>(ClassRepInfoIt.Key().ResolveObjectPtr());
		const FClassReplicationInfo& ClassInfo = ClassRepInfoIt.Value();
		UE_LOG(LogLyraRepGraph, Log, TEXT("  %s (%s) -> %s"), *Class->GetName(), *GetNameSafe(GetParentNativeClass(Class)), *ClassInfo.BuildDebugStringDelta());
	}


	// Rep destruct infos based on CVar value
	DestructInfoMaxDistanceSquared = Lyra::RepGraph::DestructionInfoMaxDist * Lyra::RepGraph::DestructionInfoMaxDist;

#if WITH_GAMEPLAY_DEBUGGER
	AGameplayDebuggerCategoryReplicator::NotifyDebuggerOwnerChange.AddUObject(this, &ThisClass::OnGameplayDebuggerOwnerChange);
#endif

	// Add to RPC_Multicast_OpenChannelForClass map
	RPC_Multicast_OpenChannelForClass.Reset();
	RPC_Multicast_OpenChannelForClass.Set(AActor::StaticClass(), true); // Open channels for multicast RPCs by default
	RPC_Multicast_OpenChannelForClass.Set(AController::StaticClass(), false); // multicasts should never open channels on Controllers since opening a channel on a non-owner breaks the Controller's replication.
	RPC_Multicast_OpenChannelForClass.Set(AServerStatReplicator::StaticClass(), false);

	for (const FRepGraphActorClassSettings& ActorClassSettings : LyraRepGraphSettings->ClassSettings)
	{
		if (ActorClassSettings.bAddToRPC_Multicast_OpenChannelForClassMap)
		{
			if (UClass* StaticActorClass = ActorClassSettings.GetStaticActorClass())
			{
				UE_LOG(LogLyraRepGraph, Log, TEXT("ActorClassSettings -- RPC_Multicast_OpenChannelForClass - %s"), *StaticActorClass->GetName());
				RPC_Multicast_OpenChannelForClass.Set(StaticActorClass, ActorClassSettings.bRPC_Multicast_OpenChannelForClass);
			}
		}
	}
}

void ULyraReplicationGraph::InitGlobalGraphNodes()
{
	// -----------------------------------------------
	//	Spatial Actors
	// -----------------------------------------------

	GridNode = CreateNewNode<UReplicationGraphNode_GridSpatialization2D>();
	GridNode->CellSize = Lyra::RepGraph::CellSize;
	GridNode->SpatialBias = FVector2D(Lyra::RepGraph::SpatialBiasX, Lyra::RepGraph::SpatialBiasY);

	if (Lyra::RepGraph::DisableSpatialRebuilds)
	{
		GridNode->AddToClassRebuildDenyList(AActor::StaticClass()); // Disable All spatial rebuilding
	}
	
	AddGlobalGraphNode(GridNode);

	// -----------------------------------------------
	//	Always Relevant (to everyone) Actors
	// -----------------------------------------------
	AlwaysRelevantNode = CreateNewNode<UReplicationGraphNode_ActorList>();
	AddGlobalGraphNode(AlwaysRelevantNode);

	// -----------------------------------------------
	//	Player State specialization. This will return a rolling subset of the player states to replicate
	// -----------------------------------------------
	ULyraReplicationGraphNode_PlayerStateFrequencyLimiter* PlayerStateNode = CreateNewNode<ULyraReplicationGraphNode_PlayerStateFrequencyLimiter>();
	AddGlobalGraphNode(PlayerStateNode);
}

void ULyraReplicationGraph::InitConnectionGraphNodes(UNetReplicationGraphConnection* RepGraphConnection)
{
	Super::InitConnectionGraphNodes(RepGraphConnection);

	ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = CreateNewNode<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>();

	// This node needs to know when client levels go in and out of visibility
	RepGraphConnection->OnClientVisibleLevelNameAdd.AddUObject(AlwaysRelevantConnectionNode, &ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd);
	RepGraphConnection->OnClientVisibleLevelNameRemove.AddUObject(AlwaysRelevantConnectionNode, &ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove);

	AddConnectionGraphNode(AlwaysRelevantConnectionNode, RepGraphConnection);
}

EClassRepNodeMapping ULyraReplicationGraph::GetMappingPolicy(UClass* Class)
{
	EClassRepNodeMapping* PolicyPtr = ClassRepNodePolicies.Get(Class);
	EClassRepNodeMapping Policy = PolicyPtr ? *PolicyPtr : EClassRepNodeMapping::NotRouted;
	return Policy;
}

void ULyraReplicationGraph::RouteAddNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo, FGlobalActorReplicationInfo& GlobalInfo)
{
	EClassRepNodeMapping Policy = GetMappingPolicy(ActorInfo.Class);
	switch(Policy)
	{
		case EClassRepNodeMapping::NotRouted:
		{
			break;
		}
		
		case EClassRepNodeMapping::RelevantAllConnections:
		{
			if (ActorInfo.StreamingLevelName == NAME_None)
			{
				AlwaysRelevantNode->NotifyAddNetworkActor(ActorInfo);
			}
			else
			{
				FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindOrAdd(ActorInfo.StreamingLevelName);
				RepList.ConditionalAdd(ActorInfo.Actor);
			}
			break;
		}

		case EClassRepNodeMapping::Spatialize_Static:
		{
			GridNode->AddActor_Static(ActorInfo, GlobalInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dynamic:
		{
			GridNode->AddActor_Dynamic(ActorInfo, GlobalInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dormancy:
		{
			GridNode->AddActor_Dormancy(ActorInfo, GlobalInfo);
			break;
		}
	};
}

void ULyraReplicationGraph::RouteRemoveNetworkActorToNodes(const FNewReplicatedActorInfo& ActorInfo)
{
	EClassRepNodeMapping Policy = GetMappingPolicy(ActorInfo.Class);
	switch(Policy)
	{
		case EClassRepNodeMapping::NotRouted:
		{
			break;
		}
		
		case EClassRepNodeMapping::RelevantAllConnections:
		{
			if (ActorInfo.StreamingLevelName == NAME_None)
			{
				AlwaysRelevantNode->NotifyRemoveNetworkActor(ActorInfo);
			}
			else
			{
				FActorRepListRefView& RepList = AlwaysRelevantStreamingLevelActors.FindChecked(ActorInfo.StreamingLevelName);
				if (RepList.RemoveFast(ActorInfo.Actor) == false)
				{
					UE_LOG(LogLyraRepGraph, Warning, TEXT("Actor %s was not found in AlwaysRelevantStreamingLevelActors list. LevelName: %s"), *GetActorRepListTypeDebugString(ActorInfo.Actor), *ActorInfo.StreamingLevelName.ToString());
				}				
			}

			SetActorDestructionInfoToIgnoreDistanceCulling(ActorInfo.GetActor());

			break;
		}

		case EClassRepNodeMapping::Spatialize_Static:
		{
			GridNode->RemoveActor_Static(ActorInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dynamic:
		{
			GridNode->RemoveActor_Dynamic(ActorInfo);
			break;
		}
		
		case EClassRepNodeMapping::Spatialize_Dormancy:
		{
			GridNode->RemoveActor_Dormancy(ActorInfo);
			break;
		}
	};
}

// Since we listen to global (static) events, we need to watch out for cross world broadcasts (PIE)
#if WITH_EDITOR
#define CHECK_WORLDS(X) if(X->GetWorld() != GetWorld()) return;
#else
#define CHECK_WORLDS(X)
#endif

#if WITH_GAMEPLAY_DEBUGGER
void ULyraReplicationGraph::OnGameplayDebuggerOwnerChange(AGameplayDebuggerCategoryReplicator* Debugger, APlayerController* OldOwner)
{
	CHECK_WORLDS(Debugger);

	auto GetAlwaysRelevantForConnectionNode = [this](APlayerController* Controller) -> ULyraReplicationGraphNode_AlwaysRelevant_ForConnection*
	{
		if (Controller)
		{
			if (UNetConnection* NetConnection = Controller->GetNetConnection())
			{
				if (NetConnection->GetDriver() == NetDriver)
				{
					if (UNetReplicationGraphConnection* GraphConnection = FindOrAddConnectionManager(NetConnection))
					{
						for (UReplicationGraphNode* ConnectionNode : GraphConnection->GetConnectionGraphNodes())
						{
							if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = Cast<ULyraReplicationGraphNode_AlwaysRelevant_ForConnection>(ConnectionNode))
							{
								return AlwaysRelevantConnectionNode;
							}
						}
					}
				}
			}
		}

		return nullptr;
	};

	if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = GetAlwaysRelevantForConnectionNode(OldOwner))
	{
		AlwaysRelevantConnectionNode->GameplayDebugger = nullptr;
	}

	if (ULyraReplicationGraphNode_AlwaysRelevant_ForConnection* AlwaysRelevantConnectionNode = GetAlwaysRelevantForConnectionNode(Debugger->GetReplicationOwner()))
	{
		AlwaysRelevantConnectionNode->GameplayDebugger = Debugger;
	}
}
#endif

#undef CHECK_WORLDS

// ------------------------------------------------------------------------------

void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::ResetGameWorldState()
{
	ReplicationActorList.Reset();
	AlwaysRelevantStreamingLevelsNeedingReplication.Empty();
}

void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	ULyraReplicationGraph* LyraGraph = CastChecked<ULyraReplicationGraph>(GetOuter());

	ReplicationActorList.Reset();

	for (const FNetViewer& CurViewer : Params.Viewers)
	{
		ReplicationActorList.ConditionalAdd(CurViewer.InViewer);
		ReplicationActorList.ConditionalAdd(CurViewer.ViewTarget);

		if (ALyraPlayerController* PC = Cast<ALyraPlayerController>(CurViewer.InViewer))
		{
			// 50% throttling of PlayerStates.
			const bool bReplicatePS = (Params.ConnectionManager.ConnectionOrderNum % 2) == (Params.ReplicationFrameNum % 2);
			if (bReplicatePS)
			{
				// Always return the player state to the owning player. Simulated proxy player states are handled by ULyraReplicationGraphNode_PlayerStateFrequencyLimiter
				if (APlayerState* PS = PC->PlayerState)
				{
					if (!bInitializedPlayerState)
					{
						bInitializedPlayerState = true;
						FConnectionReplicationActorInfo& ConnectionActorInfo = Params.ConnectionManager.ActorInfoMap.FindOrAdd(PS);
						ConnectionActorInfo.ReplicationPeriodFrame = 1;
					}

					ReplicationActorList.ConditionalAdd(PS);
				}
			}

			FCachedAlwaysRelevantActorInfo& LastData = PastRelevantActorMap.FindOrAdd(CurViewer.Connection);

			if (ALyraCharacter* Pawn = Cast<ALyraCharacter>(PC->GetPawn()))
			{
				UpdateCachedRelevantActor(Params, Pawn, LastData.LastViewer);

				if (Pawn != CurViewer.ViewTarget)
				{
					ReplicationActorList.ConditionalAdd(Pawn);
				}
			}

			if (ALyraCharacter* ViewTargetPawn = Cast<ALyraCharacter>(CurViewer.ViewTarget))
			{
				UpdateCachedRelevantActor(Params, ViewTargetPawn, LastData.LastViewTarget);
			}
		}
	}

	CleanupCachedRelevantActors(PastRelevantActorMap);

	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorList);

	// Always relevant streaming level actors.
	FPerConnectionActorInfoMap& ConnectionActorInfoMap = Params.ConnectionManager.ActorInfoMap;
	
	TMap<FName, FActorRepListRefView>& AlwaysRelevantStreamingLevelActors = LyraGraph->AlwaysRelevantStreamingLevelActors;

	for (int32 Idx=AlwaysRelevantStreamingLevelsNeedingReplication.Num()-1; Idx >= 0; --Idx)
	{
		const FName& StreamingLevel = AlwaysRelevantStreamingLevelsNeedingReplication[Idx];

		FActorRepListRefView* Ptr = AlwaysRelevantStreamingLevelActors.Find(StreamingLevel);
		if (Ptr == nullptr)
		{
			// No always relevant lists for that level
			UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING Removing %s from AlwaysRelevantStreamingLevelActors because FActorRepListRefView is null. %s "), *StreamingLevel.ToString(),  *Params.ConnectionManager.GetName());
			AlwaysRelevantStreamingLevelsNeedingReplication.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			continue;
		}

		FActorRepListRefView& RepList = *Ptr;

		if (RepList.Num() > 0)
		{
			bool bAllDormant = true;
			for (FActorRepListType Actor : RepList)
			{
				FConnectionReplicationActorInfo& ConnectionActorInfo = ConnectionActorInfoMap.FindOrAdd(Actor);
				if (ConnectionActorInfo.bDormantOnConnection == false)
				{
					bAllDormant = false;
					break;
				}
			}

			if (bAllDormant)
			{
				UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING All AlwaysRelevant Actors Dormant on StreamingLevel %s for %s. Removing list."), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
				AlwaysRelevantStreamingLevelsNeedingReplication.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			}
			else
			{
				UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING Adding always Actors on StreamingLevel %s for %s because it has at least one non dormant actor"), *StreamingLevel.ToString(), *Params.ConnectionManager.GetName());
				Params.OutGatheredReplicationLists.AddReplicationActorList(RepList);
			}
		}
		else
		{
			UE_LOG(LogLyraRepGraph, Warning, TEXT("ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::GatherActorListsForConnection - empty RepList %s"), *Params.ConnectionManager.GetName());
		}

	}

#if WITH_GAMEPLAY_DEBUGGER
	if (GameplayDebugger)
	{
		ReplicationActorList.ConditionalAdd(GameplayDebugger);
	}
#endif
}

void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityAdd(FName LevelName, UWorld* StreamingWorld)
{
	UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING ::OnClientLevelVisibilityAdd - %s"), *LevelName.ToString());
	AlwaysRelevantStreamingLevelsNeedingReplication.Add(LevelName);
}

void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::OnClientLevelVisibilityRemove(FName LevelName)
{
	UE_CLOG(Lyra::RepGraph::DisplayClientLevelStreaming > 0, LogLyraRepGraph, Display, TEXT("CLIENTSTREAMING ::OnClientLevelVisibilityRemove - %s"), *LevelName.ToString());
	AlwaysRelevantStreamingLevelsNeedingReplication.Remove(LevelName);
}

void ULyraReplicationGraphNode_AlwaysRelevant_ForConnection::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();
	LogActorRepList(DebugInfo, NodeName, ReplicationActorList);

	for (const FName& LevelName : AlwaysRelevantStreamingLevelsNeedingReplication)
	{
		ULyraReplicationGraph* LyraGraph = CastChecked<ULyraReplicationGraph>(GetOuter());
		if (FActorRepListRefView* RepList = LyraGraph->AlwaysRelevantStreamingLevelActors.Find(LevelName))
		{
			LogActorRepList(DebugInfo, FString::Printf(TEXT("AlwaysRelevant StreamingLevel List: %s"), *LevelName.ToString()), *RepList);
		}
	}

	DebugInfo.PopIndent();
}

// ------------------------------------------------------------------------------

ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::ULyraReplicationGraphNode_PlayerStateFrequencyLimiter()
{
	bRequiresPrepareForReplicationCall = true;
}

void ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::PrepareForReplication()
{
	ReplicationActorLists.Reset();
	ForceNetUpdateReplicationActorList.Reset();

	ReplicationActorLists.AddDefaulted();
	FActorRepListRefView* CurrentList = &ReplicationActorLists[0];

	// We rebuild our lists of player states each frame. This is not as efficient as it could be but its the simplest way
	// to handle players disconnecting and keeping the lists compact. If the lists were persistent we would need to defrag them as players left.

	for (TActorIterator<APlayerState> It(GetWorld()); It; ++It)
	{
		APlayerState* PS = *It;
		if (IsActorValidForReplicationGather(PS) == false)
		{
			continue;
		}

		if (CurrentList->Num() >= TargetActorsPerFrame)
		{
			ReplicationActorLists.AddDefaulted();
			CurrentList = &ReplicationActorLists.Last(); 
		}
		
		CurrentList->Add(PS);
	}	
}

void ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::GatherActorListsForConnection(const FConnectionGatherActorListParameters& Params)
{
	const int32 ListIdx = Params.ReplicationFrameNum % ReplicationActorLists.Num();
	Params.OutGatheredReplicationLists.AddReplicationActorList(ReplicationActorLists[ListIdx]);

	if (ForceNetUpdateReplicationActorList.Num() > 0)
	{
		Params.OutGatheredReplicationLists.AddReplicationActorList(ForceNetUpdateReplicationActorList);
	}	
}

void ULyraReplicationGraphNode_PlayerStateFrequencyLimiter::LogNode(FReplicationGraphDebugInfo& DebugInfo, const FString& NodeName) const
{
	DebugInfo.Log(NodeName);
	DebugInfo.PushIndent();	

	int32 i=0;
	for (const FActorRepListRefView& List : ReplicationActorLists)
	{
		LogActorRepList(DebugInfo, FString::Printf(TEXT("Bucket[%d]"), i++), List);
	}

	DebugInfo.PopIndent();
}

// ------------------------------------------------------------------------------

void ULyraReplicationGraph::PrintRepNodePolicies()
{
	UEnum* Enum = StaticEnum<EClassRepNodeMapping>();
	if (!Enum)
	{
		return;
	}

	GLog->Logf(TEXT("===================================="));
	GLog->Logf(TEXT("Lyra Replication Routing Policies"));
	GLog->Logf(TEXT("===================================="));

	for (auto It = ClassRepNodePolicies.CreateIterator(); It; ++It)
	{
		FObjectKey ObjKey = It.Key();
		
		EClassRepNodeMapping Mapping = It.Value();

		GLog->Logf(TEXT("%-40s --> %s"), *GetNameSafe(ObjKey.ResolveObjectPtr()), *Enum->GetNameStringByValue(static_cast<uint32>(Mapping)));
	}
}

FAutoConsoleCommandWithWorldAndArgs LyraPrintRepNodePoliciesCmd(TEXT("Lyra.RepGraph.PrintRouting"),TEXT("Prints how actor classes are routed to RepGraph nodes"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		for (TObjectIterator<ULyraReplicationGraph> It; It; ++It)
		{
			It->PrintRepNodePolicies();
		}
	})
);

// ------------------------------------------------------------------------------

FAutoConsoleCommandWithWorldAndArgs ChangeFrequencyBucketsCmd(TEXT("Lyra.RepGraph.FrequencyBuckets"), TEXT("Resets frequency bucket count."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray< FString >& Args, UWorld* World) 
{
	int32 Buckets = 1;
	if (Args.Num() > 0)
	{
		LexTryParseString<int32>(Buckets, *Args[0]);
	}

	UE_LOG(LogLyraRepGraph, Display, TEXT("Setting Frequency Buckets to %d"), Buckets);
	for (TObjectIterator<UReplicationGraphNode_ActorListFrequencyBuckets> It; It; ++It)
	{
		UReplicationGraphNode_ActorListFrequencyBuckets* Node = *It;
		Node->SetNonStreamingCollectionSize(Buckets);
	}
}));
