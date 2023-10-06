// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActor.h"

#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDScene.h"
#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Visualizers/ChaosVDParticleDataVisualizer.h"

static FAutoConsoleVariable CVarChaosVDHideVolumeAndBrushesHack(
	TEXT("p.Chaos.VD.Tool.HideVolumeAndBrushesHack"),
	true,
	TEXT("If true, it will hide any geometry if its name contains Volume or Brush"));

AChaosVDParticleActor::AChaosVDParticleActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent0"));
	CreateVisualizers();
}

void AChaosVDParticleActor::UpdateFromRecordedParticleData(const FChaosVDParticleDataWrapper& InRecordedData, const Chaos::FRigidTransform3& SimulationTransform)
{
	//TODO: Make the simulation transform be cached on the CVD Scene, so we can query from it when needed
	// Copying it to each particle actor is not efficient
	CachedSimulationTransform = SimulationTransform;

	if (InRecordedData.ParticlePositionRotation.HasValidData())
	{
		// TODO: Only update if the transform (either the simulation or the particle) has changed;
		SetActorLocationAndRotation(SimulationTransform.TransformPosition(InRecordedData.ParticlePositionRotation.MX), SimulationTransform.GetRotation() * InRecordedData.ParticlePositionRotation.MR, false);
	}

	if (ParticleDataViewer.GeometryHash != InRecordedData.GeometryHash)
	{
		UpdateGeometry(InRecordedData.GeometryHash, EChaosVDActorGeometryUpdateFlags::ForceUpdate);
	}

	// TODO: We should store a ptr to the data and in our custom details panel draw it
	ParticleDataViewer = InRecordedData;
}

void AChaosVDParticleActor::UpdateCollisionData(const TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>& InRecordedMidPhases)
{
	// TODO: We should store a ptr to the data and in our custom details panel draw it
	ParticleDataViewer.ParticleMidPhases.Reserve(InRecordedMidPhases.Num());
	ParticleDataViewer.ParticleMidPhases.Reset(InRecordedMidPhases.Num());

	for (const TSharedPtr<FChaosVDParticlePairMidPhase>& MidPhase : InRecordedMidPhases)
	{
		ParticleDataViewer.ParticleMidPhases.Emplace(*MidPhase.Get());
	}
}

void AChaosVDParticleActor::UpdateCollisionData(const TArray<FChaosVDConstraint>& InRecordedConstraints)
{
	// TODO: We should store a ptr to the data and in our custom details panel draw it
	ParticleDataViewer.ParticleConstraints.Reserve(InRecordedConstraints.Num());
	ParticleDataViewer.ParticleConstraints.Reset(InRecordedConstraints.Num());

	for (const FChaosVDConstraint& Constraint : InRecordedConstraints)
	{
		ParticleDataViewer.ParticleConstraints.Emplace(Constraint);
	}
}

void AChaosVDParticleActor::UpdateGeometry(const TSharedPtr<const Chaos::FImplicitObject>& ImplicitObject, EChaosVDActorGeometryUpdateFlags OptionsFlags)
{
	if (EnumHasAnyFlags(OptionsFlags, EChaosVDActorGeometryUpdateFlags::ForceUpdate))
	{
		bIsGeometryDataGenerationStarted = false;

		for (TWeakObjectPtr<UMeshComponent>& MeshComponent : MeshComponents)
		{
			if (MeshComponent.IsValid())
			{
				MeshComponent->DestroyComponent();
			}
		}

		MeshComponents.Reset();
	}

	if (bIsGeometryDataGenerationStarted)
	{
		return;
	}

	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		if (const TSharedPtr<FChaosVDGeometryBuilder>& GeometryGenerator = ScenePtr->GetGeometryGenerator())
		{
			TArray<TWeakObjectPtr<UMeshComponent>> OutGeneratedMeshComponents;
			Chaos::FRigidTransform3 Transform;

			// Heightfields need to be created as Static meshes and use normal Static Mesh components because we need LODs for them due to their high triangle count
			if (FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitObject.Get(), Chaos::ImplicitObjectType::HeightField))
			{
				constexpr int32 LODsToGenerateNum = 3;
				constexpr int32 StartingMeshComponentIndex = 0;
				GeometryGenerator->CreateMeshComponentsFromImplicit<UStaticMesh, UStaticMeshComponent>(ImplicitObject.Get(), this, OutGeneratedMeshComponents, Transform, StartingMeshComponentIndex, LODsToGenerateNum);
			}
			else
			{
				GeometryGenerator->CreateMeshComponentsFromImplicit<UStaticMesh, UInstancedStaticMeshComponent>(ImplicitObject.Get(), this, OutGeneratedMeshComponents, Transform);
			}

			if (OutGeneratedMeshComponents.Num() > 0)
			{
				MeshComponents.Append(OutGeneratedMeshComponents);

				if (CVarChaosVDHideVolumeAndBrushesHack->GetBool())
				{
					// This is a temp hack (and is not performant) we need until we have a proper way to filer out trigger volumes/brushes at will
					// Without this most maps will be covered in boxes
					if (ParticleDataViewer.DebugName.Contains(TEXT("Brush")) || ParticleDataViewer.DebugName.Contains(TEXT("Volume")))
					{
						for (TWeakObjectPtr<UMeshComponent> MeshComponent : OutGeneratedMeshComponents)
						{
							if (MeshComponent.IsValid())
							{
								MeshComponent->SetVisibility(false);
							}
						}
					}
				}

				bIsGeometryDataGenerationStarted = true;
			}
		}
	}
}

