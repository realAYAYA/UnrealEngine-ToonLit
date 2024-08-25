// Copyright Epic Games, Inc. All Rights Reserved.

#include "Actors/ChaosVDSolverInfoActor.h"

#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "ChaosVDScene.h"
#include "EditorActorFolders.h"
#include "Components/ChaosVDParticleDataComponent.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "Components/ChaosVDSolverJointConstraintDataComponent.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Engine/World.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

AChaosVDSolverInfoActor::AChaosVDSolverInfoActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	CollisionDataComponent = CreateDefaultSubobject<UChaosVDSolverCollisionDataComponent>(TEXT("SolverCollisionDataComponent"));
	ParticleDataComponent = CreateDefaultSubobject<UChaosVDParticleDataComponent>(TEXT("ParticleCollisionDataComponent"));
	JointsDataComponent = CreateDefaultSubobject<UChaosVDSolverJointConstraintDataComponent>(TEXT("JointDataComponent"));
	bIsServer = false;
}

void AChaosVDSolverInfoActor::SetSolverName(const FString& InSolverName)
{
	SolverName = InSolverName;
	SetActorLabel(TEXT("Solver Data Container | ") + InSolverName);
}

void AChaosVDSolverInfoActor::SetScene(TWeakPtr<FChaosVDScene> InScene)
{
	FChaosVDSceneObjectBase::SetScene(InScene);

	if (TSharedPtr<FChaosVDScene> ScenePtr = InScene.Pin())
	{
		RegisterSelectionSetObject(ScenePtr->GetElementSelectionSet());
	}
}

void AChaosVDSolverInfoActor::RegisterParticleActor(int32 ParticleID, AChaosVDParticleActor* ParticleActor)
{
	if (!ensure(ParticleActor) || !ensure(ParticleActor->GetParticleData()))
	{
		return;
	}

	if (!SolverParticlesByID.Contains(ParticleID))
	{
		SolverParticlesByID.Add(ParticleID, ParticleActor);

#if WITH_EDITOR
		ApplySolverVisibilityToParticle(ParticleActor, IsTemporarilyHiddenInEditor());
#endif

	}

	ParticleActor->SetFolderPath(GetFolderPathForParticleType(ParticleActor->GetParticleData()->Type));
	CreatedFolders.Add(ParticleActor->GetFolder());
}

AChaosVDParticleActor* AChaosVDSolverInfoActor::GetParticleActor(int32 ParticleID)
{
	AChaosVDParticleActor** FoundParticleActor = SolverParticlesByID.Find(ParticleID);

	return FoundParticleActor ? *FoundParticleActor : nullptr;
}

bool AChaosVDSolverInfoActor::IsParticleSelectedByID(int32 ParticleID)
{
	// Currently CVD does not support multi selection, so this should not be slow.
	// But we might want to find another container for faster search after multi selection support is added
	return SelectedParticlesID.Contains(ParticleID);
}

bool AChaosVDSolverInfoActor::SelectParticleByID(int32 ParticleIDToSelect)
{
	const TSharedPtr<FChaosVDScene> CVDScene = SceneWeakPtr.Pin();
	if (!CVDScene)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Tried to select a particle without a valid CVD Scene."), ANSI_TO_TCHAR(__FUNCTION__))
		return false;
	}

	AChaosVDParticleActor* ParticleToSelect = GetParticleActor(ParticleIDToSelect);
	if (!ParticleToSelect)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Particle ID [%d] not found in Solver [%s]"), ANSI_TO_TCHAR(__FUNCTION__), ParticleIDToSelect, *GetSolverName());
		return false;
	}

	CVDScene->SetSelectedObject(ParticleToSelect);

	return true;
}

void AChaosVDSolverInfoActor::HandleVisibilitySettingsUpdated()
{
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleWithIDPair : SolverParticlesByID)
	{
		if (AChaosVDParticleActor* ParticleActor = ParticleWithIDPair.Value)
		{
			ParticleActor->UpdateGeometryComponentsVisibility();
		}
	}
}

void AChaosVDSolverInfoActor::HandleColorsSettingsUpdated()
{
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleWithIDPair : SolverParticlesByID)
	{
		if (AChaosVDParticleActor* ParticleActor = ParticleWithIDPair.Value)
		{
			ParticleActor->UpdateGeometryColors();
		}	
	}
}

void AChaosVDSolverInfoActor::RemoveSolverFolders(UWorld* World)
{
	if (!World)
	{
		return;
	}

	TOptional<FFolder> ParentFolder;

	for (const FFolder& Folder : CreatedFolders)
	{
		FActorFolders::Get().DeleteFolder(*World, Folder);

		// All folders for the particles from this solver will have the same parent
		if (!ParentFolder.IsSet())
		{
			ParentFolder = Folder.GetParent();
		}
	}
	
	if (ParentFolder.IsSet())
	{
		FActorFolders::Get().DeleteFolder(*World, ParentFolder.GetValue());
	}
}

