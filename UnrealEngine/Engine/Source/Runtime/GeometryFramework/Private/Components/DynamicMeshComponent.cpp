// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Components/DynamicMeshComponent.h"
#include "PrimitiveSceneProxy.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Async/Async.h"
#include "Engine/CollisionProfile.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshNormals.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "Util/ColorConstants.h"

#include "Changes/MeshVertexChange.h"
#include "Changes/MeshChange.h"
#include "DynamicMesh/MeshTransforms.h"

#include "UObject/UE5ReleaseStreamObjectVersion.h"

// default proxy for this component
#include "Components/DynamicMeshSceneProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DynamicMeshComponent)

using namespace UE::Geometry;

namespace
{
	// probably should be something defined for the whole tool framework...
#if WITH_EDITOR
	static EAsyncExecution DynamicMeshComponentAsyncExecTarget = EAsyncExecution::LargeThreadPool;
#else
	static EAsyncExecution DynamicMeshComponentAsyncExecTarget = EAsyncExecution::ThreadPool;
#endif
}



namespace UELocal
{
	static EMeshRenderAttributeFlags ConvertChangeFlagsToUpdateFlags(EDynamicMeshAttributeChangeFlags ChangeFlags)
	{
		EMeshRenderAttributeFlags UpdateFlags = EMeshRenderAttributeFlags::None;
		if ((ChangeFlags & EDynamicMeshAttributeChangeFlags::VertexPositions) != EDynamicMeshAttributeChangeFlags::Unknown)
		{
			UpdateFlags |= EMeshRenderAttributeFlags::Positions;
		}
		if ((ChangeFlags & EDynamicMeshAttributeChangeFlags::NormalsTangents) != EDynamicMeshAttributeChangeFlags::Unknown)
		{
			UpdateFlags |= EMeshRenderAttributeFlags::VertexNormals;
		}
		if ((ChangeFlags & EDynamicMeshAttributeChangeFlags::VertexColors) != EDynamicMeshAttributeChangeFlags::Unknown)
		{
			UpdateFlags |= EMeshRenderAttributeFlags::VertexColors;
		}
		if ((ChangeFlags & EDynamicMeshAttributeChangeFlags::UVs) != EDynamicMeshAttributeChangeFlags::Unknown)
		{
			UpdateFlags |= EMeshRenderAttributeFlags::VertexUVs;
		}
		return UpdateFlags;
	}

}



UDynamicMeshComponent::UDynamicMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	MeshObject = CreateDefaultSubobject<UDynamicMesh>(TEXT("DynamicMesh"));
	//MeshObject->SetFlags(RF_Transactional);

	MeshObjectChangedHandle = MeshObject->OnMeshChanged().AddUObject(this, &UDynamicMeshComponent::OnMeshObjectChanged);

	ResetProxy();
}

void UDynamicMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
}

void UDynamicMeshComponent::PostLoad()
{
	Super::PostLoad();

	const int32 UE5ReleaseStreamObjectVersion = GetLinkerCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);
	if (UE5ReleaseStreamObjectVersion < FUE5ReleaseStreamObjectVersion::DynamicMeshComponentsDefaultUseExternalTangents)
	{
		// Set the old default value
		if (TangentsType == EDynamicMeshComponentTangentsMode::Default)
		{
			TangentsType = EDynamicMeshComponentTangentsMode::NoTangents;
		}
	}

	// The intention here is that MeshObject is never nullptr, however we cannot guarantee this as a subclass
	// may have set it to null, and/or some type of serialization issue has caused it to fail to save/load.
	// Avoid immediate crashes by creating a new UDynamicMesh here in such cases
	if (ensure(MeshObject != nullptr) == false)
	{
		MeshObject = NewObject<UDynamicMesh>(this, TEXT("DynamicMesh"));
	}

	MeshObjectChangedHandle = MeshObject->OnMeshChanged().AddUObject(this, &UDynamicMeshComponent::OnMeshObjectChanged);

	ResetProxy();

	// This is a fixup for existing UDynamicMeshComponents that did not have the correct flags 
	// on the Instanced UBodySetup, these flags are now set in GetBodySetup() on new instances
	if (MeshBodySetup && IsTemplate())
	{
		MeshBodySetup->SetFlags(RF_Public | RF_ArchetypeObject);
	}

	// make sure BodySetup is created
	GetBodySetup();
}


#if WITH_EDITOR
void UDynamicMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropName = PropertyChangedEvent.GetPropertyName();
	if (PropName == GET_MEMBER_NAME_CHECKED(UDynamicMeshComponent, TangentsType))
	{
		InvalidateAutoCalculatedTangents();
	}
	else if ( (PropName == GET_MEMBER_NAME_CHECKED(UDynamicMeshComponent, bEnableComplexCollision)) ||
		(PropName == GET_MEMBER_NAME_CHECKED(UDynamicMeshComponent, CollisionType)) ||
		(PropName == GET_MEMBER_NAME_CHECKED(UDynamicMeshComponent, bDeferCollisionUpdates))  )
	{
		if (bDeferCollisionUpdates)
		{
			InvalidatePhysicsData();
		}
		else
		{
			RebuildPhysicsData();
		}
	}
}
#endif


void UDynamicMeshComponent::SetMesh(UE::Geometry::FDynamicMesh3&& MoveMesh)
{
	if (ensureMsgf(IsEditable(), TEXT("Attempted to modify the internal mesh of a UDynamicMeshComponent that is not editable")))
	{
		MeshObject->SetMesh(MoveTemp(MoveMesh));
	}
}


void UDynamicMeshComponent::ProcessMesh(
	TFunctionRef<void(const UE::Geometry::FDynamicMesh3&)> ProcessFunc ) const
{
	MeshObject->ProcessMesh(ProcessFunc);
}


void UDynamicMeshComponent::EditMesh(TFunctionRef<void(UE::Geometry::FDynamicMesh3&)> EditFunc,
										   EDynamicMeshComponentRenderUpdateMode UpdateMode )
{
	if (ensureMsgf(IsEditable(), TEXT("Attempted to modify the internal mesh of a UDynamicMeshComponent that is not editable")))
	{
		MeshObject->EditMesh(EditFunc);
		if (UpdateMode != EDynamicMeshComponentRenderUpdateMode::NoUpdate)
		{
			NotifyMeshUpdated();
		}
	}
}


