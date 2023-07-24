// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleSystemRender.cpp: Particle system rendering functions.
=============================================================================*/

#include "ParticleEmitterInstances.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "Particles/Orientation/ParticleModuleOrientationAxisLock.h"
#include "UObject/UObjectIterator.h"
#include "MaterialDomain.h"
#include "MeshParticleVertexFactory.h"
#include "Particles/ParticleEmitter.h"
#include "Particles/ParticleSystemComponent.h"
#include "Particles/ParticleModule.h"
#include "StaticMeshResources.h"
#include "ParticleResources.h"
#include "Particles/ParticlePerfStats.h"
#include "Particles/TypeData/ParticleModuleTypeDataBeam2.h"
#include "Particles/ParticleSpriteEmitter.h"
#include "Particles/TypeData/ParticleModuleTypeDataMesh.h"
#include "Particles/ParticleSystem.h"
#include "Particles/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Particles/ParticleModuleRequired.h"
#include "ParticleBeamTrailVertexFactory.h"
#include "SceneInterface.h"
#include "SceneRendering.h"
#include "Particles/ParticleLODLevel.h"
#include "Engine/StaticMesh.h"
#include "Stats/StatsTrace.h"
#include "UnrealEngine.h"
#include "RenderCore.h"
#include "DataDrivenShaderPlatformInfo.h"

DECLARE_CYCLE_STAT(TEXT("ParticleSystemSceneProxy Create GT"), STAT_FParticleSystemSceneProxy_Create, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("ParticleSystemSceneProxy GetMeshElements RT"), STAT_FParticleSystemSceneProxy_GetMeshElements, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("DynamicSpriteEmitterData GetDynamicMeshElementsEmitter GetParticleOrderData RT"), STAT_FDynamicSpriteEmitterData_GetDynamicMeshElementsEmitter_GetParticleOrderData, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("DynamicSpriteEmitterData PerParticleWorkOrTasks RT"), STAT_FDynamicSpriteEmitterData_PerParticleWorkOrTasks, STATGROUP_Particles);
DECLARE_CYCLE_STAT(TEXT("DynamicSpriteEmitterData GetDynamicMeshElementsEmitter Task RT"), STAT_FDynamicSpriteEmitterData_GetDynamicMeshElementsEmitter_Task, STATGROUP_Particles);


#include "InGamePerformanceTracker.h"

static int32 GFXAllowParticleMeshLODs = 0;
static FAutoConsoleVariableRef CVarFXAllowParticleMeshLODs(
	TEXT("fx.FXAllowParticleMeshLODs"),
	GFXAllowParticleMeshLODs,
	TEXT("If we allow particle meshes to use LODs or not"),
	ECVF_Default
);

/** 
 * Whether to track particle rendering stats.  
 * Enable with the TRACKPARTICLERENDERINGSTATS command. 
 */
bool GTrackParticleRenderingStats = false;

/** Seconds between stat captures. */
float GTimeBetweenParticleRenderStatCaptures = 5.0f;

/** Minimum render time for a single DrawDynamicElements call that should be recorded. */
float GMinParticleDrawTimeToTrack = .0001f;

/** Whether to do LOD calculation on GameThread in game */
extern bool GbEnableGameThreadLODCalculation;

///////////////////////////////////////////////////////////////////////////////
FParticleOrderPool GParticleOrderPool;

///////////////////////////////////////////////////////////////////////////////

/**
 * Retrieve the appropriate camera Up and Right vectors for LockAxis situations
 *
 * @param LockAxisFlag	The lock axis flag to compute camera vectors for.
 * @param LocalToWorld	The local-to-world transform for the emitter (identify unless the emitter is rendering in local space).
 * @param CameraUp		OUTPUT - the resulting camera Up vector
 * @param CameraRight	OUTPUT - the resulting camera Right vector
 */
void ComputeLockedAxes(EParticleAxisLock LockAxisFlag, const FMatrix& LocalToWorld, FVector& CameraUp, FVector& CameraRight)
{
	switch (LockAxisFlag)
	{
	case EPAL_X:
		CameraUp	= -LocalToWorld.GetUnitAxis( EAxis::Z );
		CameraRight	= -LocalToWorld.GetUnitAxis( EAxis::Y );
		break;
	case EPAL_Y:
		CameraUp	= -LocalToWorld.GetUnitAxis( EAxis::Z );
		CameraRight	=  LocalToWorld.GetUnitAxis( EAxis::X );
		break;
	case EPAL_Z:
		CameraUp	= -LocalToWorld.GetUnitAxis( EAxis::X );
		CameraRight	=  LocalToWorld.GetUnitAxis( EAxis::Y );
		break;
	case EPAL_NEGATIVE_X:
		CameraUp	= -LocalToWorld.GetUnitAxis( EAxis::Z );
		CameraRight	=  LocalToWorld.GetUnitAxis( EAxis::Y );
		break;
	case EPAL_NEGATIVE_Y:
		CameraUp	= -LocalToWorld.GetUnitAxis( EAxis::Z );
		CameraRight	= -LocalToWorld.GetUnitAxis( EAxis::X );
		break;
	case EPAL_NEGATIVE_Z:
		CameraUp	= -LocalToWorld.GetUnitAxis( EAxis::X );
		CameraRight	= -LocalToWorld.GetUnitAxis( EAxis::Y );
		break;
	case EPAL_ROTATE_X:
		CameraRight	= LocalToWorld.GetUnitAxis( EAxis::X );
		CameraUp	= FVector::ZeroVector;
		break;
	case EPAL_ROTATE_Y:
		CameraRight	= LocalToWorld.GetUnitAxis( EAxis::Y );
		CameraUp	= FVector::ZeroVector;
		break;
	case EPAL_ROTATE_Z:
		CameraRight	= LocalToWorld.GetUnitAxis( EAxis::Z );
		CameraUp	= FVector::ZeroVector;
		break;
	}
}

FORCEINLINE FVector GetCameraOffset(
	float CameraPayloadOffset,
	FVector DirToCamera
	)
{
	float CheckSize = DirToCamera.SizeSquared();
	DirToCamera.Normalize();

	if (CheckSize > (CameraPayloadOffset * CameraPayloadOffset))
	{
		return DirToCamera * CameraPayloadOffset;
	}
	else
	{
		// If the offset will push the particle behind the camera, then push it 
		// WAY behind the camera. This is a hack... but in the case of 
		// PSA_Velocity, it is required to ensure that the particle doesn't 
		// 'spin' flat and come into view.
		return DirToCamera * CameraPayloadOffset * HALF_WORLD_MAX;
	}
}

/**
 *	Helper function for retrieving the camera offset payload of a particle.
 *
 *	@param	InCameraPayloadOffset	The offset to the camera offset payload data.
 *	@param	InParticle				The particle being processed.
 *	@param	InPosition				The position of the particle being processed.
 *	@param	InCameraPosition		The position of the camera in local space.
 *
 *	@returns the offset to apply to the particle's position.
 */
FORCEINLINE FVector GetCameraOffsetFromPayload(
	int32 InCameraPayloadOffset,
	const FBaseParticle& InParticle,
	const FVector& InParticlePosition,
	const FVector& InCameraPosition 
	)
{
	checkSlow(InCameraPayloadOffset > 0);

	FVector DirToCamera = InCameraPosition - InParticlePosition;
	FCameraOffsetParticlePayload* CameraPayload = ((FCameraOffsetParticlePayload*)((uint8*)(&InParticle) + InCameraPayloadOffset));
	
	return GetCameraOffset(CameraPayload->Offset, DirToCamera);
}

void FDynamicSpriteEmitterDataBase::SortSpriteParticles(int32 SortMode, bool bLocalSpace, 
	int32 ParticleCount, const uint8* ParticleData, int32 ParticleStride, const uint16* ParticleIndices,
	const FSceneView* View, const FMatrix& LocalToWorld, FParticleOrder* ParticleOrder) const
{
	SCOPE_CYCLE_COUNTER(STAT_SortingTime);

	struct FCompareParticleOrderZ
	{
		FORCEINLINE bool operator()( const FParticleOrder& A, const FParticleOrder& B ) const { return B.Z < A.Z; }
	};
	struct FCompareParticleOrderC
	{
		FORCEINLINE bool operator()( const FParticleOrder& A, const FParticleOrder& B ) const { return B.C < A.C; }
	};

	if (SortMode == PSORTMODE_ViewProjDepth)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			float InZ;
			if (bLocalSpace)
			{
				InZ = View->ViewMatrices.GetViewProjectionMatrix().TransformPosition(LocalToWorld.TransformPosition(Particle.Location)).W;
			}
			else
			{
				InZ = View->ViewMatrices.GetViewProjectionMatrix().TransformPosition(Particle.Location).W;
			}
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;

			ParticleOrder[ParticleIndex].Z = InZ;
		}
		Sort( ParticleOrder, ParticleCount, FCompareParticleOrderZ() );
	}
	else if (SortMode == PSORTMODE_DistanceToView)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			float InZ;
			FVector Position;
			if (bLocalSpace)
			{
				Position = LocalToWorld.TransformPosition(Particle.Location);
			}
			else
			{
				Position = Particle.Location;
			}
			InZ = (View->ViewMatrices.GetViewOrigin() - Position).SizeSquared();
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].Z = InZ;
		}
		Sort( ParticleOrder, ParticleCount, FCompareParticleOrderZ() );
	}
	else if (SortMode == PSORTMODE_Age_OldestFirst)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].C = Particle.Flags & STATE_CounterMask;
		}
		Sort( ParticleOrder, ParticleCount, FCompareParticleOrderC() );
	}
	else if (SortMode == PSORTMODE_Age_NewestFirst)
	{
		for (int32 ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
		{
			DECLARE_PARTICLE(Particle, ParticleData + ParticleStride * ParticleIndices[ParticleIndex]);
			ParticleOrder[ParticleIndex].ParticleIndex = ParticleIndex;
			ParticleOrder[ParticleIndex].C = (~Particle.Flags) & STATE_CounterMask;
		}
		Sort( ParticleOrder, ParticleCount, FCompareParticleOrderC() );
	}
}

void FDynamicSpriteEmitterDataBase::RenderDebug(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, bool bCrosses) const
{
	check(Proxy);

	const FDynamicSpriteEmitterReplayData& SpriteSource =
		static_cast< const FDynamicSpriteEmitterReplayData& >( GetSource() );

	const FMatrix& LocalToWorld = SpriteSource.bUseLocalSpace ? Proxy->GetLocalToWorld() : FMatrix::Identity;

	FMatrix CameraToWorld = View->ViewMatrices.GetInvViewMatrix();
	FVector CamX = CameraToWorld.TransformVector(FVector(1,0,0));
	FVector CamY = CameraToWorld.TransformVector(FVector(0,1,0));

	FLinearColor EmitterEditorColor = FLinearColor(1.0f,1.0f,0);

	for (int32 i = 0; i < SpriteSource.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE(Particle, SpriteSource.DataContainer.ParticleData + SpriteSource.ParticleStride * SpriteSource.DataContainer.ParticleIndices[i]);

		FVector DrawLocation = LocalToWorld.TransformPosition(Particle.Location);
		if (bCrosses)
		{
			FVector Size(Particle.Size * SpriteSource.Scale);
			PDI->DrawLine(DrawLocation - (0.5f * Size.X * CamX), DrawLocation + (0.5f * Size.X * CamX), EmitterEditorColor, Proxy->GetDepthPriorityGroup(View));
			PDI->DrawLine(DrawLocation - (0.5f * Size.Y * CamY), DrawLocation + (0.5f * Size.Y * CamY), EmitterEditorColor, Proxy->GetDepthPriorityGroup(View));
		}
		else
		{
			PDI->DrawPoint(DrawLocation, EmitterEditorColor, 2, Proxy->GetDepthPriorityGroup(View));
		}
	}
}

void FDynamicSpriteEmitterDataBase::BuildViewFillData(
	const FParticleSystemSceneProxy* Proxy, 
	const FSceneView* InView, 
	int32 InVertexCount, 
	int32 InVertexSize, 
	int32 InDynamicParameterVertexStride, 
	FGlobalDynamicIndexBuffer& DynamicIndexBuffer,
	FGlobalDynamicVertexBuffer& DynamicVertexBuffer,
	FGlobalDynamicVertexBuffer::FAllocation& DynamicVertexAllocation,
	FGlobalDynamicIndexBuffer::FAllocation& DynamicIndexAllocation,
	FGlobalDynamicVertexBuffer::FAllocation* DynamicParameterAllocation,
	FAsyncBufferFillData& Data) const
{
	Data.LocalToWorld = Proxy->GetLocalToWorld();
	Data.WorldToLocal = Proxy->GetWorldToLocal();
	Data.View = InView;
	check(Data.VertexSize == 0 || Data.VertexSize == InVertexSize);

	DynamicVertexAllocation = DynamicVertexBuffer.Allocate( InVertexCount * InVertexSize );

	if (DynamicVertexBuffer.IsRenderAlarmLoggingEnabled())
	{
		UE_LOG(LogParticles, Warning, TEXT("Panic logging.  Allocated %u bytes for Resource: %s, Owner: %s"), InVertexCount * InVertexSize, *Proxy->GetResourceName().ToString(), *Proxy->GetOwnerName().ToString())
	}

	Data.VertexData = DynamicVertexAllocation.Buffer;
	Data.VertexCount = InVertexCount;
	Data.VertexSize = InVertexSize;

	int32 NumIndices, IndexStride;
	GetIndexAllocInfo(NumIndices, IndexStride);
	check(IndexStride > 0);

	DynamicIndexAllocation = DynamicIndexBuffer.Allocate( NumIndices, IndexStride );
	Data.IndexData = DynamicIndexAllocation.Buffer;
	Data.IndexCount = NumIndices;

	Data.DynamicParameterData = NULL;

	if( bUsesDynamicParameter )
	{
		check( InDynamicParameterVertexStride > 0 );
		*DynamicParameterAllocation = DynamicVertexBuffer.Allocate( InVertexCount * InDynamicParameterVertexStride );

		if (DynamicVertexBuffer.IsRenderAlarmLoggingEnabled())
		{
			UE_LOG(LogParticles, Warning, TEXT("Panic logging.  Allocated %u bytes for Resource: %s, Owner: %s"), InVertexCount * InDynamicParameterVertexStride, *Proxy->GetResourceName().ToString(), *Proxy->GetOwnerName().ToString())
		}

		Data.DynamicParameterData = DynamicParameterAllocation->Buffer;
	}
}

///////////////////////////////////////////////////////////////////////////////
//	ParticleMeshEmitterInstance
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//	FDynamicSpriteEmitterData
///////////////////////////////////////////////////////////////////////////////

/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicSpriteEmitterData::Init( bool bInSelected )
{
	bSelected = bInSelected;

	bUsesDynamicParameter = GetSourceData()->DynamicParameterDataOffset > 0;

	UMaterialInterface const* MaterialInterface = const_cast<UMaterialInterface const*>(Source.MaterialInterface);
	MaterialResource = MaterialInterface->GetRenderProxy();

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}

FVector2D GetParticleSize(const FBaseParticle& Particle, const FDynamicSpriteEmitterReplayDataBase& Source)
{
	FVector2D Size;
	Size.X = FMath::Abs(Particle.Size.X * Source.Scale.X);
	Size.Y = FMath::Abs(Particle.Size.Y * Source.Scale.Y);
	if (Source.ScreenAlignment == PSA_Square || Source.ScreenAlignment == PSA_FacingCameraPosition || Source.ScreenAlignment == PSA_FacingCameraDistanceBlend)
	{
		Size.Y = Size.X;
	}

	return Size;
}

void ApplyOrbitToPosition(
	const FBaseParticle& Particle, 
	const FDynamicSpriteEmitterReplayDataBase& Source, 
	const FMatrix& InLocalToWorld,
	FVector& ParticlePosition,
	FVector& ParticleOldPosition
	)
{
	if (Source.OrbitModuleOffset != 0)
	{
		int32 CurrentOffset = Source.OrbitModuleOffset;
		const uint8* ParticleBase = (const uint8*)&Particle;
		PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);

		if (Source.bUseLocalSpace)
		{
			ParticlePosition += (FVector)OrbitPayload.Offset;
			ParticleOldPosition += (FVector)OrbitPayload.PreviousOffset;
		}
		else
		{
			ParticlePosition += InLocalToWorld.TransformVector((FVector)OrbitPayload.Offset);
			ParticleOldPosition += InLocalToWorld.TransformVector((FVector)OrbitPayload.PreviousOffset);
		}
	}
}

bool FDynamicSpriteEmitterData::GetVertexAndIndexData(void* VertexData, void* DynamicParameterVertexData, void* FillIndexData, FParticleOrder* ParticleOrder, const FVector& InCameraPosition, const FMatrix& InLocalToWorld, uint32 InstanceFactor) const
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);
	int32 ParticleCount = Source.ActiveParticleCount;
	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
	{
		ParticleCount = Source.MaxDrawCount;
	}

	// Put the camera origin in the appropriate coordinate space.
	FVector CameraPosition = InCameraPosition;
	if (Source.bUseLocalSpace)
	{
		FMatrix InvSelf = InLocalToWorld.Inverse();
		CameraPosition = InvSelf.TransformPosition(InCameraPosition);
	}

	// Pack the data
	int32	ParticleIndex;
	int32	ParticlePackingIndex = 0;
	int32	IndexPackingIndex = 0;

	int32 VertexStride = sizeof(FParticleSpriteVertex);
	int32 VertexDynamicParameterStride = sizeof(FParticleVertexDynamicParameter);
	uint8* TempVert = (uint8*)VertexData;
	uint8* TempDynamicParameterVert = (uint8*)DynamicParameterVertexData;
	FParticleSpriteVertex* FillVertex;
	FParticleVertexDynamicParameter* DynFillVertex;

	FVector4f DynamicParameterValue(1.0f,1.0f,1.0f,1.0f);
	FVector ParticlePosition;
	FVector ParticleOldPosition;
	float SubImageIndex = 0.0f;

	const uint8* ParticleData = Source.DataContainer.ParticleData;
	const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;
	const FParticleOrder* OrderedIndices = ParticleOrder;

	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();
	for (int32 i = 0; i < ParticleCount; i++)
	{
		ParticleIndex = OrderedIndices ? OrderedIndices[i].ParticleIndex : i;
		DECLARE_PARTICLE_CONST(Particle, ParticleData + Source.ParticleStride * ParticleIndices[ParticleIndex]);
		if (i + 1 < ParticleCount)
		{
			int32 NextIndex = OrderedIndices ? OrderedIndices[i+1].ParticleIndex : (i + 1);
			DECLARE_PARTICLE_CONST(NextParticle, ParticleData + Source.ParticleStride * ParticleIndices[NextIndex]);
			FPlatformMisc::Prefetch(&NextParticle);
		}

		const FVector2D Size = GetParticleSize(Particle, Source);

		ParticlePosition = Particle.Location;
		ParticleOldPosition = Particle.OldLocation;

		ApplyOrbitToPosition(Particle, Source, InLocalToWorld, ParticlePosition, ParticleOldPosition);

		if (Source.CameraPayloadOffset != 0)
		{
			FVector CameraOffset = GetCameraOffsetFromPayload(Source.CameraPayloadOffset, Particle, ParticlePosition, CameraPosition);
			ParticlePosition += CameraOffset;
			ParticleOldPosition += CameraOffset;
		}

		if (Source.SubUVDataOffset > 0)
		{
			FFullSubUVPayload* SubUVPayload = (FFullSubUVPayload*)(((uint8*)&Particle) + Source.SubUVDataOffset);
			SubImageIndex = SubUVPayload->ImageIndex;
		}

		if (Source.DynamicParameterDataOffset > 0)
		{
			GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, Particle, DynamicParameterValue);
		}

		//@todo - refactor into instance step rate in the RHI
		for (uint32 Factor = 0; Factor < InstanceFactor; Factor++)
		{
			FillVertex = (FParticleSpriteVertex*)TempVert;
			FillVertex->Position = FVector3f(ParticlePosition - LWCTileOffset);
			FillVertex->RelativeTime = Particle.RelativeTime;
			FillVertex->OldPosition = FVector3f(ParticleOldPosition - LWCTileOffset);
			// Create a floating point particle ID from the counter, map into approximately 0-1
			FillVertex->ParticleId = (Particle.Flags & STATE_CounterMask) / 10000.0f;
			FillVertex->Size = FVector2f(GetParticleSizeWithUVFlipInSign(Particle, Size));
			FillVertex->Rotation = Particle.Rotation;
			FillVertex->SubImageIndex = SubImageIndex;
			FillVertex->Color = Particle.Color;
			if (bUsesDynamicParameter)
			{
				DynFillVertex = (FParticleVertexDynamicParameter*)TempDynamicParameterVert;
				DynFillVertex->DynamicValue[0] = DynamicParameterValue.X;
				DynFillVertex->DynamicValue[1] = DynamicParameterValue.Y;
				DynFillVertex->DynamicValue[2] = DynamicParameterValue.Z;
				DynFillVertex->DynamicValue[3] = DynamicParameterValue.W;
				TempDynamicParameterVert += VertexDynamicParameterStride;
			}

			TempVert += VertexStride;
		}
	}

	return true;
}

bool FDynamicSpriteEmitterData::GetVertexAndIndexDataNonInstanced(void* VertexData, void* DynamicParameterVertexData, void* FillIndexData, FParticleOrder* ParticleOrder, const FVector& InCameraPosition, const FMatrix& InLocalToWorld, int32 NumVerticesPerParticle) const
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);

	int32 ParticleCount = Source.ActiveParticleCount;
	// 'clamp' the number of particles actually drawn
	//@todo.SAS. If sorted, we really want to render the front 'N' particles...
	// right now it renders the back ones. (Same for SubUV draws)
	if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
	{
		ParticleCount = Source.MaxDrawCount;
	}

	// Put the camera origin in the appropriate coordinate space.
	FVector CameraPosition = InCameraPosition;
	if (Source.bUseLocalSpace)
	{
		FMatrix InvSelf = InLocalToWorld.Inverse();
		CameraPosition = InvSelf.TransformPosition(InCameraPosition);
	}

	// Pack the data
	int32	ParticleIndex;
	int32	ParticlePackingIndex = 0;
	int32	IndexPackingIndex = 0;

	int32 VertexStride = sizeof(FParticleSpriteVertexNonInstanced) * NumVerticesPerParticle;
	int32 VertexDynamicParameterStride = sizeof(FParticleVertexDynamicParameter) * NumVerticesPerParticle;

	uint8* TempVert = (uint8*)VertexData;
	uint8* TempDynamicParameterVert = (uint8*)DynamicParameterVertexData;
	FParticleSpriteVertexNonInstanced* FillVertex;
	FParticleVertexDynamicParameter* DynFillVertex;

	FVector4f DynamicParameterValue(1.0f,1.0f,1.0f,1.0f);
	FVector ParticlePosition;
	FVector ParticleOldPosition;
	float SubImageIndex = 0.0f;

	const uint8* ParticleData = Source.DataContainer.ParticleData;
	const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;
	const FParticleOrder* OrderedIndices = ParticleOrder;
	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();

	for (int32 i = 0; i < ParticleCount; i++)
	{
		ParticleIndex = OrderedIndices ? OrderedIndices[i].ParticleIndex : i;
		DECLARE_PARTICLE_CONST(Particle, ParticleData + Source.ParticleStride * ParticleIndices[ParticleIndex]);
		if (i + 1 < ParticleCount)
		{
			int32 NextIndex = OrderedIndices ? OrderedIndices[i+1].ParticleIndex : (i + 1);
			DECLARE_PARTICLE_CONST(NextParticle, ParticleData + Source.ParticleStride * ParticleIndices[NextIndex]);
			FPlatformMisc::Prefetch(&NextParticle);
		}

		const FVector2D Size = GetParticleSize(Particle, Source);

		ParticlePosition = Particle.Location;
		ParticleOldPosition = Particle.OldLocation;

		ApplyOrbitToPosition(Particle, Source, InLocalToWorld, ParticlePosition, ParticleOldPosition);

		if (Source.CameraPayloadOffset != 0)
		{
			FVector CameraOffset = GetCameraOffsetFromPayload(Source.CameraPayloadOffset, Particle, ParticlePosition, CameraPosition);
			ParticlePosition += CameraOffset;
			ParticleOldPosition += CameraOffset;
		}

		if (Source.SubUVDataOffset > 0)
		{
			FFullSubUVPayload* SubUVPayload = (FFullSubUVPayload*)(((uint8*)&Particle) + Source.SubUVDataOffset);
			SubImageIndex = SubUVPayload->ImageIndex;
		}

		if (Source.DynamicParameterDataOffset > 0)
		{
			GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, Particle, DynamicParameterValue);
		}

		FillVertex = (FParticleSpriteVertexNonInstanced*)TempVert;

		const FVector2f* SubUVVertexData = nullptr;

		if (Source.RequiredModule->bCutoutTexureIsValid)
		{
			const int32 SubImageIndexInt = FMath::TruncToInt(SubImageIndex);
			int32 FrameIndex = SubImageIndexInt % Source.RequiredModule->NumFrames;

			if (SubImageIndexInt < 0)
			{
				// Mod operator returns remainder toward zero, not toward negative which is what we want
				FrameIndex = Source.RequiredModule->NumFrames - SubImageIndexInt;
			}

			SubUVVertexData = &Source.RequiredModule->FrameData[FrameIndex];
		}

		const bool bHasUVVertexData = SubUVVertexData && Source.RequiredModule->bCutoutTexureIsValid;

		for (int32 VertexIndex = 0; VertexIndex < NumVerticesPerParticle; ++VertexIndex)
		{
			if (bHasUVVertexData)
			{
				// Warning: not supporting UV flipping with cutout geometry in the non-instanced path
				FillVertex[VertexIndex].UV = SubUVVertexData[VertexIndex];
			}
			else
			{
				if(VertexIndex == 0)
				{
					FillVertex[VertexIndex].UV = FVector2f(0.0f, 0.0f);
				}
				if(VertexIndex == 1)
				{
					FillVertex[VertexIndex].UV = FVector2f(0.0f, 1.0f);
				}
				if(VertexIndex == 2)
				{
					FillVertex[VertexIndex].UV = FVector2f(1.0f, 1.0f);
				}
				if(VertexIndex == 3)
				{
					FillVertex[VertexIndex].UV = FVector2f(1.0f, 0.0f);
				}
			}

			FillVertex[VertexIndex].Position	= FVector3f(ParticlePosition - LWCTileOffset);
			FillVertex[VertexIndex].RelativeTime = Particle.RelativeTime;
			FillVertex[VertexIndex].OldPosition	= FVector3f(ParticleOldPosition - LWCTileOffset);
			// Create a floating point particle ID from the counter, map into approximately 0-1
			FillVertex[VertexIndex].ParticleId = (Particle.Flags & STATE_CounterMask) / 10000.0f;
			FillVertex[VertexIndex].Size = FVector2f(GetParticleSizeWithUVFlipInSign(Particle, Size));
			FillVertex[VertexIndex].Rotation	= Particle.Rotation;
			FillVertex[VertexIndex].SubImageIndex = SubImageIndex;
			FillVertex[VertexIndex].Color		= Particle.Color;
		}

		if (bUsesDynamicParameter)
		{
			DynFillVertex = (FParticleVertexDynamicParameter*)TempDynamicParameterVert;

			for (int32 VertexIndex = 0; VertexIndex < NumVerticesPerParticle; ++VertexIndex)
			{
				DynFillVertex[VertexIndex].DynamicValue[0] = DynamicParameterValue.X;
				DynFillVertex[VertexIndex].DynamicValue[1] = DynamicParameterValue.Y;
				DynFillVertex[VertexIndex].DynamicValue[2] = DynamicParameterValue.Z;
				DynFillVertex[VertexIndex].DynamicValue[3] = DynamicParameterValue.W;
			}
			TempDynamicParameterVert += VertexDynamicParameterStride;
		}
		TempVert += VertexStride;
	}

	return true;
}

void GatherParticleLightData(const FDynamicSpriteEmitterReplayDataBase& Source, const FMatrix& InLocalToWorld, const FSceneViewFamily& InViewFamily, FSimpleLightArray& OutParticleLights)
{
	if (Source.LightDataOffset != 0)
	{
		int32 ParticleCount = Source.ActiveParticleCount;
		// 'clamp' the number of particles actually drawn
		//@todo.SAS. If sorted, we really want to render the front 'N' particles...
		// right now it renders the back ones. (Same for SubUV draws)
		if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
		{
			ParticleCount = Source.MaxDrawCount;
		}

		OutParticleLights.InstanceData.Reserve(OutParticleLights.InstanceData.Num() + ParticleCount);
		
		// Reserve memory for per-view data. If camera offset is not used then all views can share per-view data in order to save memory.
		if(Source.CameraPayloadOffset != 0)
		{
			OutParticleLights.PerViewData.Reserve(OutParticleLights.PerViewData.Num() + ParticleCount * InViewFamily.Views.Num());
		}
		else
		{
			OutParticleLights.PerViewData.Reserve(OutParticleLights.PerViewData.Num() + ParticleCount);
		}
		
		const uint8* ParticleData = Source.DataContainer.ParticleData;
		const uint16* ParticleIndices = Source.DataContainer.ParticleIndices;

		for (int32 i = 0; i < ParticleCount; i++)
		{
			DECLARE_PARTICLE_CONST(Particle, ParticleData + Source.ParticleStride * ParticleIndices[i]);

			if (i + 1 < ParticleCount)
			{
				int32 NextIndex = (i + 1);
				DECLARE_PARTICLE_CONST(NextParticle, ParticleData + Source.ParticleStride * ParticleIndices[NextIndex]);
				FPlatformMisc::Prefetch(&NextParticle);
			}

			const FLightParticlePayload* LightPayload = (const FLightParticlePayload*)(((const uint8*)&Particle) + Source.LightDataOffset);

			if (LightPayload->bValid)
			{
				const FVector2D Size = GetParticleSize(Particle, Source);
				
				FSimpleLightEntry ParticleLight;
				ParticleLight.Radius =  LightPayload->RadiusScale * (Size.X + Size.Y) / 2.0f;
				ParticleLight.Color = FVector3f(Particle.Color) * Particle.Color.A * LightPayload->ColorScale;
				ParticleLight.Exponent = LightPayload->LightExponent;
				ParticleLight.InverseExposureBlend = LightPayload->InverseExposureBlend;
				ParticleLight.VolumetricScatteringIntensity = Source.LightVolumetricScatteringIntensity;
				ParticleLight.bAffectTranslucency = LightPayload->bAffectsTranslucency;

				// Early out if the light will have no visible contribution
				if (LightPayload->bHighQuality || 
					(ParticleLight.Radius <= UE_KINDA_SMALL_NUMBER && ParticleLight.Color.GetMax() <= UE_KINDA_SMALL_NUMBER))
				{
					continue;
				}

				FVector ParticlePosition = Particle.Location;
				FVector Unused(0, 0, 0);

				ApplyOrbitToPosition(Particle, Source, InLocalToWorld, ParticlePosition, Unused);

				FVector LightPosition = Source.bUseLocalSpace ? FVector(InLocalToWorld.TransformPosition(ParticlePosition)) : ParticlePosition;
				
				// Calculate light positions per-view if we are using a camera offset.
				/*	disabling camera offset on lights for now; it's not reliably working and does more harm than good
				if (Source.CameraPayloadOffset != 0)
				{
					//Reserve mem for the indices into the per view data.
					if (InViewFamily.Views.Num() > 1)
					{	
						OutParticleLights.InstancePerViewDataIndices.Reserve(OutParticleLights.InstanceData.Num() + ParticleCount);
						if (InViewFamily.Views.Num() > 1 && OutParticleLights.InstancePerViewDataIndices.Num() == 0)
						{
							//If this is the first time we've needed to add these then we need to fill data up to this light.
							int32 NumLights = OutParticleLights.InstanceData.Num();
							OutParticleLights.InstancePerViewDataIndices.AddUninitialized(OutParticleLights.InstanceData.Num());
							for (int32 LightIndex = 0; LightIndex < NumLights; ++LightIndex)
							{
								OutParticleLights.InstancePerViewDataIndices[LightIndex].PerViewIndex = LightIndex;
								OutParticleLights.InstancePerViewDataIndices[LightIndex].bHasPerViewData = false;
							}
						}

						//If non-zero, the numbers of instances to indices should always match.
						check(OutParticleLights.InstancePerViewDataIndices.Num() == OutParticleLights.InstanceData.Num());
						//Add index of this light into the per veiw data.
						FSimpleLightInstacePerViewIndexData PerViewIndexData;
						PerViewIndexData.bHasPerViewData = true;
						PerViewIndexData.PerViewIndex = OutParticleLights.PerViewData.Num();
						OutParticleLights.InstancePerViewDataIndices.Add(PerViewIndexData);
					}

					for(int32 ViewIndex = 0; ViewIndex < InViewFamily.Views.Num(); ++ViewIndex)
					{
						FSimpleLightPerViewEntry PerViewData;
						const FVector& ViewOrigin = InViewFamily.Views[ViewIndex]->ViewMatrices.ViewOrigin;
						FVector CameraOffset = GetCameraOffsetFromPayload(Source.CameraPayloadOffset, Particle, ParticlePosition, ViewOrigin);
						PerViewData.Position = LightPosition + CameraOffset;

						OutParticleLights.PerViewData.Add(PerViewData);
					}
				}
				else*/
				{
					//Add index of this light into the per view data if needed.
					if (OutParticleLights.InstancePerViewDataIndices.Num() != 0)
					{
						//If non-zero, the numbers of instances to indices should always match.
						check(OutParticleLights.InstancePerViewDataIndices.Num() == OutParticleLights.InstanceData.Num());
						FSimpleLightInstacePerViewIndexData PerViewIndexData;
						PerViewIndexData.bHasPerViewData = false;
						PerViewIndexData.PerViewIndex = OutParticleLights.PerViewData.Num();
						OutParticleLights.InstancePerViewDataIndices.Add(PerViewIndexData);
					}

					// When not using camera-offset, output one position for all views to share. 
					FSimpleLightPerViewEntry PerViewData;
					PerViewData.Position = LightPosition;
					OutParticleLights.PerViewData.Add(PerViewData);
				}

				// Add an entry for the light instance.
				OutParticleLights.InstanceData.Add(ParticleLight);
			}
		}
	}
}

