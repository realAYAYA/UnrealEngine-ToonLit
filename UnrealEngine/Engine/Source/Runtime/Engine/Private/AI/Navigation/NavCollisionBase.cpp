// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavCollisionBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavCollisionBase)


UNavCollisionBase::FConstructNew UNavCollisionBase::ConstructNewInstanceDelegate;
UNavCollisionBase::FDelegateInitializer UNavCollisionBase::DelegateInitializer;

UNavCollisionBase::FDelegateInitializer::FDelegateInitializer()
{
	UNavCollisionBase::ConstructNewInstanceDelegate.BindLambda([](UObject&) { return nullptr; });
}
	
UNavCollisionBase::UNavCollisionBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsDynamicObstacle = false;
	bHasConvexGeometry = false;
}

