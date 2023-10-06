// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectSubsystemRenderingActor.h"

#include "Engine/World.h"
#include "SmartObjectSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectSubsystemRenderingActor)

//----------------------------------------------------------------------//
// USmartObjectSubsystemRenderingComponent
//----------------------------------------------------------------------//
FBoxSphereBounds USmartObjectSubsystemRenderingComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (const USmartObjectSubsystem* Subsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(GetWorld()))
	{
		return Subsystem->GetSmartObjectContainer().GetBounds();
	}
	return FBox(ForceInit);
}

#if UE_ENABLE_DEBUG_DRAWING
void USmartObjectSubsystemRenderingComponent::DebugDraw(FDebugRenderSceneProxy* DebugProxy)
{
	if (const USmartObjectSubsystem* Subsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(GetWorld()))
	{
		Subsystem->DebugDraw(DebugProxy);
	}
}

void USmartObjectSubsystemRenderingComponent::DebugDrawCanvas(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (const USmartObjectSubsystem* Subsystem = UWorld::GetSubsystem<USmartObjectSubsystem>(GetWorld()))
	{
		Subsystem->DebugDrawCanvas(Canvas, PlayerController);
	}
}
#endif // UE_ENABLE_DEBUG_DRAWING


//----------------------------------------------------------------------//
// ASmartObjectSubsystemRenderingActor
//----------------------------------------------------------------------//
ASmartObjectSubsystemRenderingActor::ASmartObjectSubsystemRenderingActor()
{
	RenderingComponent = CreateDefaultSubobject<USmartObjectSubsystemRenderingComponent>(TEXT("RenderingComp"));
	RootComponent = RenderingComponent;

#if WITH_EDITORONLY_DATA
	bListedInSceneOutliner = false;
#endif
}


