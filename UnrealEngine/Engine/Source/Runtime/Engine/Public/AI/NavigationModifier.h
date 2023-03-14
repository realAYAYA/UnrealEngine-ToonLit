// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "CollisionShape.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "AI/Navigation/NavLinkDefinition.h"

class UBrushComponent;
class UPrimitiveComponent;
class UNavAreaBase;

template<typename InElementType> class TNavStatArray;

struct ENGINE_API FNavigationModifier
{
	FNavigationModifier() : bHasMetaAreas(false) {}
	FORCEINLINE bool HasMetaAreas() const { return !!bHasMetaAreas; }

protected:
	/** set to true if any of areas used by this modifier is a meta nav area (UNavArea::IsMetaArea is true)*/
	int32 bHasMetaAreas : 1;
};

namespace ENavigationShapeType
{
	enum Type
	{
		Unknown,
		Cylinder,
		Box,
		Convex,
		InstancedConvex,
	};
}

namespace ENavigationAreaMode
{
	enum Type
	{
		// apply area modifier on all voxels in bounds 
		Apply,
		
		// apply area modifier only on those voxels in bounds that are matching replace area Id
		Replace,

		// apply area modifier on all voxels in bounds, performed during low area prepass (see: ARecastNavMesh.bMarkLowHeightAreas)
		// (ReplaceInLowPass: mark ONLY "low" voxels that will be removed after prepass, ApplyInLowPass: mark all voxels, including "low" ones)
		ApplyInLowPass,

		// apply area modifier only on those voxels in bounds that are matching replace area Id, performed during low area prepass (see: ARecastNavMesh.bMarkLowHeightAreas)
		// (ReplaceInLowPass: mark ONLY "low" voxels that will be removed after prepass, ApplyInLowPass: mark all voxels, including "low" ones)
		ReplaceInLowPass,
	};
}

namespace ENavigationCoordSystem
{
	enum Type
	{
		Unreal,
		Navigation,

		MAX
	};
}

/** Area modifier: cylinder shape */
struct FCylinderNavAreaData
{
	FVector Origin;
	float Radius;
	float Height;
};

/** Area modifier: box shape (AABB) */
struct FBoxNavAreaData
{
	FVector Origin;
	FVector Extent;
};

struct FConvexNavAreaData
{
	TArray<FVector> Points;
	FVector::FReal MinZ;
	FVector::FReal MaxZ;
};

/** Area modifier: base */
struct ENGINE_API FAreaNavModifier : public FNavigationModifier
{
	/** transient value used for navigation modifiers sorting. If < 0 then not set*/
	float Cost;
	float FixedCost;

	FAreaNavModifier();
	FAreaNavModifier(float Radius, float Height, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);
	FAreaNavModifier(const FVector& Extent, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);
	FAreaNavModifier(const FBox& Box, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);
	FAreaNavModifier(const TArray<FVector>& Points, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);
	FAreaNavModifier(const TArray<FVector>& Points, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);
	FAreaNavModifier(const TNavStatArray<FVector>& Points, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);
	UE_DEPRECATED(5.0, "FAreaNavModifier constructor with a UBrushComponent* parameter has been deprecated since it wasn't able to handle concave shapes. Use FCompositeNavModifier::CreateAreaModifiers instead")
	FAreaNavModifier(const UBrushComponent* BrushComponent, const TSubclassOf<UNavAreaBase> AreaClass);

	void InitializePerInstanceConvex(const TNavStatArray<FVector>& Points, const int32 FirstIndex, const int32 LastIndex, const TSubclassOf<UNavAreaBase> AreaClass);
	void InitializeConvex(const TNavStatArray<FVector>& Points, const int32 FirstIndex, const int32 LastIndex, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass);

