// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/SplineMeshComponent.h"
#include "BodySetupEnums.h"
#include "Modules/ModuleManager.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "MeshDrawShaderBindings.h"
#include "SplineMeshSceneProxy.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollisionBase.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshMaterialShader.h"
#include "PhysicsEngine/SphylElem.h"
#include "RenderUtils.h"
#include "SceneInterface.h"
#include "UObject/UnrealType.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"

#if WITH_EDITOR
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "LandscapeSplineActor.h"
#endif // WITH_EDITOR

int32 GNoRecreateSplineMeshProxy = 1;
static FAutoConsoleVariableRef CVarNoRecreateSplineMeshProxy(
	TEXT("r.SplineMesh.NoRecreateProxy"),
	GNoRecreateSplineMeshProxy,
	TEXT("Optimization. If true, spline mesh proxies will not be recreated every time they are changed. They are simply updated."));

FSplineMeshInstanceData::FSplineMeshInstanceData(const USplineMeshComponent* SourceComponent)
	: FStaticMeshComponentInstanceData(SourceComponent)
{
	StartPos = SourceComponent->SplineParams.StartPos;
	EndPos = SourceComponent->SplineParams.EndPos;
	StartTangent = SourceComponent->SplineParams.StartTangent;
	EndTangent = SourceComponent->SplineParams.EndTangent;
}

//////////////////////////////////////////////////////////////////////////
// FSplineMeshVertexFactoryShaderParameters

void FSplineMeshVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	SplineMeshParams.Bind(ParameterMap, TEXT("SplineParams"), SPF_Optional);
}

void FSplineMeshVertexFactoryShaderParameters::GetElementShaderBindings(
	const class FSceneInterface* Scene,
	const FSceneView* View,
	const class FMeshMaterialShader* Shader,
	const EVertexInputStreamType InputStreamType,
	ERHIFeatureLevel::Type FeatureLevel,
	const FVertexFactory* VertexFactory,
	const FMeshBatchElement& BatchElement,
	class FMeshDrawSingleShaderBindings& ShaderBindings,
	FVertexInputStreamArray& VertexStreams
) const
{
	const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);

		if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
		{
			LocalVertexFactory->GetColorOverrideStream(OverrideColorVertexBuffer, VertexStreams);
		}
	}
	if (LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel) || UseGPUScene(GMaxRHIShaderPlatform, FeatureLevel))
	{
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), LocalVertexFactory->GetUniformBuffer());
	}

	checkSlow(BatchElement.bIsSplineProxy);
	FSplineMeshSceneProxy* SplineProxy = BatchElement.SplineMeshSceneProxy;
	FSplineMeshParams& SplineParams = SplineProxy->SplineParams;

	FVector4f ParamData[10];
	// LWC_TODO: Precision loss
	ParamData[0] = FVector4f(FVector3f(SplineParams.StartPos), SplineParams.StartRoll);
	ParamData[1] = FVector4f(FVector3f(SplineParams.StartTangent), SplineParams.EndRoll);
	ParamData[2] = FVector4f(FVector2f(SplineParams.StartScale), FVector2f(SplineParams.StartOffset));
	ParamData[3] = FVector4f(FVector3f(SplineParams.EndPos), (float)(int32)SplineProxy->bSmoothInterpRollScale);
	ParamData[4] = FVector4f(FVector3f(SplineParams.EndTangent), SplineProxy->SplineMeshMinZ);
	ParamData[5] = FVector4f(FVector2f(SplineParams.EndScale), FVector2f(SplineParams.EndOffset));
	ParamData[6] = FVector4f(FVector3f(SplineProxy->SplineUpDir), SplineProxy->SplineMeshScaleZ);

		FVector3f DirMask(0, 0, 0);
	DirMask = FVector3f::ZeroVector;
		DirMask[SplineProxy->ForwardAxis] = 1;
	ParamData[7] = FVector4f(DirMask, 0);
		DirMask = FVector3f::ZeroVector;
		DirMask[(SplineProxy->ForwardAxis + 1) % 3] = 1;
	ParamData[8] = FVector4f(DirMask, 0);
		DirMask = FVector3f::ZeroVector;
		DirMask[(SplineProxy->ForwardAxis + 2) % 3] = 1;
	ParamData[9] = FVector4f(DirMask, 0);

	ShaderBindings.Add(SplineMeshParams, ParamData);
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshVertexFactory

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSplineMeshVertexFactory, SF_Vertex     , FSplineMeshVertexFactoryShaderParameters);
#if RHI_RAYTRACING
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSplineMeshVertexFactory, SF_RayHitGroup, FSplineMeshVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FSplineMeshVertexFactory, SF_Compute    , FSplineMeshVertexFactoryShaderParameters);
#endif

IMPLEMENT_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory, "/Engine/Private/LocalVertexFactory.ush",
	  EVertexFactoryFlags::UsedWithMaterials
	| EVertexFactoryFlags::SupportsStaticLighting
	| EVertexFactoryFlags::SupportsDynamicLighting
	| EVertexFactoryFlags::SupportsPrecisePrevWorldPos
	| EVertexFactoryFlags::SupportsPositionOnly
	| EVertexFactoryFlags::SupportsPrimitiveIdStream
	| EVertexFactoryFlags::SupportsPSOPrecaching
	| EVertexFactoryFlags::SupportsRayTracing
	| EVertexFactoryFlags::SupportsRayTracingDynamicGeometry
	| EVertexFactoryFlags::SupportsManualVertexFetch
);

