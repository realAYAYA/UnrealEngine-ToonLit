// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SpotLightComponent.cpp: LightComponent implementation.
=============================================================================*/

#include "Components/SpotLightComponent.h"
#include "RenderUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "PointLightSceneProxy.h"
#include "UObject/UnrealType.h"
#include "SceneInterface.h"
#include "SceneView.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SpotLightComponent)


/**
 * The scene info for a spot light.
 */
class FSpotLightSceneProxy : public FPointLightSceneProxy
{
public:

	/** Outer cone angle in radians, clamped to a valid range. */
	float OuterConeAngle;

	/** Cosine of the spot light's inner cone angle. */
	float CosInnerCone;

	/** Cosine of the spot light's outer cone angle. */
	float CosOuterCone;

	/** 1 / (CosInnerCone - CosOuterCone) */
	float InvCosConeDifference;

	/** Sine of the spot light's outer cone angle. */
	float SinOuterCone;

	/** 1 / Tangent of the spot light's outer cone angle. */
	float InvTanOuterCone;

	/** Initialization constructor. */
	FSpotLightSceneProxy(const USpotLightComponent* Component)
	:	FPointLightSceneProxy(Component)
	{
		const float ClampedInnerConeAngle = FMath::Clamp(Component->InnerConeAngle,0.0f,89.0f) * (float)UE_PI / 180.0f;
		const float ClampedOuterConeAngle = FMath::Clamp(Component->OuterConeAngle * (float)UE_PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)UE_PI / 180.0f + 0.001f);
		OuterConeAngle = ClampedOuterConeAngle;
		CosOuterCone = FMath::Cos(ClampedOuterConeAngle);
		SinOuterCone = FMath::Sin(ClampedOuterConeAngle);
		CosInnerCone = FMath::Cos(ClampedInnerConeAngle);
		InvCosConeDifference = 1.0f / (CosInnerCone - CosOuterCone);
		InvTanOuterCone = 1.0f / FMath::Tan(ClampedOuterConeAngle);
	}

	/** Accesses parameters needed for rendering the light. */
	virtual void GetLightShaderParameters(FLightRenderParameters& LightParameters, uint32 Flags=0) const override
	{
		LightParameters.WorldPosition = GetOrigin();
		LightParameters.InvRadius = InvRadius;
		LightParameters.Color = GetColor();
		LightParameters.FalloffExponent = FalloffExponent;
		LightParameters.Direction = FVector3f(-GetDirection());
		LightParameters.Tangent = FVector3f(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);
		LightParameters.SpotAngles = FVector2f(CosOuterCone, InvCosConeDifference);
		LightParameters.SpecularScale = SpecularScale;
		LightParameters.SourceRadius = SourceRadius;
		LightParameters.SoftSourceRadius = SoftSourceRadius;
		LightParameters.SourceLength = SourceLength;
		LightParameters.RectLightBarnCosAngle = 0.0f;
		LightParameters.RectLightBarnLength = -2.0f;
		LightParameters.RectLightAtlasUVOffset = FVector2f::ZeroVector;
		LightParameters.RectLightAtlasUVScale = FVector2f::ZeroVector;
		LightParameters.RectLightAtlasMaxLevel = FLightRenderParameters::GetRectLightAtlasInvalidMIPLevel();
		LightParameters.IESAtlasIndex = INDEX_NONE;
		LightParameters.LightFunctionAtlasLightIndex = GetLightFunctionAtlasLightIndex();
		LightParameters.InverseExposureBlend = InverseExposureBlend;

		if (IESAtlasId != uint32(INDEX_NONE))
		{
			GetSceneInterface()->GetLightIESAtlasSlot(this, &LightParameters);
		}
	}

	// FLightSceneInfo interface.
	virtual bool AffectsBounds(const FBoxSphereBounds& Bounds) const override
	{
		if(!FLocalLightSceneProxy::AffectsBounds(Bounds))
		{
			return false;
		}

		FVector	U = GetOrigin() - (Bounds.SphereRadius / SinOuterCone) * GetDirection(),
				D = Bounds.Origin - U;
		float	dsqr = D | D,
				E = GetDirection() | D;
		if(E > 0.0f && E * E >= dsqr * FMath::Square(CosOuterCone))
		{
			D = Bounds.Origin - GetOrigin();
			dsqr = D | D;
			E = -(GetDirection() | D);
			if(E > 0.0f && E * E >= dsqr * FMath::Square(SinOuterCone))
				return dsqr <= FMath::Square(Bounds.SphereRadius);
			else
				return true;
		}

		return false;
	}
	
	/**
	 * Sets up a projected shadow initializer for shadows from the entire scene.
	 * @return True if the whole-scene projected shadow should be used.
	 */
	virtual bool GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const override
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = OutInitializers.AddDefaulted_GetRef();
		OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
		OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
		OutInitializer.Scales = FVector2D(InvTanOuterCone,InvTanOuterCone);

		const FSphere AbsoluteBoundingSphere = FSpotLightSceneProxy::GetBoundingSphere();
		OutInitializer.SubjectBounds = FBoxSphereBounds(
			AbsoluteBoundingSphere.Center - GetOrigin(),
			FVector(AbsoluteBoundingSphere.W, AbsoluteBoundingSphere.W, AbsoluteBoundingSphere.W),
			AbsoluteBoundingSphere.W
			);

		OutInitializer.WAxis = FVector4(0,0,1,0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.bRayTracedDistanceField = UseRayTracedDistanceFieldShadows() && DoesPlatformSupportDistanceFieldShadowing(ViewFamily.GetShaderPlatform());
		return true;
	}

	virtual float GetOuterConeAngle() const override { return OuterConeAngle; }

	virtual FSphere GetBoundingSphere() const override
	{
		return FMath::ComputeBoundingSphereForCone(GetOrigin(), GetDirection(), (FSphere::FReal)Radius, (FSphere::FReal)CosOuterCone, (FSphere::FReal)SinOuterCone);
	}
	
	virtual float GetEffectiveScreenRadius(const FViewMatrices& ShadowViewMatrices, const FIntPoint& CameraViewRectSize) const 
	{
		// Heuristic: use the radius of the inscribed sphere at the cone's end as the light's effective screen radius
		// We do so because we do not want to use the light's radius directly, which will make us overestimate the shadow map resolution greatly for a spot light

		// In the correct form,
		//   InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius() / CosOuterCone
		//   InscribedSphereRadius = GetRadius() / SinOuterCone
		// Do it incorrectly to avoid division which is more expensive and risks division by zero
		const FVector InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius() * CosOuterCone;
		const float InscribedSphereRadius = GetRadius() * SinOuterCone;

		const float SphereDistanceFromViewOrigin = (InscribedSpherePosition - ShadowViewMatrices.GetViewOrigin()).Size();

		const FVector2D &ProjectionScale = ShadowViewMatrices.GetProjectionScale();
		const float ScreenScale = FMath::Max(CameraViewRectSize.X * 0.5f * ProjectionScale.X, CameraViewRectSize.Y * 0.5f * ProjectionScale.Y);

		return ScreenScale * InscribedSphereRadius / FMath::Max(SphereDistanceFromViewOrigin, 1.0f);
	}

	virtual float GetEffectiveScreenRadius(const FViewMatrices& ShadowViewMatrices) const override
	{
		// Heuristic: use the radius of the inscribed sphere at the cone's end as the light's effective screen radius
		// We do so because we do not want to use the light's radius directly, which will make us overestimate the shadow map resolution greatly for a spot light

		// In the correct form,
		//   InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius() / CosOuterCone
		//   InscribedSphereRadius = GetRadius() / SinOuterCone
		// Do it incorrectly to avoid division which is more expensive and risks division by zero
		const FVector InscribedSpherePosition = GetOrigin() + GetDirection() * GetRadius() * CosOuterCone;
		const float InscribedSphereRadius = GetRadius() * SinOuterCone;

		const float SphereDistanceFromViewOrigin = (InscribedSpherePosition - ShadowViewMatrices.GetViewOrigin()).Size();

		return ShadowViewMatrices.GetScreenScale() * InscribedSphereRadius / FMath::Max(SphereDistanceFromViewOrigin, 1.0f);
	}
};