void FDynamicSpriteEmitterData::GatherSimpleLights(const FParticleSystemSceneProxy* Proxy, const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const
{
	GatherParticleLightData(Source, Proxy->GetLocalToWorld(), ViewFamily, OutParticleLights);
}

class FDynamicSpriteCollectorResources : public FOneFrameResource
{
public:
	FDynamicSpriteCollectorResources(ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel)
	{
	}

	virtual ~FDynamicSpriteCollectorResources()
	{
		VertexFactory.ReleaseResource();
	}

	FParticleSpriteVertexFactory VertexFactory;
	FParticleSpriteUniformBufferRef UniformBuffer;
};

void FDynamicSpriteEmitterData::GetDynamicMeshElementsEmitter(const FParticleSystemSceneProxy* Proxy, const FSceneView* View, const FSceneViewFamily& ViewFamily, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_SpriteRenderingTime);

	const auto FeatureLevel = View->GetFeatureLevel();

	// Sort and generate particles for this view.
	const FDynamicSpriteEmitterReplayDataBase* SourceData = GetSourceData();

	if (bValid && SourceData)
	{
		if (SourceData->EmitterRenderMode == ERM_Normal)
		{
			// Determine how many vertices and indices are needed to render.
			int32 ParticleCount = SourceData->ActiveParticleCount;
			const int32 VertexSize = GetDynamicVertexStride(FeatureLevel);
			const int32 DynamicParameterVertexSize = sizeof(FParticleVertexDynamicParameter);
			int32 NumVerticesPerParticle = 4;
			int32 NumTrianglesPerParticle = 2;

			if (SourceData->RequiredModule->bCutoutTexureIsValid)
			{
				NumVerticesPerParticle = SourceData->RequiredModule->NumBoundingVertices;
				NumTrianglesPerParticle = SourceData->RequiredModule->NumBoundingTriangles;
			}

			const int32 NumVerticesPerParticleInBuffer = 1;
			const uint32 InstanceFactor = View->IsInstancedStereoPass() ? 2 : 1;

			check(NumVerticesPerParticle == 4 || NumVerticesPerParticle == 8);
			const FVertexBuffer* TexCoordBuffer = (NumVerticesPerParticle == 4) ? (const FVertexBuffer*)&GParticleTexCoordVertexBuffer : (const FVertexBuffer*)&GParticleEightTexCoordVertexBuffer;

			FDynamicSpriteCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FDynamicSpriteCollectorResources>(FeatureLevel);
			FParticleSpriteVertexFactory* SpriteVertexFactory = &CollectorResources.VertexFactory;
			SpriteVertexFactory->SetParticleFactoryType(PVFT_Sprite);
			SpriteVertexFactory->SetNumVertsInInstanceBuffer(SourceData->RequiredModule->bCutoutTexureIsValid ? SourceData->RequiredModule->NumBoundingVertices : 4);
			SpriteVertexFactory->SetUsesDynamicParameter(bUsesDynamicParameter, bUsesDynamicParameter ? GetDynamicParameterVertexStride() : 0);
			SpriteVertexFactory->InitResource();

			if (SourceData->bUseLocalSpace == false)
			{
				Proxy->UpdateWorldSpacePrimitiveUniformBuffer();
			}

			FGlobalDynamicVertexBuffer& DynamicVertexBuffer = Collector.GetDynamicVertexBuffer();
			FGlobalDynamicVertexBuffer::FAllocation Allocation;
			FGlobalDynamicVertexBuffer::FAllocation DynamicParameterAllocation;

			// Allocate memory for render data.
			Allocation = DynamicVertexBuffer.Allocate( InstanceFactor * ParticleCount * VertexSize * NumVerticesPerParticleInBuffer );
			if (DynamicVertexBuffer.IsRenderAlarmLoggingEnabled())
			{
				UE_LOG(LogParticles, Warning, TEXT("Panic logging.  Allocated %u bytes for Resource: %s, Owner: %s"), InstanceFactor * ParticleCount * VertexSize * NumVerticesPerParticleInBuffer, *Proxy->GetResourceName().ToString(), *Proxy->GetOwnerName().ToString())
			}

			if (bUsesDynamicParameter)
			{
				DynamicParameterAllocation = DynamicVertexBuffer.Allocate( InstanceFactor * ParticleCount * DynamicParameterVertexSize * NumVerticesPerParticleInBuffer );

				if (DynamicVertexBuffer.IsRenderAlarmLoggingEnabled())
				{
					UE_LOG(LogParticles, Warning, TEXT("Panic logging.  Allocated %u bytes for Resource: %s, Owner: %s"), InstanceFactor * ParticleCount * DynamicParameterVertexSize * NumVerticesPerParticleInBuffer, *Proxy->GetResourceName().ToString(), *Proxy->GetOwnerName().ToString())
				}
			}

			if (Allocation.IsValid() && (!bUsesDynamicParameter || DynamicParameterAllocation.IsValid()))
			{
				// Sort the particles if needed.
				bool bSort = false;
				if (SourceData->SortMode != PSORTMODE_None)
				{
					SCOPE_CYCLE_COUNTER(STAT_FDynamicSpriteEmitterData_GetDynamicMeshElementsEmitter_GetParticleOrderData);
					// If material is using unlit translucency and the blend mode is translucent then we need to sort (back to front)
					const FMaterial* Material = MaterialResource ? &MaterialResource->GetIncompleteMaterialWithFallback(FeatureLevel) : nullptr;

					if (Material && 
						(IsTranslucentOnlyBlendMode(*Material) || Material->GetBlendMode() == BLEND_AlphaComposite || IsAlphaHoldoutBlendMode(*Material) ||
						((SourceData->SortMode == PSORTMODE_Age_OldestFirst) || (SourceData->SortMode == PSORTMODE_Age_NewestFirst)))
						)
					{
						bSort = true;
					}
				}
				{
					SCOPE_CYCLE_COUNTER(STAT_FDynamicSpriteEmitterData_PerParticleWorkOrTasks);
					if (Collector.ShouldUseTasks())
					{
						Collector.AddTask(
							[this, SourceData, View, Proxy, Allocation, DynamicParameterAllocation, bSort, ParticleCount, NumVerticesPerParticleInBuffer, InstanceFactor]()
							{
								SCOPE_CYCLE_COUNTER(STAT_FDynamicSpriteEmitterData_GetDynamicMeshElementsEmitter_Task);
								SCOPE_CYCLE_COUNTER(STAT_ParticlesOverview_RT_CNC);

								FMemMark Mark(FMemStack::Get());
								FParticleOrder* ParticleOrder = NULL;
								if (bSort)
								{
									ParticleOrder = (FParticleOrder*)FMemStack::Get().Alloc(sizeof(FParticleOrder)* ParticleCount, alignof(FParticleOrder));
									SortSpriteParticles(SourceData->SortMode, SourceData->bUseLocalSpace, SourceData->ActiveParticleCount, 
										SourceData->DataContainer.ParticleData, SourceData->ParticleStride, SourceData->DataContainer.ParticleIndices,
										View, Proxy->GetLocalToWorld(), ParticleOrder);
								}
								// Fill vertex buffers.
								GetVertexAndIndexData(Allocation.Buffer, DynamicParameterAllocation.Buffer, NULL, ParticleOrder, View->ViewMatrices.GetViewOrigin(), Proxy->GetLocalToWorld(), InstanceFactor);
							}
						);
					}
					else
					{
						FParticleOrder* ParticleOrder = NULL;

						if (bSort)
						{
							ParticleOrder = GParticleOrderPool.GetParticleOrderData(ParticleCount);
							SortSpriteParticles(SourceData->SortMode, SourceData->bUseLocalSpace, SourceData->ActiveParticleCount, 
								SourceData->DataContainer.ParticleData, SourceData->ParticleStride, SourceData->DataContainer.ParticleIndices,
								View, Proxy->GetLocalToWorld(), ParticleOrder);
						}

						// Fill vertex buffers.
						GetVertexAndIndexData(Allocation.Buffer, DynamicParameterAllocation.Buffer, NULL, ParticleOrder, View->ViewMatrices.GetViewOrigin(), Proxy->GetLocalToWorld(), InstanceFactor);
					}
				}


				// Create per-view uniform buffer.
				FParticleSpriteUniformParameters PerViewUniformParameters = UniformParameters;
				FVector2D ObjectNDCPosition;
				FVector2D ObjectMacroUVScales;
					Proxy->GetObjectPositionAndScale(*View, ObjectNDCPosition, ObjectMacroUVScales);
				PerViewUniformParameters.MacroUVParameters = FVector4f(ObjectNDCPosition.X, ObjectNDCPosition.Y, ObjectMacroUVScales.X, ObjectMacroUVScales.Y);
				CollectorResources.UniformBuffer = FParticleSpriteUniformBufferRef::CreateUniformBufferImmediate(PerViewUniformParameters, UniformBuffer_SingleFrame);

				// Set the sprite uniform buffer for this view.
				SpriteVertexFactory->SetSpriteUniformBuffer(CollectorResources.UniformBuffer);
#if PLATFORM_SWITCH
				// use the full vertex for non-instancing case, and the "4 float" instance data for the instanced case
				uint32 InstanceBufferStride = ((sizeof(float) * 4) * NumVerticesPerParticle);
#else
				uint32 InstanceBufferStride = VertexSize;
#endif
				SpriteVertexFactory->SetInstanceBuffer(Allocation.VertexBuffer, Allocation.VertexOffset, InstanceBufferStride);
				SpriteVertexFactory->SetDynamicParameterBuffer(DynamicParameterAllocation.VertexBuffer, DynamicParameterAllocation.VertexOffset, GetDynamicParameterVertexStride());

				if (SourceData->RequiredModule->bCutoutTexureIsValid)
				{
					SpriteVertexFactory->SetCutoutParameters(SourceData->RequiredModule->NumBoundingVertices, SourceData->RequiredModule->BoundingGeometryBufferSRV);
				}

				SpriteVertexFactory->SetTexCoordBuffer(TexCoordBuffer);
			}

			// Don't render if the material will be ignored
			const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

			// Calculate the number of particles that must be drawn.
			ParticleCount = Source.ActiveParticleCount;
			if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
			{
				ParticleCount = Source.MaxDrawCount;
			}

			// Construct the mesh element to render.
			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& BatchElement = Mesh.Elements[0];
			check(NumTrianglesPerParticle == 2 || NumTrianglesPerParticle == 6);
			BatchElement.IndexBuffer = NumTrianglesPerParticle == 2 ? (const FIndexBuffer*)&GParticleIndexBuffer : (const FIndexBuffer*)&GSixTriangleParticleIndexBuffer;
			BatchElement.NumPrimitives = NumTrianglesPerParticle;
			BatchElement.NumInstances = ParticleCount;
			BatchElement.FirstIndex = 0;
			Mesh.VertexFactory = SpriteVertexFactory;
			// if the particle rendering data is presupplied, use it directly
			Mesh.LCI = NULL;
			if (SourceData->bUseLocalSpace == true)
			{
				BatchElement.PrimitiveUniformBuffer = Proxy->GetUniformBuffer();
			}
			else
			{
				BatchElement.PrimitiveUniformBuffer = Proxy->GetWorldSpacePrimitiveUniformBuffer();
			}
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = (ParticleCount * NumVerticesPerParticle) - 1;
			Mesh.CastShadow = Proxy->GetCastShadow();
			Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)Proxy->GetDepthPriorityGroup(View);

			if ( bIsWireframe )
			{
				Mesh.MaterialRenderProxy = UMaterial::GetDefaultMaterial( MD_Surface )->GetRenderProxy();
			}
			else
			{
				Mesh.MaterialRenderProxy = MaterialResource;
			}
			Mesh.Type = PT_TriangleList;

			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.bUseWireframeSelectionColoring = Proxy->IsSelected();
			
		#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			Mesh.VisualizeLODIndex = (int8)Proxy->GetVisualizeLODIndex();
		#endif

			Collector.AddMesh(ViewIndex, Mesh);
		}
		else if (SourceData->EmitterRenderMode == ERM_Point)
		{
			RenderDebug(Proxy, Collector.GetPDI(ViewIndex), View, false);
		}
		else if (SourceData->EmitterRenderMode == ERM_Cross)
		{
			RenderDebug(Proxy, Collector.GetPDI(ViewIndex), View, true);
		}
	}
}

void FDynamicSpriteEmitterData::UpdateRenderThreadResourcesEmitter(const FParticleSystemSceneProxy* InOwnerProxy)
{
	// Generate the uniform buffer.
	const FDynamicSpriteEmitterReplayDataBase* SourceData = GetSourceData();
	if( SourceData )
	{
		UniformParameters.AxisLockRight = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		UniformParameters.AxisLockUp = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		UniformParameters.RotationScale = 1.0f;
		UniformParameters.RotationBias = 0.0f;
		UniformParameters.TangentSelector = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		UniformParameters.InvDeltaSeconds = SourceData->InvDeltaSeconds;
		UniformParameters.LWCTile = SourceData->LWCTile;

		// Parameters for computing sprite tangents.
		const FMatrix& LocalToWorld = InOwnerProxy->GetLocalToWorld();
		const EParticleAxisLock LockAxisFlag = (EParticleAxisLock)SourceData->LockAxisFlag;
		const bool bRotationLock = (LockAxisFlag >= EPAL_ROTATE_X) && (LockAxisFlag <= EPAL_ROTATE_Z);
		if ( SourceData->ScreenAlignment == PSA_Velocity )
		{
			// No rotation for PSA_Velocity.
			UniformParameters.RotationScale = 0.0f;
			UniformParameters.TangentSelector.Y = 1.0f;
		}
		else if (LockAxisFlag == EPAL_NONE)
		{
			if ( SourceData->ScreenAlignment == PSA_FacingCameraPosition)
			{
				UniformParameters.TangentSelector.W = 1.0f;
			}
			else
			{						
				UniformParameters.TangentSelector.X = 1.0f;
			}
		}
		else
		{
			FVector AxisLockUp, AxisLockRight;
			const FMatrix& AxisLocalToWorld = SourceData->bUseLocalSpace ? LocalToWorld : FMatrix::Identity;
			ComputeLockedAxes( LockAxisFlag, AxisLocalToWorld, AxisLockUp, AxisLockRight );

			UniformParameters.AxisLockRight = (FVector3f)AxisLockRight; // LWC_TODO: precision loss
			UniformParameters.AxisLockRight.W = 1.0f;
			UniformParameters.AxisLockUp = (FVector3f)AxisLockUp; // LWC_TODO: precision loss
			UniformParameters.AxisLockUp.W = 1.0f;

			if ( bRotationLock )
			{
				UniformParameters.TangentSelector.Z = 1.0f;
			}
			else
			{
				UniformParameters.TangentSelector.X = 1.0f;
			}

			// For locked rotation about Z the particle should be rotated by 90 degrees.
			UniformParameters.RotationBias = (LockAxisFlag == EPAL_ROTATE_Z) ? (0.5f * UE_PI) : 0.0f;
		}

		// Alignment overrides
		UniformParameters.RemoveHMDRoll = SourceData->bRemoveHMDRoll ? 1.f : 0.f;

		if (SourceData->ScreenAlignment == PSA_FacingCameraDistanceBlend)
		{
			float DistanceBlendMinSq = SourceData->MinFacingCameraBlendDistance * SourceData->MinFacingCameraBlendDistance;
			float DistanceBlendMaxSq = SourceData->MaxFacingCameraBlendDistance * SourceData->MaxFacingCameraBlendDistance;
			float InvBlendRange = 1.f / FMath::Max(DistanceBlendMaxSq - DistanceBlendMinSq, 1.f);
			float BlendScaledMinDistace = DistanceBlendMinSq * InvBlendRange;

			UniformParameters.CameraFacingBlend.X = 1.f;
			UniformParameters.CameraFacingBlend.Y = InvBlendRange;
			UniformParameters.CameraFacingBlend.Z = BlendScaledMinDistace;

			// Treat as camera facing if needed
			UniformParameters.TangentSelector.W = 1.f;
		}
		else
		{
			UniformParameters.CameraFacingBlend.X = 0.f;
			UniformParameters.CameraFacingBlend.Y = 0.f;
			UniformParameters.CameraFacingBlend.Z = 0.f;
		}	

		// SubUV information.
		UniformParameters.SubImageSize = FVector4f(
			SourceData->SubImages_Horizontal,
			SourceData->SubImages_Vertical,
			1.0f / SourceData->SubImages_Horizontal,
			1.0f / SourceData->SubImages_Vertical );

		const EEmitterNormalsMode NormalsMode = (EEmitterNormalsMode)SourceData->EmitterNormalsMode;
		UniformParameters.NormalsType = NormalsMode;
		UniformParameters.NormalsSphereCenter = FVector3f::ZeroVector;
		UniformParameters.NormalsCylinderUnitDirection = FVector3f(0.0f,0.0f,1.0f);

		if (NormalsMode != ENM_CameraFacing)
		{
			UniformParameters.NormalsSphereCenter = (FVector4f)LocalToWorld.TransformPosition((FVector)SourceData->NormalsSphereCenter); // LWC_TODO: Precision loss
			if (NormalsMode == ENM_Cylindrical)
			{
				UniformParameters.NormalsCylinderUnitDirection = (FVector4f)LocalToWorld.TransformVector((FVector)SourceData->NormalsCylinderDirection); // LWC_TODO: Precision loss
			}
		}

		UniformParameters.PivotOffset = SourceData->PivotOffset;
	}
}

///////////////////////////////////////////////////////////////////////////////
//	FDynamicMeshEmitterData
///////////////////////////////////////////////////////////////////////////////

FDynamicMeshEmitterData::FDynamicMeshEmitterData(const UParticleModuleRequired* RequiredModule)
	: FDynamicSpriteEmitterDataBase(RequiredModule)
	, LastFramePreRendered(-1)
	, StaticMesh( NULL )
	, MeshTypeDataOffset(0xFFFFFFFF)
	, bApplyPreRotation(false)
	, bUseMeshLockedAxis(false)
	, bUseCameraFacing(false)
	, bApplyParticleRotationAsSpin(false)
	, bFaceCameraDirectionRatherThanPosition(false)
	, CameraFacingOption(0)
	, LastCalculatedMeshLOD(0)
	, EmitterInstance(nullptr)
{
	// only update motion blur transforms if we are not paused
	// bPlayersOnlyPending allows us to keep the particle transforms 
	// from the last ticked frame
}

FDynamicMeshEmitterData::~FDynamicMeshEmitterData()
{
}

/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicMeshEmitterData::Init( bool bInSelected,
									const FParticleMeshEmitterInstance* InEmitterInstance,
									UStaticMesh* InStaticMesh,
									bool InUseStaticMeshLODs,
									float InLODSizeScale,
									ERHIFeatureLevel::Type InFeatureLevel )
{
	EmitterInstance = InEmitterInstance;

	// @todo: For replays, currently we're assuming the original emitter instance is bound to the same mesh as
	//        when the replay was generated (safe), and various mesh/material indices are intact.  If
	//        we ever support swapping meshes/material on the fly, we'll need cache the mesh
	//        reference and mesh component/material indices in the actual replay data.

	StaticMesh = InStaticMesh;
	bUseStaticMeshLODs = InUseStaticMeshLODs;
	LODSizeScale = InLODSizeScale;
	check(StaticMesh);
	check(Source.ParticleStride < 2 * 1024);	// TTP #3375

	TArray<UMaterialInterface*, TInlineAllocator<2> > MeshMaterialsGT;
	InEmitterInstance->GetMeshMaterials(
		MeshMaterialsGT,
		InEmitterInstance->SpriteTemplate->LODLevels[InEmitterInstance->CurrentLODLevelIndex],
		InFeatureLevel);	

	for (int32 i = 0; i < MeshMaterialsGT.Num(); ++i)
	{
		UMaterialInterface* RenderMaterial = MeshMaterialsGT[i];
		if (RenderMaterial == NULL  || (RenderMaterial->CheckMaterialUsage_Concurrent(MATUSAGE_MeshParticles) == false))
		{
			MeshMaterialsGT[i] = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	MeshMaterials.AddZeroed(MeshMaterialsGT.Num());
	for (int32 i = 0; i < MeshMaterialsGT.Num(); ++i)
	{
		MeshMaterials[i] = MeshMaterialsGT[i]->GetRenderProxy();
	}

	bUsesDynamicParameter = GetSourceData()->DynamicParameterDataOffset > 0;

	// Find the offset to the mesh type data 
	if (InEmitterInstance->MeshTypeData != NULL)
	{
		UParticleModuleTypeDataMesh* MeshTD = InEmitterInstance->MeshTypeData;
		// offset to the mesh emitter type data
		MeshTypeDataOffset = InEmitterInstance->TypeDataOffset;

		FVector Mins, Maxs;
		MeshTD->RollPitchYawRange.GetRange(Mins, Maxs);

		// Enable/Disable pre-rotation
		if (Mins.SizeSquared() || Maxs.SizeSquared())
		{
			bApplyPreRotation = true;
		}
		else
		{
			bApplyPreRotation = false;
		}

		// Setup the camera facing options
		if (MeshTD->bCameraFacing == true)
		{
			bUseCameraFacing = true;
			CameraFacingOption = MeshTD->CameraFacingOption;
			bApplyParticleRotationAsSpin = MeshTD->bApplyParticleRotationAsSpin;
			bFaceCameraDirectionRatherThanPosition = MeshTD->bFaceCameraDirectionRatherThanPosition;
		}
		else
		{
			bUseCameraFacing = false;
			CameraFacingOption = 0;
			bApplyParticleRotationAsSpin = false;
			bFaceCameraDirectionRatherThanPosition = false;
		}

		// Camera facing trumps locked axis... but can still use it.
		// Setup the locked axis option
		uint8 CheckAxisLockOption = MeshTD->AxisLockOption;
		if ((CheckAxisLockOption >= EPAL_X) && (CheckAxisLockOption <= EPAL_NEGATIVE_Z))
		{
			bUseMeshLockedAxis = true;
			Source.LockedAxis = FVector3f(
				(CheckAxisLockOption == EPAL_X) ? 1.0f : ((CheckAxisLockOption == EPAL_NEGATIVE_X) ? -1.0f :  0.0),
				(CheckAxisLockOption == EPAL_Y) ? 1.0f : ((CheckAxisLockOption == EPAL_NEGATIVE_Y) ? -1.0f :  0.0),
				(CheckAxisLockOption == EPAL_Z) ? 1.0f : ((CheckAxisLockOption == EPAL_NEGATIVE_Z) ? -1.0f :  0.0)
				);
		}
		else if ((CameraFacingOption >= LockedAxis_ZAxisFacing) && (CameraFacingOption <= LockedAxis_NegativeYAxisFacing))
		{
			// Catch the case where we NEED locked axis...
			bUseMeshLockedAxis = true;
			Source.LockedAxis = FVector3f(1.0f, 0.0f, 0.0f);
		}
	}

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}

void FDynamicMeshEmitterData::UpdateRenderThreadResourcesEmitter(const FParticleSystemSceneProxy* InOwnerProxy)
{
}

/**
 *	Release the render thread resources for this emitter data
 *
 *	@param	InOwnerProxy	The proxy that owns this dynamic emitter data
 *
 *	@return	bool			true if successful, false if failed
 */
void FDynamicMeshEmitterData::ReleaseRenderThreadResources(const FParticleSystemSceneProxy* InOwnerProxy)
{
	return FDynamicSpriteEmitterDataBase::ReleaseRenderThreadResources(InOwnerProxy);
}

class FDynamicMeshEmitterCollectorResources : public FOneFrameResource
{
public:
	FDynamicMeshEmitterCollectorResources(ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel)
	{
	}

	virtual ~FDynamicMeshEmitterCollectorResources()
	{
		VertexFactory.ReleaseResource();
	}

	FMeshParticleVertexFactory VertexFactory;
	FMeshParticleUniformBufferRef UniformBuffer;
};

class FMeshParticleInstanceVertices : public FOneFrameResource
{
public:
	TArray<FMeshParticleInstanceVertex, SceneRenderingAllocator> InstanceDataAllocationsCPU;
	TArray<FMeshParticleInstanceVertexDynamicParameter, SceneRenderingAllocator> DynamicParameterDataAllocationsCPU;
	TArray<FMeshParticleInstanceVertexPrevTransform, SceneRenderingAllocator> PrevTransformDataAllocationsCPU;
};

uint32 FDynamicMeshEmitterData::GetMeshLODIndexFromProxy(const FParticleSystemSceneProxy *InOwnerProxy) const
{
	// Determine first available LOD level, top level can be stripped per platform
	check(IsInRenderingThread());
	int32 FirstAvailableLOD = StaticMesh->GetRenderData()->CurrentFirstLODIdx;
	for (; FirstAvailableLOD < StaticMesh->GetRenderData()->LODResources.Num(); FirstAvailableLOD++)
	{
		if (StaticMesh->GetRenderData()->LODResources[FirstAvailableLOD].GetNumVertices() > 0)
		{
			break;
		}
	}

	if (!InOwnerProxy || !GFXAllowParticleMeshLODs)
	{
		return FirstAvailableLOD;
	}
	const int32 EffectiveMinLOD = StaticMesh->GetMinLODIdx();
	const int32 MaxLOD = StaticMesh->GetRenderData()->LODResources.Num() - 1;
	const int32 ClampedMinLOD = FMath::Clamp(EffectiveMinLOD, FirstAvailableLOD, MaxLOD);
	const int32 MeshLOD = (InOwnerProxy->MeshEmitterLODIndices.IsValidIndex(EmitterIndex)) ? InOwnerProxy->MeshEmitterLODIndices[EmitterIndex] : 0;
	return FMath::Clamp(MeshLOD, ClampedMinLOD, MaxLOD);
}

void FDynamicMeshEmitterData::GetDynamicMeshElementsEmitter(const FParticleSystemSceneProxy* Proxy, const FSceneView* View, const FSceneViewFamily& ViewFamily, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_MeshRenderingTime);

	if (bValid)
	{
		if (Source.EmitterRenderMode == ERM_Normal)
		{
			const auto FeatureLevel = ViewFamily.GetFeatureLevel();
			const auto ShaderPlatform = GShaderPlatformForFeatureLevel[FeatureLevel];

			int32 ParticleCount = Source.ActiveParticleCount;
			if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
			{
				ParticleCount = Source.MaxDrawCount;
			}

			const int32 InstanceVertexStride = GetDynamicVertexStride(FeatureLevel);
			const int32 DynamicParameterVertexStride  = GetDynamicParameterVertexStride();

			// Setup the vertex factory.
			FMeshParticleInstanceVertices* InstanceVerticesCPU = nullptr;

			check(StaticMesh != nullptr);

			const uint32 ChosenLODIdx = GetMeshLODIndexFromProxy(Proxy);
			const FStaticMeshLODResources& LODModel = StaticMesh->GetRenderData()->LODResources[ChosenLODIdx];

			FDynamicMeshEmitterCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FDynamicMeshEmitterCollectorResources>(FeatureLevel);
			FMeshParticleVertexFactory* MeshVertexFactory = &CollectorResources.VertexFactory;
			SetupVertexFactory(MeshVertexFactory, LODModel, ChosenLODIdx);

			MeshVertexFactory->SetStrides(InstanceVertexStride, bUsesDynamicParameter ? DynamicParameterVertexStride : 0);
			MeshVertexFactory->InitResource();

			const FDynamicSpriteEmitterReplayDataBase* SourceData = GetSourceData();
			FMeshParticleUniformParameters UniformParameters;
			UniformParameters.SubImageSize = FVector4f(
				1.0f / (SourceData ? SourceData->SubImages_Horizontal : 1),
				1.0f / (SourceData ? SourceData->SubImages_Vertical : 1),
				0, 0);

			// A weight is used to determine whether the mesh texture coordinates or SubUVs are passed from the vertex shader to the pixel shader.
			const uint32 TexCoordWeight = (SourceData && SourceData->SubUVDataOffset > 0) ? 1 : 0;
			UniformParameters.TexCoordWeightA = TexCoordWeight;
			UniformParameters.TexCoordWeightB = 1 - TexCoordWeight;
			UniformParameters.PrevTransformAvailable = Source.MeshMotionBlurOffset ? 1 : 0;
			UniformParameters.bUseLocalSpace = Source.bUseLocalSpace;
			UniformParameters.LWCTile = Source.LWCTile;

			CollectorResources.UniformBuffer = FMeshParticleUniformBufferRef::CreateUniformBufferImmediate(UniformParameters, UniformBuffer_MultiFrame);
			MeshVertexFactory->SetUniformBuffer(CollectorResources.UniformBuffer);

			// For OpenGL & Metal we can't assume that it is OK to leave the PrevTransformBuffer buffer unbound.
			// Doing so can lead to undefined behaviour if the buffer is referenced in the shader even if protected by a branch that is not meant to be taken.
			bool const bGeneratePrevTransformBuffer = (FeatureLevel >= ERHIFeatureLevel::SM5) && 
														  (Source.MeshMotionBlurOffset || IsOpenGLPlatform(ShaderPlatform) || IsMetalPlatform(ShaderPlatform) || FDataDrivenShaderPlatformInfo::GetRequiresGeneratePrevTransformBuffer(ShaderPlatform));


			const uint32 InstanceFactor = View->IsInstancedStereoPass() ? 2 : 1;
			FGlobalDynamicVertexBuffer& DynamicVertexBuffer = Collector.GetDynamicVertexBuffer();
			FGlobalDynamicVertexBuffer::FAllocation Allocation = DynamicVertexBuffer.Allocate(InstanceFactor * ParticleCount * InstanceVertexStride);
			FGlobalDynamicVertexBuffer::FAllocation DynamicParameterAllocation;
			uint8* PrevTransformBuffer = nullptr;

			if (bUsesDynamicParameter)
			{
				DynamicParameterAllocation = DynamicVertexBuffer.Allocate(InstanceFactor * ParticleCount * DynamicParameterVertexStride);

				if (DynamicVertexBuffer.IsRenderAlarmLoggingEnabled())
				{
					UE_LOG(LogParticles, Warning, TEXT("Panic logging.  Allocated %u bytes for Resource: %s, Owner: %s"), InstanceFactor * ParticleCount * DynamicParameterVertexStride, *Proxy->GetResourceName().ToString(), *Proxy->GetOwnerName().ToString());
				}
			}

			if (bGeneratePrevTransformBuffer)
			{
				PrevTransformBuffer = MeshVertexFactory->LockPreviousTransformBuffer(ParticleCount);
			}
				
			// todo: mobile Note hat if the allocation fails, PrevTransformBuffer SRV buffer wont be filled. Assuming this is ok since there is nothing to draw at that point.
			if (PrevTransformBuffer && !Source.MeshMotionBlurOffset)
			{
				SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);
				int32 ActiveParticleCount = Source.ActiveParticleCount;
				if ((Source.MaxDrawCount >= 0) && (ActiveParticleCount > Source.MaxDrawCount))
				{
					ActiveParticleCount = Source.MaxDrawCount;
				}
					
				int32 PrevTransformVertexStride = sizeof(FVector4f) * 3;
					
				uint8* TempPrevTranformVert = (uint8*)PrevTransformBuffer;

				for (int32 i = ActiveParticleCount - 1; i >= 0; i--)
				{
					FVector4f* PrevTransformVertex = (FVector4f*)TempPrevTranformVert;
						
					const int32	CurrentIndex = Source.DataContainer.ParticleIndices[i];
					const uint8* ParticleBase = Source.DataContainer.ParticleData + CurrentIndex * Source.ParticleStride;
					const FBaseParticle& Particle = *((const FBaseParticle*)ParticleBase);

					// Instance to world transformation. Translation (Instance world position) is packed into W
					FMatrix TransMat(FMatrix::Identity);
					GetParticleTransform(Particle, Proxy, View, TransMat);
					// Transpose on CPU to allow for simpler shader code to perform the transform.
					const FMatrix Transpose = TransMat.GetTransposed();
						
					PrevTransformVertex[0] = FVector4f(Transpose.M[0][0], Transpose.M[0][1], Transpose.M[0][2], Transpose.M[0][3]);
					PrevTransformVertex[1] = FVector4f(Transpose.M[1][0], Transpose.M[1][1], Transpose.M[1][2], Transpose.M[1][3]);
					PrevTransformVertex[2] = FVector4f(Transpose.M[2][0], Transpose.M[2][1], Transpose.M[2][2], Transpose.M[2][3]);
						
					TempPrevTranformVert += PrevTransformVertexStride;
				}
					
				PrevTransformBuffer = nullptr;
			}

			if (Allocation.IsValid() && (!bUsesDynamicParameter || DynamicParameterAllocation.IsValid()))
			{
				// Fill instance buffer.
				if (Collector.ShouldUseTasks())
				{
					Collector.AddTask(
						[this, View, Proxy, Allocation, DynamicParameterAllocation, PrevTransformBuffer, InstanceFactor]()
						{
							GetInstanceData(Allocation.Buffer, DynamicParameterAllocation.Buffer, PrevTransformBuffer, Proxy, View, InstanceFactor);
						}
					);
				}
				else
				{
					GetInstanceData(Allocation.Buffer, DynamicParameterAllocation.Buffer, PrevTransformBuffer, Proxy, View, InstanceFactor);
				}
			}

			if (bGeneratePrevTransformBuffer)
			{
				MeshVertexFactory->UnlockPreviousTransformBuffer();
			}

			MeshVertexFactory->SetInstanceBuffer(Allocation.VertexBuffer, Allocation.VertexOffset, InstanceVertexStride);
			MeshVertexFactory->SetDynamicParameterBuffer(DynamicParameterAllocation.VertexBuffer, DynamicParameterAllocation.VertexOffset, GetDynamicParameterVertexStride());

			Proxy->UpdateWorldSpacePrimitiveUniformBuffer();
			MeshVertexFactory->GetInstanceVerticesCPU() = InstanceVerticesCPU;

			const bool bIsWireframe = AllowDebugViewmodes() && View->Family->EngineShowFlags.Wireframe;

			//@todo. Handle LODs.
			for (int32 LODIndex = 0; LODIndex < 1; LODIndex++)
			{
				uint32 TotalTriangles = 0;
				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					FMaterialRenderProxy* MaterialProxy = nullptr;
					if (SectionIndex < MeshMaterials.Num() && MeshMaterials[SectionIndex])
					{
						MaterialProxy = MeshMaterials[SectionIndex];
					}
					const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

					if (Section.NumTriangles == 0)
					{
						//@todo. This should never occur, but it does occasionally.
						UE_LOG(LogParticles, Warning, TEXT("Mesh section %i has 0 triangles (LOD %i)"), SectionIndex, GetMeshLODIndexFromProxy(Proxy));
						continue;
					}

					if (MaterialProxy == nullptr)
					{
						UE_LOG(LogParticles, Warning, TEXT("Mesh section %i has a null material proxy (LOD %i)"), SectionIndex, GetMeshLODIndexFromProxy(Proxy));
						continue;
					}

					TotalTriangles += Section.NumTriangles;

					FMeshBatch& Mesh = Collector.AllocateMesh();
					Mesh.VertexFactory = MeshVertexFactory;
					Mesh.LCI = NULL;
					Mesh.ReverseCulling = Proxy->IsLocalToWorldDeterminantNegative();
					Mesh.CastShadow = Proxy->GetCastShadow();
					Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)Proxy->GetDepthPriorityGroup(View);

					FMeshBatchElement& BatchElement = Mesh.Elements[0];
					BatchElement.PrimitiveUniformBuffer = ((SourceData == nullptr) || SourceData->bUseLocalSpace) ? Proxy->GetUniformBuffer() : Proxy->GetWorldSpacePrimitiveUniformBuffer();
					BatchElement.FirstIndex = Section.FirstIndex;
					BatchElement.MinVertexIndex = Section.MinVertexIndex;
					BatchElement.MaxVertexIndex = Section.MaxVertexIndex;
					BatchElement.NumInstances = ParticleCount;

					if (bIsWireframe)
					{
						if (LODModel.AdditionalIndexBuffers && LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.IsInitialized())
						{
							Mesh.Type = PT_LineList;
							Mesh.MaterialRenderProxy = Proxy->GetDeselectedWireframeMatInst();
							BatchElement.FirstIndex = 0;
							BatchElement.IndexBuffer = &LODModel.AdditionalIndexBuffers->WireframeIndexBuffer;
							BatchElement.NumPrimitives = LODModel.AdditionalIndexBuffers->WireframeIndexBuffer.GetNumIndices() / 2;

						}
						else
						{
							Mesh.Type = PT_TriangleList;
							Mesh.MaterialRenderProxy = MaterialProxy;
							Mesh.bWireframe = true;
							BatchElement.FirstIndex = 0;
							BatchElement.IndexBuffer = &LODModel.IndexBuffer;
							BatchElement.NumPrimitives = LODModel.IndexBuffer.GetNumIndices() / 3;
						}
					}
					else
					{
						Mesh.Type = PT_TriangleList;
						Mesh.MaterialRenderProxy = MaterialProxy;
						BatchElement.IndexBuffer = &LODModel.IndexBuffer;
						BatchElement.FirstIndex = Section.FirstIndex;
						BatchElement.NumPrimitives = Section.NumTriangles;
					}

					Mesh.bCanApplyViewModeOverrides = true;
					Mesh.bUseWireframeSelectionColoring = Proxy->IsSelected();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					Mesh.VisualizeLODIndex = (int8)Proxy->GetVisualizeLODIndex();
#endif

					Collector.AddMesh(ViewIndex, Mesh);

					INC_DWORD_STAT_BY(STAT_MeshParticlePolys, ParticleCount * LODModel.GetNumTriangles());
				}

				if (TotalTriangles == 0)
				{
					UE_LOG(LogParticles, Warning, TEXT("WARNING: particle mesh LOD %i has no triangles, but is reported as a valid LOD!"), GetMeshLODIndexFromProxy(Proxy));
				}
			}
		}
		else if (Source.EmitterRenderMode == ERM_Point)
		{
			RenderDebug(Proxy, Collector.GetPDI(ViewIndex), View, false);
		}
		else if (Source.EmitterRenderMode == ERM_Cross)
		{
			RenderDebug(Proxy, Collector.GetPDI(ViewIndex), View, true);
		}
	}

	if (bUseStaticMeshLODs)
	{
		if (EmitterIndex < Proxy->MeshEmitterLODIndices.Num())
		{
			Proxy->MeshEmitterLODIndices[EmitterIndex] = LastCalculatedMeshLOD;
		}
	}
}

