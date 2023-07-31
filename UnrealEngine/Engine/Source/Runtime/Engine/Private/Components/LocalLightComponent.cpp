// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightComponent.cpp: PointLightComponent implementation.
=============================================================================*/

#include "Components/LocalLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "RenderingThread.h"
#include "Engine/Texture2D.h"
#include "SceneManagement.h"
#include "PointLightSceneProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LocalLightComponent)

ULocalLightComponent::ULocalLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Intensity = 5000;
	Radius_DEPRECATED = 1024.0f;
	AttenuationRadius = 1000;
	InverseExposureBlend = 0;
}

void ULocalLightComponent::SetAttenuationRadius(float NewRadius)
{
	// Only movable lights can change their radius at runtime
	if (AreDynamicDataChangesAllowed(false)
		&& NewRadius != AttenuationRadius)
	{
		AttenuationRadius = NewRadius;
		PushRadiusToRenderThread();
	}
}

void ULocalLightComponent::SetIntensityUnits(ELightUnits NewIntensityUnits)
{
	if (AreDynamicDataChangesAllowed()
		&& IntensityUnits != NewIntensityUnits)
	{
		IntensityUnits = NewIntensityUnits;

		UpdateColorAndBrightness();
	}
}

bool ULocalLightComponent::AffectsBounds(const FBoxSphereBounds& InBounds) const
{
	if((InBounds.Origin - GetComponentTransform().GetLocation()).SizeSquared() > FMath::Square(AttenuationRadius + InBounds.SphereRadius))
	{
		return false;
	}

	if(!Super::AffectsBounds(InBounds))
	{
		return false;
	}

	return true;
}

void ULocalLightComponent::SendRenderTransform_Concurrent()
{
	// Update the scene info's cached radius-dependent data.
	if(SceneProxy)
	{
		((FLocalLightSceneProxy*)SceneProxy)->UpdateRadius_GameThread(AttenuationRadius);
	}

	Super::SendRenderTransform_Concurrent();
}

FVector4 ULocalLightComponent::GetLightPosition() const
{
	return FVector4(GetComponentTransform().GetLocation(),1);
}

FBox ULocalLightComponent::GetBoundingBox() const
{
	return FBox(GetComponentLocation() - FVector(AttenuationRadius,AttenuationRadius,AttenuationRadius),GetComponentLocation() + FVector(AttenuationRadius,AttenuationRadius,AttenuationRadius));
}

FSphere ULocalLightComponent::GetBoundingSphere() const
{
	return FSphere(GetComponentTransform().GetLocation(), AttenuationRadius);
}

void ULocalLightComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.UEVer() < VER_UE4_INVERSE_SQUARED_LIGHTS_DEFAULT)
	{
		AttenuationRadius = Radius_DEPRECATED;
	}
}

#if WITH_EDITOR

bool ULocalLightComponent::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, bCastShadowsFromCinematicObjectsOnly) && bUseRayTracedDistanceFieldShadows)
		{
			return false;
		}
	}

	return Super::CanEditChange(InProperty);
}

/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	FProperty that has been changed, NULL if unknown
 */
void ULocalLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Intensity = FMath::Max(0.0f, Intensity);
	LightmassSettings.IndirectLightingSaturation = FMath::Max(LightmassSettings.IndirectLightingSaturation, 0.0f);
	LightmassSettings.ShadowExponent = FMath::Clamp(LightmassSettings.ShadowExponent, .5f, 8.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void ULocalLightComponent::PostInterpChange(FProperty* PropertyThatChanged)
{
	static FName RadiusName(TEXT("Radius"));
	static FName AttenuationRadiusName(TEXT("AttenuationRadius"));
	FName PropertyName = PropertyThatChanged->GetFName();

	if (PropertyName == RadiusName
		|| PropertyName == AttenuationRadiusName)
	{
		// Old radius tracks will animate the deprecated value
		if (PropertyName == RadiusName)
		{
			AttenuationRadius = Radius_DEPRECATED;
		}

		PushRadiusToRenderThread();
	}
	else
	{
		Super::PostInterpChange(PropertyThatChanged);
	}
}

void ULocalLightComponent::PushRadiusToRenderThread()
{
	if (CastShadows)
	{
		// Shadow casting lights need to recompute light interactions
		// to determine which primitives to draw in shadow depth passes.
		MarkRenderStateDirty();
	}
	else
	{
		if (SceneProxy)
		{
			((FLocalLightSceneProxy*)SceneProxy)->UpdateRadius_GameThread(AttenuationRadius);
		}
	}
}

float ULocalLightComponent::GetUnitsConversionFactor(ELightUnits SrcUnits, ELightUnits TargetUnits, float CosHalfConeAngle)
{
	CosHalfConeAngle = FMath::Clamp<float>(CosHalfConeAngle, -1, 1 - UE_KINDA_SMALL_NUMBER);

	if (SrcUnits == TargetUnits)
	{
		return 1.f;
	}
	else
	{
		float CnvFactor = 1.f;
		
		if (SrcUnits == ELightUnits::Candelas)
		{
			CnvFactor = 100.f * 100.f;
		}
		else if (SrcUnits == ELightUnits::Lumens)
		{
			CnvFactor = 100.f * 100.f / 2.f / UE_PI / (1.f - CosHalfConeAngle);
		}
		else
		{
			CnvFactor = 16.f;
		}

		if (TargetUnits == ELightUnits::Candelas)
		{
			CnvFactor *= 1.f / 100.f / 100.f;
		}
		else if (TargetUnits == ELightUnits::Lumens)
		{
			CnvFactor *= 2.f  * UE_PI * (1.f - CosHalfConeAngle) / 100.f / 100.f;
		}
		else
		{
			CnvFactor *= 1.f / 16.f;
		}

		return CnvFactor;
	}
}

