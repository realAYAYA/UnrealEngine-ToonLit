// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavigationDataHandler.h"
#include "Engine/Level.h"
#include "NavMesh/RecastNavMeshGenerator.h"
#include "NavAreas/NavArea.h"

DEFINE_LOG_CATEGORY_STATIC(LogNavOctree, Warning, All);

namespace UE::NavigationHelper::Private
{
	int32 GetDirtyFlag(int32 UpdateFlags, int32 DefaultValue)
	{
		return ((UpdateFlags & FNavigationOctreeController::OctreeUpdate_Geometry) != 0) ? ENavigationDirtyFlag::All :
			((UpdateFlags & FNavigationOctreeController::OctreeUpdate_Modifiers) != 0) ? ENavigationDirtyFlag::DynamicModifier :
			DefaultValue;
	}

	FString GetElementName(const UObject& ElementOwner)
	{
		if (const UActorComponent* ActorComp = Cast<UActorComponent>(&ElementOwner))
		{
			return FString::Format(TEXT("Comp {0} of Actor {1}"), {*GetNameSafe(&ElementOwner), *GetNameSafe(ActorComp->GetOwner())});
		}
		return *GetNameSafe(&ElementOwner);
	}
}
	
FNavigationDataHandler::FNavigationDataHandler(FNavigationOctreeController& InOctreeController, FNavigationDirtyAreasController& InDirtyAreasController)
		: OctreeController(InOctreeController), DirtyAreasController(InDirtyAreasController)
{}

void FNavigationDataHandler::ConstructNavOctree(const FVector& Origin, const float Radius, const ENavDataGatheringModeConfig DataGatheringMode, const float GatheringNavModifiersWarningLimitTime)
{
	UE_LOG(LogNavOctree, Log, TEXT("CREATE (Origin:%s Radius:%.2f)"), *Origin.ToString(), Radius);

	OctreeController.Reset();
	OctreeController.NavOctree = MakeShareable(new FNavigationOctree(Origin, Radius));
	OctreeController.NavOctree->SetDataGatheringMode(DataGatheringMode);
#if !UE_BUILD_SHIPPING
	OctreeController.NavOctree->SetGatheringNavModifiersTimeLimitWarning(GatheringNavModifiersWarningLimitTime);
#endif // !UE_BUILD_SHIPPING
}

void FNavigationDataHandler::RemoveNavOctreeElementId(const FOctreeElementId2& ElementId, int32 UpdateFlags)
{
	if (ensure(OctreeController.IsValidElement(ElementId)))
	{
		const FNavigationOctreeElement& ElementData = OctreeController.NavOctree->GetElementById(ElementId);
		// mark area occupied by given element as dirty except if explicitly set to skip this default behavior
		if (!ElementData.Data->bShouldSkipDirtyAreaOnAddOrRemove)
		{
			const int32 DirtyFlag = UE::NavigationHelper::Private::GetDirtyFlag(UpdateFlags, ElementData.Data->GetDirtyFlag());
			DirtyAreasController.AddArea(ElementData.Bounds.GetBox(), DirtyFlag, [&ElementData] { return ElementData.Data->SourceObject.Get(); }, nullptr, "Remove from navoctree");
		}

		OctreeController.NavOctree->RemoveNode(ElementId);
	}
}