void InitSplineMeshVertexFactoryComponents(
	FStaticMeshVertexBuffers& VertexBuffers, 
	FSplineMeshVertexFactory* VertexFactory, 
	int32 LightMapCoordinateIndex, 
	bool bOverrideColorVertexBuffer,
	FLocalVertexFactory::FDataType& OutData)
{
	VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, OutData);
	VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, OutData);
	VertexBuffers.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, OutData);
	VertexBuffers.StaticMeshVertexBuffer.BindLightMapVertexBuffer(VertexFactory, OutData, LightMapCoordinateIndex);
	if (bOverrideColorVertexBuffer)
	{
		FColorVertexBuffer::BindDefaultColorVertexBuffer(VertexFactory, OutData, FColorVertexBuffer::NullBindStride::FColorSizeForComponentOverride);
	}
	else
	{
		VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(VertexFactory, OutData);
	}
}

//////////////////////////////////////////////////////////////////////////
// SplineMeshSceneProxy

void FSplineMeshSceneProxy::InitVertexFactory(USplineMeshComponent* InComponent, int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer)
{
	if (InComponent == nullptr || InComponent->GetStaticMesh() == nullptr)
	{
		return;
	}

	FStaticMeshLODResources* RenderData2 = &InComponent->GetStaticMesh()->GetRenderData()->LODResources[InLODIndex];
	FStaticMeshVertexFactories* VertexFactories = &InComponent->GetStaticMesh()->GetRenderData()->LODVertexFactories[InLODIndex];

	// Skip LODs that have their render data stripped (eg. platform MinLod settings)
	if (RenderData2->VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		return;
	}

	UStaticMesh* Parent = InComponent->GetStaticMesh();
	bool bOverrideColorVertexBuffer = !!InOverrideColorVertexBuffer;
	ERHIFeatureLevel::Type FeatureLevel = GetScene().GetFeatureLevel();

	if ((VertexFactories->SplineVertexFactory && !bOverrideColorVertexBuffer) || (VertexFactories->SplineVertexFactoryOverrideColorVertexBuffer && bOverrideColorVertexBuffer))
	{
		// we already have it
		return;
	}

	FSplineMeshVertexFactory* VertexFactory = new FSplineMeshVertexFactory(FeatureLevel);
	if (bOverrideColorVertexBuffer)
	{
		VertexFactories->SplineVertexFactoryOverrideColorVertexBuffer = VertexFactory;
	}
	else
	{
		VertexFactories->SplineVertexFactory = VertexFactory;
	}

	int32 LightMapCoordinateIndex = Parent->GetLightMapCoordinateIndex();
	// Initialize the static mesh's vertex factory.
	ENQUEUE_RENDER_COMMAND(InitSplineMeshVertexFactory)(
		[VertexFactory, RenderData2, bOverrideColorVertexBuffer, LightMapCoordinateIndex](FRHICommandListImmediate& RHICmdList)
	{
		FLocalVertexFactory::FDataType Data;
		InitSplineMeshVertexFactoryComponents(RenderData2->VertexBuffers, VertexFactory, LightMapCoordinateIndex, bOverrideColorVertexBuffer, Data);
		VertexFactory->SetData(Data);
		VertexFactory->InitResource();
	});
}


//////////////////////////////////////////////////////////////////////////
// SplineMeshComponent

USplineMeshComponent::USplineMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Mobility = EComponentMobility::Static;

	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	bAllowSplineEditingPerInstance = false;
	bSmoothInterpRollScale = false;
	bNeverNeedsCookedCollisionData = false;
	bHasCustomNavigableGeometry = EHasCustomNavigableGeometry::Yes;

	SplineUpDir.Z = 1.0f;

	// Default to useful length and scale
	SplineParams.StartTangent = FVector(100.f, 0.f, 0.f);
	SplineParams.StartScale = FVector2D(1.f, 1.f);

	SplineParams.EndPos = FVector(100.f, 0.f, 0.f);
	SplineParams.EndTangent = FVector(100.f, 0.f, 0.f);
	SplineParams.EndScale = FVector2D(1.f, 1.f);

	SplineBoundaryMin = 0;
	SplineBoundaryMax = 0;

	bMeshDirty = false;
}

FVector USplineMeshComponent::GetStartPosition() const
{
	return SplineParams.StartPos;
}

