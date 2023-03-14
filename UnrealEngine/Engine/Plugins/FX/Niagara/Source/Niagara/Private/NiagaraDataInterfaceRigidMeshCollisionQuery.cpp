// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceRigidMeshCollisionQuery.h"
#include "Algo/ForEach.h"
#include "Animation/SkeletalMeshActor.h"
#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "DistanceFieldLightingShared.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "SkeletalRenderPublic.h"

#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceUtilities.h"
#include "NiagaraDistanceFieldHelper.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraGpuComputeDispatch.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraRenderer.h"
#include "NiagaraShader.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSimStageData.h"
#include "NiagaraSystemInstance.h"
#include "ShaderParameterUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceRigidMeshCollisionQuery)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceRigidMeshCollisionQuery"
DEFINE_LOG_CATEGORY_STATIC(LogRigidMeshCollision, Log, All);

// outstanding/known issues:
// -when actors change and the arrays are fully updated we'll experience a frame of 0 velocities
//		-potentially we could keep track of ranges of rigid bodies for given actors and then smartly reassign
//		the previous frame's transforms
// -could add a vM function for setting the maximum number of primitives


namespace NDIRigidMeshCollisionLocal
{

struct FNiagaraRigidMeshCollisionDIFunctionVersion
{
	enum Type
	{
		InitialVersion = 0,
		LargeWorldCoordinates = 1,
		SetMaxDistance = 2,
		FindActorRotation = 3,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
};

//------------------------------------------------------------------------------------------------------------

BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
	SHADER_PARAMETER(uint32,				MaxTransforms)
	SHADER_PARAMETER(uint32,				CurrentOffset)
	SHADER_PARAMETER(uint32,				PreviousOffset)
	SHADER_PARAMETER(FUintVector4,			ElementOffsets)
	SHADER_PARAMETER_SRV(Buffer<float4>,	WorldTransformBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>,	InverseTransformBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>,	ElementExtentBuffer)
	SHADER_PARAMETER_SRV(Buffer<uint32>,	PhysicsTypeBuffer)
	SHADER_PARAMETER_SRV(Buffer<uint32>,	DFIndexBuffer)
	SHADER_PARAMETER(FVector3f,				SystemLWCTile)
END_SHADER_PARAMETER_STRUCT()

static const TCHAR* TemplateShaderFile = TEXT("/Plugin/FX/Niagara/Private/NiagaraDataInterfaceRigidMeshCollisionQuery.ush");

//------------------------------------------------------------------------------------------------------------

static const FName FindActorsName(TEXT("FindActors"));

//------------------------------------------------------------------------------------------------------------

static const FName GetNumBoxesName(TEXT("GetNumBoxes"));
static const FName GetNumSpheresName(TEXT("GetNumSpheres"));
static const FName GetNumCapsulesName(TEXT("GetNumCapsules"));

//------------------------------------------------------------------------------------------------------------

static const FName GetClosestElementName(TEXT("GetClosestElement"));
static const FName GetElementPointName(TEXT("GetElementPoint"));
static const FName GetElementDistanceName(TEXT("GetElementDistance"));
static const FName GetClosestPointName(TEXT("GetClosestPoint"));
static const FName GetClosestDistanceName(TEXT("GetClosestDistance"));
static const FName GetClosestPointMeshDistanceFieldName(TEXT("GetClosestPointMeshDistanceField"));
static const FName GetClosestPointMeshDistanceFieldAccurateName(TEXT("GetClosestPointMeshDistanceFieldAccurate"));
static const FName GetClosestPointMeshDistanceFieldNoNormalName(TEXT("GetClosestPointMeshDistanceFieldNoNormal"));

static const FText OverlapOriginDescription = IF_WITH_EDITORONLY_DATA(
	LOCTEXT("RigidBodyOverlapOriginDescription", "The center point, in world space, where the overlap trace will be performed."),
	FText()
);

static const FText OverlapRotationDescription = IF_WITH_EDITORONLY_DATA(
	LOCTEXT("RigidBodyOVerlapRotationDesciption", "The orientation of the box to be used for hte overlap trace."),
	FText()
);

static const FText OverlapExtentDescription = IF_WITH_EDITORONLY_DATA(
	LOCTEXT("RigidBodyOverlapExtentDescription", "The extent, in world space, of the overlap trace."),
	FText()
);

static const FText TraceChannelDescription = IF_WITH_EDITORONLY_DATA(
	LOCTEXT("RigidBodyTraceChannelDescription", "The trace channel to collide against. Trace channels can be configured in the project settings."),
	FText()
);

static const FText SkipOverlapDescription = IF_WITH_EDITORONLY_DATA(
	LOCTEXT("RigidBodySkipTraceDescription", "If enabled, the overlap test will not be performed."),
	FText()
);

//------------------------------------------------------------------------------------------------------------

bool IsMeshDistanceFieldEnabled()
{
	static const auto* CVarGenerateMeshDistanceFields = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
	return CVarGenerateMeshDistanceFields != nullptr && CVarGenerateMeshDistanceFields->GetValueOnAnyThread() > 0;
}

//------------------------------------------------------------------------------------------------------------

template<typename BufferType, EPixelFormat PixelFormat>
void CreateInternalBuffer(FReadBuffer& OutputBuffer, uint32 ElementCount)
{
	if (ElementCount > 0)
	{
		OutputBuffer.Initialize(TEXT("FNDIRigidMeshCollisionBuffer"), sizeof(BufferType), ElementCount, PixelFormat, BUF_Static);
	}
}

template<typename BufferType, EPixelFormat PixelFormat>
void UpdateInternalBuffer(const TArray<BufferType>& InputData, FReadBuffer& OutputBuffer)
{
	uint32 ElementCount = InputData.Num();
	if (ElementCount > 0 && OutputBuffer.Buffer.IsValid())
	{
		const uint32 BufferBytes = sizeof(BufferType) * ElementCount;

		void* OutputData = RHILockBuffer(OutputBuffer.Buffer, 0, BufferBytes, RLM_WriteOnly);

		FMemory::Memcpy(OutputData, InputData.GetData(), BufferBytes);
		RHIUnlockBuffer(OutputBuffer.Buffer);
	}
}

void FillCurrentTransforms(const FTransform& ElementTransform, uint32 ElementCount, TArray<FVector4f>& OutCurrentTransform, TArray<FVector4f>& OutCurrentInverse)
{
	// LWC_TODO: precision loss
	const uint32 ElementOffset = 3 * ElementCount;
	const FMatrix44f ElementMatrix = FMatrix44f(ElementTransform.ToMatrixWithScale());
	const FMatrix44f ElementInverse = ElementMatrix.Inverse();

	ElementMatrix.To3x4MatrixTranspose(&OutCurrentTransform[ElementOffset].X);
	ElementInverse.To3x4MatrixTranspose(&OutCurrentInverse[ElementOffset].X);
}

template<typename TComponentType, typename TComponentFilterPredicate>
static void GenerateComponentList(
	TConstArrayView<AActor*> Actors,
	TConstArrayView<FName> ComponentTags,
	TComponentFilterPredicate FilterPredicate,
	TInlineComponentArray<TComponentType*>& Components)
{
	for (const AActor* Actor : Actors)
	{
		if (Actor)
		{
			for (UActorComponent* ActorComponent : Actor->GetComponents())
			{
				if (TComponentType* TypedComponent = Cast<TComponentType>(ActorComponent))
				{
					if (IsValid(TypedComponent) && FilterPredicate(TypedComponent))
					{
						if (ComponentTags.IsEmpty() || ComponentTags.ContainsByPredicate([&](const FName& Tag) { return Tag == NAME_None || TypedComponent->ComponentHasTag(Tag); }))
						{
							Components.Add(TypedComponent);
						}
					}
				}
			}
		}
	}
}

template<typename TComponentType>
void CollectComponents(TConstArrayView<AActor*> Actors, TConstArrayView<FName> ComponentTags, TInlineComponentArray<TComponentType*>& Components);

template<typename TComponentType, typename TBodySetupPredicate>
void ForEachBodySetup(TComponentType* Component, TBodySetupPredicate Predicate);

/// Begin UkeletalMeshComponent

template<>
void CollectComponents(TConstArrayView<AActor*> Actors, TConstArrayView<FName> ComponentTags, TInlineComponentArray<USkeletalMeshComponent*>& Components)
{
	auto SkeletalFilterPredicate = [&](USkeletalMeshComponent* Component)
	{
		if (UPhysicsAsset* PhysicsAsset = Component->GetPhysicsAsset())
		{
			USkeletalMesh* MeshAsset = Component->GetSkeletalMeshAsset() ? Component->GetSkeletalMeshAsset() : PhysicsAsset->GetPreviewMesh();
			if (!MeshAsset || !MeshAsset->GetRefSkeleton().GetNum())
			{
				return false;
			}

			return true;
		}

		return false;
	};

	GenerateComponentList(Actors, ComponentTags, SkeletalFilterPredicate, Components);
}

template<typename TBodySetupPredicate>
void ForEachBodySetup(USkeletalMeshComponent* Component, TBodySetupPredicate Predicate)
{
	if (UPhysicsAsset* PhysicsAsset = Component->GetPhysicsAsset())
	{
		USkeletalMesh* SkeletalMesh = Component->GetSkeletalMeshAsset() ? Component->GetSkeletalMeshAsset() : PhysicsAsset->GetPreviewMesh();
		const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();

		for (const UBodySetup* BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			const FName BoneName = BodySetup->BoneName;
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
			if (BoneIndex != INDEX_NONE && BoneIndex < RefSkeleton.GetNum())
			{
				Predicate(Component, BodySetup);
			}
		}
	}
}

/// End UkeletalMeshComponent

/// Begin UStaticMeshComponent

template<>
void CollectComponents(TConstArrayView<AActor*> Actors, TConstArrayView<FName> ComponentTags, TInlineComponentArray<UStaticMeshComponent*>& Components)
{
	auto StaticFilterPredicate = [&](UStaticMeshComponent* Component)
	{
		return Component->GetBodySetup() != nullptr;
	};

	GenerateComponentList(Actors, ComponentTags, StaticFilterPredicate, Components);
}

template<typename TBodySetupPredicate>
void ForEachBodySetup(UStaticMeshComponent* Component, TBodySetupPredicate Predicate)
{
	Predicate(Component, Component->GetBodySetup());
}

/// End UStaticMeshComponent

template<typename TComponentType>
void CountCollisionPrimitives(TConstArrayView<TComponentType*> Components, uint32& BoxCount, uint32& SphereCount, uint32& CapsuleCount)
{
	for (TComponentType* Component : Components)
	{
		bool HasConvexElements = false;

		ForEachBodySetup(Component, [&](TComponentType* Component, const UBodySetup* BodySetup)
		{
			for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
			{
				if (CollisionEnabledHasPhysics(ConvexElem.GetCollisionEnabled()))
				{
					HasConvexElements = true;
					++BoxCount;
				}
			}
			for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
			{
				if (CollisionEnabledHasPhysics(BoxElem.GetCollisionEnabled()))
				{
					++BoxCount;
				}
			}
			for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
			{
				if (CollisionEnabledHasPhysics(SphereElem.GetCollisionEnabled()))
				{
					++SphereCount;
				}
			}
			for (const FKSphylElem& CapsuleElem : BodySetup->AggGeom.SphylElems)
			{
				if (CollisionEnabledHasPhysics(CapsuleElem.GetCollisionEnabled()))
				{
					++CapsuleCount;
				}
			}
		});

		if (HasConvexElements)
		{
			UE_LOG(LogRigidMeshCollision, Warning, TEXT("Convex collision objects encountered and will be interpreted as a bounding box on %s"), *Component->GetOwner()->GetName());
		}
	}
}

template<typename TComponentType>
FTransform CreateElementTransform(const TComponentType* Component, const UBodySetup* BodySetup)
{
	return Component->GetComponentTransform();
}

template<>
FTransform CreateElementTransform<USkeletalMeshComponent>(const USkeletalMeshComponent* Component, const UBodySetup* BodySetup)
{
	if (USkeletalMesh* SkeletalMesh = Component->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& RefSkeleton = SkeletalMesh->GetRefSkeleton();
		const int32 BoneCount = RefSkeleton.GetNum();

		if (BoneCount > 0)
		{
			const FName BoneName = BodySetup->BoneName;
			const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
			if (BoneIndex != INDEX_NONE && BoneIndex < BoneCount)
			{
				return Component->GetBoneTransform(BoneIndex);
			}
		}
	}

	return Component->GetComponentTransform();
}

template<typename TComponentType, bool InitializeStatics>
void UpdateAssetArrays(TConstArrayView<TComponentType*> Components, const FVector& LWCTile, FNDIRigidMeshCollisionArrays* OutAssetArrays, uint32& BoxIndex, uint32& SphereIndex, uint32& CapsuleIndex)
{
	auto UpdateAssetPredicate = [&](TComponentType* Component, const UBodySetup* BodySetup)
	{
		FTransform MeshTransform = CreateElementTransform(Component, BodySetup);
		MeshTransform.AddToTranslation(LWCTile * -FLargeWorldRenderScalar::GetTileSize());

		const int32 ComponentIdIndex = OutAssetArrays->UniqueCompnentId.AddUnique(Component->ComponentId);

		for (const FKConvexElem& ConvexElem : BodySetup->AggGeom.ConvexElems)
		{
			if (CollisionEnabledHasPhysics(ConvexElem.GetCollisionEnabled()))
			{
				FBox BBox = ConvexElem.ElemBox;

				if (InitializeStatics)
				{
					FVector3f Extent = FVector3f(BBox.Max - BBox.Min);
					OutAssetArrays->ElementExtent[BoxIndex] = FVector4f(Extent.X, Extent.Y, Extent.Z, 0);
					OutAssetArrays->PhysicsType[BoxIndex] = (ConvexElem.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
					OutAssetArrays->ComponentIdIndex[BoxIndex] = ComponentIdIndex;
				}

				FVector Center = (BBox.Max + BBox.Min) * .5;
				const FTransform ElementTransform = FTransform(Center) * MeshTransform;
				FillCurrentTransforms(ElementTransform, BoxIndex, OutAssetArrays->CurrentTransform, OutAssetArrays->CurrentInverse);
				++BoxIndex;
			}
		}
		for (const FKBoxElem& BoxElem : BodySetup->AggGeom.BoxElems)
		{
			if (CollisionEnabledHasPhysics(BoxElem.GetCollisionEnabled()))
			{
				if (InitializeStatics)
				{
					OutAssetArrays->ElementExtent[BoxIndex] = FVector4f(BoxElem.X, BoxElem.Y, BoxElem.Z, 0);
					OutAssetArrays->PhysicsType[BoxIndex] = (BoxElem.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
					OutAssetArrays->ComponentIdIndex[BoxIndex] = ComponentIdIndex;
				}

				const FTransform ElementTransform = FTransform(BoxElem.Rotation, BoxElem.Center) * MeshTransform;
				FillCurrentTransforms(ElementTransform, BoxIndex, OutAssetArrays->CurrentTransform, OutAssetArrays->CurrentInverse);
				++BoxIndex;
			}
		}

		for (const FKSphereElem& SphereElem : BodySetup->AggGeom.SphereElems)
		{
			if (CollisionEnabledHasPhysics(SphereElem.GetCollisionEnabled()))
			{
				if (InitializeStatics)
				{
					OutAssetArrays->ElementExtent[SphereIndex] = FVector4f(SphereElem.Radius, 0, 0, 0);
					OutAssetArrays->PhysicsType[SphereIndex] = (SphereElem.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
					OutAssetArrays->ComponentIdIndex[SphereIndex] = ComponentIdIndex;
				}

				const FTransform ElementTransform = FTransform(SphereElem.Center) * MeshTransform;
				FillCurrentTransforms(ElementTransform, SphereIndex, OutAssetArrays->CurrentTransform, OutAssetArrays->CurrentInverse);
				++SphereIndex;
			}
		}

		for (const FKSphylElem& CapsuleElem : BodySetup->AggGeom.SphylElems)
		{
			if (CollisionEnabledHasPhysics(CapsuleElem.GetCollisionEnabled()))
			{
				if (InitializeStatics)
				{
					OutAssetArrays->ElementExtent[CapsuleIndex] = FVector4f(CapsuleElem.Radius, CapsuleElem.Length, 0, 0);
					OutAssetArrays->PhysicsType[CapsuleIndex] = (CapsuleElem.GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics);
					OutAssetArrays->ComponentIdIndex[CapsuleIndex] = ComponentIdIndex;
				}

				const FTransform ElementTransform = FTransform(CapsuleElem.Rotation, CapsuleElem.Center) * MeshTransform;
				FillCurrentTransforms(ElementTransform, CapsuleIndex, OutAssetArrays->CurrentTransform, OutAssetArrays->CurrentInverse);
				++CapsuleIndex;
			}
		}
	};

	for (TComponentType* Component : Components)
	{
		ForEachBodySetup(Component, UpdateAssetPredicate);
	}
}

void UpdateInternalArrays(TConstArrayView<AActor*> Actors, TConstArrayView<FName> ComponentTags, FVector LWCTile, bool bFullUpdate, FNDIRigidMeshCollisionArrays* OutAssetArrays)
{
	if (OutAssetArrays != nullptr && OutAssetArrays->ElementOffsets.NumElements < OutAssetArrays->MaxPrimitives)
	{
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshes;
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;

		uint32 BoxCount = 0;
		uint32 SphereCount = 0;
		uint32 CapsuleCount = 0;

		CollectComponents(Actors, ComponentTags, StaticMeshes);
		CollectComponents(Actors, ComponentTags, SkeletalMeshes);

		TConstArrayView<UStaticMeshComponent*> StaticMeshView = MakeArrayView(StaticMeshes.GetData(), StaticMeshes.Num());
		TConstArrayView<USkeletalMeshComponent*> SkeletalMeshView = MakeArrayView(SkeletalMeshes.GetData(), SkeletalMeshes.Num());

		CountCollisionPrimitives(StaticMeshView, BoxCount, SphereCount, CapsuleCount);
		CountCollisionPrimitives(SkeletalMeshView, BoxCount, SphereCount, CapsuleCount);

		if ((BoxCount + SphereCount + CapsuleCount) >= OutAssetArrays->MaxPrimitives)
		{
			UE_LOG(LogRigidMeshCollision, Error, TEXT("Number of Collision DI primitives is higher than the %d limit.  Please increase it."), OutAssetArrays->MaxPrimitives);
			return;
		}

		const bool MismatchOffsets = ((OutAssetArrays->ElementOffsets.SphereOffset - OutAssetArrays->ElementOffsets.BoxOffset) != BoxCount) ||
			((OutAssetArrays->ElementOffsets.CapsuleOffset - OutAssetArrays->ElementOffsets.SphereOffset) != SphereCount) ||
			((OutAssetArrays->ElementOffsets.NumElements - OutAssetArrays->ElementOffsets.CapsuleOffset) != CapsuleCount);

		// if we're only running an update, then make sure that the offsets aren't mismatched
		check(!MismatchOffsets || bFullUpdate);

		if (bFullUpdate)
		{			
			OutAssetArrays->ElementOffsets.BoxOffset = 0;
			OutAssetArrays->ElementOffsets.SphereOffset = OutAssetArrays->ElementOffsets.BoxOffset + BoxCount;
			OutAssetArrays->ElementOffsets.CapsuleOffset = OutAssetArrays->ElementOffsets.SphereOffset + SphereCount;
			OutAssetArrays->ElementOffsets.NumElements = OutAssetArrays->ElementOffsets.CapsuleOffset + CapsuleCount;
		}

		uint32 BoxIndex = OutAssetArrays->ElementOffsets.BoxOffset;
		uint32 SphereIndex = OutAssetArrays->ElementOffsets.SphereOffset;
		uint32 CapsuleIndex = OutAssetArrays->ElementOffsets.CapsuleOffset;

		if (bFullUpdate)
		{
			UpdateAssetArrays<UStaticMeshComponent, true>(StaticMeshView, LWCTile, OutAssetArrays, BoxIndex, SphereIndex, CapsuleIndex);
			UpdateAssetArrays<USkeletalMeshComponent, true>(SkeletalMeshView, LWCTile, OutAssetArrays, BoxIndex, SphereIndex, CapsuleIndex);

			// for newly created array data we need to duplicate the transforms to our previous transforms
			OutAssetArrays->PreviousTransform = OutAssetArrays->CurrentTransform;
			OutAssetArrays->PreviousInverse = OutAssetArrays->CurrentInverse;
		}
		else
		{
			// if we're updating, then copy over last frame's transforms before we generate new ones
			OutAssetArrays->PreviousTransform = OutAssetArrays->CurrentTransform;
			OutAssetArrays->PreviousInverse = OutAssetArrays->CurrentInverse;

			UpdateAssetArrays<UStaticMeshComponent, false>(StaticMeshView, LWCTile, OutAssetArrays, BoxIndex, SphereIndex, CapsuleIndex);
			UpdateAssetArrays<USkeletalMeshComponent, false>(SkeletalMeshView, LWCTile, OutAssetArrays, BoxIndex, SphereIndex, CapsuleIndex);
		}
	}
}

} // NDIRigidMeshCollisionLocal

//------------------------------------------------------------------------------------------------------------

void FNDIRigidMeshCollisionBuffer::InitRHI()
{
	using namespace NDIRigidMeshCollisionLocal;

	CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(WorldTransformBuffer, 3 * MaxNumTransforms);
	CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(InverseTransformBuffer, 3 * MaxNumTransforms);

	CreateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ElementExtentBuffer, MaxNumPrimitives);
	CreateInternalBuffer<uint32, EPixelFormat::PF_R32_UINT>(PhysicsTypeBuffer, MaxNumPrimitives);
	CreateInternalBuffer<uint32, EPixelFormat::PF_R32_UINT>(DFIndexBuffer, MaxNumPrimitives);
}

void FNDIRigidMeshCollisionBuffer::ReleaseRHI()
{
	WorldTransformBuffer.Release();
	InverseTransformBuffer.Release();
	ElementExtentBuffer.Release();
	PhysicsTypeBuffer.Release();
	DFIndexBuffer.Release();
}

//------------------------------------------------------------------------------------------------------------



void FNDIRigidMeshCollisionData::ReleaseBuffers()
{
	if (AssetBuffer)
	{
		BeginReleaseResource(AssetBuffer);
		ENQUEUE_RENDER_COMMAND(DeleteResource)(
			[ParamPointerToRelease = AssetBuffer](FRHICommandListImmediate& RHICmdList)
			{
				delete ParamPointerToRelease;
			});
		AssetBuffer = nullptr;
	}
}

bool FNDIRigidMeshCollisionData::HasActors() const
{
	return !ExplicitActors.IsEmpty() || !FoundActors.IsEmpty();
}

bool FNDIRigidMeshCollisionData::ShouldRunGlobalSearch(UNiagaraDataInterfaceRigidMeshCollisionQuery* Interface) const
{
	return Interface->GlobalSearchAllowed && (Interface->GlobalSearchForced || (Interface->GlobalSearchFallback_Unscripted && !bHasScriptedFindActor));
}

void FNDIRigidMeshCollisionData::MergeActors(FMergedActorArray& MergedActors) const
{
	MergedActors.Reserve(ExplicitActors.Num() + FoundActors.Num());

	auto AppendActors = [&](const TWeakObjectPtr<AActor>& ActorPtr)
	{
		if (AActor* Actor = ActorPtr.Get())
		{
			MergedActors.AddUnique(Actor);
		}
	};

	Algo::ForEach(ExplicitActors, AppendActors);
	Algo::ForEach(FoundActors, AppendActors);
}

void FNDIRigidMeshCollisionData::Init(int32 MaxNumPrimitives)
{
	const bool bHasActors = HasActors();
	const bool bWasInitialized = AssetArrays.IsValid();

	if (bHasActors)
	{
		if (!bWasInitialized)
		{
			AssetArrays = MakeUnique<FNDIRigidMeshCollisionArrays>(MaxNumPrimitives);

			AssetBuffer = new FNDIRigidMeshCollisionBuffer();
			AssetBuffer->SetMaxNumPrimitives(MaxNumPrimitives);

			BeginInitResource(AssetBuffer);
		}

		AssetArrays->Reset();
	}
	else if (bWasInitialized)
	{
		AssetArrays = nullptr;
		ReleaseBuffers();
	}

	bFoundActorsUpdated = false;
	bRequiresFullUpdate = true;
}

void FNDIRigidMeshCollisionData::Update(UNiagaraDataInterfaceRigidMeshCollisionQuery* Interface)
{
	using namespace NDIRigidMeshCollisionLocal;

	if (!Interface || !SystemInstance || !bRequiresSourceActors)
	{
		return;
	}

	const bool bExplicitActorsChanged = Interface->GetExplicitActors(*this);
	if (ShouldRunGlobalSearch(Interface))
	{
		if (Interface->GlobalFindActors(SystemInstance->GetWorld(), *this))
		{
			bFoundActorsUpdated = true;
		}
	}

	// see if we need to reinitialize the internals
	const bool bAlreadyInited = AssetArrays != nullptr;
	const bool bHasActors = HasActors();
	if (bAlreadyInited != bHasActors)
	{
		Init(Interface->MaxNumPrimitives);
	}

	if (bHasActors)
	{
		FMergedActorArray MergedActors;
		MergeActors(MergedActors);

		const bool bFullUpdate = bRequiresFullUpdate || bExplicitActorsChanged || bFoundActorsUpdated;
		UpdateInternalArrays(MergedActors, Interface->ComponentTags, FVector(SystemInstance->GetLWCTile()), bFullUpdate, AssetArrays.Get());
	}

	bRequiresFullUpdate = false;
	bFoundActorsUpdated = false;
}

//------------------------------------------------------------------------------------------------------------

/** Proxy to send data to gpu */
struct FNDIRigidMeshCollisionProxy : public FNiagaraDataInterfaceProxy
{
	struct FRenderThreadData
	{
		FNDIRigidMeshCollisionElementOffset ElementOffsets;
		TArray<FVector4f> WorldTransform;
		TArray<FVector4f> InverseTransform;
		TArray<FVector4f> ElementExtent;
		TArray<uint32> PhysicsType;
		TArray<int32> ComponentIdIndex;
		uint32 MaxPrimitiveCount;
		TArray<FPrimitiveComponentId> UniqueComponentIds;

		FNDIRigidMeshCollisionBuffer* AssetBuffer = nullptr;
	};

	/** Get the size of the data that will be passed to render*/
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return sizeof(FRenderThreadData);
	}

	/** Get the data that will be passed to render*/
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override
	{
		check(IsInRenderingThread());

		const FRenderThreadData* SourceData = reinterpret_cast<FRenderThreadData*>(PerInstanceData);
		FRenderThreadData& TargetData = SystemInstancesToProxyData_RT.FindOrAdd(Instance);

		if (ensure(SourceData))
		{
			TargetData = *SourceData;
			SourceData->~FRenderThreadData();
		}
	}

	/** Launch all pre stage functions */
	virtual void PreStage(const FNDIGpuComputePostStageContext& Context) override
	{
		using namespace NDIRigidMeshCollisionLocal;

		check(SystemInstancesToProxyData_RT.Contains(Context.GetSystemInstanceID()));

		FRenderThreadData* ProxyData = SystemInstancesToProxyData_RT.Find(Context.GetSystemInstanceID());
		if (ProxyData != nullptr && ProxyData->AssetBuffer)
		{
			if (Context.GetSimStageData().bFirstStage)
			{
				//-OPT: We may be able to avoid updating the buffers all the time
				UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->WorldTransform, ProxyData->AssetBuffer->WorldTransformBuffer);
				UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->InverseTransform, ProxyData->AssetBuffer->InverseTransformBuffer);
				UpdateInternalBuffer<FVector4f, EPixelFormat::PF_A32B32G32R32F>(ProxyData->ElementExtent, ProxyData->AssetBuffer->ElementExtentBuffer);
				UpdateInternalBuffer<uint32, EPixelFormat::PF_R32_UINT>(ProxyData->PhysicsType, ProxyData->AssetBuffer->PhysicsTypeBuffer);

				// the distance field indexing needs to be generated using the scene
				if (!ProxyData->ComponentIdIndex.IsEmpty() && ProxyData->AssetBuffer->DFIndexBuffer.Buffer.IsValid())
				{
					const int32 ElementCount = ProxyData->ComponentIdIndex.Num();
					const uint32 BufferBytes = sizeof(uint32) * ElementCount;
					void* BufferData = RHILockBuffer(ProxyData->AssetBuffer->DFIndexBuffer.Buffer, 0, BufferBytes, RLM_WriteOnly);

					const FScene* Scene = Context.GetComputeDispatchInterface().GetScene();
					if (Scene && !ProxyData->UniqueComponentIds.IsEmpty())
					{
						TArray<uint32> UniqueDistanceFieldIndices;
						UniqueDistanceFieldIndices.Reserve(ProxyData->UniqueComponentIds.Num());

						for (const FPrimitiveComponentId& ComponentId : ProxyData->UniqueComponentIds)
						{
							uint32& DistanceFieldIndex = UniqueDistanceFieldIndices.Add_GetRef(INDEX_NONE);
							const int32 PrimitiveSceneIndex = Scene->PrimitiveComponentIds.Find(ComponentId);
							if (PrimitiveSceneIndex != INDEX_NONE)
							{
								const TArray<int32, TInlineAllocator<1>>& DFIndices = Scene->Primitives[PrimitiveSceneIndex]->DistanceFieldInstanceIndices;
								DistanceFieldIndex = DFIndices.IsEmpty() ? INDEX_NONE : DFIndices[0];
							}
						}

						TArrayView<uint32> BufferView(reinterpret_cast<uint32*>(BufferData), ElementCount);
						for (int32 ElementIt = 0; ElementIt < ElementCount; ++ElementIt)
						{
							const int32 UniqueIdIndex = ProxyData->ComponentIdIndex[ElementIt];
							BufferView[ElementIt] = UniqueDistanceFieldIndices.IsValidIndex(UniqueIdIndex) ? UniqueDistanceFieldIndices[UniqueIdIndex] : INDEX_NONE;
						}
					}
					else
					{
						FMemory::Memset(BufferData, 0xFF, BufferBytes);
					}
					RHIUnlockBuffer(ProxyData->AssetBuffer->DFIndexBuffer.Buffer);
				}
			}
		}
	}