USpotLightComponent::USpotLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpot"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightSpotMove"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	InnerConeAngle = 0.0f;
	OuterConeAngle = 44.0f;
}

float USpotLightComponent::GetHalfConeAngle() const
{
	const float ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle, 0.0f, 89.0f) * (float)UE_PI / 180.0f;
	const float ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)UE_PI / 180.0f, ClampedInnerConeAngle + 0.001f, 89.0f * (float)UE_PI / 180.0f + 0.001f);
	return ClampedOuterConeAngle;
}

float USpotLightComponent::GetCosHalfConeAngle() const
{
	return FMath::Cos(GetHalfConeAngle());
}

void USpotLightComponent::SetInnerConeAngle(float NewInnerConeAngle)
{
	if (AreDynamicDataChangesAllowed(false)
		&& NewInnerConeAngle != InnerConeAngle)
	{
		InnerConeAngle = NewInnerConeAngle;
		MarkRenderStateDirty();
	}
}

void USpotLightComponent::SetOuterConeAngle(float NewOuterConeAngle)
{
	if (AreDynamicDataChangesAllowed(false)
		&& NewOuterConeAngle != OuterConeAngle)
	{
		OuterConeAngle = NewOuterConeAngle;
		MarkRenderStateDirty();
	}
}

float USpotLightComponent::ComputeLightBrightness() const
{
	float LightBrightness = ULightComponent::ComputeLightBrightness();

	if (bUseInverseSquaredFalloff)
	{
		if (IntensityUnits == ELightUnits::Candelas)
		{
			LightBrightness *= (100.f * 100.f); // Conversion from cm2 to m2
		}
		else if (IntensityUnits == ELightUnits::Lumens)
		{
			LightBrightness *= (100.f * 100.f / 2.f / UE_PI / (1.f - GetCosHalfConeAngle())); // Conversion from cm2 to m2 and cone remapping.
		}
		else if (IntensityUnits == ELightUnits::EV)
		{
			LightBrightness = EV100ToLuminance(LightBrightness) * (100.f * 100.f);
		}
		else
		{
			LightBrightness *= 16; // Legacy scale of 16
		}
	}
	return LightBrightness;
}

