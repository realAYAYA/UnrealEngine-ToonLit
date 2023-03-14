// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_WorldCollision.h"
#include "Math/ControlRigMathLibrary.h"
#include "Units/RigUnitContext.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_WorldCollision)

FRigUnit_SphereTraceWorld_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    bHit = false;
	HitLocation = FVector::ZeroVector;
	HitNormal = FVector(0.f, 0.f, 1.f);

	if(Context.World == nullptr)
	{
		return;
	}
	
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	if (Context.OwningActor)
	{
		QueryParams.AddIgnoredActor(Context.OwningActor);
	}
	else if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Context.OwningComponent))
	{
		QueryParams.AddIgnoredComponent(PrimitiveComponent);
	}
	
	FCollisionResponseParams ResponseParams(ECR_Ignore);
	ResponseParams.CollisionResponse.SetResponse(ECC_WorldStatic, ECR_Block);

	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Radius);

	FHitResult HitResult;
	bHit = Context.World->SweepSingleByChannel(HitResult, Context.ToWorldSpace(Start), Context.ToWorldSpace(End), 
			FQuat::Identity, Channel, CollisionShape, QueryParams, ResponseParams);

	if (bHit)
	{
		HitLocation = Context.ToRigSpace(HitResult.ImpactPoint);
		HitNormal = Context.ToWorldSpaceTransform.InverseTransformVector(HitResult.ImpactNormal);
	}
}

FRigVMStructUpgradeInfo FRigUnit_SphereTraceWorld::GetUpgradeInfo() const
{
	FRigUnit_SphereTraceByTraceChannel NewNode;
	NewNode.Start = Start;
	NewNode.End = End;
	NewNode.Radius = Radius;

	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_SphereTraceByTraceChannel_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	bHit = false;
	HitLocation = FVector::ZeroVector;
	HitNormal = FVector(0.f, 0.f, 1.f);

	if (Context.World == nullptr)
	{
		return;
	}

	const ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel); 
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Radius);

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	if (Context.OwningActor)
	{
		QueryParams.AddIgnoredActor(Context.OwningActor);
	}
	else if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Context.OwningComponent))
	{
		QueryParams.AddIgnoredComponent(PrimitiveComponent);
	}

	FHitResult HitResult;
	bHit = Context.World->SweepSingleByChannel(HitResult, Context.ToWorldSpace(Start), Context.ToWorldSpace(End), FQuat::Identity, CollisionChannel, CollisionShape, QueryParams);
	
	if (bHit)
	{
		HitLocation = Context.ToRigSpace(HitResult.ImpactPoint);
		HitNormal = Context.ToWorldSpaceTransform.InverseTransformVector(HitResult.ImpactNormal);
	}
}

FRigUnit_SphereTraceByObjectTypes_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	bHit = false;
	HitLocation = FVector::ZeroVector;
	HitNormal = FVector(0.f, 0.f, 1.f);

	if (Context.World == nullptr)
	{
		return;
	}

	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(Radius); 

	// similar to KismetTraceUtils.h -> ConfigureCollisionObjectParams
	TArray<ECollisionChannel> CollisionChannels;
	CollisionChannels.AddUninitialized(ObjectTypes.Num());

	for (int Index = 0; Index < ObjectTypes.Num(); Index++)
	{ 
		CollisionChannels[Index] = UEngineTypes::ConvertToCollisionChannel(ObjectTypes[Index]);
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const ECollisionChannel& Channel : CollisionChannels)
	{
		if (FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
		{
			ObjectParams.AddObjectTypesToQuery(Channel);
		}
	}

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;

	if (Context.OwningActor)
	{
		QueryParams.AddIgnoredActor(Context.OwningActor);
	}
	else if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Context.OwningComponent))
	{
		QueryParams.AddIgnoredComponent(PrimitiveComponent);
	} 

	FHitResult HitResult;
	bHit = Context.World->SweepSingleByObjectType(HitResult, Context.ToWorldSpace(Start), Context.ToWorldSpace(End), FQuat::Identity, ObjectParams, CollisionShape, QueryParams);

	if (bHit)
	{
		HitLocation = Context.ToRigSpace(HitResult.ImpactPoint);
		HitNormal = Context.ToWorldSpaceTransform.InverseTransformVector(HitResult.ImpactNormal);
	}
}

