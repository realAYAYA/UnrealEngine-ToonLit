// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDScene.h"

#include "Actors/ChaosVDSceneQueryDataContainer.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "ChaosVDEditorSettings.h"
#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "Chaos/ImplicitObject.h"
#include "ChaosVDRecording.h"
#include "ChaosVDSkySphereInterface.h"
#include "Components/ChaosVDSceneQueryDataComponent.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "EditorActorFolders.h"
#include "EditorLevelUtils.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Misc/ScopedSlowTask.h"
#include "Materials/Material.h"
#include "Selection.h"
#include "UObject/Package.h"
#include "WorldPersistentFolders.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "Engine/Level.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDScene::FChaosVDScene() = default;

FChaosVDScene::~FChaosVDScene() = default;

namespace ChaosVDSceneUIOptions
{
	constexpr float DelayToShowProgressDialogThreshold = 1.0f;
	constexpr bool bShowCancelButton = false;
	constexpr bool bAllowInPIE = false;
}

void FChaosVDScene::Initialize()
{
	if (!ensure(!bIsInitialized))
	{
		return;
	}

	InitializeSelectionSets();
	
	PhysicsVDWorld = CreatePhysicsVDWorld();

	GeometryGenerator = MakeShared<FChaosVDGeometryBuilder>();

	GeometryGenerator->Initialize(AsWeak());
	
	StreamableManager = MakeShared<FStreamableManager>();

	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		// TODO: Do an async load instead, and prepare a loading screen or notification popup
		// Jira for tracking UE-191639
		StreamableManager->RequestSyncLoad(Settings->QueryOnlyMeshesMaterial.ToSoftObjectPath());
		StreamableManager->RequestSyncLoad(Settings->SimOnlyMeshesMaterial.ToSoftObjectPath());
		StreamableManager->RequestSyncLoad(Settings->InstancedMeshesMaterial.ToSoftObjectPath());
		StreamableManager->RequestSyncLoad(Settings->InstancedMeshesQueryOnlyMaterial.ToSoftObjectPath());
		
		Settings->OnVisibilitySettingsChanged().AddRaw(this, &FChaosVDScene::HandleVisibilitySettingsChanged);
		Settings->OnColorSettingsChanged().AddRaw(this, &FChaosVDScene::HandleColorSettingsChanged);
	}

	bIsInitialized = true;
}

void FChaosVDScene::DeInitialize()
{
	constexpr float AmountOfWork = 1.0f;
	FScopedSlowTask ClosingSceneSlowTask(AmountOfWork, LOCTEXT("ClosingSceneMessage", "Closing Scene ..."));
	ClosingSceneSlowTask.MakeDialog();

	if (!ensure(bIsInitialized))
	{
		return;
	}

	if (UChaosVDEditorSettings* Settings = GetMutableDefault<UChaosVDEditorSettings>())
	{
		Settings->OnVisibilitySettingsChanged().RemoveAll(this);
		Settings->OnColorSettingsChanged().RemoveAll(this);
	}

	CleanUpScene();

	DeInitializeSelectionSets();

	GeometryGenerator.Reset();

	if (PhysicsVDWorld)
	{
		PhysicsVDWorld->RemoveOnActorDestroyededHandler(ActorDestroyedHandle);

		PhysicsVDWorld->DestroyWorld(true);
		GEngine->DestroyWorldContext(PhysicsVDWorld);

		PhysicsVDWorld->MarkAsGarbage();
		PhysicsVDWorld = nullptr;
	}

	{
		FScopedSlowTask CollectingGarbageSlowTask(1, LOCTEXT("CollectingGarbageDataMessage", "Collecting Garbage ..."));
		CollectingGarbageSlowTask.MakeDialog();

		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

		CollectingGarbageSlowTask.EnterProgressFrame();
	}

	bIsInitialized = false;
}

void FChaosVDScene::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(PhysicsVDWorld);
	Collector.AddReferencedObject(SelectionSet);
	Collector.AddReferencedObject(ObjectSelection);
	Collector.AddReferencedObject(ActorSelection);
	Collector.AddReferencedObject(ComponentSelection);
}

