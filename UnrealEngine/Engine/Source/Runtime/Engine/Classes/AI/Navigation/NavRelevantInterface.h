// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"
#include "AI/Navigation/NavigationTypes.h"
#include "AI/NavigationModifier.h"
#include "NavRelevantInterface.generated.h"

struct FNavigableGeometryExport;

struct FNavigationRelevantDataFilter 
{
	/** pass when actor has geometry */
	uint32 bIncludeGeometry : 1;
	/** pass when actor has any offmesh link modifier */
	uint32 bIncludeOffmeshLinks : 1;
	/** pass when actor has any area modifier */
	uint32 bIncludeAreas : 1;
	/** pass when actor has any modifier with meta area */
	uint32 bIncludeMetaAreas : 1;
	/** fail if from level loading (only valid in WP dynamic mode) */
	uint32 bExcludeLoadedData : 1;

	FNavigationRelevantDataFilter() 
		: bIncludeGeometry(false)
		, bIncludeOffmeshLinks(false)
		, bIncludeAreas(false)
		, bIncludeMetaAreas(false)
		, bExcludeLoadedData(false)
	{}
};

// @todo consider optional structures that can contain a delegate instead of 
// actual copy of collision data
struct ENGINE_API FNavigationRelevantData : public TSharedFromThis<FNavigationRelevantData, ESPMode::ThreadSafe>
{
	DECLARE_DELEGATE_RetVal_OneParam(bool, FFilterNavDataDelegate, const struct FNavDataConfig*);

	/** CollisionData should always start with this struct for validation purposes */
	struct FCollisionDataHeader
	{
		int32 DataSize;

		static bool IsValid(const uint8* RawData, int32 RawDataSize);
	};

	/** exported geometry (used by recast navmesh as FRecastGeometryCache) */
	TNavStatArray<uint8> CollisionData;

	/** cached voxels (used by recast navmesh as FRecastVoxelCache) */
	TNavStatArray<uint8> VoxelData;

	/** bounds of geometry (unreal coords) */
	FBox Bounds;

	/** Gathers per instance data for navigation geometry in a specified area box */
	FNavDataPerInstanceTransformDelegate NavDataPerInstanceTransformDelegate;

	/** called to check if hosted geometry should be used for given FNavDataConfig. If not set then "true" is assumed.
	 *  Might want to set bUseVirtualGeometryFilteringAndDirtying to true in the Navmesh class you are excluding geometry from.
	 *  This will improve cpu performance by stopping the navmesh from dirtying tiles requested by actors being excluded by this delegate.
	 */
	FFilterNavDataDelegate ShouldUseGeometryDelegate;

	/** additional modifiers: areas and external links */
	FCompositeNavModifier Modifiers;

	/** UObject these data represents */
	const TWeakObjectPtr<UObject> SourceObject;

	/** get set to true when lazy navigation exporting is enabled and this navigation data has "potential" of
	*	containing geometry data. First access will result in gathering the data and setting this flag back to false.
	*	Mind that this flag can go back to 'true' if related data gets cleared out. */
	uint32 bPendingLazyGeometryGathering : 1;
	uint32 bPendingLazyModifiersGathering : 1;
	uint32 bPendingChildLazyModifiersGathering : 1;

	uint32 bSupportsGatheringGeometrySlices : 1;

	/** From level loading (only valid in WP dynamic mode) */
	uint32 bLoadedData : 1;

	FNavigationRelevantData(UObject& Source)
		: SourceObject(&Source)
		, bPendingLazyGeometryGathering(false)
		, bPendingLazyModifiersGathering(false)
		, bPendingChildLazyModifiersGathering(false)
		, bSupportsGatheringGeometrySlices(false)
		, bLoadedData(false)
	{}

