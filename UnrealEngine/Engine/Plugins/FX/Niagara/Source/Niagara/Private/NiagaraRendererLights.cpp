// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererLights.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSceneProxy.h"
#include "NiagaraSettings.h"
#include "NiagaraStats.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraComponent.h"

#include "NiagaraLightRendererProperties.h"
#include "NiagaraRendererLights.h"
#include "NiagaraCullProxyComponent.h"
#include "PrimitiveViewRelevance.h"
#include "SceneInterface.h"

DECLARE_CYCLE_STAT(TEXT("Generate Particle Lights"), STAT_NiagaraGenLights, STATGROUP_Niagara);

static int32 GbEnableNiagaraLightRendering = 1;
static FAutoConsoleVariableRef CVarEnableNiagaraLightRendering(
	TEXT("fx.EnableNiagaraLightRendering"),
	GbEnableNiagaraLightRendering,
	TEXT("If == 0, Niagara Light Renderers are disabled. \n"),
	ECVF_Default
);

struct FNiagaraDynamicDataLights : public FNiagaraDynamicDataBase
{
	FNiagaraDynamicDataLights(const FNiagaraEmitterInstance* InEmitter)
		: FNiagaraDynamicDataBase(InEmitter)
	{
	}

	TArray<FNiagaraRendererLights::SimpleLightData> LightArray;
};

//////////////////////////////////////////////////////////////////////////


FNiagaraRendererLights::FNiagaraRendererLights(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter)
	: FNiagaraRenderer(FeatureLevel, InProps, Emitter)
{
	// todo - for platforms where we know we can't support deferred shading we can just set this to false
	bHasLights = true;
}

FPrimitiveViewRelevance FNiagaraRendererLights::GetViewRelevance(const FSceneView* View, const FNiagaraSceneProxy *SceneProxy)const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = bHasLights && SceneProxy->IsShown(View) && View->Family->EngineShowFlags.Particles && View->Family->EngineShowFlags.Niagara;
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = false;
	Result.bOpaque = false;
	Result.bHasSimpleLights = bHasLights;

	return Result;
}