void AChaosVDParticleActor::UpdateGeometry(uint32 NewGeometryHash, EChaosVDActorGeometryUpdateFlags OptionsFlags)
{
	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		if (const TSharedPtr<const Chaos::FImplicitObject>* Geometry = ScenePtr->GetUpdatedGeometry(NewGeometryHash))
		{
			UpdateGeometry(*Geometry, OptionsFlags);
		}
	}
}

void AChaosVDParticleActor::SetScene(const TSharedPtr<FChaosVDScene>& InScene)
{
	OwningScene = InScene;

	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		GeometryUpdatedDelegate = ScenePtr->OnNewGeometryAvailable().AddWeakLambda(this, [this](const TSharedPtr<const Chaos::FImplicitObject>& ImplicitObject, const uint32 ID)
		{
			if (ParticleDataViewer.GeometryHash == ID)
			{
				UpdateGeometry(ImplicitObject);
			}
		});
	}
}

void AChaosVDParticleActor::BeginDestroy()
{
	if (const TSharedPtr<FChaosVDScene>& ScenePtr = OwningScene.Pin())
	{
		ScenePtr->OnNewGeometryAvailable().Remove(GeometryUpdatedDelegate);
	}

	Super::BeginDestroy();
}

void AChaosVDParticleActor::GetVisualizationContext(FChaosVDVisualizationContext& OutVisualizationContext)
{
	OutVisualizationContext.SpaceTransform = CachedSimulationTransform;
	OutVisualizationContext.CVDScene = OwningScene;
	OutVisualizationContext.SolverID = ParticleDataViewer.SolverID;
}

void AChaosVDParticleActor::CreateVisualizers()
{
	CVDVisualizers.Add(FChaosVDParticleDataVisualizer::VisualizerID,MakeUnique<FChaosVDParticleDataVisualizer>(*this));
	CVDVisualizers.Add(FChaosVDCollisionDataVisualizer::VisualizerID, MakeUnique<FChaosVDCollisionDataVisualizer>(*this));
}

#if WITH_EDITOR

bool AChaosVDParticleActor::IsSelectedInEditor() const
{
	// The implementation of this method in UObject, used a global edit callback,
	// but as we don't use the global editor selection system, we need to re-route it.
	if (TSharedPtr<FChaosVDScene> ScenePtr = OwningScene.Pin())
	{
		return ScenePtr->IsObjectSelected(this);
	}

	return false;
}

void AChaosVDParticleActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(AChaosVDParticleActor, ParticleDataViewer))
	{
		// Not particularly useful for now. This is a test code verifying we can react to changes in the data.
		// In the future this could be part of the Re-simulation feature. When data is changed here, it can be propagated to the evolution instance that will be re-simulated
		if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(FChaosVDParticleDataWrapper, GeometryHash))
		{
			UpdateGeometry(ParticleDataViewer.GeometryHash, EChaosVDActorGeometryUpdateFlags::ForceUpdate);
		}
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(AChaosVDParticleActor, LocalCollisionDataVisualizationFlags))
	{
		if (TUniquePtr<FChaosVDDataVisualizerBase>* CollisionVisualizer = CVDVisualizers.Find(FChaosVDCollisionDataVisualizer::VisualizerID))
		{
			CollisionVisualizer->Get()->UpdateVisualizationFlags(LocalCollisionDataVisualizationFlags);
		}
	}

	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(AChaosVDParticleActor, LocalParticleDataVisualizationFlags))
	{
		if (TUniquePtr<FChaosVDDataVisualizerBase>* ParticleDataVisualizer = CVDVisualizers.Find(FChaosVDParticleDataVisualizer::VisualizerID))
		{
			ParticleDataVisualizer->Get()->UpdateVisualizationFlags(LocalParticleDataVisualizationFlags);
		}
	}
}

void AChaosVDParticleActor::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	for (const TPair<FStringView, TUniquePtr<FChaosVDDataVisualizerBase>>& VisualizerWithID : CVDVisualizers)
	{
		if (VisualizerWithID.Value)
		{
			VisualizerWithID.Value->DrawVisualization(View, PDI);
		}
	}
}

#endif //WITH_EDITOR
