// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDScene.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDParticleActor.h"
#include "Chaos/ImplicitObject.h"
#include "ChaosVDRecording.h"
#include "EditorActorFolders.h"
#include "WorldPersistentFolders.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "EditorActorFolders.h"
#include "WorldPersistentFolders.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Engine/Engine.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"

#include "UObject/Package.h"

FChaosVDScene::FChaosVDScene() = default;

FChaosVDScene::~FChaosVDScene() = default;

void FChaosVDScene::Initialize()
{
	if (!ensure(!bIsInitialized))
	{
		return;
	}

	SelectionSet = NewObject<UTypedElementSelectionSet>();

	SelectionSet->OnPreChange().AddRaw(this, &FChaosVDScene::HandlePreSelectionChange);
	SelectionSet->OnChanged().AddRaw(this, &FChaosVDScene::HandlePostSelectionChange);
	
	PhysicsVDWorld = CreatePhysicsVDWorld();

	GeometryGenerator = MakeShared<FChaosVDGeometryBuilder>();

	bIsInitialized = true;
}

void FChaosVDScene::DeInitialize()
{
	if (!ensure(bIsInitialized))
	{
		return;
	}

	SelectionSet->OnPreChange().RemoveAll(this);
	SelectionSet->OnChanged().RemoveAll(this);

	GeometryGenerator.Reset();

	CleanUpScene();

	if (PhysicsVDWorld)
	{
		PhysicsVDWorld->DestroyWorld(true);
		GEngine->DestroyWorldContext(PhysicsVDWorld);

		PhysicsVDWorld->MarkAsGarbage();
		PhysicsVDWorld = nullptr;
	}
	
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	bIsInitialized = false;
}

void FChaosVDScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PhysicsVDWorld);
	Collector.AddReferencedObject(SelectionSet);
}

void FChaosVDScene::UpdateFromRecordedStepData(const int32 SolverID, const FString& SolverName, const FChaosVDStepData& InRecordedStepData, const FChaosVDSolverFrameData& InFrameData)
{
	FChaosVDParticlesByIDMap& SolverParticlesByID = ParticlesBySolverID.FindChecked(SolverID);
	
	TSet<int32> ParticlesIDsInRecordedStepData;
	ParticlesIDsInRecordedStepData.Reserve(InRecordedStepData.RecordedParticlesData.Num());

	// Go over existing Particle VD Instances and update them or create them if needed 
	for (const FChaosVDParticleDataWrapper& Particle : InRecordedStepData.RecordedParticlesData)
	{
		const int32 ParticleVDInstanceID = GetIDForRecordedParticleData(Particle);
		ParticlesIDsInRecordedStepData.Add(ParticleVDInstanceID);

		if (InRecordedStepData.ParticlesDestroyedIDs.Contains(ParticleVDInstanceID))
		{
			// Do not process the particle if it was destroyed in the same step
			continue;
		}

		if (AChaosVDParticleActor** ExistingParticleVDInstancePtrPtr = SolverParticlesByID.Find(ParticleVDInstanceID))
		{
			if (AChaosVDParticleActor* ExistingParticleVDInstancePtr = *ExistingParticleVDInstancePtrPtr)
			{
				ExistingParticleVDInstancePtr->UpdateFromRecordedParticleData(Particle, InFrameData.SimulationTransform);
			}
			else
			{
				//TODO: Handle this error
				ensure(false);
			}
		}
		else
		{
			if (AChaosVDParticleActor* NewParticleVDInstance = SpawnParticleFromRecordedData(Particle, InFrameData))
			{
				FStringFormatOrderedArguments Args {SolverName, FString::FromInt(SolverID)};
				const FName FolderPath = *FPaths::Combine(FString::Format(TEXT("Solver {0} | ID {1}"), Args), UEnum::GetDisplayValueAsText(NewParticleVDInstance->GetParticleData()->Type).ToString());

				NewParticleVDInstance->SetFolderPath(FolderPath);

				// TODO: Precalculate the max num of entries we would see in the loaded file, and use that number to pre-allocate this map
				SolverParticlesByID.Add(ParticleVDInstanceID, NewParticleVDInstance);
			}
			else
			{
				//TODO: Handle this error
				ensure(false);
			}
		}
	}

	UpdateParticlesCollisionData(InRecordedStepData, SolverID);

	int32 AmountRemoved = 0;
	for (FChaosVDParticlesByIDMap::TIterator RemoveIterator = SolverParticlesByID.CreateIterator(); RemoveIterator; ++RemoveIterator)
	{
		// If we are playing back a keyframe, the scene should only contain what it is in the recorded data
		const bool bShouldDestroyParticleAnyway = InFrameData.bIsKeyFrame && !ParticlesIDsInRecordedStepData.Contains(RemoveIterator.Key());
		
		if (bShouldDestroyParticleAnyway || InFrameData.ParticlesDestroyedIDs.Contains(RemoveIterator.Key()))
		{
			if (AChaosVDParticleActor* ActorToRemove = RemoveIterator.Value())
			{
				if (IsObjectSelected(ActorToRemove))
				{
					ClearSelectionAndNotify();
				}

				PhysicsVDWorld->DestroyActor(ActorToRemove);
			}

			RemoveIterator.RemoveCurrent();

			AmountRemoved++;
		}
	}
	
	OnSceneUpdated().Broadcast();
}