/** Update render data buffer from attributes */
FNiagaraDynamicDataBase* FNiagaraRendererLights::GenerateDynamicData(const FNiagaraSceneProxy* Proxy, const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter) const
{
	// particle (simple) lights are only supported with deferred shading
	
	if (!bHasLights || (Proxy->GetScene().GetShadingPath() != EShadingPath::Deferred && !IsMobileDeferredShadingEnabled(Proxy->GetScene().GetShaderPlatform())))
	{
		return nullptr;
	}

	if (!IsRendererEnabled(InProperties, Emitter))
	{
		return nullptr;
	}

	SCOPE_CYCLE_COUNTER(STAT_NiagaraGenLights);

	//Bail if we don't have the required attributes to render this emitter.
	const UNiagaraLightRendererProperties* Properties = CastChecked<const UNiagaraLightRendererProperties>(InProperties);
	const FNiagaraDataSet& Data = Emitter->GetParticleData();
	const FNiagaraDataBuffer* DataToRender = Data.GetCurrentData();
	if (DataToRender == nullptr || Emitter->GetParentSystemInstance() == nullptr)
	{
		return nullptr;
	}

	if (Properties->bAllowInCullProxies == false)
	{
		check(Emitter);

		FNiagaraSystemInstance* Inst = Emitter->GetParentSystemInstance();
		check(Emitter->GetParentSystemInstance());

		//TODO: Probably should push some state into the system instance for this?
		bool bIsCullProxy = Cast<UNiagaraCullProxyComponent>(Inst->GetAttachComponent()) != nullptr;
		if (bIsCullProxy)
		{
			return nullptr;
		}
	}

	FNiagaraSystemInstance* SystemInstance = Emitter->GetParentSystemInstance();
	FNiagaraDynamicDataLights* DynamicData = new FNiagaraDynamicDataLights(Emitter);


	// This used to use Proxy->GetLocalToWorld(), but that's a bad thing to do here, because the proxy gets updated on the render thread,
	// and this function happens during EndOfFrame updates. So instead, use the most up-to-date transform here (fixes local-space frame-behind issues)
	const bool bUseLocalSpace = UseLocalSpace(Proxy);
	const FTransform SimToWorld = SystemInstance->GetLWCSimToWorld(bUseLocalSpace);
	const FVector3f DefaultSimPos = bUseLocalSpace ? FVector3f::ZeroVector : FVector3f(SystemInstance->GetWorldTransform().GetLocation());

	const FNiagaraParameterStore& ParameterStore = Emitter->GetRendererBoundVariables();
	const FVector3f DefaultPos = ParameterStore.GetParameterValueOrDefault(Properties->PositionBinding.GetParamMapBindableVariable(), DefaultSimPos);
	const FLinearColor DefaultColor = ParameterStore.GetParameterValueOrDefault(Properties->ColorBinding.GetParamMapBindableVariable(), Properties->ColorBinding.GetDefaultValue<FLinearColor>());
	const float DefaultRadius = ParameterStore.GetParameterValueOrDefault(Properties->RadiusBinding.GetParamMapBindableVariable(), Properties->RadiusBinding.GetDefaultValue<float>());
	const float DefaultScattering = ParameterStore.GetParameterValueOrDefault(Properties->VolumetricScatteringBinding.GetParamMapBindableVariable(), Properties->VolumetricScatteringBinding.GetDefaultValue<float>());
	const FNiagaraBool DefaultEnabled = ParameterStore.GetParameterValueOrDefault(Properties->LightRenderingEnabledBinding.GetParamMapBindableVariable(), FNiagaraBool(true));
	const int32 DefaultVisibilityTag = ParameterStore.GetParameterValueOrDefault(Properties->RendererVisibilityTagBinding.GetParamMapBindableVariable(), Properties->RendererVisibility);
	const float DefaultExponent = ParameterStore.GetParameterValueOrDefault(Properties->LightExponentBinding.GetParamMapBindableVariable(), Properties->DefaultExponent);
	const float DefaultSpecularScale = ParameterStore.GetParameterValueOrDefault(Properties->SpecularScaleBinding.GetParamMapBindableVariable(), Properties->SpecularScale);

	const float InverseExposureBlend = Properties->bOverrideInverseExposureBlend ? Properties->InverseExposureBlend : GetDefault<UNiagaraSettings>()->DefaultLightInverseExposureBlend;

	// Particles Source mode?
	if (Properties->SourceMode == ENiagaraRendererSourceDataMode::Particles)
	{
		//I'm not a great fan of pulling scalar components out to a structured vert buffer like this.
		//TODO: Experiment with a new VF that reads the data directly from the scalar layout.
		const auto PositionReader = Properties->PositionDataSetAccessor.GetReader(Data);
		const auto ColorReader = Properties->ColorDataSetAccessor.GetReader(Data);
		const auto RadiusReader = Properties->RadiusDataSetAccessor.GetReader(Data);
		const auto ExponentReader = Properties->ExponentDataSetAccessor.GetReader(Data);
		const auto ScatteringReader = Properties->ScatteringDataSetAccessor.GetReader(Data);
		const auto EnabledReader = Properties->EnabledDataSetAccessor.GetReader(Data);
		const auto VisTagReader = Properties->RendererVisibilityTagAccessor.GetReader(Data);
		const auto SpecularScaleReader = Properties->SpecularScaleAccessor.GetReader(Data);

		for (uint32 ParticleIndex = 0; ParticleIndex < DataToRender->GetNumInstances(); ParticleIndex++)
		{
			const int32 VisTag = VisTagReader.GetSafe(ParticleIndex, DefaultVisibilityTag);
			const bool bShouldRenderParticleLight = EnabledReader.GetSafe(ParticleIndex, DefaultEnabled).GetValue() && (VisTag == Properties->RendererVisibility);
			const float LightRadius = RadiusReader.GetSafe(ParticleIndex, DefaultRadius) * Properties->RadiusScale;
			if (bShouldRenderParticleLight && (LightRadius > 0.0f))
			{
				SimpleLightData& LightData = DynamicData->LightArray.AddDefaulted_GetRef();

				const FLinearColor Color = ColorReader.GetSafe(ParticleIndex, DefaultColor);
				const float Brightness = Properties->bAlphaScalesBrightness ? Color.A : 1.0f;
				const FVector3f SimPos = PositionReader.GetSafe(ParticleIndex, DefaultPos);

				LightData.LightEntry.Radius = LightRadius;
				LightData.LightEntry.Color = FVector3f(Color) * Brightness + Properties->ColorAdd;
				LightData.LightEntry.Exponent = Properties->bUseInverseSquaredFalloff ? 0 : ExponentReader.GetSafe(ParticleIndex, DefaultExponent);
				LightData.LightEntry.InverseExposureBlend = InverseExposureBlend;
				LightData.LightEntry.bAffectTranslucency = Properties->bAffectsTranslucency;
				LightData.LightEntry.VolumetricScatteringIntensity = ScatteringReader.GetSafe(ParticleIndex, DefaultScattering);
				LightData.LightEntry.SpecularScale = SpecularScaleReader.GetSafe(ParticleIndex, DefaultSpecularScale);
				LightData.PerViewEntry.Position = SimToWorld.TransformPosition(FVector(SimPos));
			}
		}
	}
	else
	{
		const bool bEnabled = DefaultEnabled.GetValue();
		const int32 VisTag = DefaultVisibilityTag;
		const float LightRadius = DefaultRadius * Properties->RadiusScale;
		if (bEnabled && VisTag == Properties->RendererVisibility && LightRadius > 0.0f)
		{
			const FVector3f SimPos = DefaultPos;
			const FLinearColor LightColor = DefaultColor;
			const float LightExponent = DefaultExponent;
			const float LightScattering = DefaultScattering;
			const float Brightness = Properties->bAlphaScalesBrightness ? LightColor.A : 1.0f;

			SimpleLightData& LightData = DynamicData->LightArray.AddDefaulted_GetRef();
			LightData.LightEntry.Radius = LightRadius;
			LightData.LightEntry.Color = FVector3f(LightColor) * Brightness + Properties->ColorAdd;
			LightData.LightEntry.Exponent = Properties->bUseInverseSquaredFalloff ? 0 : LightExponent;
			LightData.LightEntry.InverseExposureBlend = InverseExposureBlend;
			LightData.LightEntry.bAffectTranslucency = Properties->bAffectsTranslucency;
			LightData.LightEntry.VolumetricScatteringIntensity = LightScattering;
			LightData.LightEntry.SpecularScale = Properties->SpecularScale;

			LightData.PerViewEntry.Position = SimToWorld.TransformPosition(FVector(SimPos));
		}
	}

	return DynamicData;
}

void FNiagaraRendererLights::GatherSimpleLights(FSimpleLightArray& OutParticleLights)const
{
	if (GbEnableNiagaraLightRendering == 0)
	{
		return;
	}

	if (const FNiagaraDynamicDataLights* DynamicData = static_cast<const FNiagaraDynamicDataLights*>(DynamicDataRender))
	{
		const int32 LightCount = DynamicData->LightArray.Num();

		OutParticleLights.InstanceData.Reserve(OutParticleLights.InstanceData.Num() + LightCount);
		OutParticleLights.PerViewData.Reserve(OutParticleLights.PerViewData.Num() + LightCount);

		for (const FNiagaraRendererLights::SimpleLightData &LightData : DynamicData->LightArray)
		{
			// When not using camera-offset, output one position for all views to share.
			OutParticleLights.PerViewData.Add(LightData.PerViewEntry);

			// Add an entry for the light instance.
			OutParticleLights.InstanceData.Add(LightData.LightEntry);
		}
	}
}
