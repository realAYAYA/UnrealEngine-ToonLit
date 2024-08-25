// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PointLightComponent.cpp: PointLightComponent implementation.
=============================================================================*/

#include "Components/RectLightComponent.h"
#include "SceneInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "RectLightSceneProxy.h"
#include "SceneView.h"

#include "DataDrivenShaderPlatformInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RectLightComponent)

extern int32 GAllowPointLightCubemapShadows;

float GetRectLightBarnDoorMaxAngle()
{
	return 88.f;
}

URectLightComponent::URectLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, RayTracingData(new FRectLightRayTracingData())
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightRect"));
		static ConstructorHelpers::FObjectFinder<UTexture2D> DynamicTexture(TEXT("/Engine/EditorResources/LightIcons/S_LightRect"));

		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 0.5f;
		DynamicEditorTexture = DynamicTexture.Object;
		DynamicEditorTextureScale = 0.5f;
	}
#endif

	SourceWidth = 64.0f;
	SourceHeight = 64.0f;
	SourceTexture = nullptr;
	BarnDoorAngle = GetRectLightBarnDoorMaxAngle();
	BarnDoorLength = 20.0f;
	// RayTracingData will be initialised on the render thread.
}

FLightSceneProxy* URectLightComponent::CreateSceneProxy() const
{
	return new FRectLightSceneProxy(this);
}

void URectLightComponent::SetSourceTexture(UTexture* NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceTexture != NewValue)
	{
		SourceTexture = NewValue;

		// This will trigger a recreation of the LightSceneProxy and update RayTracingData accordingly if the texture has changed.
		MarkRenderStateDirty();
	}
}

void URectLightComponent::SetSourceWidth(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceWidth != NewValue)
	{
		SourceWidth = NewValue;
		MarkRenderStateDirty();
	}
}

void URectLightComponent::SetSourceHeight(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& SourceHeight != NewValue)
	{
		SourceHeight = NewValue;
		MarkRenderStateDirty();
	}
}

void URectLightComponent::SetBarnDoorLength(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& BarnDoorLength != NewValue)
	{
		BarnDoorLength = FMath::Max(NewValue, 0.1f);
		MarkRenderStateDirty();
	}
}

void URectLightComponent::SetBarnDoorAngle(float NewValue)
{
	if (AreDynamicDataChangesAllowed()
		&& BarnDoorAngle != NewValue)
	{
		const float MaxAngle = GetRectLightBarnDoorMaxAngle();
		BarnDoorAngle = FMath::Clamp(NewValue, 0.f, MaxAngle);
		MarkRenderStateDirty();
	}
}

float URectLightComponent::ComputeLightBrightness() const
{
	float LightBrightness = Super::ComputeLightBrightness();

	if (IntensityUnits == ELightUnits::Candelas)
	{
		LightBrightness *= (100.f * 100.f); // Conversion from cm2 to m2
	}
	else if (IntensityUnits == ELightUnits::Lumens)
	{
		LightBrightness *= (100.f * 100.f / UE_PI); // Conversion from cm2 to m2 and PI from the cosine distribution
	}
	else if (IntensityUnits == ELightUnits::EV)
	{
		LightBrightness *= (100.f * 100.f) * EV100ToLuminance(LightBrightness);
	}
	else
	{
		LightBrightness *= 16; // Legacy scale of 16
	}

	return LightBrightness;
}

#if WITH_EDITOR
void URectLightComponent::SetLightBrightness(float InBrightness)
{
	if (IntensityUnits == ELightUnits::Candelas)
	{
		Super::SetLightBrightness(InBrightness / (100.f * 100.f)); // Conversion from cm2 to m2
	}
	else if (IntensityUnits == ELightUnits::Lumens)
	{
		Super::SetLightBrightness(InBrightness / (100.f * 100.f / UE_PI)); // Conversion from cm2 to m2 and PI from the cosine distribution
	}
	else if (IntensityUnits == ELightUnits::EV)
	{
		Super::SetLightBrightness(LuminanceToEV100(InBrightness / (100.f * 100.f)));
	}
	else
	{
		Super::SetLightBrightness(InBrightness / 16); // Legacy scale of 16
	}
}
#endif // WITH_EDITOR

/**
* @return ELightComponentType for the light component class 
*/
ELightComponentType URectLightComponent::GetLightType() const
{
	return LightType_Rect;
}

float URectLightComponent::GetUniformPenumbraSize() const
{
	if (LightmassSettings.bUseAreaShadowsForStationaryLight)
	{
		// Interpret distance as shadow factor directly
		return 1.0f;
	}
	else
	{
		float SourceRadius = FMath::Sqrt( SourceWidth * SourceHeight );
		// Heuristic to derive uniform penumbra size from light source radius
		return FMath::Clamp(SourceRadius == 0 ? .05f : SourceRadius * .005f, .0001f, 1.0f);
	}
}

void URectLightComponent::BeginDestroy()
{
	FRectLightRayTracingData* DeletedRenderData = RayTracingData;
	RayTracingData = nullptr;
	ENQUEUE_RENDER_COMMAND(DeleteBuildRectLightMipTree)(
		[DeletedRenderData](FRHICommandListImmediate& RHICmdList)
	{
		delete DeletedRenderData;
	});
	Super::BeginDestroy();
}

#if WITH_EDITOR
/**
 * Called after property has changed via e.g. property window or set command.
 *
 * @param	PropertyThatChanged	FProperty that has been changed, NULL if unknown
 */
void URectLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	SourceWidth  = FMath::Max(1.0f, SourceWidth);
	SourceHeight = FMath::Max(1.0f, SourceHeight);

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