void FDynamicMeshEmitterData::GatherSimpleLights(const FParticleSystemSceneProxy* Proxy, const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const
{
	GatherParticleLightData(Source, Proxy->GetLocalToWorld(), ViewFamily, OutParticleLights);
}

void FDynamicMeshEmitterData::GetParticleTransform(
	const FBaseParticle& InParticle,
	const FParticleSystemSceneProxy* Proxy,
	const FSceneView* View,
	FMatrix& OutTransformMat
	) const
{
	const uint8* ParticleBase = (const uint8*)&InParticle;

	const FMeshRotationPayloadData* RotationPayload = (const FMeshRotationPayloadData*)((const uint8*)&InParticle + Source.MeshRotationOffset);
	FVector RotationPayloadInitialOrientation(RotationPayload->InitialOrientation);
	FVector RotationPayloadRotation(RotationPayload->Rotation);

	FVector CameraPayloadCameraOffset = FVector::ZeroVector;
	if (Source.CameraPayloadOffset != 0)
	{
		// Put the camera origin in the appropriate coordinate space.
		FVector CameraPosition = View->ViewMatrices.GetViewOrigin();
		if (Source.bUseLocalSpace)
		{
			const FMatrix InvLocalToWorld = Proxy->GetLocalToWorld().Inverse();
			CameraPosition = InvLocalToWorld.TransformPosition(CameraPosition);
		}

		CameraPayloadCameraOffset = GetCameraOffsetFromPayload(Source.CameraPayloadOffset, InParticle, InParticle.Location, CameraPosition);
	}

	FVector OrbitPayloadOrbitOffset = FVector::ZeroVector;
	if (Source.OrbitModuleOffset != 0)
	{
		int32 CurrentOffset = Source.OrbitModuleOffset;
		PARTICLE_ELEMENT(FOrbitChainModuleInstancePayload, OrbitPayload);
		OrbitPayloadOrbitOffset = (FVector)OrbitPayload.Offset;
	}

	CalculateParticleTransform(
		Proxy->GetLocalToWorld(),
		InParticle.Location,
		InParticle.Rotation,
		InParticle.Velocity,
		InParticle.Size,
		(FVector3f)RotationPayloadInitialOrientation,
		(FVector3f)RotationPayloadRotation,
		CameraPayloadCameraOffset,
		(FVector3f)OrbitPayloadOrbitOffset,
		View->ViewMatrices.GetViewOrigin(),
		(FVector3f)View->GetViewDirection(),
		OutTransformMat
		);
}

void FDynamicMeshEmitterData::GetParticlePrevTransform(
	const FBaseParticle& InParticle,
	const FParticleSystemSceneProxy* Proxy,
	const FSceneView* View,
	FMatrix& OutTransformMat
	) const
{
	const FMeshRotationPayloadData* RotationPayload = (const FMeshRotationPayloadData*)((const uint8*)&InParticle + Source.MeshRotationOffset);
	const FMeshMotionBlurPayloadData* MotionBlurPayload = (const FMeshMotionBlurPayloadData*)((const uint8*)&InParticle + Source.MeshMotionBlurOffset);

	const auto* ViewInfo = static_cast<const FViewInfo*>(View);

	FVector CameraPayloadCameraOffset = FVector::ZeroVector;
	if (Source.CameraPayloadOffset != 0)
	{
		// Put the camera origin in the appropriate coordinate space.
		FVector CameraPosition = ViewInfo->PrevViewInfo.ViewMatrices.GetViewOrigin();
		if (Source.bUseLocalSpace)
		{
			const FMatrix InvLocalToWorld = Proxy->GetLocalToWorld().Inverse();
			CameraPosition = InvLocalToWorld.TransformPosition(CameraPosition);
		}

		CameraPayloadCameraOffset = GetCameraOffset(MotionBlurPayload->PayloadPrevCameraOffset, CameraPosition - InParticle.OldLocation);
	}

	FMatrix PreviousLocalToWorld;
	if (!Proxy->GetScene().GetPreviousLocalToWorld(Proxy->GetPrimitiveSceneInfo(), PreviousLocalToWorld))
	{
		PreviousLocalToWorld = Proxy->GetLocalToWorld();
	}

	CalculateParticleTransform(
		PreviousLocalToWorld,
		InParticle.OldLocation,
		MotionBlurPayload->BaseParticlePrevRotation,
		MotionBlurPayload->BaseParticlePrevVelocity,
		MotionBlurPayload->BaseParticlePrevSize,
		RotationPayload->InitialOrientation,
		MotionBlurPayload->PayloadPrevRotation,
		CameraPayloadCameraOffset,
		MotionBlurPayload->PayloadPrevOrbitOffset,
		ViewInfo->PrevViewInfo.ViewMatrices.GetViewOrigin(),
		(FVector3f)ViewInfo->GetPrevViewDirection(),
		OutTransformMat
		);
}

void FDynamicMeshEmitterData::CalculateParticleTransform(
	const FMatrix& ProxyLocalToWorld,
	const FVector& ParticleLocation,
		  float    ParticleRotation,
	const FVector3f& ParticleVelocity,
	const FVector3f& ParticleSize,
	const FVector3f& ParticlePayloadInitialOrientation,
	const FVector3f& ParticlePayloadRotation,
	const FVector& ParticlePayloadCameraOffset,
	const FVector3f& ParticlePayloadOrbitOffset,
	const FVector& ViewOrigin,
	const FVector3f& ViewDirection,
	FMatrix& OutTransformMat
	) const
{
	FVector CameraFacingOpVector = FVector::ZeroVector;
	if (CameraFacingOption != XAxisFacing_NoUp)
	{
		switch (CameraFacingOption)
		{
		case XAxisFacing_ZUp:
			CameraFacingOpVector = FVector(0.0f, 0.0f, 1.0f);
			break;
		case XAxisFacing_NegativeZUp:
			CameraFacingOpVector = FVector(0.0f, 0.0f, -1.0f);
			break;
		case XAxisFacing_YUp:
			CameraFacingOpVector = FVector(0.0f, 1.0f, 0.0f);
			break;
		case XAxisFacing_NegativeYUp:
			CameraFacingOpVector = FVector(0.0f, -1.0f, 0.0f);
			break;
		case LockedAxis_YAxisFacing:
		case VelocityAligned_YAxisFacing:
			CameraFacingOpVector = FVector(0.0f, 1.0f, 0.0f);
			break;
		case LockedAxis_NegativeYAxisFacing:
		case VelocityAligned_NegativeYAxisFacing:
			CameraFacingOpVector = FVector(0.0f, -1.0f, 0.0f);
			break;
		case LockedAxis_ZAxisFacing:
		case VelocityAligned_ZAxisFacing:
			CameraFacingOpVector = FVector(0.0f, 0.0f, 1.0f);
			break;
		case LockedAxis_NegativeZAxisFacing:
		case VelocityAligned_NegativeZAxisFacing:
			CameraFacingOpVector = FVector(0.0f, 0.0f, -1.0f);
			break;
		}
	}

	FQuat PointToLockedAxis;
	if (bUseMeshLockedAxis == true)
	{
		// facing axis is taken to be the local x axis.	
		PointToLockedAxis = FQuat::FindBetweenNormals(FVector(1, 0, 0), (FVector)Source.LockedAxis);
	}

	OutTransformMat = FMatrix::Identity;

	FTranslationMatrix kTransMat(FVector::ZeroVector);
	FScaleMatrix kScaleMat(FVector(1.0f));
	FQuat kLockedAxisQuat = FQuat::Identity;

	FVector ParticlePosition(ParticleLocation + ParticlePayloadCameraOffset);
	kTransMat.M[3][0] = ParticlePosition.X;
	kTransMat.M[3][1] = ParticlePosition.Y;
	kTransMat.M[3][2] = ParticlePosition.Z;

	FVector3f ScaledSize = ParticleSize * Source.Scale;
	kScaleMat.M[0][0] = ScaledSize.X;
	kScaleMat.M[1][1] = ScaledSize.Y;
	kScaleMat.M[2][2] = ScaledSize.Z;

	FMatrix kRotMat(FMatrix::Identity);
	FMatrix LocalToWorld = ProxyLocalToWorld;

	FVector	LocalSpaceFacingAxis;
	FVector	LocalSpaceUpAxis;
	FVector Location;
	FVector	DirToCamera;
	FQuat PointTo = PointToLockedAxis;

	if (bUseCameraFacing)
	{
		Location = ParticlePosition;
		FVector	VelocityDirection(ParticleVelocity);

		if (Source.bUseLocalSpace)
		{
			bool bClearLocal2World = false;

			// Transform the location to world space
			Location = LocalToWorld.TransformPosition(Location);
			if (CameraFacingOption <= XAxisFacing_NegativeYUp)
			{
				bClearLocal2World = true;
			}
			else if (CameraFacingOption >= VelocityAligned_ZAxisFacing)
			{
				bClearLocal2World = true;
				VelocityDirection = LocalToWorld.InverseFast().GetTransposed().TransformVector(VelocityDirection);
			}

			if (bClearLocal2World)
			{
				// Set the translation matrix to the location
				kTransMat.SetOrigin(Location);
				// Set Local2World to identify to remove any rotational information
				LocalToWorld.SetIdentity();
			}
		}
		VelocityDirection.Normalize();

		if (bFaceCameraDirectionRatherThanPosition)
		{
			DirToCamera = (FVector)-ViewDirection;
		}
		else
		{
			DirToCamera = ViewOrigin - Location;
		}

		DirToCamera.Normalize();
		if (DirToCamera.SizeSquared() < 0.5f)
		{
			// Assert possible if DirToCamera is not normalized
			DirToCamera = FVector(1, 0, 0);
		}

		bool bFacingDirectionIsValid = true;
		if (CameraFacingOption != XAxisFacing_NoUp)
		{
			FVector FacingDir;
			FVector DesiredDir;

			if ((CameraFacingOption >= VelocityAligned_ZAxisFacing) &&
				(CameraFacingOption <= VelocityAligned_NegativeYAxisFacing))
			{
				if (VelocityDirection.IsNearlyZero())
				{
					// We have to fudge it
					bFacingDirectionIsValid = false;
				}

				// Velocity align the X-axis, and camera face the selected axis
				PointTo = FQuat::FindBetweenNormals(FVector(1.0f, 0.0f, 0.0f), VelocityDirection);
				FacingDir = VelocityDirection;
				DesiredDir = DirToCamera;
			}
			else if (CameraFacingOption <= XAxisFacing_NegativeYUp)
			{
				// Camera face the X-axis, and point the selected axis towards the world up
				PointTo = FQuat::FindBetweenNormals(FVector(1, 0, 0), DirToCamera);
				FacingDir = DirToCamera;
				DesiredDir = FVector(0, 0, 1);
			}
			else
			{
				// Align the X-axis with the selected LockAxis, and point the selected axis towards the camera
				// PointTo will contain quaternion for locked axis rotation.
				FacingDir = (FVector)Source.LockedAxis;

				if (Source.bUseLocalSpace)
				{
					//Transform the direction vector into local space.
					DesiredDir = LocalToWorld.GetTransposed().TransformVector(DirToCamera);
				}
				else
				{
					DesiredDir = DirToCamera;
				}
			}

			FVector	DirToDesiredInRotationPlane = DesiredDir - ((DesiredDir | FacingDir) * FacingDir);
			DirToDesiredInRotationPlane.Normalize();
			FQuat FacingRotation = FQuat::FindBetweenNormals(PointTo.RotateVector(CameraFacingOpVector), DirToDesiredInRotationPlane);
			PointTo = FacingRotation * PointTo;

			// Add in additional rotation about either the directional or camera facing axis
			if (bApplyParticleRotationAsSpin)
			{
				if (bFacingDirectionIsValid)
				{
					FQuat AddedRotation = FQuat(FacingDir, ParticleRotation);
					kLockedAxisQuat = (AddedRotation * PointTo);
				}
			}
			else
			{
				FQuat AddedRotation = FQuat(DirToCamera, ParticleRotation);
				kLockedAxisQuat = (AddedRotation * PointTo);
			}
		}
		else
		{
			PointTo = FQuat::FindBetweenNormals(FVector(1, 0, 0), DirToCamera);
			// Add in additional rotation about facing axis
			FQuat AddedRotation = FQuat(DirToCamera, ParticleRotation);
			kLockedAxisQuat = (AddedRotation * PointTo);
		}
	}
	else if (bUseMeshLockedAxis)
	{
		// Add any 'sprite rotation' about the locked axis
		FQuat AddedRotation = FQuat((FVector)Source.LockedAxis, ParticleRotation);
		kLockedAxisQuat = (AddedRotation * PointTo);
	}
	else if (Source.ScreenAlignment == PSA_TypeSpecific)
	{
		Location = ParticlePosition;
		if (Source.bUseLocalSpace)
		{
			// Transform the location to world space
			Location = LocalToWorld.TransformPosition(Location);
			kTransMat.SetOrigin(Location);
			LocalToWorld.SetIdentity();
		}

		DirToCamera = ViewOrigin - Location;
		DirToCamera.Normalize();
		if (DirToCamera.SizeSquared() < 0.5f)
		{
			// Assert possible if DirToCamera is not normalized
			DirToCamera = FVector(1, 0, 0);
		}

		LocalSpaceFacingAxis = FVector(1, 0, 0); // facing axis is taken to be the local x axis.	
		LocalSpaceUpAxis = FVector(0, 0, 1); // up axis is taken to be the local z axis

		if (Source.MeshAlignment == PSMA_MeshFaceCameraWithLockedAxis)
		{
			// TODO: Allow an arbitrary	vector to serve	as the locked axis

			// For the locked axis behavior, only rotate to	face the camera	about the
			// locked direction, and maintain the up vector	pointing towards the locked	direction
			// Find	the	rotation that points the localupaxis towards the targetupaxis
			FQuat PointToUp = FQuat::FindBetweenNormals(LocalSpaceUpAxis, (FVector)Source.LockedAxis);

			// Add in rotation about the TargetUpAxis to point the facing vector towards the camera
			FVector	DirToCameraInRotationPlane = DirToCamera - FVector((DirToCamera | (FVector)Source.LockedAxis)*Source.LockedAxis);
			DirToCameraInRotationPlane.Normalize();
			FQuat PointToCamera = FQuat::FindBetweenNormals(PointToUp.RotateVector(LocalSpaceFacingAxis), DirToCameraInRotationPlane);

			// Set kRotMat to the composed rotation
			FQuat MeshRotation = PointToCamera*PointToUp;
			kRotMat = FQuatRotationMatrix(MeshRotation);
		}
		else if (Source.MeshAlignment == PSMA_MeshFaceCameraWithSpin)
		{
			// Implement a tangent-rotation	version	of point-to-camera.	 The facing	direction points to	the	camera,
			// with	no roll, and has addtional sprite-particle rotation	about the tangential axis
			// (c.f. the roll rotation is about	the	radial axis)

			// Find	the	rotation that points the facing	axis towards the camera
			FRotator PointToRotation = FRotator(FQuat::FindBetweenNormals(LocalSpaceFacingAxis, DirToCamera));

			// When	constructing the rotation, we need to eliminate	roll around	the	dirtocamera	axis,
			// otherwise the particle appears to rotate	around the dircamera axis when it or the camera	moves
			PointToRotation.Roll = 0;

			// Add in the tangential rotation we do	want.
			FVector	vPositivePitch = FVector(0, 0, 1); //	this is	set	by the rotator's yaw/pitch/roll	reference frame
			FVector	vTangentAxis = vPositivePitch^DirToCamera;
			vTangentAxis.Normalize();
			if (vTangentAxis.SizeSquared() < 0.5f)
			{
				vTangentAxis = FVector(1, 0, 0); // assert is	possible if	FQuat axis/angle constructor is	passed zero-vector
			}

			FQuat AddedTangentialRotation = FQuat(vTangentAxis, ParticleRotation);

			// Set kRotMat to the composed rotation
			FQuat MeshRotation = AddedTangentialRotation*PointToRotation.Quaternion();
			kRotMat = FQuatRotationMatrix(MeshRotation);
		}
		else
		{
			// Implement a roll-rotation version of	point-to-camera.  The facing direction points to the camera,
			// with	no roll, and then rotates about	the	direction_to_camera	by the spriteparticle rotation.

			// Find	the	rotation that points the facing	axis towards the camera
			FRotator PointToRotation = FRotator(FQuat::FindBetweenNormals(LocalSpaceFacingAxis, DirToCamera));

			// When	constructing the rotation, we need to eliminate	roll around	the	dirtocamera	axis,
			// otherwise the particle appears to rotate	around the dircamera axis when it or the camera	moves
			PointToRotation.Roll = 0;

			// Add in the roll we do want.
			FQuat AddedRollRotation = FQuat(DirToCamera, ParticleRotation);

			// Set kRotMat to the composed	rotation
			FQuat MeshRotation = AddedRollRotation*PointToRotation.Quaternion();
			kRotMat = FQuatRotationMatrix(MeshRotation);
		}
	}
	else
	{
		float fRot = ParticleRotation * 180.0f / UE_PI;
		FVector kRotVec = FVector(fRot, fRot, fRot);
		FRotator kRotator = FRotator::MakeFromEuler(kRotVec);

		kRotator += FRotator::MakeFromEuler((FVector)ParticlePayloadRotation);

		kRotMat = FRotationMatrix(kRotator);
	}

	if (bApplyPreRotation == true)
	{
		FRotator MeshOrient = FRotator::MakeFromEuler((FVector)ParticlePayloadInitialOrientation);
		FRotationMatrix OrientMat(MeshOrient);

		if ((bUseCameraFacing == true) || (bUseMeshLockedAxis == true))
		{
			OutTransformMat = (OrientMat * kScaleMat) * FQuatRotationMatrix(kLockedAxisQuat) * kRotMat * kTransMat;
		}
		else
		{
			OutTransformMat = (OrientMat*kScaleMat) * kRotMat * kTransMat;
		}
	}
	else if ((bUseCameraFacing == true) || (bUseMeshLockedAxis == true))
	{
		OutTransformMat = kScaleMat * FQuatRotationMatrix(kLockedAxisQuat) * kRotMat * kTransMat;
	}
	else
	{
		OutTransformMat = kScaleMat * kRotMat * kTransMat;
	}

	FVector OrbitOffset(ParticlePayloadOrbitOffset);
	if (Source.bUseLocalSpace == false)
	{
		OrbitOffset = LocalToWorld.TransformVector(OrbitOffset);
	}

	FTranslationMatrix OrbitMatrix(OrbitOffset);
	OutTransformMat *= OrbitMatrix;

	if (Source.bUseLocalSpace)
	{
		OutTransformMat *= LocalToWorld;
	}

	const FVector3f LWCTile = Source.bUseLocalSpace ? FLargeWorldRenderScalar::GetTileFor(ProxyLocalToWorld.GetOrigin()) : Source.LWCTile;
	const FVector LWCTileOffset = FVector(LWCTile) * FLargeWorldRenderScalar::GetTileSize();
	OutTransformMat.M[3][0] -= LWCTileOffset.X;
	OutTransformMat.M[3][1] -= LWCTileOffset.Y;
	OutTransformMat.M[3][2] -= LWCTileOffset.Z;
}

void FDynamicMeshEmitterData::GetInstanceData(void* InstanceData, void* DynamicParameterData, void* PrevTransformBuffer, const FParticleSystemSceneProxy* Proxy, const FSceneView* View, uint32 InstanceFactor) const
{
	SCOPE_CYCLE_COUNTER(STAT_ParticlePackingTime);
	SCOPE_CYCLE_COUNTER(STAT_ParticlesOverview_RT_CNC);

	int32 SubImagesX = Source.SubImages_Horizontal;
	int32 SubImagesY = Source.SubImages_Vertical;
	float SubImageSizeX = 1.0f / SubImagesX;
	float SubImageSizeY = 1.0f / SubImagesY;

	int32 ParticleCount = Source.ActiveParticleCount;
	if ((Source.MaxDrawCount >= 0) && (ParticleCount > Source.MaxDrawCount))
	{
		ParticleCount = Source.MaxDrawCount;
	}

	int32 InstanceVertexStride = sizeof(FMeshParticleInstanceVertex);
	int32 DynamicParameterVertexStride = bUsesDynamicParameter ? sizeof(FMeshParticleInstanceVertexDynamicParameter) : 0;
	int32 PrevTransformVertexStride = sizeof(FVector4f) * 3;

	uint8* TempVert = (uint8*)InstanceData;
	uint8* TempDynamicParameterVert = (uint8*)DynamicParameterData;
	uint8* TempPrevTranformVert = (uint8*)PrevTransformBuffer;

	LastCalculatedMeshLOD = MAX_STATIC_MESH_LODS;
	FCachedSystemScalabilityCVars CachedSystemScalabilityCVars = GetCachedScalabilityCVars();

	float InvScreenSizeScale = (CachedSystemScalabilityCVars.StaticMeshLODDistanceScale != 0.f) ? (1.0f / CachedSystemScalabilityCVars.StaticMeshLODDistanceScale) : 1.0f;
	float TotalLODSizeScale = InvScreenSizeScale*LODSizeScale;

	for (int32 i = ParticleCount - 1; i >= 0; i--)
	{
		const int32	CurrentIndex	= Source.DataContainer.ParticleIndices[i];
		const uint8* ParticleBase	= Source.DataContainer.ParticleData + CurrentIndex * Source.ParticleStride;
		const FBaseParticle& Particle		= *((const FBaseParticle*) ParticleBase);

		FMeshParticleInstanceVertex CurrentInstanceVertex;
		
		// Populate instance buffer;
		// The particle color.
		CurrentInstanceVertex.Color = Particle.Color;
		
		// Instance to world transformation. Translation (Instance world position) is packed into W
		FMatrix TransMat(FMatrix::Identity);
		GetParticleTransform(Particle, Proxy, View, TransMat);
		
		// Transpose on CPU to allow for simpler shader code to perform the transform. 
		const FMatrix Transpose = TransMat.GetTransposed();
		CurrentInstanceVertex.Transform[0] = FVector4f(Transpose.M[0][0], Transpose.M[0][1], Transpose.M[0][2], Transpose.M[0][3]);
		CurrentInstanceVertex.Transform[1] = FVector4f(Transpose.M[1][0], Transpose.M[1][1], Transpose.M[1][2], Transpose.M[1][3]);
		CurrentInstanceVertex.Transform[2] = FVector4f(Transpose.M[2][0], Transpose.M[2][1], Transpose.M[2][2], Transpose.M[2][3]);

		if (bUseStaticMeshLODs)
		{
			// if we're in local space, transform largest particle's position to world space
			// this is then used to determine screen size for LOD selection in GetDynamicMeshElements
			FVector ParticlePos = Particle.Location;
			if (Source.bUseLocalSpace)
			{
				ParticlePos = Proxy->GetLocalToWorld().TransformPosition(ParticlePos);
			}
			FVector ParticleSize = TransMat.GetScaleVector();
			int32 LODIndexToUse = ComputeStaticMeshLOD(StaticMesh->GetRenderData(), ParticlePos, ParticleSize.Size()*0.5f*StaticMesh->GetBounds().SphereRadius, *View, 0, TotalLODSizeScale);
			LODIndexToUse = FMath::Min(LODIndexToUse, StaticMesh->GetNumLODs() - 1);
			LastCalculatedMeshLOD = LODIndexToUse < LastCalculatedMeshLOD ? LODIndexToUse : LastCalculatedMeshLOD;
		}

		if (PrevTransformBuffer)
		{
			FVector4f* PrevTransformVertex = (FVector4f*)TempPrevTranformVert;
			
			if (Source.MeshMotionBlurOffset)
			{
				// Instance to world transformation. Translation (Instance world position) is packed into W
				FMatrix PrevTransMat(FMatrix::Identity);
				GetParticlePrevTransform(Particle, Proxy, View, PrevTransMat);

				// Transpose on CPU to allow for simpler shader code to perform the transform. 
				const FMatrix PrevTranspose = PrevTransMat.GetTransposed();
				PrevTransformVertex[0] = FVector4f(PrevTranspose.M[0][0], PrevTranspose.M[0][1], PrevTranspose.M[0][2], PrevTranspose.M[0][3]);
				PrevTransformVertex[1] = FVector4f(PrevTranspose.M[1][0], PrevTranspose.M[1][1], PrevTranspose.M[1][2], PrevTranspose.M[1][3]);
				PrevTransformVertex[2] = FVector4f(PrevTranspose.M[2][0], PrevTranspose.M[2][1], PrevTranspose.M[2][2], PrevTranspose.M[2][3]);
			}
			else
			{
				PrevTransformVertex[0] = CurrentInstanceVertex.Transform[0];
				PrevTransformVertex[1] = CurrentInstanceVertex.Transform[1];
				PrevTransformVertex[2] = CurrentInstanceVertex.Transform[2];
			}

			TempPrevTranformVert += PrevTransformVertexStride;
		}

		// Particle velocity. Calculate on CPU to avoid computing in vertex shader.
		// Note: It would be preferred if we could check whether the material makes use of the 'Particle Direction' node to avoid this work.
		FVector DeltaPosition(Particle.Location - Particle.OldLocation);

		int32 CurrentOffset = Source.OrbitModuleOffset;
		if (CurrentOffset != 0)
		{
			FOrbitChainModuleInstancePayload& OrbitPayload = *((FOrbitChainModuleInstancePayload*)((uint8*)&Particle + CurrentOffset));																\
			DeltaPosition = (Particle.Location + FVector(OrbitPayload.Offset)) - (Particle.OldLocation + FVector(OrbitPayload.PreviousOffset));
		}

		if (!DeltaPosition.IsZero())
		{
			if (Source.bUseLocalSpace)
			{
				DeltaPosition = Proxy->GetLocalToWorld().TransformVector(DeltaPosition);
			}
			FVector Direction;
			float Speed; 
			DeltaPosition.ToDirectionAndLength(Direction, Speed);

			// Pack direction and speed.
			CurrentInstanceVertex.Velocity = FVector4f(FVector3f(Direction), Speed);
		}
		else
		{
			CurrentInstanceVertex.Velocity = FVector4f::Zero();
		}

		// The particle dynamic value
		if (bUsesDynamicParameter)
		{
			if (Source.DynamicParameterDataOffset > 0)
			{
				FVector4f DynamicParameterValue;
				FMeshParticleInstanceVertexDynamicParameter CurrentInstanceVertexDynParam;
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, Particle, DynamicParameterValue);
				CurrentInstanceVertexDynParam.DynamicValue[0] = DynamicParameterValue.X;
				CurrentInstanceVertexDynParam.DynamicValue[1] = DynamicParameterValue.Y;
				CurrentInstanceVertexDynParam.DynamicValue[2] = DynamicParameterValue.Z;
				CurrentInstanceVertexDynParam.DynamicValue[3] = DynamicParameterValue.W;

				//@todo - refactor into instance step rate in the RHI
				for (uint32 Factor = 0; Factor < InstanceFactor; Factor++)
				{
					FMemory::Memcpy(TempDynamicParameterVert + DynamicParameterVertexStride * Factor, &CurrentInstanceVertexDynParam, sizeof(FMeshParticleInstanceVertexDynamicParameter));
				}

				TempDynamicParameterVert += DynamicParameterVertexStride * InstanceFactor;
			}
		}
		
		// SubUVs 
		if (Source.SubUVInterpMethod != PSUVIM_None && Source.SubUVDataOffset > 0)
		{
			const FFullSubUVPayload* SubUVPayload = (const FFullSubUVPayload*)(((const uint8*)&Particle) + Source.SubUVDataOffset);

			float SubImageIndex = SubUVPayload->ImageIndex;
			float SubImageLerp = FMath::Fractional(SubImageIndex);
			int32 SubImageA = FMath::FloorToInt(SubImageIndex);
			int32 SubImageB = SubImageA + 1;
			int32 SubImageAH = SubImageA % SubImagesX;
			int32 SubImageBH = SubImageB % SubImagesX;
			int32 SubImageAV = SubImageA / SubImagesX;
			int32 SubImageBV = SubImageB / SubImagesX;

			// SubUV offsets and lerp value
			CurrentInstanceVertex.SubUVParams[0] = SubImageAH;
			CurrentInstanceVertex.SubUVParams[1] = SubImageAV;
			CurrentInstanceVertex.SubUVParams[2] = SubImageBH;
			CurrentInstanceVertex.SubUVParams[3] = SubImageBV;
			CurrentInstanceVertex.SubUVLerp = SubImageLerp;
		}

		// The particle's relative time
		CurrentInstanceVertex.RelativeTime = Particle.RelativeTime;

		//@todo - refactor into instance step rate in the RHI
		for (uint32 Factor = 0; Factor < InstanceFactor; Factor++)
		{
			FMemory::Memcpy(TempVert + InstanceVertexStride * Factor, &CurrentInstanceVertex, InstanceVertexStride);
		}

		TempVert += InstanceVertexStride * InstanceFactor;
	}
}