#if WITH_EDITOR
void USpotLightComponent::SetLightBrightness(float InBrightness)
{
	if (bUseInverseSquaredFalloff)
	{
		if (IntensityUnits == ELightUnits::Candelas)
		{
			ULightComponent::SetLightBrightness(InBrightness / (100.f * 100.f)); // Conversion from cm2 to m2
		}
		else if (IntensityUnits == ELightUnits::Lumens)
		{
			ULightComponent::SetLightBrightness(InBrightness / (100.f * 100.f / 2.f / UE_PI / (1.f - GetCosHalfConeAngle()))); // Conversion from cm2 to m2 and cone remapping
		}
		else if (IntensityUnits == ELightUnits::EV)
		{
			ULightComponent::SetLightBrightness(LuminanceToEV100(InBrightness / (100.f * 100.f)));
		}
		else
		{
			ULightComponent::SetLightBrightness(InBrightness / 16); // Legacy scale of 16
		}
	}
	else
	{
		ULightComponent::SetLightBrightness(InBrightness);
	}
}

FBox USpotLightComponent::GetStreamingBounds() const
{
	const FSphere BoundingSphere = GetBoundingSphere();
	return FBox(BoundingSphere.Center - BoundingSphere.W, BoundingSphere.Center + BoundingSphere.W);
}
#endif // WITH_EDITOR

static bool IsSpotLightSupported(const USpotLightComponent* InLight)
{
	if (GMaxRHIFeatureLevel <= ERHIFeatureLevel::ES3_1 && InLight->IsMovable())
	{
		if (!IsMobileDeferredShadingEnabled(GMaxRHIShaderPlatform))
		{
			// if project does not support dynamic point/spot lights on mobile do not add them to the renderer 
			return MobileForwardEnableLocalLights(GMaxRHIShaderPlatform);
		}
	}
	return true;
}

FLightSceneProxy* USpotLightComponent::CreateSceneProxy() const
{
	if (IsSpotLightSupported(this))
	{
		return new FSpotLightSceneProxy(this);
	}
	return nullptr;
}

FSphere USpotLightComponent::GetBoundingSphere() const
{
	FSphere::FReal ConeAngle = GetHalfConeAngle();
	FSphere::FReal CosConeAngle = FMath::Cos(ConeAngle);
	FSphere::FReal SinConeAngle = FMath::Sin(ConeAngle);
	return FMath::ComputeBoundingSphereForCone(GetComponentTransform().GetLocation(), GetDirection(), (FSphere::FReal)AttenuationRadius, CosConeAngle, SinConeAngle);
}

bool USpotLightComponent::AffectsBounds(const FBoxSphereBounds& InBounds) const
{
	if(!Super::AffectsBounds(InBounds))
	{
		return false;
	}

	float	ClampedInnerConeAngle = FMath::Clamp(InnerConeAngle,0.0f,89.0f) * (float)UE_PI / 180.0f,
			ClampedOuterConeAngle = FMath::Clamp(OuterConeAngle * (float)UE_PI / 180.0f,ClampedInnerConeAngle + 0.001f,89.0f * (float)UE_PI / 180.0f + 0.001f);

	float	Sin = FMath::Sin(ClampedOuterConeAngle),
			Cos = FMath::Cos(ClampedOuterConeAngle);

	FVector	U = GetComponentLocation() - (InBounds.SphereRadius / Sin) * GetDirection(),
			D = InBounds.Origin - U;
	float	dsqr = D | D,
			E = GetDirection() | D;
	if(E > 0.0f && E * E >= dsqr * FMath::Square(Cos))
	{
		D = InBounds.Origin - GetComponentLocation();
		dsqr = D | D;
		E = -(GetDirection() | D);
		if(E > 0.0f && E * E >= dsqr * FMath::Square(Sin))
			return dsqr <= FMath::Square(InBounds.SphereRadius);
		else
			return true;
	}

	return false;
}

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType USpotLightComponent::GetLightType() const
{
	return LightType_Spot;
}

#if WITH_EDITOR

void USpotLightComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		if (PropertyChangedEvent.Property->GetFName() == TEXT("InnerConeAngle"))
		{
			OuterConeAngle = FMath::Max(InnerConeAngle, OuterConeAngle);
		}
		else if (PropertyChangedEvent.Property->GetFName() == TEXT("OuterConeAngle"))
		{
			InnerConeAngle = FMath::Min(InnerConeAngle, OuterConeAngle);
		}
	}

	UPointLightComponent::PostEditChangeProperty(PropertyChangedEvent);
}

#endif	// WITH_EDITOR