	FORCEINLINE const FBox& GetBounds() const { return Bounds; }
	FORCEINLINE ENavigationShapeType::Type GetShapeType() const { return ShapeType; }
	FORCEINLINE ENavigationAreaMode::Type GetApplyMode() const { return ApplyMode; }
	FORCEINLINE bool IsLowAreaModifier() const { return bIsLowAreaModifier; }
	FORCEINLINE bool ShouldExpandTopByCellHeight() const { return bExpandTopByCellHeight; }
	FORCEINLINE bool ShouldIncludeAgentHeight() const { return bIncludeAgentHeight; }
	FORCEINLINE void SetExpandTopByCellHeight(bool bExpand) { bExpandTopByCellHeight = bExpand; }
	FORCEINLINE FAreaNavModifier& SetIncludeAgentHeight(bool bInclude) { bIncludeAgentHeight = bInclude; return *this; }
	FORCEINLINE const TSubclassOf<UNavAreaBase> GetAreaClass() const { return TSubclassOf<UNavAreaBase>(AreaClassOb.Get()); }
	FORCEINLINE const TSubclassOf<UNavAreaBase> GetAreaClassToReplace() const { return TSubclassOf<UNavAreaBase>(ReplaceAreaClassOb.Get()); }

	/** navigation area applied by this modifier */
	void SetAreaClass(const TSubclassOf<UNavAreaBase> AreaClass);

	/** operation mode, ReplaceInLowPass will always automatically use UNavArea_LowHeight as ReplaceAreaClass! */
	void SetApplyMode(ENavigationAreaMode::Type InApplyMode);
	
	/** additional class for used by some ApplyModes, setting it will automatically change ApplyMode to keep backwards compatibility! */
	void SetAreaClassToReplace(const TSubclassOf<UNavAreaBase> AreaClass);

	void GetCylinder(FCylinderNavAreaData& Data) const;
	void GetBox(FBoxNavAreaData& Data) const;
	void GetConvex(FConvexNavAreaData& Data) const;
	void GetPerInstanceConvex(const FTransform& InLocalToWorld, FConvexNavAreaData& OutData) const;

protected:
	/** this should take a value of a game specific navigation modifier	*/
	TWeakObjectPtr<UClass> AreaClassOb;
	TWeakObjectPtr<UClass> ReplaceAreaClassOb;
	FBox Bounds;
	
	TArray<FVector> Points;
	TEnumAsByte<ENavigationShapeType::Type> ShapeType;
	TEnumAsByte<ENavigationAreaMode::Type> ApplyMode;

	/** if set, area shape will be extended at the top by one cell height */
	uint8 bExpandTopByCellHeight : 1;

	/** if set, area shape will be extended by agent's height to cover area underneath like regular colliding geometry */
	uint8 bIncludeAgentHeight : 1;

	/** set when this modifier affects low spans in navmesh generation step */
	uint8 bIsLowAreaModifier : 1;

	void Init(const TSubclassOf<UNavAreaBase> InAreaClass);
	/** @param CoordType specifies which coord system the input data is in */
	void SetConvex(const FVector* InPoints, const int32 FirstIndex, const int32 LastIndex, ENavigationCoordSystem::Type CoordType, const FTransform& LocalToWorld);
	void SetPerInstanceConvex(const FVector* InPoints, const int32 InFirstIndex, const int32 InLastIndex);
	void SetBox(const FBox& Box, const FTransform& LocalToWorld);
	
	static void FillConvexNavAreaData(const FVector* InPoints, const int32 InNumPoints, const FTransform& InLocalToWorld, FConvexNavAreaData& OutConvexData, FBox& OutBounds);
};

/**
 *	This modifier allows defining ad-hoc navigation links defining 
 *	connections in an straightforward way.
 */
struct ENGINE_API FSimpleLinkNavModifier : public FNavigationModifier
{
	/** use Set/Append/Add function to update links, they will take care of meta areas */
	TArray<FNavigationLink> Links;
	TArray<FNavigationSegmentLink> SegmentLinks;
	FTransform LocalToWorld;
	int32 UserId;