void FDynamicMeshEmitterData::SetupVertexFactory( FMeshParticleVertexFactory* InVertexFactory, const FStaticMeshLODResources& LODResources, uint32 LODIdx) const
{
		FMeshParticleVertexFactory::FDataType Data;

		LODResources.VertexBuffers.PositionVertexBuffer.BindPositionVertexBuffer(InVertexFactory, Data);
		LODResources.VertexBuffers.StaticMeshVertexBuffer.BindTangentVertexBuffer(InVertexFactory, Data);
		LODResources.VertexBuffers.StaticMeshVertexBuffer.BindTexCoordVertexBuffer(InVertexFactory, Data, MAX_TEXCOORDS);
		LODResources.VertexBuffers.ColorVertexBuffer.BindColorVertexBuffer(InVertexFactory, Data);

		// Initialize instanced data. Vertex buffer and stride are set before render.
		// Particle color
		Data.ParticleColorComponent = FVertexStreamComponent(
			NULL,
			STRUCT_OFFSET(FMeshParticleInstanceVertex, Color),
			0,
			VET_Float4,
			EVertexStreamUsage::Instancing
			);

		// Particle transform matrix
		for (int32 MatrixRow = 0; MatrixRow < 3; MatrixRow++)
		{
			Data.TransformComponent[MatrixRow] = FVertexStreamComponent(
				NULL,
				STRUCT_OFFSET(FMeshParticleInstanceVertex, Transform) + sizeof(FVector4f) * MatrixRow, 
				0,
				VET_Float4,
				EVertexStreamUsage::Instancing
				);
		}

		Data.VelocityComponent = FVertexStreamComponent(
			NULL,
			STRUCT_OFFSET(FMeshParticleInstanceVertex,Velocity),
			0,
			VET_Float4,
			EVertexStreamUsage::Instancing
			);

		// SubUVs.
		Data.SubUVs = FVertexStreamComponent(
			NULL,
			STRUCT_OFFSET(FMeshParticleInstanceVertex, SubUVParams), 
			0,
			VET_Short4,
			EVertexStreamUsage::Instancing
			);

		// Pack SubUV Lerp and the particle's relative time
		Data.SubUVLerpAndRelTime = FVertexStreamComponent(
			NULL,
			STRUCT_OFFSET(FMeshParticleInstanceVertex, SubUVLerp), 
			0,
			VET_Float2,
			EVertexStreamUsage::Instancing
			);

		Data.bInitialized = true;
		InVertexFactory->SetData(Data);
		InVertexFactory->SetLODIdx((uint8)LODIdx);
}

///////////////////////////////////////////////////////////////////////////////
//	FDynamicBeam2EmitterData
///////////////////////////////////////////////////////////////////////////////

FDynamicBeam2EmitterData::~FDynamicBeam2EmitterData()
{
}


/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicBeam2EmitterData::Init( bool bInSelected )
{
	bSelected = bInSelected;

	check(Source.ActiveParticleCount < (MaxBeams));	// TTP #33330 - Max of 2048 beams from a single emitter
	check(Source.ParticleStride < 
		((MaxInterpolationPoints + 2) * (sizeof(FVector3f) + sizeof(float))) + 
		(MaxNoiseFrequency * (sizeof(FVector3f) + sizeof(FVector3f) + sizeof(float) + sizeof(float)))
		);	// TTP #33330 - Max of 10k per beam (includes interpolation points, noise, etc.)

	MaterialResource = Source.MaterialInterface->GetRenderProxy();

	bUsesDynamicParameter = false;

	// We won't need this on the render thread
	Source.MaterialInterface = NULL;
}

/** Perform the actual work of filling the buffer, often called from another thread 
* @param Me Fill data structure
*/
void FDynamicBeam2EmitterData::DoBufferFill(FAsyncBufferFillData& Me) const
{
	if( Me.VertexCount <= 0 || Me.IndexCount <= 0 || Me.VertexData == NULL || Me.IndexData == NULL )
	{
		return;
	}

	FillIndexData(Me);

	if (Source.bLowFreqNoise_Enabled)
	{
		FillData_Noise(Me);
	}
	else
	{
		FillVertexData_NoNoise(Me);
	}
}

FParticleBeamTrailUniformBufferRef CreateBeamTrailUniformBuffer(
	const FParticleSystemSceneProxy* Proxy,
	const FDynamicSpriteEmitterReplayDataBase* SourceData,
	const FSceneView* View )
{
	FParticleBeamTrailUniformParameters UniformParameters;

	// Tangent vectors.
	FVector CameraUp(0.0f);
	FVector CameraRight(0.0f);
	const EParticleAxisLock LockAxisFlag = (EParticleAxisLock)SourceData->LockAxisFlag;
	if (LockAxisFlag == EPAL_NONE)
	{
		CameraUp	= -View->ViewMatrices.GetInvViewProjectionMatrix().TransformVector(FVector(1.0f,0.0f,0.0f)).GetSafeNormal();
		CameraRight	= -View->ViewMatrices.GetInvViewProjectionMatrix().TransformVector(FVector(0.0f,1.0f,0.0f)).GetSafeNormal();
	}
	else
	{
		const FMatrix& LocalToWorld = SourceData->bUseLocalSpace ? Proxy->GetLocalToWorld() : FMatrix::Identity;
		ComputeLockedAxes( LockAxisFlag, LocalToWorld, CameraUp, CameraRight );
	}
	UniformParameters.CameraUp = FVector4f( (FVector3f)CameraUp, 0.0f );
	UniformParameters.CameraRight = FVector4f( (FVector3f)CameraRight, 0.0f );

	// Screen alignment.
	UniformParameters.ScreenAlignment = FVector4f( (float)SourceData->ScreenAlignment, 0.0f, 0.0f, 0.0f );
	UniformParameters.bUseLocalSpace = SourceData->bUseLocalSpace;
	UniformParameters.LWCTile = SourceData->LWCTile;

	return FParticleBeamTrailUniformBufferRef::CreateUniformBufferImmediate( UniformParameters, UniformBuffer_SingleFrame );
}

class FDynamicBeamTrailCollectorResources : public FOneFrameResource
{
public:
	FDynamicBeamTrailCollectorResources(ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel)
	{
	}

	virtual ~FDynamicBeamTrailCollectorResources()
	{
		VertexFactory.ReleaseResource();
	}

	FParticleBeamTrailVertexFactory VertexFactory;
};

void FDynamicBeam2EmitterData::GetDynamicMeshElementsEmitter(const FParticleSystemSceneProxy* Proxy, const FSceneView* View, const FSceneViewFamily& ViewFamily, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_BeamRenderingTime);
	INC_DWORD_STAT(STAT_BeamParticlesRenderCalls);

	if (bValid == false)
	{
		return;
	}

	if ((Source.VertexCount == 0) && (Source.IndexCount == 0))
	{
		return;
	}
	FIndexBuffer* IndexBuffer = nullptr;
	uint32 FirstIndex = 0;
	int32 OutTriangleCount = 0;
	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;

	FGlobalDynamicVertexBuffer::FAllocation DynamicVertexAllocation;
	FGlobalDynamicIndexBuffer::FAllocation DynamicIndexAllocation;
	FGlobalDynamicVertexBuffer::FAllocation DynamicParameterAllocation;
	FAsyncBufferFillData Data;

	BuildViewFillData(Proxy, View, Source.VertexCount, sizeof(FParticleBeamTrailVertex), 0,
		Collector.GetDynamicIndexBuffer(), Collector.GetDynamicVertexBuffer(),
		DynamicVertexAllocation, DynamicIndexAllocation, &DynamicParameterAllocation, Data);
	DoBufferFill(Data);
	OutTriangleCount = Data.OutTriangleCount;

	// Early out if we have no triangles
	if (OutTriangleCount == 0)
	{
		return;
	}

	if (Source.bUseLocalSpace == false)
	{
		Proxy->UpdateWorldSpacePrimitiveUniformBuffer();
	}

	auto FeatureLevel = View->GetFeatureLevel();
	FDynamicBeamTrailCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FDynamicBeamTrailCollectorResources>(FeatureLevel);
	FParticleBeamTrailVertexFactory* BeamTrailVertexFactory = &CollectorResources.VertexFactory;
	BeamTrailVertexFactory->SetParticleFactoryType(PVFT_BeamTrail);
	BeamTrailVertexFactory->SetUsesDynamicParameter(bUsesDynamicParameter);
	BeamTrailVertexFactory->InitResource();

	// Create and set the uniform buffer for this emitter.
	BeamTrailVertexFactory->SetBeamTrailUniformBuffer(CreateBeamTrailUniformBuffer(Proxy, &Source, View));
	BeamTrailVertexFactory->SetVertexBuffer(DynamicVertexAllocation.VertexBuffer, DynamicVertexAllocation.VertexOffset, GetDynamicVertexStride(FeatureLevel));
	BeamTrailVertexFactory->SetDynamicParameterBuffer(NULL, 0, 0);
	IndexBuffer = DynamicIndexAllocation.IndexBuffer;
	FirstIndex = DynamicIndexAllocation.FirstIndex;

	BeamTrailVertexFactory->GetIndexBuffer() = IndexBuffer;
	BeamTrailVertexFactory->GetFirstIndex() = FirstIndex;
	BeamTrailVertexFactory->GetOutTriangleCount() = OutTriangleCount;

	// Create mesh batch
	FMeshBatch& Mesh = Collector.AllocateMesh();
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = IndexBuffer;
	BatchElement.FirstIndex = FirstIndex;
	Mesh.VertexFactory = BeamTrailVertexFactory;
	Mesh.LCI = NULL;
	if (Source.bUseLocalSpace == true)
	{
		BatchElement.PrimitiveUniformBuffer = Proxy->GetUniformBuffer();
	}
	else
	{
		BatchElement.PrimitiveUniformBuffer = Proxy->GetWorldSpacePrimitiveUniformBuffer();
	}
	int32 TrianglesToRender = OutTriangleCount;
	if ((TrianglesToRender % 2) != 0)
	{
		TrianglesToRender--;
	}
	BatchElement.NumPrimitives = TrianglesToRender;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = Source.VertexCount - 1;
	Mesh.ReverseCulling = Proxy->IsLocalToWorldDeterminantNegative();
	Mesh.CastShadow = Proxy->GetCastShadow();
	Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)Proxy->GetDepthPriorityGroup(View);

	if (AllowDebugViewmodes() && bIsWireframe && !ViewFamily.EngineShowFlags.Materials)
	{
		Mesh.MaterialRenderProxy = Proxy->GetDeselectedWireframeMatInst();
	}
	else
	{
		Mesh.MaterialRenderProxy = MaterialResource;
	}
	Mesh.Type = PT_TriangleStrip;

	Mesh.bCanApplyViewModeOverrides = true;
	Mesh.bUseWireframeSelectionColoring = Proxy->IsSelected();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Mesh.VisualizeLODIndex = (int8)Proxy->GetVisualizeLODIndex();
#endif

	Collector.AddMesh(ViewIndex, Mesh);

	INC_DWORD_STAT_BY(STAT_BeamParticlesTrianglesRendered, Mesh.GetNumPrimitives());

	if (Source.bRenderDirectLine == true)
	{
		RenderDirectLine(Proxy, Collector.GetPDI(ViewIndex), View);
	}

	if ((Source.bRenderLines == true) ||
		(Source.bRenderTessellation == true))
	{
		RenderLines(Proxy, Collector.GetPDI(ViewIndex), View);
	}
}

void FDynamicBeam2EmitterData::RenderDirectLine(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View) const
{
	for (int32 Beam = 0; Beam < Source.ActiveParticleCount; Beam++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * Beam);

		FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
		FVector*				InterpolatedPoints	= NULL;
		float*					NoiseRate			= NULL;
		float*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		float*					TaperValues			= NULL;

		BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}

		DrawWireStar(PDI, (FVector)BeamPayloadData->SourcePoint, 20.0f, FColor::Green, Proxy->GetDepthPriorityGroup(View));
		DrawWireStar(PDI, (FVector)BeamPayloadData->TargetPoint, 20.0f, FColor::Red, Proxy->GetDepthPriorityGroup(View));
		PDI->DrawLine((FVector)BeamPayloadData->SourcePoint, (FVector)BeamPayloadData->TargetPoint, FColor::Yellow, Proxy->GetDepthPriorityGroup(View));
	}
}

void FDynamicBeam2EmitterData::RenderLines(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View) const
{
	if (Source.bLowFreqNoise_Enabled)
	{
		int32	TrianglesToRender = 0;

		FMatrix WorldToLocal = Proxy->GetWorldToLocal();
		FMatrix LocalToWorld = Proxy->GetLocalToWorld();
		FMatrix CameraToWorld = View->ViewMatrices.GetInvViewMatrix();
		FVector	ViewOrigin = CameraToWorld.GetOrigin();

		// NoiseTessellation is the amount of tessellation that should occur between noise points.
		int32	TessFactor	= Source.NoiseTessellation ? Source.NoiseTessellation : 1;
		float	InvTessFactor	= 1.0f / TessFactor;
		int32		i;

		// The last position processed
		FVector	LastPosition, LastDrawPosition, LastTangent;
		// The current position
		FVector	CurrPosition, CurrDrawPosition;
		// The target
		FVector	TargetPosition, TargetDrawPosition;
		// The next target
		FVector	NextTargetPosition, NextTargetDrawPosition, TargetTangent;
		// The interperted draw position
		FVector InterpDrawPos;
		FVector	InterimDrawPosition;

		FVector	Size;

		FVector Location;
		FVector EndPoint;
		FVector Offset;
		FVector LastOffset;
		float	fStrength;
		float	fTargetStrength;

		int32	 VertexCount	= 0;

		// Tessellate the beam along the noise points
		for (i = 0; i < Source.ActiveParticleCount; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);

			// Retrieve the beam data from the particle.
			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			float*					NoiseRate			= NULL;
			float*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			float*					TaperValues			= NULL;
			float*					NoiseDistanceScale	= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if (Source.NoiseRateOffset != -1)
			{
				NoiseRate = (float*)((uint8*)Particle + Source.NoiseRateOffset);
			}
			if (Source.NoiseDeltaTimeOffset != -1)
			{
				NoiseDelta = (float*)((uint8*)Particle + Source.NoiseDeltaTimeOffset);
			}
			if (Source.TargetNoisePointsOffset != -1)
			{
				TargetNoisePoints = (FVector*)((uint8*)Particle + Source.TargetNoisePointsOffset);
			}
			if (Source.NextNoisePointsOffset != -1)
			{
				NextNoisePoints = (FVector*)((uint8*)Particle + Source.NextNoisePointsOffset);
			}
			if (Source.TaperValuesOffset != -1)
			{
				TaperValues = (float*)((uint8*)Particle + Source.TaperValuesOffset);
			}
			if (Source.NoiseDistanceScaleOffset != -1)
			{
				NoiseDistanceScale = (float*)((uint8*)Particle + Source.NoiseDistanceScaleOffset);
			}

			float NoiseDistScale = 1.0f;
			if (NoiseDistanceScale)
			{
				NoiseDistScale = *NoiseDistanceScale;
			}

			FVector* NoisePoints	= TargetNoisePoints;
			FVector* NextNoise		= NextNoisePoints;

			float NoiseRangeScaleFactor = Source.NoiseRangeScale;
			//@todo. How to handle no noise points?
			// If there are no noise points, why are we in here?
			if (NoisePoints == NULL)
			{
				continue;
			}

			// Pin the size to the X component
			Size = FVector(Particle->Size.X * Source.Scale.X);

			check(TessFactor > 0);

			// Setup the current position as the source point
			CurrPosition		= (FVector)BeamPayloadData->SourcePoint;
			CurrDrawPosition	= CurrPosition;

			// Setup the source tangent & strength
			if (Source.bUseSource)
			{
				// The source module will have determined the proper source tangent.
				LastTangent	= (FVector)BeamPayloadData->SourceTangent;
				fStrength	= BeamPayloadData->SourceStrength;
			}
			else
			{
				// We don't have a source module, so use the orientation of the emitter.
				LastTangent	= WorldToLocal.GetScaledAxis( EAxis::X );
				fStrength	= Source.NoiseTangentStrength;
			}
			LastTangent.Normalize();
			LastTangent *= fStrength;
			fTargetStrength	= Source.NoiseTangentStrength;

			// Set the last draw position to the source so we don't get 'under-hang'
			LastPosition		= CurrPosition;
			LastDrawPosition	= CurrDrawPosition;

			bool	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

			FVector	UseNoisePoint, CheckNoisePoint;
			FVector	NoiseDir;

			// Reset the texture coordinate
			LastPosition		= (FVector)BeamPayloadData->SourcePoint;
			LastDrawPosition	= LastPosition;

			// Determine the current position by stepping the direct line and offsetting with the noise point. 
			CurrPosition		= LastPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;

			if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
			{
				NoiseDir		= NextNoise[0] - NoisePoints[0];
				NoiseDir.Normalize();
				CheckNoisePoint	= NoisePoints[0] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
				if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
					(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
					(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
				{
					NoisePoints[0]	= NextNoise[0];
				}
				else
				{
					NoisePoints[0]	= CheckNoisePoint;
				}
			}

			CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[0] * NoiseDistScale);

			// Determine the offset for the leading edge
			Location	= LastDrawPosition;
			EndPoint	= CurrDrawPosition;

			// 'Lead' edge
			DrawWireStar(PDI, Location, 15.0f, FColor::Green, Proxy->GetDepthPriorityGroup(View));

			for (int32 StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
			{
				// Determine the current position by stepping the direct line and offsetting with the noise point. 
				CurrPosition		= LastPosition + FVector(BeamPayloadData->Direction * BeamPayloadData->StepSize);

				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
					if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[StepIndex]	= NextNoise[StepIndex];
					}
					else
					{
						NoisePoints[StepIndex]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex] * NoiseDistScale);

				// Prep the next draw position to determine tangents
				bool bTarget = false;
				NextTargetPosition	= CurrPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;
				if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
				{
					// If we are locked, and the next step is the target point, set the draw position as such.
					// (ie, we are on the last noise point...)
					NextTargetDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
							if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
					}
					TargetTangent = (FVector)BeamPayloadData->TargetTangent;
					fTargetStrength	= BeamPayloadData->TargetStrength;
				}
				else
				{
					// Just another noise point... offset the target to get the draw position.
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
						if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
						}
						else
						{
							NoisePoints[StepIndex + 1]	= CheckNoisePoint;
						}
					}

					NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex + 1] * NoiseDistScale);

					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
				}
				TargetTangent.Normalize();
				TargetTangent *= fTargetStrength;

				InterimDrawPosition = LastDrawPosition;
				// Tessellate between the current position and the last position
				for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = FMath::CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;

					FColor StarColor(255,0,255);
					if (TessIndex == 0)
					{
						StarColor = FColor::Blue;
					}
					else
					if (TessIndex == (TessFactor - 1))
					{
						StarColor = FColor::Yellow;
					}

					// Generate the vertex
					DrawWireStar(PDI, EndPoint, 15.0f, StarColor, Proxy->GetDepthPriorityGroup(View));
					PDI->DrawLine(Location, EndPoint, FLinearColor(1.0f,1.0f,0.0f), Proxy->GetDepthPriorityGroup(View));
					InterimDrawPosition	= InterpDrawPos;
				}
				LastPosition		= CurrPosition;
				LastDrawPosition	= CurrDrawPosition;
				LastTangent			= TargetTangent;
			}

			if (bLocked)
			{
				// Draw the line from the last point to the target
				CurrDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
				if (Source.bTargetNoise)
				{
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
						if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
						}
						else
						{
							NoisePoints[Source.Frequency]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
				}

				if (Source.bUseTarget)
				{
					TargetTangent = (FVector)BeamPayloadData->TargetTangent;
				}
				else
				{
					NextTargetDrawPosition	= CurrPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;
					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
						(NextTargetDrawPosition - LastDrawPosition);
				}
				TargetTangent.Normalize();
				TargetTangent *= fTargetStrength;

				// Tessellate this segment
				InterimDrawPosition = LastDrawPosition;
				for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = FMath::CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;

					FColor StarColor(255,0,255);
					if (TessIndex == 0)
					{
						StarColor = FColor::White;
					}
					else if (TessIndex == (TessFactor - 1))
					{
						StarColor = FColor::Yellow;
					}

					// Generate the vertex
					DrawWireStar(PDI, EndPoint, 15.0f, StarColor, Proxy->GetDepthPriorityGroup(View));
					PDI->DrawLine(Location, EndPoint, FLinearColor(1.0f,1.0f,0.0f), Proxy->GetDepthPriorityGroup(View));
					VertexCount++;
					InterimDrawPosition	= InterpDrawPos;
				}
			}
		}
	}

	if (Source.InterpolationPoints > 1)
	{
		FMatrix CameraToWorld = View->ViewMatrices.GetInvViewMatrix();
		FVector	ViewOrigin = CameraToWorld.GetOrigin();
		int32 TessFactor = Source.InterpolationPoints ? Source.InterpolationPoints : 1;

		if (TessFactor <= 1)
		{
			for (int32 i = 0; i < Source.ActiveParticleCount; i++)
			{
				DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);
				FBeam2TypeDataPayload* BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
				if (BeamPayloadData->TriangleCount == 0)
				{
					continue;
				}

				FVector EndPoint	= Particle->Location;
				FVector Location	= (FVector)BeamPayloadData->SourcePoint;

				DrawWireStar(PDI, Location, 15.0f, FColor::Red, Proxy->GetDepthPriorityGroup(View));
				DrawWireStar(PDI, EndPoint, 15.0f, FColor::Red, Proxy->GetDepthPriorityGroup(View));
				PDI->DrawLine(Location, EndPoint, FColor::Yellow, Proxy->GetDepthPriorityGroup(View));
			}
		}
		else
		{
			for (int32 i = 0; i < Source.ActiveParticleCount; i++)
			{
				DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);

				FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
				FVector*				InterpolatedPoints	= NULL;

				BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
				if (BeamPayloadData->TriangleCount == 0)
				{
					continue;
				}
				if (Source.InterpolatedPointsOffset != -1)
				{
					InterpolatedPoints = (FVector*)((uint8*)Particle + Source.InterpolatedPointsOffset);
				}

				FVector Location;
				FVector EndPoint;

				check(InterpolatedPoints);	// TTP #33139

				Location	= (FVector)BeamPayloadData->SourcePoint;
				EndPoint	= InterpolatedPoints[0];

				DrawWireStar(PDI, Location, 15.0f, FColor::Red, Proxy->GetDepthPriorityGroup(View));
				for (int32 StepIndex = 0; StepIndex < BeamPayloadData->InterpolationSteps; StepIndex++)
				{
					EndPoint = InterpolatedPoints[StepIndex];
					DrawWireStar(PDI, EndPoint, 15.0f, FColor::Red, Proxy->GetDepthPriorityGroup(View));
					PDI->DrawLine(Location, EndPoint, FColor::Yellow, Proxy->GetDepthPriorityGroup(View));
					Location = EndPoint;
				}
			}
		}
	}
}

void FDynamicBeam2EmitterData::RenderDebug(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, bool bCrosses) const
{
}

void FDynamicBeam2EmitterData::GetIndexAllocInfo(int32& OutNumIndices, int32& OutStride ) const
{
	//	bool bWireframe = (View->Family->EngineShowFlags.Wireframe) && !View->Family->EngineShowFlags.Materials;
	bool bWireframe = false;

	int32 TempIndexCount = 0;
	for (int32 ii = 0; ii < Source.TrianglesPerSheet.Num(); ii++)
	{
		int32 Triangles = Source.TrianglesPerSheet[ii];
		if (bWireframe)
		{
			TempIndexCount += (8 * Triangles + 2) * Source.Sheets;
		}
		else
		{
			if (Triangles > 0)
			{
				if (TempIndexCount == 0)
				{
					TempIndexCount = 2;     // First Beam
				}
				else
				{
					TempIndexCount += 4;	// Degenerate indices between beams
				}

				TempIndexCount += Triangles * Source.Sheets;
				TempIndexCount += 4 * (Source.Sheets - 1);	// Degenerate indices between sheets
			}
		}
	}

	OutNumIndices = TempIndexCount;
	OutStride = Source.IndexStride;
}

template <typename TIndexType>
static int32 CreateDynamicBeam2EmitterIndices(TIndexType* OutIndex, const FDynamicBeam2EmitterReplayData& Source)
{
	int32	TrianglesToRender = 0;
	TIndexType	VertexIndex = 0;
	TIndexType	StartVertexIndex = 0;

	TIndexType *BaseIndex = OutIndex;

	// Signed as we are comparing against pointer arithmetic
	const int32 MaxIndexCount = 65535;

	for (int32 Beam = 0; Beam < Source.ActiveParticleCount; Beam++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * Beam);
		FBeam2TypeDataPayload*	BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}
		if ((Source.InterpolationPoints > 0) && (BeamPayloadData->Steps == 0))
		{
			continue;
		}

		if (VertexIndex == 0)//First Beam
		{
			if ((OutIndex - BaseIndex <= MaxIndexCount - 2))
			{
				*(OutIndex++) = VertexIndex++;	// SheetIndex + 0
				*(OutIndex++) = VertexIndex++;	// SheetIndex + 1
			}
		}
		else//Degenerate tris between beams
		{
			if ((OutIndex - BaseIndex <= MaxIndexCount - 4))
			{
				*(OutIndex++) = VertexIndex - 1;	// Last vertex of the previous sheet
				*(OutIndex++) = VertexIndex;		// First vertex of the next sheet
				*(OutIndex++) = VertexIndex++;		// First vertex of the next sheet
				*(OutIndex++) = VertexIndex++;		// Second vertex of the next sheet
				TrianglesToRender += 4;
			}
		}

		for (int32 SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
		{
			// 2 triangles per tessellation factor
			TrianglesToRender += BeamPayloadData->TriangleCount;

			// Sequentially step through each triangle - 1 vertex per triangle
			for (int32 i = 0; i < BeamPayloadData->TriangleCount; i++)
			{
				*(OutIndex++) = VertexIndex++;

				if (OutIndex - BaseIndex > MaxIndexCount)
				{
					break;
				}
			}

			// Degenerate tris
			if ((SheetIndex + 1) < Source.Sheets
				&& (OutIndex - BaseIndex <= MaxIndexCount-4))
			{
				*(OutIndex++) = VertexIndex - 1;	// Last vertex of the previous sheet
				*(OutIndex++) = VertexIndex;		// First vertex of the next sheet
				*(OutIndex++) = VertexIndex++;		// First vertex of the next sheet
				*(OutIndex++) = VertexIndex++;		// Second vertex of the next sheet

				TrianglesToRender += 4;
			}

			if (OutIndex - BaseIndex > MaxIndexCount)
			{
				break;
			}

		}
	}

	return TrianglesToRender;
}

