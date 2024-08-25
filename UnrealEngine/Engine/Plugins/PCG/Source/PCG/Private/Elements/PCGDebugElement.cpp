// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDebugElement.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"
#include "Helpers/PCGHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGDebugElement)

#define LOCTEXT_NAMESPACE "PCGDebugElement"

namespace PCGDebugElement
{
	void ExecuteDebugDisplay(FPCGContext* Context, AActor* InTargetActor)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGDebugElement::ExecuteDebugDisplay);
#if WITH_EDITOR
		// Early validation: if we don't have a valid PCG component, we're not going to add the debug display info
		if (!Context->SourceComponent.IsValid())
		{
			return;
		}

		const UPCGSettingsInterface* SettingsInterface = Context->GetInputSettingsInterface();

		if (!SettingsInterface)
		{
			return;
		}

		const FPCGDebugVisualizationSettings& DebugSettings = SettingsInterface->DebugSettings;

		UStaticMesh* Mesh = DebugSettings.PointMesh.LoadSynchronous();

		if (!Mesh)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("UnableToLoadMesh", "Debug display was unable to load mesh '{0}'"),
				FText::FromString(DebugSettings.PointMesh.ToString())));
			return;
		}

		UMaterialInterface* Material = DebugSettings.GetMaterial().LoadSynchronous();

		TArray<UMaterialInterface*> Materials;
		if (Material)
		{
			Materials.Add(Material);
		}

		// In the case of a node with multiple output pins, we will select only the inputs from the first non-empty pin.
		bool bFilterOnPin = false;
		FName PinFilter = NAME_None;

		if (Context->Node)
		{
			TArray<FPCGPinProperties> OutputPinProperties = Context->Node->OutputPinProperties();
			for(const FPCGPinProperties& OutPin : OutputPinProperties)
			{
				if (Context->Node->IsOutputPinConnected(OutPin.Label))
				{
					PinFilter = OutPin.Label;
					bFilterOnPin = true;
					break;
				}
			}
		}

		TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
		for(const FPCGTaggedData& Input : Inputs)
		{
			// Skip output if we're filtering on the first pin
			if (bFilterOnPin && Input.Pin != PinFilter)
			{
				continue;
			}

			const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Input.Data);

			if (!SpatialData)
			{
				// Data type mismatch
				continue;
			}

			AActor* TargetActor = InTargetActor ? InTargetActor : Context->GetTargetActor(nullptr);

			if (!TargetActor)
			{
				// No target actor
				PCGE_LOG_C(Error, GraphAndLog, Context, LOCTEXT("NoTargetActor", "Debug display cannot show data that have no target actor"));
				continue;
			}

			const UPCGPointData* PointData = SpatialData->ToPointData(Context);

			if (!PointData)
			{
				continue;
			}

			const TArray<FPCGPoint>& Points = PointData->GetPoints();

			if (Points.Num() == 0)
			{
				continue;
			}

			const int NumCustomData = 8;

			TArray<FTransform> ForwardInstances;
			TArray<FTransform> ReverseInstances;
			TArray<float> InstanceCustomData;

			ForwardInstances.Reserve(Points.Num());
			InstanceCustomData.Reserve(NumCustomData);

			// First, create target instance transforms
			const float PointScale = DebugSettings.PointScale;
			const bool bIsAbsolute = DebugSettings.ScaleMethod == EPCGDebugVisScaleMethod::Absolute;
			const bool bIsRelative = DebugSettings.ScaleMethod == EPCGDebugVisScaleMethod::Relative;
			const bool bScaleWithExtents = DebugSettings.ScaleMethod == EPCGDebugVisScaleMethod::Extents;
			const FVector MeshExtents = Mesh->GetBoundingBox().GetExtent();
			const FVector MeshCenter = Mesh->GetBoundingBox().GetCenter();

			for (const FPCGPoint& Point : Points)
			{
				TArray<FTransform>& Instances = ((bIsAbsolute || Point.Transform.GetDeterminant() >= 0) ? ForwardInstances : ReverseInstances);
				FTransform& InstanceTransform = Instances.Add_GetRef(Point.Transform);
				if (bIsRelative)
				{
					InstanceTransform.SetScale3D(InstanceTransform.GetScale3D() * PointScale);
				}
				else if (bScaleWithExtents)
				{
					const FVector ScaleWithExtents = Point.GetExtents() / MeshExtents;
					const FVector TransformedBoxCenterWithOffset = InstanceTransform.TransformPosition(Point.GetLocalCenter()) - InstanceTransform.GetLocation();
					InstanceTransform.SetTranslation(InstanceTransform.GetTranslation() + TransformedBoxCenterWithOffset);
					InstanceTransform.SetScale3D(InstanceTransform.GetScale3D() * ScaleWithExtents);
				}
				else // absolute scaling only
				{
					InstanceTransform.SetScale3D(FVector(PointScale));
				}
			}

			FPCGISMCBuilderParameters Params[2];
			Params[0].NumCustomDataFloats = NumCustomData;
			Params[0].Descriptor.StaticMesh = Mesh;
			Params[0].Descriptor.OverrideMaterials = Materials;
			Params[0].Descriptor.BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			// Note: In the future we may consider enabling culling for performance reasons, but for now culling disabled.
			Params[0].Descriptor.InstanceStartCullDistance = Params[0].Descriptor.InstanceEndCullDistance = 0;
			
			// If the root actor we're binding to is movable, then the ISMC should be movable by default
			if (USceneComponent* SceneComponent = TargetActor->GetRootComponent())
			{
				Params[0].Descriptor.Mobility = SceneComponent->Mobility;
			}

			Params[1] = Params[0];
			Params[1].Descriptor.bReverseCulling = true;

			for (int32 Direction = 0; Direction < 2; ++Direction)
			{
				TArray<FTransform>& Instances = (Direction == 0 ? ForwardInstances : ReverseInstances);

				if (Instances.IsEmpty())
				{
					continue;
				}

				UInstancedStaticMeshComponent* ISMC = UPCGActorHelpers::GetOrCreateISMC(TargetActor, Context->SourceComponent.Get(), SettingsInterface->GetSettings()->UID, Params[Direction]);
				check(ISMC && ISMC->NumCustomDataFloats == NumCustomData);

				ISMC->ComponentTags.AddUnique(PCGHelpers::DefaultPCGDebugTag);
				const int32 PreExistingInstanceCount = ISMC->GetInstanceCount();
				ISMC->AddInstances(Instances, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);

				// Scan all points looking for points that match current direction and add their custom data.
				int32 PointCounter = 0;
				for (const FPCGPoint& Point : Points)
				{
					const int32 PointDirection = ((bIsAbsolute || Point.Transform.GetDeterminant() >= 0) ? 0 : 1);
					if (PointDirection != Direction)
					{
						continue;
					}

					InstanceCustomData.Add(Point.Density);
					const FVector Extents = Point.GetExtents();
					InstanceCustomData.Add(Extents[0]);
					InstanceCustomData.Add(Extents[1]);
					InstanceCustomData.Add(Extents[2]);
					InstanceCustomData.Add(Point.Color[0]);
					InstanceCustomData.Add(Point.Color[1]);
					InstanceCustomData.Add(Point.Color[2]);
					InstanceCustomData.Add(Point.Color[3]);

					ISMC->SetCustomData(PreExistingInstanceCount + PointCounter, InstanceCustomData);

					InstanceCustomData.Reset();

					++PointCounter;
				}

				ISMC->UpdateBounds();
			}
		}
#endif
	}
}

FPCGElementPtr UPCGDebugSettings::CreateElement() const
{
	return MakeShared<FPCGDebugElement>();
}

TArray<FPCGPinProperties> UPCGDebugSettings::InputPinProperties() const
{
	return Super::DefaultPointInputPinProperties();
}

TArray<FPCGPinProperties> UPCGDebugSettings::OutputPinProperties() const
{
	return TArray<FPCGPinProperties>();
}

bool FPCGDebugElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDebugElement::Execute);
	PCGDebugElement::ExecuteDebugDisplay(Context, Context->GetInputSettings<UPCGDebugSettings>()->TargetActor.Get());
	
	return true;
}

#undef LOCTEXT_NAMESPACE
