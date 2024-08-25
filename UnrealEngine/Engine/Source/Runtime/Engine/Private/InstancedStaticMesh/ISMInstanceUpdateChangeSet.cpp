// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedStaticMesh/ISMInstanceUpdateChangeSet.h"
#include "InstancedStaticMesh/ISMScatterGatherUtil.h"
#include "Engine/InstancedStaticMesh.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Rendering/RenderingSpatialHash.h"
#include "Rendering/MotionVectorSimulation.h"
#include "SceneInterface.h"



#if WITH_EDITOR

/**
 * Ugly little wrapper to make sure hitproxies that are kept alive by the proxy on the RT are deleted on the GT
 */
class FOpaqueHitProxyContainer
{
public:
	FOpaqueHitProxyContainer(const TArray<TRefCountPtr<HHitProxy>>& InHitProxies) : HitProxies(InHitProxies) {}
	~FOpaqueHitProxyContainer();

private:
	FOpaqueHitProxyContainer(const FOpaqueHitProxyContainer &) = delete;
	void operator=(const FOpaqueHitProxyContainer &) = delete;

	TArray<TRefCountPtr<HHitProxy>> HitProxies;
};

FOpaqueHitProxyContainer::~FOpaqueHitProxyContainer()
{
	struct DeferDeleteHitProxies : FDeferredCleanupInterface
	{
		DeferDeleteHitProxies(TArray<TRefCountPtr<HHitProxy>>&& InHitProxies) : HitProxies(MoveTemp(InHitProxies)) {}
		TArray<TRefCountPtr<HHitProxy>> HitProxies;
	};

	BeginCleanup(new DeferDeleteHitProxies(MoveTemp(HitProxies)));
}


void FISMInstanceUpdateChangeSet::SetEditorData(const TArray<TRefCountPtr<HHitProxy>>& HitProxies, const TBitArray<> &InSelectedInstances)//, bool bWasHitProxiesReallocated)
{
	HitProxyContainer = MakeOpaqueHitProxyContainer(HitProxies);
	for (int32 Index = 0; Index < HitProxies.Num(); ++Index)
	{
		// Record if the instance is selected
		FColor HitProxyColor(ForceInit);
		bool bSelected = InSelectedInstances.IsValidIndex(Index) && InSelectedInstances[Index];
		if (HitProxies.IsValidIndex(Index))
		{
			HitProxyColor = HitProxies[Index]->Id.GetColor();
		}
		InstanceEditorData.Add(FInstanceEditorData::Pack(HitProxyColor, bSelected));
	}
	SelectedInstances = InSelectedInstances;
}

#endif

void FISMInstanceUpdateChangeSet::SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms, const FVector Offset)
{

	GatherTransform(GetTransformDelta(), Transforms, InInstanceTransforms, [Offset](const FMatrix &M) -> FRenderTransform { return FRenderTransform(M.ConcatTranslation(Offset)); });
}

void FISMInstanceUpdateChangeSet::SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms)
{
	GatherTransform(GetTransformDelta(), Transforms, InInstanceTransforms, [](const FMatrix &M) -> FRenderTransform { return FRenderTransform(M); });
}

void FISMInstanceUpdateChangeSet::SetInstanceTransforms(TStridedView<FMatrix> InInstanceTransforms, FBox const& InInstanceBounds, FBox& OutGatheredBounds)
{
	GatherTransform(GetTransformDelta(),Transforms, InInstanceTransforms, [&InInstanceBounds, &OutGatheredBounds](const FMatrix& M) -> FRenderTransform
	{
		FRenderTransform Transform(M);
		OutGatheredBounds += InInstanceBounds.TransformBy(Transform.ToMatrix());
		return Transform;
	});
}

void FISMInstanceUpdateChangeSet::SetInstancePrevTransforms(TArrayView<FMatrix> InPrevInstanceTransforms, const FVector &Offset)
{
	if (Flags.bHasPerInstanceDynamicData)
	{
		if (InPrevInstanceTransforms.IsEmpty())
		{
			Flags.bHasPerInstanceDynamicData = false;
		}
		else
		{
			GatherTransform(GetTransformDelta(), PrevTransforms, InPrevInstanceTransforms, [Offset](const FMatrix &M) -> FRenderTransform { return FRenderTransform(M.ConcatTranslation(Offset)); });
		}
	}
}

void FISMInstanceUpdateChangeSet::SetInstancePrevTransforms(TArrayView<FMatrix> InPrevInstanceTransforms)
{
	if (Flags.bHasPerInstanceDynamicData)
	{
		if (InPrevInstanceTransforms.IsEmpty())
		{
			Flags.bHasPerInstanceDynamicData = false;
		}
		else
		{
			GatherTransform(GetTransformDelta(), PrevTransforms, InPrevInstanceTransforms, [](const FMatrix &M) -> FRenderTransform { return FRenderTransform(M); });
		}
	}
}

void FISMInstanceUpdateChangeSet::SetCustomData(const TArrayView<const float> &InPerInstanceCustomData, int32 InNumCustomDataFloats)
{
	if (Flags.bHasPerInstanceCustomData)
	{
		check(InNumCustomDataFloats == NumCustomDataFloats);
		check(NumCustomDataFloats > 0);
		//check(InPerInstanceCustomData.Num() == InstanceIdIndexMap.GetMaxInstanceIndex() * NumCustomDataFloats);

		Gather(GetCustomDataDelta(), PerInstanceCustomData, InPerInstanceCustomData, InNumCustomDataFloats);
	}
	else
	{
		checkSlow(GetCustomDataDelta().IsEmpty());
	}
}

void FISMInstanceUpdateChangeSet::SetInstanceLocalBounds(const FRenderBounds &Bounds)
{
	InstanceLocalBounds.SetNum(1);
	InstanceLocalBounds[0] = Bounds;
}

#if WITH_EDITOR

TPimplPtr<FOpaqueHitProxyContainer> MakeOpaqueHitProxyContainer(const TArray<TRefCountPtr<HHitProxy>>& InHitProxies)
{
	return MakePimpl<FOpaqueHitProxyContainer>(InHitProxies);
}

#endif