void FChaosVDScene::UpdateFromRecordedStepData(const int32 SolverID, const FChaosVDStepData& InRecordedStepData, const FChaosVDSolverFrameData& InFrameData)
{
	AChaosVDSolverInfoActor* SolverSceneData = nullptr;
	if (AChaosVDSolverInfoActor** SolverSceneDataPtrPtr = SolverDataContainerBySolverID.Find(SolverID))
	{
		SolverSceneData = *SolverSceneDataPtrPtr;
	}
	else
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Attempted to playback a solver frame from an invalid solver container"), ANSI_TO_TCHAR(__FUNCTION__));	
	}

	if (!SolverSceneData)
	{
		return;
	}

	SolverSceneData->SetSimulationTransform(InFrameData.SimulationTransform);
	
	TSet<int32> ParticlesIDsInRecordedStepData;
	ParticlesIDsInRecordedStepData.Reserve(InRecordedStepData.RecordedParticlesData.Num());
	
	{
		constexpr float AmountOfWork = 1.0f;
		const float PercentagePerElement = 1.0f / InRecordedStepData.RecordedParticlesData.Num();

		const FText ProgressBarTitle = FText::Format(FTextFormat(LOCTEXT("ProcessingParticleData", "Processing Particle Data for {0} Solver with ID {1} ...")), FText::FromString(SolverSceneData->GetSolverName()), FText::AsNumber(SolverID));
		FScopedSlowTask UpdatingSceneSlowTask(AmountOfWork, ProgressBarTitle);
		UpdatingSceneSlowTask.MakeDialogDelayed(ChaosVDSceneUIOptions::DelayToShowProgressDialogThreshold, ChaosVDSceneUIOptions::bShowCancelButton, ChaosVDSceneUIOptions::bAllowInPIE);
	
		// Go over existing Particle VD Instances and update them or create them if needed 
		for (const TSharedPtr<FChaosVDParticleDataWrapper>& Particle : InRecordedStepData.RecordedParticlesData)
		{
			const int32 ParticleVDInstanceID = GetIDForRecordedParticleData(Particle);
			ParticlesIDsInRecordedStepData.Add(ParticleVDInstanceID);

			if (InRecordedStepData.ParticlesDestroyedIDs.Contains(ParticleVDInstanceID))
			{
				// Do not process the particle if it was destroyed in the same step
				continue;
			}

			if (AChaosVDParticleActor* ExistingParticleVDInstancePtr = SolverSceneData->GetParticleActor(ParticleVDInstanceID))
			{
				// We have new data for this particle, so re-activate the existing actor
				if (!ExistingParticleVDInstancePtr->IsActive())
				{
					ExistingParticleVDInstancePtr->SetIsActive(true);
				}

				ExistingParticleVDInstancePtr->UpdateFromRecordedParticleData(Particle, InFrameData.SimulationTransform);
			}
			else
			{
				if (AChaosVDParticleActor* NewParticleVDInstance = SpawnParticleFromRecordedData(Particle, InFrameData))
				{
					// TODO: Precalculate the max num of entries we would see in the loaded file, and use that number to pre-allocate this map
					SolverSceneData->RegisterParticleActor(ParticleVDInstanceID, NewParticleVDInstance);
				}
				else
				{
					//TODO: Handle this error
					ensure(false);
				}
			}
			
			UpdatingSceneSlowTask.EnterProgressFrame(PercentagePerElement);
		}
	}

	UpdateParticlesCollisionData(InRecordedStepData, SolverID);

	UpdateJointConstraintsData(InRecordedStepData, SolverID);
	
	const TMap<int32, AChaosVDParticleActor*>& AllSolverParticlesByID = SolverSceneData->GetAllParticleActorsByIDMap();

	for (const TPair<int32, AChaosVDParticleActor*>& ParticleActorWithID : AllSolverParticlesByID)
	{
		// If we are playing back a keyframe, the scene should only contain what it is in the recorded data
		const bool bShouldDestroyParticleAnyway = InFrameData.bIsKeyFrame && !ParticlesIDsInRecordedStepData.Contains(ParticleActorWithID.Key);
		
		if (bShouldDestroyParticleAnyway || InFrameData.ParticlesDestroyedIDs.Contains(ParticleActorWithID.Key))
		{
			// In large maps moving at high speed (like when moving on a vehicle), level streaming adds/removes hundreds of actors (and therefore particles) constantly.
			// Destroying particle actors is expensive, specially if we need to spawn them again sooner as we will nee to rebuild-them.
			// So, we deactivate them instead.

			// TODO: We need an actor pool system, so we can keep memory under control as well.
			if (AChaosVDParticleActor* ActorToDeactivate = ParticleActorWithID.Value)
			{
				if (IsObjectSelected(ActorToDeactivate))
				{
					ClearSelectionAndNotify();
				}

				ActorToDeactivate->SetIsActive(false);
			}

		}
	}
	
	OnSceneUpdated().Broadcast();
}

