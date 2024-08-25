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
#include "Materials/Material.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Engine/World.h"
#include "NaniteVertexFactory.h"

#if WITH_EDITOR
#include "IHierarchicalLODUtilities.h"
#include "HierarchicalLODUtilitiesModule.h"
#include "LandscapeSplineActor.h"
#endif // WITH_EDITOR

namespace UE::SplineMesh
{
	float RealToFloatChecked(const double Value)
	{
		ensureMsgf(Value >= TNumericLimits<float>::Lowest() && Value <= TNumericLimits<float>::Max(), TEXT("Value %f exceeds float limits"), Value);
		return static_cast<float>(Value);
	}
}

int32 GNoRecreateSplineMeshProxy = 1;
static FAutoConsoleVariableRef CVarNoRecreateSplineMeshProxy(
	TEXT("r.SplineMesh.NoRecreateProxy"),
	GNoRecreateSplineMeshProxy,
	TEXT("Optimization. If true, spline mesh proxies will not be recreated every time they are changed. They are simply updated."));

int32 GSplineMeshRenderNanite = 1;
static FAutoConsoleVariableRef CVarSplineMeshRenderNanite(
	TEXT("r.SplineMesh.RenderNanite"),
	GSplineMeshRenderNanite,
	TEXT("When true, allows spline meshes to render as Nanite when enabled on the mesh (otherwise uses fallback mesh)."),
	FConsoleVariableDelegate::CreateLambda(
		[] (IConsoleVariable* InVariable)
		{
			FGlobalComponentRecreateRenderStateContext Context;
		}
	)
);

static bool ShouldRenderNaniteSplineMeshes()
{
	return NaniteSplineMeshesSupported() && GSplineMeshRenderNanite != 0;
}