FSetElementId FNavigationDataHandler::RegisterNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_RegisterNavOctreeElement);

	FSetElementId SetId;

	if (OctreeController.NavOctree.IsValid() == false)
	{
		return SetId;
	}

	if (OctreeController.IsNavigationOctreeLocked())
	{
		UE_LOG(LogNavOctree, Log, TEXT("IGNORE(RegisterNavOctreeElement) %s"), *GetPathNameSafe(&ElementOwner));
		return SetId;
	}

	const bool bIsRelevant = ElementInterface.IsNavigationRelevant();
	UE_LOG(LogNavOctree, Log, TEXT("REG %s %s"), *UE::NavigationHelper::Private::GetElementName(ElementOwner), bIsRelevant ? TEXT("[relevant]") : TEXT("[skip]"));

	if (bIsRelevant)
	{
		bool bCanAdd = false;

		UObject* ParentNode = ElementInterface.GetNavigationParent();
		if (ParentNode)
		{
			OctreeController.OctreeChildNodesMap.AddUnique(ParentNode, FWeakObjectPtr(&ElementOwner));
			bCanAdd = true;
		}
		else
		{
			bCanAdd = (OctreeController.HasObjectsNavOctreeId(ElementOwner) == false);
		}

		if (bCanAdd)
		{
			FNavigationDirtyElement UpdateInfo(&ElementOwner, &ElementInterface, UE::NavigationHelper::Private::GetDirtyFlag(UpdateFlags, 0), DirtyAreasController.bUseWorldPartitionedDynamicMode);

			SetId = OctreeController.PendingOctreeUpdates.FindId(UpdateInfo);
			if (SetId.IsValidId())
			{
				// make sure this request stays, in case it has been invalidated already and keep any dirty areas
				UpdateInfo.ExplicitAreasToDirty = OctreeController.PendingOctreeUpdates[SetId].ExplicitAreasToDirty;
				OctreeController.PendingOctreeUpdates[SetId] = UpdateInfo;
			}
			else
			{
				SetId = OctreeController.PendingOctreeUpdates.Add(UpdateInfo);
			}
		}
	}

	return SetId;
}

void FNavigationDataHandler::AddElementToNavOctree(const FNavigationDirtyElement& DirtyElement)
{
	check(OctreeController.NavOctree.IsValid());
	LLM_SCOPE_BYTAG(NavigationOctree);

	// handle invalidated requests first
	if (DirtyElement.bInvalidRequest)
	{
		if (DirtyElement.bHasPrevData)
		{
			DirtyAreasController.AddArea(DirtyElement.PrevBounds, DirtyElement.PrevFlags, [&DirtyElement] { return DirtyElement.Owner.Get(); }, &DirtyElement, "Addition to navoctree (invalid request)");
		}

		return;
	}

	UObject* ElementOwner = DirtyElement.Owner.Get();
	if (!IsValid(ElementOwner) || DirtyElement.NavInterface == nullptr)
	{
		return;
	}

	FNavigationOctreeElement GeneratedData(*ElementOwner);

	// In WP dynamic mode, store if this is loaded data.
	if (DirtyAreasController.bUseWorldPartitionedDynamicMode)
	{
		GeneratedData.Data->bLoadedData = DirtyElement.bIsFromVisibilityChange || FNavigationSystem::IsLevelVisibilityChanging(ElementOwner);
	}
	
	const FBox ElementBounds = DirtyElement.NavInterface->GetNavigationBounds();

	UObject* NavigationParent = DirtyElement.NavInterface->GetNavigationParent();
	if (NavigationParent)
	{
		// check if parent node is waiting in queue
		const FSetElementId ParentRequestId = OctreeController.PendingOctreeUpdates.FindId(FNavigationDirtyElement(NavigationParent));
		const FOctreeElementId2* ParentId = OctreeController.GetObjectsNavOctreeId(*NavigationParent);
		if (ParentRequestId.IsValidId() && ParentId == nullptr)
		{
			FNavigationDirtyElement& ParentNode = OctreeController.PendingOctreeUpdates[ParentRequestId];
			AddElementToNavOctree(ParentNode);

			// mark as invalid so it won't be processed twice
			ParentNode.bInvalidRequest = true;
		}

		const FOctreeElementId2* ElementId = ParentId ? ParentId : OctreeController.GetObjectsNavOctreeId(*NavigationParent);
		if (ElementId && ensure(OctreeController.IsValidElement(*ElementId)))
		{
			UE_LOG(LogNavOctree, Log, TEXT("ADD %s to %s"), *UE::NavigationHelper::Private::GetElementName(*ElementOwner), *GetNameSafe(NavigationParent));
			OctreeController.NavOctree->AppendToNode(*ElementId, DirtyElement.NavInterface, ElementBounds, GeneratedData);
		}
		else
		{
			UE_LOG(LogNavOctree, Warning, TEXT("Can't add node [%s] - parent [%s] not found in octree!"), *UE::NavigationHelper::Private::GetElementName(*ElementOwner), *UE::NavigationHelper::Private::GetElementName(*NavigationParent));
		}
	}
	else
	{
		UE_LOG(LogNavOctree, Log, TEXT("ADD %s"), *UE::NavigationHelper::Private::GetElementName(*ElementOwner));
		OctreeController.NavOctree->AddNode(ElementOwner, DirtyElement.NavInterface, ElementBounds, GeneratedData);
	}

	// mark area occupied by given element as dirty except if explicitly set to skip this default behavior
	const int32 DirtyFlag = DirtyElement.FlagsOverride ? DirtyElement.FlagsOverride : GeneratedData.Data->GetDirtyFlag();
	if (GeneratedData.Data->bShouldSkipDirtyAreaOnAddOrRemove)
	{
		if (DirtyElement.ExplicitAreasToDirty.Num() > 0)
		{
			DirtyAreasController.AddAreas(DirtyElement.ExplicitAreasToDirty, DirtyFlag, [&ElementOwner] { return ElementOwner; }, &DirtyElement, "Addition to navoctree");
		}
	}
	else if (!GeneratedData.IsEmpty())
	{
		DirtyAreasController.AddArea(GeneratedData.Bounds.GetBox(), DirtyFlag, [&ElementOwner] { return ElementOwner; }, &DirtyElement, "Addition to navoctree");
	}
}