void FChaosVDScene::UpdateParticlesCollisionData(const FChaosVDStepData& InRecordedStepData, int32 SolverID)
{
	if (AChaosVDSolverInfoActor* SolverDataInfoContainer = SolverDataContainerBySolverID.FindChecked(SolverID))
	{
		if (UChaosVDSolverCollisionDataComponent* CollisionDataContainer = SolverDataInfoContainer->GetCollisionDataComponent())
		{
			CollisionDataContainer->UpdateCollisionData(InRecordedStepData.RecordedMidPhases);
		}
	}
}

void FChaosVDScene::UpdateJointConstraintsData(const FChaosVDStepData& InRecordedStepData, int32 SolverID)
{
	if (AChaosVDSolverInfoActor* SolverDataInfoContainer = SolverDataContainerBySolverID.FindChecked(SolverID))
	{
		if (UChaosVDSolverJointConstraintDataComponent* JointsDataContainer = SolverDataInfoContainer->GetJointsDataComponent())
		{
			JointsDataContainer->UpdateConstraintData(InRecordedStepData.RecordedJointConstraints);
		}
	}
}

void FChaosVDScene::HandleNewGeometryData(const Chaos::FConstImplicitObjectPtr& GeometryData, const uint32 GeometryID) const
{
	NewGeometryAvailableDelegate.Broadcast(GeometryData, GeometryID);
}

void FChaosVDScene::CreateSolverInfoActor(int32 SolverID)
{
	if (!SolverDataContainerBySolverID.Contains(SolverID))
	{
		AChaosVDSolverInfoActor* SolverDataInfo = PhysicsVDWorld->SpawnActor<AChaosVDSolverInfoActor>();
		check(SolverDataInfo);

		FString SolverName = LoadedRecording->GetSolverName_AssumedLocked(SolverID);
		const bool bIsServer = SolverName.Contains(TEXT("Server"));

		const FStringFormatOrderedArguments Args {SolverName, FString::FromInt(SolverID)};
		const FName FolderPath = *FString::Format(TEXT("Solver {0} | ID {1}"), Args);

		SolverDataInfo->SetFolderPath(FolderPath);

		SolverDataInfo->SetSolverID(SolverID);
		SolverDataInfo->SetSolverName(SolverName);
		SolverDataInfo->SetScene(AsWeak());
		SolverDataInfo->SetIsServer(bIsServer);

		SolverDataContainerBySolverID.Add(SolverID, SolverDataInfo);

		SolverInfoActorCreatedDelegate.Broadcast(SolverDataInfo);
	}
}

void FChaosVDScene::HandleEnterNewGameFrame(int32 FrameNumber, const TArray<int32>& AvailableSolversIds, const FChaosVDGameFrameData& InNewGameFrameData)
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

		CreateSolverInfoActor(SolverID);
	}

	int32 AmountRemoved = 0;

	for (TMap<int32, AChaosVDSolverInfoActor*>::TIterator RemoveIterator = SolverDataContainerBySolverID.CreateIterator(); RemoveIterator; ++RemoveIterator)
	{
		if (!AvailableSolversSet.Contains(RemoveIterator.Key()))
		{
			UE_LOG(LogChaosVDEditor, Log, TEXT("[%s] Removing Solver [%d] as it is no longer present in the recording"), ANSI_TO_TCHAR(__FUNCTION__), RemoveIterator.Key());

			if (AChaosVDSolverInfoActor* SolverInfoActor = RemoveIterator.Value())
			{
				PhysicsVDWorld->DestroyActor(SolverInfoActor);
			}

			RemoveIterator.RemoveCurrent();
			AmountRemoved++;
		}
	}

	if (AmountRemoved > 0)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

	if (SceneQueriesContainer)
	{
		if (UChaosVDSceneQueryDataComponent* QueryDataComponent = SceneQueriesContainer->GetSceneQueryDataComponent())
		{
			QueryDataComponent->UpdateQueriesFromFrameData(InNewGameFrameData);
		}
	}
}

void FChaosVDScene::CleanUpScene()
{
	constexpr float AmountOfWork = 1.0f;
	const float PercentagePerElement = 1.0f / SolverDataContainerBySolverID.Num();

	FScopedSlowTask CleaningSceneSlowTask(AmountOfWork, LOCTEXT("CleaningupSceneSolverMessage", "Clearing Solver Data ..."));
	CleaningSceneSlowTask.MakeDialog();

	ClearSelectionAndNotify();

	if (PhysicsVDWorld)
	{
		for (const TPair<int32, AChaosVDSolverInfoActor*>& SolverDataInfoWithID : SolverDataContainerBySolverID)
		{
			PhysicsVDWorld->DestroyActor(SolverDataInfoWithID.Value);
			CleaningSceneSlowTask.EnterProgressFrame(PercentagePerElement);
		}
	}

	SolverDataContainerBySolverID.Reset();
}

