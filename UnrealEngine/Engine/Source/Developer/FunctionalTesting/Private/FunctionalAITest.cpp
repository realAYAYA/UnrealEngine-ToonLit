// Copyright Epic Games, Inc. All Rights Reserved.

#include "FunctionalAITest.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "FunctionalTestingModule.h"
#include "FunctionalTestingManager.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavAreaBase.h"
#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationOctree.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(FunctionalAITest)

AFunctionalAITestBase::AFunctionalAITestBase( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
	, CurrentSpawnSetIndex(INDEX_NONE)
	, bSingleSetRun(false)
{
	SpawnLocationRandomizationRange = 0.f;
	bWaitForNavMesh = true;
	bDebugNavMeshOnTimeout = false;
}

bool AFunctionalAITestBase::IsOneOfSpawnedPawns(AActor* Actor)
{
	APawn* Pawn = Cast<APawn>(Actor);
	return Pawn != NULL && SpawnedPawns.Contains(Pawn);
}

void AFunctionalAITestBase::BeginPlay()
{
	// do a post-load step and remove all disabled spawn sets
	RemoveSpawnSetIfPredicate([&](FAITestSpawnSetBase& SpawnSet) {
		if (SpawnSet.bEnabled == false)
		{
			UE_LOG(LogFunctionalTest, Log, TEXT("Removing disabled spawn set \'%s\'."), *SpawnSet.Name.ToString());
			return true;
		}
		return false;
	});

	// update all spawn info that doesn't have spawn location set, and set spawn set name
	ForEachSpawnSet([&](FAITestSpawnSetBase& SpawnSet) {
		SpawnSet.ForEachSpawnInfo([&](FAITestSpawnInfoBase& SpawnInfo) {
			SpawnInfo.SpawnSetName = SpawnSet.Name;
			if (SpawnInfo.SpawnLocation == NULL)
			{
				SpawnInfo.SpawnLocation = SpawnSet.FallbackSpawnLocation ? SpawnSet.FallbackSpawnLocation : this;
			}
		});
	});

	Super::BeginPlay();
}

bool AFunctionalAITestBase::RunTest(const TArray<FString>& Params)
{
	KillOffSpawnedPawns();
	ClearPendingDelayedSpawns();

	RandomNumbersStream.Reset();

	bSingleSetRun = Params.Num() > 0;
	if (bSingleSetRun)
	{
		TTypeFromString<int32>::FromString(CurrentSpawnSetIndex, *Params[0]);
	}
	else
	{
		++CurrentSpawnSetIndex;
	}

	if (!IsValidSpawnSetIndex(CurrentSpawnSetIndex))
	{
		return false;
	}
	
	return Super::RunTest(Params);
}

void AFunctionalAITestBase::StartTest()
{
	Super::StartTest();
	StartSpawning();
}

bool AFunctionalAITestBase::IsReady_Implementation()
{
	return Super::IsReady_Implementation() && (bWaitForNavMesh == false || IsNavMeshReady());
}

void AFunctionalAITestBase::OnTimeout()
{
	// tracking for FORT-42587, FORT-42994
	// - log pending navmesh rebuilds / dirty areas
	// - check if area modifiers from navoctree were applied

	UNavigationSystemV1* NavSys = bDebugNavMeshOnTimeout ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
	if (NavSys)
	{
		const ARecastNavMesh* Navmesh = NavSys ? Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance()) : nullptr;

		UE_LOG(LogFunctionalTest, Log, TEXT("Test timed out, log details for: %s"), *GetNameSafe(Navmesh));
		UE_LOG(LogFunctionalTest, Log, TEXT("> dirty areas? %s"), NavSys->HasDirtyAreasQueued() ? TEXT("YES") : TEXT("no"));

		const FNavigationOctree* NavigationOctree = NavSys->GetNavOctree();
		
		FNavigationOctreeFilter AreaFilter;
		AreaFilter.bIncludeAreas = true;
		AreaFilter.bIncludeGeometry = false;
		AreaFilter.bIncludeMetaAreas = true;
		AreaFilter.bIncludeOffmeshLinks = false;

		const FVector TransformedOrigin = GetTransform().TransformPosition(NavMeshDebugOrigin);
		const FBox DebugBounds = FBox::BuildAABB(TransformedOrigin, NavMeshDebugExtent);

		NavigationOctree->FindElementsWithBoundsTest(DebugBounds, [&AreaFilter, &Navmesh](const FNavigationOctreeElement& Element)
		{
			if (Element.IsMatchingFilter(AreaFilter))
			{
				const FCompositeNavModifier NavModifier = Element.GetModifierForAgent(&Navmesh->GetConfig());
				const TArray<FAreaNavModifier> AreaMods = NavModifier.GetAreas();

				FString DebugAreaNames;
				for (int32 Idx = 0; Idx < AreaMods.Num(); Idx++)
				{
					DebugAreaNames += GetNameSafe(AreaMods[Idx].GetAreaClass().Get());
					DebugAreaNames += TEXT(',');
				}

				UE_LOG(LogFunctionalTest, Log, TEXT("> modifier, owner:%s areas:%s"), *GetNameSafe(Element.GetOwner()), *DebugAreaNames);
			}
		});
	}

	Super::OnTimeout();
}