void UDynamicMeshComponent::SetRenderMeshPostProcessor(TUniquePtr<IRenderMeshPostProcessor> Processor)
{
	RenderMeshPostProcessor = MoveTemp(Processor);
	if (RenderMeshPostProcessor)
	{
		if (!RenderMesh)
		{
			RenderMesh = MakeUnique<FDynamicMesh3>(*GetMesh());
		}
	}
	else
	{
		// No post processor, no render mesh
		RenderMesh = nullptr;
	}
}

FDynamicMesh3* UDynamicMeshComponent::GetRenderMesh()
{
	if (RenderMeshPostProcessor && RenderMesh)
	{
		return RenderMesh.Get();
	}
	else
	{
		return GetMesh();
	}
}

const FDynamicMesh3* UDynamicMeshComponent::GetRenderMesh() const
{
	if (RenderMeshPostProcessor && RenderMesh)
	{
		return RenderMesh.Get();
	}
	else
	{
		return GetMesh();
	}
}




void UDynamicMeshComponent::ApplyTransform(const FTransform3d& Transform, bool bInvert)
{
	if (ensureMsgf(IsEditable(), TEXT("Attempted to modify the internal mesh of a UDynamicMeshComponent that is not editable")))
	{
		MeshObject->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			if (bInvert)
			{
				MeshTransforms::ApplyTransformInverse(EditMesh, Transform, true);
			}
			else
			{
				MeshTransforms::ApplyTransform(EditMesh, Transform, true);
			}
		}, EDynamicMeshChangeType::DeformationEdit);
	}
}



bool UDynamicMeshComponent::ValidateMaterialSlots(bool bCreateIfMissing, bool bDeleteExtraSlots)
{
	int32 MaxMeshMaterialID = 0;
	MeshObject->ProcessMesh([&](const FDynamicMesh3& EditMesh)
	{
		if (EditMesh.HasAttributes() && EditMesh.Attributes()->HasMaterialID() && EditMesh.Attributes()->GetMaterialID() != nullptr)
		{
			const FDynamicMeshMaterialAttribute* MaterialIDs = EditMesh.Attributes()->GetMaterialID();
			for (int TriangleID : EditMesh.TriangleIndicesItr())
			{
				MaxMeshMaterialID = FMath::Max(MaxMeshMaterialID, MaterialIDs->GetValue(TriangleID));
			}
		}
	});
	int32 NumRequiredMaterials = MaxMeshMaterialID + 1;

	int32 NumMaterials = GetNumMaterials();
	if ( bCreateIfMissing && NumMaterials < NumRequiredMaterials )
	{
		for (int32 k = NumMaterials; k < NumRequiredMaterials; ++k)
		{
			SetMaterial(k, nullptr);
		}
	}
	NumMaterials = GetNumMaterials();

	if (bDeleteExtraSlots && NumMaterials > NumRequiredMaterials)
	{
		SetNumMaterials(NumRequiredMaterials);
	}
	NumMaterials = GetNumMaterials();

	return (NumMaterials == NumRequiredMaterials);
}


void UDynamicMeshComponent::ConfigureMaterialSet(const TArray<UMaterialInterface*>& NewMaterialSet)
{
	for (int k = 0; k < NewMaterialSet.Num(); ++k)
	{
		SetMaterial(k, NewMaterialSet[k]);
	}	
}


void UDynamicMeshComponent::SetTangentsType(EDynamicMeshComponentTangentsMode NewTangentsType)
{
	if (NewTangentsType != TangentsType)
	{
		TangentsType = NewTangentsType;
		InvalidateAutoCalculatedTangents();
	}
}

void UDynamicMeshComponent::InvalidateAutoCalculatedTangents() 
{ 
	bAutoCalculatedTangentsValid = false; 
}

const UE::Geometry::FMeshTangentsf* UDynamicMeshComponent::GetAutoCalculatedTangents() 
{ 
	if (GetTangentsType() == EDynamicMeshComponentTangentsMode::AutoCalculated && GetDynamicMesh()->GetMeshRef().HasAttributes())
	{
		UpdateAutoCalculatedTangents();
		return (bAutoCalculatedTangentsValid) ? &AutoCalculatedTangents : nullptr;
	}
	return nullptr;
}

void UDynamicMeshComponent::UpdateAutoCalculatedTangents()
{
	if (GetTangentsType() == EDynamicMeshComponentTangentsMode::AutoCalculated && bAutoCalculatedTangentsValid == false)
	{
		GetDynamicMesh()->ProcessMesh([&](const FDynamicMesh3& Mesh)
		{
			if (Mesh.HasAttributes())
			{
				const FDynamicMeshUVOverlay* UVOverlay = Mesh.Attributes()->PrimaryUV();
				const FDynamicMeshNormalOverlay* NormalOverlay = Mesh.Attributes()->PrimaryNormals();
				if (UVOverlay && NormalOverlay)
				{
					AutoCalculatedTangents.SetMesh(&Mesh);
					AutoCalculatedTangents.ComputeTriVertexTangents(NormalOverlay, UVOverlay, FComputeTangentsOptions());
					AutoCalculatedTangents.SetMesh(nullptr);
					bAutoCalculatedTangentsValid = true;
				}
			}
		});
	}
}




void UDynamicMeshComponent::UpdateLocalBounds()
{
	LocalBounds = GetMesh()->GetBounds(true);
	if (LocalBounds.MaxDim() <= 0)
	{
		// If bbox is empty, set a very small bbox to avoid log spam/etc in other engine systems.
		// The check used is generally IsNearlyZero(), which defaults to KINDA_SMALL_NUMBER, so set 
		// a slightly larger box here to be above that threshold
		LocalBounds = FAxisAlignedBox3d(FVector3d::Zero(), (double)(KINDA_SMALL_NUMBER + SMALL_NUMBER) );
	}
}

FDynamicMeshSceneProxy* UDynamicMeshComponent::GetCurrentSceneProxy() 
{ 
	if (bProxyValid)
	{
		return (FDynamicMeshSceneProxy*)SceneProxy;
	}
	return nullptr;
}


void UDynamicMeshComponent::ResetProxy()
{
	bProxyValid = false;
	InvalidateAutoCalculatedTangents();

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();
	UpdateLocalBounds();
	UpdateBounds();

	// this seems speculative, ie we may not actually have a mesh update, but we currently ResetProxy() in lots
	// of places where that is what it means
	GetDynamicMesh()->PostRealtimeUpdate();
}