bool AChaosVDSolverInfoActor::IsVisible() const
{
	return !IsTemporarilyHiddenInEditor();
}

void AChaosVDSolverInfoActor::ApplySolverVisibilityToParticle(AChaosVDParticleActor* ParticleActor, bool bIsHidden)
{
	if (!ParticleActor)
	{
		return;
	}

	if (bIsHidden)
	{
		// Note: We should probably add a priority system for the hide requests
		// For now just clear the HideBySceneOutliner flag when a hide by solver request is done as this has priority
		ParticleActor->RemoveHiddenFlag(EChaosVDHideParticleFlags::HiddenBySceneOutliner);
		ParticleActor->AddHiddenFlag(EChaosVDHideParticleFlags::HiddenBySolverVisibility);
	}
	else
	{
		ParticleActor->RemoveHiddenFlag(EChaosVDHideParticleFlags::HiddenBySolverVisibility);
	}
}

#if WITH_EDITOR
void AChaosVDSolverInfoActor::SetIsTemporarilyHiddenInEditor(bool bIsHidden)
{
	if (IsTemporarilyHiddenInEditor() != bIsHidden)
	{
		Super::SetIsTemporarilyHiddenInEditor(bIsHidden);

		constexpr float AmountOfWork = 1.0f;
		const float PercentagePerElement = 1.0f / SolverParticlesByID.Num();

		FScopedSlowTask VisibilityUpdateProgress(AmountOfWork, LOCTEXT("UpdatingParticlesVisisibility", "Updating Particles Visibility ..."));
		VisibilityUpdateProgress.MakeDialog();

		for (const TPair<int32, AChaosVDParticleActor*>& ParticleVDInstanceWithID : SolverParticlesByID)
		{
			ApplySolverVisibilityToParticle(ParticleVDInstanceWithID.Value, bIsHidden);

			VisibilityUpdateProgress.EnterProgressFrame(PercentagePerElement);
		}

		if (const TSharedPtr<FChaosVDScene> CVDScene = SceneWeakPtr.Pin())
		{
			CVDScene->OnSolverVisibilityUpdated().Broadcast(SolverID, !bIsHidden);
		}
	}
}
#endif

void AChaosVDSolverInfoActor::Destroyed()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	constexpr float AmountOfWork = 1.0f;
	const float PercentagePerElement = 1.0f / SolverParticlesByID.Num();

	FScopedSlowTask CleaningParticleDataSlowTask(AmountOfWork, LOCTEXT("CleaningParticleDataMessage", "Cleaning Up Particle Data ..."));
	CleaningParticleDataSlowTask.MakeDialog();
	
	for (const TPair<int32, AChaosVDParticleActor*>& ParticleVDInstanceWithID : SolverParticlesByID)
	{
		World->DestroyActor(ParticleVDInstanceWithID.Value);

		CleaningParticleDataSlowTask.EnterProgressFrame(PercentagePerElement);
	}

	RemoveSolverFolders(World);

	Super::Destroyed();
}

void AChaosVDSolverInfoActor::HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet)
{
	SelectedParticlesID.Reset();

	TArray<AChaosVDParticleActor*> SelectedParticles = ChangesSelectionSet->GetSelectedObjects<AChaosVDParticleActor>();

	//TODO: Support multi-selection
	if (SelectedParticles.Num() > 0)
	{
		if (AChaosVDParticleActor* SelectedParticle = SelectedParticles[0])
		{
			if (const FChaosVDParticleDataWrapper* ParticleData = SelectedParticle->GetParticleData())
			{
				SelectedParticlesID.Add(ParticleData->ParticleIndex);
			}
		}
	}
}

FName AChaosVDSolverInfoActor::GetFolderPathForParticleType(EChaosVDParticleType ParticleType)
{
	if (const FName* FoundPathPtr = FolderPathByParticlePath.Find(ParticleType))
	{
		return *FoundPathPtr;
	}
	else
	{
		const FStringFormatOrderedArguments Args {SolverName, FString::FromInt(SolverID)};
		const FName ParticleFolderPath = *FPaths::Combine(FString::Format(TEXT("Solver {0} | ID {1}"), Args), UEnum::GetDisplayValueAsText(ParticleType).ToString());

		FolderPathByParticlePath.Add(ParticleType, ParticleFolderPath);
		return ParticleFolderPath;
	}
}

#undef LOCTEXT_NAMESPACE
