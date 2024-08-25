// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDParticleActor.h"
#include "ChaosVDSceneObjectBase.h"
#include "ChaosVDSceneSelectionObserver.h"
#include "GameFramework/Actor.h"
#include "ChaosVDSolverInfoActor.generated.h"

class UChaosVDSolverJointConstraintDataComponent;
struct FChaosVDParticleDataWrapper;
class AChaosVDParticleActor;
class UChaosVDSolverCollisionDataComponent;
class UChaosVDParticleDataComponent;

enum class EChaosVDParticleType : uint8;

UCLASS()
class AChaosVDSolverInfoActor : public AActor, public FChaosVDSceneObjectBase, public FChaosVDSceneSelectionObserver
{
	GENERATED_BODY()

public:

	AChaosVDSolverInfoActor(const FObjectInitializer& ObjectInitializer);

	void SetSolverID(int32 InSolverID) { SolverID = InSolverID; }
	int32 GetSolverID() const { return SolverID; }

	void SetSolverName(const FString& InSolverName);
	const FString& GetSolverName() { return SolverName; }

	void SetIsServer(bool bInIsServer) { bIsServer = bInIsServer; }
	bool GetIsServer() const { return bIsServer; }

	virtual void SetScene(TWeakPtr<FChaosVDScene> InScene) override;

	void SetSimulationTransform(const FTransform& InSimulationTransform) { SimulationTransform = InSimulationTransform; }
	const FTransform& GetSimulationTransform() const { return SimulationTransform; }

	UChaosVDSolverCollisionDataComponent* GetCollisionDataComponent() { return CollisionDataComponent; }
	UChaosVDParticleDataComponent* GetParticleDataComponent() { return ParticleDataComponent; }
	UChaosVDSolverJointConstraintDataComponent* GetJointsDataComponent() { return JointsDataComponent; }

	void RegisterParticleActor(int32 ParticleID, AChaosVDParticleActor* ParticleActor);

	AChaosVDParticleActor* GetParticleActor(int32 ParticleID);
	const TMap<int32, AChaosVDParticleActor*>& GetAllParticleActorsByIDMap() { return  SolverParticlesByID; }

	const TArray<int32>& GetSelectedParticlesIDs() const { return SelectedParticlesID; }

	bool IsParticleSelectedByID(int32 ParticleID);

	bool SelectParticleByID(int32 ParticleIDToSelect);

	template <typename TCallback>
	void VisitSelectedParticleData(TCallback VisitCallback);

	template <typename TCallback>
	void VisitAllParticleData(TCallback VisitCallback);

	void HandleVisibilitySettingsUpdated();
	void HandleColorsSettingsUpdated();
	void RemoveSolverFolders(UWorld* World);

	bool IsVisible() const;

#if WITH_EDITOR
	void SetIsTemporarilyHiddenInEditor(bool bIsHidden) override;
#endif
	virtual void Destroyed() override;

protected:

	void ApplySolverVisibilityToParticle(AChaosVDParticleActor* ParticleActor, bool bIsHidden);

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

	FName GetFolderPathForParticleType(EChaosVDParticleType ParticleType);

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	int32 SolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	FTransform SimulationTransform;

	UPROPERTY(VisibleAnywhere, Category="Solver Data")
	FString SolverName;

	UPROPERTY()
	TObjectPtr<UChaosVDSolverCollisionDataComponent> CollisionDataComponent;

	UPROPERTY()
	TMap<int32, AChaosVDParticleActor*> SolverParticlesByID;

	TArray<int32> SelectedParticlesID;

	TSortedMap<EChaosVDParticleType, FName> FolderPathByParticlePath;

	TSet<FFolder> CreatedFolders;

	bool bIsServer;

	UPROPERTY()
	TObjectPtr<UChaosVDParticleDataComponent> ParticleDataComponent;

	UPROPERTY()
	TObjectPtr<UChaosVDSolverJointConstraintDataComponent> JointsDataComponent;

};

template <typename TCallback>
void AChaosVDSolverInfoActor::VisitSelectedParticleData(TCallback VisitCallback)
{
	for (const int32 SelectedParticleID : SelectedParticlesID)
	{
		AChaosVDParticleActor* ParticleActor = GetParticleActor(SelectedParticleID);
		const FChaosVDParticleDataWrapper* ParticleDataViewer = ParticleActor ? ParticleActor->GetParticleData() : nullptr;
		if (!ensure(ParticleDataViewer))
		{
			continue;
		}

		if (!VisitCallback(*ParticleDataViewer))
		{
			return;
		}
	}
}

template <typename TCallback>
void AChaosVDSolverInfoActor::VisitAllParticleData(TCallback VisitCallback)
{
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleWithIDPair : SolverParticlesByID)
	{
		AChaosVDParticleActor* ParticleActor = ParticleWithIDPair.Value;
		const FChaosVDParticleDataWrapper* ParticleDataViewer = ParticleActor ? ParticleActor->GetParticleData() : nullptr;
		if (!ensure(ParticleDataViewer))
		{
			continue;
		}

		if (!VisitCallback(*ParticleDataViewer))
		{
			return;
		}
	}
}
