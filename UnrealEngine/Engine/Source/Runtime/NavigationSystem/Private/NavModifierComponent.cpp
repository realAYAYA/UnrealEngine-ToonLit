// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavModifierComponent.h"
#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "AI/NavigationModifier.h"
#include "NavAreas/NavArea_Null.h"
#include "PhysicsEngine/BodySetup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavModifierComponent)

UNavModifierComponent::UNavModifierComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AreaClass = UNavArea_Null::StaticClass();
	FailsafeExtent = FVector(100, 100, 100);
	bIncludeAgentHeight = true;
}

void UNavModifierComponent::CalcAndCacheBounds() const
{
	AActor* MyOwner = GetOwner();
	if (MyOwner)
	{
		CachedTransform = MyOwner->GetActorTransform();
		
		if (TransformUpdateHandle.IsValid() == false && MyOwner->GetRootComponent())
		{
			// binding to get notifies when the root component moves. We need
			// this only when the rootcomp is nav-irrelevant (since the default 
			// mechanisms won't kick in) but we're binding without checking it since
			// this property can change without re-running CalcAndCacheBounds.
			// We're filtering for nav relevancy in OnTransformUpdated.
			TransformUpdateHandle = MyOwner->GetRootComponent()->TransformUpdated.AddUObject(const_cast<UNavModifierComponent*>(this), &UNavModifierComponent::OnTransformUpdated);
		}

		Bounds = FBox(ForceInit);
		ComponentBounds.Reset();
		for (UActorComponent* Component : MyOwner->GetComponents())
		{
			UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Component);
			if (PrimComp && PrimComp->IsRegistered() && PrimComp->IsCollisionEnabled() && PrimComp->CanEverAffectNavigation())
			{
				UBodySetup* BodySetup = PrimComp->GetBodySetup();
				if (BodySetup)
				{
					FTransform ParentTM = PrimComp->GetComponentTransform();
					const FVector Scale3D = ParentTM.GetScale3D();
					ParentTM.RemoveScaling();
					Bounds += PrimComp->Bounds.GetBox();

					for (int32 SphereIdx = 0; SphereIdx < BodySetup->AggGeom.SphereElems.Num(); SphereIdx++)
					{
						const FKSphereElem& ElemInfo = BodySetup->AggGeom.SphereElems[SphereIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= ParentTM;

						const FBox SphereBounds = FBox::BuildAABB(ElemTM.GetLocation(), ElemInfo.Radius * Scale3D);
						ComponentBounds.Add(FRotatedBox(SphereBounds, ElemTM.GetRotation()));
					}

					for (int32 BoxIdx = 0; BoxIdx < BodySetup->AggGeom.BoxElems.Num(); BoxIdx++)
					{
						const FKBoxElem& ElemInfo = BodySetup->AggGeom.BoxElems[BoxIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= ParentTM;

						const FBox BoxBounds = FBox::BuildAABB(ElemTM.GetLocation(), FVector(ElemInfo.X, ElemInfo.Y, ElemInfo.Z) * Scale3D * 0.5f);
						ComponentBounds.Add(FRotatedBox(BoxBounds, ElemTM.GetRotation()));
					}

					for (int32 SphylIdx = 0; SphylIdx < BodySetup->AggGeom.SphylElems.Num(); SphylIdx++)
					{
						const FKSphylElem& ElemInfo = BodySetup->AggGeom.SphylElems[SphylIdx];
						FTransform ElemTM = ElemInfo.GetTransform();
						ElemTM.ScaleTranslation(Scale3D);
						ElemTM *= ParentTM;

						const FBox SphylBounds = FBox::BuildAABB(ElemTM.GetLocation(), FVector(ElemInfo.Radius, ElemInfo.Radius, ElemInfo.Length) * Scale3D);
						ComponentBounds.Add(FRotatedBox(SphylBounds, ElemTM.GetRotation()));
					}

					for (int32 ConvexIdx = 0; ConvexIdx < BodySetup->AggGeom.ConvexElems.Num(); ConvexIdx++)
					{
						const FKConvexElem& ElemInfo = BodySetup->AggGeom.ConvexElems[ConvexIdx];
						FTransform ElemTM = ElemInfo.GetTransform();

						const FBox ConvexBounds = FBox::BuildAABB(ParentTM.TransformPosition(ElemInfo.ElemBox.GetCenter() * Scale3D), ElemInfo.ElemBox.GetExtent() * Scale3D);
						ComponentBounds.Add(FRotatedBox(ConvexBounds, ElemTM.GetRotation() * ParentTM.GetRotation()));
					}
				}
			}
		}

		if (ComponentBounds.Num() == 0)
		{
			Bounds = FBox::BuildAABB(MyOwner->GetActorLocation(), FailsafeExtent);
			ComponentBounds.Add(FRotatedBox(Bounds, MyOwner->GetActorQuat()));
		}

		for (int32 Idx = 0; Idx < ComponentBounds.Num(); Idx++)
		{
			const FVector BoxOrigin = ComponentBounds[Idx].Box.GetCenter();
			const FVector BoxExtent = ComponentBounds[Idx].Box.GetExtent();

			const FVector NavModBoxOrigin = FTransform(ComponentBounds[Idx].Quat).InverseTransformPosition(BoxOrigin);
			ComponentBounds[Idx].Box = FBox::BuildAABB(NavModBoxOrigin, BoxExtent);
		}
	}
}

void UNavModifierComponent::GetNavigationData(FNavigationRelevantData& Data) const
{
	for (int32 Idx = 0; Idx < ComponentBounds.Num(); Idx++)
	{
		Data.Modifiers.Add(FAreaNavModifier(ComponentBounds[Idx].Box, FTransform(ComponentBounds[Idx].Quat), AreaClass).SetIncludeAgentHeight(bIncludeAgentHeight));
	}
}

void UNavModifierComponent::SetAreaClass(TSubclassOf<UNavArea> NewAreaClass)
{
	if (AreaClass != NewAreaClass)
	{
		AreaClass = NewAreaClass;
		RefreshNavigationModifiers();
	}
}

void UNavModifierComponent::OnTransformUpdated(USceneComponent* RootComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	// force bounds recaching next time GetNavigationBounds gets called.
	bBoundsInitialized = false;

	// otherwise the update will be handled by the default path
	if (RootComponent && RootComponent->CanEverAffectNavigation() == false)
	{
		// since the parent is not nav-relevant we need to manually tell nav sys to update
		RefreshNavigationModifiers();
	}
}