bool FNavigationDataHandler::UnregisterNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags)
{
	SCOPE_CYCLE_COUNTER(STAT_Navigation_UnregisterNavOctreeElement);

	if (OctreeController.NavOctree.IsValid() == false)
	{
		return false;
	}

	if (OctreeController.IsNavigationOctreeLocked())
	{
#if !WITH_EDITOR
		UE_LOG(LogNavOctree, Log, TEXT("IGNORE(UnregisterNavOctreeElement) %s"), *GetPathNameSafe(&ElementOwner));
#endif // WITH_EDITOR
		return false;
	}

	bool bUnregistered = false;
	const FOctreeElementId2* ElementId = OctreeController.GetObjectsNavOctreeId(ElementOwner);
	UE_LOG(LogNavOctree, Log, TEXT("UNREG %s %s"), *UE::NavigationHelper::Private::GetElementName(ElementOwner), ElementId ? TEXT("[exists]") : TEXT("[doesn\'t exist]"));

	if (ElementId != nullptr)
	{
		RemoveNavOctreeElementId(*ElementId, UpdateFlags);
		OctreeController.RemoveObjectsNavOctreeId(ElementOwner);
		bUnregistered = true;
	}
	else
	{
		const bool bCanRemoveChildNode = (UpdateFlags & FNavigationOctreeController::OctreeUpdate_ParentChain) == 0;
		UObject* ParentNode = ElementInterface.GetNavigationParent();
		if (ParentNode && bCanRemoveChildNode)
		{
			// if node has navigation parent (= doesn't exists in octree on its own)
			// and it's not part of parent chain update
			// remove it from map and force update on parent to rebuild octree element

			OctreeController.OctreeChildNodesMap.RemoveSingle(ParentNode, FWeakObjectPtr(&ElementOwner));
			UpdateNavOctreeParentChain(*ParentNode);
		}
	}

	// mark pending update as invalid, it will be dirtied according to currently active settings
	const bool bCanInvalidateQueue = (UpdateFlags & FNavigationOctreeController::OctreeUpdate_Refresh) == 0;
	if (bCanInvalidateQueue)
	{
		const FSetElementId RequestId = OctreeController.PendingOctreeUpdates.FindId(FNavigationDirtyElement(&ElementOwner));
		if (RequestId.IsValidId())
		{
			FNavigationDirtyElement& DirtyElement = OctreeController.PendingOctreeUpdates[RequestId];

			// Only consider as unregistered when pending update was not already invalidated since return value must indicate
			// that ElementOwner was fully added or about to be added (valid pending update).
			bUnregistered |= (DirtyElement.bInvalidRequest == false);

			DirtyElement.bInvalidRequest = true;
		}
	}

	return bUnregistered;
}