FRectLightSceneProxy::FRectLightSceneProxy(const URectLightComponent* Component)
	: FLocalLightSceneProxy(Component)
	, SourceWidth(Component->SourceWidth)
	, SourceHeight(Component->SourceHeight)
	, BarnDoorAngle(FMath::Clamp(Component->BarnDoorAngle, 0.f, GetRectLightBarnDoorMaxAngle()))
	, BarnDoorLength(FMath::Max(0.1f, Component->BarnDoorLength))
	, RayTracingData(Component->RayTracingData)
	, SourceTexture(Component->SourceTexture)
{
	RectAtlasId = ~0u;
}

FRectLightSceneProxy::~FRectLightSceneProxy() {}

bool FRectLightSceneProxy::IsRectLight() const
{
	return true;
}

bool FRectLightSceneProxy::HasSourceTexture() const
{
	return SourceTexture != nullptr;
}

/** Accesses parameters needed for rendering the light. */
void FRectLightSceneProxy::GetLightShaderParameters(FLightRenderParameters& LightParameters, uint32 Flags) const
{
	FLinearColor LightColor = GetColor();
	LightColor /= 0.5f * SourceWidth * SourceHeight;
	LightParameters.WorldPosition = GetOrigin();
	LightParameters.InvRadius = InvRadius;
	LightParameters.Color = LightColor;
	LightParameters.FalloffExponent = 0.0f;

	LightParameters.Direction = FVector3f(-GetDirection());
	LightParameters.Tangent = FVector3f(WorldToLight.M[0][2], WorldToLight.M[1][2], WorldToLight.M[2][2]);
	LightParameters.SpotAngles = FVector2f(-2.0f, 1.0f);
	LightParameters.SpecularScale = SpecularScale;
	LightParameters.SourceRadius = SourceWidth * 0.5f;
	LightParameters.SoftSourceRadius = 0.0f;
	LightParameters.SourceLength = SourceHeight * 0.5f;
	LightParameters.RectLightBarnCosAngle = FMath::Cos(FMath::DegreesToRadians(BarnDoorAngle));
	LightParameters.RectLightBarnLength = BarnDoorLength;
	LightParameters.RectLightAtlasUVOffset = FVector2f::ZeroVector;
	LightParameters.RectLightAtlasUVScale = FVector2f::ZeroVector;
	LightParameters.RectLightAtlasMaxLevel = FLightRenderParameters::GetRectLightAtlasInvalidMIPLevel();
	LightParameters.IESAtlasIndex = INDEX_NONE;
	LightParameters.InverseExposureBlend = InverseExposureBlend;
	LightParameters.LightFunctionAtlasLightIndex = GetLightFunctionAtlasLightIndex();

	if (IESAtlasId != ~0)
	{
		GetSceneInterface()->GetLightIESAtlasSlot(this, &LightParameters);
	}

	if (RectAtlasId != ~0u)
	{
		GetSceneInterface()->GetRectLightAtlasSlot(this, &LightParameters);
	}
	
	// Render RectLight approximately as SpotLight if the requester does not support rect light (e.g., translucent light grid or mobile)
	const bool bRenderAsSpotLight = !!(Flags & ELightShaderParameterFlags::RectAsSpotLight) || (SceneInterface && IsMobilePlatform(SceneInterface->GetShaderPlatform()));
	if (bRenderAsSpotLight)
	{
		float ClampedOuterConeAngle = FMath::DegreesToRadians(89.001f);
		float ClampedInnerConeAngle = FMath::DegreesToRadians(70.0f);
		float CosOuterCone = FMath::Cos(ClampedOuterConeAngle);
		float CosInnerCone = FMath::Cos(ClampedInnerConeAngle);
		float InvCosConeDifference = 1.0f / (CosInnerCone - CosOuterCone);

		LightParameters.Color = GetColor();
		LightParameters.FalloffExponent = 8.0f;
		LightParameters.SpotAngles = FVector2f(CosOuterCone, InvCosConeDifference);
		LightParameters.SourceRadius = (SourceWidth + SourceHeight) * 0.5 * 0.5f;
		LightParameters.SourceLength = 0.0f;
		LightParameters.RectLightBarnCosAngle = 0.0f;
		LightParameters.RectLightBarnLength = -2.0f;
	}
}

/**
* Sets up a projected shadow initializer for shadows from the entire scene.
* @return True if the whole-scene projected shadow should be used.
*/
bool FRectLightSceneProxy::GetWholeSceneProjectedShadowInitializer(const FSceneViewFamily& ViewFamily, TArray<FWholeSceneProjectedShadowInitializer, TInlineAllocator<6> >& OutInitializers) const
{
	if (ViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::SM5
		&& GAllowPointLightCubemapShadows != 0)
	{
		FWholeSceneProjectedShadowInitializer& OutInitializer = *new(OutInitializers) FWholeSceneProjectedShadowInitializer;
		OutInitializer.PreShadowTranslation = -GetLightToWorld().GetOrigin();
		OutInitializer.WorldToLight = GetWorldToLight().RemoveTranslation();
		OutInitializer.Scales = FVector2D(1, 1);
		OutInitializer.SubjectBounds = FBoxSphereBounds(FVector(0, 0, 0), FVector(Radius, Radius, Radius), Radius);
		OutInitializer.WAxis = FVector4(0, 0, 1, 0);
		OutInitializer.MinLightW = 0.1f;
		OutInitializer.MaxDistanceToCastInLightW = Radius;
		OutInitializer.bOnePassPointLightShadow = true;

		OutInitializer.bRayTracedDistanceField = UseRayTracedDistanceFieldShadows() && DoesPlatformSupportDistanceFieldShadowing(ViewFamily.GetShaderPlatform());
		return true;
	}

	return false;
}