// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionISMPoolActor.h"
#include "GeometryCollection/GeometryCollectionISMPoolComponent.h"
#include "GeometryCollection/GeometryCollectionISMPoolDebugDrawComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionISMPoolActor)

AGeometryCollectionISMPoolActor::AGeometryCollectionISMPoolActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ISMPoolComp = CreateDefaultSubobject<UGeometryCollectionISMPoolComponent>(TEXT("ISMPoolComp"));
	RootComponent = ISMPoolComp;

#if UE_ENABLE_DEBUG_DRAWING
	ISMPoolDebugDrawComp = CreateDefaultSubobject<UGeometryCollectionISMPoolDebugDrawComponent>(TEXT("ISMPoolDebug"));
	ISMPoolDebugDrawComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISMPoolDebugDrawComp->SetCanEverAffectNavigation(false);
	ISMPoolDebugDrawComp->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	ISMPoolDebugDrawComp->SetGenerateOverlapEvents(false);
	ISMPoolDebugDrawComp->SetupAttachment(ISMPoolComp);
#endif
}