void UDynamicMeshComponent::NotifyMeshUpdated()
{
	if (RenderMeshPostProcessor)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
	}

	ResetProxy();
}

void UDynamicMeshComponent::NotifyMeshModified()
{
	NotifyMeshUpdated();
}


void UDynamicMeshComponent::FastNotifyColorsUpdated()
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (Proxy)
	{
		if (HasTriangleColorFunction() && Proxy->bUsePerTriangleColor == false )
		{
			Proxy->bUsePerTriangleColor = true;
			Proxy->PerTriangleColorFunc = [this](const FDynamicMesh3* MeshIn, int TriangleID) { return GetTriangleColor(MeshIn, TriangleID); };
		} 
		else if ( !HasTriangleColorFunction() && Proxy->bUsePerTriangleColor == true)
		{
			Proxy->bUsePerTriangleColor = false;
			Proxy->PerTriangleColorFunc = nullptr;
		}

		if (HasVertexColorRemappingFunction() && Proxy->bApplyVertexColorRemapping == false)
		{
			Proxy->bApplyVertexColorRemapping = true;
			Proxy->VertexColorRemappingFunc = [this](FVector4f& Color) { RemapVertexColor(Color); };
		}
		else if (!HasVertexColorRemappingFunction() && Proxy->bApplyVertexColorRemapping == true)
		{
			Proxy->bApplyVertexColorRemapping = false;
			Proxy->VertexColorRemappingFunc = nullptr;
		}

		Proxy->FastUpdateVertices(false, false, true, false);
		//MarkRenderDynamicDataDirty();
	}
	else
	{
		ResetProxy();
	}
}



void UDynamicMeshComponent::FastNotifyPositionsUpdated(bool bNormals, bool bColors, bool bUVs)
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (Proxy)
	{
		// calculate bounds while we are updating vertices
		TFuture<void> UpdateBoundsCalc;
		UpdateBoundsCalc = Async(DynamicMeshComponentAsyncExecTarget, [this]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastPositionsUpdate_AsyncBoundsUpdate);
			UpdateLocalBounds();
		});

		GetCurrentSceneProxy()->FastUpdateVertices(true, bNormals, bColors, bUVs);

		//MarkRenderDynamicDataDirty();
		MarkRenderTransformDirty();
		UpdateBoundsCalc.Wait();
		UpdateBounds();

		GetDynamicMesh()->PostRealtimeUpdate();
	}
	else
	{
		ResetProxy();
	}
}


void UDynamicMeshComponent::FastNotifyVertexAttributesUpdated(bool bNormals, bool bColors, bool bUVs)
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (Proxy && ensure(bNormals || bColors || bUVs) )
	{
		GetCurrentSceneProxy()->FastUpdateVertices(false, bNormals, bColors, bUVs);
		//MarkRenderDynamicDataDirty();
		//MarkRenderTransformDirty();

		GetDynamicMesh()->PostRealtimeUpdate();
	}
	else
	{
		ResetProxy();
	}
}


void UDynamicMeshComponent::FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags UpdatedAttributes)
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (Proxy && ensure(UpdatedAttributes != EMeshRenderAttributeFlags::None))
	{
		bool bPositions = (UpdatedAttributes & EMeshRenderAttributeFlags::Positions) != EMeshRenderAttributeFlags::None;

		// calculate bounds while we are updating vertices
		TFuture<void> UpdateBoundsCalc;
		if (bPositions)
		{
			UpdateBoundsCalc = Async(DynamicMeshComponentAsyncExecTarget, [this]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexAttribUpdate_AsyncBoundsUpdate);
				UpdateLocalBounds();
			});
		}

		GetCurrentSceneProxy()->FastUpdateVertices(bPositions,
			(UpdatedAttributes & EMeshRenderAttributeFlags::VertexNormals) != EMeshRenderAttributeFlags::None,
			(UpdatedAttributes & EMeshRenderAttributeFlags::VertexColors) != EMeshRenderAttributeFlags::None,
			(UpdatedAttributes & EMeshRenderAttributeFlags::VertexUVs) != EMeshRenderAttributeFlags::None);

		if (bPositions)
		{
			MarkRenderTransformDirty();
			UpdateBoundsCalc.Wait();
			UpdateBounds();
		}

		GetDynamicMesh()->PostRealtimeUpdate();
	}
	else
	{
		ResetProxy();
	}
}

void UDynamicMeshComponent::FastNotifyUVsUpdated()
{
	FastNotifyVertexAttributesUpdated(EMeshRenderAttributeFlags::VertexUVs);
}


void UDynamicMeshComponent::NotifyMeshVertexAttributesModified( bool bPositions, bool bNormals, bool bUVs, bool bColors )
{
	EMeshRenderAttributeFlags Flags = EMeshRenderAttributeFlags::None;
	if (bPositions)
	{
		Flags |= EMeshRenderAttributeFlags::Positions;
	}
	if (bNormals)
	{
		Flags |= EMeshRenderAttributeFlags::VertexNormals;
	}
	if (bUVs)
	{
		Flags |= EMeshRenderAttributeFlags::VertexUVs;
	}
	if (bColors)
	{
		Flags |= EMeshRenderAttributeFlags::VertexColors;
	}

	if (Flags == EMeshRenderAttributeFlags::None)
	{
		return;
	}
	FastNotifyVertexAttributesUpdated(Flags);
}



void UDynamicMeshComponent::FastNotifySecondaryTrianglesChanged()
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (Proxy)
	{
		GetCurrentSceneProxy()->FastUpdateAllIndexBuffers();
		GetDynamicMesh()->PostRealtimeUpdate();
	}
	else
	{
		ResetProxy();
	}
}


