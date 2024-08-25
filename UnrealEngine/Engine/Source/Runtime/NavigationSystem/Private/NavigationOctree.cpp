// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationOctree.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "NavigationSystem.h"
#include "UObject/Package.h"

LLM_DEFINE_TAG(NavigationOctree);


//----------------------------------------------------------------------//
// FNavigationOctree
//----------------------------------------------------------------------//
FNavigationOctree::FNavigationOctree(const FVector& Origin, FVector::FReal Radius)
	: TOctree2<FNavigationOctreeElement, FNavigationOctreeSemantics>(Origin, Radius)
	, DefaultGeometryGatheringMode(ENavDataGatheringMode::Instant)
	, bGatherGeometry(false)
	, NodesMemory(0)
#if !UE_BUILD_SHIPPING	
	, GatheringNavModifiersTimeLimitWarning(-1.0f)
#endif // !UE_BUILD_SHIPPING	
{
	INC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );
}

FNavigationOctree::~FNavigationOctree()
{
	DEC_DWORD_STAT_BY( STAT_NavigationMemory, sizeof(*this) );
	DEC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, NodesMemory);
	
	ObjectToOctreeId.Empty();
}

void FNavigationOctree::SetDataGatheringMode(ENavDataGatheringModeConfig Mode)
{
	check(Mode != ENavDataGatheringModeConfig::Invalid);
	DefaultGeometryGatheringMode = ENavDataGatheringMode(Mode);
}

void FNavigationOctree::SetNavigableGeometryStoringMode(ENavGeometryStoringMode NavGeometryMode)
{
	bGatherGeometry = (NavGeometryMode == FNavigationOctree::StoreNavGeometry);
}

void FNavigationOctree::DemandLazyDataGathering(FNavigationRelevantData& ElementData)
{
	LLM_SCOPE_BYTAG(NavigationOctree);

	UObject* const NavRelevantObject = ElementData.GetOwner();
	INavRelevantInterface* const NavRelevantInterface = Cast<INavRelevantInterface>(NavRelevantObject);
	if (NavRelevantInterface == nullptr)
	{
		return;
	}

	bool bShrink = false;
	const int32 OrgElementMemory = IntCastChecked<int32>(ElementData.GetGeometryAllocatedSize());

	if (ElementData.IsPendingLazyGeometryGathering() == true && ElementData.SupportsGatheringGeometrySlices() == false)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMeshGenerator_LazyGeometryExport);

		NavRelevantGeometryExportDelegate.ExecuteIfBound(*NavRelevantInterface, ElementData);
		bShrink = true;

		// mark this element as no longer needing geometry gathering
		ElementData.bPendingLazyGeometryGathering = false;
	}

	if (ElementData.IsPendingLazyModifiersGathering())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_RecastNavMeshGenerator_LazyModifiersExport);

#if !UE_BUILD_SHIPPING
		const bool bCanOutputDurationWarning = GatheringNavModifiersTimeLimitWarning >= 0.0f;
		const double StartTime = bCanOutputDurationWarning ? FPlatformTime::Seconds() : 0.0f;
#endif //!UE_BUILD_SHIPPING

		NavRelevantInterface->GetNavigationData(ElementData);
		ElementData.bPendingLazyModifiersGathering = false;
		bShrink = true;

#if !UE_BUILD_SHIPPING
		// If GatheringNavModifiersWarningLimitTime is positive, it will print a Warning if the time taken to call GetNavigationData is more than GatheringNavModifiersWarningLimitTime			
		if (bCanOutputDurationWarning)
		{
			const double DeltaTime = FPlatformTime::Seconds() - StartTime;
			if (DeltaTime > GatheringNavModifiersTimeLimitWarning)
			{
				UE_LOG(LogNavigation, Warning, TEXT("The time (%f sec) for gathering navigation data on an INavRelevantInterface navigation element exceeded the time limit (%f sec) | NavElement = %s | NavElement's Owner = %s | Level = %s"),
					DeltaTime,
					GatheringNavModifiersTimeLimitWarning,
					*GetNameSafe(NavRelevantObject),
					*GetNameSafe(NavRelevantObject->GetOuter()),
					*GetNameSafe(NavRelevantObject->GetOutermost()));
			}
		}
