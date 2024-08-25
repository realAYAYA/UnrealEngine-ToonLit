// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActor.h"

#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDModule.h"
#include "ChaosVDScene.h"
#include "Actors/ChaosVDSolverInfoActor.h"
#include "Components/ChaosVDInstancedStaticMeshComponent.h"
#include "Components/ChaosVDSolverCollisionDataComponent.h"
#include "Components/ChaosVDStaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Engine/StaticMesh.h"

namespace Chaos::VisualDebugger::Cvars
{
	static bool bForceStaticMeshComponentUse = false;
	static FAutoConsoleVariableRef CVarChaosVDForceStaticMeshComponentUse(
		TEXT("p.Chaos.VD.Tool.ForceStaticMeshComponentUse"),
		bForceStaticMeshComponentUse,
		TEXT("If true, static mesh components will be used instead of Instanced Static mesh components when recreating the geometry for each particle"));
}

AChaosVDParticleActor::AChaosVDParticleActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent0"));
	RootComponent->SetCanEverAffectNavigation(false);
	RootComponent->bNavigationRelevant = false;
}

void AChaosVDParticleActor::UpdateFromRecordedParticleData(const TSharedPtr<FChaosVDParticleDataWrapper>& InRecordedData, const Chaos::FRigidTransform3& SimulationTransform)
{
	if (!ensure(InRecordedData.IsValid()))
	{
		return;	
	}

	if (TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		if (InRecordedData->ParticleCluster.HasValidData())
		{
			if (AChaosVDParticleActor* ParentParticle = ScenePtr->GetParticleActor(InRecordedData->SolverID, InRecordedData->ParticleCluster.ParentParticleID))
			{
				AttachToActor(ParentParticle, FAttachmentTransformRules::KeepWorldTransform);
			}
			else if (AActor* CurrentParent = GetAttachParentActor())
			{
				DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	//TODO: Make the simulation transform be cached on the CVD Scene, so we can query from it when needed
	// Copying it to each particle actor is not efficient
	CachedSimulationTransform = SimulationTransform;

	FTransform NewParticleTransform;
	bool bHasNewTransform = false;
	if (InRecordedData->ParticlePositionRotation.HasValidData())
	{
		const FVector TargetLocation = SimulationTransform.TransformPosition(InRecordedData->ParticlePositionRotation.MX);
		const FVector CurrentLocation = SimulationTransform.TransformPosition(ParticleDataPtr && ParticleDataPtr->ParticlePositionRotation.HasValidData() ? ParticleDataPtr->ParticlePositionRotation.MX : FVector::ZeroVector);

		const FQuat TargetRotation = SimulationTransform.GetRotation() * InRecordedData->ParticlePositionRotation.MR;
		const FQuat CurrentRotation = SimulationTransform.GetRotation() * (ParticleDataPtr && ParticleDataPtr->ParticlePositionRotation.HasValidData() ? ParticleDataPtr->ParticlePositionRotation.MR : FQuat::Identity);

		NewParticleTransform.SetLocation(TargetLocation);
		NewParticleTransform.SetRotation(TargetRotation);
		NewParticleTransform.SetScale3D(FVector(1.0f,1.0f,1.0f));

		bHasNewTransform = CurrentRotation != TargetRotation || CurrentLocation != TargetLocation;

	}

	// This is iterating and comparing each element of the array,
	// We might need to find a faster way of determine if the data changed, but for now this is faster than assuming it changed
	const bool bShapeDataIsDirty = !ParticleDataPtr || (ParticleDataPtr->CollisionDataPerShape != InRecordedData->CollisionDataPerShape);
	const bool bDisabledStateChanged = ParticleDataPtr && (ParticleDataPtr->ParticleDynamicsMisc.bDisabled != InRecordedData->ParticleDynamicsMisc.bDisabled);
	const bool bHasNewGeometry = !ParticleDataPtr || (ParticleDataPtr->GeometryHash != InRecordedData->GeometryHash);

	ParticleDataPtr = InRecordedData;

	if (bHasNewGeometry)
	{
		UpdateGeometry(InRecordedData->GeometryHash, EChaosVDActorGeometryUpdateFlags::ForceUpdate);
	}

	if (bHasNewTransform)
	{
		VisitGeometryInstances([NewParticleTransform](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
		{
			MeshDataHandle->SetWorldTransform(NewParticleTransform);
		});
	}

	// Now that we have updated particle data, update the Shape data and visibility as needed
	if (bShapeDataIsDirty || bHasNewGeometry)
	{
		UpdateShapeDataComponents();
		UpdateGeometryComponentsVisibility();
	}
	else if (bDisabledStateChanged)
	{
		UpdateGeometryComponentsVisibility();
	}

	UpdateGeometryColors();

	OnParticleDataUpdated().ExecuteIfBound();
}

void AChaosVDParticleActor::ProcessUpdatedAndRemovedHandles(TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>>& OutExtractedGeometryDataHandles)
{
	for (TArray<TSharedPtr<FChaosVDMeshDataInstanceHandle>>::TIterator MeshDataHandleRemoveIterator = MeshDataHandles.CreateIterator(); MeshDataHandleRemoveIterator; ++MeshDataHandleRemoveIterator)
	{
		TSharedPtr<FChaosVDMeshDataInstanceHandle>& ExistingMeshDataHandle = *MeshDataHandleRemoveIterator;
		if (ExistingMeshDataHandle.IsValid() && ExistingMeshDataHandle->GetGeometryHandle())
		{
			bool bExists = false;

			// TODO: This search is n2, but I didn't see this as bottleneck. We should check if it is worth adding this to a TSet, or implementing the < operator so we can sort the array and do a binary search
			// (avoiding the need to allocate a new container) 
			for (TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>>::TIterator HandleRemoveIterator = OutExtractedGeometryDataHandles.CreateIterator(); HandleRemoveIterator; ++HandleRemoveIterator)
			{
				const TSharedPtr<FChaosVDExtractedGeometryDataHandle> GeometryDataHandle = *HandleRemoveIterator;
				const TSharedPtr<FChaosVDExtractedGeometryDataHandle> ExistingComponentGeometryDataHandle = ExistingMeshDataHandle->GetGeometryHandle();

				const bool bBothHandlesAreValid = GeometryDataHandle && ExistingComponentGeometryDataHandle;
				if (bBothHandlesAreValid && *GeometryDataHandle == *ExistingMeshDataHandle->GetGeometryHandle())
				{
					bExists = true;

					// If we have a CVD Geometry Component for this handle, just remove it from the list as it means we don't need to re-create it
					HandleRemoveIterator.RemoveCurrent();
					break;
				}
			}

			if (!bExists)
			{
				if (IChaosVDGeometryComponent* AsGeometryComponent = Cast<IChaosVDGeometryComponent>(ExistingMeshDataHandle->GetMeshComponent()))
				{
					AsGeometryComponent->RemoveMeshInstance(ExistingMeshDataHandle);
				}

				MeshDataHandleRemoveIterator.RemoveCurrent();
			}
		}		
	}
}


void AChaosVDParticleActor::UpdateGeometry(const Chaos::FConstImplicitObjectPtr& InImplicitObject, EChaosVDActorGeometryUpdateFlags OptionsFlags)
{
	if (EnumHasAnyFlags(OptionsFlags, EChaosVDActorGeometryUpdateFlags::ForceUpdate))
	{
		bIsGeometryDataGenerationStarted = false;
	}

	if (bIsGeometryDataGenerationStarted)
	{
		return;
	}

	if (!ParticleDataPtr)
	{
		return;
	}

	if (!InImplicitObject.IsValid())
	{
		return;
	}

	const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin();
	if (!ScenePtr.IsValid())
	{
		return;
	}

	const TSharedPtr<FChaosVDGeometryBuilder> GeometryGenerator = ScenePtr->GetGeometryGenerator();
	if (!GeometryGenerator.IsValid())
	{
		return;
	}

	const int32 ObjectsToGenerateNum = InImplicitObject->CountLeafObjectsInHierarchyImpl();

	// If the new implicit object is empty, then we can just clear all the mesh components and early out
	if (ObjectsToGenerateNum == 0)
	{
		VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
		{
			if (IChaosVDGeometryComponent* AsGeometryComponent = Cast<IChaosVDGeometryComponent>(MeshDataHandle->GetMeshComponent()))
			{
				AsGeometryComponent->RemoveMeshInstance(MeshDataHandle);
			}
		});

		MeshDataHandles.Reset();
		return;
	}
	
	TArray<TSharedPtr<FChaosVDExtractedGeometryDataHandle>> OutExtractedGeometryDataHandles;
	OutExtractedGeometryDataHandles.Reserve(ObjectsToGenerateNum);

	// Heightfields need to be created as Static meshes and use normal Static Mesh components because we need LODs for them due to their high triangle count
	const bool bHasToUseStaticMeshComponent = Chaos::VisualDebugger::Cvars::bForceStaticMeshComponentUse || FChaosVDGeometryBuilder::DoesImplicitContainType(InImplicitObject, Chaos::ImplicitObjectType::HeightField);
	constexpr int32 LODsToGenerateNum = 3;
	constexpr int32 LODsToGenerateNumForInstancedStaticMesh = 0;

	GeometryGenerator->CreateMeshesFromImplicitObject<UStaticMesh>(InImplicitObject, this, OutExtractedGeometryDataHandles, bHasToUseStaticMeshComponent ? LODsToGenerateNum : LODsToGenerateNumForInstancedStaticMesh);

	// This should not happen in theory, but there might be some valid situations where it does. Adding an ensure to catch them and then evaluate if it is really an issue (if it is not I will remove the ensure later on). 
	if (!ensure(ObjectsToGenerateNum == OutExtractedGeometryDataHandles.Num()))
	{
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Geometry objects being generated doesn't match the number of objects in the implicit object | Expected [%d] | Being generated [%d] | Particle Actor [%s]"), ANSI_TO_TCHAR(__FUNCTION__), ObjectsToGenerateNum, OutExtractedGeometryDataHandles.Num(), *GetName());
	}

	// Figure out what geometry was removed, and destroy their components as needed. Also, if a geometry is already generated an active, remove it from the geometry to generate list
	ProcessUpdatedAndRemovedHandles(OutExtractedGeometryDataHandles);

	if (OutExtractedGeometryDataHandles.Num() > 0)
	{
		for (const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& ExtractedGeometryDataHandle : OutExtractedGeometryDataHandles)
		{
			//TODO: Time Slice component creation
			TSharedPtr<FChaosVDMeshDataInstanceHandle> MeshDataInstance;

			if (bHasToUseStaticMeshComponent)
			{
				MeshDataInstance = GeometryGenerator->CreateMeshDataInstance<UChaosVDStaticMeshComponent>(*ParticleDataPtr.Get(), ExtractedGeometryDataHandle);
			}
			else
			{
				MeshDataInstance = GeometryGenerator->CreateMeshDataInstance<UChaosVDInstancedStaticMeshComponent>(*ParticleDataPtr.Get(), ExtractedGeometryDataHandle);
			}

			const UMeshComponent* CreatedMeshComponent = MeshDataInstance->GetMeshComponent();
			if (!CreatedMeshComponent)
			{
				UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Failed To Create mesh component for [%s]"), ANSI_TO_TCHAR(__FUNCTION__), *GetName());
				continue;
			}

			// If we have a valid transform data, we need to update our instance with it as the mesh component is not part of this actor (and event if it is, we don't use the actor transform anymore)
			if (ParticleDataPtr && ParticleDataPtr->ParticlePositionRotation.HasValidData())
			{
				const FVector TargetLocation = CachedSimulationTransform.TransformPosition(ParticleDataPtr->ParticlePositionRotation.MX);
				const FQuat TargetRotation = CachedSimulationTransform.GetRotation() * ParticleDataPtr->ParticlePositionRotation.MR;

				FTransform ParticleTransform;
				ParticleTransform.SetLocation(TargetLocation);
				ParticleTransform.SetRotation(TargetRotation);

				MeshDataInstance->SetWorldTransform(ParticleTransform);
			}

			MeshDataHandles.Add(MeshDataInstance);
		}

		// Ensure that visibility and colorization is up to date after updating this Particle's Geometry
		UpdateGeometryComponentsVisibility();
		
		UpdateGeometryColors();

		bIsGeometryDataGenerationStarted = true;
	}
}

void AChaosVDParticleActor::UpdateGeometry(uint32 NewGeometryHash, EChaosVDActorGeometryUpdateFlags OptionsFlags)
{
	if (const TSharedPtr<FChaosVDScene>& ScenePtr = SceneWeakPtr.Pin())
	{
		if (const Chaos::FConstImplicitObjectPtr& Geometry = ScenePtr->GetUpdatedGeometry(NewGeometryHash))
		{
			UpdateGeometry(Geometry, OptionsFlags);
		}
	}
}

void AChaosVDParticleActor::SetScene(TWeakPtr<FChaosVDScene> InScene)
{
	FChaosVDSceneObjectBase::SetScene(InScene);

	if (const TSharedPtr<FChaosVDScene>& ScenePtr = SceneWeakPtr.Pin())
	{
		GeometryUpdatedDelegate = ScenePtr->OnNewGeometryAvailable().AddWeakLambda(this, [this](const Chaos::FConstImplicitObjectPtr& ImplicitObject, const uint32 ID)
		{
			if (ParticleDataPtr && ParticleDataPtr->GeometryHash == ID)
			{
				UpdateGeometry(ImplicitObject, EChaosVDActorGeometryUpdateFlags::ForceUpdate);
			}
		});
	}
}

void AChaosVDParticleActor::Destroyed()
{
	if (const TSharedPtr<FChaosVDScene>& ScenePtr = SceneWeakPtr.Pin())
	{
		ScenePtr->OnNewGeometryAvailable().Remove(GeometryUpdatedDelegate);
	}

	VisitGeometryInstances([](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		if (IChaosVDGeometryComponent* AsGeometryComponent = Cast<IChaosVDGeometryComponent>(MeshDataHandle->GetMeshComponent()))
		{
			AsGeometryComponent->RemoveMeshInstance(MeshDataHandle);
		}
	});

	MeshDataHandles.Empty();

	Super::Destroyed();
}

#if WITH_EDITOR

bool AChaosVDParticleActor::IsSelectedInEditor() const
{
	// The implementation of this method in UObject, used a global edit callback,
	// but as we don't use the global editor selection system, we need to re-route it.
	if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
	{
		return ScenePtr->IsObjectSelected(this);
	}

	return false;
}

void AChaosVDParticleActor::SetIsTemporarilyHiddenInEditor(bool bIsHidden)
{
	if (bIsHidden)
	{
		AddHiddenFlag(EChaosVDHideParticleFlags::HiddenBySceneOutliner);
	}
	else
	{
		RemoveHiddenFlag(EChaosVDHideParticleFlags::HiddenBySceneOutliner);
	}
}

FBox AChaosVDParticleActor::GetComponentsBoundingBox(bool bNonColliding, bool bIncludeFromChildActors) const
{
	FBox BoundingBox = FBox(ForceInitToZero);
	if (ParticleDataPtr)
	{
		FBoxSphereBounds::Builder BoundsBuilder;

		for (const TSharedPtr<FChaosVDMeshDataInstanceHandle>& MeshDataHandle : MeshDataHandles)
		{
			if (MeshDataHandle)
			{
				if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshDataHandle->GetMeshComponent()))
				{
					if (const UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh())
					{
						BoundsBuilder += Mesh->GetBounds().TransformBy(MeshDataHandle->GetWorldTransform());
					}
				}
			}
		}

		const FBoxSphereBounds SphereBounds= BoundsBuilder;
		BoundingBox = SphereBounds.GetBox();
	}

	return BoundingBox;
}

void AChaosVDParticleActor::GetCollisionData(TArray<TSharedPtr<FChaosVDCollisionDataFinder>>& OutCollisionDataFound)
{
	if (const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* MidPhases = GetCollisionMidPhasesArray())
	{
		OutCollisionDataFound.Reserve(MidPhases->Num());

		for (const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhasePtr : *MidPhases)
		{
			TSharedPtr<FChaosVDCollisionDataFinder> FinderData = MakeShared<FChaosVDCollisionDataFinder>();
			FinderData->OwningMidPhase = MidPhasePtr;
			FinderData->OwningConstraint = MidPhasePtr->Constraints.Num() > 0 ? &MidPhasePtr->Constraints[0] : nullptr;
			FinderData->ContactIndex = INDEX_NONE;

			OutCollisionDataFound.Add(FinderData);
		}
	}
}

bool AChaosVDParticleActor::HasCollisionData()
{
	if (const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* MidPhases = GetCollisionMidPhasesArray())
	{
		return MidPhases->Num() > 0;
	}

	return false;
}

FName AChaosVDParticleActor::GetProviderName()
{
	return GetFName();
}

void AChaosVDParticleActor::PushSelectionToProxies()
{
	Super::PushSelectionToProxies();

	VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		const bool bIsSelectedInEditor = IsSelectedInEditor();
		MeshDataHandle->SetIsSelected(bIsSelectedInEditor);
	});
}