void UDynamicMeshComponent::FastNotifyTriangleVerticesUpdated(const TArray<int32>& Triangles, EMeshRenderAttributeFlags UpdatedAttributes)
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	bool bUpdateSecondarySort = (SecondaryTriFilterFunc) &&
		((UpdatedAttributes & EMeshRenderAttributeFlags::SecondaryIndexBuffers) != EMeshRenderAttributeFlags::None);

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (!Proxy)
	{
		ResetProxy();
	}
	else if ( ! Decomposition )
	{
		FastNotifyVertexAttributesUpdated(UpdatedAttributes);
		if (bUpdateSecondarySort)
		{
			Proxy->FastUpdateAllIndexBuffers();
		}
		GetDynamicMesh()->PostRealtimeUpdate();
	}
	else
	{
		// compute list of sets to update
		TArray<int32> UpdatedSets;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_FindSets);
			for (int32 tid : Triangles)
			{
				int32 SetID = Decomposition->GetGroupForTriangle(tid);
				UpdatedSets.AddUnique(SetID);
			}
		}

		bool bPositions = (UpdatedAttributes & EMeshRenderAttributeFlags::Positions) != EMeshRenderAttributeFlags::None;

		// calculate bounds while we are updating vertices
		TFuture<void> UpdateBoundsCalc;
		if (bPositions)
		{
			UpdateBoundsCalc = Async(DynamicMeshComponentAsyncExecTarget, [this]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_AsyncBoundsUpdate);
				UpdateLocalBounds();
			});
		}

		// update the render buffers
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_ApplyUpdate);
			Proxy->FastUpdateVertices(UpdatedSets, bPositions,
				(UpdatedAttributes & EMeshRenderAttributeFlags::VertexNormals) != EMeshRenderAttributeFlags::None,
				(UpdatedAttributes & EMeshRenderAttributeFlags::VertexColors) != EMeshRenderAttributeFlags::None,
				(UpdatedAttributes & EMeshRenderAttributeFlags::VertexUVs) != EMeshRenderAttributeFlags::None);
		}

		if (bUpdateSecondarySort)
		{
			Proxy->FastUpdateIndexBuffers(UpdatedSets);
		}

		if (bPositions)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_FinalPositionsUpdate);
			MarkRenderTransformDirty();
			UpdateBoundsCalc.Wait();
			UpdateBounds();
		}

		GetDynamicMesh()->PostRealtimeUpdate();
	}
}




void UDynamicMeshComponent::FastNotifyTriangleVerticesUpdated(const TSet<int32>& Triangles, EMeshRenderAttributeFlags UpdatedAttributes)
{
	// should not be using fast paths if we have to run mesh postprocessor
	if (ensure(!RenderMeshPostProcessor) == false)
	{
		RenderMeshPostProcessor->ProcessMesh(*GetMesh(), *RenderMesh);
		ResetProxy();
		return;
	}

	bool bUpdateSecondarySort = (SecondaryTriFilterFunc) &&
		((UpdatedAttributes & EMeshRenderAttributeFlags::SecondaryIndexBuffers) != EMeshRenderAttributeFlags::None);

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	if (!Proxy)
	{
		ResetProxy();
	}
	else if (!Decomposition)
	{
		FastNotifyVertexAttributesUpdated(UpdatedAttributes);
		if (bUpdateSecondarySort)
		{
			Proxy->FastUpdateAllIndexBuffers();
		}
		GetDynamicMesh()->PostRealtimeUpdate();
	}
	else
	{
		// compute list of sets to update
		TArray<int32> UpdatedSets;
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_FindSets);
			for (int32 tid : Triangles)
			{
				int32 SetID = Decomposition->GetGroupForTriangle(tid);
				UpdatedSets.AddUnique(SetID);
			}
		}

		bool bPositions = (UpdatedAttributes & EMeshRenderAttributeFlags::Positions) != EMeshRenderAttributeFlags::None;

		// calculate bounds while we are updating vertices
		TFuture<void> UpdateBoundsCalc;
		if (bPositions)
		{
			UpdateBoundsCalc = Async(DynamicMeshComponentAsyncExecTarget, [this]()
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_AsyncBoundsUpdate);
				UpdateLocalBounds();
			});
		}

		// update the render buffers
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_ApplyUpdate);
			Proxy->FastUpdateVertices(UpdatedSets, bPositions,
				(UpdatedAttributes & EMeshRenderAttributeFlags::VertexNormals) != EMeshRenderAttributeFlags::None,
				(UpdatedAttributes & EMeshRenderAttributeFlags::VertexColors) != EMeshRenderAttributeFlags::None,
				(UpdatedAttributes & EMeshRenderAttributeFlags::VertexUVs) != EMeshRenderAttributeFlags::None);
		}

		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_UpdateIndexBuffers);
			if (bUpdateSecondarySort)
			{
				Proxy->FastUpdateIndexBuffers(UpdatedSets);
			}
		}

		// finish up, have to wait for background bounds recalculation here
		if (bPositions)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_FinalPositionsUpdate);
			MarkRenderTransformDirty();
			UpdateBoundsCalc.Wait();
			UpdateBounds();
		}

		GetDynamicMesh()->PostRealtimeUpdate();
	}
}



/**
 * Compute the combined bounding-box of the Triangles array in parallel, by computing
 * partial boxes for subsets of this array, and then combining those boxes.
 * TODO: this should move to a pulbic utility function, and possibly the block-based ParallelFor
 * should be refactored out into something more general, as this pattern is useful in many places...
 */
static FAxisAlignedBox3d ParallelComputeROIBounds(const FDynamicMesh3& Mesh, const TArray<int32>& Triangles)
{
	FAxisAlignedBox3d FinalBounds = FAxisAlignedBox3d::Empty();
	FCriticalSection FinalBoundsLock;
	int32 N = Triangles.Num();
	constexpr int32 BlockSize = 4096;
	int32 Blocks = (N / BlockSize) + 1;
	ParallelFor(Blocks, [&](int bi)
	{
		FAxisAlignedBox3d BlockBounds = FAxisAlignedBox3d::Empty();
		for (int32 k = 0; k < BlockSize; ++k)
		{
			int32 i = bi * BlockSize + k;
			if (i < N)
			{
				int32 tid = Triangles[i];
				const FIndex3i& TriV = Mesh.GetTriangleRef(tid);
				BlockBounds.Contain(Mesh.GetVertexRef(TriV.A));
				BlockBounds.Contain(Mesh.GetVertexRef(TriV.B));
				BlockBounds.Contain(Mesh.GetVertexRef(TriV.C));
			}
		}
		FinalBoundsLock.Lock();
		FinalBounds.Contain(BlockBounds);
		FinalBoundsLock.Unlock();
	});
	return FinalBounds;
}