	FSimpleLinkNavModifier() 
		: bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
	}

	FSimpleLinkNavModifier(const FNavigationLink& InLink, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		UserId = InLink.UserId;
		AddLink(InLink);
	}	

	FSimpleLinkNavModifier(const TArray<FNavigationLink>& InLinks, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		if (InLinks.Num() > 0)
		{
			UserId = InLinks[0].UserId;
			SetLinks(InLinks);
		}
	}

	FSimpleLinkNavModifier(const FNavigationSegmentLink& InLink, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		UserId = InLink.UserId;
		AddSegmentLink(InLink);
	}	

	FSimpleLinkNavModifier(const TArray<FNavigationSegmentLink>& InSegmentLinks, const FTransform& InLocalToWorld) 
		: LocalToWorld(InLocalToWorld)
		, bHasFallDownLinks(false)
		, bHasMetaAreasPoint(false)
		, bHasMetaAreasSegment(false)
	{
		if (InSegmentLinks.Num() > 0)
		{
			UserId = InSegmentLinks[0].UserId;
			SetSegmentLinks(InSegmentLinks);
		}
	}

	FORCEINLINE bool HasFallDownLinks() const { return !!bHasFallDownLinks; }
	void SetLinks(const TArray<FNavigationLink>& InLinks);
	void SetSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks);
	void AppendLinks(const TArray<FNavigationLink>& InLinks);
	void AppendSegmentLinks(const TArray<FNavigationSegmentLink>& InLinks);
	void AddLink(const FNavigationLink& InLink);
	void AddSegmentLink(const FNavigationSegmentLink& InLink);
	void UpdateFlags();

protected:
	/** set to true if any of links stored is a "fall down" link, i.e. requires vertical snapping to geometry */
	int32 bHasFallDownLinks : 1;
	int32 bHasMetaAreasPoint : 1;
	int32 bHasMetaAreasSegment : 1;
};

struct ENGINE_API FCustomLinkNavModifier : public FNavigationModifier
{
	FTransform LocalToWorld;

	void Set(TSubclassOf<UNavLinkDefinition> LinkDefinitionClass, const FTransform& InLocalToWorld);
	FORCEINLINE const TSubclassOf<UNavLinkDefinition> GetNavLinkClass() const { return TSubclassOf<UNavLinkDefinition>(LinkDefinitionClassOb.Get()); }

protected:
	TWeakObjectPtr<UClass> LinkDefinitionClassOb;
};

struct ENGINE_API FCompositeNavModifier : public FNavigationModifier
{
	FCompositeNavModifier() 
		: bHasPotentialLinks(false)
		, bAdjustHeight(false)
		, bHasLowAreaModifiers(false)
		, bIsPerInstanceModifier(false)
		, bFillCollisionUnderneathForNavmesh(false)
		, bMaskFillCollisionUnderneathForNavmesh(false)
	{}

	void Shrink();
	void Reset();
	void Empty();

	FORCEINLINE bool IsEmpty() const 
	{ 
		return (Areas.Num() == 0) && (SimpleLinks.Num() == 0) && (CustomLinks.Num() == 0) && 
			!bFillCollisionUnderneathForNavmesh && 
			!bMaskFillCollisionUnderneathForNavmesh;
	}

	void Add(const FAreaNavModifier& Area)
	{
		Areas.Add(Area);
		bHasMetaAreas |= Area.HasMetaAreas(); 
		bAdjustHeight |= Area.ShouldIncludeAgentHeight();
		bHasLowAreaModifiers |= Area.IsLowAreaModifier();
	}

	void Add(const FSimpleLinkNavModifier& Link)
	{
		SimpleLinks.Add(Link);
		bHasMetaAreas |= Link.HasMetaAreas(); 
	}

	void Add(const FCustomLinkNavModifier& Link)
	{
		CustomLinks.Add(Link); 
		bHasMetaAreas |= Link.HasMetaAreas(); 
	}

	void Add(const FCompositeNavModifier& Modifiers)
	{
		Areas.Append(Modifiers.Areas); 
		SimpleLinks.Append(Modifiers.SimpleLinks); 
		CustomLinks.Append(Modifiers.CustomLinks); 
		bHasMetaAreas |= Modifiers.bHasMetaAreas; 
		bAdjustHeight |= Modifiers.HasAgentHeightAdjust();
		bHasLowAreaModifiers |= Modifiers.HasLowAreaModifiers();
		bFillCollisionUnderneathForNavmesh |= Modifiers.GetFillCollisionUnderneathForNavmesh();
		bMaskFillCollisionUnderneathForNavmesh |= Modifiers.GetMaskFillCollisionUnderneathForNavmesh();
	}