void FNavigationDataHandler::UpdateNavOctreeElement(UObject& ElementOwner, INavRelevantInterface& ElementInterface, int32 UpdateFlags)
{
	INC_DWORD_STAT(STAT_Navigation_UpdateNavOctree);

	if (OctreeController.IsNavigationOctreeLocked())
	{
		UE_LOG(LogNavOctree, Log, TEXT("IGNORE(UpdateNavOctreeElement) %s"), *ElementOwner.GetPathName());
		return;
	}

	// grab existing octree data
	FBox CurrentBounds;
	int32 CurrentFlags;
	const bool bAlreadyExists = OctreeController.GetNavOctreeElementData(ElementOwner, CurrentFlags, CurrentBounds);

	// don't invalidate pending requests
	UpdateFlags |= FNavigationOctreeController::OctreeUpdate_Refresh;

	// always try to unregister, even if element owner doesn't exists in octree (parent nodes)
	UnregisterNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);

	const FSetElementId RequestId = RegisterNavOctreeElement(ElementOwner, ElementInterface, UpdateFlags);

	// add original data to pending registration request
	// so it could be dirtied properly when system receive unregister request while actor is still queued
	if (RequestId.IsValidId())
	{
		FNavigationDirtyElement& UpdateInfo = OctreeController.PendingOctreeUpdates[RequestId];
		UpdateInfo.PrevFlags = CurrentFlags;
		if (UpdateInfo.PrevBounds.IsValid)
		{
			// Is we have something stored already we want to 
			// sum it up, since we care about the whole bounding
			// box of changes that potentially took place
			UpdateInfo.PrevBounds += CurrentBounds;
		}
		else
		{
			UpdateInfo.PrevBounds = CurrentBounds;
		}
		UpdateInfo.bHasPrevData = bAlreadyExists;
	}

	UpdateNavOctreeParentChain(ElementOwner, /*bSkipElementOwnerUpdate=*/ true);
}

