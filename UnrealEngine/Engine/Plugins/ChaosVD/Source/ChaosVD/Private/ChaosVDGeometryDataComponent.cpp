// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDGeometryDataComponent.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDGeometryBuilder.h"
#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

FChaosVDMeshDataInstanceHandle::FChaosVDMeshDataInstanceHandle(int32 InInstanceIndex, UMeshComponent* InMeshComponent, int32 InParticleID, int32 InSolverID)
{
	MeshComponent = InMeshComponent;
	MeshInstanceIndex = InInstanceIndex;
	OwningParticleID = InParticleID;
	OwningSolverID = InSolverID;

	if (Cast<UInstancedStaticMeshComponent>(MeshComponent))
	{
		MeshComponentType = EChaosVDMeshComponent::InstancedStatic;
	}
	else if (Cast<UStaticMeshComponent>(MeshComponent))
	{
		MeshComponentType = EChaosVDMeshComponent::Static;
	}
	else
	{
		MeshComponentType = EChaosVDMeshComponent::Dynamic;
	}
}

void FChaosVDMeshDataInstanceHandle::SetWorldTransform(const FTransform& InTransform)
{
	if (!ExtractedGeometryHandle)
	{
		UE_LOG(LogChaosVDEditor, Error, TEXT("[%s] Attempted to update the world transform without a valid geometry handle"), ANSI_TO_TCHAR(__FUNCTION__));
		return;
	}

	const FTransform ExtractedRelativeTransform = ExtractedGeometryHandle->GetRelativeTransform();

	CurrentWorldTransform.SetLocation(InTransform.TransformPosition(ExtractedRelativeTransform.GetLocation()));
	CurrentWorldTransform.SetRotation(InTransform.TransformRotation(ExtractedRelativeTransform.GetRotation()));
	CurrentWorldTransform.SetScale3D(ExtractedGeometryHandle->GetRelativeTransform().GetScale3D());
	
	if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(GetMeshComponent()))
	{
		CVDGeometryComponent->UpdateInstanceWorldTransform(AsShared(), CurrentWorldTransform);
	}
}

void FChaosVDMeshDataInstanceHandle::SetInstanceColor(const FLinearColor& NewColor)
{
	if (CurrentGeometryColor != NewColor)
	{
		if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(GetMeshComponent()))
		{
			CVDGeometryComponent->UpdateInstanceColor(AsShared(), NewColor);
			CurrentGeometryColor = NewColor;
		}
	}
}

void FChaosVDMeshDataInstanceHandle::UpdateMeshComponentForCollisionData(const FChaosVDShapeCollisionData& InCollisionData)
{
	if (InCollisionData.bIsValid && CollisionData != InCollisionData)
	{
		if (const TSharedPtr<FChaosVDGeometryBuilder> GeometryBuilderPtr = GeometryBuilderInstance.Pin())
		{
			EChaosVDMeshAttributesFlags RequiredMeshAttributes = EChaosVDMeshAttributesFlags::None;

			// If this is a query only type of geometry, we need a translucent mesh
			if (InCollisionData.bQueryCollision && !InCollisionData.bSimCollision)
			{
				EnumAddFlags(RequiredMeshAttributes, EChaosVDMeshAttributesFlags::TranslucentGeometry);
			}

			// Mirrored geometry needs to be on a instanced mesh component with reversed culling
			if (GeometryBuilderPtr->HasNegativeScale(ExtractedGeometryHandle->GetRelativeTransform()))
			{
				EnumAddFlags(RequiredMeshAttributes, EChaosVDMeshAttributesFlags::MirroredGeometry);
			}

			// If the current mesh component does not meet the required mesh attributes, we need to move to a new mesh component that it does
			bool bMeshComponentWasUpdated = false;
			if (IChaosVDGeometryComponent* CVDOldGeometryComponent = Cast<IChaosVDGeometryComponent>(GetMeshComponent()))
			{
				if (RequiredMeshAttributes != CVDOldGeometryComponent->GetMeshComponentAttributeFlags())
				{
					if (bIsSelected)
					{
						CVDOldGeometryComponent->SetIsSelected(AsShared(), false);
					}

					CVDOldGeometryComponent->RemoveMeshInstance(AsShared());

					GeometryBuilderPtr->UpdateMeshDataInstance<UChaosVDInstancedStaticMeshComponent>(AsShared(), RequiredMeshAttributes);

					bMeshComponentWasUpdated = true;
				}
			}

			if (bMeshComponentWasUpdated)
			{
				if (IChaosVDGeometryComponent* CVDNewGeometryComponent = Cast<IChaosVDGeometryComponent>(GetMeshComponent()))
				{
					// Reset the color so it is updated in the next Update color calls (which always happens after updating the shape instance data)
					CurrentGeometryColor = FLinearColor(ForceInitToZero);
		
					CVDNewGeometryComponent->UpdateInstanceVisibility(AsShared(), bIsVisible);
					CVDNewGeometryComponent->SetIsSelected(AsShared(), bIsSelected);
				}
			}
		}
	}
}

