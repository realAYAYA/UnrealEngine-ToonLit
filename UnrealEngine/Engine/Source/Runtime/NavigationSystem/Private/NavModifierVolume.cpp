// Copyright Epic Games, Inc. All Rights Reserved.

#include "NavModifierVolume.h"
#include "NavigationSystem.h"
#include "NavigationSystemTypes.h"
#include "AI/NavigationModifier.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationOctree.h"
#include "Components/BrushComponent.h"
#include "AI/NavigationSystemHelpers.h"
#include "Engine/CollisionProfile.h"
#include "Model.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavModifierVolume)

//----------------------------------------------------------------------//
// ANavModifierVolume
//----------------------------------------------------------------------//
ANavModifierVolume::ANavModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AreaClass(UNavArea_Null::StaticClass())
{
	if (GetBrushComponent())
	{
		GetBrushComponent()->SetGenerateOverlapEvents(false);
		GetBrushComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	}
}

void ANavModifierVolume::GetNavigationData(FNavigationRelevantData& Data) const
{
	if (Brush && AreaClass && AreaClass != FNavigationSystem::GetDefaultWalkableArea())
	{
		Data.Modifiers.CreateAreaModifiers(GetBrushComponent(), AreaClass);
	}

	if (bMaskFillCollisionUnderneathForNavmesh)
	{
		if (GetBrushComponent()->Brush != nullptr)
		{
			const FBox& Box = GetBrushComponent()->Brush->Bounds.GetBox();
			FAreaNavModifier AreaMod(Box, GetBrushComponent()->GetComponentTransform(), AreaClass);
			Data.Modifiers.SetMaskFillCollisionUnderneathForNavmesh(true);
			Data.Modifiers.Add(AreaMod);
		}
	}
}

FBox ANavModifierVolume::GetNavigationBounds() const
{
	return GetComponentsBoundingBox(/*bNonColliding=*/ true);
}

void ANavModifierVolume::SetAreaClass(TSubclassOf<UNavArea> NewAreaClass)
{
	if (NewAreaClass != AreaClass)
	{
		AreaClass = NewAreaClass;

		FNavigationSystem::UpdateActorData(*this);
	}
}

void ANavModifierVolume::RebuildNavigationData()
{
	FNavigationSystem::UpdateActorData(*this);
}

#if WITH_EDITOR

void ANavModifierVolume::PostEditUndo()
{
	Super::PostEditUndo();

	if (GetBrushComponent())
	{
		GetBrushComponent()->BuildSimpleBrushCollision();
	}
	FNavigationSystem::UpdateActorData(*this);
}

void ANavModifierVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_AreaClass = GET_MEMBER_NAME_CHECKED(ANavModifierVolume, AreaClass);
	static const FName NAME_BrushComponent = TEXT("BrushComponent");

	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	const FName PropName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropName == NAME_AreaClass)
	{
		FNavigationSystem::UpdateActorData(*this);
	}
	else if (PropName == NAME_BrushComponent)
	{
		if (GetBrushComponent())
		{
			if (GetBrushComponent()->GetBodySetup() && NavigationHelper::IsBodyNavigationRelevant(*GetBrushComponent()->GetBodySetup()))
			{
				FNavigationSystem::UpdateActorData(*this);
			}
			else
			{
				FNavigationSystem::OnActorUnregistered(*this);
			}
		}
	}
}

#endif