void FNavigationDataHandler::UpdateNavOctreeParentChain(UObject& ElementOwner, bool bSkipElementOwnerUpdate)
{
	const int32 UpdateFlags = FNavigationOctreeController::OctreeUpdate_ParentChain | FNavigationOctreeController::OctreeUpdate_Refresh;

	TArray<FWeakObjectPtr> ChildNodes;
	OctreeController.OctreeChildNodesMap.MultiFind(&ElementOwner, ChildNodes);

	auto ElementOwnerUpdateFunc = [&]()->bool
	{
		bool bShouldRegisterChildren = true;
		if (bSkipElementOwnerUpdate == false)
		{
			INavRelevantInterface* ElementInterface = Cast<INavRelevantInterface>(&ElementOwner);
			if (ElementInterface != nullptr)
			{
				// We don't want to register NavOctreeElement if owner was not already registered or queued
				// so we use Unregister/Register combo instead of UpdateNavOctreeElement
				if (UnregisterNavOctreeElement(ElementOwner, *ElementInterface, UpdateFlags))
				{
					FSetElementId NewId = RegisterNavOctreeElement(ElementOwner, *ElementInterface, UpdateFlags);
					bShouldRegisterChildren = NewId.IsValidId();
				}
				else
				{
					bShouldRegisterChildren = false;
				}
			}
		}
		return bShouldRegisterChildren;		
	};

	if (ChildNodes.Num() == 0)
	{
		// Last child was removed, only need to rebuild owner's NavOctreeElement 
		ElementOwnerUpdateFunc();
		return;
	}

	TArray<INavRelevantInterface*> ChildNavInterfaces;
	ChildNavInterfaces.AddZeroed(ChildNodes.Num());

	for (int32 Idx = 0; Idx < ChildNodes.Num(); Idx++)
	{
		if (ChildNodes[Idx].IsValid())
		{
			UObject* ChildNodeOb = ChildNodes[Idx].Get();
			ChildNavInterfaces[Idx] = Cast<INavRelevantInterface>(ChildNodeOb);
			if (ChildNodeOb && ChildNavInterfaces[Idx])
			{
				UnregisterNavOctreeElement(*ChildNodeOb, *ChildNavInterfaces[Idx], UpdateFlags);
			}
		}
	}

	const bool bShouldRegisterChildren = ElementOwnerUpdateFunc();
	
	if (bShouldRegisterChildren)
	{
		for (int32 Idx = 0; Idx < ChildNodes.Num(); Idx++)
		{
			UObject* ChildElement = ChildNodes[Idx].Get();
			if (ChildElement && ChildNavInterfaces[Idx])
			{
				RegisterNavOctreeElement(*ChildElement, *ChildNavInterfaces[Idx], UpdateFlags);
			}
		}
	}
}

bool FNavigationDataHandler::UpdateNavOctreeElementBounds(UObject& Object, const FBox& NewBounds, const TConstArrayView<FBox> DirtyAreas)
{
	const FOctreeElementId2* ElementId = OctreeController.GetObjectsNavOctreeId(Object);
	if (ElementId != nullptr && ensure(OctreeController.IsValidElement(*ElementId)))
	{
		OctreeController.NavOctree->UpdateNode(*ElementId, NewBounds);

		// Dirty areas
		if (DirtyAreas.Num() > 0)
		{
			// Refresh ElementId since object may be stored in a different node after updating bounds
			ElementId = OctreeController.GetObjectsNavOctreeId(Object);
			if (ElementId != nullptr && ensure(OctreeController.IsValidElement(*ElementId)))
			{
				const FNavigationOctreeElement& ElementData = OctreeController.NavOctree->GetElementById(*ElementId);
				DirtyAreasController.AddAreas(DirtyAreas, ElementData.Data->GetDirtyFlag(), [&Object] { return &Object; }, nullptr, "Bounds change");
			}
		}

		return true;
	}

	// If dirty areas are provided we need to append them to a pending update since the object is not added yet.
	// Not necessary for the bounds since they are not stored in the update but fetched when the update is processed.
	if (DirtyAreas.Num() > 0)
	{
		const FSetElementId PendingElementId = OctreeController.PendingOctreeUpdates.FindId(FNavigationDirtyElement(&Object));
		if (PendingElementId.IsValidId())
		{
			FNavigationDirtyElement& DirtyElement = OctreeController.PendingOctreeUpdates[PendingElementId];
			if (!DirtyElement.bInvalidRequest)
			{
				DirtyElement.ExplicitAreasToDirty.Append(DirtyAreas);
				return true;
			}
		}
	}

	return false;
}

bool FNavigationDataHandler::UpdateNavOctreeElementBounds(UObject& Object, const FBox& NewBounds, const FBox& DirtyArea)
{
	return UpdateNavOctreeElementBounds(Object, NewBounds,  TConstArrayView<FBox>{DirtyArea});
}
	