TFuture<bool> UDynamicMeshComponent::FastNotifyTriangleVerticesUpdated_TryPrecompute(
	const TArray<int32>& Triangles,
	TArray<int32>& UpdateSetsOut,
	FAxisAlignedBox3d& BoundsOut)
{
	if ((!!RenderMeshPostProcessor) || (GetCurrentSceneProxy() == nullptr) || (!Decomposition))
	{
		// is there a simpler way to do this? cannot seem to just make a TFuture<bool>...
		return Async(DynamicMeshComponentAsyncExecTarget, []() { return false; });
	}

	return Async(DynamicMeshComponentAsyncExecTarget, [this, &Triangles, &UpdateSetsOut, &BoundsOut]()
	{
		TFuture<void> ComputeBounds = Async(DynamicMeshComponentAsyncExecTarget, [this, &BoundsOut, &Triangles]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdatePrecomp_CalcBounds);
			BoundsOut = ParallelComputeROIBounds(*GetMesh(), Triangles);
		});

		TFuture<void> ComputeSets = Async(DynamicMeshComponentAsyncExecTarget, [this, &UpdateSetsOut, &Triangles]()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdatePrecomp_FindSets);
			int32 NumBuffers = Decomposition->Num();
			TArray<std::atomic<bool>> BufferFlags;
			BufferFlags.SetNum(NumBuffers);
			for (int32 k = 0; k < NumBuffers; ++k)
			{
				BufferFlags[k] = false;
			}
			ParallelFor(Triangles.Num(), [&](int32 k)
			{
				int32 SetID = Decomposition->GetGroupForTriangle(Triangles[k]);
				BufferFlags[SetID] = true;
			});
			UpdateSetsOut.Reset();
			for (int32 k = 0; k < NumBuffers; ++k)
			{
				if (BufferFlags[k])
				{
					UpdateSetsOut.Add(k);
				}
			}

		});

		ComputeSets.Wait();
		ComputeBounds.Wait();

		return true;
	});
}


void UDynamicMeshComponent::FastNotifyTriangleVerticesUpdated_ApplyPrecompute(
	const TArray<int32>& Triangles,
	EMeshRenderAttributeFlags UpdatedAttributes, 
	TFuture<bool>& Precompute, 
	const TArray<int32>& UpdateSets, 
	const FAxisAlignedBox3d& UpdateSetBounds)
{
	Precompute.Wait();

	bool bPrecomputeOK = Precompute.Get();
	if (bPrecomputeOK == false || GetCurrentSceneProxy() == nullptr )
	{
		FastNotifyTriangleVerticesUpdated(Triangles, UpdatedAttributes);
		return;
	}

	FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy();
	bool bPositions = (UpdatedAttributes & EMeshRenderAttributeFlags::Positions) != EMeshRenderAttributeFlags::None;
	bool bUpdateSecondarySort = (SecondaryTriFilterFunc) &&
		((UpdatedAttributes & EMeshRenderAttributeFlags::SecondaryIndexBuffers) != EMeshRenderAttributeFlags::None);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_ApplyUpdate);
		Proxy->FastUpdateVertices(UpdateSets, bPositions,
			(UpdatedAttributes & EMeshRenderAttributeFlags::VertexNormals) != EMeshRenderAttributeFlags::None,
			(UpdatedAttributes & EMeshRenderAttributeFlags::VertexColors) != EMeshRenderAttributeFlags::None,
			(UpdatedAttributes & EMeshRenderAttributeFlags::VertexUVs) != EMeshRenderAttributeFlags::None);
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_UpdateIndexBuffers);
		if (bUpdateSecondarySort)
		{
			Proxy->FastUpdateIndexBuffers(UpdateSets);
		}
	}

	if (bPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SimpleDynamicMeshComponent_FastVertexUpdate_FinalPositionsUpdate);
		MarkRenderTransformDirty();
		LocalBounds.Contain(UpdateSetBounds);
		UpdateBounds();
	}

	GetDynamicMesh()->PostRealtimeUpdate();
}





FPrimitiveSceneProxy* UDynamicMeshComponent::CreateSceneProxy()
{
	// if this is not always the case, we have made incorrect assumptions
	ensure(GetCurrentSceneProxy() == nullptr);

	FDynamicMeshSceneProxy* NewProxy = nullptr;
	if (GetMesh()->TriangleCount() > 0)
	{
		NewProxy = new FDynamicMeshSceneProxy(this);

		if (TriangleColorFunc)
		{
			NewProxy->bUsePerTriangleColor = true;
			NewProxy->PerTriangleColorFunc = [this](const FDynamicMesh3* MeshIn, int TriangleID) { return GetTriangleColor(MeshIn, TriangleID); };
		}
		else if ( GetColorOverrideMode() == EDynamicMeshComponentColorOverrideMode::Polygroups )
		{
			NewProxy->bUsePerTriangleColor = true;
			NewProxy->PerTriangleColorFunc = [this](const FDynamicMesh3* MeshIn, int TriangleID) { return GetGroupColor(MeshIn, TriangleID); };
		}

		if (HasVertexColorRemappingFunction())
		{
			NewProxy->bApplyVertexColorRemapping = true;
			NewProxy->VertexColorRemappingFunc = [this](FVector4f& Color) { RemapVertexColor(Color); };
		}

		if (SecondaryTriFilterFunc)
		{
			NewProxy->bUseSecondaryTriBuffers = true;
			NewProxy->SecondaryTriFilterFunc = [this](const FDynamicMesh3* MeshIn, int32 TriangleID) 
			{ 
				return (SecondaryTriFilterFunc) ? SecondaryTriFilterFunc(MeshIn, TriangleID) : false;
			};
		}

		if (Decomposition)
		{
			NewProxy->InitializeFromDecomposition(Decomposition);
		}
		else
		{
			NewProxy->Initialize();
		}

		NewProxy->SetVerifyUsedMaterials(bProxyVerifyUsedMaterials);
	}

	bProxyValid = true;
	return NewProxy;
}



void UDynamicMeshComponent::NotifyMaterialSetUpdated()
{
	if (GetCurrentSceneProxy() != nullptr)
	{
		GetCurrentSceneProxy()->UpdatedReferencedMaterials();
	}
}


void UDynamicMeshComponent::SetTriangleColorFunction(
	TUniqueFunction<FColor(const FDynamicMesh3*, int)> TriangleColorFuncIn,
	EDynamicMeshComponentRenderUpdateMode UpdateMode)
{
	TriangleColorFunc = MoveTemp(TriangleColorFuncIn);

	if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FastUpdate)
	{
		FastNotifyColorsUpdated();
	}
	else if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FullUpdate)
	{
		NotifyMeshUpdated();
	}
}

