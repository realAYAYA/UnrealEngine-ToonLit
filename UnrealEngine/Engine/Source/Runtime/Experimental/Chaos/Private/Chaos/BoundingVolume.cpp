// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/BoundingVolume.h"
#include "Chaos/AABBTree.h"
#include "UObject/ExternalPhysicsCustomObjectVersion.h"

int FBoundingVolumeCVars::FilterFarBodies = 0;

FAutoConsoleVariableRef FBoundingVolumeCVars::CVarFilterFarBodies(
    TEXT("p.RemoveFarBodiesFromBVH"),
    FBoundingVolumeCVars::FilterFarBodies,
    TEXT("Removes bodies far from the scene from the bvh\n")
        TEXT("0: Kept, 1: Removed"),
    ECVF_Default);

namespace Chaos
{
#if WITH_EDITOR
	CHAOS_API int32 MaxDirtyElements = MAX_int32;
#else
	CHAOS_API int32 MaxDirtyElements = 10000;
#endif

	FAutoConsoleVariableRef CVarMaxDirtyElements(
	    TEXT("p.MaxDirtyElements"),
	    MaxDirtyElements,
	    TEXT("The max number of dirty elements. This forces a flush which is very expensive"));

	    template<typename TPayloadType, typename T, int d>
	    ISpatialAcceleration<TPayloadType, T, d>* ISpatialAcceleration<TPayloadType, T, d>::SerializationFactory(FChaosArchive& Ar, ISpatialAcceleration<TPayloadType, T, d>* Accel)
	{
		if (Ar.CustomVer(FExternalPhysicsCustomObjectVersion::GUID) < FExternalPhysicsCustomObjectVersion::SerializeEvolutionGenericAcceleration)
		{
			return new TBoundingVolume<TPayloadType, T, d>();
		}

		int8 AccelType = Ar.IsLoading() ? 0 : (int8)Accel->Type;
		Ar << AccelType;
		switch ((ESpatialAcceleration)AccelType)
		{
		case ESpatialAcceleration::BoundingVolume: return Ar.IsLoading() ? new TBoundingVolume<TPayloadType, T, d>() : nullptr;
		case ESpatialAcceleration::AABBTree: return Ar.IsLoading() ? new TAABBTree<TPayloadType, TAABBTreeLeafArray<TPayloadType>>() : nullptr;
		case ESpatialAcceleration::AABBTreeBV: return Ar.IsLoading() ? new TAABBTree<TPayloadType, TBoundingVolume<TPayloadType, T, 3>>() : nullptr;
		case ESpatialAcceleration::Collection: check(false);	//Collections must be serialized directly since they are variadic
		default: check(false); return nullptr;
		}
	}

	template class CHAOS_API Chaos::ISpatialAcceleration<int32, FReal, 3>;
	template class CHAOS_API Chaos::ISpatialAcceleration<FAccelerationStructureHandle, FReal, 3>;

	template class CHAOS_API Chaos::TBoundingVolume<int32, FReal, 3>;
	template class CHAOS_API Chaos::TBoundingVolume<FAccelerationStructureHandle, FReal, 3>;
}