void FChaosVDScene::UpdateParticlesCollisionData(const FChaosVDStepData& InRecordedStepData, int32 SolverID)
{
	FChaosVDParticlesByIDMap& SolverParticlesByID = ParticlesBySolverID.FindChecked(SolverID);
	
	for (const TPair<int32, TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>>& ParticleIDMidPhasePair : InRecordedStepData.RecordedMidPhasesByParticleID)
	{
		if (AChaosVDParticleActor** ExistingParticleVDInstancePtrPtr = SolverParticlesByID.Find(ParticleIDMidPhasePair.Key))
		{
			if (AChaosVDParticleActor* ExistingParticleVDInstancePtr = *ExistingParticleVDInstancePtrPtr)
			{
				ExistingParticleVDInstancePtr->UpdateCollisionData(ParticleIDMidPhasePair.Value);
			}
		}
	}

	for (const TPair<int32, TArray<FChaosVDConstraint>>& ParticleIDMidPhasePair : InRecordedStepData.RecordedConstraintsByParticleID)
	{
		if (AChaosVDParticleActor** ExistingParticleVDInstancePtrPtr = SolverParticlesByID.Find(ParticleIDMidPhasePair.Key))
		{
			if (AChaosVDParticleActor* ExistingParticleVDInstancePtr = *ExistingParticleVDInstancePtrPtr)
			{
				ExistingParticleVDInstancePtr->UpdateCollisionData(ParticleIDMidPhasePair.Value);
			}
		}
	}
}


void FChaosVDScene::HandleNewGeometryData(const TSharedPtr<const Chaos::FImplicitObject>& GeometryData, const uint32 GeometryID) const
{
	NewGeometryAvailableDelegate.Broadcast(GeometryData, GeometryID);
}

void FChaosVDScene::HandleEnterNewGameFrame(int32 FrameNumber, const TArray<int32>& AvailableSolversIds)
{
	// Currently the particle actors from all the solvers are in the same level, and we manage them by keeping track
	// of to which solvers they belong using maps.
	// Using Level instead or a Sub ChaosVDScene could be a better solution
	// I'm intentionally not making that change right now until the "level streaming" solution for the tool is defined
	// As that would impose restriction on how levels could be used. For now the map approach is simpler and will be easier to refactor later on.

	TSet<int32> AvailableSolversSet;
	AvailableSolversSet.Reserve(AvailableSolversIds.Num());

	for (int32 SolverID : AvailableSolversIds)
	{
		AvailableSolversSet.Add(SolverID);
		if (!ParticlesBySolverID.Contains(SolverID))
		{
			ParticlesBySolverID.Add(SolverID);
		}
	}

	int32 AmountRemoved = 0;
	for (TMap<int32, FChaosVDParticlesByIDMap>::TIterator RemoveIterator = ParticlesBySolverID.CreateIterator(); RemoveIterator; ++RemoveIterator)
	{
		if (!AvailableSolversSet.Contains(RemoveIterator.Key()))
		{
			for (const TPair<int32, AChaosVDParticleActor*>& ParticleVDInstanceWithID : RemoveIterator.Value())
			{
				if (IsObjectSelected(ParticleVDInstanceWithID.Value))
				{
					ClearSelectionAndNotify();
				}

				PhysicsVDWorld->DestroyActor(ParticleVDInstanceWithID.Value);
			}

			RemoveIterator.RemoveCurrent();

			AmountRemoved++;
		}
	}

	if (AmountRemoved > 0)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
}

