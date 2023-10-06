// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectContainerRenderingComponent.h"
#include "Engine/CollisionProfile.h"
#include "PrimitiveViewRelevance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectContainerRenderingComponent)

#if WITH_EDITORONLY_DATA
#include "Engine/CollisionProfile.h"
#include "SceneManagement.h"
#include "DebugRenderSceneProxy.h"
#include "Math/ColorList.h"
#include "SmartObjectPersistentCollection.h"
#else
#include "Engine/CollisionProfile.h"
#endif

#if WITH_EDITORONLY_DATA

//----------------------------------------------------------------------//
// FSOContainerRenderingSceneProxy
//----------------------------------------------------------------------//
class FSOContainerRenderingSceneProxy final : public FDebugRenderSceneProxy
{
public:
	/** Initialization constructor. */
	FSOContainerRenderingSceneProxy(const USmartObjectContainerRenderingComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{
		ASmartObjectPersistentCollection* Owner = Cast<ASmartObjectPersistentCollection>(InComponent.GetOwner());
		if (Owner == nullptr || Owner->IsEmpty() || Owner->ShouldDebugDraw() == false)
		{
			return;
		}

		OwnerLocation = Owner->GetActorLocation();
		EndLocations.Reserve(Owner->GetSmartObjectContainer().GetEntries().Num());

		for (const FSmartObjectCollectionEntry& Entry : Owner->GetSmartObjectContainer().GetEntries())
		{
			const FVector EntryLocation = Entry.GetTransform().GetLocation();
			EndLocations.Add(EntryLocation);
		}
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		constexpr float ArrowLineThickness = 5.f;
		constexpr float ArrowHeadSize = 30.f;
		static const FLinearColor RadiusColor = FLinearColor(FColorList::Orange);

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

				for (const FVector& Location : EndLocations)
				{
					PDI->DrawLine(OwnerLocation, Location, RadiusColor, SDPG_World, ArrowLineThickness, /*DepthBias=*/0.f, /*bScreenSpace=*/true);
					DrawArrowHead(PDI, Location, OwnerLocation, ArrowHeadSize, RadiusColor, SDPG_World, ArrowLineThickness, /*bScreenSpace=*/true);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && IsSelected();
		Result.bDynamicRelevance = true;
		// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
		Result.bSeparateTranslucency = Result.bNormalTranslucency = IsShown(View);
		return Result;
	}

	FVector OwnerLocation;
	TArray<FVector> EndLocations;
};

#endif // WITH_EDITORONLY_DATA

//----------------------------------------------------------------------//
// USmartObjectContainerRenderingComponent
//----------------------------------------------------------------------//
USmartObjectContainerRenderingComponent::USmartObjectContainerRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Allows updating in game, while optimizing rendering for the case that it is not modified
	Mobility = EComponentMobility::Stationary;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bIsEditorOnly = true;

	SetGenerateOverlapEvents(false);
}

FPrimitiveSceneProxy* USmartObjectContainerRenderingComponent::CreateSceneProxy()
{
#if WITH_EDITORONLY_DATA
	return new FSOContainerRenderingSceneProxy(*this);
#else
	return nullptr;
#endif // WITH_EDITORONLY_DATA
}

FBoxSphereBounds USmartObjectContainerRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(EForceInit::ForceInitToZero);
#if WITH_EDITORONLY_DATA
	const ASmartObjectPersistentCollection* Owner = Cast<ASmartObjectPersistentCollection>(GetOwner());
	if (Owner && HasAnyFlags(RF_BeginDestroyed) == false)
	{
		FVector Origin(ForceInit), BoxExtent(ForceInit);
		Owner->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, BoxExtent);
		BoundingBox = FBox(Origin - BoxExtent, Origin + BoxExtent);
		BoundingBox += Owner->GetBounds();
	}
#endif // WITH_EDITORONLY_DATA
	return FBoxSphereBounds(BoundingBox);
}
