// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeSplineActor.h"
#include "LandscapeInfo.h"
#include "LandscapeInfoMap.h"
#include "Landscape.h"
#include "LandscapeSplinesComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeSplineActor)

#if WITH_EDITOR
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "ControlPointMeshComponent.h"
#endif

ALandscapeSplineActor::ALandscapeSplineActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ULandscapeSplinesComponent* SplineComponent = CreateDefaultSubobject<ULandscapeSplinesComponent>(TEXT("RootComponent0"));
	
	RootComponent = SplineComponent;	
	RootComponent->Mobility = EComponentMobility::Static;
}

ULandscapeSplinesComponent* ALandscapeSplineActor::GetSplinesComponent() const
{
	return Cast<ULandscapeSplinesComponent>(RootComponent);
}

FTransform ALandscapeSplineActor::LandscapeActorToWorld() const
{
	return GetLandscapeInfo()->LandscapeActor->LandscapeActorToWorld();
}

ULandscapeInfo* ALandscapeSplineActor::GetLandscapeInfo() const
{
	return ULandscapeInfo::Find(GetWorld(), LandscapeGuid);
}

#if WITH_EDITOR
void ALandscapeSplineActor::GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const
{
	Super::GetActorDescProperties(PropertyPairsMap);

	if (LandscapeGuid.IsValid())
	{
		PropertyPairsMap.AddProperty(ALandscape::AffectsLandscapeActorDescProperty, *LandscapeGuid.ToString());
	}
}

void ALandscapeSplineActor::GetSharedProperties(ULandscapeInfo* InLandscapeInfo)
{
	Modify();
	LandscapeGuid = InLandscapeInfo->LandscapeGuid;
	LandscapeActor = InLandscapeInfo->LandscapeActor.Get();
}

void ALandscapeSplineActor::SetLandscapeGuid(const FGuid& InGuid)
{ 
	LandscapeGuid = InGuid;
	check(!LandscapeActor || (LandscapeGuid == LandscapeActor->GetLandscapeGuid()));
}

// This function is only allowed to be called to fixup LandscapeActor pointer
void ALandscapeSplineActor::SetLandscapeActor(ALandscape* InLandscapeActor)
{
	if (LandscapeActor != InLandscapeActor)
	{
		Modify();
		check(LandscapeActor == nullptr);
		check(LandscapeGuid == InLandscapeActor->GetLandscapeGuid());
		LandscapeActor = InLandscapeActor;
	}
}

void ALandscapeSplineActor::Destroyed()
{
	Super::Destroyed();

	UWorld* World = GetWorld();

	if (GIsEditor && !World->IsGameWorld())
	{
		// Modify Splines Objects to support Undo/Redo
		GetSplinesComponent()->ModifySplines();
	}
}

void ALandscapeSplineActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (!IsPendingKillPending())
	{
		UWorld* World = GetWorld();
		if (LandscapeGuid.IsValid())
		{
			ULandscapeInfo* LandscapeInfo = ULandscapeInfo::FindOrCreate(World, LandscapeGuid);
			LandscapeInfo->RegisterSplineActor(this);
		}

		// If Landscape uses generated LandscapeSplineMeshesActors, ensure SplineMeshComponents & ControlPointMeshComponents are hidden in PIE
		if (World->IsGameWorld() && HasGeneratedLandscapeSplineMeshesActors())
		{
			ForEachComponent<UStaticMeshComponent>(true, [](UStaticMeshComponent* Component)
			{
				if (Component->IsA<USplineMeshComponent>() || Component->IsA<UControlPointMeshComponent>())
				{
					Component->SetHiddenInGame(true);
				}
			});
		}
	}
}

void ALandscapeSplineActor::UnregisterAllComponents(bool bForReregister)
{
	if (GetWorld() && IsValidChecked(GetWorld()) && !GetWorld()->IsUnreachable() && LandscapeGuid.IsValid())
	{
		if (ULandscapeInfo* LandscapeInfo = GetLandscapeInfo())
		{
			LandscapeInfo->UnregisterSplineActor(this);
		}
	}

	Super::UnregisterAllComponents(bForReregister);
}

void ALandscapeSplineActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		GetLandscapeInfo()->RequestSplineLayerUpdate();
	}
}

AActor* ALandscapeSplineActor::GetSceneOutlinerParent() const
{
	if (ULandscapeInfo* LandscapeInfo = GetLandscapeInfo())
	{
		return LandscapeInfo->LandscapeActor.Get();
	}

	return nullptr;
}

bool ALandscapeSplineActor::HasGeneratedLandscapeSplineMeshesActors() const
{
	return IsValid(LandscapeActor) && LandscapeActor->GetUseGeneratedLandscapeSplineMeshesActors();
}

#endif

