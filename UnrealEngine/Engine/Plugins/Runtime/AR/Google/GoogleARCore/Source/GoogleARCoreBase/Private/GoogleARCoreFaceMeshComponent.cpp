// Copyright Epic Games, Inc. All Rights Reserved.

#include "GoogleARCoreFaceMeshComponent.h"

#include "ARBlueprintLibrary.h"
#include "ARTrackable.h"
#include "ARSessionConfig.h"
#include "DrawDebugHelpers.h"

UDEPRECATED_GoogleARCoreFaceMeshComponent::UDEPRECATED_GoogleARCoreFaceMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (bAutoBindToLocalFaceMesh)
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::CreateMesh(const TArray<FVector>& Vertices, const TArray<int32>& Triangles, const TArray<FVector2D>& UV0)
{
	TArray<FVector> Normals;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, bWantsCollision);
}

FMatrix UDEPRECATED_GoogleARCoreFaceMeshComponent::GetRenderMatrix() const
{
	FTransform RenderTrans;

	switch (TransformSetting)
	{
	case EARCoreFaceComponentTransformMixing::ComponentOnly:
	{
		RenderTrans = GetComponentTransform();
		break;
	}
	case EARCoreFaceComponentTransformMixing::ComponentLocationTrackedRotation:
	{
		RenderTrans = GetComponentTransform();
		FQuat TrackedRot = LocalToWorldTransform.GetRotation();
		RenderTrans.SetRotation(TrackedRot);
		break;
	}
	case EARCoreFaceComponentTransformMixing::TrackingOnly:
	{
		RenderTrans = LocalToWorldTransform;
		break;
	}
	}
	return RenderTrans.ToMatrixWithScale();
}

class UMaterialInterface* UDEPRECATED_GoogleARCoreFaceMeshComponent::GetMaterial(int32 ElementIndex) const
{
	if (ElementIndex == 0)
	{
		return FaceMaterial;
	}
	return nullptr;
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::SetMaterial(int32 ElementIndex, class UMaterialInterface* Material)
{
	if (ElementIndex == 0)
	{
		FaceMaterial = Material;
	}
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::UpdateMesh(const TArray<FVector>& Vertices)
{
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;

	UpdateMeshSection_LinearColor(0, Vertices, Normals, UV0, VertexColors, Tangents);
}

UARFaceGeometry* UDEPRECATED_GoogleARCoreFaceMeshComponent::FindFaceGeometry()
{
	const TArray<UARTrackedGeometry*> Geometries = UARBlueprintLibrary::GetAllGeometries();
	for (UARTrackedGeometry* Geo : Geometries)
	{
		UARFaceGeometry* FaceGeo = Cast<UARFaceGeometry>(Geo);
		if (Geo->GetTrackingState() == EARTrackingState::Tracking && FaceGeo != nullptr)
		{
			return FaceGeo;
		}
	}
	return nullptr;
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::SetAutoBind(bool bAutoBind)
{
	if (bAutoBindToLocalFaceMesh != bAutoBind)
	{
		bAutoBindToLocalFaceMesh = bAutoBind;
		PrimaryComponentTick.SetTickFunctionEnable(bAutoBind);
	}
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::BindARFaceGeometry(UARFaceGeometry* FaceGeometry)
{
	BoundFaceGeometry = FaceGeometry;
}

void UDEPRECATED_GoogleARCoreFaceMeshComponent::TickComponent(float DeltaTime, ELevelTick, FActorComponentTickFunction*)
{
	if (bAutoBindToLocalFaceMesh)
	{
		if (BoundFaceGeometry == nullptr || BoundFaceGeometry->GetTrackingState() != EARTrackingState::Tracking) {
			BoundFaceGeometry = FindFaceGeometry();
		}
	}

	// Find the tracked face geometry and skip updates if it is not found (can happen if tracking is lost)
	if (BoundFaceGeometry != nullptr && BoundFaceGeometry->GetTrackingState() == EARTrackingState::Tracking)
	{
		LocalToWorldTransform = BoundFaceGeometry->GetLocalToWorldTransform();

		SetWorldTransform(LocalToWorldTransform);

		// Create or update the mesh depending on if we've been created before
		if (GetNumSections() > 0)
		{
			UpdateMesh(BoundFaceGeometry->GetVertexBuffer());
		}
		else
		{
			const TArray<FVector>& VertexBuffer = BoundFaceGeometry->GetVertexBuffer();
			CreateMesh(BoundFaceGeometry->GetVertexBuffer(), BoundFaceGeometry->GetIndexBuffer(), BoundFaceGeometry->GetUVs());
		}
	}
	else
	{
		ClearAllMeshSections();
	}
}

FTransform UDEPRECATED_GoogleARCoreFaceMeshComponent::GetTransform() const
{
	return LocalToWorldTransform;
}
