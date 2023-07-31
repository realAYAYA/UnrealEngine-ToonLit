// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DisplayClusterScreenComponent.h"

#include "DisplayClusterVersion.h"
#include "Components/StaticMeshComponent.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "UObject/ConstructorHelpers.h"


UDisplayClusterScreenComponent::UDisplayClusterScreenComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ScreenMesh(TEXT("/nDisplay/Meshes/plane_1x1"));
	SetStaticMesh(ScreenMesh.Object);

	SetMobility(EComponentMobility::Movable);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetVisibility(false);
	SetHiddenInGame(true);

#if WITH_EDITOR
	if (GIsEditor)
	{
		SetVisibility(true);
	}

	SetCastShadow(false);

	bVisibleInReflectionCaptures = false;
	bVisibleInRayTracing = false;
	bVisibleInRealTimeSkyCaptures = false;
#endif

	// Default screen size
	SetScreenSize(FVector2D(100.f, 56.25f));
}

FVector2D UDisplayClusterScreenComponent::GetScreenSize() const
{
	const FVector ComponentScale = GetComponentScale();
	const FVector2D ComponentScale2D(ComponentScale.Y, ComponentScale.Z);
	return ComponentScale2D;
}

void UDisplayClusterScreenComponent::SetScreenSize(const FVector2D& InSize)
{
	SetWorldScale3D(FVector(1.f, InSize.X, InSize.Y));

#if WITH_EDITOR
	Size = InSize;
#endif
}

void UDisplayClusterScreenComponent::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FDisplayClusterCustomVersion::GUID);
	if (Ar.IsLoading() &&
		Ar.CustomVer(FDisplayClusterCustomVersion::GUID) < FDisplayClusterCustomVersion::ComponentParentChange_4_27)
	{
		USceneComponent::Serialize(Ar);

#if WITH_EDITOR
		// Filtering by GetOwner() allows to avoid double upscaling for many cases. But it's not enough unfortunately.
		// There are still some cases where screens get upscaled repeatedly and get x10000 in the end.
		SetScreenSize(Size * (GetOwner() ? 1.f : 100.f));
#endif
	}
	else
	{
		Super::Serialize(Ar);
	}
}

#if WITH_EDITOR
void UDisplayClusterScreenComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.MemberProperty)
	{
		if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDisplayClusterScreenComponent, Size))
		{
			SetScreenSize(Size);
		}
		else if (PropertyChangedEvent.MemberProperty->GetFName() == TEXT("RelativeScale3D"))
		{
			UpdateScreenSizeFromScale();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UDisplayClusterScreenComponent::UpdateScreenSizeFromScale()
{
	Size = GetScreenSize();
}
#endif