void FChaosVDMeshDataInstanceHandle::SetGeometryCollisionData(const FChaosVDShapeCollisionData& InCollisionData)
{
	// If this is a static mesh component, we can't just update change the material. We need to remove this instance from the current component and move it to a
	// component that has the correct translucent mesh
	if (GetMeshComponentType() == EChaosVDMeshComponent::InstancedStatic)
	{
		UpdateMeshComponentForCollisionData(InCollisionData);
	}

	CollisionData = InCollisionData;
}

void FChaosVDMeshDataInstanceHandle::SetIsSelected(bool bInIsSelected)
{
	if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(GetMeshComponent()))
	{
		CVDGeometryComponent->SetIsSelected(AsShared(), bInIsSelected);
	}

	bIsSelected = bInIsSelected;
}

void FChaosVDMeshDataInstanceHandle::SetVisibility(bool bInIsVisible)
{
	if (IChaosVDGeometryComponent* CVDGeometryComponent = Cast<IChaosVDGeometryComponent>(GetMeshComponent()))
	{
		CVDGeometryComponent->UpdateInstanceVisibility(AsShared(), bInIsVisible);
	}

	bIsVisible = bInIsVisible;
}

void FChaosVDMeshDataInstanceHandle::HandleInstanceIndexUpdated(TArrayView<const FInstancedStaticMeshDelegates::FInstanceIndexUpdateData> InIndexUpdates)
{
	// When an index changes, we receive an array with all the indexes that were modified. We need to only act upon the update of for the index we are tracking in this handle
	for (const FInstancedStaticMeshDelegates::FInstanceIndexUpdateData& IndexUpdateData : InIndexUpdates)
	{
		switch (IndexUpdateData.Type)
		{
			case FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Added:
				break; // We don't need to process 'Added' updates as they can't affect existing IDs
			case FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Relocated:
				{
					if (MeshInstanceIndex == IndexUpdateData.OldIndex)
					{
						MeshInstanceIndex = IndexUpdateData.Index;
					}
					break;
				}

			case FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Removed:
			case FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Cleared:
			case FInstancedStaticMeshDelegates::EInstanceIndexUpdateType::Destroyed:
				{
					if (MeshInstanceIndex == IndexUpdateData.Index)
					{
						MeshInstanceIndex = INDEX_NONE;
					}
					break;
				}
			default:
				break;
		}
	}
}