void AFunctionalAITestBase::StartSpawning()
{
	if (bWaitForNavMesh && !IsNavMeshReady())
	{
		GetWorldTimerManager().SetTimer(NavmeshDelayTimer, this, &AFunctionalAITestBase::StartSpawning, 0.5f, false);
		return;
	}

	FAITestSpawnSetBase* SpawnSet = GetSpawnSet(CurrentSpawnSetIndex);
	if (!SpawnSet)
	{
		FinishTest(EFunctionalTestResult::Failed, FString::Printf(TEXT("Unable to use spawn set: %d"), CurrentSpawnSetIndex));
		return;
	}

	UWorld* World = GetWorld();
	check(World);
	bool bSuccessfullySpawnedAll = true;

	// NOTE: even if some pawns fail to spawn we don't stop spawning to find all spawns that will fails.
	// all spawned pawns get filled off in case of failure.
	CurrentSpawnSetName = SpawnSet->Name.ToString();

	int32 SpawnInfoIndex = 0;
	SpawnSet->ForEachSpawnInfo([&](FAITestSpawnInfoBase& SpawnInfo) {
		if (SpawnInfo.IsValid())
		{
			if (SpawnInfo.PreSpawnDelay > 0)
			{
				PendingDelayedSpawns.Add(FPendingDelayedSpawn(CurrentSpawnSetIndex, SpawnInfoIndex, SpawnInfo.NumberToSpawn, SpawnInfo.PreSpawnDelay));
			}
			else if (SpawnInfo.SpawnDelay == 0.0)
			{
				for (int32 SpawnedCount = 0; SpawnedCount < SpawnInfo.NumberToSpawn; ++SpawnedCount)
				{
					bSuccessfullySpawnedAll &= SpawnInfo.Spawn(this);
				}
			}
			else
			{
				bSuccessfullySpawnedAll &= SpawnInfo.Spawn(this);
				if (SpawnInfo.NumberToSpawn > 1)
				{
					PendingDelayedSpawns.Add(FPendingDelayedSpawn(CurrentSpawnSetIndex, SpawnInfoIndex, SpawnInfo.NumberToSpawn - 1, SpawnInfo.SpawnDelay));
				}
			}
		}
		else
		{
			const FString SpawnFailureMessage = FString::Printf(TEXT("Spawn set \'%s\' contains invalid entry at index %d")
				, *SpawnSet->Name.ToString()
				, SpawnInfoIndex);

			UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *SpawnFailureMessage);

			bSuccessfullySpawnedAll = false;
		}
		++SpawnInfoIndex;
	});

	if (bSuccessfullySpawnedAll == false)
	{
		KillOffSpawnedPawns();
		
		// wait a bit if it's in the middle of StartTest call
		FTimerHandle DummyHandle;
		World->GetTimerManager().SetTimer(DummyHandle, this, &AFunctionalAITestBase::OnSpawningFailure, 0.1f, false);
	}		
	else
	{
		if (PendingDelayedSpawns.Num() > 0)
		{
			SetActorTickEnabled(true);
		}
	}
}