void FNavigationDataHandler::FindElementsInNavOctree(const FBox& QueryBox, const FNavigationOctreeFilter& Filter, TArray<FNavigationOctreeElement>& Elements)
{
	if (OctreeController.NavOctree.IsValid() == false)
	{
		UE_LOG(LogNavOctree, Warning, TEXT("FNavigationDataHandler::FindElementsInNavOctree gets called while NavOctree is null"));
		return;
	}

	OctreeController.NavOctree->FindElementsWithBoundsTest(QueryBox, [&Elements, &Filter](const FNavigationOctreeElement& Element)
	{
		if (Element.IsMatchingFilter(Filter))
		{
			Elements.Add(Element);
		}
	});
}

bool FNavigationDataHandler::ReplaceAreaInOctreeData(const UObject& Object, TSubclassOf<UNavArea> OldArea, TSubclassOf<UNavArea> NewArea, bool bReplaceChildClasses)
{
	FNavigationRelevantData* Data = OctreeController.GetMutableDataForObject(Object);

	if (Data == nullptr || Data->HasModifiers() == false)
	{
		return false;
	}

	for (FAreaNavModifier& AreaModifier : Data->Modifiers.GetMutableAreas())
	{
		if (AreaModifier.GetAreaClass() == OldArea
			|| (bReplaceChildClasses && AreaModifier.GetAreaClass()->IsChildOf(OldArea)))
		{
			AreaModifier.SetAreaClass(NewArea);
		}
	}
	for (FSimpleLinkNavModifier& SimpleLink : Data->Modifiers.GetSimpleLinks())
	{
		for (FNavigationLink& Link : SimpleLink.Links)
		{
			if (Link.GetAreaClass() == OldArea
				|| (bReplaceChildClasses && Link.GetAreaClass()->IsChildOf(OldArea)))
			{
				Link.SetAreaClass(NewArea);
			}
		}
		for (FNavigationSegmentLink& Link : SimpleLink.SegmentLinks)
		{
			if (Link.GetAreaClass() == OldArea
				|| (bReplaceChildClasses && Link.GetAreaClass()->IsChildOf(OldArea)))
			{
				Link.SetAreaClass(NewArea);
			}
		}
	}
	for (FCustomLinkNavModifier& CustomLink : Data->Modifiers.GetCustomLinks())
	{
		ensureMsgf(false, TEXT("Not implemented yet"));
	}

	return true;
}

void FNavigationDataHandler::AddLevelCollisionToOctree(ULevel& Level)
{
#if WITH_RECAST
	if (OctreeController.NavOctree.IsValid() &&
		OctreeController.NavOctree->GetNavGeometryStoringMode() == FNavigationOctree::StoreNavGeometry)
	{
		const TArray<FVector>* LevelGeom = Level.GetStaticNavigableGeometry();
		const FOctreeElementId2* ElementId = OctreeController.GetObjectsNavOctreeId(Level);

		if (!ElementId && LevelGeom && LevelGeom->Num() > 0)
		{
			FNavigationOctreeElement BSPElem(Level);
			
			// In WP dynamic mode, store if this is loaded data.
			if (DirtyAreasController.bUseWorldPartitionedDynamicMode)
			{
				BSPElem.Data->bLoadedData = Level.HasVisibilityChangeRequestPending();
			}
			
			FRecastNavMeshGenerator::ExportVertexSoupGeometry(*LevelGeom, *BSPElem.Data);

			const FBox& Bounds = BSPElem.Data->Bounds;
			if (!Bounds.GetExtent().IsNearlyZero())
			{
				OctreeController.NavOctree->AddNode(&Level, nullptr, Bounds, BSPElem);
				DirtyAreasController.AddArea(Bounds, ENavigationDirtyFlag::All, [&Level] { return &Level; }, nullptr, "Add level");

				UE_LOG(LogNavOctree, Log, TEXT("ADD %s"), *UE::NavigationHelper::Private::GetElementName(Level));
			}
		}
	}
#endif// WITH_RECAST
}