int32 FDynamicBeam2EmitterData::FillIndexData(struct FAsyncBufferFillData& Data) const
{
	SCOPE_CYCLE_COUNTER(STAT_BeamFillIndexTime);

	// Beam2 polygons are packed and joined as follows:
	//
	// 1--3--5--7--9-...
	// |\ |\ |\ |\ |\...
	// | \| \| \| \| ...
	// 0--2--4--6--8-...
	//
	// (ie, the 'leading' edge of polygon (n) is the trailing edge of polygon (n+1)
	//
	// NOTE: This is primed for moving to tri-strips...
	//
	int32 TessFactor = Source.InterpolationPoints ? Source.InterpolationPoints : 1;

	check(Data.IndexCount > 0 && Data.IndexData != NULL);

	//DESCRIPTION:
	//	When in Cascade, creating a particle beam with 20 beams and 130 sheets or higher causes the system to crash.

	int32	TrianglesToRender = 0;
	if (Source.IndexStride == sizeof(uint16))
	{
		TrianglesToRender = CreateDynamicBeam2EmitterIndices<uint16>((uint16*)Data.IndexData, Source);
	}
	else
	{
		check(Source.IndexStride == sizeof(uint32));
		TrianglesToRender = CreateDynamicBeam2EmitterIndices<uint32>((uint32*)Data.IndexData, Source);
	}

	Data.OutTriangleCount = TrianglesToRender;
	return TrianglesToRender;
}

int32 FDynamicBeam2EmitterData::FillVertexData_NoNoise(FAsyncBufferFillData& Me) const
{
	SCOPE_CYCLE_COUNTER(STAT_BeamFillVertexTime);

	int32	TrianglesToRender = 0;

	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)Me.VertexData;
	FMatrix CameraToWorld = Me.View->ViewMatrices.GetInvViewMatrix();
	FVector	ViewOrigin = CameraToWorld.GetOrigin();
	int32 TessFactor = Source.InterpolationPoints ? Source.InterpolationPoints : 1;

	FVector	Offset(0.0f), LastOffset(0.0f);

	int32 PackedCount = 0;

	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();
	if (TessFactor <= 1)
	{
		for (int32 i = 0; i < Source.ActiveParticleCount; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);

			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			FVector*				InterpolatedPoints	= NULL;
			float*					NoiseRate			= NULL;
			float*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			float*					TaperValues			= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if (Source.InterpolatedPointsOffset != -1)
			{
				InterpolatedPoints = (FVector*)((uint8*)Particle + Source.InterpolatedPointsOffset);
			}
			if (Source.NoiseRateOffset != -1)
			{
				NoiseRate = (float*)((uint8*)Particle + Source.NoiseRateOffset);
			}
			if (Source.NoiseDeltaTimeOffset != -1)
			{
				NoiseDelta = (float*)((uint8*)Particle + Source.NoiseDeltaTimeOffset);
			}
			if (Source.TargetNoisePointsOffset != -1)
			{
				TargetNoisePoints = (FVector*)((uint8*)Particle + Source.TargetNoisePointsOffset);
			}
			if (Source.NextNoisePointsOffset != -1)
			{
				NextNoisePoints = (FVector*)((uint8*)Particle + Source.NextNoisePointsOffset);
			}
			if (Source.TaperValuesOffset != -1)
			{
				TaperValues = (float*)((uint8*)Particle + Source.TaperValuesOffset);
			}

			// Pin the size to the X component
			FVector2D Size(Particle->Size.X * Source.Scale.X, Particle->Size.X * Source.Scale.X);

			FVector EndPoint	= Particle->Location;
			FVector Location	= (FVector)BeamPayloadData->SourcePoint;
			FVector Right, Up;
			FVector WorkingUp;

			Right = Location - EndPoint;
			Right.Normalize();
			if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
			{
				//Up = Right ^ ViewDirection;
				Up = Right ^ (Location - ViewOrigin);
				if (!Up.Normalize())
				{
					Up = CameraToWorld.GetScaledAxis( EAxis::Y );
				}
			}

			float	fUEnd;
			float	Tiles		= 1.0f;
			if (Source.TextureTileDistance > UE_KINDA_SMALL_NUMBER)
			{
				FVector	Direction	= FVector(BeamPayloadData->TargetPoint - BeamPayloadData->SourcePoint);
				float	Distance	= Direction.Size();
				Tiles				= Distance / Source.TextureTileDistance;
			}
			else
			{
				Tiles = FMath::Max((float)Source.TextureTile, 1.0f);
			}

			fUEnd		= Tiles;

			if (BeamPayloadData->TravelRatio > UE_KINDA_SMALL_NUMBER)
			{
				fUEnd	= Tiles * BeamPayloadData->TravelRatio;
			}

			// For the direct case, this isn't a big deal, as it will not require much work per sheet.
			for (int32 SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				if (SheetIndex)
				{
					float	Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
					FQuat	QuatRotator	= FQuat(Right, Angle);
					WorkingUp			= QuatRotator.RotateVector(Up);
				}
				else
				{
					WorkingUp	= Up;
				}

				float	Taper	= 1.0f;
				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				// Size is locked to X, see initialization of Size.
				Offset.X		= WorkingUp.X * Size.X * Taper;
				Offset.Y		= WorkingUp.Y * Size.X * Taper;
				Offset.Z		= WorkingUp.Z * Size.X * Taper;

				// 'Lead' edge
				Vertex->Position	= FVector3f(Location + Offset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= 0.0f;
				Vertex->Tex_V		= 0.0f;
				Vertex->Tex_U2		= 0.0f;
				Vertex->Tex_V2		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				Vertex->Position	= FVector3f(Location - Offset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= 0.0f;
				Vertex->Tex_V		= 1.0f;
				Vertex->Tex_U2		= 0.0f;
				Vertex->Tex_V2		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[1];
				}

				// Size is locked to X, see initialization of Size.
				Offset.X		= WorkingUp.X * Size.X * Taper;
				Offset.Y		= WorkingUp.Y * Size.X * Taper;
				Offset.Z		= WorkingUp.Z * Size.X * Taper;

				//
				Vertex->Position	= FVector3f(EndPoint + Offset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Particle->OldLocation - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fUEnd;
				Vertex->Tex_V		= 0.0f;
				Vertex->Tex_U2		= 1.0f;
				Vertex->Tex_V2		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				Vertex->Position	= FVector3f(EndPoint - Offset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Particle->OldLocation - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fUEnd;
				Vertex->Tex_V		= 1.0f;
				Vertex->Tex_U2		= 1.0f;
				Vertex->Tex_V2		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;
			}
		}
	}
	else
	{
		float	fTextureIncrement	= 1.0f / Source.InterpolationPoints;;

		for (int32 i = 0; i < Source.ActiveParticleCount; i++)
		{
			DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);

			FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
			FVector*				InterpolatedPoints	= NULL;
			float*					NoiseRate			= NULL;
			float*					NoiseDelta			= NULL;
			FVector*				TargetNoisePoints	= NULL;
			FVector*				NextNoisePoints		= NULL;
			float*					TaperValues			= NULL;

			BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
			if (BeamPayloadData->TriangleCount == 0)
			{
				continue;
			}
			if (Source.InterpolatedPointsOffset != -1)
			{
				InterpolatedPoints = (FVector*)((uint8*)Particle + Source.InterpolatedPointsOffset);
			}
			if (Source.NoiseRateOffset != -1)
			{
				NoiseRate = (float*)((uint8*)Particle + Source.NoiseRateOffset);
			}
			if (Source.NoiseDeltaTimeOffset != -1)
			{
				NoiseDelta = (float*)((uint8*)Particle + Source.NoiseDeltaTimeOffset);
			}
			if (Source.TargetNoisePointsOffset != -1)
			{
				TargetNoisePoints = (FVector*)((uint8*)Particle + Source.TargetNoisePointsOffset);
			}
			if (Source.NextNoisePointsOffset != -1)
			{
				NextNoisePoints = (FVector*)((uint8*)Particle + Source.NextNoisePointsOffset);
			}
			if (Source.TaperValuesOffset != -1)
			{
				TaperValues = (float*)((uint8*)Particle + Source.TaperValuesOffset);
			}

			if (Source.TextureTileDistance > UE_KINDA_SMALL_NUMBER)
			{
				FVector	Direction	= FVector(BeamPayloadData->TargetPoint - BeamPayloadData->SourcePoint);
				float	Distance	= Direction.Size();
				float	Tiles		= Distance / Source.TextureTileDistance;
				fTextureIncrement	= Tiles / Source.InterpolationPoints;
			}

			// Pin the size to the X component
			FVector2D Size(Particle->Size.X * Source.Scale.X, Particle->Size.X * Source.Scale.X);

			float	Angle;
			FQuat	QuatRotator(0, 0, 0, 0);

			FVector Location;
			FVector EndPoint;
			FVector Right;
			FVector Up;
			FVector WorkingUp;
			float	fU;

			float	Tex_U2 = 0.0f;
			float	Tex_U2_Increment = 1.0f / BeamPayloadData->Steps;

			check(InterpolatedPoints);	// TTP #33139
			// For the direct case, this isn't a big deal, as it will not require much work per sheet.
			for (int32 SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				fU			= 0.0f;
				Location	= (FVector)BeamPayloadData->SourcePoint;
				EndPoint	= InterpolatedPoints[0];
				Right		= Location - EndPoint;
				Right.Normalize();
				if (Source.UpVectorStepSize == 0)
				{
					//Up = Right ^ ViewDirection;
					Up = Right ^ (Location - ViewOrigin);
					if (!Up.Normalize())
					{
						Up = CameraToWorld.GetScaledAxis( EAxis::Y );
					}
				}

				if (SheetIndex)
				{
					Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
					QuatRotator	= FQuat(Right, Angle);
					WorkingUp	= QuatRotator.RotateVector(Up);
				}
				else
				{
					WorkingUp	= Up;
				}

				float	Taper	= 1.0f;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				// Size is locked to X, see initialization of Size.
				Offset.X	= WorkingUp.X * Size.X * Taper;
				Offset.Y	= WorkingUp.Y * Size.X * Taper;
				Offset.Z	= WorkingUp.Z * Size.X * Taper;

				// 'Lead' edge
				Vertex->Position	= FVector3f(Location + Offset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 0.0f;
				Vertex->Tex_U2		= 0.0f;
				Vertex->Tex_V2		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				Vertex->Position	= FVector3f(Location - Offset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 1.0f;
				Vertex->Tex_U2		= 0.0f;
				Vertex->Tex_V2		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				PackedCount++;

				for (int32 StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
				{
					Tex_U2 += Tex_U2_Increment;

					EndPoint	= InterpolatedPoints[StepIndex];
					if (Source.UpVectorStepSize == 0)
					{
						//Up = Right ^ ViewDirection;
						Up = Right ^ (Location - ViewOrigin);
						if (!Up.Normalize())
						{
							Up = CameraToWorld.GetScaledAxis( EAxis::Y );
						}
					}

					if (SheetIndex)
					{
						WorkingUp	= QuatRotator.RotateVector(Up);
					}
					else
					{
						WorkingUp	= Up;
					}

					if (Source.TaperMethod != PEBTM_None)
					{
						check(TaperValues);
						Taper	= TaperValues[StepIndex + 1];
					}

					// Size is locked to X, see initialization of Size.
					Offset.X		= WorkingUp.X * Size.X * Taper;
					Offset.Y		= WorkingUp.Y * Size.X * Taper;
					Offset.Z		= WorkingUp.Z * Size.X * Taper;

					//
					Vertex->Position	= FVector3f(EndPoint + Offset - LWCTileOffset);
					Vertex->OldPosition	= FVector3f(EndPoint - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size		= FVector2f(Size);
					Vertex->Tex_U		= fU + fTextureIncrement;
					Vertex->Tex_V		= 0.0f;
					Vertex->Tex_U2		= Tex_U2;
					Vertex->Tex_V2		= 0.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
				PackedCount++;

					Vertex->Position	= FVector3f(EndPoint - Offset - LWCTileOffset);
					Vertex->OldPosition	= FVector3f(EndPoint - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size		= FVector2f(Size);
					Vertex->Tex_U		= fU + fTextureIncrement;
					Vertex->Tex_V		= 1.0f;
					Vertex->Tex_U2		= Tex_U2;
					Vertex->Tex_V2		= 1.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
				PackedCount++;

					Location			 = EndPoint;
					fU					+= fTextureIncrement;
				}

				if (BeamPayloadData->TravelRatio > UE_KINDA_SMALL_NUMBER)
				{
					//@todo.SAS. Re-implement partial-segment beams
				}
			}
		}
	}

	check(PackedCount <= Source.VertexCount);

	return TrianglesToRender;
}

int32 FDynamicBeam2EmitterData::FillData_Noise(FAsyncBufferFillData& Me) const
{
	SCOPE_CYCLE_COUNTER(STAT_BeamFillVertexTime);

	int32	TrianglesToRender = 0;

	if (Source.InterpolationPoints > 0)
	{
		return FillData_InterpolatedNoise(Me);
	}

	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)Me.VertexData;
	FMatrix CameraToWorld = Me.View->ViewMatrices.GetInvViewMatrix();

	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	// NoiseTessellation is the amount of tessellation that should occur between noise points.
	int32	TessFactor	= Source.NoiseTessellation ? Source.NoiseTessellation : 1;
	
	float	InvTessFactor	= 1.0f / TessFactor;
	int32		i;

	// The last position processed
	FVector	LastPosition, LastDrawPosition, LastTangent;
	// The current position
	FVector	CurrPosition, CurrDrawPosition;
	// The target
	FVector	TargetPosition, TargetDrawPosition;
	// The next target
	FVector	NextTargetPosition, NextTargetDrawPosition, TargetTangent;
	// The interperted draw position
	FVector InterpDrawPos;
	FVector	InterimDrawPosition;

	float	Angle;
	FQuat	QuatRotator;

	FVector Location;
	FVector EndPoint;
	FVector Right;
	FVector Up;
	FVector WorkingUp;
	FVector LastUp;
	FVector WorkingLastUp;
	FVector Offset;
	FVector LastOffset;
	float	fStrength;
	float	fTargetStrength;

	float	fU;
	float	TextureIncrement	= 1.0f / (((Source.Frequency > 0) ? Source.Frequency : 1) * TessFactor);	// TTP #33140/33159

	int32	 CheckVertexCount	= 0;

	FVector THE_Up(0.0f);

	FMatrix WorldToLocal = Me.WorldToLocal;
	FMatrix LocalToWorld = Me.LocalToWorld;

	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();

	// Tessellate the beam along the noise points
	for (i = 0; i < Source.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);

		// Retrieve the beam data from the particle.
		FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
		FVector*				InterpolatedPoints	= NULL;
		float*					NoiseRate			= NULL;
		float*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		float*					TaperValues			= NULL;
		float*					NoiseDistanceScale	= NULL;

		BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}
		if (Source.InterpolatedPointsOffset != -1)
		{
			InterpolatedPoints = (FVector*)((uint8*)Particle + Source.InterpolatedPointsOffset);
		}
		if (Source.NoiseRateOffset != -1)
		{
			NoiseRate = (float*)((uint8*)Particle + Source.NoiseRateOffset);
		}
		if (Source.NoiseDeltaTimeOffset != -1)
		{
			NoiseDelta = (float*)((uint8*)Particle + Source.NoiseDeltaTimeOffset);
		}
		if (Source.TargetNoisePointsOffset != -1)
		{
			TargetNoisePoints = (FVector*)((uint8*)Particle + Source.TargetNoisePointsOffset);
		}
		if (Source.NextNoisePointsOffset != -1)
		{
			NextNoisePoints = (FVector*)((uint8*)Particle + Source.NextNoisePointsOffset);
		}
		if (Source.TaperValuesOffset != -1)
		{
			TaperValues = (float*)((uint8*)Particle + Source.TaperValuesOffset);
		}
		if (Source.NoiseDistanceScaleOffset != -1)
		{
			NoiseDistanceScale = (float*)((uint8*)Particle + Source.NoiseDistanceScaleOffset);
		}

		float NoiseDistScale = 1.0f;
		if (NoiseDistanceScale)
		{
			NoiseDistScale = *NoiseDistanceScale;
		}

		FVector* NoisePoints	= TargetNoisePoints;
		FVector* NextNoise		= NextNoisePoints;

		float NoiseRangeScaleFactor = Source.NoiseRangeScale;
		//@todo. How to handle no noise points?
		// If there are no noise points, why are we in here?
		if (NoisePoints == NULL)
		{
			continue;
		}

		// Pin the size to the X component
		FVector2D Size(Particle->Size.X * Source.Scale.X, Particle->Size.X * Source.Scale.X);

		if (TessFactor <= 1)
		{
			// Setup the current position as the source point
			CurrPosition		= (FVector)BeamPayloadData->SourcePoint;
			CurrDrawPosition	= CurrPosition;

			// Setup the source tangent & strength
			if (Source.bUseSource)
			{
				// The source module will have determined the proper source tangent.
				LastTangent	= (FVector)BeamPayloadData->SourceTangent;
				fStrength	= BeamPayloadData->SourceStrength;
			}
			else
			{
				// We don't have a source module, so use the orientation of the emitter.
				LastTangent	= WorldToLocal.GetScaledAxis( EAxis::X );
				fStrength	= Source.NoiseTangentStrength;
			}
			LastTangent.Normalize();
			LastTangent *= fStrength;

			fTargetStrength	= Source.NoiseTangentStrength;

			// Set the last draw position to the source so we don't get 'under-hang'
			LastPosition		= CurrPosition;
			LastDrawPosition	= CurrDrawPosition;

			bool	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

			FVector	UseNoisePoint, CheckNoisePoint;
			FVector	NoiseDir;

			for (int32 SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				// Reset the texture coordinate
				fU					= 0.0f;
				LastPosition		= (FVector)BeamPayloadData->SourcePoint;
				LastDrawPosition	= LastPosition;

				// Determine the current position by stepping the direct line and offsetting with the noise point. 
				CurrPosition		= LastPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;

				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[0] - NoisePoints[0];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[0] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
					if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[0]	= NextNoise[0];
					}
					else
					{
						NoisePoints[0]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[0] * NoiseDistScale);

				// Determine the offset for the leading edge
				Location	= LastDrawPosition;
				EndPoint	= CurrDrawPosition;
				Right		= Location - EndPoint;
				Right.Normalize();
				if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
				{
					//LastUp = Right ^ ViewDirection;
					LastUp = Right ^ (Location - ViewOrigin);
					if (!LastUp.Normalize())
					{
						LastUp = CameraToWorld.GetScaledAxis( EAxis::Y );
					}
					THE_Up = LastUp;
				}
				else
				{
					LastUp = THE_Up;
				}

				if (SheetIndex)
				{
					Angle			= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
					QuatRotator		= FQuat(Right, Angle);
					WorkingLastUp	= QuatRotator.RotateVector(LastUp);
				}
				else
				{
					WorkingLastUp	= LastUp;
				}

				float	Taper	= 1.0f;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				// Size is locked to X, see initialization of Size.
				LastOffset.X	= WorkingLastUp.X * Size.X * Taper;
				LastOffset.Y	= WorkingLastUp.Y * Size.X * Taper;
				LastOffset.Z	= WorkingLastUp.Z * Size.X * Taper;

				// 'Lead' edge
				Vertex->Position	= FVector3f(Location + LastOffset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				Vertex->Position	= FVector3f(Location - LastOffset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				fU	+= TextureIncrement;

				for (int32 StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
				{
					// Determine the current position by stepping the direct line and offsetting with the noise point. 
					CurrPosition		= LastPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;

					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
						if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex]	= NextNoise[StepIndex];
						}
						else
						{
							NoisePoints[StepIndex]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex] * NoiseDistScale);

					// Prep the next draw position to determine tangents
					bool bTarget = false;
					NextTargetPosition	= CurrPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;
					if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
					{
						// If we are locked, and the next step is the target point, set the draw position as such.
						// (ie, we are on the last noise point...)
						NextTargetDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
						if (Source.bTargetNoise)
						{
							if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
							{
								NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
								NoiseDir.Normalize();
								CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
								if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
									(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
									(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
								{
									NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
								}
								else
								{
									NoisePoints[Source.Frequency]	= CheckNoisePoint;
								}
							}

							NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
						}
						TargetTangent = (FVector)BeamPayloadData->TargetTangent;
						fTargetStrength	= BeamPayloadData->TargetStrength;
					}
					else
					{
						// Just another noise point... offset the target to get the draw position.
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
							if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
							}
							else
							{
								NoisePoints[StepIndex + 1]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex + 1] * NoiseDistScale);

						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					InterimDrawPosition = LastDrawPosition;
					// Tessellate between the current position and the last position
					for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++) //-V1008
					{
						InterpDrawPos = FMath::CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetScaledAxis( EAxis::Y );
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[StepIndex * TessFactor + TessIndex];
						}

						// Size is locked to X, see initialization of Size.
						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.X * Taper;
						Offset.Z	= WorkingUp.Z * Size.X * Taper;

						// Generate the vertex
						Vertex->Position	= FVector3f(InterpDrawPos + Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= FVector3f(InterpDrawPos - Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
					LastPosition		= CurrPosition;
					LastDrawPosition	= CurrDrawPosition;
					LastTangent			= TargetTangent;
				}

				if (bLocked)
				{
					// Draw the line from the last point to the target
					CurrDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
							if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
					}

					if (Source.bUseTarget)
					{
						TargetTangent = (FVector)BeamPayloadData->TargetTangent;
					}
					else
					{
						NextTargetDrawPosition	= CurrPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;
						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
							(NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					// Tessellate this segment
					InterimDrawPosition = LastDrawPosition;
					for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++) //-V1008
					{
						InterpDrawPos = FMath::CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetScaledAxis( EAxis::Y );
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[BeamPayloadData->Steps * TessFactor + TessIndex];
						}

						// Size is locked to X, see initialization of Size.
						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.X * Taper;
						Offset.Z	= WorkingUp.Z * Size.X * Taper;

						// Generate the vertex
						Vertex->Position	= FVector3f(InterpDrawPos + Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= FVector3f(InterpDrawPos - Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
				}
			}
		}
		else
		{
			// Setup the current position as the source point
			CurrPosition		= (FVector)BeamPayloadData->SourcePoint;
			CurrDrawPosition	= CurrPosition;

			// Setup the source tangent & strength
			if (Source.bUseSource)
			{
				// The source module will have determined the proper source tangent.
				LastTangent	= (FVector)BeamPayloadData->SourceTangent;
				fStrength	= BeamPayloadData->SourceStrength;
			}
			else
			{
				// We don't have a source module, so use the orientation of the emitter.
				LastTangent	= WorldToLocal.GetScaledAxis( EAxis::X );
				fStrength	= Source.NoiseTangentStrength;
			}
			LastTangent.Normalize();
			LastTangent *= fStrength;

			// Setup the target tangent strength
			fTargetStrength	= Source.NoiseTangentStrength;

			// Set the last draw position to the source so we don't get 'under-hang'
			LastPosition		= CurrPosition;
			LastDrawPosition	= CurrDrawPosition;

			bool	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

			FVector	UseNoisePoint, CheckNoisePoint;
			FVector	NoiseDir;

			for (int32 SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
			{
				// Reset the texture coordinate
				fU					= 0.0f;
				LastPosition		= (FVector)BeamPayloadData->SourcePoint;
				LastDrawPosition	= LastPosition;

				// Determine the current position by stepping the direct line and offsetting with the noise point. 
				CurrPosition		= LastPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;

				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[0] - NoisePoints[0];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[0] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
					if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[0]	= NextNoise[0];
					}
					else
					{
						NoisePoints[0]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[0] * NoiseDistScale);

				// Determine the offset for the leading edge
				Location	= LastDrawPosition;
				EndPoint	= CurrDrawPosition;
				Right		= Location - EndPoint;
				Right.Normalize();
				if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
				{
					//LastUp = Right ^ ViewDirection;
					LastUp = Right ^ (Location - ViewOrigin);
					if (!LastUp.Normalize())
					{
						LastUp = CameraToWorld.GetScaledAxis( EAxis::Y );
					}
					THE_Up = LastUp;
				}
				else
				{
					LastUp = THE_Up;
				}

				if (SheetIndex)
				{
					Angle			= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
					QuatRotator		= FQuat(Right, Angle);
					WorkingLastUp	= QuatRotator.RotateVector(LastUp);
				}
				else
				{
					WorkingLastUp	= LastUp;
				}

				float	Taper	= 1.0f;

				if (Source.TaperMethod != PEBTM_None)
				{
					check(TaperValues);
					Taper	= TaperValues[0];
				}

				// Size is locked to X, see initialization of Size.
				LastOffset.X	= WorkingLastUp.X * Size.X * Taper;
				LastOffset.Y	= WorkingLastUp.Y * Size.X * Taper;
				LastOffset.Z	= WorkingLastUp.Z * Size.X * Taper;

				// 'Lead' edge
				Vertex->Position	= FVector3f(Location + LastOffset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 0.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				Vertex->Position	= FVector3f(Location - LastOffset - LWCTileOffset);
				Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size		= FVector2f(Size);
				Vertex->Tex_U		= fU;
				Vertex->Tex_V		= 1.0f;
				Vertex->Rotation	= Particle->Rotation;
				Vertex->Color		= Particle->Color;
				Vertex++;
				CheckVertexCount++;

				fU	+= TextureIncrement;

				for (int32 StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
				{
					// Determine the current position by stepping the direct line and offsetting with the noise point. 
					CurrPosition		= LastPosition + FVector(BeamPayloadData->Direction * BeamPayloadData->StepSize);

					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
						if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex]	= NextNoise[StepIndex];
						}
						else
						{
							NoisePoints[StepIndex]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex] * NoiseDistScale);

					// Prep the next draw position to determine tangents
					bool bTarget = false;
					NextTargetPosition	= CurrPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;
					if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
					{
						// If we are locked, and the next step is the target point, set the draw position as such.
						// (ie, we are on the last noise point...)
						NextTargetDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
						if (Source.bTargetNoise)
						{
							if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
							{
								NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
								NoiseDir.Normalize();
								CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
								if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
									(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
									(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
								{
									NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
								}
								else
								{
									NoisePoints[Source.Frequency]	= CheckNoisePoint;
								}
							}

							NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
						}
						TargetTangent = (FVector)BeamPayloadData->TargetTangent;
						fTargetStrength	= BeamPayloadData->TargetStrength;
					}
					else
					{
						// Just another noise point... offset the target to get the draw position.
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
							if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
							}
							else
							{
								NoisePoints[StepIndex + 1]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex + 1] * NoiseDistScale);

						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					InterimDrawPosition = LastDrawPosition;
					// Tessellate between the current position and the last position
					for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++)
					{
						InterpDrawPos = FMath::CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						FPlatformMisc::Prefetch(Vertex+2);

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetScaledAxis( EAxis::Y );
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[StepIndex * TessFactor + TessIndex];
						}

						// Size is locked to X, see initialization of Size.
						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.X * Taper;
						Offset.Z	= WorkingUp.Z * Size.X * Taper;

						// Generate the vertex
						Vertex->Position	= FVector3f(InterpDrawPos + Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= FVector3f(InterpDrawPos - Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
					LastPosition		= CurrPosition;
					LastDrawPosition	= CurrDrawPosition;
					LastTangent			= TargetTangent;
				}

				if (bLocked)
				{
					// Draw the line from the last point to the target
					CurrDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
							if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
					}

					if (Source.bUseTarget)
					{
						TargetTangent = (FVector)BeamPayloadData->TargetTangent;
					}
					else
					{
						NextTargetDrawPosition	= CurrPosition + FVector(BeamPayloadData->Direction * BeamPayloadData->StepSize);
						TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
							(NextTargetDrawPosition - LastDrawPosition);
					}
					TargetTangent.Normalize();
					TargetTangent *= fTargetStrength;

					// Tessellate this segment
					InterimDrawPosition = LastDrawPosition;
					for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++)
					{
						InterpDrawPos = FMath::CubicInterp(
							LastDrawPosition, LastTangent,
							CurrDrawPosition, TargetTangent,
							InvTessFactor * (TessIndex + 1));

						Location	= InterimDrawPosition;
						EndPoint	= InterpDrawPos;
						Right		= Location - EndPoint;
						Right.Normalize();
						if (Source.UpVectorStepSize == 0)
						{
							//Up = Right ^  (Location - CameraToWorld.GetOrigin());
							Up = Right ^ (Location - ViewOrigin);
							if (!Up.Normalize())
							{
								Up = CameraToWorld.GetScaledAxis( EAxis::Y );
							}
						}
						else
						{
							Up = THE_Up;
						}

						if (SheetIndex)
						{
							Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
							QuatRotator	= FQuat(Right, Angle);
							WorkingUp	= QuatRotator.RotateVector(Up);
						}
						else
						{
							WorkingUp	= Up;
						}

						if (Source.TaperMethod != PEBTM_None)
						{
							check(TaperValues);
							Taper	= TaperValues[BeamPayloadData->Steps * TessFactor + TessIndex];
						}

						// Size is locked to X, see initialization of Size.
						Offset.X	= WorkingUp.X * Size.X * Taper;
						Offset.Y	= WorkingUp.Y * Size.X * Taper;
						Offset.Z	= WorkingUp.Z * Size.X * Taper;

						// Generate the vertex
						Vertex->Position	= FVector3f(InterpDrawPos + Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 0.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						Vertex->Position	= FVector3f(InterpDrawPos - Offset - LWCTileOffset);
						Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
						Vertex->ParticleId	= 0;
						Vertex->Size		= FVector2f(Size);
						Vertex->Tex_U		= fU;
						Vertex->Tex_V		= 1.0f;
						Vertex->Rotation	= Particle->Rotation;
						Vertex->Color		= Particle->Color;
						Vertex++;
						CheckVertexCount++;

						fU	+= TextureIncrement;
						InterimDrawPosition	= InterpDrawPos;
					}
				}
				else
				if (BeamPayloadData->TravelRatio > UE_KINDA_SMALL_NUMBER)
				{
					//@todo.SAS. Re-implement partial-segment beams
				}
			}
		}
	}

	check(CheckVertexCount <= Source.VertexCount);

	return TrianglesToRender;
}

int32 FDynamicBeam2EmitterData::FillData_InterpolatedNoise(FAsyncBufferFillData& Me) const
{
	int32	TrianglesToRender = 0;

	check(Source.InterpolationPoints > 0);
	check(Source.Frequency > 0);

	FParticleBeamTrailVertex* Vertex = (FParticleBeamTrailVertex*)Me.VertexData;
	FMatrix CameraToWorld = Me.View->ViewMatrices.GetInvViewMatrix();
	
	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	// NoiseTessellation is the amount of tessellation that should occur between noise points.
	int32	TessFactor	= Source.NoiseTessellation ? Source.NoiseTessellation : 1;
	
	float	InvTessFactor	= 1.0f / TessFactor;
	int32		i;

	// The last position processed
	FVector	LastPosition, LastDrawPosition, LastTangent;
	// The current position
	FVector	CurrPosition, CurrDrawPosition;
	// The target
	FVector	TargetPosition, TargetDrawPosition;
	// The next target
	FVector	NextTargetPosition, NextTargetDrawPosition, TargetTangent;
	// The interperted draw position
	FVector InterpDrawPos;
	FVector	InterimDrawPosition;

	float	Angle;
	FQuat	QuatRotator;

	FVector Location;
	FVector EndPoint;
	FVector Right;
	FVector Up;
	FVector WorkingUp;
	FVector LastUp;
	FVector WorkingLastUp;
	FVector Offset;
	FVector LastOffset;
	float	fStrength;
	float	fTargetStrength;

	float	fU;
	float	TextureIncrement	= 1.0f / (((Source.Frequency > 0) ? Source.Frequency : 1) * TessFactor);	// TTP #33140/33159

	FVector THE_Up(0.0f);

	int32	 CheckVertexCount	= 0;

	FMatrix WorldToLocal = Me.WorldToLocal;
	FMatrix LocalToWorld = Me.LocalToWorld;
	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();

	// Tessellate the beam along the noise points
	for (i = 0; i < Source.ActiveParticleCount; i++)
	{
		DECLARE_PARTICLE_PTR(Particle, Source.DataContainer.ParticleData + Source.ParticleStride * i);

		// Retrieve the beam data from the particle.
		FBeam2TypeDataPayload*	BeamPayloadData		= NULL;
		FVector*				InterpolatedPoints	= NULL;
		float*					NoiseRate			= NULL;
		float*					NoiseDelta			= NULL;
		FVector*				TargetNoisePoints	= NULL;
		FVector*				NextNoisePoints		= NULL;
		float*					TaperValues			= NULL;
		float*					NoiseDistanceScale	= NULL;

		BeamPayloadData = (FBeam2TypeDataPayload*)((uint8*)Particle + Source.BeamDataOffset);
		if (BeamPayloadData->TriangleCount == 0)
		{
			continue;
		}
		if (BeamPayloadData->Steps == 0)
		{
			continue;
		}

		if (Source.InterpolatedPointsOffset != -1)
		{
			InterpolatedPoints = (FVector*)((uint8*)Particle + Source.InterpolatedPointsOffset);
		}
		if (Source.NoiseRateOffset != -1)
		{
			NoiseRate = (float*)((uint8*)Particle + Source.NoiseRateOffset);
		}
		if (Source.NoiseDeltaTimeOffset != -1)
		{
			NoiseDelta = (float*)((uint8*)Particle + Source.NoiseDeltaTimeOffset);
		}
		if (Source.TargetNoisePointsOffset != -1)
		{
			TargetNoisePoints = (FVector*)((uint8*)Particle + Source.TargetNoisePointsOffset);
		}
		if (Source.NextNoisePointsOffset != -1)
		{
			NextNoisePoints = (FVector*)((uint8*)Particle + Source.NextNoisePointsOffset);
		}
		if (Source.TaperValuesOffset != -1)
		{
			TaperValues = (float*)((uint8*)Particle + Source.TaperValuesOffset);
		}
		if (Source.NoiseDistanceScaleOffset != -1)
		{
			NoiseDistanceScale = (float*)((uint8*)Particle + Source.NoiseDistanceScaleOffset);
		}

		float NoiseDistScale = 1.0f;
		if (NoiseDistanceScale)
		{
			NoiseDistScale = *NoiseDistanceScale;
		}

		int32 Freq = BEAM2_TYPEDATA_FREQUENCY(BeamPayloadData->Lock_Max_NumNoisePoints);
		float InterpStepSize = (float)(BeamPayloadData->InterpolationSteps) / (float)(BeamPayloadData->Steps);
		float InterpFraction = FMath::Fractional(InterpStepSize);
		//bool bInterpFractionIsZero = (FMath::Abs(InterpFraction) < KINDA_SMALL_NUMBER) ? true : false;
		bool bInterpFractionIsZero = false;
		int32 InterpIndex = FMath::TruncToInt(InterpStepSize);

		FVector* NoisePoints	= TargetNoisePoints;
		FVector* NextNoise		= NextNoisePoints;

		// Appropriate checks are made before access access, no need to assert here
		CA_ASSUME(NoisePoints != NULL);
		CA_ASSUME(NextNoise != NULL);
		CA_ASSUME(InterpolatedPoints != NULL);

		float NoiseRangeScaleFactor = Source.NoiseRangeScale;
		//@todo. How to handle no noise points?
		// If there are no noise points, why are we in here?
		if (NoisePoints == NULL)
		{
			continue;
		}

		// Pin the size to the X component
		FVector2D Size(Particle->Size.X * Source.Scale.X, Particle->Size.X * Source.Scale.X);

		// Setup the current position as the source point
		CurrPosition		= (FVector)BeamPayloadData->SourcePoint;
		CurrDrawPosition	= CurrPosition;

		// Setup the source tangent & strength
		if (Source.bUseSource)
		{
			// The source module will have determined the proper source tangent.
			LastTangent	= (FVector)BeamPayloadData->SourceTangent;
			fStrength	= Source.NoiseTangentStrength;
		}
		else
		{
			// We don't have a source module, so use the orientation of the emitter.
			LastTangent	= WorldToLocal.GetScaledAxis( EAxis::X );
			fStrength	= Source.NoiseTangentStrength;
		}
		LastTangent *= fStrength;

		// Setup the target tangent strength
		fTargetStrength	= Source.NoiseTangentStrength;

		// Set the last draw position to the source so we don't get 'under-hang'
		LastPosition		= CurrPosition;
		LastDrawPosition	= CurrDrawPosition;

		bool	bLocked	= BEAM2_TYPEDATA_LOCKED(BeamPayloadData->Lock_Max_NumNoisePoints);

		FVector	UseNoisePoint, CheckNoisePoint;
		FVector	NoiseDir;

		for (int32 SheetIndex = 0; SheetIndex < Source.Sheets; SheetIndex++)
		{
			// Reset the texture coordinate
			fU					= 0.0f;
			LastPosition		= (FVector)BeamPayloadData->SourcePoint;
			LastDrawPosition	= LastPosition;

			// Determine the current position by finding it along the interpolated path and 
			// offsetting with the noise point. 
			if (bInterpFractionIsZero)
			{
				CurrPosition = InterpolatedPoints[InterpIndex];
			}
			else
			{
				CurrPosition = 
					(InterpolatedPoints[InterpIndex + 0] * InterpFraction) + 
					(InterpolatedPoints[InterpIndex + 1] * (1.0f - InterpFraction));
			}

			if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
			{
				NoiseDir		= NextNoise[0] - NoisePoints[0];
				NoiseDir.Normalize();
				CheckNoisePoint	= NoisePoints[0] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
				if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[0].X) < Source.NoiseLockRadius) &&
					(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[0].Y) < Source.NoiseLockRadius) &&
					(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[0].Z) < Source.NoiseLockRadius))
				{
					NoisePoints[0]	= NextNoise[0];
				}
				else
				{
					NoisePoints[0]	= CheckNoisePoint;
				}
			}

			CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[0] * NoiseDistScale);

			// Determine the offset for the leading edge
			Location	= LastDrawPosition;
			EndPoint	= CurrDrawPosition;
			Right		= Location - EndPoint;
			Right.Normalize();
			if (((Source.UpVectorStepSize == 1) && (i == 0)) || (Source.UpVectorStepSize == 0))
			{
				//LastUp = Right ^ ViewDirection;
				LastUp = Right ^ (Location - ViewOrigin);
				if (!LastUp.Normalize())
				{
					LastUp = CameraToWorld.GetScaledAxis( EAxis::Y );
				}
				THE_Up = LastUp;
			}
			else
			{
				LastUp = THE_Up;
			}

			if (SheetIndex)
			{
				Angle			= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
				QuatRotator		= FQuat(Right, Angle);
				WorkingLastUp	= QuatRotator.RotateVector(LastUp);
			}
			else
			{
				WorkingLastUp	= LastUp;
			}

			float	Taper	= 1.0f;

			if (Source.TaperMethod != PEBTM_None)
			{
				check(TaperValues);
				Taper	= TaperValues[0];
			}

			// Size is locked to X, see initialization of Size.
			LastOffset.X	= WorkingLastUp.X * Size.X * Taper;
			LastOffset.Y	= WorkingLastUp.Y * Size.X * Taper;
			LastOffset.Z	= WorkingLastUp.Z * Size.X * Taper;

			// 'Lead' edge
			Vertex->Position	= FVector3f(Location + LastOffset - LWCTileOffset);
			Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
			Vertex->ParticleId	= 0;
			Vertex->Size		= FVector2f(Size);
			Vertex->Tex_U		= fU;
			Vertex->Tex_V		= 0.0f;
			Vertex->Rotation	= Particle->Rotation;
			Vertex->Color		= Particle->Color;
			Vertex++;
			CheckVertexCount++;

			Vertex->Position	= FVector3f(Location - LastOffset - LWCTileOffset);
			Vertex->OldPosition	= FVector3f(Location - LWCTileOffset);
			Vertex->ParticleId	= 0;
			Vertex->Size		= FVector2f(Size);
			Vertex->Tex_U		= fU;
			Vertex->Tex_V		= 1.0f;
			Vertex->Rotation	= Particle->Rotation;
			Vertex->Color		= Particle->Color;
			Vertex++;
			CheckVertexCount++;

			fU	+= TextureIncrement;

			check(InterpolatedPoints);
			for (int32 StepIndex = 0; StepIndex < BeamPayloadData->Steps; StepIndex++)
			{
				// Determine the current position by finding it along the interpolated path and 
				// offsetting with the noise point. 
				if (bInterpFractionIsZero)
				{
					CurrPosition = InterpolatedPoints[StepIndex  * InterpIndex];
				}
				else
				{
					if (StepIndex == (BeamPayloadData->Steps - 1))
					{
						CurrPosition = 
							(InterpolatedPoints[StepIndex * InterpIndex] * (1.0f - InterpFraction)) + 
							FVector(BeamPayloadData->TargetPoint * InterpFraction);
					}
					else
					{
						CurrPosition = 
							(InterpolatedPoints[StepIndex * InterpIndex + 0] * (1.0f - InterpFraction)) + 
							(InterpolatedPoints[StepIndex * InterpIndex + 1] * InterpFraction);
					}
				}


				if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
				{
					NoiseDir		= NextNoise[StepIndex] - NoisePoints[StepIndex];
					NoiseDir.Normalize();
					CheckNoisePoint	= NoisePoints[StepIndex] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
					if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex].X) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex].Y) < Source.NoiseLockRadius) &&
						(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex].Z) < Source.NoiseLockRadius))
					{
						NoisePoints[StepIndex]	= NextNoise[StepIndex];
					}
					else
					{
						NoisePoints[StepIndex]	= CheckNoisePoint;
					}
				}

				CurrDrawPosition	= CurrPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex] * NoiseDistScale);

				// Prep the next draw position to determine tangents
				bool bTarget = false;
				NextTargetPosition	= CurrPosition + (FVector)BeamPayloadData->Direction * BeamPayloadData->StepSize;
				// Determine the current position by finding it along the interpolated path and 
				// offsetting with the noise point. 
				if (bInterpFractionIsZero)
				{
					if (StepIndex == (BeamPayloadData->Steps - 2))
					{
						NextTargetPosition = (FVector)BeamPayloadData->TargetPoint;
					}
					else
					{
						NextTargetPosition = InterpolatedPoints[(StepIndex + 2) * InterpIndex + 0];
					}
				}
				else
				{
					if (StepIndex == (BeamPayloadData->Steps - 1))
					{
						NextTargetPosition = 
							(InterpolatedPoints[(StepIndex + 1) * InterpIndex + 0] * InterpFraction) + 
							FVector(BeamPayloadData->TargetPoint * (1.0f - InterpFraction));
					}
					else
					{
						NextTargetPosition = 
							(InterpolatedPoints[(StepIndex + 1) * InterpIndex + 0] * InterpFraction) + 
							(InterpolatedPoints[(StepIndex + 1) * InterpIndex + 1] * (1.0f - InterpFraction));
					}
				}
				if (bLocked && ((StepIndex + 1) == BeamPayloadData->Steps))
				{
					// If we are locked, and the next step is the target point, set the draw position as such.
					// (ie, we are on the last noise point...)
					NextTargetDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
					if (Source.bTargetNoise)
					{
						if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
						{
							NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
							NoiseDir.Normalize();
							CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
							if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
								(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
							{
								NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
							}
							else
							{
								NoisePoints[Source.Frequency]	= CheckNoisePoint;
							}
						}

						NextTargetDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
					}
					TargetTangent = (FVector)BeamPayloadData->TargetTangent;
					fTargetStrength	= Source.NoiseTangentStrength;
				}
				else
				{
					// Just another noise point... offset the target to get the draw position.
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[StepIndex + 1] - NoisePoints[StepIndex + 1];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[StepIndex + 1] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
						if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[StepIndex + 1].X) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[StepIndex + 1].Y) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[StepIndex + 1].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[StepIndex + 1]	= NextNoise[StepIndex + 1];
						}
						else
						{
							NoisePoints[StepIndex + 1]	= CheckNoisePoint;
						}
					}

					NextTargetDrawPosition	= NextTargetPosition + NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[StepIndex + 1] * NoiseDistScale);

					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
				}
				TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * (NextTargetDrawPosition - LastDrawPosition);
				TargetTangent.Normalize();
				TargetTangent *= fTargetStrength;

				InterimDrawPosition = LastDrawPosition;
				// Tessellate between the current position and the last position
				for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = FMath::CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;
					Right		= Location - EndPoint;
					Right.Normalize();
					if (Source.UpVectorStepSize == 0)
					{
						//Up = Right ^  (Location - CameraToWorld.GetOrigin());
						Up = Right ^ (Location - ViewOrigin);
						if (!Up.Normalize())
						{
							Up = CameraToWorld.GetScaledAxis( EAxis::Y );
						}
					}
					else
					{
						Up = THE_Up;
					}

					if (SheetIndex)
					{
						Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
						QuatRotator	= FQuat(Right, Angle);
						WorkingUp	= QuatRotator.RotateVector(Up);
					}
					else
					{
						WorkingUp	= Up;
					}

					if (Source.TaperMethod != PEBTM_None)
					{
						check(TaperValues);
						Taper	= TaperValues[StepIndex * TessFactor + TessIndex];
					}

					// Size is locked to X, see initialization of Size.
					Offset.X	= WorkingUp.X * Size.X * Taper;
					Offset.Y	= WorkingUp.Y * Size.X * Taper;
					Offset.Z	= WorkingUp.Z * Size.X * Taper;

					// Generate the vertex
					Vertex->Position	= FVector3f(InterpDrawPos + Offset - LWCTileOffset);
					Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size		= FVector2f(Size);
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 0.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					Vertex->Position	= FVector3f(InterpDrawPos - Offset - LWCTileOffset);
					Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size		= FVector2f(Size);
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 1.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					fU	+= TextureIncrement;
					InterimDrawPosition	= InterpDrawPos;
				}
				LastPosition		= CurrPosition;
				LastDrawPosition	= CurrDrawPosition;
				LastTangent			= TargetTangent;
			}

			if (bLocked)
			{
				// Draw the line from the last point to the target
				CurrDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
				if (Source.bTargetNoise)
				{
					if ((Source.NoiseLockTime >= 0.0f) && Source.bSmoothNoise_Enabled)
					{
						NoiseDir		= NextNoise[Source.Frequency] - NoisePoints[Source.Frequency];
						NoiseDir.Normalize();
						CheckNoisePoint	= NoisePoints[Source.Frequency] + NoiseDir * (FVector)Source.NoiseSpeed * *NoiseRate;
						if ((FMath::Abs<float>(CheckNoisePoint.X - NextNoise[Source.Frequency].X) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Y - NextNoise[Source.Frequency].Y) < Source.NoiseLockRadius) &&
							(FMath::Abs<float>(CheckNoisePoint.Z - NextNoise[Source.Frequency].Z) < Source.NoiseLockRadius))
						{
							NoisePoints[Source.Frequency]	= NextNoise[Source.Frequency];
						}
						else
						{
							NoisePoints[Source.Frequency]	= CheckNoisePoint;
						}
					}

					CurrDrawPosition += NoiseRangeScaleFactor * LocalToWorld.TransformVector(NoisePoints[Source.Frequency] * NoiseDistScale);
				}

				NextTargetDrawPosition	= (FVector)BeamPayloadData->TargetPoint;
				if (Source.bUseTarget)
				{
					TargetTangent = (FVector)BeamPayloadData->TargetTangent;
				}
				else
				{
					TargetTangent = ((1.0f - Source.NoiseTension) / 2.0f) * 
						(NextTargetDrawPosition - LastDrawPosition);
					TargetTangent.Normalize();
				}
				TargetTangent *= fTargetStrength;

				// Tessellate this segment
				InterimDrawPosition = LastDrawPosition;
				for (int32 TessIndex = 0; TessIndex < TessFactor; TessIndex++)
				{
					InterpDrawPos = FMath::CubicInterp(
						LastDrawPosition, LastTangent,
						CurrDrawPosition, TargetTangent,
						InvTessFactor * (TessIndex + 1));

					Location	= InterimDrawPosition;
					EndPoint	= InterpDrawPos;
					Right		= Location - EndPoint;
					Right.Normalize();
					if (Source.UpVectorStepSize == 0)
					{
						//Up = Right ^  (Location - CameraToWorld.GetOrigin());
						Up = Right ^ (Location - ViewOrigin);
						if (!Up.Normalize())
						{
							Up = CameraToWorld.GetScaledAxis( EAxis::Y );
						}
					}
					else
					{
						Up = THE_Up;
					}

					if (SheetIndex)
					{
						Angle		= ((float)UE_PI / (float)Source.Sheets) * SheetIndex;
						QuatRotator	= FQuat(Right, Angle);
						WorkingUp	= QuatRotator.RotateVector(Up);
					}
					else
					{
						WorkingUp	= Up;
					}

					if (Source.TaperMethod != PEBTM_None)
					{
						check(TaperValues);
						Taper	= TaperValues[BeamPayloadData->Steps * TessFactor + TessIndex];
					}

					// Size is locked to X, see initialization of Size.
					Offset.X	= WorkingUp.X * Size.X * Taper;
					Offset.Y	= WorkingUp.Y * Size.X * Taper;
					Offset.Z	= WorkingUp.Z * Size.X * Taper;

					// Generate the vertex
					Vertex->Position	= FVector3f(InterpDrawPos + Offset - LWCTileOffset);
					Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size		= FVector2f(Size);
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 0.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					Vertex->Position	= FVector3f(InterpDrawPos - Offset - LWCTileOffset);
					Vertex->OldPosition	= FVector3f(InterpDrawPos - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size		= FVector2f(Size);
					Vertex->Tex_U		= fU;
					Vertex->Tex_V		= 1.0f;
					Vertex->Rotation	= Particle->Rotation;
					Vertex->Color		= Particle->Color;
					Vertex++;
					CheckVertexCount++;

					fU	+= TextureIncrement;
					InterimDrawPosition	= InterpDrawPos;
				}
			}
			else
			if (BeamPayloadData->TravelRatio > UE_KINDA_SMALL_NUMBER)
			{
				//@todo.SAS. Re-implement partial-segment beams
			}
		}
	}

	check(CheckVertexCount <= Source.VertexCount);

	return TrianglesToRender;
}