Chaos::FConstImplicitObjectPtr FChaosVDScene::GetUpdatedGeometry(int32 GeometryID) const
{
	if (ensure(LoadedRecording.IsValid()))
	{
		if (const Chaos::FConstImplicitObjectPtr* Geometry = LoadedRecording->GetGeometryMap().Find(GeometryID))
		{
			return *Geometry;
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("Geometry for key [%d] is not loaded in the recording yet"), GeometryID);
		}
	}

	return nullptr;
}

AChaosVDParticleActor* FChaosVDScene::GetParticleActor(int32 SolverID, int32 ParticleID)
{
	if (AChaosVDSolverInfoActor** SolverDataInfo = SolverDataContainerBySolverID.Find(SolverID))
	{
		return (*SolverDataInfo)->GetParticleActor(ParticleID);
	}

	return nullptr;
}

AChaosVDSolverInfoActor* FChaosVDScene::GetSolverInfoActor(int32 SolverID)
{
	if (AChaosVDSolverInfoActor** SolverDataInfo = SolverDataContainerBySolverID.Find(SolverID))
	{
		return *SolverDataInfo;
	}

	return nullptr;
}

bool FChaosVDScene::IsSolverForServer(int32 SolverID) const
{
	if (const AChaosVDSolverInfoActor* const * PSolverDataInfo = SolverDataContainerBySolverID.Find(SolverID))
	{
		const AChaosVDSolverInfoActor* SolverDataInfo = *PSolverDataInfo;
		return SolverDataInfo->GetIsServer();
	}

	return false;
}

UChaosVDSceneQueryDataComponent* FChaosVDScene::GetSceneQueryDataContainerComponent() const
{
	return SceneQueriesContainer ? SceneQueriesContainer->GetSceneQueryDataComponent() : nullptr;
}

AChaosVDParticleActor* FChaosVDScene::SpawnParticleFromRecordedData(const TSharedPtr<FChaosVDParticleDataWrapper>& InParticleData, const FChaosVDSolverFrameData& InFrameData)
{
	using namespace Chaos;

	if (!InParticleData.IsValid())
	{
		return nullptr;
	}

	if (AChaosVDParticleActor* NewActor = PhysicsVDWorld->SpawnActor<AChaosVDParticleActor>())
	{
		NewActor->SetIsActive(true);
		NewActor->SetScene(AsShared());
		NewActor->SetIsServerParticle(IsSolverForServer(InParticleData->SolverID));
		NewActor->UpdateFromRecordedParticleData(InParticleData, InFrameData.SimulationTransform);

		const bool bHasDebugName = !InParticleData->DebugName.IsEmpty();
		NewActor->SetActorLabel(bHasDebugName ? InParticleData->DebugName : TEXT("Unnamed Particle - ID : ") + FString::FromInt(InParticleData->ParticleIndex));

		return NewActor;
	}

	return nullptr;
}

int32 FChaosVDScene::GetIDForRecordedParticleData(const TSharedPtr<FChaosVDParticleDataWrapper>& InParticleData) const
{
	return InParticleData ? InParticleData->ParticleIndex : INDEX_NONE;
}

void FChaosVDScene::CreateBaseLights(UWorld* TargetWorld) const
{
	if (!TargetWorld)
	{
		return;
	}

	const FName LightingFolderPath("ChaosVisualDebugger/Lighting");

	const FVector SpawnPosition(0.0, 0.0, 2000.0);
	
	if (const UChaosVDEditorSettings* Settings = GetDefault<UChaosVDEditorSettings>())
	{
		if (ADirectionalLight* DirectionalLightActor = TargetWorld->SpawnActor<ADirectionalLight>())
		{
			DirectionalLightActor->SetCastShadows(false);
			DirectionalLightActor->SetMobility(EComponentMobility::Movable);
			DirectionalLightActor->SetActorLocation(SpawnPosition);

			DirectionalLightActor->SetFolderPath(LightingFolderPath);

			TSubclassOf<AActor> SkySphereClass = Settings->SkySphereActorClass.TryLoadClass<AActor>();
			SkySphere = TargetWorld->SpawnActor(SkySphereClass.Get());
			if (SkySphere)
			{
				SkySphere->SetActorLocation(SpawnPosition);
				SkySphere->SetFolderPath(LightingFolderPath);
				if (SkySphere->Implements<UChaosVDSkySphereInterface>())
				{
					IChaosVDSkySphereInterface::Execute_SetDirectionalLightSource(SkySphere, DirectionalLightActor);
				}
			}
		}
	}
}