const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>* AChaosVDParticleActor::GetCollisionMidPhasesArray() const
{
	if (!ParticleDataPtr.IsValid())
	{
		return nullptr;
	}

	const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin();
	if (!ScenePtr.IsValid())
	{
		return nullptr;
	}

	if (AChaosVDSolverInfoActor* SolverInfoActor = ScenePtr->GetSolverInfoActor(ParticleDataPtr->SolverID))
	{
		if (const UChaosVDSolverCollisionDataComponent* CollisionDataComponent = SolverInfoActor->GetCollisionDataComponent())
		{
			return CollisionDataComponent->GetMidPhasesForParticle(ParticleDataPtr->ParticleIndex, EChaosVDParticlePairSlot::Any);
		}
	}

	return nullptr;
}

void AChaosVDParticleActor::UpdateShapeDataComponents()
{
	VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		if (ParticleDataPtr)
		{
			FChaosVDGeometryComponentUtils::UpdateCollisionDataFromShapeArray(ParticleDataPtr->CollisionDataPerShape, MeshDataHandle);
		}
	});
}

void AChaosVDParticleActor::UpdateGeometryComponentsVisibility()
{
	VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		if (ParticleDataPtr)
		{
			FChaosVDGeometryComponentUtils::UpdateMeshVisibility(MeshDataHandle, *ParticleDataPtr.Get(), IsVisible());
		}
	});
}