void FChaosVDScene::CleanUpScene()
{
	ClearSelectionAndNotify();

	if (PhysicsVDWorld)
	{
		for (const TPair<int32, FChaosVDParticlesByIDMap>& SolverParticleVDInstanceWithID : ParticlesBySolverID)
		{
			for (const TPair<int32, AChaosVDParticleActor*>& ParticleVDInstanceWithID : SolverParticleVDInstanceWithID.Value)
			{
				PhysicsVDWorld->DestroyActor(ParticleVDInstanceWithID.Value);
			}
		}
	}

	ParticlesBySolverID.Reset();
}

const TSharedPtr<const Chaos::FImplicitObject>* FChaosVDScene::GetUpdatedGeometry(int32 GeometryID) const
{
	if (ensure(LoadedRecording.IsValid()))
	{
		if (const TSharedPtr<const Chaos::FImplicitObject>* Geometry = LoadedRecording->GetGeometryDataMap().Find(GeometryID))
		{
			return Geometry;
		}
	}

	return nullptr;
}

AChaosVDParticleActor* FChaosVDScene::GetParticleActor(int32 SolverID, int32 ParticleID)
{
	AChaosVDParticleActor** ParticleActorPtrPtr = ParticlesBySolverID[SolverID].Find(ParticleID);
	return ParticleActorPtrPtr ? *ParticleActorPtrPtr : nullptr;
}

AChaosVDParticleActor* FChaosVDScene::SpawnParticleFromRecordedData(const FChaosVDParticleDataWrapper& InParticleData, const FChaosVDSolverFrameData& InFrameData)
{
	using namespace Chaos;

	FActorSpawnParameters Params;
	Params.Name = *InParticleData.DebugName;
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;

	if (AChaosVDParticleActor* NewActor = PhysicsVDWorld->SpawnActor<AChaosVDParticleActor>(Params))
	{
		NewActor->UpdateFromRecordedParticleData(InParticleData, InFrameData.SimulationTransform);

		if (!InParticleData.DebugName.IsEmpty())
		{
			NewActor->SetActorLabel(InParticleData.DebugName);
		}

		NewActor->SetScene(AsShared());
		
		if (ensure(LoadedRecording.IsValid()))
		{
			if (const TSharedPtr<const Chaos::FImplicitObject>* Geometry = LoadedRecording->GetGeometryDataMap().Find(InParticleData.GeometryHash))
			{
				NewActor->UpdateGeometry(*Geometry);
			}
		}

		return NewActor;
	}

	return nullptr;
}

int32 FChaosVDScene::GetIDForRecordedParticleData(const FChaosVDParticleDataWrapper& InParticleData) const
{
	return InParticleData.ParticleIndex;
}