#endif //!UE_BUILD_SHIPPING
	}

	if (bShrink)
	{
		// validate exported data
		// shrink arrays before counting memory
		// it will be reallocated when adding to octree and RemoveNode will have different value returned by GetAllocatedSize()
		ElementData.ValidateAndShrink();
	}

	const int32 ElementMemoryChange = IntCastChecked<int32>(ElementData.GetGeometryAllocatedSize()) - OrgElementMemory;
	NodesMemory += ElementMemoryChange;
	INC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, ElementMemoryChange);
}

void FNavigationOctree::DemandChildLazyDataGathering(FNavigationRelevantData& ElementData, INavRelevantInterface& ChildNavInterface)
{
	LLM_SCOPE_BYTAG(NavigationOctree);

	if (IsLazyGathering(ChildNavInterface))
	{
		ChildNavInterface.GetNavigationData(ElementData);
		ElementData.ValidateAndShrink();
	}
}

#if !UE_BUILD_SHIPPING	
void FNavigationOctree::SetGatheringNavModifiersTimeLimitWarning(const float Threshold)
{
	GatheringNavModifiersTimeLimitWarning = Threshold;
}
#endif // !UE_BUILD_SHIPPING	

bool FNavigationOctree::IsLazyGathering(const INavRelevantInterface& ChildNavInterface) const
{
	const ENavDataGatheringMode GatheringMode = ChildNavInterface.GetGeometryGatheringMode();
	const bool bDoInstantGathering = ((GatheringMode == ENavDataGatheringMode::Default && DefaultGeometryGatheringMode == ENavDataGatheringMode::Instant)
		|| GatheringMode == ENavDataGatheringMode::Instant);

	return !bDoInstantGathering;
}

void FNavigationOctree::AddNode(UObject* ElementOb, INavRelevantInterface* NavElement, const FBox& Bounds, FNavigationOctreeElement& Element)
{
	LLM_SCOPE_BYTAG(NavigationOctree);

	if (UNLIKELY(!Bounds.IsValid || Bounds.GetSize().IsNearlyZero())) 
	{
		UE_LOG(LogNavigation, Warning, TEXT("%hs: %s bounds, ignoring %s."), __FUNCTION__, !Bounds.IsValid ? TEXT("Invalid") : TEXT("Empty"), *GetFullNameSafe(ElementOb));
		return;
	}
	
	Element.Bounds = Bounds;

	if (NavElement)
	{
		checkf(ElementOb, TEXT("We assume NavElement is ElementOb already cast"));

		Element.Data->bShouldSkipDirtyAreaOnAddOrRemove = NavElement && NavElement->ShouldSkipDirtyAreaOnAddOrRemove();

		const bool bDoInstantGathering = !IsLazyGathering(*NavElement);

		if (bGatherGeometry)
		{
			bool bIsCompiling = false;
#if WITH_EDITOR
			const IInterface_AsyncCompilation* AsyncCompiledObject = Cast<IInterface_AsyncCompilation>(ElementOb);
			bIsCompiling = AsyncCompiledObject && AsyncCompiledObject->IsCompiling();

			UE_CLOG(bIsCompiling, LogNavigation, Warning, TEXT("%hs: Objects %s should not be considered relevant to navigation until associated asset compilation is completed."),
				__FUNCTION__, *GetFullNameSafe(ElementOb));
#endif

			// Skip custom navigation export during async compilation, the node will be invalidated once
			// compilation finishes.
			if (!bIsCompiling)
			{
				if (bDoInstantGathering)
				{
					NavRelevantGeometryExportDelegate.ExecuteIfBound(*NavElement, *Element.Data);
				}
				else
				{
					Element.Data->bPendingLazyGeometryGathering = true;
					Element.Data->bSupportsGatheringGeometrySlices = NavElement && NavElement->SupportsGatheringGeometrySlices();
				}
			}
		}

		SCOPE_CYCLE_COUNTER(STAT_Navigation_GatheringNavigationModifiersSync);
		if (bDoInstantGathering)
		{
#if !UE_BUILD_SHIPPING
			const bool bCanOutputDurationWarning = GatheringNavModifiersTimeLimitWarning >= 0.0f;
			const double StartTime = bCanOutputDurationWarning ? FPlatformTime::Seconds() : 0.0f;
#endif //!UE_BUILD_SHIPPING

			NavElement->GetNavigationData(*Element.Data);

#if !UE_BUILD_SHIPPING
			// If GatheringNavModifiersWarningLimitTime is positive, it will print a Warning if the time taken to call GetNavigationData is more than GatheringNavModifiersWarningLimitTime			
			if (bCanOutputDurationWarning)
			{
				const double DeltaTime = FPlatformTime::Seconds() - StartTime;
				if (DeltaTime > GatheringNavModifiersTimeLimitWarning)
				{
					UE_LOG(LogNavigation, Warning, TEXT("The time (%f sec) for gathering navigation data on an INavRelevantInterface navigation element exceeded the time limit (%f sec) | NavElement = %s | Element's Owner = %s | Level = %s"),
						DeltaTime,
						GatheringNavModifiersTimeLimitWarning,
						*GetNameSafe(ElementOb),
						*GetNameSafe(ElementOb->GetOuter()),
						*GetNameSafe(ElementOb->GetOutermost()));
				}
			}
#endif //!UE_BUILD_SHIPPING
		}
		else
		{
			Element.Data->bPendingLazyModifiersGathering = true;
		}
	}

	// validate exported data
	// shrink arrays before counting memory
	// it will be reallocated when adding to octree and RemoveNode will have different value returned by GetAllocatedSize()
	Element.ValidateAndShrink();

	const int32 ElementMemory = Element.GetAllocatedSize();
	NodesMemory += ElementMemory;
	INC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, ElementMemory);

	AddElement(Element);
}

