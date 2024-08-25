// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectRenderingComponent.h"
#include "DebugRenderSceneProxy.h"
#include "Engine/CollisionProfile.h"
#include "SmartObjectComponent.h"
#include "ObjectEditorUtils.h"
#include "PrimitiveViewRelevance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectRenderingComponent)

//----------------------------------------------------------------------//
// FSORenderingSceneProxy
//----------------------------------------------------------------------//
class FSORenderingSceneProxy final : public FDebugRenderSceneProxy
{
	typedef TPair<FVector, FVector> FVectorPair;
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	/** Initialization constructor. */
	explicit FSORenderingSceneProxy(const USmartObjectRenderingComponent& InComponent)
		: FDebugRenderSceneProxy(&InComponent)
	{
		const AActor* Owner = InComponent.GetOwner();
		if (Owner == nullptr)
		{
			return;
		}

		const USmartObjectComponent* SOComp = Owner->FindComponentByClass<USmartObjectComponent>();
		if (SOComp == nullptr || SOComp->GetDefinition() == nullptr)
		{
			return;
		}

		const USmartObjectDefinition* Definition = SOComp->GetDefinition();
		if (Definition == nullptr)
		{
			return;
		}

		// For loaded instances, we draw slots only when selected but 
		// for other cases (e.g. instances newly added to world, preview actors, etc.) we
		// want to draw all the time to improve authoring.
		bDrawEvenIfNotSelected = !Owner->HasAnyFlags(RF_WasLoaded);

		constexpr float DebugCylinderRadius = 40.f;
		constexpr float DebugCylinderHalfHeight = 100.f;
		FColor DebugColor = FColor::Yellow;
		ESmartObjectSlotShape SlotShape = ESmartObjectSlotShape::Circle;
		float SlotSize = DebugCylinderRadius;

		const FTransform OwnerLocalToWorld = SOComp->GetComponentTransform();

		const TConstArrayView<FSmartObjectSlotDefinition> Slots(Definition->GetSlots());
		for (int32 Index = 0; Index < Slots.Num(); Index++)
		{
			const FSmartObjectSlotDefinition& SlotDefinition = Slots[Index]; 
			const FTransform Transform = Definition->GetSlotWorldTransform(Index, OwnerLocalToWorld);
			
#if WITH_EDITORONLY_DATA
			DebugColor = SlotDefinition.DEBUG_DrawColor;
			SlotShape = SlotDefinition.DEBUG_DrawShape;
			SlotSize = SlotDefinition.DEBUG_DrawSize;
#endif
			// @todo: these are not in par with the other Smart Object debug rendering.
			const FVector DebugPosition = Transform.GetLocation();
			const FVector Direction = Transform.GetRotation().GetForwardVector();
			if (SlotShape == ESmartObjectSlotShape::Circle) //-V547
			{
				Cylinders.Emplace(DebugPosition, FVector::UpVector, SlotSize, DebugCylinderHalfHeight, DebugColor);
			}
			else if (SlotShape == ESmartObjectSlotShape::Rectangle)
			{
				Boxes.Emplace(FBox(FVector(-SlotSize,-SlotSize,0), FVector(SlotSize,SlotSize,DebugCylinderHalfHeight)), DebugColor, Transform);
			}
			
			ArrowLines.Emplace(DebugPosition, DebugPosition + Direction * 2.0f * SlotSize, DebugColor);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && (IsSelected() || bDrawEvenIfNotSelected);
		Result.bDynamicRelevance = true;
		// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
		Result.bSeparateTranslucency = Result.bNormalTranslucency = IsShown(View);
		return Result;
	}

	bool bDrawEvenIfNotSelected = true;
};

//----------------------------------------------------------------------//
// USmartObjectRenderingComponent
//----------------------------------------------------------------------//
USmartObjectRenderingComponent::USmartObjectRenderingComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Allows updating in game, while optimizing rendering for the case that it is not modified
	Mobility = EComponentMobility::Stationary;

	BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	bIsEditorOnly = true;

	SetGenerateOverlapEvents(false);
}

FPrimitiveSceneProxy* USmartObjectRenderingComponent::CreateSceneProxy()
{
	return new FSORenderingSceneProxy(*this);
}

FBoxSphereBounds USmartObjectRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBox BoundingBox(EForceInit::ForceInitToZero);

	if (HasAnyFlags(RF_BeginDestroyed) == false && GetAttachParent() != nullptr)
	{
		const USmartObjectComponent* SOComp = nullptr;

		const AActor* Owner = GetOwner();
		if (Owner)
		{
			BoundingBox += Owner->GetActorLocation();
			SOComp = Owner->FindComponentByClass<USmartObjectComponent>();
		}

		if (SOComp)
		{
			BoundingBox += SOComp->GetSmartObjectBounds();
		}
	}

	return FBoxSphereBounds(BoundingBox);
}

#if WITH_EDITOR
void USmartObjectRenderingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName SmartObjectsName = TEXT("SmartObjects");

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property
		&& FObjectEditorUtils::GetCategoryFName(PropertyChangedEvent.Property) == SmartObjectsName)
	{
		MarkRenderStateDirty();
	}
}
#endif // WITH_EDITOR