//
//	FDynamicTrailsEmitterData
//


FDynamicTrailsEmitterData::~FDynamicTrailsEmitterData()
{
}


/** Dynamic emitter data for Ribbon emitters */
/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicTrailsEmitterData::Init(bool bInSelected)
{
	bSelected = bInSelected;

	ensure(SourcePointer->ActiveParticleCount < (16 * 1024));	// TTP #33330
	ensure(SourcePointer->ParticleStride < (2 * 1024));			// TTP #33330

	MaterialResource = SourcePointer->MaterialInterface->GetRenderProxy();

	bUsesDynamicParameter = GetSourceData()->DynamicParameterDataOffset > 0;

	// We won't need this on the render thread
	SourcePointer->MaterialInterface = NULL;
}

void FDynamicTrailsEmitterData::GetDynamicMeshElementsEmitter(const FParticleSystemSceneProxy* Proxy, const FSceneView* View, const FSceneViewFamily& ViewFamily, int32 ViewIndex, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_TrailRenderingTime);
	INC_DWORD_STAT(STAT_TrailParticlesRenderCalls);

	if (bValid == false)
	{
		return;
	}

	if ((SourcePointer->VertexCount <= 0) || (SourcePointer->ActiveParticleCount <= 0) || (SourcePointer->IndexCount < 3))
	{
		return;
	}
	const bool bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
	FIndexBuffer* IndexBuffer = nullptr;
	uint32 FirstIndex = 0;
	int32 OutTriangleCount = 0;
	int32 RenderedPrimitiveCount = 0;

	FGlobalDynamicVertexBuffer::FAllocation DynamicVertexAllocation;
	FGlobalDynamicIndexBuffer::FAllocation DynamicIndexAllocation;
	FGlobalDynamicVertexBuffer::FAllocation DynamicParameterAllocation;
	FAsyncBufferFillData Data;

	auto FeatureLevel = ViewFamily.GetFeatureLevel();
	const int32 VertexStride = GetDynamicVertexStride(FeatureLevel);
	const int32 DynamicParameterVertexStride = bUsesDynamicParameter ? GetDynamicParameterVertexStride() : 0;

	DoBufferFill(Data);
	BuildViewFillData(Proxy, View, SourcePointer->VertexCount, VertexStride, DynamicParameterVertexStride,
		Collector.GetDynamicIndexBuffer(), Collector.GetDynamicVertexBuffer(),
		DynamicVertexAllocation, DynamicIndexAllocation, &DynamicParameterAllocation, Data);
	DoBufferFill(Data);
	OutTriangleCount = Data.OutTriangleCount;

	// Early out, nothing to render
	if (OutTriangleCount == 0 || !bRenderGeometry)
	{
		return;
	}

	if (SourcePointer->bUseLocalSpace == false)
	{
		Proxy->UpdateWorldSpacePrimitiveUniformBuffer();
	}

	FDynamicBeamTrailCollectorResources& CollectorResources = Collector.AllocateOneFrameResource<FDynamicBeamTrailCollectorResources>(FeatureLevel);
	FParticleBeamTrailVertexFactory* BeamTrailVertexFactory = &CollectorResources.VertexFactory;
	BeamTrailVertexFactory->SetParticleFactoryType(PVFT_BeamTrail);
	BeamTrailVertexFactory->SetUsesDynamicParameter(bUsesDynamicParameter);
	BeamTrailVertexFactory->InitResource();

	// Create and set the uniform buffer for this emitter.
	BeamTrailVertexFactory->SetBeamTrailUniformBuffer(CreateBeamTrailUniformBuffer(Proxy, SourcePointer, View));
	BeamTrailVertexFactory->SetVertexBuffer(DynamicVertexAllocation.VertexBuffer, DynamicVertexAllocation.VertexOffset, GetDynamicVertexStride(View->GetFeatureLevel()));
	BeamTrailVertexFactory->SetDynamicParameterBuffer(DynamicParameterAllocation.IsValid() ? DynamicParameterAllocation.VertexBuffer : NULL, DynamicParameterAllocation.IsValid() ? DynamicParameterAllocation.VertexOffset : 0, GetDynamicParameterVertexStride());
	IndexBuffer = DynamicIndexAllocation.IndexBuffer;
	FirstIndex = DynamicIndexAllocation.FirstIndex;

	BeamTrailVertexFactory->GetIndexBuffer() = IndexBuffer;
	BeamTrailVertexFactory->GetFirstIndex() = FirstIndex;
	BeamTrailVertexFactory->GetOutTriangleCount() = OutTriangleCount;

	// Create mesh batch
	FMeshBatch& Mesh = Collector.AllocateMesh();
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = IndexBuffer;
	BatchElement.FirstIndex = FirstIndex;
	Mesh.VertexFactory = BeamTrailVertexFactory;
	Mesh.LCI = NULL;

	BatchElement.PrimitiveUniformBuffer = SourcePointer->bUseLocalSpace ? Proxy->GetUniformBuffer() : Proxy->GetWorldSpacePrimitiveUniformBuffer();
	BatchElement.NumPrimitives = OutTriangleCount;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = SourcePointer->VertexCount - 1;
	Mesh.ReverseCulling = Proxy->IsLocalToWorldDeterminantNegative();
	Mesh.CastShadow = Proxy->GetCastShadow();
	Mesh.DepthPriorityGroup = (ESceneDepthPriorityGroup)Proxy->GetDepthPriorityGroup(View);

	if (AllowDebugViewmodes() && bIsWireframe && !ViewFamily.EngineShowFlags.Materials)
	{
		Mesh.MaterialRenderProxy = Proxy->GetDeselectedWireframeMatInst();
	}
	else
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		if (OutTriangleCount != SourcePointer->PrimitiveCount)
		{
			UE_LOG(LogParticles, Log, TEXT("Data.OutTriangleCount = %4d vs. SourcePrimCount = %4d"), OutTriangleCount, SourcePointer->PrimitiveCount);

			int32 CheckTrailCount = 0;
			int32 CheckTriangleCount = 0;
			for (int32 ParticleIdx = 0; ParticleIdx < SourcePointer->ActiveParticleCount; ParticleIdx++)
			{
				int32 CurrentIndex = SourcePointer->DataContainer.ParticleIndices[ParticleIdx];
				DECLARE_PARTICLE_PTR(CheckParticle, SourcePointer->DataContainer.ParticleData + SourcePointer->ParticleStride * CurrentIndex);
				FTrailsBaseTypeDataPayload* TrailPayload = (FTrailsBaseTypeDataPayload*)((uint8*)CheckParticle + SourcePointer->TrailDataOffset);
				if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == false)
				{
					continue;
				}

				UE_LOG(LogParticles, Log, TEXT("Trail %2d has %5d triangles"), TrailPayload->TrailIndex, TrailPayload->TriangleCount);
				CheckTriangleCount += TrailPayload->TriangleCount;
				CheckTrailCount++;
			}
			UE_LOG(LogParticles, Log, TEXT("Total 'live' trail count = %d"), CheckTrailCount);
			UE_LOG(LogParticles, Log, TEXT("\t%5d triangles total (not counting degens)"), CheckTriangleCount);
		}
#endif
		checkf(OutTriangleCount <= SourcePointer->PrimitiveCount, TEXT("Data.OutTriangleCount = %4d vs. SourcePrimCount = %4d"), OutTriangleCount, SourcePointer->PrimitiveCount);
		Mesh.MaterialRenderProxy = MaterialResource;
	}
	Mesh.Type = PT_TriangleStrip;

	Mesh.bCanApplyViewModeOverrides = true;
	Mesh.bUseWireframeSelectionColoring = Proxy->IsSelected();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	Mesh.VisualizeLODIndex = (int8)Proxy->GetVisualizeLODIndex();
#endif

	// attempt to prevent crash seen in FORT-116687
	if (ensure(Mesh.MaterialRenderProxy != nullptr))
	{
		Collector.AddMesh(ViewIndex, Mesh);
		RenderedPrimitiveCount = Mesh.GetNumPrimitives();
	}

	RenderDebug(Proxy, Collector.GetPDI(ViewIndex), View, false);

	INC_DWORD_STAT_BY(STAT_TrailParticlesTrianglesRendered, RenderedPrimitiveCount);
}

void FDynamicTrailsEmitterData::RenderDebug(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, bool bCrosses) const
{
	// Can't do anything in here...
}

void FDynamicTrailsEmitterData::GetIndexAllocInfo( int32& OutNumIndices, int32& OutStride ) const
{
	OutNumIndices = SourcePointer->IndexCount;
	OutStride = SourcePointer->IndexStride;
}

// Data fill functions
int32 FDynamicTrailsEmitterData::FillIndexData(struct FAsyncBufferFillData& Data) const
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillIndexTime);

	int32	TrianglesToRender = 0;

	// Trails polygons are packed and joined as follows:
	//
	// 1--3--5--7--9-...
	// |\ |\ |\ |\ |\...
	// | \| \| \| \| ...
	// 0--2--4--6--8-...
	//
	// (ie, the 'leading' edge of polygon (n) is the trailing edge of polygon (n+1)
	//

	int32	Sheets = 1;
	int32	TessFactor = 1;//FMath::Max<int32>(Source.TessFactor, 1);

	bool bWireframe = (Data.View->Family->EngineShowFlags.Wireframe && !Data.View->Family->EngineShowFlags.Materials);

	int32	CheckCount	= 0;

	uint16*	Index		= (uint16*)Data.IndexData;
	uint16	VertexIndex	= 0;

	int32 CurrentTrail = 0;
	for (int32 ParticleIdx = 0; ParticleIdx < SourcePointer->ActiveParticleCount; ParticleIdx++)
	{
		int32 CurrentIndex = SourcePointer->DataContainer.ParticleIndices[ParticleIdx];
		DECLARE_PARTICLE_PTR(Particle, SourcePointer->DataContainer.ParticleData + SourcePointer->ParticleStride * CurrentIndex);

		FTrailsBaseTypeDataPayload* TrailPayload = (FTrailsBaseTypeDataPayload*)((uint8*)Particle + SourcePointer->TrailDataOffset);
		if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == false)
		{
			continue;
		}

		int32 LocalTrianglesToRender = TrailPayload->TriangleCount;
		if (LocalTrianglesToRender == 0)
		{
			continue;
		}

		//@todo. Support clip source segment

		// For the source particle itself
		if (CurrentTrail == 0)
		{
			*(Index++) = VertexIndex++;		// The first vertex..
			*(Index++) = VertexIndex++;		// The second index..
			CheckCount += 2;
		}
		else
		{
			// Add the verts to join this trail with the previous one
			*(Index++) = VertexIndex - 1;	// Last vertex of the previous sheet
			*(Index++) = VertexIndex;		// First vertex of the next sheet
			*(Index++) = VertexIndex++;		// First vertex of the next sheet
			*(Index++) = VertexIndex++;		// Second vertex of the next sheet
			TrianglesToRender += 4;
			CheckCount += 4;
		}

		for (int32 LocalIdx = 0; LocalIdx < LocalTrianglesToRender; LocalIdx++)
		{
			*(Index++) = VertexIndex++;
			CheckCount++;
			TrianglesToRender++;
		}

		//@todo. Support sheets!

		CurrentTrail++;
	}

	Data.OutTriangleCount = TrianglesToRender;
	return TrianglesToRender;
}

int32 FDynamicTrailsEmitterData::FillVertexData(struct FAsyncBufferFillData& Data) const
{
	check(!TEXT("FillVertexData: Base implementation should NOT be called!"));
	return 0;
}

//
//	FDynamicRibbonEmitterData
//
/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicRibbonEmitterData::Init(bool bInSelected)
{
	SourcePointer = &Source;
	FDynamicTrailsEmitterData::Init(bInSelected);
	
	bUsesDynamicParameter = GetSourceData()->DynamicParameterDataOffset > 0;
}

