// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AI/Navigation/NavigationTypes.h"

#if WITH_RECAST
#include "Detour/DetourNavMesh.h"
#include "Detour/DetourNavMeshQuery.h"

#define RECAST_VERY_SMALL_AGENT_RADIUS 0.0f

class UNavigationSystemV1;

// LWC_TODO_AI: make a lot of the float params used in this class FReals. Not until after 5.0!
class NAVIGATIONSYSTEM_API FRecastQueryFilter : public INavigationQueryFilterInterface, public dtQueryFilter
{
public:
	FRecastQueryFilter(bool bIsVirtual = true);
	virtual ~FRecastQueryFilter(){}

	virtual void Reset() override;

	virtual void SetAreaCost(uint8 AreaType, float Cost) override;
	virtual void SetFixedAreaEnteringCost(uint8 AreaType, float Cost) override;
	virtual void SetExcludedArea(uint8 AreaType) override;
	virtual void SetAllAreaCosts(const float* CostArray, const int32 Count) override;
	virtual void GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const override;
	virtual void SetBacktrackingEnabled(const bool bBacktracking) override;
	virtual bool IsBacktrackingEnabled() const override;
	virtual float GetHeuristicScale() const override;
	virtual bool IsEqual(const INavigationQueryFilterInterface* Other) const override;
	virtual void SetIncludeFlags(uint16 Flags) override;
	virtual uint16 GetIncludeFlags() const override;
	virtual void SetExcludeFlags(uint16 Flags) override;
	virtual uint16 GetExcludeFlags() const override;
	virtual FVector GetAdjustedEndLocation(const FVector& EndLocation) const override { return EndLocation; }
	virtual INavigationQueryFilterInterface* CreateCopy() const override;

	const dtQueryFilter* GetAsDetourQueryFilter() const { return this; }

	/** Changes whether the filter will use virtual set of filtering functions (getVirtualCost and passVirtualFilter)
	 *	or the inlined ones (getInlineCost and passInlineFilter) */
	void SetIsVirtual(bool bIsVirtual);

	/** Instruct filter whether it can reopen nodes already on closed list */
	void SetShouldIgnoreClosedNodes(const bool bIgnoreClosed);

	//----------------------------------------------------------------------//
	// @note you might also want to override following functions from dtQueryFilter	
	// virtual bool passVirtualFilter(const dtPolyRef ref, const dtMeshTile* tile, const dtPoly* poly) const;
	// virtual FVector::FReal getVirtualCost(const FVector::FReal* pa, const FVector::FReal* pb, const dtPolyRef prevRef, const dtMeshTile* prevTile, const dtPoly* prevPoly, const dtPolyRef curRef, const dtMeshTile* curTile, const dtPoly* curPoly, const dtPolyRef nextRef, const dtMeshTile* nextTile, const dtPoly* nextPoly) const;
};

struct NAVIGATIONSYSTEM_API FRecastSpeciaLinkFilter : public dtQuerySpecialLinkFilter
{
	FRecastSpeciaLinkFilter(UNavigationSystemV1* NavSystem, const UObject* Owner) : NavSys(NavSystem), SearchOwner(Owner), CachedOwnerOb(nullptr) {}
	virtual bool isLinkAllowed(const int32 UserId) const override;
	virtual void initialize() override;

	UNavigationSystemV1* NavSys;
	FWeakObjectPtr SearchOwner;
	UObject* CachedOwnerOb;
};

#endif	// WITH_RECAST