void FChaosVDGeometryComponentUtils::UpdateCollisionDataFromShapeArray(const TArray<FChaosVDShapeCollisionData>& InShapeArray, const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle)
{
	if (!ensureMsgf(InInstanceHandle && InInstanceHandle->GetGeometryHandle() && InInstanceHandle->GetGeometryHandle()->GetRootImplicitObject(), TEXT("Tried to Update Collision Data without a valid Implicit Object")))
	{
		return;
	}

	FChaosVDShapeCollisionData CollisionDataToUpdate = InInstanceHandle->GetGeometryCollisionData();

	InInstanceHandle->GetGeometryHandle()->GetRootImplicitObject()->VisitObjects([InInstanceHandle, &CollisionDataToUpdate, &InShapeArray] (const Chaos::FImplicitObject* ImplicitA, const Chaos::FRigidTransform3& RelativeTransformA, const int32 RootObjectIndexA, const int32 ObjectIndex, const int32 LeafObjectIndexA)
	{
		if (!InShapeArray.IsValidIndex(LeafObjectIndexA))
		{
			return true;
		}

		if (ImplicitA == InInstanceHandle->GetGeometryHandle()->GetImplicitObject())
		{
			
			CollisionDataToUpdate = InShapeArray[LeafObjectIndexA];
			CollisionDataToUpdate.bIsComplex = FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitA, Chaos::ImplicitObjectType::HeightField) || FChaosVDGeometryBuilder::DoesImplicitContainType(ImplicitA, Chaos::ImplicitObjectType::TriangleMesh);
			CollisionDataToUpdate.bIsValid = true;
		}

		return true;
	});

	InInstanceHandle->SetGeometryCollisionData(CollisionDataToUpdate);
}

void FChaosVDGeometryComponentUtils::UpdateMeshColor(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, const FChaosVDParticleDataWrapper& InParticleData, bool bIsServer)
{
	const FChaosVDShapeCollisionData ShapeData = InInstanceHandle->GetGeometryCollisionData();
	const bool bIsQueryOnly = ShapeData.bQueryCollision && !ShapeData.bSimCollision;

	if (ShapeData.bIsValid)
	{
		FLinearColor ColorToApply = GetGeometryParticleColor(InInstanceHandle->GetGeometryHandle(), InParticleData, bIsServer);

		constexpr float QueryOnlyShapeOpacity = 0.6f;
		ColorToApply.A = bIsQueryOnly ? QueryOnlyShapeOpacity : 1.0f;

		InInstanceHandle->SetInstanceColor(ColorToApply);
	}
}

void FChaosVDGeometryComponentUtils::UpdateMeshVisibility(const TSharedPtr<FChaosVDMeshDataInstanceHandle>& InInstanceHandle, const FChaosVDParticleDataWrapper& InParticleData, bool bIsActive)
{
	if (!InInstanceHandle || !InInstanceHandle->GetGeometryHandle())
	{
		return;
	}

	if (!bIsActive)
	{
		InInstanceHandle->SetVisibility(bIsActive);
		return;
	}

	if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
	{
		const EChaosVDGeometryVisibilityFlags CurrentVisibilityFlags = static_cast<EChaosVDGeometryVisibilityFlags>(EditorSettings->GeometryVisibilityFlags);
		
		bool bShouldGeometryBeVisible = false;

		if (!EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::ShowDisabledParticles))
		{
			if (InParticleData.ParticleDynamicsMisc.HasValidData() && InParticleData.ParticleDynamicsMisc.bDisabled)
			{
				InInstanceHandle->SetVisibility(bShouldGeometryBeVisible);
				return;
			}
		}

		// TODO: Re-visit the way we determine visibility of the meshes.
		// Now that the options have grown and they will continue to do so, these checks are becoming hard to read and extend

		const bool bIsHeightfield = InInstanceHandle->GetGeometryHandle()->GetImplicitObject() && Chaos::GetInnerType(InInstanceHandle->GetGeometryHandle()->GetImplicitObject()->GetType()) == Chaos::ImplicitObjectType::HeightField;

		if (bIsHeightfield && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::ShowHeightfields))
		{
			bShouldGeometryBeVisible = true;
		}
		else
		{
			const FChaosVDShapeCollisionData InstanceShapeData = InInstanceHandle->GetGeometryCollisionData();

			if (InstanceShapeData.bIsValid)
			{
				// Complex vs Simple takes priority although this is subject to change
				const bool bShouldBeVisibleIfComplex = InstanceShapeData.bIsComplex && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Complex);
				const bool bShouldBeVisibleIfSimple = !InstanceShapeData.bIsComplex && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Simple);
		
				if (bShouldBeVisibleIfComplex || bShouldBeVisibleIfSimple)
				{
					bShouldGeometryBeVisible = (InstanceShapeData.bSimCollision && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Simulated))
					|| (InstanceShapeData.bQueryCollision && EnumHasAnyFlags(CurrentVisibilityFlags, EChaosVDGeometryVisibilityFlags::Query));
				}
			}
		}

		InInstanceHandle->SetVisibility(bShouldGeometryBeVisible);
	}
}