void FNavigationDataHandler::RemoveLevelCollisionFromOctree(ULevel& Level)
{
	if (OctreeController.NavOctree.IsValid())
	{
		const FOctreeElementId2* ElementId = OctreeController.GetObjectsNavOctreeId(Level);
		UE_LOG(LogNavOctree, Log, TEXT("UNREG %s %s"), *UE::NavigationHelper::Private::GetElementName(Level), ElementId ? TEXT("[exists]") : TEXT(""));

		if (ElementId != nullptr)
		{
			if (ensure(OctreeController.IsValidElement(*ElementId)))
			{
				// mark area occupied by given actor as dirty
				const FNavigationOctreeElement& ElementData = OctreeController.NavOctree->GetElementById(*ElementId);
				DirtyAreasController.AddArea(ElementData.Bounds.GetBox(), ENavigationDirtyFlag::All, [&Level] { return &Level; }, nullptr, "Remove level");
			}

			OctreeController.NavOctree->RemoveNode(*ElementId);
			OctreeController.RemoveObjectsNavOctreeId(Level);
		}
	}
}

void FNavigationDataHandler::UpdateActorAndComponentsInNavOctree(AActor& Actor)
{
	INavRelevantInterface* NavElement = Cast<INavRelevantInterface>(&Actor);
	if (NavElement)
	{
		UpdateNavOctreeElement(Actor, *NavElement, FNavigationOctreeController::OctreeUpdate_Default);
	}

	for (UActorComponent* Component : Actor.GetComponents())
	{
		INavRelevantInterface* CompNavElement = Cast<INavRelevantInterface>(Component);
		if (CompNavElement)
		{
			// Component != null is implied by successful INavRelevantInterface cast 
			if (Actor.IsComponentRelevantForNavigation(Component))
			{
				UpdateNavOctreeElement(*Component, *CompNavElement, FNavigationOctreeController::OctreeUpdate_Default);
			}
			else
			{
				UnregisterNavOctreeElement(*Component, *CompNavElement, FNavigationOctreeController::OctreeUpdate_Default);
			}
		}
	}
}

void FNavigationDataHandler::ProcessPendingOctreeUpdates()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_Navigation_ProcessPendingOctreeUpdates);

	if (OctreeController.NavOctree)
	{
		// AddElementToNavOctree (through some of its resulting function calls) modifies PendingOctreeUpdates so invalidates the iterators,
		// (via WaitUntilAsyncPropertyReleased() / UpdateComponentInNavOctree() / RegisterNavOctreeElement()). This means we can't iterate
		// through this set in the normal way. Previously the code iterated through this which also left us open to other potential bugs
		// in that we may have tried to modify elements we had already processed.
		while (TSet<FNavigationDirtyElement>::TIterator It = OctreeController.PendingOctreeUpdates.CreateIterator())
		{
			FNavigationDirtyElement Element = *It;
			It.RemoveCurrent();
			AddElementToNavOctree(Element);
		}
	}
	OctreeController.PendingOctreeUpdates.Empty(32);
}

void FNavigationDataHandler::DemandLazyDataGathering(FNavigationRelevantData& ElementData)
{
	// Do the lazy gathering on the element
	OctreeController.NavOctree->DemandLazyDataGathering(ElementData);

    // Check if any child asked for some lazy gathering
	if (ElementData.IsPendingChildLazyModifiersGathering())
	{
		TArray<FWeakObjectPtr> ChildNodes;
		OctreeController.OctreeChildNodesMap.MultiFind(ElementData.GetOwner(), ChildNodes);

		for (FWeakObjectPtr& ChildNode : ChildNodes)
		{
			if (ChildNode.IsValid())
			{
				UObject* ChildNodeOb = ChildNode.Get();
				INavRelevantInterface* ChildNavInterface = ChildNodeOb ? Cast<INavRelevantInterface>(ChildNodeOb) : nullptr;
				if (ChildNavInterface)
				{
					OctreeController.NavOctree->DemandChildLazyDataGathering(ElementData, *ChildNavInterface);
				}
			}
		}
		ElementData.bPendingChildLazyModifiersGathering = false;
	}
}