	FORCEINLINE bool HasGeometry() const { return VoxelData.Num() || CollisionData.Num(); }
	FORCEINLINE bool HasModifiers() const { return !Modifiers.IsEmpty(); }
	FORCEINLINE bool IsPendingLazyGeometryGathering() const { return bPendingLazyGeometryGathering; }
	FORCEINLINE bool IsPendingLazyModifiersGathering() const { return bPendingLazyModifiersGathering; }
	FORCEINLINE bool IsPendingChildLazyModifiersGathering() const { return bPendingChildLazyModifiersGathering; }
	FORCEINLINE bool NeedAnyPendingLazyModifiersGathering() const { return bPendingLazyModifiersGathering || bPendingChildLazyModifiersGathering; }
	FORCEINLINE bool SupportsGatheringGeometrySlices() const { return bSupportsGatheringGeometrySlices; }
	FORCEINLINE bool IsEmpty() const { return !HasGeometry() && !HasModifiers(); }
	FORCEINLINE SIZE_T GetAllocatedSize() const { return CollisionData.GetAllocatedSize() + VoxelData.GetAllocatedSize() + Modifiers.GetAllocatedSize(); }
	FORCEINLINE SIZE_T GetGeometryAllocatedSize() const { return CollisionData.GetAllocatedSize() + VoxelData.GetAllocatedSize(); }
	FORCEINLINE int32 GetDirtyFlag() const
	{
		return ((HasGeometry() || IsPendingLazyGeometryGathering() || Modifiers.GetMaskFillCollisionUnderneathForNavmesh()) ? ENavigationDirtyFlag::Geometry : 0) |
			((HasModifiers() || NeedAnyPendingLazyModifiersGathering()) ? ENavigationDirtyFlag::DynamicModifier : 0) |
			(Modifiers.HasAgentHeightAdjust() ? ENavigationDirtyFlag::UseAgentHeight : 0);
	}

	FORCEINLINE FCompositeNavModifier GetModifierForAgent(const struct FNavAgentProperties* NavAgent = nullptr) const
	{
		return Modifiers.HasMetaAreas() ? Modifiers.GetInstantiatedMetaModifier(NavAgent, SourceObject) : Modifiers;
	}

	bool HasPerInstanceTransforms() const;
	bool IsMatchingFilter(const FNavigationRelevantDataFilter& Filter) const;
	void Shrink();
	bool IsCollisionDataValid() const;

	void ValidateAndShrink()
	{
		if (IsCollisionDataValid())
		{
			Shrink();
		}
		else
		{
			CollisionData.Empty();
		}
	}

	FORCEINLINE UObject* GetOwner() const { return SourceObject.Get(); }
};

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UNavRelevantInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class INavRelevantInterface
{
	GENERATED_IINTERFACE_BODY()

	/** Prepare navigation modifiers */
	virtual void GetNavigationData(FNavigationRelevantData& Data) const {}

	/** Get bounds for navigation octree */
	virtual FBox GetNavigationBounds() const { return FBox(ForceInit); }

	/** if this instance knows how to export sub-sections of self */
	virtual bool SupportsGatheringGeometrySlices() const { return false; }

	/** This function is called "on demand", whenever that specified piece of geometry is needed for navigation generation */
	virtual void GatherGeometrySlice(FNavigableGeometryExport& GeomExport, const FBox& SliceBox) const {}

	virtual ENavDataGatheringMode GetGeometryGatheringMode() const { return ENavDataGatheringMode::Default; }

	/** Called on Game-thread to give implementer a change to perform actions that require game-thread to run, 
	 *	for example precaching physics data */
	virtual void PrepareGeometryExportSync() {}

	/** Update bounds, called after moving owning actor */
	virtual void UpdateNavigationBounds() {}

	/** Are modifiers active? */
	virtual bool IsNavigationRelevant() const { return true; }

	/** Get navigation parent
	 *  Adds modifiers to existing octree node, GetNavigationBounds and IsNavigationRelevant won't be checked
	 */
	virtual UObject* GetNavigationParent() const { return NULL; }
};