	/** List of proxy data for each system instances*/
	TMap<FNiagaraSystemInstanceID, FRenderThreadData> SystemInstancesToProxyData_RT;
};

//------------------------------------------------------------------------------------------------------------

UNiagaraDataInterfaceRigidMeshCollisionQuery::UNiagaraDataInterfaceRigidMeshCollisionQuery(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)	
{
	Proxy.Reset(new FNDIRigidMeshCollisionProxy());
}

#if WITH_NIAGARA_DEBUGGER

void UNiagaraDataInterfaceRigidMeshCollisionQuery::DrawDebugHud(UCanvas* Canvas, FNiagaraSystemInstance* SystemInstance, FString& VariableDataString, bool bVerbose) const
{
	FNDIRigidMeshCollisionData* InstanceData_GT = SystemInstance->FindTypedDataInterfaceInstanceData<FNDIRigidMeshCollisionData>(this);
	if (InstanceData_GT == nullptr || !InstanceData_GT->AssetArrays.IsValid())
	{
		return;
	}

	const FNDIRigidMeshCollisionElementOffset& ElementOffsets = InstanceData_GT->AssetArrays->ElementOffsets;

	const uint32 BoxCount = ElementOffsets.SphereOffset - ElementOffsets.BoxOffset;
	const uint32 SphereCount = ElementOffsets.CapsuleOffset - ElementOffsets.SphereOffset;
	const uint32 CapsuleCount = ElementOffsets.NumElements - ElementOffsets.CapsuleOffset;

	VariableDataString = FString::Printf(TEXT("Boxes(%d) Spheres(%d) Capsules(%d)"), BoxCount, SphereCount, CapsuleCount);

	auto GetCurrentTransform = [&](int32 ElementIndex)
	{
		const uint32 ElementOffset = 3 * ElementIndex;
		FVector4f* TransformVec = InstanceData_GT->AssetArrays->CurrentTransform.GetData() + ElementOffset;

		FMatrix ElementMatrix;
		ElementMatrix.SetIdentity();

		for (int32 RowIt = 0; RowIt < 3; ++RowIt)
		{
			for (int32 ColIt = 0; ColIt < 4; ++ColIt)
			{
				ElementMatrix.M[RowIt][ColIt] = TransformVec[RowIt][ColIt];
			}
		}

		return ElementMatrix.GetTransposed();
	};

	if (bVerbose)
	{
		// the DrawDebugCanvas* functions don't reasoanbly handle the near clip plane (both in terms of clipping and in terms of
		// objects being behind the camera); so we introduce this culling behavior to work around it
		auto ShouldClip = [&](UCanvas* Canvas, const FMatrix& Transform, const FBoxSphereBounds& Bounds)
		{
			const FVector Origin = Transform.TransformPosition(Bounds.Origin);
			return (Canvas->Project(Origin).GetMin() < UE_KINDA_SMALL_NUMBER);
		};

		// Boxes
		for (uint32 BoxIt = 0; BoxIt < BoxCount; ++BoxIt)
		{
			const FVector3f HalfBoxExtent = 0.5f * InstanceData_GT->AssetArrays->ElementExtent[ElementOffsets.BoxOffset + BoxIt];
			const FBox Box(-HalfBoxExtent, HalfBoxExtent);
			const FMatrix CurrentTransform = GetCurrentTransform(ElementOffsets.BoxOffset + BoxIt);
			if (!ShouldClip(Canvas, CurrentTransform, FSphere(FVector::ZeroVector, HalfBoxExtent.Size())))
			{
				DrawDebugCanvasWireBox(Canvas, CurrentTransform, Box, FColor::Blue);
			}
		}

		// Spheres
		for (uint32 SphereIt = 0; SphereIt < SphereCount; ++SphereIt)
		{
			const float Radius = InstanceData_GT->AssetArrays->ElementExtent[ElementOffsets.SphereOffset + SphereIt].X;
			const FMatrix CurrentTransform = GetCurrentTransform(ElementOffsets.SphereOffset + SphereIt);
			if (!ShouldClip(Canvas, CurrentTransform, FSphere(FVector::ZeroVector, Radius)))
			{
				DrawDebugCanvasWireSphere(Canvas, CurrentTransform.TransformPosition(FVector::ZeroVector), FColor::Blue, Radius, 20);
			}
		}

		// Capsules
		for (uint32 CapsuleIt = 0; CapsuleIt < CapsuleCount; ++CapsuleIt)
		{
			const FVector2f RadiusLength(InstanceData_GT->AssetArrays->ElementExtent[ElementOffsets.CapsuleOffset + CapsuleIt]);
			const FMatrix CurrentTransform = GetCurrentTransform(ElementOffsets.CapsuleOffset + CapsuleIt);
			const float HalfTotalLength = RadiusLength.X + 0.5f * RadiusLength.Y;
			if (!ShouldClip(Canvas, CurrentTransform, FSphere(FVector::ZeroVector, HalfTotalLength)))
			{
				DrawDebugCanvasCapsule(Canvas, CurrentTransform, HalfTotalLength, RadiusLength.X, FColor::Blue);
			}
		}
	}
}
#endif

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	using namespace NDIRigidMeshCollisionLocal;

	bool RequiresSourceActors = false;
	bool HasScriptedFindActor = false;

	FNiagaraDataInterfaceUtilities::ForEachGpuFunctionEquals(this, SystemInstance->GetSystem(), SystemInstance, [&](const FNiagaraDataInterfaceGeneratedFunction Function)
	{
		RequiresSourceActors = true;
		return false;
	});

	FNiagaraDataInterfaceUtilities::ForEachVMFunctionEquals(this, SystemInstance->GetSystem(), SystemInstance, [&](const FVMExternalFunctionBindingInfo& Binding)
	{
		if (Binding.Name == FindActorsName)
		{
			HasScriptedFindActor = true;
			return false;
		}
		return true;
	});

	FNDIRigidMeshCollisionData* InstanceData = new (PerInstanceData) FNDIRigidMeshCollisionData(SystemInstance, RequiresSourceActors, HasScriptedFindActor);

	GetExplicitActors(*InstanceData);

	// if we're running a global search, then run that now
	if (InstanceData->ShouldRunGlobalSearch(this))
	{
		GlobalFindActors(SystemInstance->GetWorld(), *InstanceData);
	}

	InstanceData->Init(MaxNumPrimitives);

	return true;
}