	void CreateAreaModifiers(const UPrimitiveComponent* PrimComp, const TSubclassOf<UNavAreaBase> AreaClass);
	void CreateAreaModifiers(const FCollisionShape& CollisionShape, const FTransform& LocalToWorld, const TSubclassOf<UNavAreaBase> AreaClass, const bool bIncludeAgentHeight = false);

	FORCEINLINE const TArray<FAreaNavModifier>& GetAreas() const { return Areas; }
	FORCEINLINE const TArray<FSimpleLinkNavModifier>& GetSimpleLinks() const { return SimpleLinks; }
	FORCEINLINE const TArray<FCustomLinkNavModifier>& GetCustomLinks() const { return CustomLinks; }
	
	FORCEINLINE bool HasLinks() const { return (SimpleLinks.Num() > 0) || (CustomLinks.Num() > 0); }
	FORCEINLINE bool HasPotentialLinks() const { return bHasPotentialLinks; }
	FORCEINLINE bool HasAgentHeightAdjust() const { return bAdjustHeight; }
	FORCEINLINE bool HasAreas() const { return Areas.Num() > 0; }
	FORCEINLINE bool HasLowAreaModifiers() const { return bHasLowAreaModifiers; }
	FORCEINLINE bool IsPerInstanceModifier() const { return bIsPerInstanceModifier; }
	FORCEINLINE bool GetFillCollisionUnderneathForNavmesh() const { return bFillCollisionUnderneathForNavmesh; }
	FORCEINLINE void SetFillCollisionUnderneathForNavmesh(bool bValue) { bFillCollisionUnderneathForNavmesh = bValue; }
	FORCEINLINE bool GetMaskFillCollisionUnderneathForNavmesh() const { return bMaskFillCollisionUnderneathForNavmesh; }
	FORCEINLINE void SetMaskFillCollisionUnderneathForNavmesh(bool bValue) { bMaskFillCollisionUnderneathForNavmesh = bValue; }
	FORCEINLINE void ReserveForAdditionalAreas(int32 AdditionalElementsCount) { Areas.Reserve(Areas.Num() + AdditionalElementsCount); }

	void MarkPotentialLinks() { bHasPotentialLinks = true; }
    void MarkAsPerInstanceModifier() { bIsPerInstanceModifier = true; }

	/** returns a copy of Modifier */
	FCompositeNavModifier GetInstantiatedMetaModifier(const struct FNavAgentProperties* NavAgent, TWeakObjectPtr<UObject> WeakOwnerPtr) const;
	uint32 GetAllocatedSize() const;

	UE_DEPRECATED(4.24, "This method will be removed in future versions. Use FNavigationRelevantData::HasPerInstanceTransforms instead.")
	bool HasPerInstanceTransforms() const;

	TArray<FAreaNavModifier>& GetMutableAreas() { return Areas; }
	TArray<FSimpleLinkNavModifier>& GetSimpleLinks() { return SimpleLinks; }
	TArray<FCustomLinkNavModifier>& GetCustomLinks() { return CustomLinks; }

public:

	// This property is deprecated and will be removed in future versions. Use FNavigationRelevantData NavDataPerInstanceTransformDelegate instead.
	// Gathers per instance data for navigation area modifiers in a specified area box
	FNavDataPerInstanceTransformDelegate NavDataPerInstanceTransformDelegate;

private:
	TArray<FAreaNavModifier> Areas;
	TArray<FSimpleLinkNavModifier> SimpleLinks;
	TArray<FCustomLinkNavModifier> CustomLinks;
	uint32 bHasPotentialLinks : 1;
	uint32 bAdjustHeight : 1;
	uint32 bHasLowAreaModifiers : 1;
    uint32 bIsPerInstanceModifier : 1;
	uint32 bFillCollisionUnderneathForNavmesh : 1;
	uint32 bMaskFillCollisionUnderneathForNavmesh : 1;
};