void FChaosVDScene::CreateSceneQueriesContainer(UWorld* TargetWorld)
{
	const FName FolderPath("ChaosVisualDebugger/SceneQueries");

	SceneQueriesContainer = TargetWorld->SpawnActor<AChaosVDSceneQueryDataContainer>();
	SceneQueriesContainer->SetFolderPath(FolderPath);
	SceneQueriesContainer->SetScene(AsWeak());
}

AActor* FChaosVDScene::CreateMeshComponentsContainer(UWorld* TargetWorld)
{
	const FName GeometryFolderPath("ChaosVisualDebugger/GeneratedMeshComponents");

	MeshComponentContainerActor = TargetWorld->SpawnActor<AActor>();
	MeshComponentContainerActor->SetFolderPath(GeometryFolderPath);

	return MeshComponentContainerActor;
}

UWorld* FChaosVDScene::CreatePhysicsVDWorld()
{
	const FName UniqueWorldName = FName(FGuid::NewGuid().ToString());
	UWorld* NewWorld = NewObject<UWorld>( GetTransientPackage(), UniqueWorldName );
	
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

	if (ULevel* Level = NewWorld->GetCurrentLevel())
	{
		Level->SetUseActorFolders(true);
	}

	CreateBaseLights(NewWorld);
	CreateSceneQueriesContainer(NewWorld);
	CreateMeshComponentsContainer(NewWorld);

	ActorDestroyedHandle = NewWorld->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateRaw(this, &FChaosVDScene::HandleActorDestroyed));
	
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

void FChaosVDScene::HandleVisibilitySettingsChanged(UChaosVDEditorSettings* SettingsObject)
{
	for (const TPair<int32, AChaosVDSolverInfoActor*>& SolverDataInfoWithID : SolverDataContainerBySolverID)
	{
		if (AChaosVDSolverInfoActor* SolverDataInfo = SolverDataInfoWithID.Value)
		{
			SolverDataInfo->HandleVisibilitySettingsUpdated();
		}
	}
}

void FChaosVDScene::HandleColorSettingsChanged(UChaosVDEditorSettings* SettingsObject)
{
	for (const TPair<int32, AChaosVDSolverInfoActor*>& SolverDataInfoWithID : SolverDataContainerBySolverID)
	{
		if (AChaosVDSolverInfoActor* SolverDataInfo = SolverDataInfoWithID.Value)
		{
			SolverDataInfo->HandleColorsSettingsUpdated();
		}
	}
}

void FChaosVDScene::InitializeSelectionSets()
{
	SelectionSet = NewObject<UTypedElementSelectionSet>(GetTransientPackage(), NAME_None, RF_Transactional);
	SelectionSet->AddToRoot();

	FString ActorSelectionObjectName = FString::Printf(TEXT("CVDSelectedActors-%s"), *FGuid::NewGuid().ToString());
	ActorSelection = USelection::CreateActorSelection(GetTransientPackage(), *ActorSelectionObjectName, RF_Transactional);
	ActorSelection->SetElementSelectionSet(SelectionSet);

	FString ComponentSelectionObjectName = FString::Printf(TEXT("CVDSelectedComponents-%s"), *FGuid::NewGuid().ToString());
	ComponentSelection = USelection::CreateComponentSelection(GetTransientPackage(), *ComponentSelectionObjectName, RF_Transactional);
	ComponentSelection->SetElementSelectionSet(SelectionSet);

	FString ObjectSelectionObjectName = FString::Printf(TEXT("CVDSelectedObjects-%s"), *FGuid::NewGuid().ToString());
	ObjectSelection = USelection::CreateObjectSelection(GetTransientPackage(), *ObjectSelectionObjectName, RF_Transactional);
	ObjectSelection->SetElementSelectionSet(SelectionSet);

	SelectionSet->OnPreChange().AddRaw(this, &FChaosVDScene::HandlePreSelectionChange);
	SelectionSet->OnChanged().AddRaw(this, &FChaosVDScene::HandlePostSelectionChange);
}

void FChaosVDScene::DeInitializeSelectionSets()
{
	ActorSelection->SetElementSelectionSet(nullptr);
	ComponentSelection->SetElementSelectionSet(nullptr);
	ObjectSelection->SetElementSelectionSet(nullptr);

	SelectionSet->OnPreChange().RemoveAll(this);
	SelectionSet->OnChanged().RemoveAll(this);
}

void FChaosVDScene::HandleActorDestroyed(AActor* ActorDestroyed)
{
	if (IsObjectSelected(ActorDestroyed))
	{
		ClearSelectionAndNotify();
	}
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

#undef LOCTEXT_NAMESPACE