void PackSplineMeshParams(const FSplineMeshShaderParams& Params, const TArrayView<FVector4f>& Output)
{
	auto PackF16 = [](float Value, uint32 Shift = 0) -> uint32
	{
		return uint32(FFloat16(Value).Encoded) << Shift;
	};
	auto PackSNorm16 = [](float Value, uint32 Shift = 0) -> uint32
	{
		const float N = FMath::Clamp(Value, -1.0f, 1.0f) * 0.5f + 0.5f;
		return uint32(N * 65535.0f) << Shift;
	};

	static_assert(SPLINE_MESH_PARAMS_FLOAT4_SIZE == 7, "If you changed the packed size of FSplineMeshShaderParams, this function needs to be updated");
	check(Output.Num() >= SPLINE_MESH_PARAMS_FLOAT4_SIZE);

	Output[0]	= FVector4f(Params.StartPos, Params.EndTangent.X);
	Output[1]	= FVector4f(Params.EndPos, Params.EndTangent.Y);
	Output[2]	= FVector4f(Params.StartTangent, Params.EndTangent.Z);
	Output[3]	= FVector4f(Params.StartOffset, Params.EndOffset);

	Output[4].X	= FMath::AsFloat(PackF16(Params.StartRoll) | PackF16(Params.EndRoll, 16u));
	Output[4].Y	= FMath::AsFloat(PackF16(Params.StartScale.X) | PackF16(Params.StartScale.Y, 16u));
	Output[4].Z	= FMath::AsFloat(PackF16(Params.EndScale.X) | PackF16(Params.EndScale.Y, 16u));
	Output[4].W	= FMath::AsFloat((Params.TextureCoord.X & 0xFFFFu) | (Params.TextureCoord.Y << 16u));

	Output[5].X	= Params.MeshZScale;
	Output[5].Y	= Params.MeshZOffset;
	Output[5].Z	= FMath::AsFloat(PackF16(Params.MeshDeformScaleMinMax.X) | PackF16(Params.MeshDeformScaleMinMax.Y, 16u));
	Output[5].W = FMath::AsFloat(PackF16(Params.SplineDistToTexelScale) | PackF16(Params.SplineDistToTexelOffset, 16u));

	Output[6].X = FMath::AsFloat(PackSNorm16(Params.SplineUpDir.X) | PackSNorm16(Params.SplineUpDir.Y, 16u));
	Output[6].Y = FMath::AsFloat(PackSNorm16(Params.SplineUpDir.Z) |
								 PackF16(FMath::Max(0.0f, Params.NaniteClusterBoundsScale), 16u) |
								 (Params.bSmoothInterpRollScale ? (1 << 31u) : 0u));

	const FQuat4f MeshRot = FQuat4f(FMatrix44f(Params.MeshDir, Params.MeshX, Params.MeshY, FVector3f::ZeroVector));
	Output[6].Z = FMath::AsFloat(PackSNorm16(MeshRot.X) | PackSNorm16(MeshRot.Y, 16u));
	Output[6].W = FMath::AsFloat(PackSNorm16(MeshRot.Z) | PackSNorm16(MeshRot.W, 16u));
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

static FVector3f SplineEvalPos(const FVector3f& StartPos, const FVector3f& StartTangent, const FVector3f& EndPos, const FVector3f& EndTangent, float A)
{
	const float A2 = A * A;
	const float A3 = A2 * A;

	return (((2 * A3) - (3 * A2) + 1) * StartPos) + ((A3 - (2 * A2) + A) * StartTangent) + ((A3 - A2) * EndTangent) + (((-2 * A3) + (3 * A2)) * EndPos);
}

static FVector3f SplineEvalPos(const FSplineMeshParams& Params, float A)
{
	// TODO: these don't need to be doubles!
	const FVector3f StartPos = FVector3f(Params.StartPos);
	const FVector3f StartTangent = FVector3f(Params.StartTangent);
	const FVector3f EndPos = FVector3f(Params.EndPos);
	const FVector3f EndTangent = FVector3f(Params.EndTangent);

	return SplineEvalPos(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector3f SplineEvalTangent(const FVector3f& StartPos, const FVector3f& StartTangent, const FVector3f& EndPos, const FVector3f& EndTangent, const float A)
{
	const FVector3f C = (6 * StartPos) + (3 * StartTangent) + (3 * EndTangent) - (6 * EndPos);
	const FVector3f D = (-6 * StartPos) - (4 * StartTangent) - (2 * EndTangent) + (6 * EndPos);
	const FVector3f E = StartTangent;

	const float A2 = A * A;

	return (C * A2) + (D * A) + E;
}

static FVector3f SplineEvalTangent(const FSplineMeshParams& Params, const float A)
{
	// TODO: these don't need to be doubles!
	const FVector3f StartPos = FVector3f(Params.StartPos);
	const FVector3f StartTangent = FVector3f(Params.StartTangent);
	const FVector3f EndPos = FVector3f(Params.EndPos);
	const FVector3f EndTangent = FVector3f(Params.EndTangent);

	return SplineEvalTangent(StartPos, StartTangent, EndPos, EndTangent, A);
}

static FVector3f SplineEvalDir(const FSplineMeshParams& Params, const float A)
{
	return SplineEvalTangent(Params, A).GetSafeNormal();
}

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
	const EShaderPlatform ShaderPlatform = Scene ?
		Scene->GetShaderPlatform() :
		(View ? View->GetShaderPlatform() : GetFeatureLevelShaderPlatform(FeatureLevel));
	const bool bUseGPUScene = UseGPUScene(ShaderPlatform, FeatureLevel);
	const auto* LocalVertexFactory = static_cast<const FLocalVertexFactory*>(VertexFactory);
	
	if (BatchElement.bUserDataIsColorVertexBuffer)
	{
		const FColorVertexBuffer* OverrideColorVertexBuffer = (FColorVertexBuffer*)BatchElement.UserData;
		check(OverrideColorVertexBuffer);

		if (!LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel))
		{
			LocalVertexFactory->GetColorOverrideStream(OverrideColorVertexBuffer, VertexStreams);
		}
	}
	if (LocalVertexFactory->SupportsManualVertexFetch(FeatureLevel) || bUseGPUScene)
	{
		ShaderBindings.Add(Shader->GetUniformBufferParameter<FLocalVertexFactoryUniformShaderParameters>(), LocalVertexFactory->GetUniformBuffer());
	}

	// If we can't use GPU Scene instance data, we have to bind the params to the VS loosely
	// NOTE: Mobile GPU scene can't support loading the spline params from instance data in VS
	if (!bUseGPUScene || FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		checkSlow(BatchElement.bIsSplineProxy);
		const FSplineMeshSceneProxy* SplineProxy = BatchElement.SplineMeshSceneProxy;

		FVector4f ParamData[SPLINE_MESH_PARAMS_FLOAT4_SIZE];
		PackSplineMeshParams(SplineProxy->GetSplineMeshParams(), TArrayView<FVector4f>(ParamData, SPLINE_MESH_PARAMS_FLOAT4_SIZE));

		ShaderBindings.Add(SplineMeshParams, ParamData);
	}
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
	const FStaticMeshVertexBuffers& VertexBuffers, 
	const FSplineMeshVertexFactory* VertexFactory, 
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

void USplineMeshComponent::InitVertexFactory(int32 InLODIndex, FColorVertexBuffer* InOverrideColorVertexBuffer)
{
	UStaticMesh* Mesh = GetStaticMesh();
	if (Mesh == nullptr)
	{
		return;
	}

	FStaticMeshLODResources& LODRenderData = Mesh->GetRenderData()->LODResources[InLODIndex];
	FStaticMeshVertexFactories& VertexFactories = Mesh->GetRenderData()->LODVertexFactories[InLODIndex];

	// Skip LODs that have their render data stripped (eg. platform MinLod settings)
	if (LODRenderData.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() == 0)
	{
		return;
	}

	bool bOverrideColorVertexBuffer = !!InOverrideColorVertexBuffer;
	const ERHIFeatureLevel::Type FeatureLevel = GetWorld()->GetFeatureLevel();

	if ((VertexFactories.SplineVertexFactory && !bOverrideColorVertexBuffer) || (VertexFactories.SplineVertexFactoryOverrideColorVertexBuffer && bOverrideColorVertexBuffer))
	{
		// we already have it
		return;
	}

	FSplineMeshVertexFactory* VertexFactory = new FSplineMeshVertexFactory(FeatureLevel);
	if (bOverrideColorVertexBuffer)
	{
		VertexFactories.SplineVertexFactoryOverrideColorVertexBuffer = VertexFactory;
	}
	else
	{
		VertexFactories.SplineVertexFactory = VertexFactory;
	}

	int32 LightMapCoordinateIndex = Mesh->GetLightMapCoordinateIndex();
	// Initialize the static mesh's vertex factory.
	ENQUEUE_RENDER_COMMAND(InitSplineMeshVertexFactory)(
		[VertexFactory, &LODRenderData, bOverrideColorVertexBuffer, LightMapCoordinateIndex](FRHICommandListBase& RHICmdList)
	{
		FLocalVertexFactory::FDataType Data;
		InitSplineMeshVertexFactoryComponents(LODRenderData.VertexBuffers, VertexFactory, LightMapCoordinateIndex, bOverrideColorVertexBuffer, Data);
		VertexFactory->SetData(RHICmdList, Data);
		VertexFactory->InitResource(RHICmdList);
	});
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

void USplineMeshComponent::SetStartRollDegrees(float StartRollDegrees, bool bUpdateMesh)
{
	SetStartRoll(FMath::DegreesToRadians(StartRollDegrees), bUpdateMesh);
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

void USplineMeshComponent::SetEndRollDegrees(float EndRollDegrees, bool bUpdateMesh)
{
	SetEndRoll(FMath::DegreesToRadians(EndRollDegrees), bUpdateMesh);
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

FSplineMeshShaderParams USplineMeshComponent::CalculateShaderParams() const
{
	FSplineMeshShaderParams Output;

	Output.StartPos 				= FVector3f(SplineParams.StartPos);
	Output.EndPos 					= FVector3f(SplineParams.EndPos);
	Output.StartTangent 			= FVector3f(SplineParams.StartTangent);
	Output.EndTangent 				= FVector3f(SplineParams.EndTangent);
	Output.StartScale 				= FVector2f(SplineParams.StartScale);
	Output.EndScale 				= FVector2f(SplineParams.EndScale);
	Output.StartOffset 				= FVector2f(SplineParams.StartOffset);
	Output.EndOffset 				= FVector2f(SplineParams.EndOffset);
	Output.StartRoll 				= SplineParams.StartRoll;
	Output.EndRoll 					= SplineParams.EndRoll;
	Output.NaniteClusterBoundsScale	= SplineParams.NaniteClusterBoundsScale;
	Output.bSmoothInterpRollScale 	= bSmoothInterpRollScale;
	Output.SplineUpDir 				= FVector3f(SplineUpDir);
	Output.TextureCoord 			= FUintVector2(INDEX_NONE, INDEX_NONE); // either unused or assigned later

	const uint32 MeshXAxis = (ForwardAxis + 1) % 3;
	const uint32 MeshYAxis = (ForwardAxis + 2) % 3;
	Output.MeshDir = Output.MeshX = Output.MeshY = FVector3f::ZeroVector;
	Output.MeshDir[ForwardAxis] = 1.0f;
	Output.MeshX[MeshXAxis] = 1.0f;
	Output.MeshY[MeshYAxis] = 1.0f;

	Output.MeshZScale = 1.0f;
	Output.MeshZOffset = 0.0f;

	if (GetStaticMesh())
	{
		const FBoxSphereBounds StaticMeshBounds = GetStaticMesh()->GetBounds();
		const float BoundsXYRadius = FVector3f(StaticMeshBounds.BoxExtent).Dot((Output.MeshX + Output.MeshY).GetUnsafeNormal());

		const float MeshMinZ = UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(StaticMeshBounds.Origin - StaticMeshBounds.BoxExtent, ForwardAxis));
		const float MeshZLen = UE::SplineMesh::RealToFloatChecked(2 * GetAxisValueRef(StaticMeshBounds.BoxExtent, ForwardAxis));
		const float InvMeshZLen = (MeshZLen <= 0.0f) ? 1.0f : 1.0f / MeshZLen;
		constexpr float MeshTexelLen = float(SPLINE_MESH_TEXEL_WIDTH - 1);

		if (FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax))
		{
			Output.MeshZScale = InvMeshZLen;
			Output.MeshZOffset = -MeshMinZ * InvMeshZLen;
			Output.SplineDistToTexelScale = MeshTexelLen;
			Output.SplineDistToTexelOffset = 0.0f;
		}
		else
		{
			const float BoundaryLen = SplineBoundaryMax - SplineBoundaryMin;
			const float InvBoundaryLen = 1.0f / BoundaryLen;

			Output.MeshZScale = InvBoundaryLen;
			Output.MeshZOffset = -SplineBoundaryMin * InvBoundaryLen;
			Output.SplineDistToTexelScale = BoundaryLen * InvMeshZLen * MeshTexelLen;
			Output.SplineDistToTexelOffset = (SplineBoundaryMin - MeshMinZ) * InvMeshZLen * MeshTexelLen;
		}

		// Iteratively solve for an approximation of spline length
		float SplineLength = 0.0f;
		{
			static const uint32 NumIterations = 63; // 64 sampled points
			static const float IterStep = 1.0f / float(NumIterations);
			float A = 0.0f;
			FVector3f PrevPoint = SplineEvalPos(SplineParams, A);
			for (uint32 i = 0; i < NumIterations; ++i)
			{
				FVector3f Point = SplineEvalPos(SplineParams, A);
				SplineLength += (Point - PrevPoint).Length();
				PrevPoint = Point;
				A += IterStep;
			}
		}

		// Calculate an approximation of how much the mesh gets scaled in each local axis as a result of spline
		// deformation and take the smallest of the axes. This is important for LOD selection of Nanite spline
		// meshes.
		{
			// Estimate length added due to twisting as well
			const float XYRadius = BoundsXYRadius * FMath::Max(Output.StartScale.GetAbsMax(), Output.EndScale.GetAbsMax());
			const float TwistRadians = FMath::Abs(Output.StartRoll - Output.EndRoll);
			SplineLength += TwistRadians * XYRadius;

			// Take the mid-point scale in X/Y to balance out LOD selection in case either of them are extreme.
			auto AvgAbs = [](float A, float B) { return (FMath::Abs(A) + FMath::Abs(B)) * 0.5f; };
			const FVector3f DeformScale = FVector3f(
				SplineLength * Output.MeshZScale,
				AvgAbs(Output.StartScale.X, Output.EndScale.X),
				AvgAbs(Output.StartScale.Y, Output.EndScale.Y)
			);
			
			Output.MeshDeformScaleMinMax = FVector2f(DeformScale.GetMin(), DeformScale.GetMax());
		}
	}

	return Output;
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

		ENQUEUE_RENDER_COMMAND(UpdateSplineParamsRTCommand)(
			[SceneProxy=SceneProxy, Params=CalculateShaderParams()](FRHICommandList&)
			{
				UpdateSplineMeshParams_RenderThread(SceneProxy, Params);
			}
		);
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

		double Temp = SplineParams.StartOffset.X;
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
	const ALandscapeSplineActor* SplineActor = Cast<ALandscapeSplineActor>(GetOwner());
	if (SplineActor && SplineActor->HasGeneratedLandscapeSplineMeshesActors())
	{
		return true;
	}

	return false;
}

bool USplineMeshComponent::Modify(bool bAlwaysMarkDirty)
{
	const bool bSavedToTransactionBuffer = Super::Modify(bAlwaysMarkDirty);

	if (BodySetup != nullptr)
	{
		BodySetup->Modify(bAlwaysMarkDirty);
	}

	return bSavedToTransactionBuffer;
}
#endif

void USplineMeshComponent::CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FMaterialInterfacePSOPrecacheParamsList& OutParams)
{
	if (GetStaticMesh() == nullptr || GetStaticMesh()->GetRenderData() == nullptr)
	{
		return;
	}

	const FVertexFactoryType* VertexFactoryType = &FSplineMeshVertexFactory::StaticType;
	int32 LightMapCoordinateIndex = GetStaticMesh()->GetLightMapCoordinateIndex();

	auto SMC_GetElements = [LightMapCoordinateIndex](const FStaticMeshLODResources& LODRenderData, int32 LODIndex, bool bSupportsManualVertexFetch, FVertexDeclarationElementList& Elements)
	{
		// FIXME: This will miss when SM component overrides vertex colors and source StaticMesh does not have vertex colors
		constexpr bool bOverrideColorVertexBuffer = false;
		FLocalVertexFactory::FDataType Data;
		InitSplineMeshVertexFactoryComponents(LODRenderData.VertexBuffers, nullptr /*VertexFactory*/, LightMapCoordinateIndex, bOverrideColorVertexBuffer, Data);
		FLocalVertexFactory::GetVertexElements(GMaxRHIFeatureLevel, EVertexInputStreamType::Default, bSupportsManualVertexFetch, Data, Elements);
	};

	FPSOPrecacheParams SplineMeshPSOParams = BasePrecachePSOParams;
	SplineMeshPSOParams.bReverseCulling ^= (SplineParams.StartScale.X < 0) ^ (SplineParams.StartScale.Y < 0);

	if (ShouldCreateNaniteProxy())
	{
		if (NaniteLegacyMaterialsSupported())
		{
			CollectPSOPrecacheDataImpl(&Nanite::FVertexFactory::StaticType, SplineMeshPSOParams, SMC_GetElements, OutParams);
		}

		if (NaniteComputeMaterialsSupported())
		{
			CollectPSOPrecacheDataImpl(&FNaniteVertexFactory::StaticType, SplineMeshPSOParams, SMC_GetElements, OutParams);
		}
	}
	else
	{
		CollectPSOPrecacheDataImpl(VertexFactoryType, SplineMeshPSOParams, SMC_GetElements, OutParams);
	}
}

FPrimitiveSceneProxy* USplineMeshComponent::CreateStaticMeshSceneProxy(Nanite::FMaterialAudit& NaniteMaterials, bool bCreateNanite)
{
	LLM_SCOPE(ELLMTag::StaticMesh);

     if (CheckPSOPrecachingAndBoostPriority() && GetPSOPrecacheProxyCreationStrategy() == EPSOPrecacheProxyCreationStrategy::DelayUntilPSOPrecached)
	{
		return nullptr;
	}
	
	if (bCreateNanite && ShouldRenderNaniteSplineMeshes())
	{
		return ::new FNaniteSplineMeshSceneProxy(NaniteMaterials, this);
	}
	
	return ::new FSplineMeshSceneProxy(this);
}

FBoxSphereBounds USplineMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const UStaticMesh* Mesh = GetStaticMesh();
	if (Mesh == nullptr)
	{
		return FBox();
	}

	const FBox ComputedBounds = ComputeDistortedBounds(LocalToWorld, Mesh->GetBounds());
	return ComputedBounds;
}

void USplineMeshComponent::UpdateBounds()
{
	Super::UpdateBounds();

	CachedNavigationBounds = Bounds.GetBox();

	if (const UStaticMesh* Mesh = GetStaticMesh())
	{
		if (const UNavCollisionBase* NavCollision = Mesh->GetNavCollision())
		{
			// Match condition in DoCustomNavigableGeometryExport
			const FBox NavCollisionBounds = NavCollision->GetBounds();
			if (ensure(!NavCollision->IsDynamicObstacle())
				&& NavCollision->HasConvexGeometry()
				&& NavCollisionBounds.IsValid)
			{
				const FBoxSphereBounds NavCollisionBoxSphereBounds(NavCollisionBounds);
				CachedNavigationBounds = ComputeDistortedBounds(GetComponentTransform(), Mesh->GetBounds(), &NavCollisionBoxSphereBounds);
			}
		}
	}
}

float USplineMeshComponent::ComputeRatioAlongSpline(const float DistanceAlong) const 
{
	// Find how far 'along' mesh (or custom boundaries) we are
	float Alpha = 0.f;

	const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
	if (bHasCustomBoundary)
	{
		Alpha = (DistanceAlong - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
	}
	else if (GetStaticMesh())
	{
		const FBoxSphereBounds StaticMeshBounds = GetStaticMesh()->GetBounds();
		const double MeshMinZ = GetAxisValueRef(StaticMeshBounds.Origin, ForwardAxis) - GetAxisValueRef(StaticMeshBounds.BoxExtent, ForwardAxis);
		const double MeshRangeZ = 2 * GetAxisValueRef(StaticMeshBounds.BoxExtent, ForwardAxis);
		if (MeshRangeZ > UE_SMALL_NUMBER)
		{
			Alpha = UE::SplineMesh::RealToFloatChecked((DistanceAlong - MeshMinZ) / MeshRangeZ);
		}
	}
	return Alpha;
}

void USplineMeshComponent::ComputeVisualMeshSplineTRange(float& MinT, float& MaxT) const
{
    MinT = 0.0;
    MaxT = 1.0;

    const bool bHasCustomBoundary = !FMath::IsNearlyEqual(SplineBoundaryMin, SplineBoundaryMax);
    if (bHasCustomBoundary)
    {
        const FBoxSphereBounds& MeshBounds = GetStaticMesh()->GetBounds();
        // If there's a custom boundary, alter the min/max of the spline we need to evaluate
        const float BoundsMin = UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(MeshBounds.Origin - MeshBounds.BoxExtent, ForwardAxis));
        const float BoundsMax = UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(MeshBounds.Origin + MeshBounds.BoxExtent, ForwardAxis));
        const float BoundsMinT = (BoundsMin - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
        const float BoundsMaxT = (BoundsMax - SplineBoundaryMin) / (SplineBoundaryMax - SplineBoundaryMin);
        // Disallow extrapolation beyond a certain value; enormous bounding boxes cause the render thread to crash
        constexpr float MaxSplineExtrapolation = 4.0f;
        MinT = FMath::Max(-MaxSplineExtrapolation, BoundsMinT);
        MaxT = FMath::Min(BoundsMaxT, MaxSplineExtrapolation);
    }
}

FBox USplineMeshComponent::ComputeDistortedBounds(const FTransform& InLocalToWorld, const FBoxSphereBounds& InMeshBounds, const FBoxSphereBounds* InBoundsToDistort) const
{
	float MinT = 0.0f;
	float MaxT = 1.0f;
	ComputeVisualMeshSplineTRange(MinT, MaxT);
	const FBoxSphereBounds& BoundsToDistort = InBoundsToDistort ? *InBoundsToDistort : InMeshBounds;


	const FVector AxisMask = GetAxisMask(ForwardAxis);
	const FVector FlattenedBoundsOrigin = BoundsToDistort.Origin * AxisMask;
	const FVector FlattenedBoundsExtent = BoundsToDistort.BoxExtent * AxisMask;
	const FBox FlattenedBounds = FBox(FlattenedBoundsOrigin - FlattenedBoundsExtent, FlattenedBoundsOrigin + FlattenedBoundsExtent);

	FBox BoundingBox(ForceInit);
	BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(MinT, MinT, MaxT));
	BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(MaxT, MinT, MaxT));

	// Work out coefficients of the cubic spline derivative equation dx/dt
	const FVector A(6 * SplineParams.StartPos + 3 * SplineParams.StartTangent + 3 * SplineParams.EndTangent - 6 * SplineParams.EndPos);
	const FVector B(-6 * SplineParams.StartPos - 4 * SplineParams.StartTangent - 2 * SplineParams.EndTangent + 6 * SplineParams.EndPos);
	const FVector C(SplineParams.StartTangent);

	auto AppendAxisExtrema = [&BoundingBox, &FlattenedBounds, MinT, MaxT, this](const double Discriminant, const double A, const double B)
		{
			// Negative discriminant means no solution; A == 0 implies coincident start/end points
			if (Discriminant > 0 && !FMath::IsNearlyZero(A))
			{
				const double SqrtDiscriminant = FMath::Sqrt(Discriminant);
				const double Denominator = 0.5 / A;
				const double T0 = (-B + SqrtDiscriminant) * Denominator;
				const double T1 = (-B - SqrtDiscriminant) * Denominator;

				if (T0 >= MinT && T0 <= MaxT)
				{
					BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(UE::SplineMesh::RealToFloatChecked(T0), MinT, MaxT));
				}

				if (T1 >= MinT && T1 <= MaxT)
				{
					BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(UE::SplineMesh::RealToFloatChecked(T1), MinT, MaxT));
				}
			}
		};

	// Minima/maxima happen where dx/dt == 0, calculate t values
	const FVector Discriminant = B * B - 4 * A * C;

	// Work out minima/maxima component-by-component.
	AppendAxisExtrema(Discriminant.X, A.X, B.X);
	AppendAxisExtrema(Discriminant.Y, A.Y, B.Y);
	AppendAxisExtrema(Discriminant.Z, A.Z, B.Z);

	// Applying extrapolation if bounds to apply on spline are different than the mesh bounds used
	// to define the spline range [0,1]
	if (InBoundsToDistort != nullptr && InBoundsToDistort != &InMeshBounds)
	{
		const double BoundsMin = GetAxisValueRef(BoundsToDistort.Origin - BoundsToDistort.BoxExtent, ForwardAxis);
		const double BoundsMax = GetAxisValueRef(BoundsToDistort.Origin + BoundsToDistort.BoxExtent, ForwardAxis);

		float Alpha = ComputeRatioAlongSpline(UE::SplineMesh::RealToFloatChecked(BoundsMin));
		if (Alpha < MinT)
		{
			BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(Alpha, MinT, MaxT));
		}

		Alpha = ComputeRatioAlongSpline(UE::SplineMesh::RealToFloatChecked(BoundsMax));
		if (Alpha > MaxT)
		{
			BoundingBox += FlattenedBounds.TransformBy(CalcSliceTransformAtSplineOffset(Alpha, MinT, MaxT));
		}
	}

	return BoundingBox.TransformBy(InLocalToWorld);
}