void AFunctionalAITestBase::OnSpawningFailure()
{
	FinishTest(EFunctionalTestResult::Failed, TEXT("Unable to spawn AI"));
}

bool AFunctionalAITestBase::WantsToRunAgain() const
{
	return bSingleSetRun == false && IsValidSpawnSetIndex(CurrentSpawnSetIndex + 1);
}

void AFunctionalAITestBase::GatherRelevantActors(TArray<AActor*>& OutActors) const
{
	Super::GatherRelevantActors(OutActors);

	ForEachSpawnSet([&OutActors](const FAITestSpawnSetBase& SpawnSet) {
		if (SpawnSet.FallbackSpawnLocation)
		{
			OutActors.AddUnique(SpawnSet.FallbackSpawnLocation);
		}

		SpawnSet.ForEachSpawnInfo([&OutActors](const FAITestSpawnInfoBase& SpawnInfo) {
			if (SpawnInfo.SpawnLocation)
			{
				OutActors.AddUnique(SpawnInfo.SpawnLocation);
			}
		});
	});

	for (auto Pawn : SpawnedPawns)
	{
		if (Pawn)
		{
			OutActors.Add(Pawn);
		}
	}
}

void AFunctionalAITestBase::CleanUp()
{
	Super::CleanUp();
	CurrentSpawnSetIndex = INDEX_NONE;

	KillOffSpawnedPawns();
	ClearPendingDelayedSpawns();
}

FString AFunctionalAITestBase::GetAdditionalTestFinishedMessage(EFunctionalTestResult TestResult) const
{
	FString ResultStr;

	if (SpawnedPawns.Num() > 0)
	{
		if (CurrentSpawnSetName.Len() > 0 && CurrentSpawnSetName != TEXT("None"))
		{
			ResultStr = FString::Printf(TEXT("spawn set \'%s\', pawns: "), *CurrentSpawnSetName);
		}
		else
		{
			ResultStr = TEXT("pawns: ");
		}
		

		for (int32 PawnIndex = 0; PawnIndex < SpawnedPawns.Num(); ++PawnIndex)
		{
			ResultStr += FString::Printf(TEXT("%s, "), *GetNameSafe(SpawnedPawns[PawnIndex]));
		}
	}

	return ResultStr;
}

FString AFunctionalAITestBase::GetReproString() const
{
	return FString::Printf(TEXT("%s%s%d"), *(GetFName().ToString())
		, FFunctionalTesting::ReproStringParamsSeparator
		, CurrentSpawnSetIndex);
}

void AFunctionalAITestBase::KillOffSpawnedPawns()
{
	for (int32 PawnIndex = 0; PawnIndex < SpawnedPawns.Num(); ++PawnIndex)
	{
		if (SpawnedPawns[PawnIndex])
		{
			SpawnedPawns[PawnIndex]->Destroy();
		}
	}

	SpawnedPawns.Reset();
}

void AFunctionalAITestBase::ClearPendingDelayedSpawns()
{
	SetActorTickEnabled(false);
	PendingDelayedSpawns.Reset();
}

void AFunctionalAITestBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (auto& DelayedSpawn : PendingDelayedSpawns)
	{
		DelayedSpawn.Tick(DeltaSeconds, this);
	}
}