void FNavigationOctree::AppendToNode(const FOctreeElementId2& Id, INavRelevantInterface* NavElement, const FBox& Bounds, FNavigationOctreeElement& Element)
{
	LLM_SCOPE_BYTAG(NavigationOctree);

	const FNavigationOctreeElement OrgData = GetElementById(Id);

	Element = OrgData;
	Element.Bounds = Bounds + OrgData.Bounds.GetBox();

	if (NavElement)
	{
		SCOPE_CYCLE_COUNTER(STAT_Navigation_GatheringNavigationModifiersSync);
		const bool bDoInstantGathering = !IsLazyGathering(*NavElement);

		if (bDoInstantGathering)
		{
			NavElement->GetNavigationData(*Element.Data);
		}
		else
		{
			Element.Data->bPendingChildLazyModifiersGathering = true;
		}
	}

	// validate exported data
	// shrink arrays before counting memory
	// it will be reallocated when adding to octree and RemoveNode will have different value returned by GetAllocatedSize()
	Element.ValidateAndShrink();

	const int32 OrgElementMemory = OrgData.GetAllocatedSize();
	const int32 NewElementMemory = Element.GetAllocatedSize();
	const int32 MemoryDelta = NewElementMemory - OrgElementMemory;

	NodesMemory += MemoryDelta;
	INC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, MemoryDelta);

	RemoveElement(Id);
	AddElement(Element);
}

void FNavigationOctree::UpdateNode(const FOctreeElementId2& Id, const FBox& NewBounds)
{
	FNavigationOctreeElement ElementCopy = GetElementById(Id);
	RemoveElement(Id);
	ElementCopy.Bounds = NewBounds;
	AddElement(ElementCopy);
}

void FNavigationOctree::RemoveNode(const FOctreeElementId2& Id)
{
	const FNavigationOctreeElement& Element = GetElementById(Id);
	const int32 ElementMemory = Element.GetAllocatedSize();
	NodesMemory -= ElementMemory;
	DEC_MEMORY_STAT_BY(STAT_Navigation_CollisionTreeMemory, ElementMemory);

	RemoveElement(Id);
}

const FNavigationRelevantData* FNavigationOctree::GetDataForID(const FOctreeElementId2& Id) const
{
	if (Id.IsValidId() == false)
	{
		return nullptr;
	}

	const FNavigationOctreeElement& OctreeElement = GetElementById(Id);

	return &*OctreeElement.Data;
}

void FNavigationOctree::SetElementIdImpl(const uint32 OwnerUniqueId, FOctreeElementId2 Id)
{
	ObjectToOctreeId.Add(OwnerUniqueId, Id);
}

//----------------------------------------------------------------------//
// FNavigationOctreeSemantics
//----------------------------------------------------------------------//
#if NAVSYS_DEBUG
FORCENOINLINE
#endif // NAVSYS_DEBUG
void FNavigationOctreeSemantics::SetElementId(FNavigationOctreeSemantics::FOctree& OctreeOwner, const FNavigationOctreeElement& Element, FOctreeElementId2 Id)
{
	((FNavigationOctree&)OctreeOwner).SetElementIdImpl(Element.OwnerUniqueId, Id);
}