void FDynamicRibbonEmitterData::RenderDebug(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, bool bCrosses) const
{
	if ((bRenderParticles == true) || (bRenderTangents == true))
	{
		// DEBUGGING
		// Draw all the points of the trail(s)
		FVector DrawPosition;
		float DrawSize;
		FColor DrawColor;
		FColor PrevDrawColor;
		FVector DrawTangentEnd;

		const uint8* Address = Source.DataContainer.ParticleData;
		const FRibbonTypeDataPayload* StartTrailPayload;
		const FRibbonTypeDataPayload* EndTrailPayload = NULL;
		const FBaseParticle* DebugParticle;
		const FRibbonTypeDataPayload* TrailPayload;
		const FBaseParticle* PrevParticle = NULL;
		const FRibbonTypeDataPayload* PrevTrailPayload;

		for (int32 ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
		{
			DECLARE_PARTICLE_PTR(Particle, Address + Source.ParticleStride * Source.DataContainer.ParticleIndices[ParticleIdx]);
			StartTrailPayload = (FRibbonTypeDataPayload*)((uint8*)Particle + Source.TrailDataOffset);
			if (TRAIL_EMITTER_IS_HEAD(StartTrailPayload->Flags) == 0)
			{
				continue;
			}

			// Pin the size to the X component
			float Increment = 1.0f / (StartTrailPayload->TriangleCount / 2.0f);
			float ColorScale = 0.0f;

			DebugParticle = Particle;
			// Find the end particle in this chain...
			TrailPayload = StartTrailPayload;
			const FBaseParticle* IteratorParticle = DebugParticle;
			while (TrailPayload)
			{
				int32	Next = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
				if (Next == TRAIL_EMITTER_NULL_NEXT)
				{
					DebugParticle = IteratorParticle;
					EndTrailPayload = TrailPayload;
					TrailPayload = NULL;
				}
				else
				{
					DECLARE_PARTICLE_PTR(TempParticle, Address + Source.ParticleStride * Next);
					IteratorParticle = TempParticle;
					TrailPayload = (FRibbonTypeDataPayload*)((uint8*)IteratorParticle + Source.TrailDataOffset);
				}
			}
			if (EndTrailPayload != StartTrailPayload)
			{
				const FBaseParticle* CurrSpawnedParticle = NULL;
				const FBaseParticle* NextSpawnedParticle = NULL;
				// We have more than one particle in the trail...
				TrailPayload = EndTrailPayload;

				if (TrailPayload->bInterpolatedSpawn == false)
				{
					CurrSpawnedParticle = DebugParticle;
				}
				while (TrailPayload)
				{
					int32	Prev = TRAIL_EMITTER_GET_PREV(TrailPayload->Flags);
					if (Prev == TRAIL_EMITTER_NULL_PREV)
					{
						PrevParticle = NULL;
						PrevTrailPayload = NULL;
					}
					else
					{
						DECLARE_PARTICLE_PTR(TempParticle, Address + Source.ParticleStride * Prev);
						PrevParticle = TempParticle;
						PrevTrailPayload = (FRibbonTypeDataPayload*)((uint8*)PrevParticle + Source.TrailDataOffset);
					}

					if (PrevTrailPayload && PrevTrailPayload->bInterpolatedSpawn == false)
					{
						if (CurrSpawnedParticle == NULL)
						{
							CurrSpawnedParticle = PrevParticle;
						}
						else
						{
							NextSpawnedParticle = PrevParticle;
						}
					}

					DrawPosition = DebugParticle->Location;
					DrawSize = DebugParticle->Size.X * Source.Scale.X;
					int32 Red   = FMath::TruncToInt(255.0f * (1.0f - ColorScale));
					int32 Green = FMath::TruncToInt(255.0f * ColorScale);
					ColorScale += Increment;
					DrawColor = FColor(Red,Green,0);
					Red   = FMath::TruncToInt(255.0f * (1.0f - ColorScale));
					Green = FMath::TruncToInt(255.0f * ColorScale);
					PrevDrawColor = FColor(Red,Green,0);

					if (bRenderParticles == true)
					{
						if (TrailPayload->bInterpolatedSpawn == false)
						{
							DrawWireStar(PDI, DrawPosition, DrawSize, FColor::Red, Proxy->GetDepthPriorityGroup(View));
						}
						else
						{
							DrawWireStar(PDI, DrawPosition, DrawSize, FColor::Green, Proxy->GetDepthPriorityGroup(View));
						}

						//
						if (bRenderTessellation == true)
						{
							if (PrevParticle != NULL)
							{
								// Draw a straight line between the particles
								// This will allow us to visualize the tessellation difference
								PDI->DrawLine(DrawPosition, PrevParticle->Location, FColor::Blue, Proxy->GetDepthPriorityGroup(View));
								int32 InterpCount = TrailPayload->RenderingInterpCount;
								// Interpolate between current and next...
								FVector LineStart = DrawPosition;
								float Diff = PrevTrailPayload->SpawnTime - TrailPayload->SpawnTime;
								FVector CurrUp = FVector(0.0f, 0.0f, 1.0f);
								float InvCount = 1.0f / InterpCount;
								FLinearColor StartColor = DrawColor;
								FLinearColor EndColor = PrevDrawColor;
								for (int32 SpawnIdx = 0; SpawnIdx < InterpCount; SpawnIdx++)
								{
									float TimeStep = InvCount * SpawnIdx;
									FVector LineEnd = FMath::CubicInterp<FVector>(
										DebugParticle->Location, (FVector)TrailPayload->Tangent,
										PrevParticle->Location, (FVector)PrevTrailPayload->Tangent,
										TimeStep);
									FLinearColor InterpColor = FMath::Lerp<FLinearColor>(StartColor, EndColor, TimeStep);
									PDI->DrawLine(LineStart, LineEnd, InterpColor, Proxy->GetDepthPriorityGroup(View));
									if (SpawnIdx > 0)
									{
										InterpColor.R = 1.0f - TimeStep;
										InterpColor.G = 1.0f - TimeStep;
										InterpColor.B = 1.0f - (1.0f - TimeStep);
									}
									DrawWireStar(PDI, LineEnd, DrawSize * 0.3f, InterpColor, Proxy->GetDepthPriorityGroup(View));
									LineStart = LineEnd;
								}
								PDI->DrawLine(LineStart, PrevParticle->Location, EndColor, Proxy->GetDepthPriorityGroup(View));
							}
						}
					}

					if (bRenderTangents == true)
					{
						DrawTangentEnd = DrawPosition + (FVector)TrailPayload->Tangent;
						if (TrailPayload == StartTrailPayload)
						{
							PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(0.0f, 1.0f, 0.0f), Proxy->GetDepthPriorityGroup(View));
						}
						else if (TrailPayload == EndTrailPayload)
						{
							PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(1.0f, 0.0f, 0.0f), Proxy->GetDepthPriorityGroup(View));
						}
						else
						{
							PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(1.0f, 1.0f, 0.0f), Proxy->GetDepthPriorityGroup(View));
						}
					}

					// The end will have Next set to the NULL flag...
					if (PrevParticle != NULL)
					{
						DebugParticle = PrevParticle;
						TrailPayload = PrevTrailPayload;
					}
					else
					{
						TrailPayload = NULL;
					}
				}
			}
		}
	}
}

// Data fill functions
int32 FDynamicRibbonEmitterData::FillVertexData(struct FAsyncBufferFillData& Data) const
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillVertexTime);

	int32	TrianglesToRender = 0;

	uint8* TempVertexData = (uint8*)Data.VertexData;
	uint8* TempDynamicParamData = (uint8*)Data.DynamicParameterData;
	FParticleBeamTrailVertex* Vertex;
	FParticleBeamTrailVertexDynamicParameter* DynParamVertex;

	FMatrix CameraToWorld = Data.View->ViewMatrices.GetInvViewMatrix();
	FVector CameraUp = CameraToWorld.TransformVector(FVector(0,0,1));
	FVector	ViewOrigin	= CameraToWorld.GetOrigin();

	int32 MaxTessellationBetweenParticles = FMath::Max<int32>(Source.MaxTessellationBetweenParticles, 1);
	int32 Sheets = 1;

	bool bUseDynamic = bUsesDynamicParameter && TempDynamicParamData != nullptr;

	// The distance tracking for tiling the 2nd UV set
	float CurrDistance = 0.0f;

	FBaseParticle* PackingParticle;
	const uint8* ParticleData = Source.DataContainer.ParticleData;
	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();
	for (int32 ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + Source.ParticleStride * Source.DataContainer.ParticleIndices[ParticleIdx]);
		FRibbonTypeDataPayload* TrailPayload = (FRibbonTypeDataPayload*)((uint8*)Particle + Source.TrailDataOffset);
		if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0)
		{
			continue;
		}

		if (TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags) == TRAIL_EMITTER_NULL_NEXT)
		{
			continue;
		}

		PackingParticle = Particle;
		// Pin the size to the X component
		FLinearColor CurrLinearColor = PackingParticle->Color;
		// The increment for going [0..1] along the complete trail
		float TextureIncrement = 1.0f / (TrailPayload->TriangleCount / 2.0f);
		float Tex_U = 0.0f;
		FVector CurrTilePosition = PackingParticle->Location;
		FVector PrevTilePosition = PackingParticle->Location;
		FVector PrevWorkingUp(0,0,1);
		int32 VertexStride = sizeof(FParticleBeamTrailVertex);
		int32 DynamicParameterStride = 0;
		bool bFillDynamic = false;
		if (bUseDynamic)
		{
			DynamicParameterStride = sizeof(FParticleBeamTrailVertexDynamicParameter);
			if (Source.DynamicParameterDataOffset > 0)
			{
				bFillDynamic = true;
			}
		}
		float CurrTileU;
		FEmitterDynamicParameterPayload* CurrDynPayload = NULL;
		FEmitterDynamicParameterPayload* PrevDynPayload = NULL;
		FBaseParticle* PrevParticle = NULL;
		FRibbonTypeDataPayload* PrevTrailPayload = NULL;

		FVector WorkingUp = (FVector)TrailPayload->Up;
		if (RenderAxisOption == Trails_CameraUp)
		{
			FVector DirToCamera = PackingParticle->Location - ViewOrigin;
			DirToCamera.Normalize();
			FVector NormailzedTangent = (FVector)TrailPayload->Tangent;
			NormailzedTangent.Normalize();
			WorkingUp = NormailzedTangent ^ DirToCamera;
			if (WorkingUp.IsNearlyZero())
			{
				WorkingUp = CameraUp;
			}

			WorkingUp.Normalize();
		}

		while (TrailPayload)
		{
			float CurrSize = PackingParticle->Size.X * Source.Scale.X;

			int32 InterpCount = TrailPayload->RenderingInterpCount;
			if (InterpCount > 1)
			{
				check(PrevParticle);
				check(TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0);

				// Interpolate between current and next...
				FVector CurrPosition = PackingParticle->Location;
				FVector CurrTangent = (FVector)TrailPayload->Tangent;
				FVector CurrUp = WorkingUp;
				FLinearColor CurrColor = PackingParticle->Color;

				FVector PrevPosition = PrevParticle->Location; //-V522
				FVector PrevTangent = (FVector)PrevTrailPayload->Tangent; //-V522
				FVector PrevUp = PrevWorkingUp;
				FLinearColor PrevColor = PrevParticle->Color;
				float PrevSize = PrevParticle->Size.X * Source.Scale.X;

				float InvCount = 1.0f / InterpCount;
				float Diff = PrevTrailPayload->SpawnTime - TrailPayload->SpawnTime;
				
				FVector4f CurrDynParam;
				FVector4f PrevDynParam;
				if (bFillDynamic)
				{
					GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *PackingParticle, CurrDynParam);
					GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *PrevParticle, PrevDynParam);
				}

				FVector4f InterpDynamic(1.0f, 1.0f, 1.0f, 1.0f);
				for (int32 SpawnIdx = InterpCount - 1; SpawnIdx >= 0; SpawnIdx--)
				{
					float TimeStep = InvCount * SpawnIdx;
					FVector InterpPos = FMath::CubicInterp<FVector>(CurrPosition, CurrTangent, PrevPosition, PrevTangent, TimeStep);
					FVector InterpUp = FMath::Lerp<FVector>(CurrUp, PrevUp, TimeStep);
					FLinearColor InterpColor = FMath::Lerp<FLinearColor>(CurrColor, PrevColor, TimeStep);
					float InterpSize = FMath::Lerp<float>(CurrSize, PrevSize, TimeStep);
					if (bFillDynamic)
					{
						InterpDynamic = FMath::Lerp<FVector4f>(CurrDynParam, PrevDynParam, TimeStep);
					}

					if (bTextureTileDistance == true)	
					{
						CurrTileU = FMath::Lerp<float>(TrailPayload->TiledU, PrevTrailPayload->TiledU, TimeStep);
					}
					else
					{
						CurrTileU = Tex_U;
					}

					FVector FinalPos = InterpPos + InterpUp * InterpSize;
					//if (Source.bUseLocalSpace)
					//{
					//	FinalPos += Data.LocalToWorld.GetOrigin();
					//}
					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = FVector3f(FinalPos - LWCTileOffset);
					Vertex->OldPosition = FVector3f(FinalPos - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Tex_U = Tex_U;
					Vertex->Tex_V = 0.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 0.0f;
					Vertex->Rotation = PackingParticle->Rotation;
					Vertex->Color = InterpColor;
					if (bUseDynamic)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
						TempDynamicParamData += DynamicParameterStride;
					}
					TempVertexData += VertexStride;
					//PackedVertexCount++;

					FinalPos = InterpPos - InterpUp * InterpSize;
					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = FVector3f(FinalPos - LWCTileOffset);
					Vertex->OldPosition = FVector3f(FinalPos - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Tex_U = Tex_U;
					Vertex->Tex_V = 1.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 1.0f;
					Vertex->Rotation = PackingParticle->Rotation;
					Vertex->Color = InterpColor;
					if (bUseDynamic)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
						TempDynamicParamData += DynamicParameterStride;
					}
					TempVertexData += VertexStride;
					//PackedVertexCount++;

					Tex_U += TextureIncrement;
				}
			}
			else
			{
				if (bFillDynamic == true)
				{
					CurrDynPayload = ((FEmitterDynamicParameterPayload*)((uint8*)(PackingParticle) + Source.DynamicParameterDataOffset));
				}

				if (bTextureTileDistance == true)
				{
					CurrTileU = TrailPayload->TiledU;
				}
				else
				{
					CurrTileU = Tex_U;
				}

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = FVector3f(PackingParticle->Location - LWCTileOffset + WorkingUp * CurrSize);
				Vertex->OldPosition = FVector3f(PackingParticle->OldLocation - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size.X = CurrSize;
				Vertex->Size.Y = CurrSize;
				Vertex->Tex_U = Tex_U;
				Vertex->Tex_V = 0.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 0.0f;
				Vertex->Rotation = PackingParticle->Rotation;
				Vertex->Color = PackingParticle->Color;
				if (bUseDynamic)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
					if (CurrDynPayload != NULL)
					{
						DynParamVertex->DynamicValue[0] = CurrDynPayload->DynamicParameterValue[0];
						DynParamVertex->DynamicValue[1] = CurrDynPayload->DynamicParameterValue[1];
						DynParamVertex->DynamicValue[2] = CurrDynPayload->DynamicParameterValue[2];
						DynParamVertex->DynamicValue[3] = CurrDynPayload->DynamicParameterValue[3];
					}
					else
					{
						DynParamVertex->DynamicValue[0] = 1.0f;
						DynParamVertex->DynamicValue[1] = 1.0f;
						DynParamVertex->DynamicValue[2] = 1.0f;
						DynParamVertex->DynamicValue[3] = 1.0f;
					}
					TempDynamicParamData += DynamicParameterStride;
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = FVector3f(PackingParticle->Location - LWCTileOffset - WorkingUp * CurrSize);
				Vertex->OldPosition = FVector3f(PackingParticle->OldLocation - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size.X = CurrSize;
				Vertex->Size.Y = CurrSize;
				Vertex->Tex_U = Tex_U;
				Vertex->Tex_V = 1.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 1.0f;
				Vertex->Rotation = PackingParticle->Rotation;
				Vertex->Color = PackingParticle->Color;
				if (bUseDynamic)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
					if (CurrDynPayload != NULL)
					{
						DynParamVertex->DynamicValue[0] = CurrDynPayload->DynamicParameterValue[0];
						DynParamVertex->DynamicValue[1] = CurrDynPayload->DynamicParameterValue[1];
						DynParamVertex->DynamicValue[2] = CurrDynPayload->DynamicParameterValue[2];
						DynParamVertex->DynamicValue[3] = CurrDynPayload->DynamicParameterValue[3];
					}
					else
					{
						DynParamVertex->DynamicValue[0] = 1.0f;
						DynParamVertex->DynamicValue[1] = 1.0f;
						DynParamVertex->DynamicValue[2] = 1.0f;
						DynParamVertex->DynamicValue[3] = 1.0f;
					}
					TempDynamicParamData += DynamicParameterStride;
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Tex_U += TextureIncrement;
			}

			PrevParticle = PackingParticle;
			PrevTrailPayload = TrailPayload;
			PrevWorkingUp = WorkingUp;

			int32	NextIdx = TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags);
			if (NextIdx == TRAIL_EMITTER_NULL_NEXT)
			{
				TrailPayload = NULL;
				PackingParticle = NULL;
			}
			else
			{
				DECLARE_PARTICLE_PTR(TempParticle, ParticleData + Source.ParticleStride * NextIdx);
				PackingParticle = TempParticle;
				TrailPayload = (FRibbonTypeDataPayload*)((uint8*)TempParticle + Source.TrailDataOffset);
				WorkingUp = (FVector)TrailPayload->Up;
				if (RenderAxisOption == Trails_CameraUp)
				{
					FVector DirToCamera = PackingParticle->Location - ViewOrigin;
					DirToCamera.Normalize();
					FVector NormailzedTangent = (FVector)TrailPayload->Tangent;
					NormailzedTangent.Normalize();
					WorkingUp = NormailzedTangent ^ DirToCamera;
					if (WorkingUp.IsNearlyZero())
					{
						WorkingUp = CameraUp;
					}
					WorkingUp.Normalize();
				}
			}
		}
	}

	return TrianglesToRender;
}

///////////////////////////////////////////////////////////////////////////////
/** Dynamic emitter data for AnimTrail emitters */
/** Initialize this emitter's dynamic rendering data, called after source data has been filled in */
void FDynamicAnimTrailEmitterData::Init(bool bInSelected)
{
	SourcePointer = &Source;
	FDynamicTrailsEmitterData::Init(bInSelected);
}


float GCatmullRomEndParamOffset = 0.1f;
static FAutoConsoleVariableRef CatmullRomEndParamOffset(
	TEXT("r.CatmullRomEndParamOffset"),
	GCatmullRomEndParamOffset,
	TEXT("The parameter offset for catmul rom end points.")
	);

/**
Helper class for keeping track of all the particles being used for vertex generation.
*/
struct FAnimTrailParticleRenderData
{
	const FDynamicTrailsEmitterReplayData& Source;
	const uint8* ParticleDataAddress;

	const FBaseParticle* PrevPrevParticle;
	const FAnimTrailTypeDataPayload* PrevPrevPayload;
	const FBaseParticle* PrevParticle;
	const FAnimTrailTypeDataPayload* PrevPayload;
	const FBaseParticle* Particle;
	const FAnimTrailTypeDataPayload* Payload;
	const FBaseParticle* NextParticle;
	const FAnimTrailTypeDataPayload* NextPayload;
	
	FAnimTrailParticleRenderData( const FDynamicTrailsEmitterReplayData& InSource, const FBaseParticle* InParticle, const FAnimTrailTypeDataPayload* InPayload )
		:	Source(InSource),
			ParticleDataAddress(Source.DataContainer.ParticleData),
			PrevPrevParticle(NULL),
			PrevPrevPayload(NULL),
			PrevParticle(NULL),
			PrevPayload(NULL),
			Particle(InParticle),
			Payload(InPayload),
			NextParticle(NULL),
			NextPayload(NULL)
	{
	}

	bool CanRender()
	{
		return Particle != NULL;
	}

	bool CanInterpolate()
	{
		return Particle && PrevParticle;
	}

	/**
		Initializes the state for traversing the trail and generating vertex data.
	*/
	inline void Init( )
	{
		check(Particle);
		GetNext();
	}
	
	/**
		Inits the next particle from the current Particle. 
	*/
	inline void GetNext()
	{
		check(Particle);
		int32 ParticleIdx = TRAIL_EMITTER_GET_NEXT(Payload->Flags);
		if (ParticleIdx != TRAIL_EMITTER_NULL_NEXT)
		{
			DECLARE_PARTICLE_PTR(TempParticle, ParticleDataAddress + Source.ParticleStride * ParticleIdx);
			NextParticle = TempParticle;
			NextPayload = (FAnimTrailTypeDataPayload*)((uint8*)TempParticle + Source.TrailDataOffset);
		}
		else
		{
			NextParticle = NULL;
			NextPayload = NULL;
		}
	}

	/**
		Move the pointers along the trail.
	*/
	inline void Advance( )
	{		
		PrevPrevParticle = PrevParticle;
		PrevPrevPayload = PrevPayload;
		PrevParticle = Particle;
		PrevPayload = Payload;
		Particle = NextParticle;
		Payload = NextPayload;

		if( Particle )
		{
			//Get the new next or fake it if we've reached the end of the trail.
			GetNext();
		}
	}
	
	/** 
		Generate interpolated vertex locations for the current location in the trail.
		Interpolates between PrevParticle and Particle.
	*/
	void CalcVertexData(float InterpFactor, FVector& OutLocation, FVector& OutFirst, FVector& OutSecond, float& OutTileU, float& OutSize, FLinearColor& OutColor, FVector4f* OutDynamicParameters)
	{
		check(CanRender());
		if( InterpFactor == 0.0f )
		{
			FVector Offset = FVector(Payload->Direction * Payload->Length);
			OutLocation = Particle->Location;
			OutFirst = Particle->Location - Offset;
			OutSecond = Particle->Location + Offset;
			OutTileU = Payload->TiledU;
			OutSize = Particle->Size.X * Source.Scale.X;
			OutColor = Particle->Color;
			if( OutDynamicParameters )
			{
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *Particle, *OutDynamicParameters);
			}
			return;
		}
		else if( PrevParticle && InterpFactor == 1.0f )
		{
			FVector Offset = FVector(PrevPayload->Direction * PrevPayload->Length);
			OutLocation = PrevParticle->Location;
			OutFirst = PrevParticle->Location - Offset;
			OutSecond = PrevParticle->Location + Offset;
			OutTileU = PrevPayload->TiledU;
			OutSize = PrevParticle->Size.X * Source.Scale.X;
			OutColor = PrevParticle->Color;
			if( OutDynamicParameters )
			{
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *PrevParticle, *OutDynamicParameters);
			}
			return;
		}
		else
		{
			check(CanInterpolate());
			//We are interpolating between the previous and the current.
			check( InterpFactor >= 0.0f && InterpFactor <= 1.0f );
			check( Particle && PrevParticle && Payload && PrevPayload );
				
			FVector PrevPrevLocation;
			FVector3f PrevPrevDirection;		
			float PrevPrevLength;
			float PrevPrevTiledU;
			float PrevPrevSize;
			FLinearColor PrevPrevColor;
			const FBaseParticle* PrevPrevDynParamParticle;
			if( PrevPrevParticle )
			{
				PrevPrevLocation = PrevPrevParticle->Location;
				PrevPrevDirection = PrevPrevPayload->Direction;
				PrevPrevLength = PrevPrevPayload->Length;
				PrevPrevTiledU = PrevPrevPayload->TiledU;
				PrevPrevSize = PrevPrevParticle->Size.X * Source.Scale.X;
				PrevPrevColor = PrevPrevParticle->Color;
				PrevPrevDynParamParticle = PrevPrevParticle;
			}
			else
			{
				PrevPrevLocation = PrevParticle->Location;
				PrevPrevDirection = PrevPayload->Direction;
				PrevPrevLength = PrevPayload->Length;
				PrevPrevTiledU = PrevPayload->TiledU;
				PrevPrevSize = PrevParticle->Size.X * Source.Scale.X;
				PrevPrevColor = PrevParticle->Color;
				PrevPrevDynParamParticle = PrevParticle;
			}

			FVector NextLocation;
			FVector3f NextDirection;
			float NextLength;
			float NextTiledU;
			float NextSize;
			FLinearColor NextColor;
			const FBaseParticle* NextDynParamParticle;
			if( NextParticle )
			{
				NextLocation = NextParticle->Location;
				NextDirection = NextPayload->Direction;
				NextLength = NextPayload->Length;
				NextTiledU = NextPayload->TiledU;
				NextSize = NextParticle->Size.X * Source.Scale.X;
				NextColor = NextParticle->Color;
				NextDynParamParticle = NextParticle;
			}
			else
			{
				NextLocation = Particle->Location;
				NextDirection = Payload->Direction;
				NextLength = Payload->Length;
				NextTiledU = Payload->TiledU;
				NextSize = Particle->Size.X * Source.Scale.X;
				NextColor = Particle->Color;
				NextDynParamParticle = Particle;
			}

			float NextT = 0.0f;
			float CurrT = NextParticle ? Payload->InterpolationParameter : GCatmullRomEndParamOffset;
			float PrevT = CurrT + PrevPayload->InterpolationParameter;
			float PrevPrevT = PrevT + (PrevPrevPayload ? PrevPrevPayload->InterpolationParameter : GCatmullRomEndParamOffset);
			
			float T = CurrT + ((PrevT - CurrT) * InterpFactor);
				
			//Interpolate locations
			FVector Location = FMath::CubicCRSplineInterpSafe(PrevPrevLocation, PrevParticle->Location, Particle->Location, NextLocation, PrevPrevT, PrevT, CurrT, NextT, T);
			FVector InterpDir = (FVector)FMath::CubicCRSplineInterpSafe(PrevPrevDirection, PrevPayload->Direction, Payload->Direction, NextDirection, PrevPrevT, PrevT, CurrT, NextT, T);
			InterpDir.Normalize();
			float InterpLength = FMath::CubicCRSplineInterpSafe(PrevPrevLength, PrevPayload->Length, Payload->Length, NextLength, PrevPrevT, PrevT, CurrT, NextT, T);
			OutTileU = FMath::CubicCRSplineInterpSafe(PrevPrevTiledU, PrevPayload->TiledU, Payload->TiledU, NextTiledU, PrevPrevT, PrevT, CurrT, NextT, T);
			OutSize = FMath::CubicCRSplineInterpSafe(PrevPrevSize, PrevParticle->Size.X * Source.Scale.X, Particle->Size.X * Source.Scale.X, NextSize, PrevPrevT, PrevT, CurrT, NextT, T);
			OutColor = FMath::CubicCRSplineInterpSafe(PrevPrevColor, PrevParticle->Color, Particle->Color, NextColor, PrevPrevT, PrevT, CurrT, NextT, T);

			if( OutDynamicParameters )
			{
				FVector4f PrevPrevDynamicParam;
				FVector4f PrevDynamicParam;
				FVector4f CurrDynamicParam;
				FVector4f NextDynamicParam;
				
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *PrevPrevDynParamParticle, PrevPrevDynamicParam);
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *PrevParticle, PrevDynamicParam);
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *Particle, CurrDynamicParam);
				GetDynamicValueFromPayload(Source.DynamicParameterDataOffset, *NextDynParamParticle, NextDynamicParam);

				*OutDynamicParameters = FMath::CubicCRSplineInterpSafe(PrevPrevDynamicParam,
					PrevDynamicParam,
					CurrDynamicParam,
					NextDynamicParam, PrevPrevT, PrevT, CurrT, NextT, T);
			}

			FVector Offset = (InterpDir * InterpLength);
			OutFirst = Location - Offset;
			OutSecond = Location + Offset;
			OutLocation = Location;
		}
	}
};