ETickingGroup UNiagaraDataInterfaceRigidMeshCollisionQuery::CalculateTickGroup(const void* PerInstanceData) const
{
	using namespace NDIRigidMeshCollisionLocal;

	if (const FNDIRigidMeshCollisionData* InstanceData = static_cast<const FNDIRigidMeshCollisionData*>(PerInstanceData))
	{
		ETickingGroup TickingGroup = NiagaraFirstTickGroup;

		TInlineComponentArray<UStaticMeshComponent*> StaticMeshes;
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;

		FNDIRigidMeshCollisionData::FMergedActorArray MergedActors;
		InstanceData->MergeActors(MergedActors);

		CollectComponents(MergedActors, ComponentTags, StaticMeshes);
		CollectComponents(MergedActors, ComponentTags, SkeletalMeshes);

		auto ProcessComponent = [&](const UActorComponent* Component)
		{
			const ETickingGroup ComponentTickGroup = FMath::Max(Component->PrimaryComponentTick.TickGroup, Component->PrimaryComponentTick.EndTickGroup);
			const ETickingGroup PhysicsTickGroup = ComponentTickGroup;
			const ETickingGroup ClampedTickGroup = FMath::Clamp(static_cast<ETickingGroup>(PhysicsTickGroup + 1), NiagaraFirstTickGroup, NiagaraLastTickGroup);

			TickingGroup = FMath::Max(TickingGroup, ClampedTickGroup);
		};

		for (const UStaticMeshComponent* Component : StaticMeshes)
		{
			ProcessComponent(Component);
		}

		for (const USkeletalMeshComponent* Component : SkeletalMeshes)
		{
			ProcessComponent(Component);
		}

		return TickingGroup;
	}
	return NiagaraFirstTickGroup;
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIRigidMeshCollisionData* InstanceData = static_cast<FNDIRigidMeshCollisionData*>(PerInstanceData);

	InstanceData->ReleaseBuffers();	
	InstanceData->~FNDIRigidMeshCollisionData();

	FNDIRigidMeshCollisionProxy* ThisProxy = GetProxyAs<FNDIRigidMeshCollisionProxy>();
	ENQUEUE_RENDER_COMMAND(FNiagaraDIDestroyInstanceData) (
		[ThisProxy, InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
	{
		ThisProxy->SystemInstancesToProxyData_RT.Remove(InstanceID);
	});
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float InDeltaSeconds)
{
	FNDIRigidMeshCollisionData* InstanceData = static_cast<FNDIRigidMeshCollisionData*>(PerInstanceData);
	if (InstanceData && SystemInstance)
	{
		check(InstanceData->SystemInstance == SystemInstance);
		InstanceData->Update(this);
	}
	return false;
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	UNiagaraDataInterfaceRigidMeshCollisionQuery* OtherTyped = CastChecked<UNiagaraDataInterfaceRigidMeshCollisionQuery>(Destination);		
	
	OtherTyped->ActorTags = ActorTags;
	OtherTyped->ComponentTags = ComponentTags;
	OtherTyped->SourceActors = SourceActors;
	OtherTyped->OnlyUseMoveable = OnlyUseMoveable;
	OtherTyped->GlobalSearchAllowed = GlobalSearchAllowed;
	OtherTyped->GlobalSearchForced = GlobalSearchForced;
	OtherTyped->GlobalSearchFallback_Unscripted = GlobalSearchFallback_Unscripted;
	OtherTyped->MaxNumPrimitives = MaxNumPrimitives;

	return true;
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceRigidMeshCollisionQuery* OtherTyped = CastChecked<const UNiagaraDataInterfaceRigidMeshCollisionQuery>(Other);

	return (OtherTyped->ActorTags == ActorTags)
		&& (OtherTyped->ComponentTags == ComponentTags)
		&& (OtherTyped->SourceActors == SourceActors)
		&& (OtherTyped->OnlyUseMoveable == OnlyUseMoveable)
		&& (OtherTyped->GlobalSearchAllowed == GlobalSearchAllowed)
		&& (OtherTyped->GlobalSearchForced == GlobalSearchForced)
		&& (OtherTyped->GlobalSearchFallback_Unscripted == GlobalSearchFallback_Unscripted)
		&& (OtherTyped->MaxNumPrimitives == MaxNumPrimitives);
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (!Tag_DEPRECATED.IsEmpty())
	{
		FName Tag = *Tag_DEPRECATED;
		ActorTags.AddUnique(Tag);
		Tag_DEPRECATED = TEXT("");
	}
#endif
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	using namespace NDIRigidMeshCollisionLocal;

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FindActorsName;
		Sig.SetDescription(LOCTEXT("FindActorsDescription", "Triggers an overlap test on the world to find actors to represent.."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = false;
		Sig.bSupportsCPU = true;
		Sig.bMemberFunction = true;
		Sig.bRequiresExecPin = true;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("RigidBody DI")));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Overlap Origin")), OverlapOriginDescription);
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Overlap Rotation")), OverlapRotationDescription);
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Overlap Extent")), OverlapExtentDescription);
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(StaticEnum<ECollisionChannel>()), TEXT("TraceChannel")), TraceChannelDescription);
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Skip Overlap")), SkipOverlapDescription);
		Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Actors Changed")));

		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumBoxesName;
		Sig.SetDescription(LOCTEXT("GetNumBoxesNameDescription", "Returns the number of box primitives for the collection of static meshes the DI represents."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num Boxes")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumSpheresName;
		Sig.SetDescription(LOCTEXT("GetNumSpheresNameDescription", "Returns the number of sphere primitives for the collection of static meshes the DI represents."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num Spheres")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetNumCapsulesName;
		Sig.SetDescription(LOCTEXT("GetNumCapsulesNameDescription", "Returns the number of capsule primitives for the collection of static meshes the DI represents."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Num Capsules")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetClosestPointName;
		Sig.SetDescription(LOCTEXT("GetClosestPointDescription", "Given a world space position, computes the static mesh's closest point. Also returns normal and velocity for that point."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Delta Time")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Closest Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Normal")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Velocity")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetClosestElementName;
		Sig.SetDescription(LOCTEXT("GetClosestElementDescription", "Given a world space position, computes the static mesh's closest element."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Closest Element")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetElementPointName;
		Sig.SetDescription(LOCTEXT("GetClosestElementPointDescription", "Given a world space position and an element index, computes the static mesh's closest point. Also returns normal and velocity for that point."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Delta Time")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Element Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Closest Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Normal")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Velocity")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetElementDistanceName;
		Sig.SetDescription(LOCTEXT("GetElementDistanceDescription", "Given a world space position and element index, computes the distance to the closest point for the static mesh."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Element Index")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));

		OutFunctions.Add(Sig);
	}
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetClosestDistanceName;
		Sig.SetDescription(LOCTEXT("GetClosestDistanceDescription", "Given a world space position, computes the distance to the closest point for the static mesh."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));

		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetClosestPointMeshDistanceFieldName;
		Sig.SetDescription(LOCTEXT("GetClosestPointMeshDistanceFieldDescription", "Given a world space position, computes the distance to the closest point for the static mesh, using the mesh's distance field."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Delta Time")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("MaxDistance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Closest Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Normal")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Velocity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Normal Is Valid")));		
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetClosestPointMeshDistanceFieldAccurateName;
		Sig.SetDescription(LOCTEXT("GetClosestPointMeshDistanceFieldDescription", "Given a world space position, computes the distance to the closest point for the static mesh, using the mesh's distance field."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Delta Time")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("MaxDistance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Closest Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Normal")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Velocity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Normal Is Valid")));
		OutFunctions.Add(Sig);
	}

	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = GetClosestPointMeshDistanceFieldNoNormalName;
		Sig.SetDescription(LOCTEXT("GetClosestPointMeshDistanceFieldNNDescription", "Given a world space position, computes the distance to the closest point for the static mesh, using the mesh's distance field.\nSkips the normal calculation and is more performant than it's counterpart with normal."));
		Sig.SetFunctionVersion(FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion);
		Sig.bSupportsGPU = true;
		Sig.bSupportsCPU = false;
		Sig.bMemberFunction = true;
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Collision DI")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("World Position")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Delta Time")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Time Fraction")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("MaxDistance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Closest Distance")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Closest Position")));		
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Closest Velocity")));

		OutFunctions.Add(Sig);
	}
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceRigidMeshCollisionQuery, FindActorsCPU);

void UNiagaraDataInterfaceRigidMeshCollisionQuery::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	using namespace NDIRigidMeshCollisionLocal;

	if (BindingInfo.Name == FindActorsName)
	{
		NDI_FUNC_BINDER(UNiagaraDataInterfaceRigidMeshCollisionQuery, FindActorsCPU)::Bind(this, OutFunc);
	}
	else
	{
		UE_LOG(LogNiagara, Display, TEXT("Could not find data interface external function in %s. %s\n"),
			*GetPathNameSafe(this), *BindingInfo.Name.ToString());
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceRigidMeshCollisionQuery::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	using namespace NDIRigidMeshCollisionLocal;

	if ((FunctionInfo.DefinitionName == GetNumBoxesName) ||
		(FunctionInfo.DefinitionName == GetNumCapsulesName) ||
		(FunctionInfo.DefinitionName == GetNumSpheresName) ||
		(FunctionInfo.DefinitionName == GetClosestPointName) ||
		(FunctionInfo.DefinitionName == GetClosestElementName) ||
		(FunctionInfo.DefinitionName == GetElementPointName) ||
		(FunctionInfo.DefinitionName == GetElementDistanceName) ||
		(FunctionInfo.DefinitionName == GetClosestDistanceName) ||
		(FunctionInfo.DefinitionName == GetClosestPointMeshDistanceFieldName) ||
		(FunctionInfo.DefinitionName == GetClosestPointMeshDistanceFieldAccurateName) ||
		(FunctionInfo.DefinitionName == GetClosestPointMeshDistanceFieldNoNormalName) )
	{
		return true;
	}
	OutHLSL += TEXT("\n");
	return false;
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	using namespace NDIRigidMeshCollisionLocal;

	bool bChanged = false;
	
	// upgrade from lwc changes, only parameter types changed there
	if (FunctionSignature.FunctionVersion < FNiagaraRigidMeshCollisionDIFunctionVersion::LargeWorldCoordinates)
	{
		if (FunctionSignature.Name == GetClosestPointName && ensure(FunctionSignature.Inputs.Num() == 4) && ensure(FunctionSignature.Outputs.Num() == 4))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			FunctionSignature.Outputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
		if (FunctionSignature.Name == GetClosestElementName && ensure(FunctionSignature.Inputs.Num() == 3))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
		if (FunctionSignature.Name == GetElementPointName && ensure(FunctionSignature.Inputs.Num() == 5) && ensure(FunctionSignature.Outputs.Num() == 3))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			FunctionSignature.Outputs[0].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
		if (FunctionSignature.Name == GetElementDistanceName && ensure(FunctionSignature.Inputs.Num() == 4))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
		if (FunctionSignature.Name == GetClosestDistanceName && ensure(FunctionSignature.Inputs.Num() == 3))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
		if (FunctionSignature.Name == GetClosestPointMeshDistanceFieldName && ensure(FunctionSignature.Inputs.Num() == 4) && ensure(FunctionSignature.Outputs.Num() == 4))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			FunctionSignature.Outputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
		if (FunctionSignature.Name == GetClosestPointMeshDistanceFieldNoNormalName && ensure(FunctionSignature.Inputs.Num() == 4) && ensure(FunctionSignature.Outputs.Num() == 3))
		{
			FunctionSignature.Inputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			FunctionSignature.Outputs[1].SetType(FNiagaraTypeDefinition::GetPositionDef());
			bChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraRigidMeshCollisionDIFunctionVersion::SetMaxDistance)
	{
		if (FunctionSignature.Name == GetClosestPointMeshDistanceFieldName)
		{
			FunctionSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("MaxDistance")));
			FunctionSignature.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetBoolDef()), TEXT("Normal Is Valid")));
			bChanged = true;
		}
		if (FunctionSignature.Name == GetClosestPointMeshDistanceFieldNoNormalName)
		{
			FunctionSignature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(FNiagaraTypeDefinition::GetFloatDef()), TEXT("MaxDistance")));
			bChanged = true;
		}
	}

	if (FunctionSignature.FunctionVersion < FNiagaraRigidMeshCollisionDIFunctionVersion::FindActorRotation)
	{
		if (FunctionSignature.Name == FindActorsName)
		{
			FNiagaraVariable OverlapRotation(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Overlap Rotation"));

			FunctionSignature.Inputs.Insert(OverlapRotation, 2);
			FunctionSignature.InputDescriptions.Add(OverlapRotation, OverlapRotationDescription);
			bChanged = true;
		}
	}

	FunctionSignature.FunctionVersion = FNiagaraRigidMeshCollisionDIFunctionVersion::LatestVersion;

	return bChanged;
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::GetCommonHLSL(FString& OutHLSL)
{
	Super::GetCommonHLSL(OutHLSL);

	OutHLSL += TEXT("#include \"/Engine/Private/DistanceFieldLightingShared.ush\"\n");
	OutHLSL += TEXT("#include \"/Engine/Private/MeshDistanceFieldCommon.ush\"\n");
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	Super::GetParameterDefinitionHLSL(ParamInfo, OutHLSL);

	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{TEXT("ParameterName"),	ParamInfo.DataInterfaceHLSLSymbol},
	};

	FString TemplateFile;
	LoadShaderSourceFile(NDIRigidMeshCollisionLocal::TemplateShaderFile, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
	{
		return false;
	}

	FSHAHash Hash = GetShaderFileHash(NDIRigidMeshCollisionLocal::TemplateShaderFile, EShaderPlatform::SP_PCD3D_SM5);
	InVisitor->UpdateString(TEXT("NiagaraDataInterfaceRigidMeshCollisionQueryHLSLSource"), Hash.ToString());
	InVisitor->UpdateShaderParameters<NDIRigidMeshCollisionLocal::FShaderParameters>();

	return true;
}
#endif

void UNiagaraDataInterfaceRigidMeshCollisionQuery::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<NDIRigidMeshCollisionLocal::FShaderParameters>();
	ShaderParametersBuilder.AddIncludedStruct<FDistanceFieldObjectBufferParameters>();
	ShaderParametersBuilder.AddIncludedStruct<FDistanceFieldAtlasParameters>();
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FRDGBuilder& GraphBuilder = Context.GetGraphBuilder();
	const FNDIRigidMeshCollisionProxy& InterfaceProxy = Context.GetProxy<FNDIRigidMeshCollisionProxy>();
	const FNDIRigidMeshCollisionProxy::FRenderThreadData* ProxyData = InterfaceProxy.SystemInstancesToProxyData_RT.Find(Context.GetSystemInstanceID());

	NDIRigidMeshCollisionLocal::FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<NDIRigidMeshCollisionLocal::FShaderParameters>();
	FDistanceFieldObjectBufferParameters* ShaderDistanceFieldObjectParameters = Context.GetParameterIncludedStruct<FDistanceFieldObjectBufferParameters>();
	FDistanceFieldAtlasParameters* ShaderDistanceFieldAtlasParameters = Context.GetParameterIncludedStruct<FDistanceFieldAtlasParameters>();

	const bool bDistanceFieldDataBound = Context.IsStructBound<FDistanceFieldObjectBufferParameters>(ShaderDistanceFieldObjectParameters) || Context.IsStructBound<FDistanceFieldAtlasParameters>(ShaderDistanceFieldAtlasParameters);
	const FDistanceFieldSceneData* DistanceFieldSceneData = nullptr;

	if (ProxyData != nullptr && ProxyData->AssetBuffer != nullptr && ProxyData->AssetBuffer->IsInitialized())
	{
		FNDIRigidMeshCollisionBuffer* AssetBuffer = ProxyData->AssetBuffer;

		ShaderParameters->WorldTransformBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat4(AssetBuffer->WorldTransformBuffer.SRV);
		ShaderParameters->InverseTransformBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat4(AssetBuffer->InverseTransformBuffer.SRV);
		ShaderParameters->ElementExtentBuffer = FNiagaraRenderer::GetSrvOrDefaultFloat4(AssetBuffer->ElementExtentBuffer.SRV);
		ShaderParameters->PhysicsTypeBuffer = FNiagaraRenderer::GetSrvOrDefaultUInt(AssetBuffer->PhysicsTypeBuffer.SRV);
		ShaderParameters->DFIndexBuffer = FNiagaraRenderer::GetSrvOrDefaultUInt(AssetBuffer->DFIndexBuffer.SRV);

		ShaderParameters->MaxTransforms = ProxyData->MaxPrimitiveCount * 2;
		ShaderParameters->CurrentOffset = 0;
		ShaderParameters->PreviousOffset = ProxyData->MaxPrimitiveCount * 3;

		ShaderParameters->ElementOffsets.X = ProxyData->ElementOffsets.BoxOffset;
		ShaderParameters->ElementOffsets.Y = ProxyData->ElementOffsets.SphereOffset;
		ShaderParameters->ElementOffsets.Z = ProxyData->ElementOffsets.CapsuleOffset;
		ShaderParameters->ElementOffsets.W = ProxyData->ElementOffsets.NumElements;

		if (bDistanceFieldDataBound)
		{
			DistanceFieldSceneData = static_cast<const FNiagaraGpuComputeDispatch&>(Context.GetComputeDispatchInterface()).GetMeshDistanceFieldParameters();	//-BATCHERTODO:
		}
	}
	else
	{
		ShaderParameters->WorldTransformBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->InverseTransformBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->ElementExtentBuffer = FNiagaraRenderer::GetDummyFloat4Buffer();
		ShaderParameters->PhysicsTypeBuffer = FNiagaraRenderer::GetDummyUIntBuffer();
		ShaderParameters->DFIndexBuffer = FNiagaraRenderer::GetDummyUIntBuffer();

		ShaderParameters->MaxTransforms = 0;
		ShaderParameters->CurrentOffset = 0;
		ShaderParameters->PreviousOffset = 0;
		ShaderParameters->ElementOffsets = FUintVector4(0, 0, 0, 0);
	}

	if (bDistanceFieldDataBound)
	{
		FNiagaraDistanceFieldHelper::SetMeshDistanceFieldParameters(Context.GetGraphBuilder(), DistanceFieldSceneData, *ShaderDistanceFieldObjectParameters, *ShaderDistanceFieldAtlasParameters, FNiagaraRenderer::GetDummyFloat4Buffer());
	}

	ShaderParameters->SystemLWCTile = Context.GetSystemLWCTile();
}