UWorld* FChaosVDScene::CreatePhysicsVDWorld() const
{
	const FName UniqueWorldName = FName(FGuid::NewGuid().ToString());
	UWorld* NewWorld = NewWorld = NewObject<UWorld>( GetTransientPackage(), UniqueWorldName );
	
	NewWorld->WorldType = EWorldType::EditorPreview;

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext( NewWorld->WorldType );
	WorldContext.SetCurrentWorld(NewWorld);

	NewWorld->InitializeNewWorld( UWorld::InitializationValues()
										  .AllowAudioPlayback( false )
										  .CreatePhysicsScene( false )
										  .RequiresHitProxies( true )
										  .CreateNavigation( false )
										  .CreateAISystem( false )
										  .ShouldSimulatePhysics( false )
										  .SetTransactional( false )
	);

	// Add the base content as a sublevel
	const UChaosVDEditorSettings* Settings = GetDefault<UChaosVDEditorSettings>();

	ULevelStreamingDynamic* StreamedInLevel = NewObject<ULevelStreamingDynamic>(NewWorld);
	StreamedInLevel->SetWorldAssetByPackageName(FName(Settings->BasePhysicsVDWorld.GetLongPackageName()));

	StreamedInLevel->PackageNameToLoad = FName(Settings->BasePhysicsVDWorld.GetLongPackageName());

	StreamedInLevel->SetShouldBeLoaded(true);
	StreamedInLevel->bShouldBlockOnLoad = true;
	StreamedInLevel->bInitiallyLoaded = true;

	StreamedInLevel->SetShouldBeVisible(true);
	StreamedInLevel->bInitiallyVisible = true;
	StreamedInLevel->bLocked = true;

	NewWorld->AddStreamingLevel(StreamedInLevel);

	NewWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	return NewWorld;
}

FTypedElementHandle FChaosVDScene::GetSelectionHandleForObject(const UObject* Object) const
{
	FTypedElementHandle Handle;
	if (const AActor* Actor = Cast<AActor>(Object))
	{
		Handle = UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor);
	}
	else if (const UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		Handle = UEngineElementsLibrary::AcquireEditorComponentElementHandle(Component);
	}
	else
	{
		Handle = UEngineElementsLibrary::AcquireEditorObjectElementHandle(Object);
	}

	return Handle;
}

void FChaosVDScene::UpdateSelectionProxiesForActors(const TArray<AActor*>& SelectedActors)
{
	for (AActor* SelectedActor : SelectedActors)
	{
		if (SelectedActor)
		{
			SelectedActor->PushSelectionToProxies();
		}
	}
}

void FChaosVDScene::HandlePreSelectionChange(const UTypedElementSelectionSet* PreChangeSelectionSet)
{
	PendingActorsToUpdateSelectionProxy.Append(PreChangeSelectionSet->GetSelectedObjects<AActor>());
}

void FChaosVDScene::HandlePostSelectionChange(const UTypedElementSelectionSet* PreChangeSelectionSet)
{
	TArray<AActor*> SelectedActors = PreChangeSelectionSet->GetSelectedObjects<AActor>();

	SelectedActors.Append(PendingActorsToUpdateSelectionProxy);
	UpdateSelectionProxiesForActors(SelectedActors);

	PendingActorsToUpdateSelectionProxy.Reset();
}

void FChaosVDScene::ClearSelectionAndNotify()
{
	if (!SelectionSet)
	{
		return;
	}

	SelectionSet->ClearSelection(FTypedElementSelectionOptions());
	SelectionSet->NotifyPendingChanges();
}

void FChaosVDScene::SetSelectedObject(UObject* SelectedObject)
{
	if (!SelectionSet)
	{
		return;
	}

	if (!::IsValid(SelectedObject))
	{
		ClearSelectionAndNotify();
		return;
	}

	if (IsObjectSelected(SelectedObject))
	{
		// Already selected, nothing to do here
		return;
	}

	SelectionSet->ClearSelection(FTypedElementSelectionOptions());

	TArray<FTypedElementHandle> NewEditorSelection = { GetSelectionHandleForObject(SelectedObject) };

	SelectionSet->SetSelection(NewEditorSelection, FTypedElementSelectionOptions());
	SelectionSet->NotifyPendingChanges();
}

bool FChaosVDScene::IsObjectSelected(const UObject* Object)
{
	if (!SelectionSet)
	{
		return false;
	}

	if (!::IsValid(Object))
	{
		return false;
	}

	return SelectionSet->IsElementSelected(GetSelectionHandleForObject(Object), FTypedElementIsSelectedOptions());;
}