void UDynamicMeshComponent::ClearTriangleColorFunction(EDynamicMeshComponentRenderUpdateMode UpdateMode)
{
	if (TriangleColorFunc)
	{
		TriangleColorFunc = nullptr;

		if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FastUpdate)
		{
			FastNotifyColorsUpdated();
		}
		else if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FullUpdate)
		{
			NotifyMeshUpdated();
		}
	}
}

bool UDynamicMeshComponent::HasTriangleColorFunction()
{
	return !!TriangleColorFunc;
}



void UDynamicMeshComponent::SetVertexColorRemappingFunction(
	TUniqueFunction<void(FVector4f&)> ColorMapFuncIn,
	EDynamicMeshComponentRenderUpdateMode UpdateMode)
{
	VertexColorMappingFunc = MoveTemp(ColorMapFuncIn);

	if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FastUpdate)
	{
		FastNotifyColorsUpdated();
	}
	else if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FullUpdate)
	{
		NotifyMeshUpdated();
	}
}

void UDynamicMeshComponent::ClearVertexColorRemappingFunction(EDynamicMeshComponentRenderUpdateMode UpdateMode)
{
	if (VertexColorMappingFunc)
	{
		VertexColorMappingFunc = nullptr;

		if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FastUpdate)
		{
			FastNotifyColorsUpdated();
		}
		else if (UpdateMode == EDynamicMeshComponentRenderUpdateMode::FullUpdate)
		{
			NotifyMeshUpdated();
		}
	}
}

bool UDynamicMeshComponent::HasVertexColorRemappingFunction()
{
	return !!VertexColorMappingFunc;
}


void UDynamicMeshComponent::RemapVertexColor(FVector4f& VertexColorInOut)
{
	if (VertexColorMappingFunc)
	{
		VertexColorMappingFunc(VertexColorInOut);
	}
}



void UDynamicMeshComponent::EnableSecondaryTriangleBuffers(TUniqueFunction<bool(const FDynamicMesh3*, int32)> SecondaryTriFilterFuncIn)
{
	SecondaryTriFilterFunc = MoveTemp(SecondaryTriFilterFuncIn);
	NotifyMeshUpdated();
}

void UDynamicMeshComponent::DisableSecondaryTriangleBuffers()
{
	SecondaryTriFilterFunc = nullptr;
	NotifyMeshUpdated();
}


void UDynamicMeshComponent::SetExternalDecomposition(TUniquePtr<FMeshRenderDecomposition> DecompositionIn)
{
	ensure(DecompositionIn->Num() > 0);
	Decomposition = MoveTemp(DecompositionIn);
	NotifyMeshUpdated();
}



FColor UDynamicMeshComponent::GetTriangleColor(const FDynamicMesh3* MeshIn, int TriangleID)
{
	if (TriangleColorFunc)
	{
		return TriangleColorFunc(MeshIn, TriangleID);
	}
	else
	{
		return (TriangleID % 2 == 0) ? FColor::Red : FColor::White;
	}
}


FColor UDynamicMeshComponent::GetGroupColor(const FDynamicMesh3* Mesh, int TriangleID) const
{
	int32 GroupID = Mesh->HasTriangleGroups() ? Mesh->GetTriangleGroup(TriangleID) : 0;
	return UE::Geometry::LinearColors::SelectFColor(GroupID);
}


FBoxSphereBounds UDynamicMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// can get a tighter box by calculating in world space, but we care more about performance
	FBox LocalBoundingBox = (FBox)LocalBounds;
	FBoxSphereBounds Ret(LocalBoundingBox.TransformBy(LocalToWorld));
	Ret.BoxExtent *= BoundsScale;
	Ret.SphereRadius *= BoundsScale;
	return Ret;
}




void UDynamicMeshComponent::SetInvalidateProxyOnChangeEnabled(bool bEnabled)
{
	bInvalidateProxyOnChange = bEnabled;
}


void UDynamicMeshComponent::ApplyChange(const FMeshVertexChange* Change, bool bRevert)
{
	// will fire UDynamicMesh::MeshChangedEvent, which will call OnMeshObjectChanged() below to invalidate proxy, fire change events, etc
	MeshObject->ApplyChange(Change, bRevert);
}

void UDynamicMeshComponent::ApplyChange(const FMeshChange* Change, bool bRevert)
{
	// will fire UDynamicMesh::MeshChangedEvent, which will call OnMeshObjectChanged() below to invalidate proxy, fire change events, etc
	MeshObject->ApplyChange(Change, bRevert);
}

void UDynamicMeshComponent::ApplyChange(const FMeshReplacementChange* Change, bool bRevert)
{
	// will fire UDynamicMesh::MeshChangedEvent, which will call OnMeshObjectChanged() below to invalidate proxy, fire change events, etc
	MeshObject->ApplyChange(Change, bRevert);
}

void UDynamicMeshComponent::OnMeshObjectChanged(UDynamicMesh* ChangedMeshObject, FDynamicMeshChangeInfo ChangeInfo)
{
	bool bIsFChange = (
		ChangeInfo.Type == EDynamicMeshChangeType::MeshChange
		|| ChangeInfo.Type == EDynamicMeshChangeType::MeshVertexChange
		|| ChangeInfo.Type == EDynamicMeshChangeType::MeshReplacementChange);

	if (bIsFChange)
	{
		if (bInvalidateProxyOnChange)
		{
			NotifyMeshUpdated();
		}

		OnMeshChanged.Broadcast();

		if (ChangeInfo.Type == EDynamicMeshChangeType::MeshVertexChange)
		{
			OnMeshVerticesChanged.Broadcast(this, ChangeInfo.VertexChange, ChangeInfo.bIsRevertChange);
		}
	}
	else
	{
		if (ChangeInfo.Type == EDynamicMeshChangeType::DeformationEdit)
		{
			// if ChangeType is a vertex deformation, we can do a fast-update of the vertex buffers
			// without fully rebuilding the SceneProxy
			EMeshRenderAttributeFlags UpdateFlags = UELocal::ConvertChangeFlagsToUpdateFlags(ChangeInfo.Flags);
			FastNotifyVertexAttributesUpdated(UpdateFlags);
		}
		else
		{
			NotifyMeshUpdated();
		}
		OnMeshChanged.Broadcast();
	}

	// Rebuild body setup. Should this be deferred until proxy creation? Sometimes multiple changes are emitted...
	// todo: can possibly skip this in some change situations, eg if only changing attributes
	if (bDeferCollisionUpdates || bTransientDeferCollisionUpdates )
	{
		InvalidatePhysicsData();
	}
	else
	{
		RebuildPhysicsData();
	}
}