void FDynamicAnimTrailEmitterData::RenderDebug(const FParticleSystemSceneProxy* Proxy, FPrimitiveDrawInterface* PDI, const FSceneView* View, bool bCrosses) const
{
	if ((bRenderParticles == true) || (bRenderTangents == true))
	{
		// DEBUGGING
		// Draw all the points of the trail(s)
		FVector DrawPosition;
		FVector DrawFirstEdgePosition;
		FVector DrawSecondEdgePosition;
		float DrawSize;
		FColor DrawColor;
		FColor PrevDrawColor;
		FVector DrawTangentEnd;
		float TiledU;
		FLinearColor DummyColor;

		const uint8* Address = Source.DataContainer.ParticleData;
		const FAnimTrailTypeDataPayload* StartTrailPayload;

		for (int32 ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
		{
			DECLARE_PARTICLE_PTR(Particle, Address + Source.ParticleStride * Source.DataContainer.ParticleIndices[ParticleIdx]);
			StartTrailPayload = (FAnimTrailTypeDataPayload*)((uint8*)Particle + Source.TrailDataOffset);
			if (TRAIL_EMITTER_IS_HEAD(StartTrailPayload->Flags) == 0)
			{
				continue;
			}

			// Pin the size to the X component
			float Increment = 1.0f / (StartTrailPayload->TriangleCount / 2.0f);
			float ColorScale = 0.0f;

			FAnimTrailParticleRenderData RenderData(Source, Particle, StartTrailPayload);
			RenderData.Init();

			while (RenderData.CanRender())
			{
				RenderData.CalcVertexData(0.0f, DrawPosition, DrawFirstEdgePosition, DrawSecondEdgePosition,TiledU, DrawSize, DummyColor, NULL);

				int32 Red   = FMath::TruncToInt(255.0f * (1.0f - ColorScale));
				int32 Green = FMath::TruncToInt(255.0f * ColorScale);
				ColorScale += Increment;
				DrawColor = FColor(Red,Green,0);
				Red   = FMath::TruncToInt(255.0f * (1.0f - ColorScale));
				Green = FMath::TruncToInt(255.0f * ColorScale);
				PrevDrawColor = FColor(Red,Green,0);

				if (bRenderParticles == true)
				{
					if (TRAIL_EMITTER_IS_START(RenderData.Payload->Flags))
					{
						DrawWireStar(PDI, DrawPosition, DrawSize, FColor::Green, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawFirstEdgePosition, DrawSize, FColor::Green, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawSecondEdgePosition, DrawSize, FColor::Green, Proxy->GetDepthPriorityGroup(View));
					}
					else if (TRAIL_EMITTER_IS_DEADTRAIL(RenderData.Payload->Flags))
					{
						DrawWireStar(PDI, DrawPosition, DrawSize, FColor::Red, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawFirstEdgePosition, DrawSize, FColor::Red, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawSecondEdgePosition, DrawSize, FColor::Red, Proxy->GetDepthPriorityGroup(View));
					}
					else if (TRAIL_EMITTER_IS_END(RenderData.Payload->Flags))
					{
						DrawWireStar(PDI, DrawPosition, DrawSize, FColor::White, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawFirstEdgePosition, DrawSize, FColor::White, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawSecondEdgePosition, DrawSize, FColor::White, Proxy->GetDepthPriorityGroup(View));
					}
					else
					{
						DrawWireStar(PDI, DrawPosition, DrawSize*0.5f, DrawColor, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawFirstEdgePosition, DrawSize*0.5f, DrawColor, Proxy->GetDepthPriorityGroup(View));
						DrawWireStar(PDI, DrawSecondEdgePosition, DrawSize*0.5f, DrawColor, Proxy->GetDepthPriorityGroup(View));
					}


					if ( bRenderTessellation && RenderData.CanInterpolate() )
					{
						FVector PrevDrawPosition;
						FVector PrevDrawFirstEdgePosition;
						FVector PrevDrawSecondEdgePosition;
						float PrevTiledU;
						//Get the previous particles verts.
						RenderData.CalcVertexData(1.0f, PrevDrawPosition, PrevDrawFirstEdgePosition, PrevDrawSecondEdgePosition,PrevTiledU, DrawSize, DummyColor, NULL);

						// Draw a straight line between the particles
						// This will allow us to visualize the tessellation difference
						PDI->DrawLine(DrawPosition, PrevDrawPosition, FColor::Blue, Proxy->GetDepthPriorityGroup(View));
						PDI->DrawLine(DrawFirstEdgePosition, PrevDrawFirstEdgePosition, FColor::Blue, Proxy->GetDepthPriorityGroup(View));
						PDI->DrawLine(DrawSecondEdgePosition, PrevDrawSecondEdgePosition, FColor::Blue, Proxy->GetDepthPriorityGroup(View));

						int32 InterpCount = RenderData.Payload->RenderingInterpCount;
						// Interpolate between prev and current...
						FVector LineStart = DrawPosition;
						FVector FirstStart = DrawFirstEdgePosition;
						FVector SecondStart = DrawSecondEdgePosition;
						float InvCount = 1.0f / InterpCount;
						FLinearColor StartColor = DrawColor;
						FLinearColor EndColor = PrevDrawColor;
						for (int32 SpawnIdx = 0; SpawnIdx < InterpCount; SpawnIdx++)
						{
							float TimeStep = InvCount * SpawnIdx;
							FVector LineEnd;
							FVector FirstEnd;
							FVector SecondEnd;
							float TiledUEnd;
							FLinearColor InterpColor;

							RenderData.CalcVertexData(TimeStep, LineEnd, FirstEnd, SecondEnd,TiledUEnd,DrawSize,InterpColor,NULL);

							PDI->DrawLine(LineStart, LineEnd, InterpColor, Proxy->GetDepthPriorityGroup(View));
							PDI->DrawLine(FirstStart, FirstEnd, InterpColor, Proxy->GetDepthPriorityGroup(View));
							PDI->DrawLine(SecondStart, SecondEnd, InterpColor, Proxy->GetDepthPriorityGroup(View));
							if (SpawnIdx > 0)
							{
								InterpColor.R = 1.0f - TimeStep;
								InterpColor.G = 1.0f - TimeStep;
								InterpColor.B = 1.0f - (1.0f - TimeStep);
							}
							DrawWireStar(PDI, LineEnd, DrawSize * 0.3f, InterpColor, Proxy->GetDepthPriorityGroup(View));
							DrawWireStar(PDI, FirstEnd, DrawSize * 0.3f, InterpColor, Proxy->GetDepthPriorityGroup(View));
							DrawWireStar(PDI, SecondEnd, DrawSize * 0.3f, InterpColor, Proxy->GetDepthPriorityGroup(View));
							LineStart = LineEnd;
							FirstStart = FirstEnd;
							SecondStart = SecondEnd;
						}
						PDI->DrawLine(LineStart, PrevDrawPosition, EndColor, Proxy->GetDepthPriorityGroup(View));
						PDI->DrawLine(FirstStart, PrevDrawFirstEdgePosition, EndColor, Proxy->GetDepthPriorityGroup(View));
						PDI->DrawLine(SecondStart, PrevDrawSecondEdgePosition, EndColor, Proxy->GetDepthPriorityGroup(View));
					}
				}

				if (bRenderTangents == true)
				{
					DrawTangentEnd = DrawPosition + (FVector)RenderData.Payload->Tangent * (FVector)DrawSize * 3.0f;
					PDI->DrawLine(DrawPosition, DrawTangentEnd, FLinearColor(1.0f, 1.0f, 0.0f), Proxy->GetDepthPriorityGroup(View));
				}

				RenderData.Advance();
			}
		}
	}
}

// Data fill functions
int32 FDynamicAnimTrailEmitterData::FillVertexData(struct FAsyncBufferFillData& Data) const
{
	SCOPE_CYCLE_COUNTER(STAT_TrailFillVertexTime);

	int32	TrianglesToRender = 0;

	uint8* TempVertexData = (uint8*)Data.VertexData;
	FParticleBeamTrailVertex* Vertex;

	uint8* TempDynamicParamData = (uint8*)Data.DynamicParameterData;
	FParticleBeamTrailVertexDynamicParameter* DynParamVertex;

	int32 Sheets = 1;

	bool bUseDynamic = bUsesDynamicParameter && TempDynamicParamData != nullptr;

	// The increment for going [0..1] along the complete trail
	float TextureIncrement = 1.0f / (Data.VertexCount / 2.0f);
	// The distance tracking for tiling the 2nd UV set
	float CurrDistance = 0.0f;

	const uint8* ParticleData = Source.DataContainer.ParticleData;
	const FVector LWCTileOffset = FVector(Source.LWCTile) * FLargeWorldRenderScalar::GetTileSize();
	for (int32 ParticleIdx = 0; ParticleIdx < Source.ActiveParticleCount; ParticleIdx++)
	{
		DECLARE_PARTICLE_PTR(Particle, ParticleData + Source.ParticleStride * Source.DataContainer.ParticleIndices[ParticleIdx]);
		FAnimTrailTypeDataPayload* TrailPayload = (FAnimTrailTypeDataPayload*)((uint8*)Particle + Source.TrailDataOffset);
		if (TRAIL_EMITTER_IS_HEAD(TrailPayload->Flags) == 0)
		{
			continue;
		}

		if (TRAIL_EMITTER_GET_NEXT(TrailPayload->Flags) == TRAIL_EMITTER_NULL_NEXT)
		{
			continue;
		}

		FAnimTrailParticleRenderData RenderData(Source, Particle, TrailPayload);
		RenderData.Init();

		// Pin the size to the X component=
		float Tex_U = 0.0f;
		int32 VertexStride = sizeof(FParticleBeamTrailVertex);
		int32 DynamicParamStride = 0;
		bool bFillDynamic = false;
		if (bUseDynamic)
		{
			DynamicParamStride = sizeof(FParticleBeamTrailVertexDynamicParameter);
			if (Source.DynamicParameterDataOffset > 0)
			{
				bFillDynamic = true;
			}
		}
		float CurrTileU;

		FVector Location;
		FVector FirstSocket;
		FVector SecondSocket;
		float TiledU;
		float InterpSize;
		FLinearColor InterpColor;

		while (RenderData.CanRender())
		{
			int32 InterpCount = RenderData.Payload->RenderingInterpCount;
			if (InterpCount > 1 && RenderData.CanInterpolate())
			{
				// Interpolate between current and next...
				float InvCount = 1.0f / InterpCount;

				FVector4f InterpDynamic(1.0f, 1.0f, 1.0f, 1.0f);
				for (int32 SpawnIdx = InterpCount - 1; SpawnIdx >= 0; SpawnIdx--)
				{
					float TimeStep = InvCount * SpawnIdx;

					RenderData.CalcVertexData(TimeStep, Location, FirstSocket, SecondSocket, TiledU, InterpSize, InterpColor, bFillDynamic ? &InterpDynamic : NULL);

					if (bTextureTileDistance == true)	
					{
						CurrTileU = TiledU;
					}
					else
					{
						CurrTileU = Tex_U;
					}

					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = FVector3f(FirstSocket - LWCTileOffset);
					Vertex->OldPosition = FVector3f(FirstSocket - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Tex_U = Tex_U;
					Vertex->Tex_V = 0.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 0.0f;
					Vertex->Rotation = RenderData.Particle->Rotation;
					Vertex->Color = InterpColor;
					if (bUseDynamic)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
						TempDynamicParamData += DynamicParamStride;
					}
					TempVertexData += VertexStride;

					Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
					Vertex->Position = FVector3f(SecondSocket - LWCTileOffset);
					Vertex->OldPosition = FVector3f(SecondSocket - LWCTileOffset);
					Vertex->ParticleId	= 0;
					Vertex->Size.X = InterpSize;
					Vertex->Size.Y = InterpSize;
					Vertex->Tex_U = Tex_U;
					Vertex->Tex_V = 1.0f;
					Vertex->Tex_U2 = CurrTileU;
					Vertex->Tex_V2 = 1.0f;
					Vertex->Rotation = RenderData.Particle->Rotation;
					Vertex->Color = InterpColor;
					if (bUseDynamic)
					{
						DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
						DynParamVertex->DynamicValue[0] = InterpDynamic.X;
						DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
						DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
						DynParamVertex->DynamicValue[3] = InterpDynamic.W;
						TempDynamicParamData += DynamicParamStride;
					}
					TempVertexData += VertexStride;

					Tex_U += TextureIncrement;
				}
			}
			else
			{
				FVector4f InterpDynamic(1.0f, 1.0f, 1.0f, 1.0f);
				RenderData.CalcVertexData( 0.0f, Location, FirstSocket, SecondSocket, TiledU, InterpSize, InterpColor, bFillDynamic ? &InterpDynamic : NULL );

				if (bTextureTileDistance == true)
				{
					CurrTileU = TiledU;
				}
				else
				{
					CurrTileU = Tex_U;
				}

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = FVector3f(FirstSocket - LWCTileOffset);//PackingParticle->Location + TrailPayload->FirstEdge * CurrSize;
				Vertex->OldPosition = FVector3f(RenderData.Particle->OldLocation - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size.X = InterpSize;
				Vertex->Size.Y = InterpSize;
				Vertex->Tex_U = Tex_U;
				Vertex->Tex_V = 0.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 0.0f;
				Vertex->Rotation = RenderData.Particle->Rotation;
				Vertex->Color = InterpColor;
				if (bUseDynamic)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
					DynParamVertex->DynamicValue[0] = InterpDynamic.X;
					DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
					DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
					DynParamVertex->DynamicValue[3] = InterpDynamic.W;
					TempDynamicParamData += DynamicParamStride;
				}
				TempVertexData += VertexStride;
				//PackedVertexCount++;

				Vertex = (FParticleBeamTrailVertex*)(TempVertexData);
				Vertex->Position = FVector3f(SecondSocket - LWCTileOffset);//PackingParticle->Location - TrailPayload->SecondEdge * CurrSize;
				Vertex->OldPosition = FVector3f(RenderData.Particle->OldLocation - LWCTileOffset);
				Vertex->ParticleId	= 0;
				Vertex->Size.X = InterpSize;
				Vertex->Size.Y = InterpSize;
				Vertex->Tex_U = Tex_U;
				Vertex->Tex_V = 1.0f;
				Vertex->Tex_U2 = CurrTileU;
				Vertex->Tex_V2 = 1.0f;
				Vertex->Rotation = RenderData.Particle->Rotation;
				Vertex->Color = InterpColor;
				if (bUseDynamic)
				{
					DynParamVertex = (FParticleBeamTrailVertexDynamicParameter*)(TempDynamicParamData);
					DynParamVertex->DynamicValue[0] = InterpDynamic.X;
					DynParamVertex->DynamicValue[1] = InterpDynamic.Y;
					DynParamVertex->DynamicValue[2] = InterpDynamic.Z;
					DynParamVertex->DynamicValue[3] = InterpDynamic.W;
					TempDynamicParamData += DynamicParamStride;
				}
				TempVertexData += VertexStride;

				Tex_U += TextureIncrement;
			}

			RenderData.Advance();
		}
	}

	return TrianglesToRender;
}

///////////////////////////////////////////////////////////////////////////////
//	ParticleSystemSceneProxy
///////////////////////////////////////////////////////////////////////////////
/** Initialization constructor. */
FParticleSystemSceneProxy::FParticleSystemSceneProxy(UParticleSystemComponent* Component, FParticleDynamicData* InDynamicData, bool InbCanBeOccluded)
	: FPrimitiveSceneProxy(Component, Component->Template ? Component->Template->GetFName() : NAME_None)
	, Owner(Component->GetOwner())
	, bCastShadow(Component->CastShadow)
	, bManagingSignificance(Component->ShouldManageSignificance())
	, bCanBeOccluded(InbCanBeOccluded)
	, bHasCustomOcclusionBounds(false)
	, FeatureLevel(GetScene().GetFeatureLevel())
	, MaterialRelevance(
		((Component->GetCurrentLODIndex() >= 0) && (Component->GetCurrentLODIndex() < Component->CachedViewRelevanceFlags.Num())) ?
			Component->CachedViewRelevanceFlags[Component->GetCurrentLODIndex()] :
		((Component->GetCurrentLODIndex() == -1) && (Component->CachedViewRelevanceFlags.Num() >= 1)) ?
			Component->CachedViewRelevanceFlags[0] :
			FMaterialRelevance()
		)
	, DynamicData(InDynamicData)
	, LastDynamicData(NULL)
	, DeselectedWireframeMaterialInstance(new FColoredMaterialRenderProxy(
		GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
		GetSelectionColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),false,false)
		))
	, PendingLODDistance(0.0f)
	, VisualizeLODIndex(Component->GetCurrentLODIndex())
	, LastFramePreRendered(-1)
	, FirstFreeMeshBatch(0)
{
	SetWireframeColor(FLinearColor(3.0f, 0.0f, 0.0f));
	SetLevelColor(FLinearColor(1.0f, 1.0f, 0.0f));
	SetPropertyColor(FLinearColor(1.0f, 1.0f, 1.0f));

	LODMethod = Component->LODMethod;

	// Particle systems intrinsically always have motion, but is this motion relevant to systems external to particle systems?
	bAlwaysHasVelocity = Component->Template && Component->Template->DoesAnyEmitterHaveMotionBlur(Component->GetCurrentLODIndex());

	if (bCanBeOccluded && Component->Template && (Component->Template->OcclusionBoundsMethod == EPSOBM_CustomBounds))
	{
		OcclusionBounds = FBoxSphereBounds(Component->Template->CustomOcclusionBounds);
		bHasCustomOcclusionBounds = true;
	}
}

SIZE_T FParticleSystemSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

FParticleSystemSceneProxy::~FParticleSystemSceneProxy()
{
	ReleaseRenderThreadResources();

	delete DynamicData;
	DynamicData = NULL;

	delete DeselectedWireframeMaterialInstance;
	DeselectedWireframeMaterialInstance = nullptr;
}

FMeshBatch* FParticleSystemSceneProxy::GetPooledMeshBatch()
{
	FMeshBatch* Batch = NULL;
	if (FirstFreeMeshBatch < MeshBatchPool.Num())
	{
		Batch = &MeshBatchPool[FirstFreeMeshBatch];
	}
	else
	{
		Batch = new FMeshBatch();
		MeshBatchPool.Add(Batch);
	}
	FirstFreeMeshBatch++;
	return Batch;
}

// FPrimitiveSceneProxy interface.

void FParticleSystemSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	FInGameScopedCycleCounter InGameCycleCounter(GetScene().GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::RenderThread, bManagingSignificance);

	SCOPE_CYCLE_COUNTER(STAT_FParticleSystemSceneProxy_GetMeshElements);
	SCOPE_CYCLE_COUNTER(STAT_ParticlesOverview_RT);
	PARTICLE_PERF_STAT_CYCLES_RT(PerfStatContext, GetDynamicMeshElements);

	if ((GIsEditor == true) || (GbEnableGameThreadLODCalculation == false))
	{
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				//@todo parallelrendering - get rid of this legacy feedback to the game thread!  
				const_cast<FParticleSystemSceneProxy*>(this)->DetermineLODDistance(View, ViewFamily.FrameNumber);
			}
		}
	}

	if (ViewFamily.EngineShowFlags.Particles)
	{
		SCOPE_CYCLE_COUNTER(STAT_ParticleRenderingTime);
		FScopeCycleCounter Context(GetStatId());

		const double StartTime = GTrackParticleRenderingStats ? FPlatformTime::Seconds() : 0;
		int32 NumDraws = 0;

		if (DynamicData != NULL)
		{
			for (int32 Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
			{
				FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray[Index];
				if ((Data == NULL) || (Data->bValid != true))
				{
					continue;
				}
				FScopeCycleCounter AdditionalScope(Data->StatID);

				//hold on to the emitter index in case we need to access any of its properties
				DynamicData->EmitterIndex = Index;

				for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
				{
					if (VisibilityMap & (1 << ViewIndex))
					{
						const FSceneView* View = Views[ViewIndex];
						Data->GetDynamicMeshElementsEmitter(this, View, ViewFamily, ViewIndex, Collector);
						NumDraws++;
					}
				}
			}
		}

		INC_DWORD_STAT_BY(STAT_ParticleDrawCalls, NumDraws);

		if (ViewFamily.EngineShowFlags.Particles)
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
					if (HasCustomOcclusionBounds())
					{
						RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetCustomOcclusionBounds(), IsSelected());
					}
				}
			}
		}
	}
}

void FParticleSystemSceneProxy::CreateRenderThreadResources()
{
	CreateRenderThreadResourcesForEmitterData();
}

void FParticleSystemSceneProxy::ReleaseRenderThreadResources()
{
	ReleaseRenderThreadResourcesForEmitterData();
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void FParticleSystemSceneProxy::CreateRenderThreadResourcesForEmitterData()
{
	if (DynamicData != nullptr)
	{
		for (int32 Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
		{
			FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray[Index];
			if (Data != NULL)
			{
				FScopeCycleCounter AdditionalScope(Data->StatID);
				Data->UpdateRenderThreadResourcesEmitter(this);
			}
		}
	}
}

void FParticleSystemSceneProxy::ReleaseRenderThreadResourcesForEmitterData()
{
	if (DynamicData)
	{
		for (int32 Index = 0; Index < DynamicData->DynamicEmitterDataArray.Num(); Index++)
		{
			FDynamicEmitterDataBase* Data =	DynamicData->DynamicEmitterDataArray[Index];
			if (Data != NULL)
			{
				FScopeCycleCounter AdditionalScope(Data->StatID);
				Data->ReleaseRenderThreadResources(this);
			}
		}
	}
}

void FParticleSystemSceneProxy::UpdateData(FParticleDynamicData* NewDynamicData)
{
	FParticleSystemSceneProxy* Proxy = this;
	ENQUEUE_RENDER_COMMAND(ParticleUpdateDataCommand)(
		[Proxy, NewDynamicData](FRHICommandListImmediate& RHICmdList)
		{
		#if WITH_PARTICLE_PERF_STATS
			Proxy->PerfStatContext = NewDynamicData ? NewDynamicData->PerfStatContext : FParticlePerfStatsContext();
		#endif
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ParticleUpdate);
			SCOPE_CYCLE_COUNTER(STAT_ParticleUpdateRTTime);
			STAT(FScopeCycleCounter Context(Proxy->GetStatId());)
			PARTICLE_PERF_STAT_CYCLES_WITH_COUNT_RT(Proxy->PerfStatContext, RenderUpdate, 1);

			Proxy->UpdateData_RenderThread(NewDynamicData);
		}
	);
}

void FParticleSystemSceneProxy::UpdateData_RenderThread(FParticleDynamicData* NewDynamicData)
{
	FInGameScopedCycleCounter InGameCycleCounter(GetScene().GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::RenderThread, bManagingSignificance);
	SCOPE_CYCLE_COUNTER(STAT_ParticlesOverview_RT);

	ReleaseRenderThreadResourcesForEmitterData();
	if (DynamicData != NewDynamicData)
	{
		delete DynamicData;
	}
	DynamicData = NewDynamicData;
	CreateRenderThreadResourcesForEmitterData();
}

void FParticleSystemSceneProxy::DetermineLODDistance(const FSceneView* View, int32 FrameNumber)
{
	if (LODMethod == PARTICLESYSTEMLODMETHOD_Automatic)
	{
		// Default to the highest LOD level
		FVector	CameraPosition		= View->ViewMatrices.GetViewOrigin();
		FVector	ComponentPosition	= GetLocalToWorld().GetOrigin();
		FVector	DistDiff			= ComponentPosition - CameraPosition;
		float	Distance			= DistDiff.Size() * View->LODDistanceFactor;

		if (FrameNumber != LastFramePreRendered)
		{
			// First time in the frame - then just set it...
			PendingLODDistance = Distance;
			LastFramePreRendered = FrameNumber;
		}
		else if (Distance < PendingLODDistance)
		{
			// Not first time in the frame, then we compare and set if closer
			PendingLODDistance = Distance;
		}
	}
}

int32 GEnableMacroUVDebugSpam = 1;
static FAutoConsoleVariableRef EnableMacroUVDebugSpam(
	TEXT("r.EnableDebugSpam_GetObjectPositionAndScale"),
	GEnableMacroUVDebugSpam,
	TEXT("Enables or disables debug log spam for a bug in FParticleSystemSceneProxy::GetObjectPositionAndScale()")
	);

/** Object position in post projection space. */
void FParticleSystemSceneProxy::GetObjectPositionAndScale(const FSceneView& View, FVector2D& ObjectNDCPosition, FVector2D& ObjectMacroUVScales) const
{
	const FVector4 ObjectPostProjectionPositionWithW = View.ViewMatrices.GetViewProjectionMatrix().TransformPosition(DynamicData->SystemPositionForMacroUVs);
	ObjectNDCPosition = FVector2D(ObjectPostProjectionPositionWithW / FMath::Max(ObjectPostProjectionPositionWithW.W, 0.00001f));
	
	float MacroUVRadius = DynamicData->SystemRadiusForMacroUVs;
	FVector MacroUVPosition = DynamicData->SystemPositionForMacroUVs;
   
	uint32 Index = DynamicData->EmitterIndex;
	const FMacroUVOverride& MacroUVOverride = DynamicData->DynamicEmitterDataArray[Index]->GetMacroUVOverride();
	if (MacroUVOverride.bOverride)
	{
		MacroUVRadius = MacroUVOverride.Radius;
		MacroUVPosition = GetLocalToWorld().TransformVector((FVector)MacroUVOverride.Position);

#if !(UE_BUILD_SHIPPING)
		if (MacroUVPosition.ContainsNaN())
		{
			UE_LOG(LogParticles, Error, TEXT("MacroUVPosition.ContainsNaN()"));
		}
#endif
	}

	ObjectMacroUVScales = FVector2D(0,0);
	if (MacroUVRadius > 0.0f)
	{
		// Need to determine the scales required to transform positions into UV's for the ParticleMacroUVs material node
		// Determine screenspace extents by transforming the object position + appropriate camera vector * radius
		const FVector4 RightPostProjectionPosition = View.ViewMatrices.GetViewProjectionMatrix().TransformPosition(MacroUVPosition + MacroUVRadius * View.ViewMatrices.GetTranslatedViewMatrix().GetColumn(0));
		const FVector4 UpPostProjectionPosition = View.ViewMatrices.GetViewProjectionMatrix().TransformPosition(MacroUVPosition + MacroUVRadius * View.ViewMatrices.GetTranslatedViewMatrix().GetColumn(1));
		//checkSlow(RightPostProjectionPosition.X - ObjectPostProjectionPositionWithW.X >= 0.0f && UpPostProjectionPosition.Y - ObjectPostProjectionPositionWithW.Y >= 0.0f);

		// Scales to transform the view space positions corresponding to SystemPositionForMacroUVs +- SystemRadiusForMacroUVs into [0, 1] in xy
		// Scales to transform the screen space positions corresponding to SystemPositionForMacroUVs +- SystemRadiusForMacroUVs into [0, 1] in zw

		const FVector4::FReal RightNDCPosX = RightPostProjectionPosition.X / RightPostProjectionPosition.W;
		const FVector4::FReal UpNDCPosY = UpPostProjectionPosition.Y / UpPostProjectionPosition.W;
		FVector4::FReal DX = FMath::Min(RightNDCPosX - ObjectNDCPosition.X, WORLD_MAX);
		FVector4::FReal DY = FMath::Min(UpNDCPosY - ObjectNDCPosition.Y, WORLD_MAX);
		if (DX != 0 && DY != 0 && !FMath::IsNaN(DX) && FMath::IsFinite(DX) && !FMath::IsNaN(DY) && FMath::IsFinite(DY))
		{
			ObjectMacroUVScales = FVector2D(1.0f / DX, -1.0f / DY);
		}
		else
		{
			//Spam the logs to track down infrequent / hard to repro bug.
			if (GEnableMacroUVDebugSpam != 0)
			{
				UE_LOG(LogParticles, Error, TEXT("Bad values in FParticleSystemSceneProxy::GetObjectPositionAndScale"));
				UE_LOG(LogParticles, Error, TEXT("SystemPositionForMacroUVs: {%.6f, %.6f, %.6f}"), DynamicData->SystemPositionForMacroUVs.X, DynamicData->SystemPositionForMacroUVs.Y, DynamicData->SystemPositionForMacroUVs.Z);
				UE_LOG(LogParticles, Error, TEXT("ObjectPostProjectionPositionWithW: {%.6f, %.6f, %.6f, %.6f}"), ObjectPostProjectionPositionWithW.X, ObjectPostProjectionPositionWithW.Y, ObjectPostProjectionPositionWithW.Z, ObjectPostProjectionPositionWithW.W);
				UE_LOG(LogParticles, Error, TEXT("RightPostProjectionPosition: {%.6f, %.6f, %.6f, %.6f}"), RightPostProjectionPosition.X, RightPostProjectionPosition.Y, RightPostProjectionPosition.Z, RightPostProjectionPosition.W);
				UE_LOG(LogParticles, Error, TEXT("UpPostProjectionPosition: {%.6f, %.6f, %.6f, %.6f}"), UpPostProjectionPosition.X, UpPostProjectionPosition.Y, UpPostProjectionPosition.Z, UpPostProjectionPosition.W);
				UE_LOG(LogParticles, Error, TEXT("ObjectNDCPosition: {%.6f, %.6f}"), ObjectNDCPosition.X, ObjectNDCPosition.Y);
				UE_LOG(LogParticles, Error, TEXT("RightNDCPosX: %.6f"), RightNDCPosX);
				UE_LOG(LogParticles, Error, TEXT("UpNDCPosY: %.6f"), UpNDCPosY);
				UE_LOG(LogParticles, Error, TEXT("MacroUVPosition: {%.6f, %.6f, %.6f}"), MacroUVPosition.X, MacroUVPosition.Y, MacroUVPosition.Z);
				UE_LOG(LogParticles, Error, TEXT("MacroUVRadius: %.6f"), MacroUVRadius);
				UE_LOG(LogParticles, Error, TEXT("DX: %.6f"), DX);
				UE_LOG(LogParticles, Error, TEXT("DY: %.6f"), DY);
				FVector4 View0 = View.ViewMatrices.GetViewMatrix().GetColumn(0);
				FVector4 View1 = View.ViewMatrices.GetViewMatrix().GetColumn(1);
				FVector4 View2 = View.ViewMatrices.GetViewMatrix().GetColumn(2);
				FVector4 View3 = View.ViewMatrices.GetViewMatrix().GetColumn(3);
				UE_LOG(LogParticles, Error, TEXT("View0: {%.6f, %.6f, %.6f, %.6f}"), View0.X, View0.Y, View0.Z, View0.W);
				UE_LOG(LogParticles, Error, TEXT("View1: {%.6f, %.6f, %.6f, %.6f}"), View1.X, View1.Y, View1.Z, View1.W);
				UE_LOG(LogParticles, Error, TEXT("View2: {%.6f, %.6f, %.6f, %.6f}"), View2.X, View2.Y, View2.Z, View2.W);
				UE_LOG(LogParticles, Error, TEXT("View3: {%.6f, %.6f, %.6f, %.6f}"), View3.X, View3.Y, View3.Z, View3.W);
				FVector4 ViewProj0 = View.ViewMatrices.GetViewProjectionMatrix().GetColumn(0);
				FVector4 ViewProj1 = View.ViewMatrices.GetViewProjectionMatrix().GetColumn(1);
				FVector4 ViewProj2 = View.ViewMatrices.GetViewProjectionMatrix().GetColumn(2);
				FVector4 ViewProj3 = View.ViewMatrices.GetViewProjectionMatrix().GetColumn(3);
				UE_LOG(LogParticles, Error, TEXT("ViewProj0: {%.6f, %.6f, %.6f, %.6f}"), ViewProj0.X, ViewProj0.Y, ViewProj0.Z, ViewProj0.W);
				UE_LOG(LogParticles, Error, TEXT("ViewProj1: {%.6f, %.6f, %.6f, %.6f}"), ViewProj1.X, ViewProj1.Y, ViewProj1.Z, ViewProj1.W);
				UE_LOG(LogParticles, Error, TEXT("ViewProj2: {%.6f, %.6f, %.6f, %.6f}"), ViewProj2.X, ViewProj2.Y, ViewProj2.Z, ViewProj2.W);
				UE_LOG(LogParticles, Error, TEXT("ViewProj3: {%.6f, %.6f, %.6f, %.6f}"), ViewProj3.X, ViewProj3.Y, ViewProj3.Z, ViewProj3.W);
			}
		}
	}
}

/**
* @return Relevance for rendering the particle system primitive component in the given View
*/
FPrimitiveViewRelevance FParticleSystemSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && View->Family->EngineShowFlags.Particles;
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	Result.bDynamicRelevance = true;
	Result.bHasSimpleLights = true;
	if (!View->Family->EngineShowFlags.Wireframe && View->Family->EngineShowFlags.Materials)
	{
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
	}
	if (View->Family->EngineShowFlags.Bounds || View->Family->EngineShowFlags.VectorFields)
	{
		Result.bOpaque = true;
	}
	// see if any of the emitters use dynamic vertex data
	if (DynamicData == NULL)
	{
		// In order to get the LOD distances to update,
		// we need to force a call to DrawDynamicElements...
		Result.bOpaque = true;
	}

	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;

	return Result;
}

void FParticleSystemSceneProxy::OnTransformChanged()
{
	WorldSpacePrimitiveUniformBuffer.ReleaseResource();
}

void FParticleSystemSceneProxy::UpdateWorldSpacePrimitiveUniformBuffer() const
{
	check(IsInRenderingThread());
	if (!WorldSpacePrimitiveUniformBuffer.IsInitialized())
	{
		WorldSpacePrimitiveUniformBuffer.SetContents(
			FPrimitiveUniformShaderParametersBuilder{}
			.Defaults()
				.LocalToWorld(FMatrix::Identity)
				.ActorWorldPosition(GetActorPosition())
				.WorldBounds(GetBounds())
				.LocalBounds(GetLocalBounds())
				.ReceivesDecals(ReceivesDecals())
				.OutputVelocity(AlwaysHasVelocity())
				.LightingChannelMask(GetLightingChannelMask())
				.UseSingleSampleShadowFromStationaryLights(UseSingleSampleShadowFromStationaryLights())
				.UseVolumetricLightmap(GetScene().HasPrecomputedVolumetricLightmap_RenderThread())
			.Build()
		);
		WorldSpacePrimitiveUniformBuffer.InitResource();
	}
}

void FParticleSystemSceneProxy::GatherSimpleLights(const FSceneViewFamily& ViewFamily, FSimpleLightArray& OutParticleLights) const
{
	FInGameScopedCycleCounter InGameCycleCounter(GetScene().GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::RenderThread, bManagingSignificance);
	SCOPE_CYCLE_COUNTER(STAT_ParticlesOverview_RT);
	if (DynamicData != NULL)
	{
		FScopeCycleCounter Context(GetStatId());
		for (int32 EmitterIndex = 0; EmitterIndex < DynamicData->DynamicEmitterDataArray.Num(); EmitterIndex++)
		{
			const FDynamicEmitterDataBase* DynamicEmitterData = DynamicData->DynamicEmitterDataArray[EmitterIndex];
			if (DynamicEmitterData)
			{
				FScopeCycleCounter AdditionalScope(DynamicEmitterData->StatID);
				DynamicEmitterData->GatherSimpleLights(this, ViewFamily, OutParticleLights);
			}
		}
	}
}

FPrimitiveSceneProxy* UParticleSystemComponent::CreateSceneProxy()
{
	SCOPE_CYCLE_COUNTER(STAT_FParticleSystemSceneProxy_Create);
	SCOPE_CYCLE_COUNTER(STAT_ParticlesOverview_GT);

	FParticleSystemSceneProxy* NewProxy = NULL;

	//@fixme EmitterInstances.Num() check should be here to avoid proxies for dead emitters but there are some edge cases where it happens for emitters that have just activated...
	//@fixme Get non-instanced path working in ES!
	if ((IsActive() == true)/** && (EmitterInstances.Num() > 0)*/ && Template)
	{
		if (!bPSOPrecacheCalled)
		{
			PrecacheAssetPSOs(Template);
		}

		if (IsPSOPrecaching())
		{
			UE_LOG(LogParticles, Verbose, TEXT("Skipping CreateSceneProxy for UParticleSystemComponent %s (UParticleSystem PSOs are still compiling)"), *GetFullName());
			return nullptr;
		}

		FInGameScopedCycleCounter InGameCycleCounter(GetWorld(), EInGamePerfTrackers::VFXSignificance, EInGamePerfTrackerThreads::GameThread, bIsManagingSignificance);

		UE_LOG(LogParticles,Verbose,
			TEXT("CreateSceneProxy @ %fs %s bIsActive=%d"), GetWorld()->TimeSeconds,
			Template != NULL ? *Template->GetName() : TEXT("NULL"), IsActive());

		if (EmitterInstances.Num() > 0)
		{
			CacheViewRelevanceFlags(Template);
		}

		// Create the dynamic data for rendering this particle system.
		bParallelRenderThreadUpdate = true;
		FParticleDynamicData* ParticleDynamicData = CreateDynamicData(GetScene()->GetFeatureLevel());
		bParallelRenderThreadUpdate = false;

		if (CanBeOccluded())
		{
			Template->CustomOcclusionBounds.IsValid = true;
			NewProxy = ::new FParticleSystemSceneProxy(this,ParticleDynamicData, true);
		}
		else
		{
			NewProxy = ::new FParticleSystemSceneProxy(this,ParticleDynamicData, false);
		}
		check (NewProxy);
	}
	
	// 
	return NewProxy;
}

#if WITH_EDITOR
void DrawParticleSystemHelpers(UParticleSystemComponent* InPSysComp, const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	if (InPSysComp != NULL)
	{
		for (int32 EmitterIdx = 0; EmitterIdx < InPSysComp->EmitterInstances.Num(); EmitterIdx++)
		{
			FParticleEmitterInstance* EmitterInst = InPSysComp->EmitterInstances[EmitterIdx];
			if (EmitterInst && EmitterInst->SpriteTemplate)
			{
				UParticleLODLevel* LODLevel = EmitterInst->SpriteTemplate->GetCurrentLODLevel(EmitterInst);
				for (int32 ModuleIdx = 0; ModuleIdx < LODLevel->Modules.Num(); ModuleIdx++)
				{
					UParticleModule* Module = LODLevel->Modules[ModuleIdx];
					if (Module && Module->bSupported3DDrawMode && Module->b3DDrawMode)
					{
						Module->Render3DPreview(EmitterInst, View, PDI);
					}
				}
			}
		}
	}
}

ENGINE_API void DrawParticleSystemHelpers(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	for (TObjectIterator<AActor> It; It; ++It)
	{
		for (UActorComponent* Component : It->GetComponents())
		{
			if (UParticleSystemComponent* PSC = Cast<UParticleSystemComponent>(Component))
			{
				DrawParticleSystemHelpers(PSC, View, PDI);
			}
		}
	}
}
#endif	//#if WITH_EDITOR