void AFunctionalAITestBase::AddSpawnedPawn(APawn& SpawnedPawn)
{
	SpawnedPawns.Add(&SpawnedPawn);
	OnAISpawned.Broadcast(Cast<AAIController>(SpawnedPawn.GetController()), &SpawnedPawn);
}

FVector AFunctionalAITestBase::GetRandomizedLocation(const FVector& Location) const
{
	return Location + FVector(RandomNumbersStream.FRandRange(-SpawnLocationRandomizationRange, SpawnLocationRandomizationRange), RandomNumbersStream.FRandRange(-SpawnLocationRandomizationRange, SpawnLocationRandomizationRange), 0);
}

bool AFunctionalAITestBase::IsNavMeshReady() const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (NavSys && NavSys->NavDataSet.Num() > 0 && !NavSys->IsNavigationBuildInProgress())
	{
		return true;
	}

	return false;
}

const FAITestSpawnInfoBase* AFunctionalAITestBase::GetSpawnInfo(const int32 SpawnSetIndex, const int32 SpawnInfoIndex) const
{
	const FAITestSpawnSetBase* SpawnSet = GetSpawnSet(SpawnSetIndex);
	return SpawnSet ? SpawnSet->GetSpawnInfo(SpawnInfoIndex) : nullptr;
}

FAITestSpawnInfoBase* AFunctionalAITestBase::GetSpawnInfo(const int32 SpawnSetIndex, const int32 SpawnInfoIndex)
{
	FAITestSpawnSetBase* SpawnSet = GetSpawnSet(SpawnSetIndex);
	return SpawnSet ? SpawnSet->GetSpawnInfo(SpawnInfoIndex) : nullptr;
}

bool AFunctionalAITestBase::Spawn(const int32 SpawnSetIndex, const int32 SpawnInfoIndex)
{
	const FAITestSpawnInfoBase* SpawnInfo = GetSpawnInfo(SpawnSetIndex, SpawnInfoIndex);
	return SpawnInfo ? SpawnInfo->Spawn(this) : false;
}
//----------------------------------------------------------------------//
// FAITestSpawnInfo
//----------------------------------------------------------------------//
bool FAITestSpawnInfo::Spawn(AFunctionalAITestBase* AITest) const
{
	check(AITest);

	bool bSuccessfullySpawned = false;

	APawn* SpawnedPawn = UAIBlueprintHelperLibrary::SpawnAIFromClass(AITest->GetWorld(), PawnClass, BehaviorTree
		, AITest->GetRandomizedLocation(SpawnLocation->GetActorLocation())
		, SpawnLocation->GetActorRotation()
		, /*bNoCollisionFail=*/true);

	if (SpawnedPawn == NULL)
	{
		FString FailureMessage = FString::Printf(TEXT("Failed to spawn \'%s\' pawn (\'%s\' set) ")
			, *GetNameSafe(PawnClass)
			, *SpawnSetName.ToString());

		UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *FailureMessage);
	}
	else if (SpawnedPawn->GetController() == NULL)
	{
		FString FailureMessage = FString::Printf(TEXT("Spawned Pawn %s (\'%s\' set) has no controller ")
			, *GetNameSafe(SpawnedPawn)
			, *SpawnSetName.ToString());

		UE_LOG(LogFunctionalTest, Warning, TEXT("%s"), *FailureMessage);
	}
	else
	{
		IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(SpawnedPawn);
		if (TeamAgent == nullptr)
		{
			TeamAgent = Cast<IGenericTeamAgentInterface>(SpawnedPawn->GetController());
		}

		if (TeamAgent != nullptr)
		{
			TeamAgent->SetGenericTeamId(TeamID);
		}

		AITest->AddSpawnedPawn(*SpawnedPawn);
		bSuccessfullySpawned = true;
	}

	return bSuccessfullySpawned;
}