void UDynamicMeshComponent::SetDynamicMesh(UDynamicMesh* NewMesh)
{
	if (ensure(NewMesh) == false)
	{
		return;
	}

	if (ensure(MeshObject))
	{
		MeshObject->OnMeshChanged().Remove(MeshObjectChangedHandle);
	}

	// set Outer of NewMesh to be this Component, ie transfer ownership. This is done via "renaming", which is
	// a bit odd, so the flags prevent some standard "renaming" behaviors from happening
	NewMesh->Rename( nullptr, this, REN_DontCreateRedirectors | REN_ForceNoResetLoaders);
	MeshObject = NewMesh;
	MeshObjectChangedHandle = MeshObject->OnMeshChanged().AddUObject(this, &UDynamicMeshComponent::OnMeshObjectChanged);

	NotifyMeshUpdated();
	OnMeshChanged.Broadcast();

	// Rebuild physics data
	if (bDeferCollisionUpdates || bTransientDeferCollisionUpdates)
	{
		InvalidatePhysicsData();
	}
	else
	{
		RebuildPhysicsData();
	}
}



void UDynamicMeshComponent::OnChildAttached(USceneComponent* ChildComponent)
{
	Super::OnChildAttached(ChildComponent);
	OnChildAttachmentModified.Broadcast(ChildComponent, true);
}
void UDynamicMeshComponent::OnChildDetached(USceneComponent* ChildComponent)
{
	Super::OnChildDetached(ChildComponent);
	OnChildAttachmentModified.Broadcast(ChildComponent, false);
}





bool UDynamicMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	// todo: support UPhysicsSettings::Get()->bSupportUVFromHitResults

	// this is something we currently assume, if you hit this ensure, we made a mistake
	ensure(bEnableComplexCollision);

	ProcessMesh([&](const FDynamicMesh3& Mesh)
	{
		const FDynamicMeshMaterialAttribute* MaterialAttrib = Mesh.HasAttributes() && Mesh.Attributes()->HasMaterialID() ? Mesh.Attributes()->GetMaterialID() : nullptr;

		TArray<int32> VertexMap;
		bool bIsSparseV = !Mesh.IsCompactV();
		if (bIsSparseV)
		{
			VertexMap.SetNum(Mesh.MaxVertexID());
		}

		// copy vertices
		CollisionData->Vertices.Reserve(Mesh.VertexCount());
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			int32 Index = CollisionData->Vertices.Add((FVector3f)Mesh.GetVertex(vid));
			if (bIsSparseV)
			{
				VertexMap[vid] = Index;
			}
			else
			{
				check(vid == Index);
			}
		}

		// copy triangles
		CollisionData->Indices.Reserve(Mesh.TriangleCount());
		CollisionData->MaterialIndices.Reserve(Mesh.TriangleCount());
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			FIndex3i Tri = Mesh.GetTriangle(tid);
			FTriIndices Triangle;
			Triangle.v0 = (bIsSparseV) ? VertexMap[Tri.A] : Tri.A;
			Triangle.v1 = (bIsSparseV) ? VertexMap[Tri.B] : Tri.B;
			Triangle.v2 = (bIsSparseV) ? VertexMap[Tri.C] : Tri.C;

			// Filter out triangles which will cause physics system to emit degenerate-geometry warnings.
			// These checks reproduce tests in Chaos::CleanTrimesh
			const FVector3f& A = CollisionData->Vertices[Triangle.v0];
			const FVector3f& B = CollisionData->Vertices[Triangle.v1];
			const FVector3f& C = CollisionData->Vertices[Triangle.v2];
			if (A == B || A == C || B == C)
			{
				continue;
			}
			// anything that fails the first check should also fail this, but Chaos does both so doing the same here...
			const float SquaredArea = FVector3f::CrossProduct(A - B, A - C).SizeSquared();
			if (SquaredArea < UE_SMALL_NUMBER)
			{
				continue;
			}

			CollisionData->Indices.Add(Triangle);

			int32 MaterialID = MaterialAttrib ? MaterialAttrib->GetValue(tid) : 0;
			CollisionData->MaterialIndices.Add(MaterialID);
		}

		CollisionData->bFlipNormals = true;
		CollisionData->bDeformableMesh = true;
		CollisionData->bFastCook = true;
	});

	return true;
}

bool UDynamicMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	return bEnableComplexCollision && ((MeshObject != nullptr) ? (MeshObject->GetTriangleCount() > 0) : false);
}

bool UDynamicMeshComponent::WantsNegXTriMesh()
{
	return true;
}

UBodySetup* UDynamicMeshComponent::CreateBodySetupHelper()
{
	UBodySetup* NewBodySetup = nullptr;
	{
		FGCScopeGuard Scope;

		// Below flags are copied from UProceduralMeshComponent::CreateBodySetupHelper(). Without these flags, DynamicMeshComponents inside
		// a DynamicMeshActor BP will result on a GLEO error after loading and modifying a saved Level (but *not* on the initial save)
		// The UBodySetup in a template needs to be public since the property is Instanced and thus is the archetype of the instance meaning there is a direct reference
		NewBodySetup = NewObject<UBodySetup>(this, NAME_None, (IsTemplate() ? RF_Public | RF_ArchetypeObject : RF_NoFlags));
	}
	NewBodySetup->BodySetupGuid = FGuid::NewGuid();

	NewBodySetup->bGenerateMirroredCollision = false;
	NewBodySetup->CollisionTraceFlag = this->CollisionType;

	NewBodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	NewBodySetup->bSupportUVsAndFaceRemap = false; /* bSupportPhysicalMaterialMasks; */

	return NewBodySetup;
}

UBodySetup* UDynamicMeshComponent::GetBodySetup()
{
	if (MeshBodySetup == nullptr)
	{
		UBodySetup* NewBodySetup = CreateBodySetupHelper();
		
		SetBodySetup(NewBodySetup);
	}

	return MeshBodySetup;
}

void UDynamicMeshComponent::SetBodySetup(UBodySetup* NewSetup)
{
	if (ensure(NewSetup))
	{
		MeshBodySetup = NewSetup;
	}
}