FTransform USplineMeshComponent::GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace) const
{
	if (InSocketName != NAME_None)
	{
		UStaticMeshSocket const* const Socket = GetSocketByName(InSocketName);
		if (Socket)
		{
			FTransform SocketTransform(Socket->RelativeRotation, Socket->RelativeLocation * GetAxisMask(ForwardAxis), Socket->RelativeScale);
			SocketTransform = SocketTransform * CalcSliceTransform(UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(Socket->RelativeLocation, ForwardAxis)));

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
					return (SocketTransform * GetComponentToWorld()).GetRelativeTransform(Actor->GetTransform());
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
	const float Alpha = ComputeRatioAlongSpline(DistanceAlong);

	float MinT = 0.f;
	float MaxT = 1.f;
	ComputeVisualMeshSplineTRange(MinT, MaxT);

	return CalcSliceTransformAtSplineOffset(Alpha, MinT, MaxT);
}

FTransform USplineMeshComponent::CalcSliceTransformAtSplineOffset(const float Alpha, const float MinT, const float MaxT) const
{
	// Apply hermite interp to Alpha if desired
	const float HermiteAlpha = bSmoothInterpRollScale ? SmoothStep(0.0, 1.0, Alpha) : Alpha;

	// Then find the point and direction of the spline at this point along
	FVector3f SplinePos;
	FVector3f SplineDir;

	// Use linear extrapolation
	if (Alpha < MinT)
	{
		const FVector3f StartTangent(SplineEvalTangent(SplineParams, MinT));
		SplinePos = FVector3f(SplineParams.StartPos) + (StartTangent * (Alpha - MinT));
		SplineDir = StartTangent.GetSafeNormal();
	}
	else if (Alpha > MaxT)
	{
		const FVector3f EndTangent(SplineEvalTangent(SplineParams, MaxT));
		SplinePos = FVector3f(SplineParams.EndPos) + (EndTangent * (Alpha - MaxT));
		SplineDir = EndTangent.GetSafeNormal();
	}
	else
	{
		SplinePos = SplineEvalPos(SplineParams, Alpha);
		SplineDir = SplineEvalDir(SplineParams, Alpha);
	}

	// Find base frenet frame
	const FVector3f BaseXVec = (FVector3f(SplineUpDir) ^ SplineDir).GetSafeNormal();
	const FVector3f BaseYVec = (FVector3f(SplineDir) ^ BaseXVec).GetSafeNormal();

	// Offset the spline by the desired amount
	const FVector2f SliceOffset = FMath::Lerp(FVector2f(SplineParams.StartOffset), FVector2f(SplineParams.EndOffset), HermiteAlpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const float UseRoll = FMath::Lerp(SplineParams.StartRoll, SplineParams.EndRoll, HermiteAlpha);
	const float CosAng = FMath::Cos(UseRoll);
	const float SinAng = FMath::Sin(UseRoll);
	const FVector3f XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	const FVector3f YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);

	// Find scale at this point along spline
	const FVector2f UseScale = FMath::Lerp(FVector2f(SplineParams.StartScale), FVector2f(SplineParams.EndScale), HermiteAlpha);

	// Build overall transform
	FTransform SliceTransform;
	switch (ForwardAxis)
	{
	case ESplineMeshAxis::X:
		SliceTransform = FTransform(FVector(SplineDir), FVector(XVec), FVector(YVec), FVector(SplinePos));
		SliceTransform.SetScale3D(FVector(1, UseScale.X, UseScale.Y));
		break;
	case ESplineMeshAxis::Y:
		SliceTransform = FTransform(FVector(YVec), FVector(SplineDir), FVector(XVec), FVector(SplinePos));
		SliceTransform.SetScale3D(FVector(UseScale.Y, 1, UseScale.X));
		break;
	case ESplineMeshAxis::Z:
		SliceTransform = FTransform(FVector(XVec), FVector(YVec), FVector(SplineDir), FVector(SplinePos));
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

FBox USplineMeshComponent::GetNavigationBounds() const
{
	return CachedNavigationBounds;
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
	if (BodySetup != nullptr && (BodySetup->TriMeshGeometries.Num() || BodySetup->AggGeom.GetElementCount() > 0))
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
		const UNavCollisionBase* NavCollision = GetStaticMesh()->GetNavCollision();

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
					Vertex = CalcSliceTransform(UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(Vertex, ForwardAxis))).TransformPosition(Vertex * Mask);
					VertexBuffer.Add(Vertex);
				}
				GeomExport.ExportCustomMesh(VertexBuffer.GetData(), VertexBuffer.Num(),
					NavCollision->GetConvexCollision().IndexBuffer.GetData(), NavCollision->GetConvexCollision().IndexBuffer.Num(),
					GetComponentTransform());

				VertexBuffer.Reset();
				for (int32 i = 0; i < NavCollision->GetTriMeshCollision().VertexBuffer.Num(); ++i)
				{
					FVector Vertex = NavCollision->GetTriMeshCollision().VertexBuffer[i];
					Vertex = CalcSliceTransform(UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(Vertex, ForwardAxis))).TransformPosition(Vertex * Mask);
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
				const float Z = UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(SphereElem.Center, ForwardAxis));
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphereElem.Center *= Mask;

				SphereElem.Radius *= SliceTransform.GetMaximumAxisScale();
				SphereElem.Center = SliceTransform.TransformPosition(SphereElem.Center);
			}

			// distortion of a sphyl can't be done nicely, so we just transform the origin and size
			for (FKSphylElem& SphylElem : BodySetup->AggGeom.SphylElems)
			{
				const float Z = UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(SphylElem.Center, ForwardAxis));
				FTransform SliceTransform = CalcSliceTransform(Z);
				SphylElem.Center *= Mask;

				FTransform TM = SphylElem.GetTransform();
				SphylElem.Length = UE::SplineMesh::RealToFloatChecked((TM * SliceTransform).TransformVector(FVector(0, 0, SphylElem.Length)).Size());
				SphylElem.Radius *= SliceTransform.GetMaximumAxisScale();

				SphylElem.SetTransform(TM * SliceTransform);
			}

			// Convert boxes to convex hulls to better respect distortion
			for (FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
			{
				FKConvexElem& ConvexElem = BodySetup->AggGeom.ConvexElems.AddDefaulted_GetRef();

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
					Point = CalcSliceTransform(UE::SplineMesh::RealToFloatChecked(GetAxisValueRef(TransformedPoint, ForwardAxis))).TransformPosition(TransformedPoint * Mask);
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
		constexpr float MinExtent = 1.0f;
		const FBoxSphereBounds UndeformedBounds = GetStaticMesh()->GetBounds().TransformBy(GetComponentTransform());
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

	return UE::SplineMesh::RealToFloatChecked(SplineDeformFactor) * Super::GetTextureStreamingTransformScale();
}

#if WITH_EDITOR
void USplineMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const bool bIsSplineParamsChange = MemberPropertyThatChanged && MemberPropertyThatChanged->GetNameCPP() == TEXT("SplineParams");
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