//----------------------------------------------------------------------//
// 
//----------------------------------------------------------------------//
void FPendingDelayedSpawn::Tick(float TimeDelta, AFunctionalAITestBase* AITest)
{
	if (bFinished || !AITest)
	{
		return;
	}

	TimeToNextSpawn -= TimeDelta;

	if (TimeToNextSpawn <= 0)
	{	
		AITest->Spawn(SpawnSetIndex, SpawnInfoIndex);

		if (--NumberToSpawnLeft <= 0)
		{
			bFinished = true;
		}
		else if (const FAITestSpawnInfoBase* SpawnInfo = AITest->GetSpawnInfo(SpawnSetIndex, SpawnInfoIndex))
		{
			TimeToNextSpawn = SpawnInfo->SpawnDelay;
		}
	}
}

const FAITestSpawnInfoBase* FAITestSpawnSet::GetSpawnInfo(const int32 SpawnInfoIndex) const
{
	if (SpawnInfoContainer.IsValidIndex(SpawnInfoIndex))
	{
		return &SpawnInfoContainer[SpawnInfoIndex];
	}
	return nullptr;
}

FAITestSpawnInfoBase* FAITestSpawnSet::GetSpawnInfo(const int32 SpawnInfoIndex)
{
	if (SpawnInfoContainer.IsValidIndex(SpawnInfoIndex))
	{
		return &SpawnInfoContainer[SpawnInfoIndex];
	}
	return nullptr;
}

bool FAITestSpawnSet::IsValidSpawnInfoIndex(const int32 Index) const
{
	return SpawnInfoContainer.IsValidIndex(Index);
}

void FAITestSpawnSet::ForEachSpawnInfo(TFunctionRef<void(FAITestSpawnInfoBase&)> Predicate)
{
	for (FAITestSpawnInfo& SpawnInfo : SpawnInfoContainer)
	{
		Predicate(SpawnInfo);
	}
}

void FAITestSpawnSet::ForEachSpawnInfo(TFunctionRef<void(const FAITestSpawnInfoBase&)> Predicate) const
{
	for (const FAITestSpawnInfo& SpawnInfo : SpawnInfoContainer)
	{
		Predicate(SpawnInfo);
	}
}

void AFunctionalAITest::ForEachSpawnSet(TFunctionRef<void(const FAITestSpawnSetBase&)> Predicate) const
{
	for (int32 Index = 0; Index < SpawnSets.Num(); ++Index)
	{
		Predicate(SpawnSets[Index]);
	}
}

void AFunctionalAITest::ForEachSpawnSet(TFunctionRef<void(FAITestSpawnSetBase&)> Predicate)
{
	for (int32 Index = 0; Index < SpawnSets.Num(); ++Index)
	{
		Predicate(SpawnSets[Index]);
	}
}

void AFunctionalAITest::RemoveSpawnSetIfPredicate(TFunctionRef<bool(FAITestSpawnSetBase&)> Predicate)
{
	bool bRemovedEntry = false;
	for (int32 Index = SpawnSets.Num() - 1; Index >= 0; --Index)
	{
		if (Predicate(SpawnSets[Index]))
		{
			SpawnSets.RemoveAt(Index, 1, EAllowShrinking::No);
			bRemovedEntry = true;
		}
	}

	if (bRemovedEntry)
	{
		SpawnSets.Shrink();
	}
}

const FAITestSpawnSetBase* AFunctionalAITest::GetSpawnSet(const int32 SpawnSetIndex) const
{
	if (SpawnSets.IsValidIndex(SpawnSetIndex))
	{
		return &SpawnSets[SpawnSetIndex];
	}
	return nullptr;
}

FAITestSpawnSetBase* AFunctionalAITest::GetSpawnSet(const int32 SpawnSetIndex)
{
	if (SpawnSets.IsValidIndex(SpawnSetIndex))
	{
		return &SpawnSets[SpawnSetIndex];
	}
	return nullptr;
}

bool AFunctionalAITest::IsValidSpawnSetIndex(const int32 Index) const
{
	return SpawnSets.IsValidIndex(Index);
}