#if WITH_EDITOR
void UNiagaraDataInterfaceRigidMeshCollisionQuery::ValidateFunction(const FNiagaraFunctionSignature& Function, TArray<FText>& OutValidationErrors)
{
	using namespace NDIRigidMeshCollisionLocal;

	if (Function.Name == GetClosestPointMeshDistanceFieldName || Function.Name == GetClosestPointMeshDistanceFieldNoNormalName || Function.Name == GetClosestPointMeshDistanceFieldAccurateName)
	{
		if (!IsMeshDistanceFieldEnabled())
		{
			OutValidationErrors.Add(NSLOCTEXT("UNiagaraDataInterfaceRigidMeshCollisionQuery", "NiagaraDistanceFieldNotEnabledMsg", "The mesh distance field generation is currently not enabled, please check the project settings.\nNiagara cannot query the mesh distance fields otherwise."));
		}
	}
}
#endif

void UNiagaraDataInterfaceRigidMeshCollisionQuery::ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance)
{
	const FNDIRigidMeshCollisionData* GameThreadData = reinterpret_cast<const FNDIRigidMeshCollisionData*>(PerInstanceData);
	FNDIRigidMeshCollisionProxy::FRenderThreadData* RenderThreadData = new(DataForRenderThread) FNDIRigidMeshCollisionProxy::FRenderThreadData();

	if (ensure(GameThreadData != nullptr && RenderThreadData != nullptr))
	{
		if (const FNDIRigidMeshCollisionArrays* SourceArrayData = GameThreadData->AssetArrays.Get())
		{
			RenderThreadData->ElementOffsets = GameThreadData->AssetArrays->ElementOffsets;

			// compact the world/inverse transforms
			const int32 TransformVectorCount = GameThreadData->AssetArrays->MaxPrimitives * 3;

			auto CompactTransforms = [&](const TArray<FVector4f>& Current, const TArray<FVector4f>& Previous, TArray<FVector4f>& Compact)
			{
				Compact.Reset(2 * TransformVectorCount);
				Compact.Append(Current);
				Compact.SetNumUninitialized(TransformVectorCount, false);
				Compact.Append(Previous);
				Compact.SetNumUninitialized(2 * TransformVectorCount, false);
			};

			CompactTransforms(GameThreadData->AssetArrays->CurrentTransform, GameThreadData->AssetArrays->PreviousTransform, RenderThreadData->WorldTransform);
			CompactTransforms(GameThreadData->AssetArrays->CurrentInverse, GameThreadData->AssetArrays->PreviousInverse, RenderThreadData->InverseTransform);

			RenderThreadData->ElementExtent = GameThreadData->AssetArrays->ElementExtent;
			RenderThreadData->PhysicsType = GameThreadData->AssetArrays->PhysicsType;
			RenderThreadData->ComponentIdIndex = GameThreadData->AssetArrays->ComponentIdIndex;
			RenderThreadData->UniqueComponentIds = GameThreadData->AssetArrays->UniqueCompnentId;
			RenderThreadData->MaxPrimitiveCount = GameThreadData->AssetArrays->MaxPrimitives;
			RenderThreadData->AssetBuffer = GameThreadData->AssetBuffer;
		}
	}
	check(Proxy);
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::FilterComponent(const UPrimitiveComponent* Component) const
{
	return !(Component->IsA<USkeletalMeshComponent>() || Component->IsA<UStaticMeshComponent>());
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::FilterActor(const AActor* Actor) const
{
	if (OnlyUseMoveable && !Actor->IsRootComponentMovable())
	{
		return true;
	}

	if (!ActorTags.IsEmpty() && !ActorTags.ContainsByPredicate([&](const FName& Tag) { return Tag == NAME_None || Actor->Tags.Contains(Tag); }))
	{
		return true;
	}

	return false;
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::GlobalFindActors(UWorld* World, FNDIRigidMeshCollisionData& InstanceData) const
{
	TArray<TWeakObjectPtr<AActor>> PreviousActors;
	Swap(InstanceData.FoundActors, PreviousActors);

	if (ensure(World))
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (AActor* Actor = *It)
			{
				if (FilterActor(Actor))
				{
					continue;
				}

				InstanceData.FoundActors.AddUnique(Actor);
			}
		}
	}

	return PreviousActors != InstanceData.FoundActors;
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::FindActors(UWorld* World, FNDIRigidMeshCollisionData& InstanceData, ECollisionChannel Channel, const FVector& OverlapLocation, const FVector& OverlapExtent, const FQuat& OverlapRotation) const
{
	TArray<TWeakObjectPtr<AActor>> PreviousActors;
	Swap(InstanceData.FoundActors, PreviousActors);

	if (ensure(World))
	{
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(Channel);

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NiagaraRigidMeshCollisionQuery), false);

		World->OverlapMultiByChannel(Overlaps, OverlapLocation, OverlapRotation, Channel, FCollisionShape::MakeBox(0.5f * OverlapExtent), Params);

		for (const FOverlapResult& OverlapResult : Overlaps)
		{
			if (UPrimitiveComponent* PrimitiveComponent = OverlapResult.GetComponent())
			{
				if (FilterComponent(PrimitiveComponent))
				{
					continue;
				}

				if (AActor* ComponentActor = PrimitiveComponent->GetOwner())
				{
					if (FilterActor(ComponentActor))
					{
						continue;
					}
					InstanceData.FoundActors.AddUnique(ComponentActor);
				}
			}
		}
	}

	return PreviousActors != InstanceData.FoundActors;
}

