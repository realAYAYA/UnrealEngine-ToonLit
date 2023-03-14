// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RayTracingDefinitions.h"
#include "PathTracingDefinitions.h"

#if RHI_RAYTRACING

RENDERER_API FRDGTexture* PrepareIESAtlas(const TMap<FTexture*, int>& InIESLightProfilesMap, FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel);

RENDERER_API void PrepareLightGrid(FRDGBuilder& GraphBuilder, ERHIFeatureLevel::Type FeatureLevel, FPathTracingLightGrid* LightGridParameters, const FPathTracingLight* Lights, uint32 NumLights, uint32 NumInfiniteLights, FRDGBufferSRV* LightsSRV);

static uint32 EncodeToF16x2(const FVector2f& In)
{
	return FFloat16(In.X).Encoded | (FFloat16(In.Y).Encoded << 16);
}

template<typename PassParameterType>
void SetupPathTracingLightParameters(
	const GPULightmass::FLightSceneRenderState& LightScene,
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	PassParameterType* PassParameters)
{
	TArray<FPathTracingLight> Lights;

	if (LightScene.SkyLight.IsSet())
	{
		FPathTracingLight& DestLight = Lights.AddDefaulted_GetRef();
		DestLight.Color = FVector3f(LightScene.SkyLight->Color);
		DestLight.Flags = PATHTRACER_FLAG_TRANSMISSION_MASK;
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= LightScene.SkyLight->bCastShadow ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		bool SkyLightIsStationary = LightScene.SkyLight->bStationary;
		DestLight.Flags |= SkyLightIsStationary ? PATHTRACER_FLAG_STATIONARY_MASK : 0;
		DestLight.Flags |= PATHTRACING_LIGHT_SKY;

		PassParameters->SkylightTexture = GraphBuilder.RegisterExternalTexture(LightScene.SkyLight->PathTracingSkylightTexture, TEXT("PathTracer.Skylight"));
		PassParameters->SkylightTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->SkylightPdf = GraphBuilder.RegisterExternalTexture(LightScene.SkyLight->PathTracingSkylightPdf, TEXT("PathTracer.SkylightPdf"));
		PassParameters->SkylightInvResolution = LightScene.SkyLight->SkylightInvResolution;
		PassParameters->SkylightMipCount = LightScene.SkyLight->SkylightMipCount;
	}
	else
	{
		PassParameters->SkylightTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		PassParameters->SkylightTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		PassParameters->SkylightPdf = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		PassParameters->SkylightInvResolution = 0;
		PassParameters->SkylightMipCount = 0;
	}


	for (auto Light : LightScene.DirectionalLights.Elements)
	{
		FPathTracingLight& DestLight = Lights.AddDefaulted_GetRef();

		DestLight.Normal = (FVector3f)-Light.Direction;
		DestLight.Color = FVector3f(Light.Color);
		DestLight.Dimensions = FVector2f(
			FMath::Sin(0.5f * FMath::DegreesToRadians(Light.LightSourceAngle)),
			0.0f);
		DestLight.Attenuation = 1.0;
		DestLight.IESTextureSlice = -1;

		DestLight.Flags = PATHTRACER_FLAG_TRANSMISSION_MASK;
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= Light.bCastShadow ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		DestLight.Flags |= Light.bStationary ? PATHTRACER_FLAG_STATIONARY_MASK : 0;
		DestLight.Flags |= PATHTRACING_LIGHT_DIRECTIONAL;
	}

	uint32 NumInfiniteLights = Lights.Num();

	TMap<FTexture*, int> IESLightProfilesMap;

	for (auto Light : LightScene.PointLights.Elements)
	{
		FPathTracingLight& DestLight = Lights.AddDefaulted_GetRef();

		DestLight.TranslatedWorldPosition = FVector3f(Light.Position + View.ViewMatrices.GetPreViewTranslation());
		DestLight.Color = (FVector3f)(Light.Color);
		DestLight.Normal = (FVector3f)Light.Direction;
		DestLight.dPdu = (FVector3f)FVector::CrossProduct(Light.Tangent, Light.Direction);
		DestLight.dPdv = (FVector3f)Light.Tangent;

		DestLight.Dimensions = FVector2f(Light.SourceRadius, Light.SourceLength);
		DestLight.Attenuation = 1.0f / Light.AttenuationRadius;
		DestLight.FalloffExponent = Light.FalloffExponent;

		if (Light.IESTexture)
		{
			DestLight.IESTextureSlice = IESLightProfilesMap.FindOrAdd(Light.IESTexture, IESLightProfilesMap.Num());
		}
		else
		{
			DestLight.IESTextureSlice = INDEX_NONE;
		}

		DestLight.Flags = PATHTRACER_FLAG_TRANSMISSION_MASK;
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= Light.bCastShadow ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		DestLight.Flags |= Light.IsInverseSquared ? 0 : PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK;
		DestLight.Flags |= Light.bStationary ? PATHTRACER_FLAG_STATIONARY_MASK : 0;
		DestLight.Flags |= PATHTRACING_LIGHT_POINT;

		float Radius = Light.AttenuationRadius;
		FVector3f Center = DestLight.TranslatedWorldPosition;

		// simple sphere of influence
		DestLight.TranslatedBoundMin = Center - FVector3f(Radius, Radius, Radius);
		DestLight.TranslatedBoundMax = Center + FVector3f(Radius, Radius, Radius);
	}

	for (auto Light : LightScene.SpotLights.Elements)
	{
		FPathTracingLight& DestLight = Lights.AddDefaulted_GetRef();

		DestLight.TranslatedWorldPosition = FVector3f(Light.Position + View.ViewMatrices.GetPreViewTranslation());
		DestLight.Normal = (FVector3f)Light.Direction;
		DestLight.dPdu = (FVector3f)FVector::CrossProduct(Light.Tangent, Light.Direction);
		DestLight.dPdv = (FVector3f)Light.Tangent;
		DestLight.Color = FVector3f(Light.Color);
		DestLight.Dimensions = FVector2f(Light.SourceRadius, Light.SourceLength);
		DestLight.Shaping = FVector2f(Light.SpotAngles);
		DestLight.Attenuation = 1.0f / Light.AttenuationRadius;
		DestLight.FalloffExponent = Light.FalloffExponent;

		if (Light.IESTexture)
		{
			DestLight.IESTextureSlice = IESLightProfilesMap.FindOrAdd(Light.IESTexture, IESLightProfilesMap.Num());
		}
		else
		{
			DestLight.IESTextureSlice = INDEX_NONE;
		}

		DestLight.Flags = PATHTRACER_FLAG_TRANSMISSION_MASK;
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= Light.bCastShadow ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		DestLight.Flags |= Light.IsInverseSquared ? 0 : PATHTRACER_FLAG_NON_INVERSE_SQUARE_FALLOFF_MASK;
		DestLight.Flags |= Light.bStationary ? PATHTRACER_FLAG_STATIONARY_MASK : 0;
		DestLight.Flags |= PATHTRACING_LIGHT_SPOT;

		// LWC_TODO: Precision Loss
		float Radius = Light.AttenuationRadius;
		FVector3f Center = DestLight.TranslatedWorldPosition;
		FVector3f Normal = DestLight.Normal;
		FVector3f Disc = FVector3f(
			FMath::Sqrt(FMath::Clamp(1 - Normal.X * Normal.X, 0.0f, 1.0f)),
			FMath::Sqrt(FMath::Clamp(1 - Normal.Y * Normal.Y, 0.0f, 1.0f)),
			FMath::Sqrt(FMath::Clamp(1 - Normal.Z * Normal.Z, 0.0f, 1.0f))
		);
		// box around ray from light center to tip of the cone
		FVector3f Tip = Center + Normal * Radius;
		DestLight.TranslatedBoundMin = Center.ComponentMin(Tip);
		DestLight.TranslatedBoundMax = Center.ComponentMax(Tip);
		// expand by disc around the farthest part of the cone

		float CosOuter = Light.SpotAngles.X;
		float SinOuter = FMath::Sqrt(1.0f - CosOuter * CosOuter);

		DestLight.TranslatedBoundMin = DestLight.TranslatedBoundMin.ComponentMin(Center + Radius * (Normal * CosOuter - Disc * SinOuter));
		DestLight.TranslatedBoundMax = DestLight.TranslatedBoundMax.ComponentMax(Center + Radius * (Normal * CosOuter + Disc * SinOuter));
	}

	for (auto Light : LightScene.RectLights.Elements)
	{
		FPathTracingLight& DestLight = Lights.AddDefaulted_GetRef();

		DestLight.TranslatedWorldPosition = FVector3f(Light.Position + View.ViewMatrices.GetPreViewTranslation());
		DestLight.Normal = (FVector3f)Light.Direction;
		DestLight.dPdu = (FVector3f)FVector::CrossProduct(Light.Tangent, -Light.Direction);
		DestLight.dPdv = (FVector3f)Light.Tangent;

		FLinearColor LightColor = Light.Color;
		LightColor /= 0.5f * Light.SourceWidth * Light.SourceHeight;
		DestLight.Color = FVector3f(LightColor);

		DestLight.Dimensions = FVector2f(Light.SourceWidth, Light.SourceHeight);
		DestLight.Attenuation = 1.0f / Light.AttenuationRadius;
		DestLight.Shaping = FVector2f(FMath::Cos(FMath::DegreesToRadians(Light.BarnDoorAngle)), Light.BarnDoorLength);

		if (Light.IESTexture)
		{
			DestLight.IESTextureSlice = IESLightProfilesMap.FindOrAdd(Light.IESTexture, IESLightProfilesMap.Num());
		}
		else
		{
			DestLight.IESTextureSlice = INDEX_NONE;
		}

		DestLight.Flags = PATHTRACER_FLAG_TRANSMISSION_MASK;
		DestLight.Flags |= PATHTRACER_FLAG_LIGHTING_CHANNEL_MASK;
		DestLight.Flags |= Light.bCastShadow ? PATHTRACER_FLAG_CAST_SHADOW_MASK : 0;
		DestLight.Flags |= Light.bStationary ? PATHTRACER_FLAG_STATIONARY_MASK : 0;
		DestLight.Flags |= PATHTRACING_LIGHT_RECT;

		float Radius = Light.AttenuationRadius;
		FVector3f Center = DestLight.TranslatedWorldPosition;
		FVector3f Normal = DestLight.Normal;
		FVector3f Disc = FVector3f(
			FMath::Sqrt(FMath::Clamp(1 - Normal.X * Normal.X, 0.0f, 1.0f)),
			FMath::Sqrt(FMath::Clamp(1 - Normal.Y * Normal.Y, 0.0f, 1.0f)),
			FMath::Sqrt(FMath::Clamp(1 - Normal.Z * Normal.Z, 0.0f, 1.0f))
		);
		// quad bbox is the bbox of the disc +  the tip of the hemisphere
		// TODO: is it worth trying to account for barndoors? seems unlikely to cut much empty space since the volume _inside_ the barndoor receives light
		FVector3f Tip = Center + Normal * Radius;
		DestLight.TranslatedBoundMin = Tip.ComponentMin(Center - Radius * Disc);
		DestLight.TranslatedBoundMax = Tip.ComponentMax(Center + Radius * Disc);

		DestLight.RectLightAtlasUVOffset = EncodeToF16x2(Light.RectLightAtlasUVOffset);
		DestLight.RectLightAtlasUVScale  = EncodeToF16x2(Light.RectLightAtlasUVScale);
		if (Light.RectLightAtlasMaxLevel < 16)
		{
			DestLight.Flags |= PATHTRACER_FLAG_HAS_RECT_TEXTURE_MASK;
		}
	}

	PassParameters->SceneLightCount = Lights.Num();
	{
		// Upload the buffer of lights to the GPU
		// need at least one since zero-sized buffers are not allowed
		if (Lights.Num() == 0)
		{
			Lights.AddDefaulted();
		}
		PassParameters->SceneLights = GraphBuilder.CreateSRV(FRDGBufferSRVDesc(CreateStructuredBuffer(GraphBuilder, TEXT("PathTracer.LightsBuffer"), sizeof(FPathTracingLight), Lights.Num(), Lights.GetData(), sizeof(FPathTracingLight) * Lights.Num())));
	}

	if (IESLightProfilesMap.Num() > 0)
	{
		PassParameters->IESTexture = PrepareIESAtlas(IESLightProfilesMap, GraphBuilder, View.GetFeatureLevel());
	}
	else
	{
		PassParameters->IESTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.WhiteDummy);
	}
	PassParameters->IESTextureSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	PrepareLightGrid(GraphBuilder, View.GetFeatureLevel(), &PassParameters->LightGridParameters, Lights.GetData(), PassParameters->SceneLightCount, NumInfiniteLights, PassParameters->SceneLights);
}

#endif  // RHI_RAYTRACING