void UDynamicMeshComponent::SetSimpleCollisionShapes(const struct FKAggregateGeom& AggGeomIn, bool bUpdateCollision)
{
	AggGeom = AggGeomIn;
	if (bUpdateCollision)
	{
		UpdateCollision(false);
	}
}

void UDynamicMeshComponent::ClearSimpleCollisionShapes(bool bUpdateCollision)
{
	AggGeom.EmptyElements();
	if (bUpdateCollision)
	{
		UpdateCollision(false);
	}
}

void UDynamicMeshComponent::InvalidatePhysicsData()
{
	if (GetBodySetup())
	{
		GetBodySetup()->InvalidatePhysicsData();
		bCollisionUpdatePending = true;
	}
}

void UDynamicMeshComponent::RebuildPhysicsData()
{
	UWorld* World = GetWorld();
	const bool bUseAsyncCook = World && World->IsGameWorld() && bUseAsyncCooking;

	UBodySetup* BodySetup = nullptr;
	if (bUseAsyncCook)
	{
		// Abort all previous ones still standing
		for (UBodySetup* OldBody : AsyncBodySetupQueue)
		{
			OldBody->AbortPhysicsMeshAsyncCreation();
		}

		BodySetup = CreateBodySetupHelper();
		if (BodySetup)
		{
			AsyncBodySetupQueue.Add(BodySetup);
		}
	}
	else
	{
		AsyncBodySetupQueue.Empty();	// If for some reason we modified the async at runtime, just clear any pending async body setups
		BodySetup = GetBodySetup();
	}

	if (!BodySetup)
	{
		return;
	}

	BodySetup->CollisionTraceFlag = this->CollisionType;
	// Note: Directly assigning AggGeom wouldn't do some important-looking cleanup (clearing pointers on convex elements)
	//  so we RemoveSimpleCollision then AddCollisionFrom instead
	BodySetup->RemoveSimpleCollision();
	BodySetup->AddCollisionFrom(this->AggGeom);

	if (bUseAsyncCook)
	{
		BodySetup->CreatePhysicsMeshesAsync(FOnAsyncPhysicsCookFinished::CreateUObject(this, &UDynamicMeshComponent::FinishPhysicsAsyncCook, BodySetup));
	}
	else
	{
		// New GUID as collision has changed
		BodySetup->BodySetupGuid = FGuid::NewGuid();
		// Also we want cooked data for this
		BodySetup->bHasCookedCollisionData = true;
		BodySetup->InvalidatePhysicsData();
		BodySetup->CreatePhysicsMeshes();
		RecreatePhysicsState();

		bCollisionUpdatePending = false;
	}
}

void UDynamicMeshComponent::FinishPhysicsAsyncCook(bool bSuccess, UBodySetup* FinishedBodySetup)
{
	TArray<UBodySetup*> NewQueue;
	NewQueue.Reserve(AsyncBodySetupQueue.Num());

	int32 FoundIdx;
	if (AsyncBodySetupQueue.Find(FinishedBodySetup, FoundIdx))
	{
		// Note: currently no-cook-needed is reported identically to cook failed.
		// Checking AggGeom.GetElemCount() here is a hack to distinguish the no-cook-needed case
		// TODO: remove this hack to distinguish the no-cook-needed case when/if that is no longer identical to the cook failed case
		if (bSuccess || FinishedBodySetup->AggGeom.GetElementCount() > 0)
		{
			// The new body was found in the array meaning it's newer, so use it
			MeshBodySetup = FinishedBodySetup;
			RecreatePhysicsState();

			// remove any async body setups that were requested before this one
			for (int32 AsyncIdx = FoundIdx + 1; AsyncIdx < AsyncBodySetupQueue.Num(); ++AsyncIdx)
			{
				NewQueue.Add(AsyncBodySetupQueue[AsyncIdx]);
			}

			AsyncBodySetupQueue = NewQueue;
		}
		else
		{
			AsyncBodySetupQueue.RemoveAt(FoundIdx);
		}
	}
}

void UDynamicMeshComponent::UpdateCollision(bool bOnlyIfPending)
{
	if (bOnlyIfPending == false || bCollisionUpdatePending)
	{
		RebuildPhysicsData();
	}
}

void UDynamicMeshComponent::BeginDestroy()
{
	Super::BeginDestroy();

	AggGeom.FreeRenderInfo();
}

void UDynamicMeshComponent::EnableComplexAsSimpleCollision()
{
	SetComplexAsSimpleCollisionEnabled(true, true);
}

void UDynamicMeshComponent::SetComplexAsSimpleCollisionEnabled(bool bEnabled, bool bImmediateUpdate)
{
	bool bModified = false;
	if (bEnabled)
	{
		if (bEnableComplexCollision == false)
		{
			bEnableComplexCollision = true;
			bModified = true;
		}
		if (CollisionType != ECollisionTraceFlag::CTF_UseComplexAsSimple)
		{
			CollisionType = ECollisionTraceFlag::CTF_UseComplexAsSimple;
			bModified = true;
		}
	}
	else
	{
		if (bEnableComplexCollision == true)
		{
			bEnableComplexCollision = false;
			bModified = true;
		}
		if (CollisionType == ECollisionTraceFlag::CTF_UseComplexAsSimple)
		{
			CollisionType = ECollisionTraceFlag::CTF_UseDefault;
			bModified = true;
		}
	}
	if (bModified)
	{
		InvalidatePhysicsData();
	}
	if (bImmediateUpdate)
	{
		UpdateCollision(true);
	}
}


void UDynamicMeshComponent::SetDeferredCollisionUpdatesEnabled(bool bEnabled, bool bImmediateUpdate)
{
	if (bDeferCollisionUpdates != bEnabled)
	{
		bDeferCollisionUpdates = bEnabled;
		if (bEnabled == false && bImmediateUpdate)
		{
			UpdateCollision(true);
		}
	}
}

void UDynamicMeshComponent::SetTransientDeferCollisionUpdates(bool bEnabled)
{
	bTransientDeferCollisionUpdates = bEnabled;
}

void UDynamicMeshComponent::SetSceneProxyVerifyUsedMaterials(bool bState)
{
	bProxyVerifyUsedMaterials = bState;
	if (FDynamicMeshSceneProxy* Proxy = GetCurrentSceneProxy())
	{
		Proxy->SetVerifyUsedMaterials(bState);
	}
}


