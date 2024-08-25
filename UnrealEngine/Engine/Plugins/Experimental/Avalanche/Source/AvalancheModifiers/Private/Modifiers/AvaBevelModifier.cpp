// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaBevelModifier.h"

#include "Async/Async.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/Operations/MergeCoincidentMeshEdges.h"
#include "GroupTopology.h"
#include "Operations/MeshBevel.h"
#include "Parameterization/DynamicMeshUVEditor.h"

#define LOCTEXT_NAMESPACE "AvaBevelModifier"

void UAvaBevelModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);

	InMetadata.SetName(TEXT("Bevel"));
	InMetadata.SetCategory(TEXT("Geometry"));
#if WITH_EDITOR
	InMetadata.SetDescription(LOCTEXT("ModifierDescription", "Create chamfered or rounded corners to geometry that smooth edges and corners"));
#endif
}

void UAvaBevelModifier::Apply()
{
	UDynamicMeshComponent* const DynMeshComp = GetMeshComponent();
	
	if (!IsValid(DynMeshComp))
	{
		Fail(LOCTEXT("InvalidDynamicMeshComponent", "Invalid dynamic mesh component on modified actor"));
		return;
	}

	if (Inset <= 0.f)
	{
		Next();
		return;
	}
	
	using namespace UE::Geometry;
			
	DynMeshComp->GetDynamicMesh()->EditMesh([this, DynMeshComp](FDynamicMesh3& EditMesh) 
	{
		if (EditMesh.TriangleCount() > 0)
		{
			// weld edges
			FMergeCoincidentMeshEdges Welder(&EditMesh);
			Welder.Apply();
		}

		float BevelDistance = Inset;
		float Divider = 4.f;
				
		// Apply bevel operator for multiple iterations
		TArray<int32> NewTriangles;
		for (int32 ItIdx = 0; ItIdx < Iterations; ItIdx++)
		{
			const FGroupTopology Topology(&EditMesh, true);
					
			FMeshBevel Bevel;
			Bevel.InsetDistance = BevelDistance;
			Bevel.InitializeFromGroupTopology(EditMesh, Topology);
			Bevel.Apply(EditMesh, nullptr);
					
			NewTriangles.Append(Bevel.NewTriangles);

			// lets reduce the distance to avoid reversed triangles and overlapping triangles
			BevelDistance /= Divider;
			Divider /= 2.f;
		}

		// Get polygroup layer for back side
		FDynamicMeshPolygroupAttribute* const BevelPolygroup = FindOrCreatePolygroupLayer(EditMesh, UAvaBevelModifier::BevelPolygroupLayerName, &NewTriangles);

		// TODO : Fix UVs temp, UV modifier needed instead of this
		FRotator BoxRotation = DynMeshComp->GetComponentTransform().Rotator();
		FBox MeshBounds = static_cast<FBox>(EditMesh.GetBounds(true));
		const FTransform PlaneTransform(BoxRotation, MeshBounds.GetCenter(), FVector::OneVector);
		FDynamicMeshUVOverlay* UVOverlay = EditMesh.Attributes()->GetUVLayer(0);
		FDynamicMeshUVEditor UVEditor(&EditMesh, UVOverlay);
		const FFrame3d ProjectionFrame(PlaneTransform);
		FUVEditResult Result;
		UVEditor.SetTriangleUVsFromBoxProjection(NewTriangles, [](const FVector3d& Pos)
		{
			return Pos;
		}
		, ProjectionFrame, MeshBounds.GetSize(), 3, &Result);
				
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);	

	Next();
}

#if WITH_EDITOR
void UAvaBevelModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	
	static const FName InsetName = GET_MEMBER_NAME_CHECKED(UAvaBevelModifier, Inset);
	static const FName IterationsName = GET_MEMBER_NAME_CHECKED(UAvaBevelModifier, Iterations);
	
	if (MemberName == InsetName)
	{
		OnInsetChanged();
	}
	else if (MemberName == IterationsName)
	{
		OnIterationsChanged();
	}
}
#endif

void UAvaBevelModifier::SetInset(float InInset)
{
	if (Inset == InInset)
	{
		return;
	}

	Inset = FMath::Clamp(InInset, UAvaBevelModifier::MinInset, GetMaxBevel());
	
	OnInsetChanged();
}

void UAvaBevelModifier::SetIterations(int32 InIterations)
{
	if (Iterations == InIterations)
	{
		return;
	}

	Iterations = FMath::Clamp(InIterations, UAvaBevelModifier::MinIterations, UAvaBevelModifier::MaxIterations);
	
	OnIterationsChanged();
}

void UAvaBevelModifier::OnInsetChanged()
{
	Inset = FMath::Min(Inset, GetMaxBevel());
	MarkModifierDirty();
}

void UAvaBevelModifier::OnIterationsChanged()
{
	Inset = FMath::Min(Inset, GetMaxBevel());
	MarkModifierDirty();
}

float UAvaBevelModifier::GetMaxBevel() const
{
	float MaxBevelDistance = 0;
	
	if (!PreModifierCachedMesh.IsSet())
	{
		return MaxBevelDistance;
	}

	const FBox Bounds = static_cast<FBox>(PreModifierCachedMesh.GetValue().GetBounds(true));
	const FVector Size3d = Bounds.GetSize();

	const float MinBevelDistance = FMath::Min3(Size3d.X / 2, Size3d.Y / 2, Size3d.Z / 2);
	float Divider = 4.f;
	float BevelDistance = MinBevelDistance;
	MaxBevelDistance = MinBevelDistance;
	
	for (int32 Idx = 1; Idx < Iterations; Idx++)
	{
		const float Reduce = BevelDistance / Divider;
		MaxBevelDistance -= Reduce;
		BevelDistance = Reduce;
		Divider /= 2.f;
	}
	
	return MaxBevelDistance;
}

#undef LOCTEXT_NAMESPACE