FLinearColor FChaosVDGeometryComponentUtils::GetGeometryParticleColor(const TSharedPtr<FChaosVDExtractedGeometryDataHandle>& InGeometryHandle, const FChaosVDParticleDataWrapper& InParticleData, bool bIsServer)
{
	constexpr FLinearColor DefaultColor(0.088542f, 0.088542f, 0.088542f);
	FLinearColor ColorToApply = DefaultColor;

	if (!InGeometryHandle)
	{
		return ColorToApply;
	}

	const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>();
	if (!EditorSettings)
	{
		return ColorToApply;
	}

	switch (EditorSettings->ParticleColorMode)
	{
	case EChaosVDParticleDebugColorMode::ShapeType:
		{
			ColorToApply = InGeometryHandle->GetImplicitObject() ? EditorSettings->ColorsByShapeType.GetColorFromShapeType(Chaos::GetInnerType(InGeometryHandle->GetImplicitObject()->GetType())) : DefaultColor;
			break;
		}
	case EChaosVDParticleDebugColorMode::State:
		{
			if (InParticleData.Type == EChaosVDParticleType::Static)
			{
				ColorToApply = EditorSettings->ColorsByParticleState.GetColorFromState(EChaosVDObjectStateType::Static);
			}
			else
			{
				ColorToApply = EditorSettings->ColorsByParticleState.GetColorFromState(InParticleData.ParticleDynamicsMisc.MObjectState);
			}
			break;
		}
	case EChaosVDParticleDebugColorMode::ClientServer:
		{
			if (InParticleData.Type == EChaosVDParticleType::Static)
			{
				ColorToApply = EditorSettings->ColorsByClientServer.GetColorFromState(bIsServer, EChaosVDObjectStateType::Static);
			}
			else
			{
				ColorToApply = EditorSettings->ColorsByClientServer.GetColorFromState(bIsServer, InParticleData.ParticleDynamicsMisc.MObjectState);
			}
			break;
		}

	case EChaosVDParticleDebugColorMode::None:
	default:
		// Nothing to do here. Color to apply is already set to the default
		break;
	}

	return ColorToApply;
}

UMaterialInterface* FChaosVDGeometryComponentUtils::GetBaseMaterialForType(EChaosVDMaterialType Type)
{
	const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>();
	if (!EditorSettings)
	{
		return nullptr;
	}

	switch(Type)
	{
		case EChaosVDMaterialType::QueryOnlyMaterial:
				return EditorSettings->QueryOnlyMeshesMaterial.Get();
		case EChaosVDMaterialType::SimOnlyMaterial:
				return EditorSettings->SimOnlyMeshesMaterial.Get();
		case EChaosVDMaterialType::Instanced:
				return EditorSettings->InstancedMeshesMaterial.Get();
		case EChaosVDMaterialType::InstancedQueryOnly:
				return EditorSettings->InstancedMeshesQueryOnlyMaterial.Get();
		default:
			return nullptr;
	}	
}

UMaterialInstanceDynamic* FChaosVDGeometryComponentUtils::CreateMaterialInstance(UMaterialInterface* BaseMaterial)
{
	if (!BaseMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, nullptr);
	DynamicMaterial->SetFlags(RF_Transient);
	
	return DynamicMaterial;
}

UMaterialInstanceDynamic* FChaosVDGeometryComponentUtils::CreateMaterialInstance(EChaosVDMaterialType Type)
{
	return CreateMaterialInstance(GetBaseMaterialForType(Type));
}
