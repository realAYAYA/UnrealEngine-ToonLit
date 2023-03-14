// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDebugElement.h"

#include "PCGComponent.h"
#include "PCGHelpers.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGActorHelpers.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace PCGDebugElement
{
	void ExecuteDebugDisplay(FPCGContext* Context)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGDebugElement::ExecuteDebugDisplay);
#if WITH_EDITORONLY_DATA
		// Early validation: if we don't have a valid PCG component, we're not going to add the debug display info
		if (!Context->SourceComponent.IsValid())
		{
			return;
		}

		const UPCGSettings* Settings = Context->GetInputSettings<UPCGSettings>();

		if (!Settings)
		{
			return;
		}

		const FPCGDebugVisualizationSettings& DebugSettings = Settings->DebugSettings;

		UStaticMesh* Mesh = DebugSettings.PointMesh.LoadSynchronous();

		if (!Mesh)
		{
			UE_LOG(LogPCG, Error, TEXT("Debug display was unable to load mesh %s"), *DebugSettings.PointMesh.ToString());
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

			AActor* TargetActor = SpatialData->TargetActor.Get();

			if (!TargetActor)
			{
				// No target actor
				UE_LOG(LogPCG, Error, TEXT("Debug display cannot show data that have no target actor"));
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

			TArray<FTransform> Instances;
			TArray<float> InstanceCustomData;

			Instances.Reserve(Points.Num());
			InstanceCustomData.Reserve(NumCustomData);

			// First, create target instance transforms
			const float PointScale = DebugSettings.PointScale;
			const bool bIsRelative = DebugSettings.ScaleMethod == EPCGDebugVisScaleMethod::Relative;
			const bool bScaleWithExtents = DebugSettings.ScaleMethod == EPCGDebugVisScaleMethod::Extents;
			const FVector MeshExtents = Mesh->GetBoundingBox().GetExtent();
			const FVector MeshCenter = Mesh->GetBoundingBox().GetCenter();

			for (const FPCGPoint& Point : Points)
			{
				FTransform& InstanceTransform = Instances.Add_GetRef(Point.Transform);
				if (bIsRelative)
				{
					InstanceTransform.SetScale3D(InstanceTransform.GetScale3D() * PointScale);
				}
				else if (bScaleWithExtents)
				{
					const FVector ScaleWithExtents = Point.GetExtents() / MeshExtents;
					const FVector TransformedBoxCenterWithOffset = InstanceTransform.TransformPosition(Point.GetLocalCenter()) - InstanceTransform.GetLocation() - MeshCenter * ScaleWithExtents;
					InstanceTransform.SetTranslation(InstanceTransform.GetTranslation() + TransformedBoxCenterWithOffset);
					InstanceTransform.SetScale3D(InstanceTransform.GetScale3D() * ScaleWithExtents);
				}
				else // absolute scaling only
				{
					InstanceTransform.SetScale3D(FVector(PointScale));
				}
			}

			FPCGISMCBuilderParameters Params;
			Params.Mesh = Mesh;
			Params.MaterialOverrides = Materials;
			Params.CollisionProfile = UCollisionProfile::NoCollision_ProfileName;
			// Note: In the future we may consider enabling culling for performance reasons, but for now culling disabled.
			Params.CullStartDistance = Params.CullEndDistance = 0;

			UInstancedStaticMeshComponent* ISMC = UPCGActorHelpers::GetOrCreateISMC(TargetActor, Context->SourceComponent.Get(), Params);
			check(ISMC);
			
			ISMC->ComponentTags.AddUnique(PCGHelpers::DefaultPCGDebugTag);
			ISMC->NumCustomDataFloats = NumCustomData;
			const int32 PreExistingInstanceCount = ISMC->GetInstanceCount();
			ISMC->AddInstances(Instances, false);

			// Then get & assign custom data
			for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
			{
				const FPCGPoint& Point = Points[PointIndex];
				InstanceCustomData.Add(Point.Density);
				const FVector Extents = Point.GetExtents();
				InstanceCustomData.Add(Extents[0]);
				InstanceCustomData.Add(Extents[1]);
				InstanceCustomData.Add(Extents[2]);
				InstanceCustomData.Add(Point.Color[0]);
				InstanceCustomData.Add(Point.Color[1]);
				InstanceCustomData.Add(Point.Color[2]);
				InstanceCustomData.Add(Point.Color[3]);

				ISMC->SetCustomData(PointIndex + PreExistingInstanceCount, InstanceCustomData);

				InstanceCustomData.Reset();
			}

			ISMC->UpdateBounds();
		}
#endif
	}
}

FPCGElementPtr UPCGDebugSettings::CreateElement() const
{
	return MakeShared<FPCGDebugElement>();
}

bool FPCGDebugElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGDebugElement::Execute);
	PCGDebugElement::ExecuteDebugDisplay(Context);
	
	Context->OutputData = Context->InputData;
	return true;
}