void USplineMeshComponent::SetStartPosition(FVector StartPos, bool bUpdateMesh)
{
	SplineParams.StartPos = StartPos;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetStartTangent() const
{
	return SplineParams.StartTangent;
}

void USplineMeshComponent::SetStartTangent(FVector StartTangent, bool bUpdateMesh)
{
	SplineParams.StartTangent = StartTangent;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetEndPosition() const
{
	return SplineParams.EndPos;
}

void USplineMeshComponent::SetEndPosition(FVector EndPos, bool bUpdateMesh)
{
	SplineParams.EndPos = EndPos;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetEndTangent() const
{
	return SplineParams.EndTangent;
}

void USplineMeshComponent::SetEndTangent(FVector EndTangent, bool bUpdateMesh)
{
	if (SplineParams.EndTangent == EndTangent)
	{
		return;
	}

	SplineParams.EndTangent = EndTangent;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

void USplineMeshComponent::SetStartAndEnd(FVector StartPos, FVector StartTangent, FVector EndPos, FVector EndTangent, bool bUpdateMesh)
{
	if (SplineParams.StartPos == StartPos && SplineParams.StartTangent == StartTangent && SplineParams.EndPos == EndPos && SplineParams.EndTangent == EndTangent)
	{
		return;
	}

	SplineParams.StartPos = StartPos;
	SplineParams.StartTangent = StartTangent;
	SplineParams.EndPos = EndPos;
	SetEndTangent(EndTangent, false);
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetStartScale() const
{
	return SplineParams.StartScale;
}

void USplineMeshComponent::SetStartScale(FVector2D StartScale, bool bUpdateMesh)
{
	if (SplineParams.StartScale == StartScale)
	{
		return;
	}

	SplineParams.StartScale = StartScale;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetStartRoll() const
{
	return SplineParams.StartRoll;
}

void USplineMeshComponent::SetStartRoll(float StartRoll, bool bUpdateMesh)
{
	SplineParams.StartRoll = StartRoll;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetStartOffset() const
{
	return SplineParams.StartOffset;
}

void USplineMeshComponent::SetStartOffset(FVector2D StartOffset, bool bUpdateMesh)
{
	SplineParams.StartOffset = StartOffset;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetEndScale() const
{
	return SplineParams.EndScale;
}

void USplineMeshComponent::SetEndScale(FVector2D EndScale, bool bUpdateMesh)
{
	if (SplineParams.EndScale == EndScale)
	{
		return;
	}

	SplineParams.EndScale = EndScale;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetEndRoll() const
{
	return SplineParams.EndRoll;
}

void USplineMeshComponent::SetEndRoll(float EndRoll, bool bUpdateMesh)
{
	SplineParams.EndRoll = EndRoll;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector2D USplineMeshComponent::GetEndOffset() const
{
	return SplineParams.EndOffset;
}

void USplineMeshComponent::SetEndOffset(FVector2D EndOffset, bool bUpdateMesh)
{
	SplineParams.EndOffset = EndOffset;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

ESplineMeshAxis::Type USplineMeshComponent::GetForwardAxis() const
{
	return ForwardAxis;
}

void USplineMeshComponent::SetForwardAxis(ESplineMeshAxis::Type InForwardAxis, bool bUpdateMesh)
{
	if (ForwardAxis == InForwardAxis)
	{
		return;
	}

	ForwardAxis = InForwardAxis;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

FVector USplineMeshComponent::GetSplineUpDir() const
{
	return SplineUpDir;
}

void USplineMeshComponent::SetSplineUpDir(const FVector& InSplineUpDir, bool bUpdateMesh)
{
	SplineUpDir = InSplineUpDir.GetSafeNormal();
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetBoundaryMin() const
{
	return SplineBoundaryMin;
}

void USplineMeshComponent::SetBoundaryMin(float InBoundaryMin, bool bUpdateMesh)
{
	SplineBoundaryMin = InBoundaryMin;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

float USplineMeshComponent::GetBoundaryMax() const
{
	return SplineBoundaryMax;
}

void USplineMeshComponent::SetBoundaryMax(float InBoundaryMax, bool bUpdateMesh)
{
	SplineBoundaryMax = InBoundaryMax;
	bMeshDirty = true;
	if (bUpdateMesh)
	{
		UpdateRenderStateAndCollision();
	}
}

void USplineMeshComponent::SetbNeverNeedsCookedCollisionData(bool bInValue)
{
	bNeverNeedsCookedCollisionData = bInValue;
	if (BodySetup != nullptr)
	{
		BodySetup->bNeverNeedsCookedCollisionData = bInValue;
	}
}

void USplineMeshComponent::UpdateMesh()
{
	if (bMeshDirty)
	{
		UpdateRenderStateAndCollision();
	}
}

void USplineMeshComponent::UpdateMesh_Concurrent()
{
	if (bMeshDirty)
	{
		UpdateRenderStateAndCollision_Internal(true);
	}
}

void USplineMeshComponent::CalculateScaleZAndMinZ(float& OutScaleZ, float& OutMinZ) const
{
	if (GetStaticMesh())
	{
		if (FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax))
		{
			FBoxSphereBounds StaticMeshBounds = GetStaticMesh()->GetBounds();
			OutScaleZ = 0.5f / USplineMeshComponent::GetAxisValueRef(StaticMeshBounds.BoxExtent, ForwardAxis); // 1/(2 * Extent)
			OutMinZ = USplineMeshComponent::GetAxisValueRef(StaticMeshBounds.Origin, ForwardAxis) * OutScaleZ - 0.5f;
		}
		else
		{
			OutScaleZ = 1.0f / (SplineBoundaryMax - SplineBoundaryMin);
			OutMinZ = SplineBoundaryMin * OutScaleZ;
		}
	}
}

void USplineMeshComponent::UpdateRenderStateAndCollision()
{
	UpdateRenderStateAndCollision_Internal(false);
}

void USplineMeshComponent::UpdateRenderStateAndCollision_Internal(bool bConcurrent)
{
	if (GNoRecreateSplineMeshProxy && bRenderStateCreated && SceneProxy)
	{
		if (bConcurrent)
		{
			SendRenderTransform_Concurrent();
		}
		else
		{
			MarkRenderTransformDirty();
		}

		FSplineMeshSceneProxy* SplineProxy = static_cast<FSplineMeshSceneProxy*>(SceneProxy);

		float SplineMeshScaleZ = 1.f;
		float SplineMeshMinZ = 1.f;
		CalculateScaleZAndMinZ(SplineMeshScaleZ, SplineMeshMinZ);

		ENQUEUE_RENDER_COMMAND(UpdateSplineParamsRTCommand)(
			[SplineProxy, this, SplineMeshScaleZ, SplineMeshMinZ](FRHICommandList&)
		{
			SplineProxy->SplineParams = SplineParams;
			SplineProxy->ForwardAxis = ForwardAxis;
			SplineProxy->SplineUpDir = SplineUpDir;
			SplineProxy->SplineMeshScaleZ = SplineMeshScaleZ;
			SplineProxy->SplineMeshMinZ = SplineMeshMinZ;
		});
	}
	else
	{
		if (bConcurrent)
		{
			RecreateRenderState_Concurrent();
		}
		else
		{
			MarkRenderStateDirty();
		}
	}

	CachedMeshBodySetupGuid.Invalidate();
	RecreatePhysicsState();

	bMeshDirty = false;
}

void USplineMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UEVer() < VER_UE4_SPLINE_MESH_ORIENTATION)
	{
		ForwardAxis = ESplineMeshAxis::Z;
		SplineParams.StartRoll -= UE_HALF_PI;
		SplineParams.EndRoll -= UE_HALF_PI;

		float Temp = SplineParams.StartOffset.X;
		SplineParams.StartOffset.X = -SplineParams.StartOffset.Y;
		SplineParams.StartOffset.Y = Temp;
		Temp = SplineParams.EndOffset.X;
		SplineParams.EndOffset.X = -SplineParams.EndOffset.Y;
		SplineParams.EndOffset.Y = Temp;
	}

#if WITH_EDITOR
	if (BodySetup != nullptr)
	{
		BodySetup->SetFlags(RF_Transactional);
	}
#endif
}

#if WITH_EDITOR
bool USplineMeshComponent::IsEditorOnly() const
{
	if (Super::IsEditorOnly())
	{
		return true;
	}

	// If Landscape uses generated LandscapeSplineMeshesActors, SplineMeshComponents is removed from cooked build  
	ALandscapeSplineActor* SplineActor = Cast<ALandscapeSplineActor>(GetOwner());
	if (SplineActor && SplineActor->HasGeneratedLandscapeSplineMeshesActors())
	{
		return true;
	}

	return false;
}

bool USplineMeshComponent::Modify(bool bAlwaysMarkDirty)
{
	bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	if (BodySetup != nullptr)
	{
		BodySetup->Modify(bAlwaysMarkDirty);
	}

	return bSavedToTransactionBuffer;
}
#endif

void USplineMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FComponentPSOPrecacheParamsList& OutParams)
{
	if (GetStaticMesh() == nullptr || GetStaticMesh()->GetRenderData() == nullptr)
	{
		return;
	}

	FVertexFactoryType* VertexFactoryType = &FSplineMeshVertexFactory::StaticType;
	bool bSupportsManualVertexFetch = VertexFactoryType->SupportsManualVertexFetch(GMaxRHIFeatureLevel);

	// Override color buffer is using same vertex element data as default one
	bool bOverrideColorVertexBuffer = false;
	int32 LightMapCoordinateIndex = GetStaticMesh()->GetLightMapCoordinateIndex();

	bool bAnySectionCastsShadows = false;
	FPSOPrecacheVertexFactoryDataPerMaterialIndexList VFTypesPerMaterialIndex;
	for (FStaticMeshLODResources& LODRenderData : GetStaticMesh()->GetRenderData()->LODResources)
	{
		FVertexDeclarationElementList VertexElements;
		if (!bSupportsManualVertexFetch)
		{
			FLocalVertexFactory::FDataType Data;
			InitSplineMeshVertexFactoryComponents(LODRenderData.VertexBuffers, nullptr /*VertexFactory*/, LightMapCoordinateIndex, bOverrideColorVertexBuffer, Data);
			FSplineMeshVertexFactory::GetVertexElements(GMaxRHIFeatureLevel, EVertexInputStreamType::Default, bSupportsManualVertexFetch, Data, VertexElements);						
		}

		for (FStaticMeshSection& RenderSection : LODRenderData.Sections)
		{
			bAnySectionCastsShadows |= RenderSection.bCastShadow;

			int16 MaterialIndex = RenderSection.MaterialIndex;
			FPSOPrecacheVertexFactoryDataPerMaterialIndex* VFsPerMaterial = VFTypesPerMaterialIndex.FindByPredicate(
				[MaterialIndex](const FPSOPrecacheVertexFactoryDataPerMaterialIndex& Other) { return Other.MaterialIndex == MaterialIndex; });
			if (VFsPerMaterial == nullptr)
			{
				VFsPerMaterial = &VFTypesPerMaterialIndex.AddDefaulted_GetRef();
				VFsPerMaterial->MaterialIndex = RenderSection.MaterialIndex;
			}

			if (bSupportsManualVertexFetch)
			{
				VFsPerMaterial->VertexFactoryDataList.AddUnique(FPSOPrecacheVertexFactoryData(VertexFactoryType));
			}
			else
			{	
				VFsPerMaterial->VertexFactoryDataList.AddUnique(FPSOPrecacheVertexFactoryData(VertexFactoryType, VertexElements));
			}			
		}
	}

	FPSOPrecacheParams PrecachePSOParams = BasePrecachePSOParams;
	PrecachePSOParams.bCastShadow = bAnySectionCastsShadows;
	PrecachePSOParams.bReverseCulling = bReverseCulling;

	for (FPSOPrecacheVertexFactoryDataPerMaterialIndex& VFsPerMaterial : VFTypesPerMaterialIndex)
	{
		UMaterialInterface* MaterialInterface = GetMaterial(VFsPerMaterial.MaterialIndex);
		if (MaterialInterface == nullptr)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}

		FComponentPSOPrecacheParams& ComponentParams = OutParams[OutParams.AddDefaulted()];
		ComponentParams.MaterialInterface = MaterialInterface;
		ComponentParams.VertexFactoryDataList = VFsPerMaterial.VertexFactoryDataList;
		ComponentParams.PSOPrecacheParams = PrecachePSOParams;
	}
}

FPrimitiveSceneProxy* USplineMeshComponent::CreateSceneProxy()
{
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid =
		// make sure we have an actual staticmesh
		GetStaticMesh() &&
		GetStaticMesh()->IsCompiling() == false &&
		GetStaticMesh()->HasValidRenderData() &&
		!IsPSOPrecaching();

	if (bMeshIsValid)
	{
		return ::new FSplineMeshSceneProxy(this);
	}
	else
	{
		return nullptr;
	}
}


/**
* Functions used for transforming a static mesh component based on a spline.
* This needs to be updated if the spline functionality changes!
*/
static float SmoothStep(float A, float B, float X)
{
	if (X < A)
	{
		return 0.0f;
	}
	else if (X >= B)
	{
		return 1.0f;
	}
	const float InterpFraction = (X - A) / (B - A);
	return InterpFraction * InterpFraction * (3.0f - 2.0f * InterpFraction);
}

static FVector SplineEvalPos(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const float A2 = A * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector SplineEvalDir(const FVector& StartPos, const FVector& StartTangent, const FVector& EndPos, const FVector& EndTangent, float A)
{
	const FVector C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector E = StartTangent;

	const float A2 = A * A;

	return ((C * A2) + (D * A) + E).GetSafeNormal();
}


FBoxSphereBounds USplineMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (!GetStaticMesh())
	{
		return FBoxSphereBounds(FBox(ForceInit));
	}

	float MinT = 0.0f;
	float MaxT = 1.0f;

	const FBoxSphereBounds MeshBounds = GetStaticMesh()->GetBounds();

	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
	if (bHasCustomBoundary)
	{
		// If there's a custom boundary, alter the min/max of the spline we need to evaluate
		const float MeshMin = GetAxisValueRef(MeshBounds.Origin - MeshBounds.BoxExtent, ForwardAxis);
		const float MeshMax = GetAxisValueRef(MeshBounds.Origin + MeshBounds.BoxExtent, ForwardAxis);

		const float MeshMinT = (MeshMin - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
		const float MeshMaxT = (MeshMax - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);

		// Disallow extrapolation beyond a certain value; enormous bounding boxes cause the render thread to crash
		const float MaxSplineExtrapolation = 4.0f;
		if (FMath::Abs(MeshMinT) < MaxSplineExtrapolation && FMath::Abs(MeshMaxT) < MaxSplineExtrapolation)
		{
			MinT = MeshMinT;
			MaxT = MeshMaxT;
		}
	}

	const FVector AxisMask = GetAxisMask(ForwardAxis);
	const FVector FlattenedMeshOrigin = MeshBounds.Origin * AxisMask;
	const FVector FlattenedMeshExtent = MeshBounds.BoxExtent * AxisMask;
	const FBox MeshBoundingBox = FBox(FlattenedMeshOrigin - FlattenedMeshExtent, FlattenedMeshOrigin + FlattenedMeshExtent);

	FBox BoundingBox(ForceInit);
	BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(MinT));
	BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(MaxT));

	// Work out coefficients of the cubic spline derivative equation dx/dt
	const FVector A = 6.0f * SplineParams.StartPos + 3.0f * SplineParams.StartTangent + 3.0f * SplineParams.EndTangent - 6.0f * SplineParams.EndPos;
	const FVector B = -6.0f * SplineParams.StartPos - 4.0f * SplineParams.StartTangent - 2.0f * SplineParams.EndTangent + 6.0f * SplineParams.EndPos;
	const FVector C = SplineParams.StartTangent;

	// Minima/maxima happen where dx/dt == 0, calculate t values
	const FVector Discriminant = B * B - 4.0f * A * C;

	// Work out minima/maxima component-by-component.
	// Negative discriminant means no solution; A == 0 implies coincident start/end points
	if (Discriminant.X > 0.0f && !FMath::IsNearlyZero(A.X))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.X);
		const float Denominator = 0.5f / A.X;
		const float T0 = (-B.X + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.X - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	if (Discriminant.Y > 0.0f && !FMath::IsNearlyZero(A.Y))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.Y);
		const float Denominator = 0.5f / A.Y;
		const float T0 = (-B.Y + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.Y - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	if (Discriminant.Z > 0.0f && !FMath::IsNearlyZero(A.Z))
	{
		const float SqrtDiscriminant = FMath::Sqrt(Discriminant.Z);
		const float Denominator = 0.5f / A.Z;
		const float T0 = (-B.Z + SqrtDiscriminant) * Denominator;
		const float T1 = (-B.Z - SqrtDiscriminant) * Denominator;

		if (T0 >= MinT && T0 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T0));
		}

		if (T1 >= MinT && T1 <= MaxT)
		{
			BoundingBox += MeshBoundingBox.TransformBy(CalcSliceTransformAtSplineOffset(T1));
		}
	}

	return FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld));
}

FTransform USplineMeshComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName != NAME_None)
	{
		UStaticMeshSocket const* const Socket = GetSocketByName(InSocketName);
		if (Socket)
		{
			FTransform SocketTransform;
			SocketTransform = FTransform(Socket->RelativeRotation, Socket->RelativeLocation * GetAxisMask(ForwardAxis), Socket->RelativeScale);
			SocketTransform = SocketTransform * CalcSliceTransform(GetAxisValueRef(Socket->RelativeLocation, ForwardAxis));

			switch (TransformSpace)
			{
			case RTS_World:
			{
				return SocketTransform * GetComponentToWorld();
			}
			case RTS_Actor:
			{
				if (const AActor* Actor = GetOwner())
				{
					return (SocketTransform * GetComponentToWorld()).GetRelativeTransform(GetOwner()->GetTransform());
				}
				break;
			}
			case RTS_Component:
			{
				return SocketTransform;
			}
			}
		}
	}

	return Super::GetSocketTransform(InSocketName, TransformSpace);
}


FTransform USplineMeshComponent::CalcSliceTransform(const float DistanceAlong) const
{
	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);

	// Find how far 'along' mesh we are
	float Alpha = 0.f;
	if (bHasCustomBoundary)
	{
		Alpha = (DistanceAlong - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
	}
	else if (GetStaticMesh())
	{
		const FBoxSphereBounds StaticMeshBounds = GetStaticMesh()->GetBounds();
		const float MeshMinZ = GetAxisValueRef(StaticMeshBounds.Origin, ForwardAxis) - GetAxisValueRef(StaticMeshBounds.BoxExtent, ForwardAxis);
		const float MeshRangeZ = 2.0f * GetAxisValueRef(StaticMeshBounds.BoxExtent, ForwardAxis);
		if (MeshRangeZ > UE_SMALL_NUMBER)
		{
			Alpha = (DistanceAlong - MeshMinZ) / MeshRangeZ;
		}
	}

	return CalcSliceTransformAtSplineOffset(Alpha);
}

FTransform USplineMeshComponent::CalcSliceTransformAtSplineOffset(const float Alpha) const
{
	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;

	// Then find the point and direction of the spline at this point along
	FVector SplinePos = SplineEvalPos(SplineParams.StartPos, SplineParams.StartTangent, SplineParams.EndPos, SplineParams.EndTangent, Alpha);
	const FVector SplineDir = SplineEvalDir(SplineParams.StartPos, SplineParams.StartTangent, SplineParams.EndPos, SplineParams.EndTangent, Alpha);

	// Find base frenet frame
	const FVector BaseXVec = (SplineUpDir ^ SplineDir).GetSafeNormal();
	const FVector BaseYVec = (SplineDir ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2D SliceOffset = FMath::Lerp<FVector2D>(SplineParams.StartOffset, SplineParams.EndOffset, HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);

	// Find scale at this point along spline
	const FVector2D UseScale = FMath::Lerp(SplineParams.StartScale, SplineParams.EndScale, HermiteAlpha);

	// Build overall transform
	FTransform SliceTransform;
	switch (ForwardAxis)
	{
	case ESplineMeshAxis::X:
		SliceTransform = FTransform(SplineDir, XVec, YVec, SplinePos);
		SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
		break;
	case ESplineMeshAxis::Y:
		SliceTransform = FTransform(YVec, SplineDir, XVec, SplinePos);
		SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
		break;
	case ESplineMeshAxis::Z:
		SliceTransform = FTransform(XVec, YVec, SplineDir, SplinePos);
		SliceTransform.SetScale3D(FVector(UseScale.X, UseScale.Y, 1));
		break;
	default:
		check(0);
		break;
	}

	return SliceTransform;
}


bool USplineMeshComponent::GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData)
{
	if (GetStaticMesh())
	{
		GetStaticMesh()->GetPhysicsTriMeshData(CollisionData, InUseAllTriData);

		FVector3f Mask = FVector3f(1, 1, 1);
		GetAxisValueRef(Mask, ForwardAxis) = 0;

		for (FVector3f& CollisionVert : CollisionData->Vertices)
		{
			CollisionVert = (FVector3f)CalcSliceTransform(GetAxisValueRef(CollisionVert, ForwardAxis)).TransformPosition(FVector(CollisionVert * Mask));
		}

		CollisionData->bDeformableMesh = true;

		return true;
	}

	return false;
}

bool USplineMeshComponent::ContainsPhysicsTriMeshData(bool InUseAllTriData) const
{
	if (GetStaticMesh())
	{
		return GetStaticMesh()->ContainsPhysicsTriMeshData(InUseAllTriData);
	}

	return false;
}

bool USplineMeshComponent::GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const
{
	if (GetStaticMesh())
	{
		return GetStaticMesh()->GetTriMeshSizeEstimates(OutTriMeshEstimates, bInUseAllTriData);
	}

	return false;
}

void USplineMeshComponent::GetMeshId(FString& OutMeshId)
{
	// First get the base mesh id from the static mesh
	if (GetStaticMesh())
	{
		GetStaticMesh()->GetMeshId(OutMeshId);
	}

	// new method: Same guid as the base mesh but with a unique DDC-id based on the spline params.
	// This fixes the bug where running a blueprint construction script regenerates the guid and uses
	// a new DDC slot even if the mesh hasn't changed
	// If BodySetup is null that means we're *currently* duplicating one, and haven't transformed its data
	// to fit the spline yet, so just use the data from the base mesh by using a blank MeshId
	// It would be better if we could stop it building data in that case at all...

	if (BodySetup != nullptr && BodySetup->BodySetupGuid == CachedMeshBodySetupGuid)
	{
		TArray<uint8> TempBytes;
		TempBytes.Reserve(256);

		FMemoryWriter Ar(TempBytes);
		Ar << SplineParams.StartPos;
		Ar << SplineParams.StartTangent;
		Ar << SplineParams.StartScale;
		Ar << SplineParams.StartRoll;
		Ar << SplineParams.StartOffset;
		Ar << SplineParams.EndPos;
		Ar << SplineParams.EndTangent;
		Ar << SplineParams.EndScale;
		Ar << SplineParams.EndRoll;
		Ar << SplineParams.EndOffset;
		Ar << SplineUpDir;
		bool bSmoothInterp = bSmoothInterpRollScale;
		Ar << bSmoothInterp; // can't write a bitfield member into an archive
		Ar << ForwardAxis;
		Ar << SplineBoundaryMin;
		Ar << SplineBoundaryMax;

		// Now convert the raw bytes to a string.
		const uint8* SettingsAsBytes = TempBytes.GetData();
		OutMeshId.Reserve(OutMeshId.Len() + TempBytes.Num() + 1);
		for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
		{
			ByteToHex(SettingsAsBytes[ByteIndex], OutMeshId);
		}
	}
}

void USplineMeshComponent::OnCreatePhysicsState()
{
	// With editor code we can recreate the collision if the mesh changes
	const FGuid MeshBodySetupGuid = (GetStaticMesh() != nullptr ? GetStaticMesh()->GetBodySetup()->BodySetupGuid : FGuid());
	if (CachedMeshBodySetupGuid != MeshBodySetupGuid)
	{
		RecreateCollision();
	}

	return Super::OnCreatePhysicsState();
}

UBodySetup* USplineMeshComponent::GetBodySetup()
{
	// Don't return a body setup that has no collision, it means we are interactively moving the spline and don't want to build collision.
	// Instead we explicitly build collision with USplineMeshComponent::RecreateCollision()
	if (BodySetup != nullptr && (BodySetup->ChaosTriMeshes.Num() || BodySetup->AggGeom.GetElementCount() > 0))
	{
		return BodySetup;
	}

	return nullptr;
}

bool USplineMeshComponent::DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const
{
	// the NavCollision is supposed to be faster than exporting the regular collision,
	// but I'm not sure that's true here, as the regular collision is pre-distorted to the spline

	if (GetStaticMesh() != nullptr && GetStaticMesh()->GetNavCollision() != nullptr)
	{
		UNavCollisionBase* NavCollision = GetStaticMesh()->GetNavCollision();

		if (ensure(!NavCollision->IsDynamicObstacle()))
		{
			if (NavCollision->HasConvexGeometry())
			{
				FVector Mask = FVector(1, 1, 1);
				GetAxisValueRef(Mask, ForwardAxis) = 0;

				TArray<FVector> VertexBuffer;
				VertexBuffer.Reserve(FMath::Max(NavCollision->GetConvexCollision().VertexBuffer.Num(), NavCollision->GetTriMeshCollision().VertexBuffer.Num()));

				for (int32 i = 0; i < NavCollision->GetConvexCollision().VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->GetConvexCollision().VertexBuffer[i];
					Vertex = CalcSliceTransform(GetAxisValueRef(Vertex, ForwardAxis)).TransformPosition(Vertex * Mask);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->GetConvexCollision().IndexBuffer.GetData(), NavCollision->GetConvexCollision().IndexBuffer.Num(),
					GetComponentTransform());

				VertexBuffer.Reset();
				for (int32 i = 0; i < NavCollision->GetTriMeshCollision().VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->GetTriMeshCollision().VertexBuffer[i];
					Vertex = CalcSliceTransform(GetAxisValueRef(Vertex, ForwardAxis)).TransformPosition(Vertex * Mask);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->GetTriMeshCollision().IndexBuffer.GetData(), NavCollision->GetTriMeshCollision().IndexBuffer.Num(),
					GetComponentTransform());

				return false;
			}
		}
	}

	return true;
}

void USplineMeshComponent::DestroyBodySetup()
{
	if (BodySetup != nullptr)
	{
		BodySetup->MarkAsGarbage();
		BodySetup = nullptr;
#if WITH_EDITORONLY_DATA
		CachedMeshBodySetupGuid.Invalidate();
#endif
	}
}

void USplineMeshComponent::RecreateCollision()
{
	if (GetStaticMesh())
	{
		if (BodySetup == nullptr)
		{
			BodySetup = DuplicateObject<UBodySetup>(GetStaticMesh()->GetBodySetup(), this);
			BodySetup->SetFlags(RF_Transactional);
			BodySetup->InvalidatePhysicsData();
		}
		else
		{
			const bool bDirtyPackage = false;
			BodySetup->Modify(bDirtyPackage);
			BodySetup->InvalidatePhysicsData();
			BodySetup->CopyBodyPropertiesFrom(GetStaticMesh()->GetBodySetup());
			BodySetup->CollisionTraceFlag = GetStaticMesh()->GetBodySetup()->CollisionTraceFlag;
		}
		BodySetup->BodySetupGuid = GetStaticMesh()->GetBodySetup()->BodySetupGuid;
		CachedMeshBodySetupGuid = GetStaticMesh()->GetBodySetup()->BodySetupGuid;

		BodySetup->bNeverNeedsCookedCollisionData = bNeverNeedsCookedCollisionData;

		if (BodySetup->GetCollisionTraceFlag() == CTF_UseComplexAsSimple)
		{
			BodySetup->AggGeom.EmptyElements();
		}
		else
		{
			FVector Mask = FVector(1, 1, 1);
			GetAxisValueRef(Mask, ForwardAxis) = 0;

			// distortion of a sphere can't be done nicely, so we just transform the origin and size
			for (FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
			{
				const float Z = GetAxisValueRef(SphereElem.Center, ForwardAxis);
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphereElem.Center *= Mask;

				SphereElem.Radius *= SliceTransform.GetMaximumAxisScale();
				SphereElem.Center = SliceTransform.TransformPosition(SphereElem.Center);
			}

			// distortion of a sphyl can't be done nicely, so we just transform the origin and size
			for (FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
			{
				const float Z = GetAxisValueRef(SphylElem.Center, ForwardAxis);
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphylElem.Center *= Mask;

				FTransform TM = SphylElem.GetTransform();
				SphylElem.Length = (TM * SliceTransform).TransformVector(FVector(0, 0, SphylElem.Length)).Size();
				SphylElem.Radius *= SliceTransform.GetMaximumAxisScale();

				SphylElem.SetTransform(TM * SliceTransform);
			}

			// Convert boxes to convex hulls to better respect distortion
			for (FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
			{
				FKConvexElem& ConvexElem = *new(BodySetup->AggGeom.ConvexElems) FKConvexElem();

				const FVector Radii = FVector(BoxElem.X / 2, BoxElem.Y / 2, BoxElem.Z / 2).ComponentMax(FVector(1.0f));
				const FTransform ElementTM = BoxElem.GetTransform();
				ConvexElem.VertexData.Empty(8);
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, -1, -1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, -1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1, -1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(-1, 1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, -1, -1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, -1, 1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, 1, -1)));
				ConvexElem.VertexData.Add(ElementTM.TransformPosition(Radii * FVector(1, 1, 1)));

				ConvexElem.UpdateElemBox();
			}
			BodySetup->AggGeom.BoxElems.Empty();

			// transform the points of the convex hulls into spline space
			for (FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
			{
				FTransform TM = ConvexElem.GetTransform();
				for (FVector& Point : ConvexElem.VertexData)
				{
					// pretransform the point by its local transform so we are working in untransformed local space
					FVector TransformedPoint = TM.TransformPosition(Point);
					// apply the transform to spline space
					Point = CalcSliceTransform(GetAxisValueRef(TransformedPoint, ForwardAxis)).TransformPosition(TransformedPoint * Mask);
				}

				// Set the local transform as an identity as points have already been transformed
				ConvexElem.SetTransform(FTransform::Identity);
				ConvexElem.UpdateElemBox();
			}
		}

		BodySetup->CreatePhysicsMeshes();
	}
	else
	{
		DestroyBodySetup();
	}
}

TStructOnScope<FActorComponentInstanceData> USplineMeshComponent::GetComponentInstanceData() const
{
	TStructOnScope<FActorComponentInstanceData> InstanceData;
	if (bAllowSplineEditingPerInstance)
	{
		InstanceData = MakeStructOnScope<FActorComponentInstanceData, FSplineMeshInstanceData>(this);
	}
	else
	{
		InstanceData = Super::GetComponentInstanceData();
	}
	return InstanceData;
}

void USplineMeshComponent::ApplyComponentInstanceData(FSplineMeshInstanceData* SplineMeshInstanceData)
{
	if (SplineMeshInstanceData)
	{
		if (bAllowSplineEditingPerInstance)
		{
			SplineParams.StartPos = SplineMeshInstanceData->StartPos;
			SplineParams.EndPos = SplineMeshInstanceData->EndPos;
			SplineParams.StartTangent = SplineMeshInstanceData->StartTangent;
			SetEndTangent(SplineMeshInstanceData->EndTangent, false);
			UpdateRenderStateAndCollision();
		}
	}
}


#include "PhysicsEngine/SphereElem.h"
#include "StaticMeshLight.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SplineMeshComponent)

/** */
class FSplineStaticLightingMesh : public FStaticMeshStaticLightingMesh
{
public:

	FSplineStaticLightingMesh(const USplineMeshComponent* InPrimitive, int32 InLODIndex, const TArray<ULightComponent*>& InRelevantLights) :
		FStaticMeshStaticLightingMesh(InPrimitive, InLODIndex, InRelevantLights),
		SplineComponent(InPrimitive)
	{
	}

#if WITH_EDITOR
	virtual const struct FSplineMeshParams* GetSplineParameters() const
	{
		return &SplineComponent->SplineParams;
	}
#endif	//WITH_EDITOR

private:
	const USplineMeshComponent* SplineComponent;
};

FStaticMeshStaticLightingMesh* USplineMeshComponent::AllocateStaticLightingMesh(int32 LODIndex, const TArray<ULightComponent*>& InRelevantLights)
{
	return new FSplineStaticLightingMesh(this, LODIndex, InRelevantLights);
}


float USplineMeshComponent::GetTextureStreamingTransformScale() const
{
	FVector::FReal SplineDeformFactor = 1;

	if (GetStaticMesh())
	{
		// We do this by looking at the ratio between current bounds (including deformation) and undeformed (straight from staticmesh)
		const float MinExtent = 1.0f;
		FBoxSphereBounds UndeformedBounds = GetStaticMesh()->GetBounds().TransformBy(GetComponentTransform());
		if (UndeformedBounds.BoxExtent.X >= MinExtent)
		{
			SplineDeformFactor = FMath::Max(SplineDeformFactor, Bounds.BoxExtent.X / UndeformedBounds.BoxExtent.X);
		}
		if (UndeformedBounds.BoxExtent.Y >= MinExtent)
		{
			SplineDeformFactor = FMath::Max(SplineDeformFactor, Bounds.BoxExtent.Y / UndeformedBounds.BoxExtent.Y);
		}
		if (UndeformedBounds.BoxExtent.Z >= MinExtent)
		{
			SplineDeformFactor = FMath::Max(SplineDeformFactor, Bounds.BoxExtent.Z / UndeformedBounds.BoxExtent.Z);
		}
	}

	return SplineDeformFactor * Super::GetTextureStreamingTransformScale();
}

#if WITH_EDITOR
void USplineMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	bool bIsSplineParamsChange = MemberPropertyThatChanged && MemberPropertyThatChanged->GetNameCPP() == TEXT("SplineParams");
	if (bIsSplineParamsChange)
	{
		SetEndTangent(SplineParams.EndTangent, false);
	}

	UStaticMeshComponent::PostEditChangeProperty(PropertyChangedEvent);

	// If the spline params were changed the actual geometry is, so flag the owning HLOD cluster as dirty
	if (bIsSplineParamsChange)
	{
		IHierarchicalLODUtilitiesModule& Module = FModuleManager::LoadModuleChecked<IHierarchicalLODUtilitiesModule>("HierarchicalLODUtilities");
		IHierarchicalLODUtilities* Utilities = Module.GetUtilities();
		Utilities->HandleActorModified(GetOwner());
	}

	if (MemberPropertyThatChanged && (MemberPropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(USplineMeshComponent, bNeverNeedsCookedCollisionData)))
	{
		// TODO [jonathan.bard] : this is currently needed because Setter doesn't correctly do its job in the details panel but eventually this could be removed : 
		SetbNeverNeedsCookedCollisionData(bNeverNeedsCookedCollisionData);
	}
}
#endif