void AChaosVDParticleActor::UpdateGeometryColors()
{
	VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		if (ParticleDataPtr)
		{
			FChaosVDGeometryComponentUtils::UpdateMeshColor(MeshDataHandle, *ParticleDataPtr.Get(), GetIsServerParticle());
		}
	});
}

void AChaosVDParticleActor::SetIsActive(bool bNewActive)
{
	if (bIsActive != bNewActive)
	{
		bIsActive = bNewActive;
#if WITH_EDITOR
		//TODO: We need to add support for this to our Scene Outliner
		// This will hide the actor and disable it in the outliner but it will still be listed
		// We need to add a way to unlist inactive particle actors without a full hierarchy rebuild, which would be too costly
		bEditable = bNewActive;
		bListedInSceneOutliner = bNewActive;

		if (bNewActive)
		{
			RemoveHiddenFlag(EChaosVDHideParticleFlags::HiddenByActiveState);
		}
		else
		{
			AddHiddenFlag(EChaosVDHideParticleFlags::HiddenByActiveState);
		}

		UpdateGeometryComponentsVisibility();

#endif

		if (const TSharedPtr<FChaosVDScene> ScenePtr = SceneWeakPtr.Pin())
		{
			ScenePtr->OnActorActiveStateChanged().Broadcast(this);
		}
	}
}

void AChaosVDParticleActor::AddHiddenFlag(EChaosVDHideParticleFlags Flag)
{
	EnumAddFlags(HideParticleFlags, Flag);

	VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		FChaosVDGeometryComponentUtils::UpdateMeshVisibility(MeshDataHandle, ParticleDataPtr ? *ParticleDataPtr.Get() : FChaosVDParticleDataWrapper(), IsVisible());
	});
}

void AChaosVDParticleActor::RemoveHiddenFlag(EChaosVDHideParticleFlags Flag)
{
	EnumRemoveFlags(HideParticleFlags, Flag);

	VisitGeometryInstances([this](const TSharedRef<FChaosVDMeshDataInstanceHandle>& MeshDataHandle)
	{
		FChaosVDGeometryComponentUtils::UpdateMeshVisibility(MeshDataHandle, ParticleDataPtr ? *ParticleDataPtr.Get() : FChaosVDParticleDataWrapper(), IsVisible());
	});
}

#endif //WITH_EDITOR