bool UNiagaraDataInterfaceRigidMeshCollisionQuery::GetExplicitActors(FNDIRigidMeshCollisionData& InstanceData)
{
	if (!InstanceData.bRequiresSourceActors)
	{
		return false;
	}

	TArray<TWeakObjectPtr<AActor>> PreviousActors;
	Swap(InstanceData.ExplicitActors, PreviousActors);

	for (const TSoftObjectPtr<AActor>& ActorPtr : SourceActors)
	{
		if (AActor* Actor = ActorPtr.Get())
		{
			InstanceData.ExplicitActors.AddUnique(Actor);
		}
	}

	return InstanceData.ExplicitActors != PreviousActors;
}

void UNiagaraDataInterfaceRigidMeshCollisionQuery::FindActorsCPU(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FUserPtrHandler<FNDIRigidMeshCollisionData> InstanceData(Context);

	FNDIInputParam<FNiagaraPosition> OverlapOriginParam(Context);
	FNDIInputParam<FQuat4f> OverlapRotationParam(Context);
	FNDIInputParam<FVector3f> OverlapExtentParam(Context);
	FNDIInputParam<ECollisionChannel> TraceChannelParam(Context);
	FNDIInputParam<FNiagaraBool> SkipOverlapParam(Context);

	FNDIOutputParam<FNiagaraBool> ActorsChangedParam(Context);

	if (ensure(InstanceData->SystemInstance))
	{
		FNiagaraLWCConverter LWCConverter = InstanceData->SystemInstance->GetLWCConverter();
		UWorld* World = InstanceData->SystemInstance->GetWorld();

		if (World)
		{
			for (int32 i = 0; i < Context.GetNumInstances(); ++i)
			{
				FNiagaraPosition OverlapOrigin = OverlapOriginParam.GetAndAdvance();
				FQuat4f OverlapRotation = OverlapRotationParam.GetAndAdvance();
				FVector3f OverlapExtent = OverlapExtentParam.GetAndAdvance();
				ECollisionChannel TraceChannel = TraceChannelParam.GetAndAdvance();
				bool SkipOverlap = SkipOverlapParam.GetAndAdvance() || !InstanceData->bRequiresSourceActors;

				bool ActorsChanged = false;

				if (!SkipOverlap)
				{
					const FVector ConvertedOrigin = LWCConverter.ConvertSimulationPositionToWorld(OverlapOrigin);
					if (FindActors(World, *InstanceData, TraceChannel, ConvertedOrigin, FVector(OverlapExtent), FQuat(OverlapRotation)))
					{
						ActorsChanged = true;
						InstanceData->bFoundActorsUpdated = true;
					}
				}

				ActorsChangedParam.SetAndAdvance(ActorsChanged);
			}

			return;
		}
	}

	for (int32 i = 0; i < Context.GetNumInstances(); ++i)
	{
		ActorsChangedParam.SetAndAdvance(false);
	}
}

void UNiagaraDIRigidMeshCollisionFunctionLibrary::SetSourceActors(UNiagaraComponent* NiagaraComponent, FName OverrideName, const TArray<AActor*>& InSourceActors)
{
	if (UNiagaraDataInterfaceRigidMeshCollisionQuery* QueryDI = UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceRigidMeshCollisionQuery>(NiagaraComponent, OverrideName))
	{
		QueryDI->SourceActors.Reset(InSourceActors.Num());
		Algo::Transform(InSourceActors, QueryDI->SourceActors, [&](AActor* Actor)
		{
			return Actor;
		});
	}
}

#undef LOCTEXT_NAMESPACE
